import {
	createConnection,
	TextDocuments,
	TextDocument,
	Diagnostic,
	DiagnosticSeverity,
	ProposedFeatures,
	InitializeParams,
	DidChangeConfigurationNotification,
	CompletionItem,
	CompletionItemKind,
	TextDocumentPositionParams,
	Range,
	Position
} from 'vscode-languageserver';

import { exec } from 'child_process';
import { readFile } from 'fs';
import * as path from 'path';

import * as peg from 'pegjs';
import { uriToFilePath } from 'vscode-languageserver/lib/files';

let connection = createConnection(ProposedFeatures.all);

let documents: TextDocuments = new TextDocuments();

let hasConfigurationCapability: boolean = false;
let hasWorkspaceFolderCapability: boolean = false;
let hasDiagnosticRelatedInformationCapability: boolean = false;

let parsedScripts: Map<string, any> = new Map();

connection.onInitialize((params: InitializeParams) => {
	let capabilities = params.capabilities;

	hasConfigurationCapability = !!(capabilities.workspace && !!capabilities.workspace.configuration);
	hasWorkspaceFolderCapability = !!(capabilities.workspace && !!capabilities.workspace.workspaceFolders);
	hasDiagnosticRelatedInformationCapability =
		!!(capabilities.textDocument &&
		capabilities.textDocument.publishDiagnostics &&
		capabilities.textDocument.publishDiagnostics.relatedInformation);

	return {
		capabilities: {
			textDocumentSync: documents.syncKind,
			completionProvider: {
				resolveProvider: true
			}
		}
	};
});

connection.onInitialized(() => {
	if (hasConfigurationCapability) {
		connection.client.register(
			DidChangeConfigurationNotification.type,
			undefined
		);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders(_event => {
			connection.console.log('Workspace folder change event received.');
		});
	}
});

interface ServerSettings {
	pathToScriptCompiler: string;
	scriptWorkingDirectory: string;
}

const defaultSettings: ServerSettings = { pathToScriptCompiler: '', scriptWorkingDirectory: '' };
let globalSettings: ServerSettings = defaultSettings;

let documentSettings: Map<string, Thenable<ServerSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		documentSettings.clear();
	} else {
		globalSettings = <ServerSettings>(
			(change.settings.angelscript || defaultSettings)
		);
	}

	documents.all().forEach(validateTextDocument);
});

function getDocumentSettings(resource: string): Thenable<ServerSettings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: 'angelscript'
		});
		documentSettings.set(resource, result);
	}
	return result;
}

documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
	parsedScripts.delete(e.document.uri);
});

documents.onDidSave(async e => {
	validateTextDocument(e.document);
	parsedScripts.delete(e.document.uri);
});

async function getScriptTokens(doc: TextDocument) {
	let res = parsedScripts.get(doc.uri);
	if (!res) {
		let text = doc.getText();
		let scriptParser = await getScriptParser();
		res = scriptParser.parse(text);
		parsedScripts.set(doc.uri, res);
	}
	return res;
}

async function getScriptParser(): Promise<any> {
	return new Promise<any>((resolve, reject) => {
		readFile(`${__dirname}/../block.grammar.pegjs`, (err, content) => {
			if (!!err) {
				reject(err);
			}
			else {
				resolve(peg.generate(content.toString()));
			}
		});
	});
}

async function execScript(doc: TextDocument, command: string): Promise<any> {
	return new Promise<any>(async (resolve, reject) => {
		let settings = await getDocumentSettings(doc.uri);
		exec(`${settings.pathToScriptCompiler} ${command}`, { cwd: settings.scriptWorkingDirectory }, (_, stdout) => {
			try {
				resolve(JSON.parse(stdout));
			}
			catch (jsonErr) {
				reject(jsonErr);
			}
		});
	});
}

async function inspectScript(doc: TextDocument): Promise<any> {
	let settings = await getDocumentSettings(doc.uri);
	let p = path.relative(settings.scriptWorkingDirectory, uriToFilePath(doc.uri));
	return execScript(doc, `--inspect ${p}`);
}

async function compileScript(doc: TextDocument): Promise<any> {
	let settings = await getDocumentSettings(doc.uri);
	let p = path.relative(settings.scriptWorkingDirectory, uriToFilePath(doc.uri));
	return execScript(doc, `--compile ${p}`);
}

async function validateTextDocument(textDocument: TextDocument): Promise<void> {
	let diagnostics: Diagnostic[] = [];
	let script = await compileScript(textDocument);
	if (!!script.messages) {
		for (let m of script.messages) {
			let diagnosic: Diagnostic = {
				severity: m.type == 'error' ? DiagnosticSeverity.Error : m.type == 'warning' ? DiagnosticSeverity.Warning : DiagnosticSeverity.Information,
				range: {
					start: Position.create(m.row - 1, m.col - 1),
					end: Position.create(m.row, 0)
				},
				message: m.message,
				source: 'as'
			};
			diagnostics.push(diagnosic);
		}
	}

	connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}

connection.onDidChangeWatchedFiles(_change => {
	// Monitored files have change in VSCode
	connection.console.log('We received an file change event');
});

function blockExtend(doc: TextDocument, startPos: Position): string {
	const findBlockStartPosition = (doc: TextDocument, startPos: Position): Position | null => {
		let line = startPos.line;
		let text = doc.getText(Range.create(line, 0, line + 1, 0));

		let offset = startPos.character;
		let scope = 0;

		do {
			let p = -1;

			for (let i = offset; i >= 0; --i) {
				const ch = text[i];
				scope += ch == '{' ? 1 : ch == '}' ? -1 : 0;
				if (ch == '{') {
					p = i;
					if (scope > 0) {
						break;
					}
				}
			}

			if (p >= 0 && scope > 0) {
				return Position.create(line, p);
			}

			if (--line >= 0) {
				text = doc.getText(Range.create(line, 0, line + 1, 0));
				offset = text.length - 1;
			}
		} while (line >= 0);

		return null;
	}

	const findBlockEndPosition = (doc: TextDocument, startPos: Position): Position | null => {
		let line = startPos.line;
		let text = doc.getText(Range.create(line, 0, line + 1, 0));

		let offset = startPos.character;
		let scope = 0;

		do {
			let p = -1;

			for (let i = offset; i < text.length; ++i) {
				const ch = text[i];
				scope += ch == '{' ? 1 : ch == '}' ? -1 : 0;
				if (ch == '}') {
					p = i;
					if (scope < 0) {
						break;
					}
				}
			}

			if (p >= 0 && scope < 0) {
				return Position.create(line, p + 1);
			}

			if (++line < doc.lineCount) {
				text = doc.getText(Range.create(line, 0, line + 1, 0));
				offset = 0;
			}
		} while (line < doc.lineCount);

		return null;
	}

	const blockStartPos: Position = findBlockStartPosition(doc, startPos);
	const blockEndPos: Position = findBlockEndPosition(doc, startPos);
	
	if (blockStartPos == null || blockEndPos === null) {
		return '';
	}

	return doc.getText(Range.create(blockStartPos, blockEndPos));
}

function wordExtend(doc: TextDocument, pos: Position): string {
	const text = doc.getText(Range.create(pos.line, 0, pos.line, pos.character));
	const match = text.match(/[\d\w\$_]+$/);
	return !!match ? match[0] : '';
}

function expressionExtend(doc: TextDocument, pos: Position): string[] {
	const text = doc.getText(Range.create(pos.line, 0, pos.line, pos.character));
	const match = text.match(/[\d\w\$\._]+$/);
	return !!match ? match[0].split('.') : [];
}

async function getCurrentFunction(doc: TextDocument, pos: Position) {
	let tokens = await getScriptTokens(doc);
	let offset = doc.offsetAt(pos);

	for (let t of tokens) {
		if (!!t.statement && t.statement === "function") {
			if (offset >= t.location.start.offset && offset <= t.location.end.offset)
			{
				return t;
			}
		}
	}

	return null;
}

function getLocalVars(tokens: any, recursive = true) {
	let res = [];
	if (!!tokens.body) {
		tokens = tokens.body;
	}
	for (let token of tokens) {
		if (!!token.statement && token.statement === 'var') {
			res.push(token);
		}
		if (recursive && !!token.body) {
			res = res.concat(getLocalVars(token.body, false));
		}
	}
	return res;
}

function resolveVarType(name: string, localVars: any[], func: any): any | null {
	if (func && !!func.args) {
		for (let v of func.args) {
			if (v.name === name) {
				return v.type;
			}
		}	
	}
	for (let v of localVars) {
		if (v.name === name) {
			return v.type;
		}
	}
	return null;
}

function findByName(name: string, data: any) {
	for (let t of data) {
		if (t.name === name) {
			return t;
		}
	}
	return null;
}

function findType(name: string, scriptData: any) {
	if (!!scriptData.script && !!scriptData.script.types) {
		let t = findByName(name, scriptData.script.types);
		if (t) {
			return t;
		}
	}
	if (!!scriptData.types) {
		let t = findByName(name, scriptData.types);
		if (t) {
			return t;
		}
	}
	return null;
}

connection.onCompletion(
	async (textDocumentPosition: TextDocumentPositionParams): Promise<CompletionItem[]> => {
		let doc = documents.get(textDocumentPosition.textDocument.uri);
		let pos = textDocumentPosition.position;

		let scriptParser = await getScriptParser();
		let scriptData = await inspectScript(doc);
		let func = await getCurrentFunction(doc, pos);
		
		let block = blockExtend(doc, pos);
		let word = wordExtend(doc, pos);
		let expr = expressionExtend(doc, pos);

		if (func) {
			block = blockExtend(doc, Position.create(func.body.location.start.line - 1, func.body.location.start.column));
		}
		
		let localVars = [];

		try {
			// TODO: Extend block in case of not resolved variable
			let tokens = scriptParser.parse(block);
			let tmpLocalVars = getLocalVars(tokens);

			// TODO: Resolve chain a.b.c.d
			if ((!!scriptData.types || (!!scriptData.script && !!scriptData.script.types)) && expr.length > 1) {
				let type = resolveVarType(expr[0], tmpLocalVars, func);
				if (!!type) {
					let res = [];
					let t = findType(type.name, scriptData);
					if (t && expr.length > 2) {
						let pt = findByName(expr[1], t.props);
						if (pt) {
							t = findType(pt.type, scriptData);
						}
					}
					if (t) {
						for (let p of t.props) {
							res.push({
								label: p.name,
								kind: CompletionItemKind.Property,
								detail: p.type
							});
						}
						for (let f of t.methods) {
							res.push({
								label: f.name,
								kind: CompletionItemKind.Method,
								detail: f.decl
							});
						}
					}
					return res;
				}
			}

			if (func && !!func.args) {
				for (let v of func.args) {
					localVars.push({
						label: v.name,
						kind: CompletionItemKind.Variable,
						detail: v.decl
					});
				}	
			}

			for (let v of tmpLocalVars) {
				localVars.push({
					label: v.name,
					kind: CompletionItemKind.Variable,
					detail: v.decl
				});
			}
		}
		catch (err) {
			return [];
		}

		let res = localVars;
		// Native
		if (!!scriptData.funcs) {
			for (let f of scriptData.funcs) {
				res.push({
					label: f.name,
					kind: CompletionItemKind.Function,
					detail: f.decl
				});
			}
		}
		if (!!scriptData.types) {
			for (let t of scriptData.types) {
				res.push({
					label: t.name,
					kind: CompletionItemKind.Class
				});
			}
		}
		// Script
		if (!!scriptData.script) {
			if (!!scriptData.script.funcs) {
				for (let f of scriptData.script.funcs) {
					res.push({
						label: f.name,
						kind: CompletionItemKind.Function,
						detail: f.decl
					});
				}
			}
			if (!!scriptData.script.types) {
				for (let t of scriptData.script.types) {
					res.push({
						label: t.name,
						kind: CompletionItemKind.Class
					});
				}
			}
		}
		return res;
	}
);

connection.onCompletionResolve(
	(item: CompletionItem): CompletionItem => {
		return item;
	}
);

/*
connection.onDidOpenTextDocument((params) => {
	// A text document got opened in VSCode.
	// params.uri uniquely identifies the document. For documents store on disk this is a file URI.
	// params.text the initial full content of the document.
	connection.console.log(`${params.textDocument.uri} opened.`);
});
connection.onDidChangeTextDocument((params) => {
	// The content of a text document did change in VSCode.
	// params.uri uniquely identifies the document.
	// params.contentChanges describe the content changes to the document.
	connection.console.log(`${params.textDocument.uri} changed: ${JSON.stringify(params.contentChanges)}`);
});
connection.onDidCloseTextDocument((params) => {
	// A text document got closed in VSCode.
	// params.uri uniquely identifies the document.
	connection.console.log(`${params.textDocument.uri} closed.`);
});
*/

documents.listen(connection);

connection.listen();
