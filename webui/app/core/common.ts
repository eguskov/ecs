import { OnInit, OnDestroy } from '@angular/core';
import { CoreService } from '../core/core.service';

export interface Dictionary<T>
{
  [K: string]: T;
}

export abstract class BaseComponent implements OnInit, OnDestroy
{
  constructor(public coreService: CoreService)
  {
  }

  ngOnInit(): void
  {
    console.log('BaseComponent::OnInit', this.constructor.name);
  }

  ngOnDestroy(): void 
  {
    console.log('BaseComponent::OnDestroy');
  }

  sendCommand(cmd: string, params: any = null): void
  {
    this.coreService.sendCommand(cmd, params);
  }

  trackById(item)
  {
    return item.id;
  }

  trackByName(item)
  {
    return item.name;
  }

  trackByValue(item)
  {
    return item;
  }

  get isConnected() { return this.coreService.isConnected; };
}

export interface IMessageListener
{
  onOpen(): void;
  onMessage(data): void;
}