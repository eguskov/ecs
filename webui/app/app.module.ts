import { APP_BASE_HREF } from '@angular/common';
import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

import { CoreModule } from './core/core.module';

import { AppComponent } from './app.component';

import { NgxUIModule } from '@swimlane/ngx-ui';
import { NgxDnDModule } from '@swimlane/ngx-dnd';
import { AppService } from './app.service';

@NgModule({
  imports: [BrowserModule, BrowserAnimationsModule, NgxUIModule, NgxDnDModule, CoreModule.forRoot()],
  declarations: [AppComponent],
  bootstrap: [AppComponent],
  providers: [{ provide: APP_BASE_HREF, useValue: '/' }, AppService]
})
export class AppModule { }
