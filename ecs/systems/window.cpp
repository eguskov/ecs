#include "ecs/ecs.h"

#include "stages/update.stage.h"

#include "components/position.component.h"
#include "components/color.component.h"

#include <windows.h>

struct WindowComponent
{
  bool active = false;

  HWND handle = NULL;
  HDC dc = NULL;
  HINSTANCE instance = NULL;

  int width = 0;
  int height = 0;

  bool set(const JValue &value)
  {
    assert(value.HasMember("width"));
    width = value["width"].GetInt();
    assert(value.HasMember("height"));
    height = value["height"].GetInt();
    return true;
  }
};
REG_COMP_AND_INIT(WindowComponent, window);

struct PaintStage : Stage
{
  HDC dc = NULL;
  RECT rc;
  PAINTSTRUCT ps;
};
REG_STAGE_AND_INIT(PaintStage, "paint_stage");

struct Event_WM_ACTIVATE : Event
{
  bool active = false;
  Event_WM_ACTIVATE(bool _active) : active(_active) {}
};
REG_EVENT_AND_INIT(Event_WM_ACTIVATE);


LRESULT CALLBACK WndProc(HWND	hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  LONG_PTR data = ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
  EntityId eid = (EntityId)data;
  switch (uMsg)
  {
  case WM_ACTIVATE:
  {
    g_mgr->sendEvent(eid, Event_WM_ACTIVATE{ HIWORD(wParam) ? false : true });
    return 0;
  }

  case WM_SYSCOMMAND:
  {
    switch (wParam)
    {
    case SC_SCREENSAVE:					// Screensaver Trying To Start?
    case SC_MONITORPOWER:				// Monitor Trying To Enter Powersave?
      return 0;							// Prevent From Happening
    }
    break;									// Exit
  }

  case WM_CLOSE:
  {
    PostQuitMessage(0);
    return 0;
  }

  case WM_KEYDOWN:
  {
    //keys[wParam] = TRUE;
    return 0;
  }

  case WM_KEYUP:
  {
    //keys[wParam] = FALSE;
    return 0;
  }

  case WM_PAINT:
  {
    PaintStage stage;
    BeginPaint(hWnd, &stage.ps);

    HBITMAP hbmMem = NULL, hbmOld = NULL;
    HBRUSH hbrBkGnd = NULL;
    HFONT hfntOld = NULL;

    GetClientRect(hWnd, &stage.rc);
    stage.dc = CreateCompatibleDC(stage.ps.hdc);
    hbmMem = CreateCompatibleBitmap(stage.ps.hdc,
      stage.rc.right - stage.rc.left,
      stage.rc.bottom - stage.rc.top);

    hbmOld = (HBITMAP)SelectObject(stage.dc, hbmMem);

    hbrBkGnd = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(stage.dc, &stage.rc, hbrBkGnd);
    DeleteObject(hbrBkGnd);

    g_mgr->tick(stage);

    BitBlt(stage.ps.hdc,
      stage.rc.left, stage.rc.top,
      stage.rc.right - stage.rc.left,
      stage.rc.bottom - stage.rc.top,
      stage.dc,
      0, 0,
      SRCCOPY);

    SelectObject(stage.dc, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(stage.dc);

    EndPaint(hWnd, &stage.ps);
    return 0;
  }
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void window_create_handler(const EventOnEntityCreate &ev,
  EntityId eid,
  WindowComponent &wnd)
{
  unsigned int PixelFormat;			// Holds The Results After Searching For A Match
  WNDCLASS	wc;						// Windows Class Structure
  DWORD		dwExStyle;				// Window Extended Style
  DWORD		dwStyle;				// Window Style
  RECT		WindowRect;				// Grabs Rectangle Upper Left / Lower Right Values
  WindowRect.left = (long)0;			// Set Left Value To 0
  WindowRect.right = (long)wnd.width;		// Set Right Value To Requested Width
  WindowRect.top = (long)0;				// Set Top Value To 0
  WindowRect.bottom = (long)wnd.height;		// Set Bottom Value To Requested Height

  wnd.instance = GetModuleHandle(NULL);				// Grab An Instance For Our Window
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
  wc.lpfnWndProc = (WNDPROC)WndProc;					// WndProc Handles Messages
  wc.cbClsExtra = 0;									// No Extra Window Data
  wc.cbWndExtra = 0;									// No Extra Window Data
  wc.hInstance = wnd.instance;							// Set The Instance
  wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);			// Load The Default Icon
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
  wc.hbrBackground = NULL;									// No Background Required For GL
  wc.lpszMenuName = NULL;									// We Don't Want A Menu
  wc.lpszClassName = "ECS";								// Set The Class Name

  if (!RegisterClass(&wc))									// Attempt To Register The Window Class
  {
    MessageBox(NULL, "Failed To Register The Window Class.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    return;											// Return FALSE
  }

  dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;			// Window Extended Style
  dwStyle = WS_OVERLAPPEDWINDOW;							// Windows Style

  AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);		// Adjust Window To True Requested Size
                                                               // Create The Window
  if (!(wnd.handle = CreateWindowEx(dwExStyle,							// Extended Style For The Window
    "ECS",							// Class Name
    "ECS",								// Window Title
    dwStyle |							// Defined Window Style
    WS_CLIPSIBLINGS |					// Required Window Style
    WS_CLIPCHILDREN,					// Required Window Style
    0, 0,								// Window Position
    WindowRect.right - WindowRect.left,	// Calculate Window Width
    WindowRect.bottom - WindowRect.top,	// Calculate Window Height
    NULL,								// No Parent Window
    NULL,								// No Menu
    wnd.instance,							// Instance
    NULL)))								// Dont Pass Anything To WM_CREATE
  {
    MessageBox(NULL, "Window Creation Error.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    return;								// Return FALSE
  }

  ::SetWindowLongPtr(wnd.handle, GWLP_USERDATA, eid.handle);

  static	PIXELFORMATDESCRIPTOR pfd =				// pfd Tells Windows How We Want Things To Be
  {
    sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
    1,											// Version Number
    PFD_DRAW_TO_WINDOW |						// Format Must Support Window
    PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
    PFD_DOUBLEBUFFER,							// Must Support Double Buffering
    PFD_TYPE_RGBA,								// Request An RGBA Format
    16,										// Select Our Color Depth
    0, 0, 0, 0, 0, 0,							// Color Bits Ignored
    0,											// No Alpha Buffer
    0,											// Shift Bit Ignored
    0,											// No Accumulation Buffer
    0, 0, 0, 0,									// Accumulation Bits Ignored
    16,											// 16Bit Z-Buffer (Depth Buffer)  
    0,											// No Stencil Buffer
    0,											// No Auxiliary Buffer
    PFD_MAIN_PLANE,								// Main Drawing Layer
    0,											// Reserved
    0, 0, 0										// Layer Masks Ignored
  };

  if (!(wnd.dc = GetDC(wnd.handle)))							// Did We Get A Device Context?
  {
    MessageBox(NULL, "Can't Create A GL Device Context.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    return;								// Return FALSE
  }

  if (!(PixelFormat = ChoosePixelFormat(wnd.dc, &pfd)))	// Did Windows Find A Matching Pixel Format?
  {
    MessageBox(NULL, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    return;								// Return FALSE
  }

  if (!SetPixelFormat(wnd.dc, PixelFormat, &pfd))		// Are We Able To Set The Pixel Format?
  {
    MessageBox(NULL, "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
    return;								// Return FALSE
  }

  ShowWindow(wnd.handle, SW_SHOW);						// Show The Window
  SetForegroundWindow(wnd.handle);						// Slightly Higher Priority
  SetFocus(wnd.handle);									// Sets Keyboard Focus To The Window

  SetTimer(wnd.handle, 0, 1000 / 15, nullptr);

  //initDirectInput(hWnd);

  //::SetCursorPos(320, 240);
  //::ShowCursor(FALSE);
}
REG_SYS_2(window_create_handler, "window");

void WM_ACTIVATE_handler(const Event_WM_ACTIVATE &ev,
  EntityId eid,
  WindowComponent &wnd)
{
  wnd.active = ev.active;
}
REG_SYS_2(WM_ACTIVATE_handler, "window");

void window_updater(const UpdateStage &stage,
  EntityId eid,
  const WindowComponent &wnd)
{
  MSG msg;
  /* if (g_prev_total_time == 0)
     g_prev_total_time = ::GetTickCount();

   DWORD currentTime = ::GetTickCount();
   float dt = float(currentTime - g_prev_total_time) * 1e-3f;
   g_prev_total_time = currentTime;

   const float timeSpeed = 1.0f;

   dt = glm::clamp(dt, 1e-5f, 1.f / 60.f) * timeSpeed;
   totalTime += dt;*/

  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))	// Is There A Message Waiting?
  {
    if (msg.message == WM_QUIT)				// Have We Received A Quit Message?
    {
      //done = TRUE;							// If So done=TRUE
    }
    else									// If Not, Deal With Window Messages
    {
      TranslateMessage(&msg);				// Translate The Message
      DispatchMessage(&msg);				// Dispatch The Message
    }
  }
  else
  {
    if ((wnd.active/* && !actScene(totalTime, dt)*/)/* || keys[VK_ESCAPE]*/)	// Active?  Was There A Quit Received?
    {
      //done = TRUE;							// ESC or DrawGLScene Signalled A Quit
    }
    else									// Not Time To Quit, Update Screen
    {
      SwapBuffers(wnd.dc);					// Swap Buffers (Double Buffering)

      //processUserInput();

      //if (keys[VK_F1])
      //{
      //  done = true;
      //  keys[VK_F1] = FALSE;
      //  KillGLWindow();
      //}
    }
  }

  InvalidateRect(wnd.handle, 0, TRUE);
}
REG_SYS_2(window_updater, "window");

void window_painter(const PaintStage &stage,
  EntityId eid,
  const WindowComponent &wnd)
{
  RECT rc = stage.rc;

  SetBkMode(stage.dc, TRANSPARENT);
  SetTextColor(stage.dc, GetSysColor(COLOR_WINDOWTEXT));
  DrawText(stage.dc,
    "MY TEXT",
    -1,
    &rc,
    DT_CENTER);
}
REG_SYS_2(window_painter, "window");

void rect_painter(const PaintStage &stage,
  EntityId eid,
  const ColorComponent &color,
  const PositionComponent &pos)
{
  HBRUSH brush = CreateSolidBrush(RGB(color.r, color.g, color.b));
  RECT r;
  r.left = roundf(pos.x);
  r.top = roundf(pos.y);
  r.right = roundf(pos.x) + 10;
  r.bottom = roundf(pos.y) + 10;
  ::FillRect(stage.dc, &r, brush);

  ::DeleteObject(brush);
}
REG_SYS_2(rect_painter, "color", "pos");