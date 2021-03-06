// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_contents_view_win.h"

#include <windows.h>

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/render_view_context_menu.h"
#include "chrome/browser/render_view_context_menu_controller.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/browser/views/info_bar_message_view.h"
#include "chrome/browser/views/info_bar_view.h"
#include "chrome/browser/views/sad_tab_view.h"
#include "chrome/browser/web_contents.h"
#include "chrome/browser/web_drag_source.h"
#include "chrome/browser/web_drop_target.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/os_exchange_data.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

namespace {

// Windows callback for OnDestroy to detach the plugin windows.
BOOL CALLBACK EnumPluginWindowsCallback(HWND window, LPARAM param) {
  if (WebPluginDelegateImpl::IsPluginDelegateWindow(window)) {
    ::ShowWindow(window, SW_HIDE);
    SetParent(window, NULL);
  }
  return TRUE;
}

}  // namespace

WebContentsViewWin::WebContentsViewWin(WebContents* web_contents)
    : web_contents_(web_contents),
      error_info_bar_message_(NULL),
      info_bar_visible_(false) {
}

WebContentsViewWin::~WebContentsViewWin() {
}

WebContents* WebContentsViewWin::GetWebContents() {
  return web_contents_;
}

void WebContentsViewWin::CreateView(HWND parent_hwnd,
                                    const gfx::Rect& initial_bounds) {
  set_delete_on_destroy(false);
  ContainerWin::Init(parent_hwnd, initial_bounds, false);

  // Remove the root view drop target so we can register our own.
  RevokeDragDrop(GetHWND());
  drop_target_ = new WebDropTarget(GetHWND(), web_contents_);
}

RenderWidgetHostViewWin* WebContentsViewWin::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewWin* view =
      new RenderWidgetHostViewWin(render_widget_host);
  view->Create(GetHWND());
  view->ShowWindow(SW_SHOW);
  return view;
}

HWND WebContentsViewWin::GetContainerHWND() const {
  return GetHWND();
}

HWND WebContentsViewWin::GetContentHWND() const {
  if (!web_contents_->render_widget_host_view())
    return NULL;
  return web_contents_->render_widget_host_view()->GetPluginHWND();
}

void WebContentsViewWin::GetContainerBounds(gfx::Rect *out) const {
  CRect r;
  GetBounds(&r, false);
  *out = r;
}

void WebContentsViewWin::StartDragging(const WebDropData& drop_data) {
  scoped_refptr<OSExchangeData> data(new OSExchangeData);

  // TODO(tc): Generate an appropriate drag image.

  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over a
  // shortcut.
  if (!drop_data.file_contents.empty()) {
    data->SetFileContents(drop_data.file_description_filename,
                          drop_data.file_contents);
  }
  if (!drop_data.cf_html.empty())
    data->SetCFHtml(drop_data.cf_html);
  if (drop_data.url.is_valid())
    data->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.plain_text.empty())
    data->SetString(drop_data.plain_text);

  scoped_refptr<WebDragSource> drag_source(
      new WebDragSource(GetHWND(), web_contents_->render_view_host()));

  DWORD effects;

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  DoDragDrop(data, drag_source, DROPEFFECT_COPY | DROPEFFECT_LINK, &effects);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  if (web_contents_->render_view_host())
    web_contents_->render_view_host()->DragSourceSystemDragEnded();
}

void WebContentsViewWin::OnContentsDestroy() {
  // TODO(brettw) this seems like maybe it can be moved into OnDestroy and this
  // function can be deleted? If you're adding more here, consider whether it
  // can be moved into OnDestroy which is a Windows message handler as the
  // window is being torn down.

  // First detach all plugin windows so that they are not destroyed
  // automatically. They will be cleaned up properly in plugin process.
  EnumChildWindows(GetHWND(), EnumPluginWindowsCallback, NULL);

  // Close the find bar if any.
  if (find_bar_.get())
    find_bar_->Close();
}

void WebContentsViewWin::DisplayErrorInInfoBar(const std::wstring& text) {
  InfoBarView* view = GetInfoBarView();
  if (-1 == view->GetChildIndex(error_info_bar_message_)) {
    error_info_bar_message_ = new InfoBarMessageView(text);
    view->AddChildView(error_info_bar_message_);
  } else {
    error_info_bar_message_->SetMessageText(text);
  }
}

void WebContentsViewWin::OnDestroy() {
  if (drop_target_.get()) {
    RevokeDragDrop(GetHWND());
    drop_target_ = NULL;
  }
}

void WebContentsViewWin::SetInfoBarVisible(bool visible) {
  if (info_bar_visible_ != visible) {
    info_bar_visible_ = visible;
    if (info_bar_visible_) {
      // Invoke GetInfoBarView to force the info bar to be created.
      GetInfoBarView();
    }
    web_contents_->ToolbarSizeChanged(false);
  }
}

bool WebContentsViewWin::IsInfoBarVisible() const {
  return info_bar_visible_;
}

InfoBarView* WebContentsViewWin::GetInfoBarView() {
  if (info_bar_view_.get() == NULL) {
    // TODO(brettw) currently the InfoBar thinks its owned by the WebContents,
    // but it should instead think it's owned by us.
    info_bar_view_.reset(new InfoBarView(web_contents_));
    // We own the info-bar.
    info_bar_view_->SetParentOwned(false);
  }
  return info_bar_view_.get();
}

void WebContentsViewWin::SetPageTitle(const std::wstring& title) {
  if (GetContainerHWND()) {
    // It's possible to get this after the hwnd has been destroyed.
    ::SetWindowText(GetContainerHWND(), title.c_str());
    // TODO(brettw) this call seems messy the way it reaches into the widget
    // view, and I'm not sure it's necessary. Maybe we should just remove it.
    ::SetWindowText(web_contents_->render_widget_host_view()->GetPluginHWND(),
                    title.c_str());
  }
}

void WebContentsViewWin::Invalidate() {
  // Note that it's possible to get this message after the window was destroyed.
  if (::IsWindow(GetContainerHWND()))
    InvalidateRect(GetContainerHWND(), NULL, FALSE);
}

void WebContentsViewWin::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See web_contents_view.h.
  WasSized(size);
}

void WebContentsViewWin::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
 if (!find_bar_.get()) {
    // We want the Chrome top-level (Frame) window.
    HWND hwnd = browser.GetTopLevelHWND();
    find_bar_.reset(new FindBarWin(this, hwnd));
  } else if (!find_bar_->IsVisible()) {
    find_bar_->Show();
  }

  if (find_next && !find_bar_->find_string().empty())
    find_bar_->StartFinding(forward_direction);
}

void WebContentsViewWin::HideFindBar(bool end_session) {
  if (find_bar_.get()) {
    if (end_session)
      find_bar_->EndFindSession();
    else
      find_bar_->DidBecomeUnselected();
  }
}

void WebContentsViewWin::ReparentFindWindow(Browser* new_browser) const {
  if (find_bar_.get())
    find_bar_->SetParent(new_browser->GetTopLevelHWND());
}

bool WebContentsViewWin::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  CRect window_rect;
  if (!find_bar_.get() ||
      !::IsWindow(find_bar_->GetHWND()) ||
      !::GetWindowRect(find_bar_->GetHWND(), &window_rect)) {
    *position = gfx::Point(0, 0);
    *fully_visible = false;
    return false;
  }

  *position = gfx::Point(window_rect.TopLeft().x, window_rect.TopLeft().y);
  *fully_visible = find_bar_->IsVisible() && !find_bar_->IsAnimating();
  return true;
}

void WebContentsViewWin::UpdateDragCursor(bool is_drop_target) {
  drop_target_->set_is_drop_target(is_drop_target);
}

void WebContentsViewWin::TakeFocus(bool reverse) {
  views::FocusManager* focus_manager =
      views::FocusManager::GetFocusManager(GetContainerHWND());

  // We may not have a focus manager if the tab has been switched before this
  // message arrived.
  if (focus_manager)
    focus_manager->AdvanceFocus(reverse);
}

void WebContentsViewWin::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  // The renderer returned a keyboard event it did not process. This may be
  // a keyboard shortcut that we have to process.
  if (event.type == WebInputEvent::KEY_DOWN) {
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManager(GetHWND());
    // We may not have a focus_manager at this point (if the tab has been
    // switched by the time this message returned).
    if (focus_manager) {
      views::Accelerator accelerator(event.key_code,
          (event.modifiers & WebInputEvent::SHIFT_KEY) ==
              WebInputEvent::SHIFT_KEY,
          (event.modifiers & WebInputEvent::CTRL_KEY) ==
              WebInputEvent::CTRL_KEY,
          (event.modifiers & WebInputEvent::ALT_KEY) ==
              WebInputEvent::ALT_KEY);
      if (focus_manager->ProcessAccelerator(accelerator, false))
        return;
    }
  }

  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like Alt+F4, etc to work correctly.
  DefWindowProc(event.actual_message.hwnd,
                event.actual_message.message,
                event.actual_message.wParam,
                event.actual_message.lParam);
}

void WebContentsViewWin::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  if (find_bar_.get()) {
    find_bar_->OnFindReply(request_id, number_of_matches, selection_rect,
                           active_match_ordinal, final_update);
  }
}

void WebContentsViewWin::ShowContextMenu(
    const ViewHostMsg_ContextMenu_Params& params) {
  RenderViewContextMenuController menu_controller(web_contents_, params);
  RenderViewContextMenu menu(&menu_controller,
                             GetHWND(),
                             params.type,
                             params.misspelled_word,
                             params.dictionary_suggestions,
                             web_contents_->profile());

  POINT screen_pt = { params.x, params.y };
  MapWindowPoints(GetHWND(), HWND_DESKTOP, &screen_pt, 1);

  // Enable recursive tasks on the message loop so we can get updates while
  // the context menu is being displayed.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  menu.RunMenuAt(screen_pt.x, screen_pt.y);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

WebContents* WebContentsViewWin::CreateNewWindowInternal(
    int route_id,
    HANDLE modal_dialog_event) {
  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  WebContents* new_contents =
      new WebContents(web_contents_->profile(),
                      web_contents_->GetSiteInstance(),
                      web_contents_->render_view_factory_,
                      route_id,
                      modal_dialog_event);
  new_contents->SetupController(web_contents_->profile());
  WebContentsView* new_view = new_contents->view();

  // TODO(beng)
  // The intention here is to create background tabs, which should ideally
  // be parented to NULL. However doing that causes the corresponding view
  // container windows to show up as overlapped windows, which causes
  // other issues. We should fix this.
  HWND new_view_parent_window = ::GetAncestor(GetContainerHWND(), GA_ROOT);
  new_view->CreateView(new_view_parent_window, gfx::Rect());

  // TODO(brettw) it seems bogus that we have to call this function on the
  // newly created object and give it one of its own member variables.
  new_view->CreateViewForWidget(new_contents->render_view_host());
  return new_contents;
}

RenderWidgetHostView* WebContentsViewWin::CreateNewWidgetInternal(
    int route_id) {
  // Create the widget and its associated view.
  // TODO(brettw) can widget creation be cross-platform?
  RenderWidgetHost* widget_host =
      new RenderWidgetHost(web_contents_->process(), route_id);
  RenderWidgetHostViewWin* widget_view =
      new RenderWidgetHostViewWin(widget_host);

  // We set the parent HWDN explicitly as pop-up HWNDs are parented and owned by
  // the first non-child HWND of the HWND that was specified to the CreateWindow
  // call.
  // TODO(brettw) this should not need to get the current RVHView from the
  // WebContents. We should have it somewhere ourselves.
  widget_view->set_parent_hwnd(
      web_contents_->render_widget_host_view()->GetPluginHWND());
  widget_view->set_close_on_deactivate(true);

  return widget_view;
}
  
void WebContentsViewWin::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  if (!new_web_contents->render_widget_host_view() ||
      !new_web_contents->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  // TODO(brettw) this seems bogus to reach into here and initialize the host.
  new_web_contents->render_view_host()->Init();
  web_contents_->AddNewContents(new_web_contents, disposition, initial_pos,
                                user_gesture);
}

void WebContentsViewWin::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  // TODO(beng): (Cleanup) move all this windows-specific creation and showing
  //             code into RenderWidgetHostView behind some API that a
  //             ChromeView can also reasonably implement.
  RenderWidgetHostViewWin* widget_host_view_win =
      static_cast<RenderWidgetHostViewWin*>(widget_host_view);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->process()->channel()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return;
  }

  // This logic should be implemented by RenderWidgetHostHWND (as mentioned
  // above) in the ::Init function, which should take a parent and some initial
  // bounds.
  widget_host_view_win->Create(GetContainerHWND(), NULL, NULL,
                               WS_POPUP, WS_EX_TOOLWINDOW);
  widget_host_view_win->MoveWindow(initial_pos.x(), initial_pos.y(),
                                   initial_pos.width(), initial_pos.height(),
                                   TRUE);
  widget_host_view_win->ShowWindow(SW_SHOW);
  widget_host->Init();
}

void WebContentsViewWin::OnHScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_HSCROLL, scroll_type, position, scrollbar);
}

void WebContentsViewWin::OnMouseLeave() {
  // Let our delegate know that the mouse moved (useful for resetting status
  // bubble state).
  if (web_contents_->delegate())
    web_contents_->delegate()->ContentsMouseEvent(web_contents_, WM_MOUSELEAVE);
  SetMsgHandled(FALSE);
}

LRESULT WebContentsViewWin::OnMouseRange(UINT msg,
                                         WPARAM w_param, LPARAM l_param) {
  switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      // Make sure this TabContents is activated when it is clicked on.
      if (web_contents_->delegate())
        web_contents_->delegate()->ActivateContents(web_contents_);
      DownloadRequestManager* drm =
          g_browser_process->download_request_manager();
      if (drm)
        drm->OnUserGesture(web_contents_);
      break;
    }
    case WM_MOUSEMOVE:
      // Let our delegate know that the mouse moved (useful for resetting status
      // bubble state).
      if (web_contents_->delegate()) {
        web_contents_->delegate()->ContentsMouseEvent(web_contents_,
                                                      WM_MOUSEMOVE);
      }
      break;
    default:
      break;
  }

  return 0;
}

void WebContentsViewWin::OnPaint(HDC junk_dc) {
  if (web_contents_->render_view_host() &&
      !web_contents_->render_view_host()->IsRenderViewLive()) {
    if (!sad_tab_.get())
      sad_tab_.reset(new SadTabView);
    CRect cr;
    GetClientRect(&cr);
    sad_tab_->SetBounds(gfx::Rect(cr));
    ChromeCanvasPaint canvas(GetHWND(), true);
    sad_tab_->ProcessPaint(&canvas);
    return;
  }

  // We need to do this to validate the dirty area so we don't end up in a
  // WM_PAINTstorm that causes other mysterious bugs (such as WM_TIMERs not
  // firing etc). It doesn't matter that we don't have any non-clipped area.
  CPaintDC dc(GetHWND());
  SetMsgHandled(FALSE);
}

// A message is reflected here from view().
// Return non-zero to indicate that it is handled here.
// Return 0 to allow view() to further process it.
LRESULT WebContentsViewWin::OnReflectedMessage(UINT msg, WPARAM w_param,
                                        LPARAM l_param) {
  MSG* message = reinterpret_cast<MSG*>(l_param);
  switch (message->message) {
    case WM_MOUSEWHEEL:
      // This message is reflected from the view() to this window.
      if (GET_KEYSTATE_WPARAM(message->wParam) & MK_CONTROL) {
        WheelZoom(GET_WHEEL_DELTA_WPARAM(message->wParam));
        return 1;
      }
      break;
    case WM_HSCROLL:
    case WM_VSCROLL:
      if (ScrollZoom(LOWORD(message->wParam)))
        return 1;
    default:
      break;
  }

  return 0;
}

void WebContentsViewWin::OnSetFocus(HWND window) {
  // TODO(jcampan): figure out why removing this prevents tabs opened in the
  //                background from properly taking focus.
  // We NULL-check the render_view_host_ here because Windows can send us
  // messages during the destruction process after it has been destroyed.
  if (web_contents_->render_widget_host_view()) {
    HWND inner_hwnd = web_contents_->render_widget_host_view()->GetPluginHWND();
    if (::IsWindow(inner_hwnd))
      ::SetFocus(inner_hwnd);
  }
}

void WebContentsViewWin::OnVScroll(int scroll_type, short position,
                                   HWND scrollbar) {
  ScrollCommon(WM_VSCROLL, scroll_type, position, scrollbar);
}

void WebContentsViewWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (window_pos->flags & SWP_HIDEWINDOW) {
    WasHidden();
  } else {
    // The WebContents was shown by a means other than the user selecting a
    // Tab, e.g. the window was minimized then restored.
    if (window_pos->flags & SWP_SHOWWINDOW)
      WasShown();

    // Unless we were specifically told not to size, cause the renderer to be
    // sized to the new bounds, which forces a repaint. Not required for the
    // simple minimize-restore case described above, for example, since the
    // size hasn't changed.
    if (!(window_pos->flags & SWP_NOSIZE))
      WasSized(gfx::Size(window_pos->cx, window_pos->cy));

    // If we have a FindInPage dialog, notify it that the window changed.
    if (find_bar_.get() && find_bar_->IsVisible())
      find_bar_->MoveWindowIfNecessary(gfx::Rect());
  }
}

void WebContentsViewWin::OnSize(UINT param, const CSize& size) {
  ContainerWin::OnSize(param, size);

  // Hack for thinkpad touchpad driver.
  // Set fake scrollbars so that we can get scroll messages,
  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_ALL;

  si.nMin = 1;
  si.nMax = 100;
  si.nPage = 10;
  si.nTrackPos = 50;

  ::SetScrollInfo(GetHWND(), SB_HORZ, &si, FALSE);
  ::SetScrollInfo(GetHWND(), SB_VERT, &si, FALSE);
}

LRESULT WebContentsViewWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  // Hack for thinkpad mouse wheel driver. We have set the fake scroll bars
  // to receive scroll messages from thinkpad touchpad driver. Suppress
  // painting of scrollbars by returning 0 size for them.
  return 0;
}

void WebContentsViewWin::OnNCPaint(HRGN rgn) {
  // Suppress default WM_NCPAINT handling. We don't need to do anything
  // here since the view will draw everything correctly.
}

void WebContentsViewWin::ScrollCommon(UINT message, int scroll_type,
                                      short position, HWND scrollbar) {
  // This window can receive scroll events as a result of the ThinkPad's
  // Trackpad scroll wheel emulation.
  if (!ScrollZoom(scroll_type)) {
    // Reflect scroll message to the view() to give it a chance
    // to process scrolling.
    SendMessage(GetContentHWND(), message, MAKELONG(scroll_type, position),
                (LPARAM) scrollbar);
  }
}

void WebContentsViewWin::WasHidden() {
  web_contents_->HideContents();
  if (find_bar_.get())
    find_bar_->DidBecomeUnselected();
}

void WebContentsViewWin::WasShown() {
  web_contents_->ShowContents();
  if (find_bar_.get())
    find_bar_->DidBecomeSelected();
}

void WebContentsViewWin::WasSized(const gfx::Size& size) {
  if (web_contents_->render_widget_host_view())
    web_contents_->render_widget_host_view()->SetSize(size);
  if (find_bar_.get())
    find_bar_->RespondToResize(size);

  // TODO(brettw) this function can probably be moved to this class.
  web_contents_->RepositionSupressedPopupsToFit(size);
}

bool WebContentsViewWin::ScrollZoom(int scroll_type) {
  // If ctrl is held, zoom the UI.  There are three issues with this:
  // 1) Should the event be eaten or forwarded to content?  We eat the event,
  //    which is like Firefox and unlike IE.
  // 2) Should wheel up zoom in or out?  We zoom in (increase font size), which
  //    is like IE and Google maps, but unlike Firefox.
  // 3) Should the mouse have to be over the content area?  We zoom as long as
  //    content has focus, although FF and IE require that the mouse is over
  //    content.  This is because all events get forwarded when content has
  //    focus.
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
    int distance = 0;
    switch (scroll_type) {
      case SB_LINEUP:
        distance = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:
        distance = -WHEEL_DELTA;
        break;
        // TODO(joshia): Handle SB_PAGEUP, SB_PAGEDOWN, SB_THUMBPOSITION,
        // and SB_THUMBTRACK for completeness
      default:
        break;
    }

    WheelZoom(distance);
    return true;
  }
  return false;
}

void WebContentsViewWin::WheelZoom(int distance) {
  if (web_contents_->delegate()) {
    bool zoom_in = distance > 0;
    web_contents_->delegate()->ContentsZoomChange(zoom_in);
  }
}
