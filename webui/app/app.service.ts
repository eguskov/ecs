import { Injectable } from '@angular/core';
import { IMessageListener, Dictionary } from './core/common';
import { CoreService } from './core/core.service';
import { Observable } from 'rxjs/Observable';

export interface ECSData
{
  templates: ECSTemplate[];
  systems: ECSSystem[];
  scriptSystems: ECSSystem[];
}

export interface ECSTemplate
{
  data: any;

  name: string;
  components: ECSComponent[];
  systems: number[];
}

export interface ECSSystem
{
  isQuery: boolean;
  isScript: boolean;

  data: any;

  name: string;
  components: ECSComponent[];
  haveComponents: ECSComponent[];
  notHaveComponents: ECSComponent[];
  isTrueComponents: ECSComponent[];
  isFalseComponents: ECSComponent[];
}

export interface ECSComponent
{
  data: any;

  name: string;
  type: string;

  remap: number[];
}

@Injectable()
export class AppService implements IMessageListener
{
  private _observable: Observable<any> = null;
  private _observer: any = null;

  private _ecsData: ECSData = { templates: [], systems: [], scriptSystems: [] };
  private _ecsComponents: ECSComponent[];
  private _ecsComponentsByName: Dictionary<ECSComponent>;

  private _ecsTemplates: any;
  private _ecsEntities: any;

  private _processMessages = {
    'getECSData': d => this.processECSData(d),
    'getECSTemplates': d => { console.log(d); this.processECSTemplates(d.$templates); },
    'getECSEntities': d => { console.log(d); this._ecsEntities = d; this._observer.next('ecsEntities'); },
  };

  constructor(public coreService: CoreService)
  {
    this.coreService.addListener(this);

    this._observable = Observable.create(observer =>
    {
      this._observer = observer;
    });
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

  processECSTemplates(data: any): void
  {
    this._ecsTemplates = data;

    this._ecsTemplates.forEach(t =>
    {
      for (let k in t.$components)
      {
        let c = t.$components[k];
        c.$valueStr = JSON.stringify(c.$value, null, 2);
      };
    });

    this._observer.next('ecsTemplates');
  }

  processECSData(data: any): void
  {
    this._ecsComponents = [];
    this._ecsComponentsByName = {};
    this._ecsData = data;

    this._ecsData.scriptSystems.forEach(s =>
    {
      s.data = {};
      s.components.forEach(v => v.data = {});
    });

    for (let s of this._ecsData.systems)
    {
      s.data = {};
      s.isQuery = s.name.indexOf('exec_') === 0;

      s.name = s.name.replace(/^exec_/, '');
      // if (!s.isQuery)
      // {
      //   s.name += `:${s.components[0].type}`;
      // }
      s.components = s.components.filter(v => v.name !== 'eid');
      s.components = [].concat(s.components, s.haveComponents);
      s.components = [].concat(s.components, s.isTrueComponents);
      s.components = [].concat(s.components, s.isFalseComponents);
      // s.components = [].concat(s.components, s.notHaveComponents);

      s.components.forEach(v => v.data = {});
    }

    const idx = this._ecsData.systems.findIndex(s => s.name === 'update_script');
    if (idx >= 0)
    {
      this._ecsData.scriptSystems.forEach(s => s.isScript = true);
      this._ecsData.systems.splice(idx + 1, 0, ...this._ecsData.scriptSystems);
    }

    for (let t of this._ecsData.templates)
    {
      t.data = {};
      t.systems = [];
      t.components.forEach(c =>
      {
        c.data = {};
        if (!this._ecsComponentsByName[c.name])
        {
          this._ecsComponentsByName[c.name] = c;
          this._ecsComponents.push(c);
        }
      });

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

        if (s.notHaveComponents)
        {
          for (let c of s.notHaveComponents)
          {
            if (this._hasComponent(t, c))
            {
              ok = false;
              break;
            }
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

    this._observer.next('ecsData');
  }

  getECSData(): void
  {
    this.coreService.sendCommand('getECSData', {});
  }

  get observable() { return this._observable; }
  get ecsData() { return this._ecsData; }
  get ecsComponents() { return this._ecsComponents; }
  get ecsComponentsByName() { return this._ecsComponentsByName; }
  get ecsTemplates() { return this._ecsTemplates; }
  get ecsEntities() { return this._ecsEntities; }
}