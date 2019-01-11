'use strict';

import * as vscode from 'vscode';
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';
// import { MockDebugSession } from './mockDebug';
// import * as Net from 'net';

export function activate(context: vscode.ExtensionContext) {
  const provider = new ASConfigurationProvider();
	context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('angelscript', provider));
}

export function deactivate() {
}

class ASConfigurationProvider implements vscode.DebugConfigurationProvider {
    resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {

		if (!config.type && !config.request && !config.name) {
			const editor = vscode.window.activeTextEditor;
			if (editor && editor.document.languageId === 'angelscript') {
				config.type = 'angelscript';
				config.name = 'Attach';
				config.request = 'attach';
				config.program = '${file}';
        config.stopOnEntry = false;
        config.trace = true;
			}
		}

		if (!config.program) {
			return vscode.window.showInformationMessage("Cannot find a program to debug").then(_ => {
				return undefined;	// abort launch
			});
		}

		return config;
	}
}