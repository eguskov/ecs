import { readFileSync } from 'fs';
import { EventEmitter } from 'events';

import { client, IMessage, connection } from 'websocket';
import * as BSON from 'bson';
import { logger } from 'vscode-debugadapter';

export interface MockBreakpoint {
	id: number;
	line: number;
	verified: boolean;
}

/**
 * A Mock runtime with minimal debugger functionality.
 */
export class MockRuntime extends EventEmitter {

	// the initial (and one and only) file we are 'debugging'
	private _sourceFile: string;
	public get sourceFile() {
		return this._sourceFile;
	}

	// the contents (= lines) of the one and only file
	private _sourceLines: string[];

	// This is the next line that will be 'executed'
	// private _currentLine = 0;

	// maps from sourceFile to array of Mock breakpoints
	private _breakPoints = new Map<string, MockBreakpoint[]>();

	// since we want to send breakpoint events, we will assign an id to every event
	// so that the frontend can match events with breakpoints.
  private _breakpointId = 1;
  
  private _wsClient = new client();
  private _wsConnection: connection | null;
  private _wsResponses = new Map<number, any>();
  private _wsRequestId = 0;

  // private _breakpointsOnConnect: any = [];

	constructor() {
    super();
  }
  
  // private sendCommand(command: string, args: any) {
  //   if (this._wsConnection) {
  //     this._wsConnection.sendUTF(JSON.stringify(Object.assign(args, { cmd: command })));
  //   }
  // }

  private async getConnection(): Promise<connection> {
    return new Promise<connection>((resolve, reject) => {
      if (this._wsConnection) {
        resolve(this._wsConnection);
      }
      else {
        this._wsClient.removeAllListeners('connect');
        this._wsClient.removeAllListeners('connectFailed');

        this._wsClient.on('connectFailed', (err) => {
          logger.warn(`Fail: ${err}`);
          this._wsConnection = null;
          reject(null);
        });
    
        this._wsClient.on('connect', (conn) => {
          logger.warn(`Success`);
          this._wsConnection = conn;
    
          conn.on('message', (data: IMessage) => {
            let text = !!data.utf8Data ? data.utf8Data : '{}';
            let binary = !!data.binaryData ? data.binaryData : null;
            let obj: any = {};

            if (binary)
            {
              try {
                obj = BSON.deserialize(binary);
              }
              catch (err) {
                logger.warn(`ERROR: ${err}`);
              }
            }
            else if (text)
              obj = JSON.parse(text);
  
            if (!!obj.$requestId)
            {
              logger.warn(`Got Message[${obj.cmd} - ${obj.$requestId}]: ${JSON.stringify(obj)}`);
              this._wsResponses.set(obj.$requestId, obj);
            }
            else if (!!obj.hit_breakpoint)
            {
              logger.warn(`hit_breakpoint`);
              this.sendEvent('stopOnBreakpoint');
            }
    
            // if (!!obj.getECSData) {  
            //   logger.warn(`getECSData: ${data}`);
            // }
            // else if (!!obj.getECSTemplates) {  
            //   logger.warn(`getECSTemplates: ${data}`);
            // }
            // else if (!!obj.getECSEntities) {  
            //   logger.warn(`getECSEntities: ${data}`);
            // }
            // else {
            //   logger.warn(`Message: ${JSON.stringify(obj)}`);
            // }
          });

          conn.on('error', (err) => {
            logger.warn(`Error: ${err}`);
            this._wsConnection = null;
          });

          conn.on('close', (code: number, desc: string) => {
            logger.warn(`Closed: code: ${code}; desc: ${desc};`);
            this._wsConnection = null;
          });

          resolve(conn);
        });

        this._wsClient.connect('ws://localhost:10112/');
      }
    });
  }

  private async execCommand(command: string, args: any): Promise<any> {
    return new Promise<any>(async (resolve, reject) => {
      let conn = await this.getConnection();
      if (!conn) {
        reject(null);
        return;
      }

      this._wsRequestId++;

      const requestId = this._wsRequestId;
      let timeout = 10000;

      let msg = JSON.stringify(Object.assign(args, { cmd: command, $requestId: requestId }));
      logger.warn(`Send Message: ${msg}`);

      conn.sendUTF(msg);

      const id = setInterval(() => {
        const res = this._wsResponses.get(requestId);
        
        if (res) {
          resolve(res);
          this._wsResponses.delete(requestId);
          clearInterval(id);
        }

        timeout -= 10;
        if (timeout <= 0) {
          logger.warn(`TIMEOUT: ${requestId}`);
          reject(null);
          clearInterval(id);
        }
      }, 10);
    });
  }

	/**
	 * Start executing the given program.
	 */
	public start(program: string, stopOnEntry: boolean) {
		this.loadSource(program);
		// this._currentLine = -1;

		this.verifyBreakpoints(this._sourceFile);

		if (stopOnEntry) {
			// we step once
			this.step(false, 'stopOnEntry');
		} else {
			// we just start to run until we hit a breakpoint or an exception
			this.continue();
		}
  }
  
	public async attach(program: string, stopOnEntry: boolean) {
    logger.warn(`attach`);

		this.loadSource(program);
		// this._currentLine = -1;

		// this.verifyBreakpoints(this._sourceFile);

		if (stopOnEntry) {
			// we step once
			this.step(false, 'stopOnEntry');
    }
    else {
			// we just start to run until we hit a breakpoint or an exception
			this.continue();
		}
  }
  
  public async disconnect() {
    await this.execCommand("script::debug::resume", {});
  }

	/**
	 * Continue execution to the end/beginning.
	 */
	public async continue(reverse = false) {
    this.run(reverse, undefined);
    await this.execCommand("script::debug::resume", {});
	}

	/**
	 * Step to the next/previous non empty line.
	 */
	public async step(reverse = false, event = 'stopOnStep') {
    this.run(reverse, event);
    await this.execCommand("script::debug::step", {});
  }

	public async stepIn() {
    await this.execCommand("script::debug::step_in", {});
  }

	public async stepOut() {
    await this.execCommand("script::debug::step_out", {});
  }

	public async stepOver() {
    await this.execCommand("script::debug::step_over", {});
    this.sendEvent('stopOnStep');
  }
  
  public async getLocalVars(): Promise<any> {
    let resp = await this.execCommand("script::debug::get_local_vars", {});
    return resp.localVars;
  }

	/**
	 * Returns a fake 'stacktrace' where every 'stackframe' is a word from the current line.
	 */
	public async stack(startFrame: number, endFrame: number): Promise<any> {
    let resp = await this.execCommand("script::debug::get_callstack", {});

		// const words = this._sourceLines[this._currentLine].trim().split(/\s+/);

		// const frames = new Array<any>();
		// // every word of the current line becomes a stack frame.
		// for (let i = startFrame; i < Math.min(endFrame, words.length); i++) {
		// 	const name = words[i];	// use a word of the line as the stackframe name
		// 	frames.push({
		// 		index: i,
		// 		name: `${name}(${i})`,
		// 		file: this._sourceFile,
		// 		line: this._currentLine
		// 	});
		// }
		// return {
		// 	frames: frames,
		// 	count: words.length
    // };

    const frames = new Array<any>();
    for (let i = startFrame; i < Math.min(endFrame, resp.callstack.length); i++) {
      const frame = resp.callstack[i];
      frames.push({
        index: i,
        name: frame.function,
        file: this._sourceFile,
        line: frame.line
      });
    }
    
    return { frames: frames, count: resp.callstack.length };
	}

	/*
	 * Set breakpoint in file with given line.
	 */
	public async setBreakPoint(path: string, line: number) : Promise<MockBreakpoint> {
    logger.warn(`setBreakPoint`);

    // if (this._wsConnection) {
    //   this.sendCommand("script::debug::add_breakpoint", { file: "script.as", line: line });
    // }
    // else {
    //   this._breakpointsOnConnect.push({ file: "script.as", line: line + 1 });
    // }

    await this.execCommand('script::debug::enable', {});

    let resp = await this.execCommand("script::debug::add_breakpoint", { file: "script.as", line: line });
    logger.warn(`setBreakPoint: resp: ${JSON.stringify(resp)}`);

		const bp = <MockBreakpoint> { verified: true, line, id: this._breakpointId++ };
		// let bps = this._breakPoints.get(path);
		// if (!bps) {
		// 	bps = new Array<MockBreakpoint>();
		// 	this._breakPoints.set(path, bps);
		// }
		// bps.push(bp);

    // this.verifyBreakpoints(path);
    this.sendEvent('breakpointValidated', bp);

		return Promise.resolve(bp);
	}

	/*
	 * Clear breakpoint in file with given line.
	 */
	public clearBreakPoint(path: string, line: number) : MockBreakpoint | undefined {
		let bps = this._breakPoints.get(path);
		if (bps) {
			const index = bps.findIndex(bp => bp.line === line);
			if (index >= 0) {
				const bp = bps[index];
				bps.splice(index, 1);
				return bp;
			}
		}
		return undefined;
	}

	/*
	 * Clear all breakpoints for file.
	 */
	public async clearBreakpoints(path: string): Promise<void> {
    this._breakPoints.delete(path);
    await this.execCommand('script::debug::remove_all_breakpoints', {});
	}

	// private methods

	private loadSource(file: string) {
		if (this._sourceFile !== file) {
			this._sourceFile = file;
			this._sourceLines = readFileSync(this._sourceFile).toString().split('\n');
		}
	}

	/**
	 * Run through the file.
	 * If stepEvent is specified only run a single step and emit the stepEvent.
	 */
	private run(reverse = false, stepEvent?: string) {
		// if (reverse) {
		// 	for (let ln = this._currentLine-1; ln >= 0; ln--) {
		// 		if (this.fireEventsForLine(ln, stepEvent)) {
		// 			this._currentLine = ln;
		// 			return;
		// 		}
		// 	}
		// 	// no more lines: stop at first line
		// 	this._currentLine = 0;
		// 	this.sendEvent('stopOnEntry');
		// } else {
		// 	for (let ln = this._currentLine+1; ln < this._sourceLines.length; ln++) {
		// 		if (this.fireEventsForLine(ln, stepEvent)) {
		// 			this._currentLine = ln;
		// 			return true;
		// 		}
		// 	}
		// 	// no more lines: run to end
		// 	this.sendEvent('end');
		// }
	}

	private verifyBreakpoints(path: string) : void {
		let bps = this._breakPoints.get(path);
		if (bps) {
			this.loadSource(path);
			bps.forEach(bp => {
				if (!bp.verified && bp.line < this._sourceLines.length) {
					const srcLine = this._sourceLines[bp.line].trim();

					// if a line is empty or starts with '+' we don't allow to set a breakpoint but move the breakpoint down
					if (srcLine.length === 0 || srcLine.indexOf('+') === 0) {
						bp.line++;
					}
					// if a line starts with '-' we don't allow to set a breakpoint but move the breakpoint up
					if (srcLine.indexOf('-') === 0) {
						bp.line--;
					}
					// don't set 'verified' to true if the line contains the word 'lazy'
					// in this case the breakpoint will be verified 'lazy' after hitting it once.
					if (srcLine.indexOf('lazy') < 0) {
						bp.verified = true;
						this.sendEvent('breakpointValidated', bp);
					}
				}
			});
		}
	}

	/**
	 * Fire events if line has a breakpoint or the word 'exception' is found.
	 * Returns true is execution needs to stop.
	 */
	// private fireEventsForLine(ln: number, stepEvent?: string): boolean {

	// 	const line = this._sourceLines[ln].trim();

	// 	// if 'log(...)' found in source -> send argument to debug console
	// 	const matches = /log\((.*)\)/.exec(line);
	// 	if (matches && matches.length === 2) {
	// 		this.sendEvent('output', matches[1], this._sourceFile, ln, matches.index)
	// 	}

	// 	// if word 'exception' found in source -> throw exception
	// 	if (line.indexOf('exception') >= 0) {
	// 		this.sendEvent('stopOnException');
	// 		return true;
	// 	}

	// 	// is there a breakpoint?
	// 	const breakpoints = this._breakPoints.get(this._sourceFile);
	// 	if (breakpoints) {
	// 		const bps = breakpoints.filter(bp => bp.line === ln);
	// 		if (bps.length > 0) {

	// 			// send 'stopped' event
	// 			this.sendEvent('stopOnBreakpoint');

	// 			// the following shows the use of 'breakpoint' events to update properties of a breakpoint in the UI
	// 			// if breakpoint is not yet verified, verify it now and send a 'breakpoint' update event
	// 			if (!bps[0].verified) {
	// 				bps[0].verified = true;
	// 				this.sendEvent('breakpointValidated', bps[0]);
	// 			}
	// 			return true;
	// 		}
	// 	}

	// 	// non-empty line
	// 	if (stepEvent && line.length > 0) {
	// 		this.sendEvent(stepEvent);
	// 		return true;
	// 	}

	// 	// nothing interesting found -> continue
	// 	return false;
	// }

	private sendEvent(event: string, ... args: any[]) {
		setImmediate(_ => {
			this.emit(event, ...args);
		});
	}
}