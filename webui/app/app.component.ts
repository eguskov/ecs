import { Component, OnInit, AfterViewInit, NgZone, ViewChild, ElementRef } from '@angular/core';

import { StreamService } from './core/stream.service';
import { CoreService } from './core/core.service';
import { AppService } from './app.service';

enum TemplateOrientation
{
  kLeft,
  kRight
}

@Component({
  selector: 'my-app',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent implements OnInit
{
  @ViewChild('root') root: ElementRef;
  @ViewChild('canvas') canvas: ElementRef;

  ctx: CanvasRenderingContext2D = null;
  width: number = 0;
  height: number = 0;

  status: string = 'Unknonwn';

  modes: string[] = ['Detailed', 'Compact'];
  currentMode: string = 'Compact';

  constructor(public appService: AppService, private streamService: StreamService, public coreService: CoreService, private zone: NgZone)
  {
    this._draw = this._draw.bind(this);
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

  _draw()
  {
    this.zone.runOutsideAngular(() =>
    {
      this.draw();
      requestAnimationFrame(this._draw);
    });
  }

  selectedTemplate: any = null;
  selectedSystem: any = null;

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
      if (templ.visible && clickX >= templ.leftX && clickX <= templ.rightX && clickY >= templ.y && clickY <= templ.y + templ.height)
      {
        this.selectedTemplate = this.selectedTemplate === templ ? null : templ;
        this.selectedSystem = null;
        return;
      }
    }

    for (let sys of this.appService.ecsData.systems)
    {
      if (sys.visible && clickX >= sys.leftX && clickX <= sys.rightX && clickY >= sys.y && clickY <= sys.y + sys.height)
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

  findMaxTextWidth(src, minWidth: number): number
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

  drawTemplate(x: number, y: number, orient: TemplateOrientation, templ): { w: number, h: number }
  {
    const lineHeight = this.currentMode === 'Compact' ? 40 : 60;

    let rw = this.findMaxTextWidth(templ, 80) + 30;
    const rh = lineHeight + (this.currentMode === 'Compact' ? -0.5 * lineHeight : 0.5 * lineHeight * templ.components.length);

    if (orient == TemplateOrientation.kLeft)
    {
      x += this.maxWidthLeft - rw;
    }

    this.ctx.beginPath();
    this.ctx.rect(x, y, rw, rh);
    if (this.currentMode === 'Detailed')
    {
      this.ctx.moveTo(x, y + 0.5 * lineHeight);
      this.ctx.lineTo(x + rw, y + 0.5 * lineHeight);
    }
    this.ctx.fillStyle = templ === this.selectedTemplate ? '#74EDD4' : 'rgb(255, 255, 255)';
    this.ctx.fill();
    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = 'rgb(0, 0, 0)';
    this.ctx.stroke();

    this.ctx.font = this.currentMode === 'Compact' ? '12pt Calibri' : 'bold 14pt Calibri';
    this.ctx.textBaseline = 'middle';
    this.ctx.textAlign = 'center';
    this.ctx.fillStyle = 'rgb(0, 0, 0)';
    this.ctx.fillText(templ.name, x + 0.5 * rw, y + 0.25 * lineHeight, rw);

    templ.leftX = x;
    templ.rightX = x + rw;
    templ.y = y;
    templ.width = rw;
    templ.height = rh;

    if (this.currentMode === 'Compact')
    {
      if (templ === this.selectedTemplate || this.selectedSystem !== null || this.selectedTemplate === null)
      {
        for (let sysIndex of templ.systems)
        {
          let sys = this.appService.ecsData.systems[sysIndex];
          if (this.selectedSystem !== null && sys !== this.selectedSystem)
            continue;

          sys.visible = true;
          this.ctx.beginPath();
          if (orient == TemplateOrientation.kLeft)
          {
            this.ctx.moveTo(x + rw, y + 0.5 * rh);
            this.ctx.lineTo(sys.leftX, sys.y + 0.5 * sys.height);
          }
          else if (orient == TemplateOrientation.kRight)
          {
            this.ctx.moveTo(x, y + 0.5 * rh);
            this.ctx.lineTo(sys.rightX, sys.y + 0.5 * sys.height);
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
                this.appService.ecsData.systems[sysIndex].visible = true;
                this.ctx.beginPath();
                this.ctx.moveTo(x + rw, compY);
                this.ctx.lineTo(sysComp.leftX, sysComp.y);
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
                this.appService.ecsData.systems[sysIndex].visible = true;
                this.ctx.beginPath();
                this.ctx.moveTo(x, compY);
                this.ctx.lineTo(sysComp.rightX, sysComp.y);
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

  drawSystem(x: number, y: number, sys): { w: number, h: number }
  {
    if (!sys.visible)
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

    this.ctx.beginPath();
    this.ctx.rect(x, y, rw, rh);
    if (this.currentMode === 'Detailed')
    {
      this.ctx.moveTo(x, y + 0.5 * lineHeight);
      this.ctx.lineTo(x + rw, y + 0.5 * lineHeight);
    }
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

    sys.leftX = x;
    sys.rightX = x + rw;
    sys.y = y;
    sys.width = rw;
    sys.height = rh;

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

        comp.leftX = x;
        comp.rightX = x + rw;
        comp.y = compY;

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
      sys.visible = false;
    }

    if (this.selectedSystem !== null)
    {
      this.selectedSystem.visible = true;
    }

    if (this.selectedSystem === null || this.selectedTemplate === null)
    {
      for (let templ of this.appService.ecsData.templates)
      {
        templ.visible = true;
      }
    }

    if (this.selectedSystem !== null)
    {
      for (let templ of this.appService.ecsData.templates)
      {
        templ.visible = false;
        for (let sysIndex of templ.systems)
        {
          let sys = this.appService.ecsData.systems[sysIndex];
          if (sys === this.selectedSystem)
          {
            templ.visible = true;
            break;
          }
        }
      }
    }

    for (let templ of this.appService.ecsData.templates)
    {
      if (!templ.visible)
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
      if (!sys.visible)
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
}