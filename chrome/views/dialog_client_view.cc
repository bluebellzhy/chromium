// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>

#include "chrome/views/dialog_client_view.h"

#include "base/gfx/native_theme.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/window.h"
#include "generated_resources.h"

namespace ChromeViews {

namespace {

// Updates any of the standard buttons according to the delegate.
void UpdateButtonHelper(ChromeViews::NativeButton* button_view,
                        ChromeViews::DialogDelegate* delegate,
                        ChromeViews::DialogDelegate::DialogButton button) {
  std::wstring label = delegate->GetDialogButtonLabel(button);
  if (!label.empty())
    button_view->SetLabel(label);
  button_view->SetEnabled(delegate->IsDialogButtonEnabled(button));
  button_view->SetVisible(delegate->IsDialogButtonVisible(button));
}

void FillViewWithSysColor(ChromeCanvas* canvas, View* view, COLORREF color) {
  SkColor sk_color =
      SkColorSetRGB(GetRValue(color), GetGValue(color), GetBValue(color));
  canvas->FillRectInt(sk_color, 0, 0, view->width(), view->height());
}

// DialogButton ----------------------------------------------------------------

// DialogButtons is used for the ok/cancel buttons of the window. DialogButton
// forwrds AcceleratorPressed to the delegate.

class DialogButton : public NativeButton {
 public:
  DialogButton(Window* owner,
               DialogDelegate::DialogButton type,
               const std::wstring& title,
               bool is_default)
      : NativeButton(title, is_default), owner_(owner), type_(type) {
  }

  // Overridden to forward to the delegate.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    if (!owner_->window_delegate()->AsDialogDelegate()->
        AreAcceleratorsEnabled(type_)) {
      return false;
    }
    return NativeButton::AcceleratorPressed(accelerator);
  }

 private:
  Window* owner_;
  const DialogDelegate::DialogButton type_;

  DISALLOW_EVIL_CONSTRUCTORS(DialogButton);
};

}  // namespace

// static
ChromeFont DialogClientView::dialog_button_font_;
static const int kDialogMinButtonWidth = 75;
static const int kDialogButtonLabelSpacing = 16;
static const int kDialogButtonContentSpacing = 0;

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
static const int kButtonGroup = 6666;

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Window* owner, View* contents_view)
    : ClientView(owner, contents_view),
      ok_button_(NULL),
      cancel_button_(NULL),
      extra_view_(NULL),
      accepted_(false) {
  InitClass();
}

DialogClientView::~DialogClientView() {
}

void DialogClientView::ShowDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();
  if (buttons & DialogDelegate::DIALOGBUTTON_OK && !ok_button_) {
    std::wstring label =
        dd->GetDialogButtonLabel(DialogDelegate::DIALOGBUTTON_OK);
    if (label.empty())
      label = l10n_util::GetString(IDS_OK);
    ok_button_ = new DialogButton(
        window(), DialogDelegate::DIALOGBUTTON_OK,
        label,
        (dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_OK) != 0);
    ok_button_->SetListener(this);
    ok_button_->SetGroup(kButtonGroup);
    if (!cancel_button_)
      ok_button_->AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
    AddChildView(ok_button_);
  }
  if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL && !cancel_button_) {
    std::wstring label =
        dd->GetDialogButtonLabel(DialogDelegate::DIALOGBUTTON_CANCEL);
    if (label.empty()) {
      if (buttons & DialogDelegate::DIALOGBUTTON_OK) {
        label = l10n_util::GetString(IDS_CANCEL);
      } else {
        label = l10n_util::GetString(IDS_CLOSE);
      }
    }
    cancel_button_ = new DialogButton(
        window(), DialogDelegate::DIALOGBUTTON_CANCEL,
        label,
        (dd->GetDefaultDialogButton() & DialogDelegate::DIALOGBUTTON_CANCEL)
        != 0);
    cancel_button_->SetListener(this);
    cancel_button_->SetGroup(kButtonGroup);
    cancel_button_->AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
    AddChildView(cancel_button_);
  }

  ChromeViews::View* extra_view = dd->GetExtraView();
  if (extra_view && !extra_view_) {
    extra_view_ = extra_view;
    extra_view_->SetGroup(kButtonGroup);
    AddChildView(extra_view_);
  }
  if (!buttons) {
    // Register the escape key as an accelerator which will close the window
    // if there are no dialog buttons.
    AddAccelerator(Accelerator(VK_ESCAPE, false, false, false));
  }
}

// Changing dialog labels will change button widths.
void DialogClientView::UpdateDialogButtons() {
  DialogDelegate* dd = GetDialogDelegate();
  int buttons = dd->GetDialogButtons();

  if (buttons & DialogDelegate::DIALOGBUTTON_OK)
    UpdateButtonHelper(ok_button_, dd, DialogDelegate::DIALOGBUTTON_OK);

  if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL)
    UpdateButtonHelper(cancel_button_, dd, DialogDelegate::DIALOGBUTTON_CANCEL);

  LayoutDialogButtons();
  SchedulePaint();
}

void DialogClientView::AcceptWindow() {
  if (accepted_) {
    // We should only get into AcceptWindow once.
    NOTREACHED();
    return;
  }
  accepted_ = true;
  if (GetDialogDelegate()->Accept(false))
    window()->Close();
}

void DialogClientView::CancelWindow() {
  // Call the standard Close handler, which checks with the delegate before
  // proceeding. This checking _isn't_ done here, but in the WM_CLOSE handler,
  // so that the close box on the window also shares this code path.
  window()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, ClientView overrides:

bool DialogClientView::CanClose() const {
  if (!accepted_) {
    DialogDelegate* dd = GetDialogDelegate();
    int buttons = dd->GetDialogButtons();
    if (buttons & DialogDelegate::DIALOGBUTTON_CANCEL)
      return dd->Cancel();
    if (buttons & DialogDelegate::DIALOGBUTTON_OK)
      return dd->Accept(true);
  }
  return true;
}

int DialogClientView::NonClientHitTest(const gfx::Point& point) {
  if (size_box_bounds_.Contains(point.x() - x(), point.y() - y()))
    return HTBOTTOMRIGHT;
  return ClientView::NonClientHitTest(point);
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

void DialogClientView::Paint(ChromeCanvas* canvas) {
  FillViewWithSysColor(canvas, this, GetSysColor(COLOR_3DFACE));
}

void DialogClientView::PaintChildren(ChromeCanvas* canvas) {
  View::PaintChildren(canvas);
  if (!window()->IsMaximized() && !window()->IsMinimized())
    PaintSizeBox(canvas);
}

void DialogClientView::Layout() {
  if (has_dialog_buttons())
    LayoutDialogButtons();
  LayoutContentsView();
}

void DialogClientView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    // Can only add and update the dialog buttons _after_ they are added to the
    // view hierarchy since they are native controls and require the
    // ViewContainer's HWND.
    ShowDialogButtons();
    ClientView::ViewHierarchyChanged(is_add, parent, child);
    UpdateDialogButtons();
    Layout();
  }
}

void DialogClientView::DidChangeBounds(const CRect& prev, const CRect& next) {
  Layout();
}

void DialogClientView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  contents_view()->GetPreferredSize(out);
  int button_height = 0;
  if (has_dialog_buttons()) {
    if (cancel_button_)
      button_height = cancel_button_->height();
    else
      button_height = ok_button_->height();
    // Account for padding above and below the button.
    button_height += kDialogButtonContentSpacing + kButtonVEdgeMargin;
  }
  out->cy += button_height;
}

bool DialogClientView::AcceleratorPressed(const Accelerator& accelerator) {
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);  // We only expect Escape key.
  window()->Close();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, NativeButton::Listener implementation:

void DialogClientView::ButtonPressed(NativeButton* sender) {
  if (sender == ok_button_) {
    AcceptWindow();
  } else if (sender == cancel_button_) {
    CancelWindow();
  } else {
    NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

void DialogClientView::PaintSizeBox(ChromeCanvas* canvas) {
  if (window()->window_delegate()->CanResize() ||
      window()->window_delegate()->CanMaximize()) {
    HDC dc = canvas->beginPlatformPaint();
    SIZE gripper_size = { 0, 0 };
    gfx::NativeTheme::instance()->GetThemePartSize(
        gfx::NativeTheme::STATUS, dc, SP_GRIPPER, 1, NULL, TS_TRUE,
        &gripper_size);

    // TODO(beng): (http://b/1085509) In "classic" rendering mode, there isn't
    //             a theme-supplied gripper. We should probably improvise
    //             something, which would also require changing |gripper_size|
    //             to have different default values, too...
    CRect gripper_bounds;
    GetLocalBounds(&gripper_bounds, false);
    gripper_bounds.left = gripper_bounds.right - gripper_size.cx;
    gripper_bounds.top = gripper_bounds.bottom - gripper_size.cy;
    size_box_bounds_ = gripper_bounds;
    gfx::NativeTheme::instance()->PaintStatusGripper(
        dc, SP_PANE, 1, 0, gripper_bounds);
    canvas->endPlatformPaint();
  }
}

int DialogClientView::GetButtonWidth(int button) const {
  DialogDelegate* dd = GetDialogDelegate();
  std::wstring button_label = dd->GetDialogButtonLabel(
      static_cast<DialogDelegate::DialogButton>(button));
  int string_width = dialog_button_font_.GetStringWidth(button_label);
  return std::max(string_width + kDialogButtonLabelSpacing,
                  kDialogMinButtonWidth);
}

int DialogClientView::GetButtonsHeight() const {
  if (has_dialog_buttons()) {
    if (cancel_button_)
      return cancel_button_->height() + kDialogButtonContentSpacing;
    return ok_button_->height() + kDialogButtonContentSpacing;
  }
  return 0;
}

void DialogClientView::LayoutDialogButtons() {
  CRect extra_bounds;
  if (cancel_button_) {
    CSize ps;
    cancel_button_->GetPreferredSize(&ps);
    CRect lb;
    GetLocalBounds(&lb, false);
    int button_width = GetButtonWidth(DialogDelegate::DIALOGBUTTON_CANCEL);
    CRect bounds;
    bounds.left = lb.right - button_width - kButtonHEdgeMargin;
    bounds.top = lb.bottom - ps.cy - kButtonVEdgeMargin;
    bounds.right = bounds.left + button_width;
    bounds.bottom = bounds.top + ps.cy;
    cancel_button_->SetBounds(bounds);
    // The extra view bounds are dependent on this button.
    extra_bounds.right = bounds.left;
    extra_bounds.top = bounds.top;
  }
  if (ok_button_) {
    CSize ps;
    ok_button_->GetPreferredSize(&ps);
    CRect lb;
    GetLocalBounds(&lb, false);
    int button_width = GetButtonWidth(DialogDelegate::DIALOGBUTTON_OK);
    int ok_button_right = lb.right - kButtonHEdgeMargin;
    if (cancel_button_)
      ok_button_right = cancel_button_->x() - kRelatedButtonHSpacing;
    CRect bounds;
    bounds.left = ok_button_right - button_width;
    bounds.top = lb.bottom - ps.cy - kButtonVEdgeMargin;
    bounds.right = ok_button_right;
    bounds.bottom = bounds.top + ps.cy;
    ok_button_->SetBounds(bounds);
    // The extra view bounds are dependent on this button.
    extra_bounds.right = bounds.left;
    extra_bounds.top = bounds.top;
  }
  if (extra_view_) {
    CSize ps;
    extra_view_->GetPreferredSize(&ps);
    CRect lb;
    GetLocalBounds(&lb, false);
    extra_bounds.left = lb.left + kButtonHEdgeMargin;
    extra_bounds.bottom = extra_bounds.top + ps.cy;
    extra_view_->SetBounds(extra_bounds);
  }
}

void DialogClientView::LayoutContentsView() {
  CRect lb;
  GetLocalBounds(&lb, false);
  lb.bottom = std::max(0, static_cast<int>(lb.bottom - GetButtonsHeight()));
  contents_view()->SetBounds(lb);
  contents_view()->Layout();
}

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  DialogDelegate* dd = window()->window_delegate()->AsDialogDelegate();
  DCHECK(dd);
  return dd;
}

// static
void DialogClientView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    dialog_button_font_ = rb.GetFont(ResourceBundle::BaseFont);
    initialized = true;
  }
}

}

