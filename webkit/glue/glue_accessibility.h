// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_GLUE_ACCESSIBILITY_H_
#define WEBKIT_GLUE_GLUE_ACCESSIBILITY_H_

#include <oleacc.h>
#include <hash_map>

#include "chrome/common/render_messages.h"

class WebView;

typedef stdext::hash_map<int, IAccessible*> IntToIAccessibleMap;
typedef stdext::hash_map<IAccessible*, int> IAccessibleToIntMap;

////////////////////////////////////////////////////////////////////////////////
//
// GlueAccessibility
//
// Operations that access the underlying WebKit DOM directly, exposing
// accessibility information.
////////////////////////////////////////////////////////////////////////////////
class GlueAccessibility {
 public:
  GlueAccessibility();
  ~GlueAccessibility();

  // Retrieves the IAccessible information as requested in in_params, by calling
  // into WebKit's implementation of IAccessible. Maintains a hashmap of the
  // currently active (browser ref count not zero) IAccessibles. Returns true if
  // successful, false otherwise.
  bool GetAccessibilityInfo(WebView* view,
                            const ViewMsg_Accessibility_In_Params& in_params,
                            ViewHostMsg_Accessibility_Out_Params* out_params);

  // Retrieves the RenderObject associated with this WebView, and uses it to
  // initialize the root of the render-side MSAA tree with the associated
  // accessibility information. Returns true if successful, false otherwise.
  bool InitAccessibilityRoot(WebView* view);

  // Erases the entry identified by the |iaccessible_id| from the hash map. If
  // |clear_all| is true, all entries are erased. Returns true if successful,
  // false otherwise.
  bool ClearIAccessibleMap(int iaccessible_id, bool clear_all);

 private:
  // Wrapper around the COM pointer that holds the root of the MSAA tree, to
  // ensure that we are not requiring WebKit includes outside of glue.
  struct GlueAccessibilityRoot;
  GlueAccessibilityRoot* root_;

  // Hashmap for cashing of elements in use by the AT, mapping id (int) to an
  // IAccessible pointer.
  IntToIAccessibleMap int_to_iaccessible_map_;
  // Hashmap for cashing of elements in use by the AT, mapping an IAccessible
  // pointer to its id (int). Needed for reverse lookup, to ensure unnecessary
  // duplicate entries are not created in the IntToIAccessibleMap (above).
  IAccessibleToIntMap iaccessible_to_int_map_;

  // Unique identifier for retrieving an IAccessible from the page's hashmap.
  int iaccessible_id_;

  DISALLOW_COPY_AND_ASSIGN(GlueAccessibility);
};

#endif  // WEBKIT_GLUE_GLUE_ACCESSIBILITY_H_
