#include <random>
#include <ctime>

#include <ecs/ecs.h>

#include "stages/update.stage.h"
#include "stages/render.stage.h"

#include "update.ecs.h"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scripthandle/scripthandle.h>
#include <scriptstdstring/scriptstdstring.h>

PULL_ESC_CORE;

template <size_t N>
void call(const eastl::array<int, N> &offets, uint8_t** compontes)
{
  for (int i = 0; i < N; ++i)
  {
    int offset = offsets[i];
    void *mem = compontes[i][offset];
  }
}

Camera2D camera;
int screen_width = 800;
int screen_height = 450;

template <size_t... I>
void testFunc(eastl::index_sequence<I...>);

template <>
void testFunc<>(eastl::index_sequence<0, 1, 2>)
{

}

void MessageCallback(const asSMessageInfo *msg, void *param)
{
  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";

  printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

void PrintString(const std::string &str)
{
  std::cout << "[Script]: " << str << std::endl;
}

int main()
{
  testFunc<0, 1, 2>(eastl::make_index_sequence<3>{});

  std::srand(unsigned(std::time(0)));

  InitWindow(screen_width, screen_height, "raylib [core] example - basic window");

  SetTargetFPS(60);

  asIScriptEngine *engine = asCreateScriptEngine();

  engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

  RegisterScriptHandle(engine);
  RegisterScriptArray(engine, true);
  RegisterStdString(engine);
  RegisterStdStringUtils(engine);

  int r = 0;
  r = engine->RegisterGlobalFunction("void print(string &in)", asFUNCTION(PrintString), asCALL_CDECL); assert(r >= 0);

  r = engine->RegisterObjectType("UpdateStage", sizeof(UpdateStage), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<UpdateStage>()); assert(r >= 0);
  engine->RegisterObjectProperty("UpdateStage", "float dt", asOFFSET(UpdateStage, dt));
  engine->RegisterObjectProperty("UpdateStage", "float total", asOFFSET(UpdateStage, total));

  r = engine->RegisterObjectType("vec2", sizeof(glm::vec2), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<glm::vec2>()); assert(r >= 0);
  engine->RegisterObjectProperty("vec2", "float x", asOFFSET(glm::vec2, x));
  engine->RegisterObjectProperty("vec2", "float y", asOFFSET(glm::vec2, y));

  asIScriptModule *module = engine->GetModule(0, asGM_ALWAYS_CREATE);
  {
    char *buffer = nullptr;

    FILE *file = nullptr;
    ::fopen_s(&file, "script.as", "rb");
    if (file)
    {
      size_t sz = ::ftell(file);
      ::fseek(file, 0, SEEK_END);
      sz = ::ftell(file) - sz;
      ::fseek(file, 0, SEEK_SET);

      buffer = new char[sz + 1];
      buffer[sz] = '\0';
      ::fread(buffer, 1, sz, file);
      ::fclose(file);
    }
    module->AddScriptSection("scriptMain", buffer, ::strlen(buffer));
  }
  module->Build();

  asIScriptContext *mainCtx = engine->CreateContext();
  asIScriptFunction *mainFn = engine->GetModule(0)->GetFunctionByDecl("ref@ main()");
  mainCtx->Prepare(mainFn);
  r = mainCtx->Execute();
  CScriptHandle *handle = (CScriptHandle*)mainCtx->GetReturnAddress();
  CScriptArray *systems = (CScriptArray*)handle->GetRef();

  asIScriptContext *sysCtx = engine->CreateContext();
  asIScriptFunction* sysFn = (asIScriptFunction*)((CScriptHandle*)systems->At(0))->GetRef();
  for (int i = 0; i < (int)sysFn->GetParamCount(); ++i)
  {
    int typeId = 0;
    asDWORD flags = 0;
    const char *name = nullptr;
    sysFn->GetParam(i, &typeId, &flags, &name);
    asITypeInfo *type = engine->GetTypeInfoById(typeId);
    const char *typeName = type->GetName();
    continue;
  }

  double _t = GetTime();
  for (int i = 0; i < 10000; ++i)
  {
    UpdateStage stage{ 0.1f, 10.f };
    glm::vec2 pos(1.f, 2.f);
    glm::vec2 vel(3.f, 4.f);

    sysCtx->Prepare(sysFn);
    sysCtx->SetArgAddress(0, &stage);
    sysCtx->SetArgAddress(1, &pos);
    sysCtx->SetArgAddress(2, &vel);
    sysCtx->Execute();
  }
  const double _d = GetTime() - _t;
  std::cout << _d << " s" << std::endl;
  std::cout << _d * 1e3 << " ms" << std::endl;
  std::cout << _d * 1e6 << " us" << std::endl;

  sysCtx->Release();

  mainCtx->Release();

  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ 0.f, 0.f };
  camera.rotation = 0.0f;
  camera.zoom = 1.5f;

  EntityManager::init();

  FILE *file = nullptr;
  ::fopen_s(&file, "entities.json", "rb");
  if (file)
  {
    size_t sz = ::ftell(file);
    ::fseek(file, 0, SEEK_END);
    sz = ::ftell(file) - sz;
    ::fseek(file, 0, SEEK_SET);

    char *buffer = new char[sz + 1];
    buffer[sz] = '\0';
    ::fread(buffer, 1, sz, file);
    ::fclose(file);

    JDocument doc;
    doc.Parse(buffer);
    delete[] buffer;

    assert(doc.HasMember("$entities"));
    assert(doc["$entities"].IsArray());
    for (int i = 0; i < (int)doc["$entities"].Size(); ++i)
    {
      const JValue &ent = doc["$entities"][i];
      g_mgr->createEntity(ent["$template"].GetString(), ent["$components"]);
    }
  }

  float totalTime = 0.f;
  while (!WindowShouldClose())
  {
    /*if (IsKeyReleased(KEY_SPACE))
    {
      eastl::vector<EntityId> eids;
      g_mgr->queryEids(eids, { { "timer", "timer" } });
      g_mgr->sendEvent(eids[0], EventOnSpawn{1000});
    }*/
    double t = GetTime();
    g_mgr->tick();

    const float dt = glm::clamp(GetFrameTime(), 0.f, 1.f / 60.f);
    g_mgr->tick(UpdateStage{ dt, totalTime });
    const float delta = (float)((GetTime() - t) * 1e3);

    totalTime += dt;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode2D(camera);
    g_mgr->tick(RenderStage{});
    EndMode2D();

    DrawText(FormatText("ECS time: %f ms", delta), 10, 30, 20, LIME);
    DrawText(FormatText("ECS count: %d", g_mgr->entities.size()), 10, 50, 20, LIME);
    DrawFPS(10, 10);

    g_mgr->tick(RenderHUDStage{});

    EndDrawing();
  }

  CloseWindow();

  return 0;
}