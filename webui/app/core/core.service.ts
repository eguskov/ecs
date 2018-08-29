import { Injectable } from '@angular/core';

import { Observable } from 'rxjs/Observable';
import { Subscription } from 'rxjs/Subscription';
import { map } from 'rxjs/operator/map';
import { debounceTime } from 'rxjs/operator/debounceTime';
import { distinctUntilChanged } from 'rxjs/operator/distinctUntilChanged';

import { StreamService, deserializeBSON } from './stream.service';

import * as common from './common';

const UPDATE_INTERVAL_MS: number = 500;

@Injectable()
export class CoreService
{
  private _listeners: Array<common.IMessageListener> = [];

  private _observable: Observable<any> = null;
  private _messagesObservable: Observable<any> = null;

  private _updateCount: number = 0;
  private _updateTimeout: NodeJS.Timer = null;

  constructor(public streamService: StreamService)
  {
    console.log('CoreService::CoreService');

    this.streamService.onOpen(() => { this.processOpen(); });

    this._observable = Observable.create(observer =>
    {
      this.streamService.onMessage((message) => { this.processMessage(observer, message); });
    });

    this._observable.subscribe(value => { this.processValue(value); });

    if (this.streamService.isConnected())
      this.onOpen();
  }

  addListener(listener: common.IMessageListener)
  {
    this._listeners.push(listener);

    if (this.streamService.isConnected())
      listener.onOpen();
  }

  onOpen(): void
  {
    console.log('CoreService::onOpen');

    for (let listener of this._listeners)
    {
      listener.onOpen();
    }
  }

  onMessage(data): void
  {
    if (data._update)
    {
      ++this._updateCount;
    }

    for (let listener of this._listeners)
    {
      listener.onMessage(data);
    }
  }

  continueUpdate(force: boolean = false): void
  {
    if (force)
    {
      if (this._updateTimeout != null)
        clearTimeout(this._updateTimeout);
      this._updateTimeout = null;
    }

    if (this._updateTimeout === null)
      this._updateTimeout = global.setTimeout(() => { this.sendCommand('update'); }, UPDATE_INTERVAL_MS);
  }

  processOpen(): void
  {
    this.onOpen();
    this.continueUpdate(true);
  }

  processMessage(observer, message): void
  {
    if (typeof message === 'object' && message.constructor && message.constructor.name === 'Blob')
    {
      let reader = new FileReader();
      reader.addEventListener("loadend", () =>
      {
        observer.next([deserializeBSON(new Uint8Array(<ArrayBuffer>reader.result))]);
        this.continueUpdate();
      });
      reader.readAsArrayBuffer(message);
    }
    else
    {
      let json = {};
      try
      {
        json = JSON.parse(message);
      }
      catch (err)
      {
        json = { log: message };
      }
      observer.next([json]);
    }
  }

  processValue(value): void
  {
    let [data] = value;
    this.onMessage(data);

    if (data._update)
    {

      if (this._updateTimeout !== null)
        clearTimeout(this._updateTimeout);
      this._updateTimeout = null;
    }
  }

  sendCommand(cmd: string, params: any = null): void
  {
    let tmp = params || {};
    tmp.cmd = cmd;
    this.streamService.send(tmp);
  }

  get isConnected() { return this.streamService.isConnected(); };
  get updateCount(): number { return +this._updateCount; }

  set messagesObservable(value) { this._messagesObservable = value; }
  get messagesObservable() { return this._messagesObservable; }

  get observable() { return this._observable; }
}