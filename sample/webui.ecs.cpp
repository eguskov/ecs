#include <ecs/ecs.h>

#include <scriptECS.h>

#include <stages/update.stage.h>
#include <stages/render.stage.h>

#include <mongoose.h>
#include <bson.h>
#include <bsonUtils.h>

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

struct WebsocketServer
{
  struct mg_mgr mgr;
  struct mg_connection *nc = nullptr;

  ~WebsocketServer()
  {
    mg_mgr_free(&mgr);
  }

  bool set(const JValue &value)
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
}

DEF_SYS()
static __forceinline void update_websocket_server(const UpdateStage &stage, WebsocketServer &ws_server)
{
  mg_mgr_poll(&ws_server.mgr, 0);
}

static void send(BsonStream &bson, const WSEvent &ev)
{
  const uint8_t *d = nullptr;
  size_t sz = 0;
  eastl::tie(d, sz) = bson.closeAndGetData();

  mg_send_websocket_frame(ev.conn, WEBSOCKET_OP_BINARY, d, sz);
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

      for (const auto &comp : sys.components)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name;
      }
      for (const auto &comp : sys.haveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name;
      }
      for (const auto &comp : sys.notHaveComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name;
      }
      for (const auto &comp : sys.isTrueComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name;
      }
      for (const auto &comp : sys.isFalseComponents)
      {
        auto &c = s.components.emplace_back();
        c.type = comp.desc->name;
        c.name = comp.name;
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
        bson.add("name", comp.name);
        bson.add("type", comp.desc->name);
      });
    });

    bson_array_of_documents(bson, "systems", g_mgr->systems, [&](int, const System &sys)
    {
      bson.add("name", sys.desc->name);

      bson_array_of_documents(bson, "components", sys.desc->components, [&](int, const CompDesc &comp)
      {
        bson.add("name", comp.name);
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "haveComponents", sys.desc->haveComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", comp.name);
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "notHaveComponents", sys.desc->notHaveComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", comp.name);
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "isTrueComponents", sys.desc->isTrueComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", comp.name);
        bson.add("type", comp.desc->name);
      });

      bson_array_of_documents(bson, "isFalseComponents", sys.desc->isFalseComponents, [&](int, const CompDesc &comp)
      {
        bson.add("name", comp.name);
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
}