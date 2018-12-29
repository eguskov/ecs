/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */

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
import * as peg from 'pegjs';
import { readFile } from 'fs';

// Create a connection for the server. The connection uses Node's IPC as a transport.
// Also include all preview / proposed LSP features.
let connection = createConnection(ProposedFeatures.all);

// Create a simple text document manager. The text document manager
// supports full document sync only
let documents: TextDocuments = new TextDocuments();

let hasConfigurationCapability: boolean = false;
let hasWorkspaceFolderCapability: boolean = false;
let hasDiagnosticRelatedInformationCapability: boolean = false;

connection.onInitialize((params: InitializeParams) => {
	console.log('>>> onInitialized 1');
	let capabilities = params.capabilities;

	// Does the client support the `workspace/configuration` request?
	// If not, we will fall back using global settings
	hasConfigurationCapability = !!(capabilities.workspace && !!capabilities.workspace.configuration);
	hasWorkspaceFolderCapability = !!(capabilities.workspace && !!capabilities.workspace.workspaceFolders);
	hasDiagnosticRelatedInformationCapability =
		!!(capabilities.textDocument &&
		capabilities.textDocument.publishDiagnostics &&
		capabilities.textDocument.publishDiagnostics.relatedInformation);

	return {
		capabilities: {
			textDocumentSync: documents.syncKind,
			// Tell the client that the server supports code completion
			completionProvider: {
				resolveProvider: true
			}
		}
	};
});

connection.onInitialized(() => {
	console.log('>>> onInitialized 2');
	if (hasConfigurationCapability) {
		// Register for all configuration changes.
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

// The example settings
interface ExampleSettings {
	maxNumberOfProblems: number;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: ExampleSettings = { maxNumberOfProblems: 1000 };
let globalSettings: ExampleSettings = defaultSettings;

// Cache the settings of all open documents
let documentSettings: Map<string, Thenable<ExampleSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		// Reset all cached document settings
		documentSettings.clear();
	} else {
		globalSettings = <ExampleSettings>(
			(change.settings.languageServerExample || defaultSettings)
		);
	}

	// Revalidate all open text documents
	documents.all().forEach(validateTextDocument);
});

function getDocumentSettings(resource: string): Thenable<ExampleSettings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: 'languageServerExample'
		});
		documentSettings.set(resource, result);
	}
	return result;
}

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

// The content of a text document has changed. This event is emitted
// when the text document first opened or when its content has changed.
documents.onDidChangeContent(change => {
	validateTextDocument(change.document);
});

async function getBlockGrammar(): Promise<string> {
	return new Promise<any>((resolve, reject) => {
		readFile(`${__dirname}/../block.grammar.pegjs`, (err, content) => {
			if (!!err) {
				reject(err);
			}
			else {
				resolve(content.toString());
			}
		});
	});
}

async function compileScript(): Promise<any> {
	return new Promise<any>((resolve, reject) => {
		exec("D:/projects/ecs/Debug/sample.exe --compile script.as", { cwd: 'D:/projects/ecs/sample' }, (_, stdout) => {
			try {
				resolve(JSON.parse(stdout));
			}
			catch (jsonErr) {
				reject(jsonErr);
			}
		});
	});
}

async function validateTextDocument(textDocument: TextDocument): Promise<void> {
	// In this simple example we get the settings for every validate run.
	let settings = await getDocumentSettings(textDocument.uri);

	// The validator creates diagnostics for all uppercase words length 2 and more
	let text = textDocument.getText();
	let pattern = /\b[A-Z]{2,}\b/g;
	let m: RegExpExecArray | null;

	let problems = 0;
	let diagnostics: Diagnostic[] = [];
	while ((m = pattern.exec(text)) && problems < settings.maxNumberOfProblems) {
		problems++;
		let diagnosic: Diagnostic = {
			severity: DiagnosticSeverity.Warning,
			range: {
				start: textDocument.positionAt(m.index),
				end: textDocument.positionAt(m.index + m[0].length)
			},
			message: `${m[0]} is all uppercase.`,
			source: 'ex'
		};
		if (hasDiagnosticRelatedInformationCapability) {
			diagnosic.relatedInformation = [
				{
					location: {
						uri: textDocument.uri,
						range: Object.assign({}, diagnosic.range)
					},
					message: 'Spelling matters'
				},
				{
					location: {
						uri: textDocument.uri,
						range: Object.assign({}, diagnosic.range)
					},
					message: 'Particularly for names'
				}
			];
		}
		diagnostics.push(diagnosic);
	}

	// Send the computed diagnostics to VSCode.
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

function getLocalVars(tokens: any) {
	let res = [];
	if (!!tokens.body) {
		tokens = tokens.body;
	}
	for (let token of tokens) {
		if (!!token.statement && token.statement === 'var') {
			res.push(token);
		}
		// if (!!token.body) {
		// 	res = res.concat(getLocalVars())
		// }
	}
	return res;
}

function resolveVarType(name: string, localVars: any[]): any | null {
	for (let v of localVars) {
		if (v.name === name) {
			return v.type;
		}
	}
	return null;
}

// This handler provides the initial list of the completion items.
connection.onCompletion(
	async (textDocumentPosition: TextDocumentPositionParams): Promise<CompletionItem[]> => {
		// The pass parameter contains the position of the text document in
		// which code complete got requested. For the example we ignore this
		// info and always provide the same completion items.

		let blockGrammar = await getBlockGrammar();
		let script = await compileScript();

		let blockParser = peg.generate(blockGrammar);

		let doc = documents.get(textDocumentPosition.textDocument.uri);
		let pos = textDocumentPosition.position;
		// let linePrefix = doc.getText(Range.create(pos.line, 0, pos.line, pos.character));
		let block = blockExtend(doc, pos);
		let word = wordExtend(doc, pos);
		let expr = expressionExtend(doc, pos);

		let localVars = [];

		try {
			// TODO: Extend block in case of not resolved variable
			let tokens = blockParser.parse(block);
			let tmpLocalVars = getLocalVars(tokens);

			// TODO: Resolve chain a.b.c.d
			if ((!!script.types || (!!script.script && !!script.script.types)) && expr.length > 1) {
				let type = resolveVarType(expr[0], tmpLocalVars);
				if (!!type) {
					let res = [];
					if (!!script.script && !!script.script.types) {
						for (let t of script.script.types) {
							if (t.name === type.name) {
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
						}
					}
					if (!!script.types) {
						for (let t of script.types) {
							if (t.name === type.name) {
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
						}
					}
					return res;
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
		if (!!script.funcs) {
			for (let f of script.funcs) {
				res.push({
					label: f.name,
					kind: CompletionItemKind.Function,
					detail: f.decl
				});
			}
		}
		if (!!script.types) {
			for (let t of script.types) {
				res.push({
					label: t.name,
					kind: CompletionItemKind.Class
				});
			}
		}
		// Script
		if (!!script.script) {
			if (!!script.script.funcs) {
				for (let f of script.script.funcs) {
					res.push({
						label: f.name,
						kind: CompletionItemKind.Function,
						detail: f.decl
					});
				}
			}
			if (!!script.script.types) {
				for (let t of script.script.types) {
					res.push({
						label: t.name,
						kind: CompletionItemKind.Class
					});
				}
			}
		}
		return res;

		// return [
		// 	{
		// 		label: 'TypeScript',
		// 		kind: CompletionItemKind.Text,
		// 		data: 1
		// 	},
		// 	{
		// 		label: 'JavaScript',
		// 		kind: CompletionItemKind.Text,
		// 		data: 2
		// 	}
		// ];
	}
);

// This handler resolves additional information for the item selected in
// the completion list.
connection.onCompletionResolve(
	(item: CompletionItem): CompletionItem => {

		// exec("D:/projects/ecs/Debug/sample.exe --compile script.as", { cwd: 'D:/projects/ecs/sample' }, (_, stdout) => {
		// 	try {
		// 		let json = JSON.parse(stdout);
		// 		console.log(json);
		// 	}
		// 	catch (jsonErr) {
		// 		console.log(jsonErr);
		// 	}
		// });

		// if (item.data === 1) {
		// 	(item.detail = 'TypeScript details'),
		// 		(item.documentation = 'TypeScript documentation');
		// } else if (item.data === 2) {
		// 	(item.detail = 'JavaScript details'),
		// 		(item.documentation = 'JavaScript documentation');
		// }

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

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();
