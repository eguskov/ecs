import { Component, OnInit, AfterViewInit, NgZone, ViewChild, ElementRef, OnDestroy, ViewEncapsulation, HostListener, TemplateRef } from '@angular/core';

import { StreamService } from './core/stream.service';
import { CoreService } from './core/core.service';
import { AppService, ECSTemplate, ECSSystem } from './app.service';
import { Subscription } from 'rxjs/Subscription';
import { LevelEditorComponent } from './leveleditor.component';

enum TemplateOrientation
{
  kLeft,
  kRight
}

const ENTITY_EDITOR_WIDTH = /* 300 */ 0;

@Component({
  selector: 'my-app',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css'],
  encapsulation: ViewEncapsulation.None
})
export class AppComponent implements OnInit, OnDestroy
{
  @ViewChild('levelEditor') levelEditor: LevelEditorComponent;

  @ViewChild('root') root: ElementRef;
  @ViewChild('canvas') canvas: ElementRef;

  ctx: CanvasRenderingContext2D = null;
  width: number = 0;
  height: number = 0;

  status: string = 'Unknonwn';

  modes: string[] = ['Detailed', 'Compact'];
  currentMode: string = 'Compact';

  selectedTemplateForEdit: any = null;

  sourceBuilderTools = [
    // { name: 'Section', children: [] as any[], inputType: 'section', icon: 'section', class: 'wide' },
    // { name: 'A String', inputType: 'string', icon: 'field-text', class: 'half' },
    // { name: 'A Number', inputType: 'number', icon: 'field-numeric', class: 'half' }
  ];
  targetBuilderTools: any[] = [];

  editorConfig = {
    lineNumbers: true,
    theme: 'dracula',
    mode: {
      name: 'json',
      json: true
    }
  };

  private _dataSubscription: Subscription;

  constructor(public appService: AppService, private streamService: StreamService, public coreService: CoreService, private zone: NgZone)
  {
    this._draw = this._draw.bind(this);
    this._onDataUpdate = this._onDataUpdate.bind(this);
    this._dataSubscription = this.appService.observable.subscribe(this._onDataUpdate);
  }

  ngAfterViewInit()
  {
    (<HTMLCanvasElement>this.canvas.nativeElement).width = this.root.nativeElement.offsetWidth;
    (<HTMLCanvasElement>this.canvas.nativeElement).height = this.root.nativeElement.offsetHeight;

    this.ctx = this.canvas.nativeElement.getContext('2d');
    if (this.ctx)
    {
      this.width = (<HTMLCanvasElement>this.canvas.nativeElement).width;
      this.height = (<HTMLCanvasElement>this.canvas.nativeElement).height;
      console.log(`Context is ready: ${this.width} x ${this.height}`);

      this._draw();
    }
  }

  ngOnInit(): void
  {
    console.log('AppComponent::OnInit');

    this.zone.runOutsideAngular(() =>
    {
      setInterval(() =>
      {
        let prev = this.status;
        let status = this.streamService.isConnected() ? 'Online' : 'Offline';
        if (prev !== status)
        {
          this.zone.run(() =>
          {
            this.status = status;
          })
        }
      }, 100);
    });
  }

  ngOnDestroy()
  {
    this._dataSubscription.unsubscribe();
  }

  _onDataUpdate(id: string)
  {
    console.log('_onDataUpdate', id);

    if (id === 'ecsData')
    {
      this.sourceBuilderTools = [];
      this.targetBuilderTools = [];

      for (let c of this.appService.ecsComponents)
      {
        let item = Object.assign({ inputType: 'section', icon: 'field-text', class: 'half' }, c);
        this.sourceBuilderTools.push(item);
      }

    }
    else if (id === 'ecsTemplates')
    {
      this.selectedTemplateForEdit = this.appService.ecsTemplates[0];

      for (let k in this.selectedTemplateForEdit.$components)
      {
        let c = this.selectedTemplateForEdit.$components[k];
        let item = Object.assign({ name: k, inputType: 'section', icon: 'field-text', class: 'half ecs-template' }, c);
        this.targetBuilderTools.push(item);
      }
    }
  }

  droppableItemClass = (item: any) => `${item.class} ${item.inputType}`;

  builderDrag(e: any)
  {
    const item = e.value;
    item.data =
      item.inputType === 'number'
        ? (Math.random() * 100) | 0
        : Math.random()
          .toString(36)
          .substring(20);
  }

  log(e: any)
  {
    console.log(e.type, e);
  }

  onSelectTemplateForEdit($event)
  {
    this.selectedTemplateForEdit = $event[0];

    this.targetBuilderTools = [];

    for (let k in this.selectedTemplateForEdit.$components)
    {
      let c = this.selectedTemplateForEdit.$components[k];
      let item = Object.assign({ name: k, inputType: 'section', icon: 'field-text', class: 'half' }, c);
      this.targetBuilderTools.push(item);
    }
  }

  _draw()
  {
    this.zone.runOutsideAngular(() =>
    {
      this.draw();
      requestAnimationFrame(this._draw);
    });
  }

  selectedTemplate: ECSTemplate = null;
  selectedSystem: ECSSystem = null;

  onClick($event: MouseEvent)
  {
    if (this._skipNextClick)
    {
      this._skipNextClick = false;
      return;
    }

    const clickX = $event.offsetX;
    const clickY = $event.offsetY;

    for (let templ of this.appService.ecsData.templates)
    {
      if (templ.data.visible && clickX >= templ.data.leftX && clickX <= templ.data.rightX && clickY >= templ.data.y && clickY <= templ.data.y + templ.data.height)
      {
        this.selectedTemplate = this.selectedTemplate === templ ? null : templ;
        this.selectedSystem = null;
        return;
      }
    }

    for (let sys of this.appService.ecsData.systems)
    {
      if (sys.data.visible && clickX >= sys.data.leftX && clickX <= sys.data.rightX && clickY >= sys.data.y && clickY <= sys.data.y + sys.data.height)
      {
        this.selectedTemplate = null;
        this.selectedSystem = this.selectedSystem === sys ? null : sys;
        return;
      }
    }
  }

  private _drawX = 10;
  private _drawY = 10;

  private _drag = false;
  private _skipNextClick = false;
  private _dragX = 0;
  private _dragY = 0;

  onMouseDown($event: MouseEvent)
  {
    this._drag = true;
    this._dragX = $event.offsetX;
    this._dragY = $event.offsetY;
  }

  onMouseMove($event: MouseEvent)
  {
    if (this._drag)
    {
      this._skipNextClick = true;

      const dx = $event.offsetX - this._dragX;
      const dy = $event.offsetY - this._dragY;

      this._drawX += dx;
      this._drawY += dy;

      this._dragX = $event.offsetX;
      this._dragY = $event.offsetY;
    }
  }

  onMouseUp($event: MouseEvent)
  {
    this._drag = false;
  }

  onMouseLeave($event: MouseEvent)
  {
    this._drag = false;
  }

  findMaxTextWidth(src: ECSSystem | ECSTemplate, minWidth: number): number
  {
    this.ctx.font = this.currentMode === 'Compact' ? '12pt Calibri' : 'bold 14pt Calibri';
    const m = this.ctx.measureText(src.name);
    let maxWidth = m.width;

    if (this.currentMode === 'Detailed')
    {
      for (let comp of src.components)
      {
        this.ctx.font = 'bold 12pt Calibri';
        const m = this.ctx.measureText(`${comp.name}:${comp.type}`);
        if (m.width > maxWidth)
        {
          maxWidth = m.width;
        }
      }
    }
    return Math.max(maxWidth, minWidth);
  }

  drawTemplate(x: number, y: number, orient: TemplateOrientation, templ: ECSTemplate): { w: number, h: number }
  {
    const lineHeight = this.currentMode === 'Compact' ? 40 : 60;

    let rw = this.findMaxTextWidth(templ, 80) + 30;
    const rh = lineHeight + (this.currentMode === 'Compact' ? -0.5 * lineHeight : 0.5 * lineHeight * templ.components.length);

    if (orient == TemplateOrientation.kLeft)
    {
      x += this.maxWidthLeft - rw;
    }

    const radius = 5;

    this.ctx.beginPath();
    this.ctx.moveTo(x + radius, y);
    this.ctx.lineTo(x + rw - radius, y);
    this.ctx.quadraticCurveTo(x + rw, y, x + rw, y + radius);
    this.ctx.lineTo(x + rw, y + rh - radius);
    this.ctx.quadraticCurveTo(x + rw, y + rh, x + rw - radius, y + rh);
    this.ctx.lineTo(x + radius, y + rh);
    this.ctx.quadraticCurveTo(x, y + rh, x, y + rh - radius);
    this.ctx.lineTo(x, y + radius);
    this.ctx.quadraticCurveTo(x, y, x + radius, y);

    if (this.currentMode === 'Detailed')
    {
      this.ctx.moveTo(x, y + 0.5 * lineHeight);
      this.ctx.lineTo(x + rw, y + 0.5 * lineHeight);
    }

    this.ctx.closePath();

    this.ctx.fillStyle = 'rgb(255, 255, 255)';
    this.ctx.fill();
    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = 'rgb(0, 0, 0)';
    this.ctx.stroke();

    this.ctx.font = this.currentMode === 'Compact' ? '12pt Calibri' : 'bold 14pt Calibri';
    this.ctx.textBaseline = 'middle';
    this.ctx.textAlign = 'center';
    this.ctx.fillStyle = 'rgb(0, 0, 0)';
    this.ctx.fillText(templ.name, x + 0.5 * rw, y + 0.25 * lineHeight, rw);

    templ.data.leftX = x;
    templ.data.rightX = x + rw;
    templ.data.y = y;
    templ.data.width = rw;
    templ.data.height = rh;

    if (this.currentMode === 'Compact')
    {
      if (templ === this.selectedTemplate || this.selectedSystem !== null || this.selectedTemplate === null)
      {
        for (let sysIndex of templ.systems)
        {
          let sys = this.appService.ecsData.systems[sysIndex];
          if (this.selectedSystem !== null && sys !== this.selectedSystem)
            continue;

          sys.data.visible = true;
          this.ctx.beginPath();
          if (orient == TemplateOrientation.kLeft)
          {
            this.ctx.moveTo(x + rw, y + 0.5 * rh);
            this.ctx.lineTo(sys.data.leftX, sys.data.y + 0.5 * sys.data.height);
          }
          else if (orient == TemplateOrientation.kRight)
          {
            this.ctx.moveTo(x, y + 0.5 * rh);
            this.ctx.lineTo(sys.data.rightX, sys.data.y + 0.5 * sys.data.height);
          }
          this.ctx.lineWidth = 1;
          // this.ctx.strokeStyle = 'rgb(0, 0, 0)';
          this.ctx.strokeStyle = '#EBEDF2';
          this.ctx.stroke();
        }
      }

      this.ctx.beginPath();
      if (orient == TemplateOrientation.kLeft)
        this.ctx.arc(x + rw, y + 0.5 * rh, 5, 0, 2 * Math.PI);
      else
        this.ctx.arc(x, y + 0.5 * rh, 5, 0, 2 * Math.PI);
      this.ctx.fillStyle = 'rgb(255, 255, 255)';
      this.ctx.fill();
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgb(0, 0, 0)';
      this.ctx.stroke();
    }
    else if (this.currentMode === 'Detailed')
    {
      let compX = x + rw - 14;
      let compY = y + lineHeight;
      for (let comp of templ.components)
      {
        this.ctx.font = 'bold 12pt Calibri';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillStyle = 'rgb(0, 0, 0)';
        if (orient == TemplateOrientation.kLeft)
        {
          this.ctx.textAlign = 'right';
          this.ctx.fillText(`${comp.name}:${comp.type}`, compX, compY, rw);
        }
        else
        {
          this.ctx.textAlign = 'left';
          this.ctx.fillText(`${comp.name}:${comp.type}`, compX - rw + 28, compY, rw);
        }

        if (templ === this.selectedTemplate || this.selectedSystem !== null || this.selectedTemplate === null)
        {
          if (orient == TemplateOrientation.kLeft)
          {
            for (let sysIndex in comp.remap)
            {
              let compIndex: number = comp.remap[sysIndex];
              if (compIndex >= 0)
              {
                let sysComp = this.appService.ecsData.systems[sysIndex].components[compIndex];
                if (this.selectedSystem !== null && this.appService.ecsData.systems[sysIndex] !== this.selectedSystem)
                {
                  continue;
                }
                this.appService.ecsData.systems[sysIndex].data.visible = true;
                this.ctx.beginPath();
                this.ctx.moveTo(x + rw, compY);
                this.ctx.lineTo(sysComp.data.leftX, sysComp.data.y);
                this.ctx.lineWidth = 1;
                // this.ctx.strokeStyle = 'rgb(0, 0, 0)';
                this.ctx.strokeStyle = '#EBEDF2';
                this.ctx.stroke();
              }
            }
          }
          else if (orient == TemplateOrientation.kRight)
          {
            for (let sysIndex in comp.remap)
            {
              let compIndex: number = comp.remap[sysIndex];
              if (compIndex >= 0)
              {
                let sysComp = this.appService.ecsData.systems[sysIndex].components[compIndex];
                if (this.selectedSystem !== null && this.appService.ecsData.systems[sysIndex] !== this.selectedSystem)
                {
                  continue;
                }
                this.appService.ecsData.systems[sysIndex].data.visible = true;
                this.ctx.beginPath();
                this.ctx.moveTo(x, compY);
                this.ctx.lineTo(sysComp.data.rightX, sysComp.data.y);
                this.ctx.lineWidth = 1;
                // this.ctx.strokeStyle = 'rgb(0, 0, 0)';
                this.ctx.strokeStyle = '#EBEDF2';
                this.ctx.stroke();
              }
            }
          }
        }

        this.ctx.beginPath();
        if (orient == TemplateOrientation.kLeft)
          this.ctx.arc(x + rw, compY, 5, 0, 2 * Math.PI);
        else
          this.ctx.arc(x, compY, 5, 0, 2 * Math.PI);
        this.ctx.fillStyle = 'rgb(255, 255, 255)';
        this.ctx.fill();
        this.ctx.lineWidth = 1;
        this.ctx.strokeStyle = 'rgb(0, 0, 0)';
        this.ctx.stroke();

        compY += 0.5 * lineHeight;
      }
    }

    return { w: rw, h: rh };
  }

  drawSystem(x: number, y: number, sys: ECSSystem): { w: number, h: number }
  {
    if (!sys.data.visible)
    {
      return { w: 0, h: 0 };
    }

    const lineHeight = this.currentMode === 'Compact' ? 40 : 60;
    const rw = this.findMaxTextWidth(sys, 80) + 30;
    const rh = lineHeight + (this.currentMode === 'Compact' ? -0.5 * lineHeight : 0.5 * lineHeight * sys.components.length);

    if (this.currentMode === 'Detailed')
    {
      x += 0.5 * (this.maxWidthCenter - rw);
    }

    const radius = 5;

    this.ctx.beginPath();
    this.ctx.moveTo(x + radius, y);
    this.ctx.lineTo(x + rw - radius, y);
    this.ctx.quadraticCurveTo(x + rw, y, x + rw, y + radius);
    this.ctx.lineTo(x + rw, y + rh - radius);
    this.ctx.quadraticCurveTo(x + rw, y + rh, x + rw - radius, y + rh);
    this.ctx.lineTo(x + radius, y + rh);
    this.ctx.quadraticCurveTo(x, y + rh, x, y + rh - radius);
    this.ctx.lineTo(x, y + radius);
    this.ctx.quadraticCurveTo(x, y, x + radius, y);

    if (this.currentMode === 'Detailed')
    {
      this.ctx.moveTo(x, y + 0.5 * lineHeight);
      this.ctx.lineTo(x + rw, y + 0.5 * lineHeight);
    }

    this.ctx.closePath();

    this.ctx.fillStyle = sys.isScript ? '#86DBFD' : sys.isQuery ? '#CDBEF5' : 'rgb(255, 255, 255)';
    this.ctx.fill();
    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = 'rgb(0, 0, 0)';
    this.ctx.stroke();

    this.ctx.font = this.currentMode === 'Compact' ? '12pt Calibri' : 'bold 14pt Calibri';
    this.ctx.textBaseline = 'middle';
    this.ctx.textAlign = this.currentMode === 'Compact' ? 'left' : 'center';
    this.ctx.fillStyle = 'rgb(0, 0, 0)';
    this.ctx.fillText(sys.name, this.currentMode === 'Compact' ? x + 14 : x + 0.5 * rw, y + 0.25 * lineHeight, rw);

    sys.data.leftX = x;
    sys.data.rightX = x + rw;
    sys.data.y = y;
    sys.data.width = rw;
    sys.data.height = rh;

    if (this.currentMode === 'Compact')
    {
      this.ctx.beginPath();
      this.ctx.arc(x, y + 0.5 * rh, 5, 0, 2 * Math.PI);
      this.ctx.fillStyle = 'rgb(255, 255, 255)';
      this.ctx.fill();
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgb(0, 0, 0)';
      this.ctx.stroke();
    }
    else if (this.currentMode === 'Detailed')
    {
      let compX = x + 0.5 * rw;
      let compY = y + lineHeight;
      for (let comp of sys.components)
      {
        this.ctx.font = 'bold 12pt Calibri';
        this.ctx.textBaseline = 'middle';
        this.ctx.textAlign = 'center';
        this.ctx.fillStyle = 'rgb(0, 0, 0)';
        this.ctx.fillText(`${comp.name}:${comp.type}`, compX, compY, rw);

        this.ctx.beginPath();
        this.ctx.arc(x, compY, 5, 0, 2 * Math.PI);
        this.ctx.fillStyle = 'rgb(255, 255, 255)';
        this.ctx.fill();
        this.ctx.lineWidth = 1;
        this.ctx.strokeStyle = 'rgb(0, 0, 0)';
        this.ctx.stroke();

        this.ctx.beginPath();
        this.ctx.arc(x + rw, compY, 5, 0, 2 * Math.PI);
        this.ctx.fillStyle = 'rgb(255, 255, 255)';
        this.ctx.fill();
        this.ctx.lineWidth = 1;
        this.ctx.strokeStyle = 'rgb(0, 0, 0)';
        this.ctx.stroke();

        comp.data.leftX = x;
        comp.data.rightX = x + rw;
        comp.data.y = compY;

        compY += 0.5 * lineHeight;
      }
    }

    return { w: rw, h: rh };
  }

  maxWidthLeft = 0;
  maxWidthCenter = 0;

  draw()
  {
    // this.ctx.fillStyle = 'rgb(255, 255, 255)';
    // this.ctx.fillRect(0, 0, this.width, this.height);
    this.ctx.clearRect(0, 0, this.width, this.height);

    let xLeft = this._drawX;
    let yLeft = this._drawY;
    let xRight = this._drawX + this.maxWidthLeft + this.maxWidthCenter + 2 * 200;
    let yRight = this._drawY;
    let i = 0;

    const padding = this.currentMode === 'Compact' ? 10 : 20;

    for (let sys of this.appService.ecsData.systems)
    {
      sys.data.visible = false;
    }

    if (this.selectedSystem !== null)
    {
      this.selectedSystem.data.visible = true;
    }

    if (this.selectedSystem === null || this.selectedTemplate === null)
    {
      for (let templ of this.appService.ecsData.templates)
      {
        templ.data.visible = true;
      }
    }

    if (this.selectedTemplate !== null)
    {
      for (let templ of this.appService.ecsData.templates)
      {
        templ.data.visible = false;
      }
      this.selectedTemplate.data.visible = true;
    }

    if (this.selectedSystem !== null)
    {
      for (let templ of this.appService.ecsData.templates)
      {
        templ.data.visible = false;
        for (let sysIndex of templ.systems)
        {
          let sys = this.appService.ecsData.systems[sysIndex];
          if (sys === this.selectedSystem)
          {
            templ.data.visible = true;
            break;
          }
        }
      }
    }

    for (let templ of this.appService.ecsData.templates)
    {
      if (!templ.data.visible)
      {
        continue;
      }

      const orient = this.currentMode === 'Compact' ? TemplateOrientation.kLeft :
        (i++ % 2) == 0 ? TemplateOrientation.kLeft : TemplateOrientation.kRight;

      if (orient == TemplateOrientation.kRight)
      {
        const sz = this.drawTemplate(xRight, yRight, orient, templ);
        yRight += sz.h + padding;
      }
      else if (orient == TemplateOrientation.kLeft)
      {
        const sz = this.drawTemplate(xLeft, yLeft, orient, templ);
        yLeft += sz.h + padding;

        if (sz.w > this.maxWidthLeft)
        {
          this.maxWidthLeft = sz.w;
        }
      }
    }

    let x = this._drawX + this.maxWidthLeft + 200;
    let y = this._drawY;
    for (let sys of this.appService.ecsData.systems)
    {
      if (!sys.data.visible)
      {
        continue;
      }

      const sz = this.drawSystem(x, y, sys);
      if (sz.w > this.maxWidthCenter)
      {
        this.maxWidthCenter = sz.w;
      }

      y += sz.h + padding;
    }
  }

  get entityEditorWidth() { return ENTITY_EDITOR_WIDTH; }
}