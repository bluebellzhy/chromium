// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__
#define CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__

#include "chrome/views/view.h"

namespace ChromeViews {
class Label;
class Separator;
};

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView
//
//  A helper View that gathers related options into groups with a title and
//  optional description.
//
class OptionsGroupView : public ChromeViews::View {
 public:
  OptionsGroupView(ChromeViews::View* contents,
                   const std::wstring& title,
                   const std::wstring& description,
                   bool show_separator);
  virtual ~OptionsGroupView() {}

  // Sets the group as being highlighted to attract attention.
  void SetHighlighted(bool highlighted);

  // Retrieves the width of the ContentsView. Used to help size wrapping items.
  int GetContentsWidth() const;

  class ContentsView : public ChromeViews::View {
   public:
    virtual ~ContentsView() {}

    // ChromeViews::View overrides:
    virtual void DidChangeBounds(const CRect& prev_bounds,
                                 const CRect& next_bounds) {
      Layout();
    }

   private:
    DISALLOW_EVIL_CONSTRUCTORS(ContentsView);
  };

 protected:
  // ChromeViews::View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  void Init();

  ChromeViews::View* contents_;
  ChromeViews::Label* title_label_;
  ChromeViews::Label* description_label_;
  ChromeViews::Separator* separator_;

  // True if we should show a separator line below the contents of this
  // section.
  bool show_separator_;

  // True if this section should have a highlighted treatment to draw the
  // user's attention.
  bool highlighted_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionsGroupView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_OPTIONS_OPTIONS_GROUP_VIEW_H__

