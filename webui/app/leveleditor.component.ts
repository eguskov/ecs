import { Component, OnInit, AfterViewInit, NgZone, ViewChild, ElementRef, OnDestroy, ViewEncapsulation, HostListener, TemplateRef } from '@angular/core';

import { StreamService } from './core/stream.service';
import { CoreService } from './core/core.service';
import { AppService, ECSTemplate, ECSSystem } from './app.service';
import { Subscription } from 'rxjs/Subscription';
import { Hotkey, DrawerService, CodeEditorComponent } from '@swimlane/ngx-ui';

const ENTITY_EDITOR_WIDTH = /* 300 */ 0;
const GRID_WIDTH = 32.0;
const GRID_HEIGHT = 32.0;
const STICK_TO_GRID_WIDTH = 0.5 * GRID_HEIGHT;
const STICK_TO_GRID_HEIGHT = 0.5 * GRID_HEIGHT;
const SELECT_WIDTH = 10;
const SELECT_HEIGHT = 10;

function clone(obj)
{
  return JSON.parse(JSON.stringify(obj));
}

function isEqual(a, b): boolean
{
  return JSON.stringify(a) === JSON.stringify(b);
}

function drawHead (ctx, x0, y0, x1, y1, x2, y2, style, color, width) {
  if (typeof(x0) == 'string') {
    x0 = parseInt(x0);
  }
  if (typeof(y0) == 'string') {
    y0 = parseInt(y0);
  }
  if (typeof(x1) == 'string') {
    x1 = parseInt(x1);
  }
  if (typeof(y1) == 'string') {
    y1 = parseInt(y1);
  }
  if (typeof(x2) == 'string') {
    x2 = parseInt(x2);
  }
  if (typeof(y2) == 'string') {
    y2 = parseInt(y2);
  }
  
  var radius = 3,
      twoPI = 2 * Math.PI;
  
  ctx.save();
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.fillStyle = color;
  ctx.lineWidth = width;
  ctx.moveTo(x0, y0);
  ctx.lineTo(x1, y1);
  ctx.lineTo(x2, y2);
  
  switch (style) {
    case 0:
      var backdist = Math.sqrt(((x2 - x0) * (x2 - x0)) + ((y2 - y0) * (y2 - y0)));
      ctx.arcTo(x1, y1, x0, y0, .55 * backdist);
      ctx.fill();
      break;
    case 1:
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x0, y0);
      ctx.fill();
      break;
    case 2:
      ctx.stroke();
      break;
    case 3:
      var cpx = (x0 + x1 + x2) / 3;
      var cpy = (y0 + y1 + y2) / 3;
      ctx.quadraticCurveTo(cpx, cpy, x0, y0);
      ctx.fill();
      break;
    case 4:
      var cp1x, cp1y, cp2x, cp2y, backdist: number;
      var shiftamt = 5;
      if (x2 == x0) {
        backdist = y2 - y0;
        cp1x = (x1 + x0) / 2;
        cp2x = (x1 + x0) / 2;
        cp1y = y1 + backdist / shiftamt;
        cp2y = y1 - backdist / shiftamt;
      } else {
        backdist = Math.sqrt(((x2 - x0) * (x2 - x0)) + ((y2 - y0) * (y2 - y0)));
        var xback = (x0 + x2) / 2;
        var yback = (y0 + y2) / 2;
        var xmid = (xback + x1) / 2;
        var ymid = (yback + y1) / 2;
        var m = (y2 - y0) / (x2 - x0);
        var dx = (backdist / (2 * Math.sqrt(m * m + 1))) / shiftamt;
        var dy = m * dx;
        cp1x = xmid - dx;
        cp1y = ymid - dy;
        cp2x = xmid + dx;
        cp2y = ymid + dy;
      }
      ctx.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x0, y0);
      ctx.fill();
      break;
  }
  ctx.restore();
}

function drawArrow(ctx, x1, y1, x2, y2, style, which, angle, d, color, width) {
  if (typeof(x1) == 'string') {
    x1 = parseInt(x1);
  }
  if (typeof(y1) == 'string') {
    y1 = parseInt(y1);
  }
  if (typeof(x2) == 'string') {
    x2 = parseInt(x2);
  }
  if (typeof(y2) == 'string') {
    y2 = parseInt(y2);
  }
  style = typeof(style) != 'undefined' ? style : 3;
  which = typeof(which) != 'undefined' ? which : 1;
  angle = typeof(angle) != 'undefined' ? (angle * Math.PI / 180.0) : Math.PI / 9;
  d = typeof(d) != 'undefined' ? d : 10;
  color = typeof(color) != 'undefined' ? color : '#000';
  width = typeof(width) != 'undefined' ? width : 1;
  var toDrawHead = typeof(style) != 'function' ? drawHead : style;
  var dist = Math.sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  var ratio = (dist - d / 3) / dist;
  var tox, toy, fromx, fromy;
  if (which & 1) {
    tox = Math.round(x1 + (x2 - x1) * ratio);
    toy = Math.round(y1 + (y2 - y1) * ratio);
  } else {
    tox = x2;
    toy = y2;
  }
  
  if (which & 2) {
    fromx = x1 + (x2 - x1) * (1 - ratio);
    fromy = y1 + (y2 - y1) * (1 - ratio);
  } else {
    fromx = x1;
    fromy = y1;
  }
  
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.moveTo(fromx, fromy);
  ctx.lineTo(tox, toy);
  ctx.stroke();
  
  var lineangle = Math.atan2(y2 - y1, x2 - x1);
  var h = Math.abs(d / Math.cos(angle));
  if (which & 1) {
    var angle1 = lineangle + Math.PI + angle;
    var topx = x2 + Math.cos(angle1) * h;
    var topy = y2 + Math.sin(angle1) * h;
    var angle2 = lineangle + Math.PI - angle;
    var botx = x2 + Math.cos(angle2) * h;
    var boty = y2 + Math.sin(angle2) * h;
    toDrawHead(ctx, topx, topy, x2, y2, botx, boty, style, color, width);
  }
  
  if (which & 2) {
    var angle1 = lineangle + angle;
    var topx = x1 + Math.cos(angle1) * h;
    var topy = y1 + Math.sin(angle1) * h;
    var angle2 = lineangle - angle;
    var botx = x1 + Math.cos(angle2) * h;
    var boty = y1 + Math.sin(angle2) * h;
    toDrawHead(ctx, topx, topy, x1, y1, botx, boty, style, color, width);
  }
}

@Component({
  selector: 'level-editor',
  templateUrl: './leveleditor.component.html',
  styleUrls: ['./app.component.css'],
  encapsulation: ViewEncapsulation.None
})
export class LevelEditorComponent implements OnInit, OnDestroy
{
  @ViewChild('poslessEntityTemplate') poslessEntityTemplate: TemplateRef<any>;
  @ViewChild('editEntityTemplate') editEntityTemplate: TemplateRef<any>;
  @ViewChild('editComponentTemplate') editComponentTemplate: TemplateRef<any>;
  
  @ViewChild('editorRoot') root: ElementRef;
  @ViewChild('editorCanvas') canvas: ElementRef;

  ctx: CanvasRenderingContext2D = null;
  width: number = 0;
  height: number = 0;

  selectedTemplate: any = null;

  editorConfig = {
    lineNumbers: true,
    theme: 'dracula',
    mode: {
      name: 'json',
      json: true
    }
  };

  private _entities = [];
  private _currentEntity: any = { $template: '', $components: {}, $componentsStr: {} };
  private _lastEntity: any;

  private _dataSubscription: Subscription;

  constructor(public appService: AppService, private streamService: StreamService, public coreService: CoreService, private zone: NgZone, private drawerService: DrawerService)
  {
    this._draw = this._draw.bind(this);
    this._onDataUpdate = this._onDataUpdate.bind(this);
    this._dataSubscription = this.appService.observable.subscribe(this._onDataUpdate);
  }

  ngOnInit(): void
  {
  }

  ngOnDestroy(): void
  {
    this._dataSubscription.unsubscribe();
  }

  @Hotkey('ctrl+e', 'Add entity')
  onKeyAddEntity()
  {
    this._indexForEdit = -1;
    this._addEntity = true;
  }

  @Hotkey('ctrl+shift+e', 'Add last entity')
  onKeyAddLastEntity()
  {
    this._indexForEdit = -1;
    if (this.selectedTemplate)
    {
      this._addLastEntity = true;
    }
  }
  
  _poslessEntities: any = [];
  
  @Hotkey('alt+e', 'Add pos-less entity')
  onKeyAddPoslessEntity()
  {
    this._indexForEdit = -1;
    this.drawerService.destroyAll();
    this.drawerService.create({
      direction: 'left',
      template: this.poslessEntityTemplate,
      size: 50.0,
      context: 'Alert Everyone!'
    });
  }

  private _editComponentDrawer;

  onEditComponent(e, c)
  {
    console.log(e.$componentsStr[c]);

    this._editComponentDrawer = this.drawerService.create({
      direction: 'left',
      template: this.editComponentTemplate,
      size: 40.0,
      context: { entity: e, component: c, value: e.$componentsStr[c] }
    });
  }

  onSaveComponent($event, component, entity, code: CodeEditorComponent)
  {
    this.drawerService.destroy(this._editComponentDrawer);
    entity.$componentsStr[component] = code.value;

    console.log('onSaveComponent', entity);
  }

  @Hotkey('ctrl+s', 'Save to entities.json')
  onKeySave()
  {
    console.log(this._entities);
    console.log(this._poslessEntities);

    let res = { $entities: [] };

    for (let entities of [ this._poslessEntities, this._entities ])
    {
      for (let e of entities)
      {
        const templ = this.appService.ecsTemplates.find(t => t.$name == e.$template);

        let $components = {};
        for (let k in e.$componentsStr)
        {
          const value = JSON.parse(e.$componentsStr[k]);
          if (!isEqual(templ.$allComponents[k].$value, value))
          {
            $components[k] = value;
          }
        }
        res.$entities.push({ $template: e.$template, $components: $components });
      }
    }

    console.log(JSON.stringify(res, null, 2));

    this.appService.saveECSEntities(res);
  }

  @HostListener('window:resize', ['$event'])
  onResize(event)
  {
    this.resize();
  }

  resize()
  {
    const body = document.querySelector('body');
    const header = document.querySelector('.ngx-tabs-list');
    (<HTMLCanvasElement>this.canvas.nativeElement).width = body.offsetWidth - ENTITY_EDITOR_WIDTH - 20;
    (<HTMLCanvasElement>this.canvas.nativeElement).height = body.offsetHeight - header.clientHeight - 40;

    this.width = (<HTMLCanvasElement>this.canvas.nativeElement).width;
    this.height = (<HTMLCanvasElement>this.canvas.nativeElement).height;

    this._drawX = 0.5 * this.width;
    this._drawY = 0.5 * this.height;

    console.log(`Context for LevelEditor is ready: ${this.width} x ${this.height}`);
  }

  ngAfterViewInit()
  {
    this.ctx = this.canvas.nativeElement.getContext('2d');

    this.resize();

    if (this.ctx)
      this._draw();
  }

  _draw()
  {
    this.zone.runOutsideAngular(() =>
    {
      this.draw();
      requestAnimationFrame(this._draw);
    });
  }

  drawGrid()
  {
    let x = this._drawX;
    let y = this._drawY;
    while (x > 0)
    {
      this.ctx.beginPath();
      this.ctx.moveTo(x, 0.0);
      this.ctx.lineTo(x, 0.0 + this.height);
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      this.ctx.stroke();

      x -= GRID_WIDTH;
    }
    while (y > 0)
    {
      this.ctx.beginPath();
      this.ctx.moveTo(0.0, y);
      this.ctx.lineTo(0.0 + this.width, y);
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      this.ctx.stroke();

      y -= GRID_WIDTH;
    }
    x = this._drawX + GRID_WIDTH;
    y = this._drawY + GRID_HEIGHT;
    while (x < this.width)
    {
      this.ctx.beginPath();
      this.ctx.moveTo(x, 0.0);
      this.ctx.lineTo(x, 0.0 + this.height);
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      this.ctx.stroke();

      x += GRID_WIDTH;
    }
    while (y < this.height)
    {
      this.ctx.beginPath();
      this.ctx.moveTo(0.0, y);
      this.ctx.lineTo(0.0 + this.width, y);
      this.ctx.lineWidth = 1;
      this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      this.ctx.stroke();

      y += GRID_WIDTH;
    }

    this.ctx.beginPath();
    this.ctx.arc(this._drawX, this._drawY, 5, 0, 2 * Math.PI);
    this.ctx.fillStyle = 'rgb(255, 255, 255, 0.1)';
    this.ctx.fill();
  }

  private _gridX = 0;
  private _gridY = 0;

  draw()
  {
    this.ctx.clearRect(0, 0, this.width, this.height);
    this.drawGrid();

    if (this._addEntity || this._addLastEntity)
    {
      const x = this._drawX + Math.round((this._mouseX - this._drawX) / STICK_TO_GRID_WIDTH) * STICK_TO_GRID_WIDTH;
      const y = this._drawY + Math.round((this._mouseY - this._drawY) / STICK_TO_GRID_HEIGHT) * STICK_TO_GRID_HEIGHT;
      this.ctx.beginPath();
      this.ctx.arc(x, y, 5, 0, 2 * Math.PI);
      this.ctx.fillStyle = this._addLastEntity ? 'rgb(0, 0, 255)' : 'rgb(255, 0, 0)';
      this.ctx.fill();

      this.ctx.font = '12pt Calibri';
      this.ctx.textBaseline = 'top';
      this.ctx.textAlign = 'center';
      this.ctx.fillStyle = 'rgb(255, 255, 255)';
      if (this._addLastEntity)
      {
        this.ctx.fillText(`${this.selectedTemplate.$name} (${this._gridX}, ${this._gridY})`, x, y + 5);
      }
      else
      {
        this.ctx.fillText(`(${this._gridX}, ${this._gridY})`, x, y + 5);
      }
    }

    for (let e of this._entities)
    {
      const shape = e.$components.collision_shape;
      const rect = e.$components.collision_rect;
      const auto_move = e.$components.auto_move;
      const vel = e.$components.vel;

      const x = this._drawX + e.$components.pos[0];
      const y = this._drawY + e.$components.pos[1];
      this.ctx.beginPath();
      this.ctx.arc(x, y, 5, 0, 2 * Math.PI);
      this.ctx.fillStyle = 'rgb(0, 255, 0)';
      this.ctx.fill();

      if (shape && shape[0] && shape[0].type === 'box')
      {
        const size = shape[0].size;
        this.ctx.beginPath();
        this.ctx.rect(x, y, size[0], size[1]);
        this.ctx.fillStyle = 'rgb(0, 255, 0, 0.1)';
        this.ctx.fill();

        if (auto_move && vel)
        {
          const dst = [ vel[0] * auto_move.length, vel[1] * auto_move.length ];
          drawArrow(this.ctx,
            x + 0.5 * size[0], y + 0.5 * size[1],
            x + 0.5 * size[0] + dst[0], y + 0.5 * size[1] + dst[1],
            1, 1, 20, 10, 'rgba(0, 255, 0, 0.1)', 1); 
        }
      }

      if (rect)
      {
        const size = [ rect[2], rect[3] ];
        this.ctx.beginPath();
        this.ctx.rect(x, y, size[0], size[1]);
        this.ctx.fillStyle = 'rgb(0, 255, 0, 0.1)';
        this.ctx.fill();

        if (auto_move && vel)
        {
          const dst = [ vel[0] * auto_move.length, vel[1] * auto_move.length ];
          drawArrow(this.ctx,
            x + 0.5 * size[0], y + 0.5 * size[1],
            x + 0.5 * size[0] + dst[0], y + 0.5 * size[1] + dst[1],
            1, 1, 20, 10, 'rgba(0, 255, 0, 0.1)', 1); 
        }
      }

      const gx = x - 0.5 * SELECT_WIDTH;
      const gy = y - 0.5 * SELECT_HEIGHT;

      if (this._mouseX > gx && this._mouseX < gx + SELECT_WIDTH && this._mouseY > gy && this._mouseY < gy + SELECT_HEIGHT)
      {
        // TODO: Draw on hover
        this.ctx.font = '8pt Calibri';
        this.ctx.textBaseline = 'top';
        this.ctx.textAlign = 'center';
        this.ctx.fillStyle = 'rgb(255, 255, 255)';
        if (e.$components.key !== undefined)
        {
          this.ctx.fillText(`${e.$components.key}: ${e.$template}`, x, y + 5);
        }
        else
        {
          this.ctx.fillText(`${e.$template}`, x, y + 5);
        }
      }
    }
  }

  getPoslessEntityTitle(e)
  {
    if (e.$components.key !== undefined)
    {
      return `${e.$components.key}: ${e.$template}`;
    }
    return `${e.$template}`;
  }

  private _indexForEdit = -1;

  onClick($event: MouseEvent)
  {
    console.log('onClick!', this._skipNextClick);

    if (this._skipNextClick)
    {
      this._skipNextClick = false;
      return;
    }

    const clickX = $event.offsetX;
    const clickY = $event.offsetY;

    if (this._addEntity)
    {
      this._addEntity = false;
      this._entityPosX = this._gridX;
      this._entityPosY = this._gridY;
      this._currentEntity = { $template: '', $components: {}, $componentsStr: {} };

      if (this.selectedTemplate)
      {
        for (let k in this.selectedTemplate.$allComponents)
        {
          this._currentEntity.$components[k] = clone(this.selectedTemplate.$allComponents[k].$value);
          this._currentEntity.$componentsStr[k] = clone(this.selectedTemplate.$allComponents[k].$valueStr);
        }
      }

      this.drawerService.create({
        direction: 'left',
        template: this.editEntityTemplate,
        size: 50.0,
        context: 'Alert Everyone!'
      });
    }
    else if (this._addLastEntity)
    {
      this._addLastEntity = false;
      this._currentEntity = clone(this._lastEntity);
      if (this.selectedTemplate.$allComponents.pos !== undefined)
      {
        this._currentEntity.$components.pos = [ this._gridX, this._gridY ];
        this._currentEntity.$componentsStr.pos = JSON.stringify(this._currentEntity.$components.pos, null, 2);
      }
      if (this.selectedTemplate.$allComponents.pos !== undefined)
      {
        this._entities.push(this._currentEntity);
      }
      else
      {
        this._poslessEntities.push(this._currentEntity);
      }
      this._currentEntity = { $template: '', $components: {}, $componentsStr: {} };
    }
    else
    {
      let i = -1;
      for (let e of this._entities)
      {
        ++i;

        const x = this._drawX + e.$components.pos[0];
        const y = this._drawY + e.$components.pos[1];
        const gx = x - 0.5 * SELECT_WIDTH;
        const gy = y - 0.5 * SELECT_HEIGHT;
        if (this._mouseX > gx && this._mouseX < gx + SELECT_WIDTH && this._mouseY > gy && this._mouseY < gy + SELECT_HEIGHT)
        {
          // TODO: Edit entity
          this.selectedTemplate = this.appService.ecsTemplates.find(t => t.$name === e.$template);
          this._indexForEdit = i;
          this._currentEntity = clone(e);
          for (let k in this.selectedTemplate.$allComponents)
          {
            if (this._currentEntity.$components[k] === undefined)
            {
              this._currentEntity.$components[k] = clone(this.selectedTemplate.$allComponents[k].$value);
              this._currentEntity.$componentsStr[k] = clone(this.selectedTemplate.$allComponents[k].$valueStr);
            }
          }
          this.drawerService.create({
            direction: 'left',
            template: this.editEntityTemplate,
            size: 50.0,
            context: 'Alert Everyone!'
          });
          break;
        }
      }
    }
  }

  private _drawX = 0;
  private _drawY = 0;

  private _drag = false;
  private _skipNextClick = false;
  private _dragX = 0;
  private _dragY = 0;

  private _mouseX = 0;
  private _mouseY = 0;

  private _dragDistX = 0;
  private _dragDistY = 0;

  onMouseDown($event: MouseEvent)
  {
    this._drag = true;
    this._dragX = $event.offsetX;
    this._dragY = $event.offsetY;
  }

  onMouseMove($event: MouseEvent)
  {
    this._mouseX = $event.offsetX;
    this._mouseY = $event.offsetY;

    this._gridX = Math.round((this._mouseX - this._drawX) / STICK_TO_GRID_WIDTH) * STICK_TO_GRID_WIDTH;
    this._gridY = Math.round((this._mouseY - this._drawY) / STICK_TO_GRID_HEIGHT) * STICK_TO_GRID_HEIGHT;

    if (this._drag)
    {
      const dx = $event.offsetX - this._dragX;
      const dy = $event.offsetY - this._dragY;

      this._drawX += dx;
      this._drawY += dy;

      this._dragX = $event.offsetX;
      this._dragY = $event.offsetY;

      this._dragDistX += dx;
      this._dragDistY += dy;
      if (Math.abs(this._dragDistX) > 1 || Math.abs(this._dragDistY) > 1)
      {
        this._skipNextClick = true;
      }
    }
  }

  onMouseUp($event: MouseEvent)
  {
    this._drag = false;
    this._dragDistX = 0;
    this._dragDistY = 0;
  }

  onMouseLeave($event: MouseEvent)
  {
    this._drag = false;
    this._dragDistX = 0;
    this._dragDistY = 0;
  }

  private _addEntity = false;
  private _addLastEntity = false;
  private _entityPosX = 0;
  private _entityPosY = 0;

  onSelectTemplate($event)
  {
    this.selectedTemplate = $event[0];

    this._currentEntity = { $template: '', $components: {}, $componentsStr: {} };

    for (let k in this.selectedTemplate.$allComponents)
    {
      this._currentEntity.$components[k] = clone(this.selectedTemplate.$allComponents[k].$value);
      this._currentEntity.$componentsStr[k] = clone(this.selectedTemplate.$allComponents[k].$valueStr);
    }
  }

  _onDataUpdate(id: string)
  {
    if (id === 'ecsEntities')
    {
      this._poslessEntities = clone(this.appService.ecsEntities.$entities.filter(e => this.appService.ecsTemplates.find(t => t.$name === e.$template).$allComponents.pos === undefined));
      this._entities = clone(this.appService.ecsEntities.$entities.filter(e => this.appService.ecsTemplates.find(t => t.$name === e.$template).$allComponents.pos !== undefined));

      for (let entities of [ this._poslessEntities, this._entities ])
      {
        for (let e of entities)
        {
          const templ = this.appService.ecsTemplates.find(t => t.$name == e.$template);

          e.$componentsStr = {};
          for (let k in templ.$allComponents)
          {
            if (e.$components[k] === undefined)
            {
              e.$components[k] = clone(templ.$allComponents[k].$value);
            }
            e.$componentsStr[k] = JSON.stringify(e.$components[k], null, 2);
          }
        }
      }
      console.log('_poslessEntities', this._poslessEntities);
      console.log('_entities', this._entities);
    }
  }

  onDeleteEntity($event)
  {
    if (this._indexForEdit >= 0)
    {
      this._entities.splice(this._indexForEdit, 1);
      this._indexForEdit = -1;
      this.drawerService.destroyAll();
    }
  }

  onSaveEntity($event)
  {
    console.log('onSaveEntity', this._currentEntity);
    // TODO: Read values from CodeMirrior
    this._currentEntity.$template = this.selectedTemplate.$name;
    for (let k in this._currentEntity.$components)
    {
      this._currentEntity.$components[k] = JSON.parse(this._currentEntity.$componentsStr[k]);
    }
    if (this._indexForEdit < 0 && this.selectedTemplate.$allComponents.pos !== undefined)
    {
      this._currentEntity.$components.pos = [ this._entityPosX, this._entityPosY ];
      this._currentEntity.$componentsStr.pos = JSON.stringify(this._currentEntity.$components.pos, null, 2);
    }
    // for (let k in this._currentEntity.$components)
    // {
    //   if (k === 'pos')
    //     continue;
    //   if (isEqual(this.selectedTemplate.$allComponents[k].$value, this._currentEntity.$components[k]))
    //   {
    //     delete this._currentEntity.$components[k];
    //     delete this._currentEntity.$componentsStr[k];
    //   }
    // }
    console.log(this._currentEntity);
    if (this._indexForEdit >= 0)
    {
      this._entities[this._indexForEdit] = clone(this._currentEntity);
      this._indexForEdit = -1;
    }
    else
    {
      this._lastEntity = clone(this._currentEntity);
      if (this.selectedTemplate.$allComponents.pos !== undefined)
      {
        this._entities.push(this._currentEntity);
      }
      else
      {
        this._poslessEntities.push(this._currentEntity);
      }
    }
    this._currentEntity = { $template: '', $components: {}, $componentsStr: {} };

    this.drawerService.destroyAll();
  }

  onCancelEntity($event)
  {
    this.drawerService.destroyAll();
  }
}