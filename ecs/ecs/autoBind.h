#pragma once

#include "ecs/hash.h"

namespace das
{
  class Module;
  class ModuleLibrary;
}

struct AutoBindDescription
{
  using AutoBindCallback = void (*)(das::Module &, das::ModuleLibrary &);

  ConstHashedString module;
  AutoBindCallback callback;

  static const AutoBindDescription *head;
  static int count;

  const AutoBindDescription *next = nullptr;

  AutoBindDescription(const ConstHashedString &_module, AutoBindCallback _cb):
    module(_module),
    callback(_cb)
  {
    next = AutoBindDescription::head;
    AutoBindDescription::head = this;
    ++AutoBindDescription::count;
  }
};

void do_auto_bind_module(const ConstHashedString &module, das::Module &, das::ModuleLibrary &);