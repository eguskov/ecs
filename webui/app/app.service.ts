import { Injectable } from '@angular/core';
import { IMessageListener } from './core/common';
import { CoreService } from './core/core.service';
import { THIS_EXPR } from '@angular/compiler/src/output/output_ast';

@Injectable()
export class AppService implements IMessageListener
{
  private _ecsData: any = { templates: [], systems: [] };

  private _processMessages = { 'getECSData': d => this.processECSData(d) };

  constructor(public coreService: CoreService)
  {
    this.coreService.addListener(this);
  }

  onOpen(): void
  {
    this.getECSData();
  }

  onMessage(data: any): void
  {
    for (let k in data)
    {
      if (this._processMessages[k])
      {
        this._processMessages[k](data[k]);
      }
    }
  }

  private _hasComponent(src, comp): boolean
  {
    if (comp.name === 'eid')
    {
      return true;
    }
    return this._getComponentIndex(src, comp) >= 0;
  }

  private _getComponentIndex(src, comp): number
  {
    if (comp.name === 'eid')
    {
      return src.components.findIndex(v => v.name === 'eid');
    }
    if (comp.type.indexOf('Stage') >= 0)
    {
      return 0;
    }
    if (comp.type.indexOf('Event') >= 0)
    {
      return 0;
    }
    return src.components.findIndex(v => v.name === comp.name);
  }

  processECSData(data: any): void
  {
    this._ecsData = data;

    for (let s of this._ecsData.systems)
    {
      s.name = s.name.replace(/^exec_/, '');
      s.components = s.components.filter(v => v.name !== 'eid');
      s.components = [].concat(s.components, s.haveComponents);
      s.components = [].concat(s.components, s.isTrueComponents);
      s.components = [].concat(s.components, s.isFalseComponents);
      
      // s.components = [].concat(s.components, s.notHaveComponents);
    }

    for (let t of this._ecsData.templates)
    {
      t.systems = [];

      let sysIndex = 0;
      for (let s of this._ecsData.systems)
      {
        let ok = true;
        for (let c of s.components)
        {
          if (!this._hasComponent(t, c))
          {
            ok = false;
            break;
          }
        }

        if (ok)
        {
          t.systems.push(sysIndex);

          for (let c of t.components)
          {
            if (!c.remap)
            {
              c.remap = [];
            }
            c.remap[sysIndex] = this._getComponentIndex(s, c);
          }
        }

        ++sysIndex;
      }
    }

    console.log(this._ecsData);
  }

  getECSData(): void
  {
    this.coreService.sendCommand('getECSData', {});
  }

  get ecsData() { return this._ecsData; }
}