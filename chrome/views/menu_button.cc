// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <atlbase.h>
#include <atlapp.h>

#include "chrome/views/menu_button.h"

#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/event.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_menu_delegate.h"
#include "chrome/views/view_container.h"

#include "generated_resources.h"

namespace ChromeViews {

// The amount of time, in milliseconds, we wait before allowing another mouse
// pressed event to show the menu.
static const int64 kMinimumTimeBetweenButtonClicks = 100;

// The down arrow used to differentiate the menu button from normal
// text buttons.
static const SkBitmap* kMenuMarker = NULL;

// How much padding to put on the left and right of the menu marker.
static const int kMenuMarkerPaddingLeft = 3;
static const int kMenuMarkerPaddingRight = -1;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(const std::wstring& text,
                       ViewMenuDelegate* menu_delegate,
                       bool show_menu_marker)
    : TextButton(text),
      menu_visible_(false),
      menu_closed_time_(Time::Now()),
      menu_delegate_(menu_delegate),
      show_menu_marker_(show_menu_marker) {
  if (kMenuMarker == NULL) {
    kMenuMarker = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_MENU_MARKER);
  }
  SetTextAlignment(TextButton::ALIGN_LEFT);
}

MenuButton::~MenuButton() {
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

void MenuButton::GetPreferredSize(CSize* result) {
  TextButton::GetPreferredSize(result);
  if (show_menu_marker_) {
    result->cx += kMenuMarker->width() + kMenuMarkerPaddingLeft +
        kMenuMarkerPaddingRight;
  }
}

void MenuButton::Paint(ChromeCanvas* canvas, bool for_drag) {
  TextButton::Paint(canvas, for_drag);

  if (show_menu_marker_) {
    gfx::Insets insets = GetInsets();

    // We can not use the ChromeViews' mirroring infrastructure for mirroring
    // a MenuButton control (see TextButton::Paint() for a detailed
    // explanation regarding why we can not flip the canvas). Therefore, we
    // need to manually mirror the position of the down arrow.
    gfx::Rect arrow_bounds(GetWidth() - insets.right() -
                           kMenuMarker->width() - kMenuMarkerPaddingRight,
                           GetHeight() / 2,
                           kMenuMarker->width(),
                           kMenuMarker->height());
    arrow_bounds.set_x(MirroredLeftPointForRect(arrow_bounds));
    canvas->DrawBitmapInt(*kMenuMarker, arrow_bounds.x(), arrow_bounds.y());
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

int MenuButton::GetMaximumScreenXCoordinate() {
  ViewContainer* vc = GetViewContainer();

  if (!vc) {
    NOTREACHED();
    return 0;
  }

  HWND hwnd = vc->GetHWND();
  CRect t;
  ::GetWindowRect(hwnd, &t);

  gfx::Rect r(t);
  gfx::Rect monitor_rect = win_util::GetMonitorBoundsForRect(r);
  return monitor_rect.x() + monitor_rect.width() - 1;
}

bool MenuButton::Activate() {
  SetState(BS_PUSHED);
  // We need to synchronously paint here because subsequently we enter a
  // menu modal loop which will stop this window from updating and
  // receiving the paint message that should be spawned by SetState until
  // after the menu closes.
  PaintNow();
  if (menu_delegate_) {
    CRect lb;
    GetLocalBounds(&lb, true);

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    CPoint menu_position = lb.BottomRight();
    if (UILayoutIsRightToLeft())
      menu_position.x = lb.left;

    View::ConvertPointToScreen(this, &menu_position);
    if (UILayoutIsRightToLeft())
      menu_position.Offset(2, -4);
    else
      menu_position.Offset(-2, -4);

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x)
      menu_position.x = max_x_coordinate - 1;

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    GetRootView()->SetMouseHandler(NULL);

    menu_visible_ = true;
    menu_delegate_->RunMenu(this, menu_position, GetViewContainer()->GetHWND());
    menu_visible_ = false;
    menu_closed_time_ = Time::Now();

    // Now that the menu has closed, we need to manually reset state to
    // "normal" since the menu modal loop will have prevented normal
    // mouse move messages from getting to this View. We set "normal"
    // and not "hot" because the likelihood is that the mouse is now
    // somewhere else (user clicked elsewhere on screen to close the menu
    // or selected an item) and we will inevitably refresh the hot state
    // in the event the mouse _is_ over the view.
    SetState(BS_NORMAL);
    SchedulePaint();

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }
  return true;
}

bool MenuButton::OnMousePressed(const ChromeViews::MouseEvent& e) {
  using namespace ChromeViews;
  if (IsFocusable())
    RequestFocus();
  if (GetState() != BS_DISABLED) {
    // If we're draggable (GetDragOperations returns a non-zero value), then
    // don't pop on press, instead wait for release.
    if (e.IsOnlyLeftMouseButton() && HitTest(e.GetLocation()) &&
        GetDragOperations(e.GetX(), e.GetY()) == DragDropTypes::DRAG_NONE) {
      TimeDelta delta = Time::Now() - menu_closed_time_;
      int64 delta_in_milliseconds = delta.InMilliseconds();
      if (delta_in_milliseconds > kMinimumTimeBetweenButtonClicks) {
        return Activate();
      }
    }
  }
  return true;
}

void MenuButton::OnMouseReleased(const ChromeViews::MouseEvent& e,
                                 bool canceled) {
  if (GetDragOperations(e.GetX(), e.GetY()) != DragDropTypes::DRAG_NONE &&
      GetState() != BS_DISABLED && !canceled && !InDrag() &&
      e.IsOnlyLeftMouseButton() && HitTest(e.GetLocation())) {
    Activate();
  } else {
    TextButton::OnMouseReleased(e, canceled);
  }
}

// When the space bar or the enter key is pressed we need to show the menu.
bool MenuButton::OnKeyReleased(const KeyEvent& e) {
  if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
    return Activate();
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - accessibility
//
////////////////////////////////////////////////////////////////////////////////

bool MenuButton::GetAccessibleDefaultAction(std::wstring* action) {
  DCHECK(action);

  action->assign(l10n_util::GetString(IDS_ACCACTION_PRESS));
  return true;
}

bool MenuButton::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_BUTTONDROPDOWN;
  return true;
}

bool MenuButton::GetAccessibleState(VARIANT* state) {
  DCHECK(state);

  state->lVal |= STATE_SYSTEM_HASPOPUP;
  return true;
}

// The reason we override View::OnMouseExited is because we get this event when
// we display the menu. If we don't override this method then
// BaseButton::OnMouseExited will get the event and will set the button's state
// to BS_NORMAL instead of keeping the state BM_PUSHED. This, in turn, will
// cause the button to appear depressed while the menu is displayed.
void MenuButton::OnMouseExited(const MouseEvent& event) {
  using namespace ChromeViews;
  if ((state_ != BS_DISABLED) && (!menu_visible_) && (!InDrag())) {
    SetState(BS_NORMAL);
  }
}

}  // namespace ChromeViews
