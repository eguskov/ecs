import { Injectable } from '@angular/core';
import { $WebSocket } from 'angular2-websocket/angular2-websocket';

import * as common from './common';

var bson = require('../../vendor/bson.js');

export function deserializeBSON(buffer: any): any
{
  let BSON = bson().BSON
  return BSON.deserialize(buffer);
}

@Injectable()
export class StreamService
{
  socket: $WebSocket;

  init(): void
  {
    if (this.socket)
    {
      return;
    }

    this.socket = new $WebSocket("ws://localhost:10112/stream", null, { initialTimeout: 500, maxTimeout: 5000, reconnectIfNotNormalClose: true });
  }

  onOpen(callback: Function): void
  {
    this.init();
    this.socket.onOpen((event: Event) =>
    {
      callback();
    });
  }

  onMessage(callback: Function): void
  {
    this.init();
    this.socket.onMessage((event: MessageEvent) => { callback(event.data); }, null);
  }

  isConnected(): boolean
  {
    this.init();
    return this.socket && this.socket.getReadyState() == 1;
  }

  send(json: any): void
  {
    if (this.isConnected())
    {
      this.socket.send(json).subscribe();
    }
  }
}