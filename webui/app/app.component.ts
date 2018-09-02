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
  @ViewChild('canvas') canvas: ElementRef;

  ctx: CanvasRenderingContext2D = null;
  width: number = 0;
  height: number = 0;

  status: string = "Unknonwn";

  constructor(public appService : AppService, private streamService: StreamService, public coreService: CoreService, private zone: NgZone)
  {
    this._draw = this._draw.bind(this);
  }

  ngAfterViewInit()
  {
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
        let status = this.streamService.isConnected() ? "Online" : "Offline";
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

  onClick($event: MouseEvent)
  {
  }

  private _drawX = 10;
  private _drawY = 10;

  private _drag = false;
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
    this.ctx.font = 'bold 14pt Calibri';
    const m = this.ctx.measureText(src.name);
    let maxWidth = m.width;

    for (let comp of src.components)
    {
      this.ctx.font = 'bold 12pt Calibri';
      const m = this.ctx.measureText(`${comp.name}:${comp.type}`);
      if (m.width > maxWidth)
      {
        maxWidth = m.width;
      }
    }
    return Math.max(maxWidth, minWidth);
  }

  drawTemplate(x: number, y: number, orient: TemplateOrientation, templ): { w: number, h: number }
  {
    let rw = this.findMaxTextWidth(templ, 80) + 30;
    const rh = 60 + 30 * templ.components.length;

    if (orient == TemplateOrientation.kLeft)
    {
      x += this.maxWidthLeft - rw;
    }

    this.ctx.beginPath();
    this.ctx.rect(x, y, rw, rh);
    this.ctx.moveTo(x, y + 30);
    this.ctx.lineTo(x + rw, y + 30);
    this.ctx.fillStyle = 'rgb(255, 255, 255)';
    this.ctx.fill();
    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = 'rgb(0, 0, 0)';
    this.ctx.stroke();

    this.ctx.font = 'bold 14pt Calibri';
    this.ctx.textBaseline = 'middle';
    this.ctx.textAlign = 'center';
    this.ctx.fillStyle = 'rgb(0, 0, 0)';
    this.ctx.fillText(templ.name, x + 0.5 * rw, y + 0.5 * 30, rw);

    let compX = x + rw - 14;
    let compY = y + 60;
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

      if (orient == TemplateOrientation.kLeft)
      {
        for (let sysIndex in comp.remap)
        {
          let compIndex: number = comp.remap[sysIndex];
          if (compIndex >= 0)
          {
            let sysComp = this.appService.ecsData.systems[sysIndex].components[compIndex];
            this.appService.ecsData.systems[sysIndex].visible = true;
            this.ctx.beginPath();
            this.ctx.moveTo(x + rw, compY);
            this.ctx.lineTo(sysComp.leftX, sysComp.y);
            this.ctx.lineWidth = 1;
            this.ctx.strokeStyle = 'rgb(0, 0, 0)';
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
            this.appService.ecsData.systems[sysIndex].visible = true;
            this.ctx.beginPath();
            this.ctx.moveTo(x, compY);
            this.ctx.lineTo(sysComp.rightX, sysComp.y);
            this.ctx.lineWidth = 1;
            this.ctx.strokeStyle = 'rgb(0, 0, 0)';
            this.ctx.stroke();
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

      compY += 30;
    }

    return { w: rw, h: rh };
  }

  drawSystem(x: number, y: number, sys): { w: number, h: number }
  {
    if (!sys.visible)
    {
      return { w: 0, h: 0 };
    }

    const rw = this.findMaxTextWidth(sys, 80) + 30;
    const rh = 60 + 30 * sys.components.length;

    x += 0.5 * (this.maxWidthCenter - rw);

    this.ctx.beginPath();
    this.ctx.rect(x, y, rw, rh);
    this.ctx.moveTo(x, y + 30);
    this.ctx.lineTo(x + rw, y + 30);
    this.ctx.fillStyle = 'rgb(255, 255, 255)';
    this.ctx.fill();
    this.ctx.lineWidth = 1;
    this.ctx.strokeStyle = 'rgb(0, 0, 0)';
    this.ctx.stroke();

    this.ctx.font = 'bold 14pt Calibri';
    this.ctx.textBaseline = 'middle';
    this.ctx.textAlign = 'center';
    this.ctx.fillStyle = 'rgb(0, 0, 0)';
    this.ctx.fillText(sys.name, x + 0.5 * rw, y + 0.5 * 30, rw);

    let compX = x + 0.5 * rw;
    let compY = y + 60;
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

      compY += 30;
    }

    return { w: rw, h: rh };
  }

  maxWidthLeft = 0;
  maxWidthCenter = 0;

  draw()
  {
    this.ctx.fillStyle = 'rgb(255, 255, 255)';
    this.ctx.fillRect(0, 0, this.width, this.height);

    let xLeft = this._drawX;
    let yLeft = this._drawY;
    let xRight = this._drawX + this.maxWidthLeft + this.maxWidthCenter + 2 * 200;
    let yRight = this._drawY;
    let i = 0;

    for (let templ of this.appService.ecsData.templates)
    {
      const orient = (i++ % 2) == 0 ? TemplateOrientation.kLeft : TemplateOrientation.kRight;

      if (orient == TemplateOrientation.kRight)
      {
        const sz = this.drawTemplate(xRight, yRight, orient, templ);
        yRight += sz.h + 20;
      }
      else if (orient == TemplateOrientation.kLeft)
      {
        const sz = this.drawTemplate(xLeft, yLeft, orient, templ);
        yLeft += sz.h + 20;

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
        continue;

      const sz = this.drawSystem(x, y, sys);
      if (sz.w > this.maxWidthCenter)
      {
        this.maxWidthCenter = sz.w;
      }

      y += sz.h + 20;
    }
  }
}