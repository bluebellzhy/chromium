// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_TABBED_PANE_H__
#define CHROME_VIEWS_TABBED_PANE_H__

#include "chrome/views/native_control.h"

namespace views {

// The TabbedPane class is a view that shows tabs.  When the user clicks on a
// tab, the associated view is displayed.
// TODO (jcampan): implement GetPreferredSize().
class ContainerWin;

class TabbedPane : public NativeControl {
 public:
  TabbedPane();
  virtual ~TabbedPane();

  // An interface an object can implement to be notified about events within
  // the TabbedPane.
  class Listener {
   public:
    // Called when the tab at the specified |index| is selected by the user.
    virtual void TabSelectedAt(int index) = 0;
  };
  void SetListener(Listener* listener);

  // Adds a new tab at the end of this TabbedPane with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane.
  void AddTab(const std::wstring& title, View* contents);

  // Adds a new tab at the specified |index| with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane. If |select_if_first_tab| is true and the tabbed pane is
  // currently empty, the new tab is selected. If you pass in false for
  // |select_if_first_tab| you need to explicitly invoke SelectTabAt, otherwise
  // the tabbed pane will not have a valid selection.
  void AddTabAtIndex(int index,
                     const std::wstring& title,
                     View* contents,
                     bool select_if_first_tab);

  // Removes the tab at the specified |index| and returns the associated content
  // view.  The caller becomes the owner of the returned view.
  View* RemoveTabAtIndex(int index);

  // Selects the tab at the specified |index|.
  void SelectTabAt(int index);

  // Returns the number of tabs.
  int GetTabCount();

  virtual HWND CreateNativeControl(HWND parent_container);
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

  virtual void Layout();

  virtual RootView* GetContentsRootView();
  virtual FocusTraversable* GetFocusTraversable();
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  // Changes the contents view to the view associated with the tab at |index|.
  void DoSelectTabAt(int index);

  void ResizeContents(HWND tab_control);

  HWND tab_control_;

  // The views associated with the different tabs.
  std::vector<View*> tab_views_;

  // The window displayed in the tab.
  ContainerWin* content_window_;

  // The listener we notify about tab selection changes.
  Listener* listener_;

  DISALLOW_EVIL_CONSTRUCTORS(TabbedPane);
};

}  // namespace views

#endif  // #define CHROME_VIEWS_TABBED_PANE_H__
