#include "webui.h"

#include <ecs/ecs.h>

#include <script-ecs.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <mongoose.h>
#include <bson.h>
#include <bsonUtils.h>

#include <EASTL/queue.h>
#include <EASTL/unique_ptr.h>

#include <rapidjson/prettywriter.h>

#include <sstream>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>

#include "script-update.h"

struct WebsocketServer
{
  struct mg_mgr mgr;
  struct mg_connection *nc = nullptr;

  ~WebsocketServer()
  {
    mg_mgr_free(&mgr);
  }

  void init(const char *address);

  void poll()
  {
    mg_mgr_poll(&mgr, 100);
  }
};

struct QueueItem
{
  struct mg_connection *conn;

  JDocument doc;

  using Handler = eastl::function<void (const JDocument&, BsonStream&)>;
  Handler handler;

  QueueItem() = default;
  QueueItem(const QueueItem &assign) :
    conn(assign.conn),
    handler(assign.handler)
  {
    doc.CopyFrom(assign.doc, doc.GetAllocator());
  }

  QueueItem& operator=(const QueueItem &assign)
  {
    if (this == &assign)
      return *this;

    new (this) QueueItem(assign);

    return *this;
  }
};

static WebsocketServer *server = nullptr;

static void run_on_main_thread_and_wait(struct mg_connection *conn, const JDocument &doc, QueueItem::Handler &&handler);

static char* get_file_content(const char *path)
{
  char *buffer = nullptr;

  FILE *file = nullptr;
  ::fopen_s(&file, path, "rb");
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

  return buffer;
}

static void send(BsonStream &bson, struct mg_connection *conn)
{
  const uint8_t *d = nullptr;
  size_t sz = 0;
  eastl::tie(d, sz) = bson.closeAndGetData();

  mg_send_websocket_frame(conn, WEBSOCKET_OP_BINARY, d, sz);
}

static void send(const char *message, struct mg_connection *conn)
{
  mg_send_websocket_frame(conn, WEBSOCKET_OP_TEXT, message, ::strlen(message));
}

static void send_json_file(const char *name, const char *path, struct mg_connection *conn)
{
  char *buffer = get_file_content(path);
  ASSERT(buffer != nullptr);

  if (buffer)
  {
    std::ostringstream oss;
    oss << "{ \"" << name << "\": ";
    oss << buffer;
    oss << " }";

    delete[] buffer;

    send(oss.str().c_str(), conn);
  }
}

static void ws_get_ecs_data(struct mg_connection *conn, const JDocument&);
static void ws_save_ecs_entities(struct mg_connection *conn, const JDocument &doc);

static void send_ok(const char *cmd, struct mg_connection *conn)
{
  BsonStream bson;
  bson_document(bson, cmd, [&]()
  {
    bson.add("status", "ok");
  });
  send(bson, conn);
}

static void run_script_debug_on_main_thread_and_wait(struct mg_connection *conn, const JDocument &doc, QueueItem::Handler &&handler)
{
  if (script::debug::is_suspended())
  {
    BsonStream bson;
    handler(doc, bson);
    if (doc.HasMember("$requestId") && doc["$requestId"].IsInt())
    {
      bson.add("cmd", doc["cmd"].GetString());
      bson.add("$requestId", doc["$requestId"].GetInt());
      send(bson, conn);
    }
  }
  else
    run_on_main_thread_and_wait(conn, doc, eastl::move(handler));
}

static void ws_script_debug_enable(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::enable();
  });
}

static void ws_script_debug_resume(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::resume();
  });
}

static void ws_script_debug_add_breakpoint(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::add_breakpoint(doc["file"].GetString(), doc["line"].GetInt());
  });
}

static void ws_script_debug_remove_breakpoint(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::remove_breakpoint(doc["file"].GetString(), doc["line"].GetInt());
  });
}

static void ws_script_debug_remove_all_breakpoints(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::remove_all_breakpoints();
  });
}

static void ws_script_debug_step_into(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::step_into();
  });
}

static void ws_script_debug_step_out(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::step_out();
  });
}

static void ws_script_debug_step_over(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    script::debug::step_over();
  });
}

static void json_to_bson(const JFrameValue &json, BsonStream &bson)
{
  // bool IsNull()   const { return data_.f.flags == kNullFlag; }
  // bool IsFalse()  const { return data_.f.flags == kFalseFlag; }
  // bool IsTrue()   const { return data_.f.flags == kTrueFlag; }
  // bool IsBool()   const { return (data_.f.flags & kBoolFlag) != 0; }
  // bool IsObject() const { return data_.f.flags == kObjectFlag; }
  // bool IsArray()  const { return data_.f.flags == kArrayFlag; }
  // bool IsNumber() const { return (data_.f.flags & kNumberFlag) != 0; }
  // bool IsInt()    const { return (data_.f.flags & kIntFlag) != 0; }
  // bool IsUint()   const { return (data_.f.flags & kUintFlag) != 0; }
  // bool IsInt64()  const { return (data_.f.flags & kInt64Flag) != 0; }
  // bool IsUint64() const { return (data_.f.flags & kUint64Flag) != 0; }
  // bool IsDouble() const { return (data_.f.flags & kDoubleFlag) != 0; }
  // bool IsString() const { return (data_.f.flags & kStringFlag) != 0; }

  for (auto it = json.MemberBegin(); it != json.MemberEnd(); ++it)
  {
    const JFrameValue &v = it->value;
    if (v.IsInt())
      bson.add(it->name.GetString(), v.GetInt());
    else if (v.IsFloat())
      bson.add(it->name.GetString(), v.GetFloat());
    else if (v.IsString())
      bson.add(it->name.GetString(), v.GetString());
    else if (v.IsObject())
    {
      bson.begin(it->name.GetString());
      json_to_bson(v, bson);
      bson.end();
    }
    else if (v.IsArray())
    {
      bson.beginArray(it->name.GetString());
      for (BsonStream::StringIndex i; i.idx() < (int)v.Size(); i.increment())
      {
        bson.begin(i.str());
        json_to_bson(v[i.idx()], bson);
        bson.end();
      }
      bson.end();
    }
  }
}

static void ws_script_debug_get_callstack(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    JFrameDocument res;
    script::debug::get_callstack(res);

    bson.beginArray("callstack");
    for (BsonStream::StringIndex i; i.idx() < (int)res.Size(); i.increment())
    {
      bson.begin(i.str());
      json_to_bson(res[i.idx()], bson);
      bson.end();
    }
    bson.end();
  });
}

static void ws_script_debug_get_local_vars(struct mg_connection *conn, const JDocument &doc)
{
  run_script_debug_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    JFrameDocument res;
    script::debug::get_local_vars(res);

    bson.beginArray("localVars");
    for (BsonStream::StringIndex i; i.idx() < (int)res.Size(); i.increment())
    {
      bson.begin(i.str());
      json_to_bson(res[i.idx()], bson);
      bson.end();
    }
    bson.end();
  });
}

struct WSCommand
{
  const char *name;
  void (*handler)(struct mg_connection*, const JDocument&);
  bool shouldSendOk;
};

constexpr WSCommand commands[] = {
  {"getECSData", &ws_get_ecs_data, false},
  {"saveECSEntities", &ws_save_ecs_entities, false},

  {"script::debug::enable", &ws_script_debug_enable, false},
  {"script::debug::resume", &ws_script_debug_resume, false},
  {"script::debug::add_breakpoint", &ws_script_debug_add_breakpoint, false},
  {"script::debug::remove_breakpoint", &ws_script_debug_remove_breakpoint, false},
  {"script::debug::remove_all_breakpoints", &ws_script_debug_remove_all_breakpoints, false},
  {"script::debug::step_into", &ws_script_debug_step_into, false},
  {"script::debug::step_out", &ws_script_debug_step_out, false},
  {"script::debug::step_over", &ws_script_debug_step_over, false},

  {"script::debug::get_callstack", &ws_script_debug_get_callstack, false},
  {"script::debug::get_local_vars", &ws_script_debug_get_local_vars, false},
};

static int is_websocket(const struct mg_connection *nc)
{
  return nc->flags & MG_F_IS_WEBSOCKET;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
  switch (ev)
  {
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
  {
    /* New websocket connection. Tell everybody. */
    //broadcast(nc, mg_mk_str("++ joined"));
    break;
  }
  case MG_EV_WEBSOCKET_FRAME:
  {
    struct websocket_message *wm = (struct websocket_message *) ev_data;
    /* New websocket message. Tell everybody. */
    struct mg_str d = { (char *)wm->data, wm->size };
    //broadcast(nc, d);
    JDocument doc;
    doc.Parse(d.p, d.len);
    if (doc.HasMember("cmd"))
    {
      for (const auto &c : commands)
      {
        if (doc["cmd"] == c.name)
        {
          c.handler(nc, doc);
          if (c.shouldSendOk)
            send_ok(c.name, nc);
          break;
        }
      }
    }
    break;
  }
  case MG_EV_HTTP_REQUEST:
  {
    //mg_serve_http(nc, (struct http_message *) ev_data, s_http_server_opts);
    break;
  }
  case MG_EV_CLOSE: {
    /* Disconnect. Tell everybody. */
    if (is_websocket(nc)) {
      //broadcast(nc, mg_mk_str("-- left"));
    }
    break;
  }
  }
}

static void handle_reload_script(struct mg_connection *conn, int ev, void *ev_data)
{
  mg_printf(conn, "HTTP/1.0 200 OK\r\n\r\n");
  conn->flags |= MG_F_SEND_AND_CLOSE;

  struct http_message *hm = (struct http_message *) ev_data;
  if (hm->body.len > 0)
  {
    JDocument doc;
    doc.Parse(hm->body.p, hm->body.len);

    run_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream&)
    {
      if (doc.HasMember("script"))
      {
        ecs::send_event_broadcast(CmdReloadScript{});
      }
    });
  }
}

void WebsocketServer::init(const char *address)
{
  mg_mgr_init(&mgr, nullptr);
  nc = mg_bind(&mgr, address, ev_handler);

  mg_set_protocol_http_websocket(nc);
  nc->user_data = (void*)this;

  mg_register_http_endpoint(nc, "/reload_script", handle_reload_script);
}

static std::atomic<int> server_terminated = 1;
static std::atomic<int> queue_processed = 0;
static std::thread server_thread;

static std::mutex run_on_main_thread_mutex;
static eastl::queue<QueueItem> run_on_main_thread_queue;

static void webui_thread_routine()
{
  while (server_terminated.load() != 1)
  {
    if (script::debug::is_enabled() && script::debug::is_first_hit())
    {
      for (struct mg_connection *conn = server->mgr.active_connections; conn != nullptr; conn = conn->next)
      {
        BsonStream bson;
        bson.begin("hit_breakpoint");
        bson.add("line", script::debug::get_current_line());
        bson.end();
        send(bson, conn);
      }
      script::debug::reset_first_hit();
    }

    server->poll();
  }
}

void webui::init(const char *address)
{
  server = new WebsocketServer;
  server->init(address);

  server_terminated.store(0);
  server_thread = std::thread(webui_thread_routine);
}

void webui::release()
{
  server_terminated.store(1);
  if (server_thread.joinable())
    server_thread.join();
  delete server;
}

void webui::update()
{
  if (server_terminated.load() == 1)
  {
    std::lock_guard<std::mutex> lock(run_on_main_thread_mutex);
    decltype(run_on_main_thread_queue) tmp;
    run_on_main_thread_queue.swap(tmp);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(run_on_main_thread_mutex);
    while (!run_on_main_thread_queue.empty())
    {
      QueueItem &item = run_on_main_thread_queue.front();

      BsonStream bson;
      item.handler(item.doc, bson);
      if (item.doc.HasMember("$requestId") && item.doc["$requestId"].IsInt())
      {
        bson.add("$requestId", item.doc["$requestId"].GetInt());
        send(bson, item.conn);
      }

      run_on_main_thread_queue.pop();
    }

    queue_processed.store(1);
  }
}

static void run_on_main_thread_and_wait(struct mg_connection *conn, const JDocument &doc, QueueItem::Handler &&handler)
{
  {
    std::lock_guard<std::mutex> lock(run_on_main_thread_mutex);

    run_on_main_thread_queue.emplace_back();
    QueueItem &item = run_on_main_thread_queue.back();
    item.conn = conn;
    item.doc.CopyFrom(doc, item.doc.GetAllocator());
    item.handler = eastl::move(handler);

    queue_processed.store(0);
  }

  while (queue_processed.load() != 1)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

struct Script
{
  ECS_QUERY;

  const script::ScriptComponent &script;
};

struct SystemData
{
  struct ComponentData
  {
    eastl::string type;
    eastl::string name;
  };

  eastl::string name;
  eastl::vector<ComponentData> components;
  eastl::vector<ComponentData> haveComponents;
  eastl::vector<ComponentData> notHaveComponents;
  eastl::vector<ComponentData> isTrueComponents;
  eastl::vector<ComponentData> isFalseComponents;
};

static void gather_script_data(eastl::vector<SystemData> &scriptSystems)
{
  // TODO: Use query without codegen
  Script::foreach([&](Script &&s)
  {
    for (const auto &sys : s.script.scriptECS.systems)
    {
      auto &s = scriptSystems.emplace_back();
      s.name = sys.fn->GetName();

      for (const auto &comp : sys.queryDesc.components)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name.str;
      }
      for (const auto &comp : sys.queryDesc.haveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name.str;
      }
      for (const auto &comp : sys.queryDesc.notHaveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name.str;
      }
      for (const auto &comp : sys.queryDesc.isTrueComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name.str;
      }
      for (const auto &comp : sys.queryDesc.isFalseComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name.str;
      }
    }
  });
}

static void ws_get_ecs_data(struct mg_connection *conn, const JDocument &doc)
{
  run_on_main_thread_and_wait(conn, doc, [](const JDocument &, BsonStream &bson)
  {
    eastl::vector<SystemData> scriptSystems;
    gather_script_data(scriptSystems);

    bson_document(bson, "getECSData", [&]()
    {
      bson_array_of_documents(bson, "templates", g_mgr->templates, [&](int, const EntityTemplate &templ)
      {
        bson.add("name", templ.name);

        bson_array_of_documents(bson, "components", templ.components, [&](int, const CompDesc &comp)
        {
          bson.add("type", comp.desc->name);
          bson.add("name", comp.name.str);
        });
      });

      bson_array_of_documents(bson, "systems", g_mgr->systems, [&](int, const System &sys)
      {
        bson.add("name", sys.desc->name.str);

        bson_array_of_documents(bson, "components", sys.desc->queryDesc.components, [&](int, const ConstCompDesc &comp)
        {
          bson.add("name", comp.name.str);
          bson.add("type", g_mgr->getComponentDescByName(comp.name)->name);
        });

        bson_array_of_documents(bson, "haveComponents", sys.desc->queryDesc.haveComponents, [&](int, const ConstCompDesc &comp)
        {
          bson.add("name", comp.name.str);
          bson.add("type", g_mgr->getComponentDescByName(comp.name)->name);
        });

        bson_array_of_documents(bson, "notHaveComponents", sys.desc->queryDesc.notHaveComponents, [&](int, const ConstCompDesc &comp)
        {
          bson.add("name", comp.name.str);
          bson.add("type", g_mgr->getComponentDescByName(comp.name)->name);
        });

        bson_array_of_documents(bson, "isTrueComponents", sys.desc->queryDesc.isTrueComponents, [&](int, const ConstCompDesc &comp)
        {
          bson.add("name", comp.name.str);
          bson.add("type", g_mgr->getComponentDescByName(comp.name)->name);
        });

        bson_array_of_documents(bson, "isFalseComponents", sys.desc->queryDesc.isFalseComponents, [&](int, const ConstCompDesc &comp)
        {
          bson.add("name", comp.name.str);
          bson.add("type", g_mgr->getComponentDescByName(comp.name)->name);
        });
      });

      bson_array_of_documents(bson, "scriptSystems", scriptSystems, [&](int, const SystemData &sys)
      {
        bson.add("name", sys.name);

        bson_array_of_documents(bson, "components", sys.components, [&](int, const SystemData::ComponentData &comp)
        {
          bson.add("name", comp.name);
          bson.add("type", comp.type);
        });
      });
    });
  });

  send_json_file("getECSTemplates", "data/templates.json", conn);
  send_json_file("getECSEntities", "data/level_1.json", conn);
}

static void ws_save_ecs_entities(struct mg_connection *conn, const JDocument &doc)
{
  run_on_main_thread_and_wait(conn, doc, [](const JDocument &doc, BsonStream &bson)
  {
    static constexpr size_t writeBufferSize = 4 << 20;
    eastl::unique_ptr<char> writeBuffer(new char[writeBufferSize]);

    JDocument docToSave;
    docToSave.CopyFrom(doc["data"], docToSave.GetAllocator());

    FILE *fp = nullptr;
    ::fopen_s(&fp, "data/level_1.json", "wb");

    if (fp)
    {
      rapidjson::FileWriteStream os(fp, writeBuffer.get(), writeBufferSize);
      rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
      writer.SetIndent(' ', 2);
      writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);
      docToSave.Accept(writer);
      fclose(fp);

      ecs::send_event_broadcast(CmdReloadScript{});
    }
  });
}