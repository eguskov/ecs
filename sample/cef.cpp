#include "cef.h"

#include <ecs/ecs.h>

#ifdef _DEBUG

#include <include/base/cef_build.h>
#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_browser.h>
#include <include/cef_command_line.h>
#include <include/views/cef_browser_view.h>
#include <include/views/cef_window.h>
#include <include/wrapper/cef_helpers.h>
#include <include/wrapper/cef_closure_task.h>

#include <windows.h>

#endif

#include <EASTL/list.h>

struct CEFEventOnReady : Event
{
  CEFEventOnReady() = default;
}
DEF_EVENT(CEFEventOnReady);

struct CEFEventOnAfterCreated : Event
{
  CEFEventOnAfterCreated() = default;
}
DEF_EVENT(CEFEventOnAfterCreated);

static EntityId g_webui_eid;

#ifdef _DEBUG

static HWND g_main_window = nullptr;

struct DevToolsHandler : public CefClient
{
  DevToolsHandler() {}
  ~DevToolsHandler() {}

  IMPLEMENT_REFCOUNTING(DevToolsHandler);
};

struct WebUIHandler : public CefClient, public CefDisplayHandler, public CefLifeSpanHandler, public CefLoadHandler
{
  using BrowserList = eastl::list<CefRefPtr<CefBrowser>>;
  BrowserList browsers;

  bool isClosing = false;

  WebUIHandler() {}
  ~WebUIHandler() {}

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override final { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override final { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override final { return this; }

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override final
  {
    CEF_REQUIRE_UI_THREAD();

    PlatformTitleChange(browser, title);
  }

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override final
  {
    CEF_REQUIRE_UI_THREAD();

    browsers.push_back(browser);

    g_mgr->sendEvent(g_webui_eid, CEFEventOnAfterCreated{});
  }

  bool DoClose(CefRefPtr<CefBrowser> browser) override final
  {
    CEF_REQUIRE_UI_THREAD();

    if (browsers.size() == 1)
      isClosing = true;

    return false;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override final
  {
    CEF_REQUIRE_UI_THREAD();

    for (auto it = browsers.begin(); it != browsers.end(); ++it)
      if ((*it)->IsSame(browser))
      {
        browsers.erase(it);
        break;
      }

    if (browsers.empty())
      CefQuitMessageLoop();
  }

  // CefLoadHandler methods:
  void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override final
  {
    CEF_REQUIRE_UI_THREAD();

    if (errorCode == ERR_ABORTED)
      return;

    std::stringstream ss;
    ss << "<html><body bgcolor=\"white\">"
          "<h2>Failed to load URL "
      << std::string(failedUrl) << " with error " << std::string(errorText)
      << " (" << errorCode << ").</h2></body></html>";
    frame->LoadString(ss.str(), failedUrl);
  }

  void CloseAllBrowsers(bool force_close)
  {
    if (!CefCurrentlyOn(TID_UI))
    {
      CefPostTask(TID_UI, base::Bind(&WebUIHandler::CloseAllBrowsers, this, force_close));
      return;
    }

    for (const auto &it : browsers)
      it->GetHost()->CloseBrowser(force_close);
  }

  CefWindowHandle getHandle() const
  {
    return browsers.front()->GetHost()->GetWindowHandle();
  }

  void ToggleDevTools()
  {
    CefRefPtr<CefBrowserHost> host = browsers.front()->GetHost();
    if (host->HasDevTools())
      CloseDevTools(browsers.front());
    else
      ShowDevTools(browsers.front(), CefPoint());
  }

  void ShowDevTools(CefRefPtr<CefBrowser> browser, const CefPoint& inspect_element_at)
  {
    if (!CefCurrentlyOn(TID_UI))
    {
      CefPostTask(TID_UI, base::Bind(&WebUIHandler::ShowDevTools, this, browser, inspect_element_at));
      return;
    }

    CefWindowInfo windowInfo;
    CefRefPtr<CefClient> client;
    CefBrowserSettings settings;

    CefRefPtr<CefBrowserHost> host = browser->GetHost();

    bool has_devtools = host->HasDevTools();
    if (!has_devtools)
    {
      windowInfo.SetAsPopup(g_main_window, "devtools");
      windowInfo.style &= ~WS_OVERLAPPED;
      // windowInfo.style &= ~WS_CAPTION;
      windowInfo.style |= WS_POPUP;
      windowInfo.x = 20;
      windowInfo.y = 100;
      windowInfo.width = 500;
      windowInfo.height = 500;

      client = new DevToolsHandler;
      host->ShowDevTools(windowInfo, client, settings, inspect_element_at);
    }
  }

  void CloseDevTools(CefRefPtr<CefBrowser> browser)
  {
    if (browser->GetHost()->HasDevTools())
      browser->GetHost()->CloseDevTools();
  }

  void CloseDevTools()
  {
    if (!browsers.empty())
      CloseDevTools(browsers.front());
  }

  void PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
  {
    CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
    SetWindowText(hwnd, std::string(title).c_str());
  }

  IMPLEMENT_REFCOUNTING(WebUIHandler);
};

struct WebUIApp : public CefApp, public CefBrowserProcessHandler
{
  using HandlerList = eastl::list<CefRefPtr<WebUIHandler>>;
  HandlerList handlers;

  WebUIApp() {}

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override final { return this; }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override final
  {
    CEF_REQUIRE_UI_THREAD();

    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();

    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");

    CefRefPtr<WebUIHandler> handler(new WebUIHandler);
    handlers.push_back(handler);

    g_mgr->sendEvent(g_webui_eid, CEFEventOnReady{});
  }

  IMPLEMENT_REFCOUNTING(WebUIApp);
};

static CefRefPtr<WebUIApp> g_app;

#endif

EntityId cef::get_eid()
{
  return g_webui_eid;
}

void cef::init()
{
#ifdef _DEBUG
  g_webui_eid = g_mgr->createEntitySync("WebUI", JValue());

  CefEnableHighDPISupport();

  CefMainArgs main_args(GetModuleHandle(NULL));

  CefSettings settings;
  settings.no_sandbox = true;
  settings.external_message_pump = 1;
  settings.multi_threaded_message_loop = 0;

  g_app = new WebUIApp;

  CefInitialize(main_args, settings, g_app.get(), nullptr);
#endif
}

void cef::update()
{
#ifdef _DEBUG
  CefDoMessageLoopWork();
#endif
}

void cef::release()
{
#ifdef _DEBUG
  CefDoMessageLoopWork();
  CefShutdown();
#endif
}

DEF_SYS(HAVE_COMP(webui))
static __forceinline void cef_ready_handler(const CEFEventOnReady &ev)
{
#ifdef _DEBUG
  g_main_window = ::FindWindow(nullptr, "raylib [core] example - basic window");

  CefBrowserSettings browserSettings;

  CefWindowInfo windowInfo;
  // windowInfo.SetAsChild(g_main_window, { 0, 300, 800, 600 });

  windowInfo.SetAsPopup(g_main_window, "");
  // windowInfo.style &= ~WS_OVERLAPPED;
  // windowInfo.style &= ~WS_CAPTION;
  // windowInfo.style |= WS_POPUP;
  windowInfo.x = 20;
  windowInfo.y = 100;
  windowInfo.width = 500;
  windowInfo.height = 500;

  auto handler = g_app->handlers.front();
  CefBrowserHost::CreateBrowser(windowInfo, handler, "http://localhost:10010", browserSettings, NULL);
#endif
}

DEF_SYS(HAVE_COMP(webui))
static __forceinline void cef_after_created_handler(const CEFEventOnAfterCreated &ev)
{
#ifdef _DEBUG
  ::SetFocus(g_main_window);
#endif
}

DEF_SYS(HAVE_COMP(webui))
static __forceinline void cef_click_outside_handler(const cef::EventOnClickOutside &ev)
{
#ifdef _DEBUG
  ::SetFocus(g_main_window);
#endif
}

DEF_SYS(HAVE_COMP(webui))
static __forceinline void cef_toggle_dev_tools_handler(const cef::CmdToggleDevTools &ev)
{
#ifdef _DEBUG
  g_app->handlers.front()->ToggleDevTools();
#endif
}

DEF_SYS(HAVE_COMP(webui))
static __forceinline void cef_toggle_webui_handler(const cef::CmdToggleWebUI &ev)
{
#ifdef _DEBUG
  HWND hWnd = g_app->handlers.front()->browsers.front()->GetHost()->GetWindowHandle();

  ::ShowWindow(hWnd, ::IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
  ::UpdateWindow(hWnd);
#endif
}