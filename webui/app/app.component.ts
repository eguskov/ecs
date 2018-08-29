import { Component, OnInit, AfterViewInit, NgZone, ViewChild, ElementRef } from '@angular/core';

import { StreamService } from './core/stream.service';
import { CoreService } from './core/core.service';
import { bind } from '@angular/core/src/render3/instructions';

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

  test: Array<{ x: number, y: number }> = [];

  constructor(private streamService: StreamService, public coreService: CoreService, private zone: NgZone)
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
    this.test.push({ x: $event.offsetX, y: $event.offsetY });
  }

  draw()
  {
    this.ctx.fillStyle = 'rgb(255, 255, 255)';
    this.ctx.fillRect(0, 0, this.width, this.height);

    this.ctx.lineWidth = 2;
    this.ctx.strokeStyle = '#000000';
    this.ctx.beginPath();
    for (let { x, y } of this.test)
    {
      // this.ctx.moveTo(x, y);
      this.ctx.rect(x, y, 50, 50);
    }
    this.ctx.stroke();
  }
}