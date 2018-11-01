#include <ecs/ecs.h>

#include <scriptECS.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <mongoose.h>
#include <bson.h>
#include <bsonUtils.h>

#include <sstream>

#include "script.ecs.h"

struct WSEvent : Event
{
  mg_connection *conn = nullptr;

  WSEvent() = default;
  WSEvent(mg_connection *_conn) : conn(_conn) {}
};

struct WSGetECSData : WSEvent
{
  WSGetECSData() = default;
  WSGetECSData(mg_connection *_conn) : WSEvent(_conn) {}
}
DEF_EVENT(WSGetECSData);

static int is_websocket(const struct mg_connection *nc)
{
  return nc->flags & MG_F_IS_WEBSOCKET;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
  EntityId eid;
  if (nc->user_data)
    eid = (uint32_t)nc->user_data;

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
    if (doc.HasMember("cmd") && doc["cmd"] == "getECSData")
    {
      g_mgr->sendEvent(eid, WSGetECSData{nc});
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

static void handle_reload_script(struct mg_connection *nc, int ev, void *ev_data)
{
  mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n");
  nc->flags |= MG_F_SEND_AND_CLOSE;

  struct http_message *hm = (struct http_message *) ev_data;
  if (hm->body.len > 0)
  {
    JDocument doc;
    doc.Parse(hm->body.p, hm->body.len);

    if (doc.HasMember("script"))
    {
      g_mgr->sendEventBroadcast(CmdReloadScript{});
    }
  }
}

struct WebsocketServer
{
  struct mg_mgr mgr;
  struct mg_connection *nc = nullptr;

  ~WebsocketServer()
  {
    mg_mgr_free(&mgr);
  }

  bool set(const JFrameValue &value)
  {
    return true;
  };
}
DEF_COMP(WebsocketServer, websocket_server);

DEF_SYS()
static __forceinline void init_websocket_server_handler(const EventOnEntityCreate &ev, const EntityId &eid, WebsocketServer &ws_server)
{
  mg_mgr_init(&ws_server.mgr, nullptr);
  ws_server.nc = mg_bind(&ws_server.mgr, "127.0.0.1:10112", ev_handler);

  mg_set_protocol_http_websocket(ws_server.nc);
  ws_server.nc->user_data = (void*)(uint32_t)eid.handle;

  mg_register_http_endpoint(ws_server.nc, "/reload_script", handle_reload_script);
}

DEF_SYS()
static __forceinline void update_websocket_server(const UpdateStage &stage, WebsocketServer &ws_server)
{
  mg_mgr_poll(&ws_server.mgr, 0);
}

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

static void send(BsonStream &bson, const WSEvent &ev)
{
  const uint8_t *d = nullptr;
  size_t sz = 0;
  eastl::tie(d, sz) = bson.closeAndGetData();

  mg_send_websocket_frame(ev.conn, WEBSOCKET_OP_BINARY, d, sz);
}

static void send(const char *message, const WSEvent &ev)
{
  mg_send_websocket_frame(ev.conn, WEBSOCKET_OP_TEXT, message, ::strlen(message));
}

static void send_json_file(const char *name, const char *path, const WSEvent &ev)
{
  char *buffer = get_file_content(path);
  assert(buffer != nullptr);

  if (buffer)
  {
    std::ostringstream oss;
    oss << "{ \"" << name << "\": ";
    oss << buffer;
    oss << " }";

    delete[] buffer;

    send(oss.str().c_str(), ev);
  }
}

DEF_QUERY(AllScriptsQuery);

DEF_SYS()
static __forceinline void ws_get_ecs_data(const WSGetECSData &ev, const WebsocketServer &ws_server)
{
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
  eastl::vector<SystemData> scriptSystems;

  AllScriptsQuery::exec([&](const script::ScriptComponent &script)
  {
    for (const auto &sys : script.scriptECS.systems)
    {
      auto &s = scriptSystems.emplace_back();
      s.name = sys.fn->GetName();

      for (const auto &comp : sys.queryDesc.components)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = g_mgr->getComponentName(comp.nameId);
      }
      for (const auto &comp : sys.queryDesc.haveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = g_mgr->getComponentName(comp.nameId);
      }
      for (const auto &comp : sys.queryDesc.notHaveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = g_mgr->getComponentName(comp.nameId);
      }
      for (const auto &comp : sys.queryDesc.isTrueComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = g_mgr->getComponentName(comp.nameId);
      }
      for (const auto &comp : sys.queryDesc.isFalseComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = g_mgr->getComponentName(comp.nameId);
      }
    }
  });

  BsonStream bson;

  bson_document(bson, "getECSData", [&]()
  {
    bson_array_of_documents(bson, "templates", g_mgr->templates, [&](int, const EntityTemplate &templ)
    {
      bson.add("name", templ.name);

      bson_array_of_documents(bson, "components", templ.components, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
      });
    });

    bson_array_of_documents(bson, "systems", g_mgr->systems, [&](int, const System &sys)
    {
      bson.add("name", sys.desc->name);

      bson_array_of_documents(bson, "components", sys.desc->queryDesc.components, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "haveComponents", sys.desc->queryDesc.haveComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "notHaveComponents", sys.desc->queryDesc.notHaveComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "isTrueComponents", sys.desc->queryDesc.isTrueComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "isFalseComponents", sys.desc->queryDesc.isFalseComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", g_mgr->getComponentName(comp.nameId));
        bson.add("type", comp.desc->name);
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

  send(bson, ev);

  send_json_file("getECSTemplates", "templates.json", ev);
  send_json_file("getECSEntities", "entities.json", ev);
}