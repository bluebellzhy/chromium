// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/tree_view.h"

#include <shellapi.h>

#include "base/win_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/icon_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/views/container.h"
#include "chrome/views/focus_manager.h"

namespace views {

static HIMAGELIST tree_image_list_ = NULL;

// Creates the default image list used for trees. The image list is populated
// from the shell's icons.
static HIMAGELIST CreateDefaultImageList(bool rtl) {
  SkBitmap* closed_icon =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          (rtl ? IDR_FOLDER_CLOSED_RTL : IDR_FOLDER_CLOSED));
  SkBitmap* opened_icon =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          (rtl ? IDR_FOLDER_OPEN_RTL : IDR_FOLDER_OPEN));
  int width = closed_icon->width();
  int height = closed_icon->height();
  DCHECK(opened_icon->width() == width && opened_icon->height() == height);
  HIMAGELIST image_list = ImageList_Create(width, height, ILC_COLOR32, 2, 2);
  if (image_list) {
    // NOTE: the order the images are added in effects the selected
    // image index when adding items to the tree. If you change the
    // order you'll undoubtedly need to update itemex.iSelectedImage
    // when the item is added.
    HICON h_closed_icon = IconUtil::CreateHICONFromSkBitmap(*closed_icon);
    HICON h_opened_icon = IconUtil::CreateHICONFromSkBitmap(*opened_icon);
    ImageList_AddIcon(image_list, h_closed_icon);
    ImageList_AddIcon(image_list, h_opened_icon);
    DestroyIcon(h_closed_icon);
    DestroyIcon(h_opened_icon);
  }
  return image_list;
}

TreeView::TreeView()
    : tree_view_(NULL),
      model_(NULL),
      editable_(true),
      next_id_(0),
      controller_(NULL),
      editing_node_(NULL),
      root_shown_(true),
      process_enter_(false),
      show_context_menu_only_when_node_selected_(true),
      select_on_right_mouse_down_(true),
      wrapper_(this),
      original_handler_(NULL) {
}

TreeView::~TreeView() {
  if (model_)
    model_->SetObserver(NULL);
  // Both param_to_details_map_ and node_to_details_map_ have the same value,
  // as such only need to delete from one.
  STLDeleteContainerPairSecondPointers(id_to_details_map_.begin(),
                                       id_to_details_map_.end());
}

void TreeView::SetModel(TreeModel* model) {
  if (model == model_)
    return;
  if(model_ && tree_view_)
    DeleteRootItems();
  if (model_)
    model_->SetObserver(NULL);
  model_ = model;
  if (tree_view_ && model_) {
    CreateRootItems();
    model_->SetObserver(this);
  }
}

// Sets whether the user can edit the nodes. The default is true.
void TreeView::SetEditable(bool editable) {
  if (editable == editable_)
    return;
  editable_ = editable;
  if (!tree_view_)
    return;
  LONG_PTR style = GetWindowLongPtr(tree_view_, GWL_STYLE);
  style &= ~TVS_EDITLABELS;
  SetWindowLongPtr(tree_view_, GWL_STYLE, style);
}

void TreeView::StartEditing(TreeModelNode* node) {
  DCHECK(node && tree_view_);
  // Cancel the current edit.
  CancelEdit();
  // Make sure all ancestors are expanded.
  if (model_->GetParent(node))
    Expand(model_->GetParent(node));
  const NodeDetails* details = GetNodeDetails(node);
  // Tree needs focus for editing to work.
  SetFocus(tree_view_);
  // Select the node, else if the user commits the edit the selection reverts.
  SetSelectedNode(node);
  TreeView_EditLabel(tree_view_, details->tree_item);
}

void TreeView::CancelEdit() {
  DCHECK(tree_view_);
  TreeView_EndEditLabelNow(tree_view_, TRUE);
}

void TreeView::CommitEdit() {
  DCHECK(tree_view_);
  TreeView_EndEditLabelNow(tree_view_, FALSE);
}

TreeModelNode* TreeView::GetEditingNode() {
  // I couldn't find a way to dynamically query for this, so it is cached.
  return editing_node_;
}

void TreeView::SetSelectedNode(TreeModelNode* node) {
  DCHECK(tree_view_);
  if (!node) {
    TreeView_SelectItem(tree_view_, NULL);
    return;
  }
  if (node != model_->GetRoot())
    Expand(model_->GetParent(node));
  if (!root_shown_ && node == model_->GetRoot()) {
    // If the root isn't shown, we can't select it, clear out the selection
    // instead.
    TreeView_SelectItem(tree_view_, NULL);
  } else {
    // Select the node and make sure it is visible.
    TreeView_SelectItem(tree_view_, GetNodeDetails(node)->tree_item);
  }
}

TreeModelNode* TreeView::GetSelectedNode() {
  if (!tree_view_)
    return NULL;
  HTREEITEM selected_item = TreeView_GetSelection(tree_view_);
  if (!selected_item)
    return NULL;
  NodeDetails* details = GetNodeDetailsByTreeItem(selected_item);
  DCHECK(details);
  return details->node;
}

void TreeView::Expand(TreeModelNode* node) {
  DCHECK(model_ && node);
  if (!root_shown_ && model_->GetRoot() == node) {
    // Can only expand the root if it is showing.
    return;
  }
  TreeModelNode* parent = model_->GetParent(node);
  if (parent) {
    // Make sure all the parents are expanded.
    Expand(parent);
  }
  // And expand this item.
  TreeView_Expand(tree_view_, GetNodeDetails(node)->tree_item, TVE_EXPAND);
}

void TreeView::ExpandAll() {
  DCHECK(model_);
  ExpandAll(model_->GetRoot());
}

void TreeView::SetRootShown(bool root_shown) {
  if (root_shown_ == root_shown)
    return;
  root_shown_ = root_shown;
  if (!model_)
    return;
  // Repopulate the tree.
  DeleteRootItems();
  CreateRootItems();
}

void TreeView::TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count) {
  DCHECK(parent && start >= 0 && count > 0);
  if (node_to_details_map_.find(parent) == node_to_details_map_.end()) {
    // User hasn't navigated to this entry yet. Ignore the change.
    return;
  }
  HTREEITEM parent_tree_item = NULL;
  if (root_shown_ || parent != model_->GetRoot()) {
    const NodeDetails* details = GetNodeDetails(parent);
    if (!details->loaded_children) {
      if (count == model_->GetChildCount(parent)) {
        // Reset the treeviews child count. This triggers the treeview to call
        // us back.
        TV_ITEM tv_item = {0};
        tv_item.mask = TVIF_CHILDREN;
        tv_item.cChildren = count;
        tv_item.hItem = details->tree_item;
        TreeView_SetItem(tree_view_, &tv_item);
      }

      // Ignore the change, we haven't actually created entries in the tree
      // for the children.
      return;
    }
    parent_tree_item = details->tree_item;
  }

  // The user has expanded this node, add the items to it.
  for (int i = 0; i < count; ++i) {
    if (i == 0 && start == 0) {
      CreateItem(parent_tree_item, TVI_FIRST, model_->GetChild(parent, 0));
    } else {
      TreeModelNode* previous_sibling = model_->GetChild(parent, i + start - 1);
      CreateItem(parent_tree_item,
                   GetNodeDetails(previous_sibling)->tree_item,
                   model_->GetChild(parent, i + start));
    }
  }
}

void TreeView::TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count) {
  DCHECK(parent && start >= 0 && count > 0);
  if (node_to_details_map_.find(parent) == node_to_details_map_.end()) {
    // User hasn't navigated to this entry yet. Ignore the change.
    return;
  }
  HTREEITEM parent_tree_item = NULL;
  if (!root_shown_ || parent != model_->GetRoot()) {
    const NodeDetails* details = GetNodeDetails(parent);
    if (!details->loaded_children) {
      // Ignore the change, we haven't actually created entries in the tree
      // for the children.
      return;
    }
    parent_tree_item = details->tree_item;
  } else {
    parent_tree_item = TreeView_GetRoot(tree_view_);
  }
  // Find the last item. Windows doesn't offer a convenient way to get the
  // TREEITEM at a particular index, so we iterate.
  HTREEITEM tree_item = TreeView_GetChild(tree_view_, parent_tree_item);
  for (int i = 0; i < (start + count - 1); ++i) {
    tree_item = TreeView_GetNextSibling(tree_view_, tree_item);
  }
  // NOTE: the direction doesn't matter here. I've made it backwards to
  // reinforce we're deleting from the end forward.
  for (int i = count - 1; i >= 0; --i) {
    HTREEITEM previous = (start + i) > 0 ?
        TreeView_GetPrevSibling(tree_view_, tree_item) : NULL;
    RecursivelyDelete(GetNodeDetailsByTreeItem(tree_item));
    tree_item = previous;
  }
}

void TreeView::TreeNodeChanged(TreeModel* model, TreeModelNode* node) {
  if (node_to_details_map_.find(node) == node_to_details_map_.end()) {
    // User hasn't navigated to this entry yet. Ignore the change.
    return;
  }
  const NodeDetails* details = GetNodeDetails(node);
  TV_ITEM tv_item = {0};
  tv_item.mask = TVIF_TEXT;
  tv_item.hItem = details->tree_item;
  tv_item.pszText = LPSTR_TEXTCALLBACK;
  TreeView_SetItem(tree_view_, &tv_item);
}

HWND TreeView::CreateNativeControl(HWND parent_container) {
  int style = WS_CHILD | TVS_DISABLEDRAGDROP | TVS_HASBUTTONS |
              TVS_HASLINES | TVS_SHOWSELALWAYS;
  if (editable_)
    style |= TVS_EDITLABELS;
  tree_view_ = ::CreateWindowEx(WS_EX_CLIENTEDGE | GetAdditionalExStyle(),
                                WC_TREEVIEW,
                                L"",
                                style,
                                0, 0, width(), height(),
                                parent_container, NULL, NULL, NULL);
  SetWindowLongPtr(tree_view_, GWLP_USERDATA,
                   reinterpret_cast<LONG_PTR>(&wrapper_));
  original_handler_ = win_util::SetWindowProc(tree_view_,
                                              &TreeWndProc);
  // Tree-View doesn't render icons by default. Use an image list that is
  // populated with icons from the shell.
  if (!tree_image_list_)
    tree_image_list_ = CreateDefaultImageList(UILayoutIsRightToLeft());
  if (tree_image_list_)
    TreeView_SetImageList(tree_view_, tree_image_list_, TVSIL_NORMAL);

  if (model_) {
    CreateRootItems();
    model_->SetObserver(this);
  }

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(tree_view_, NULL, 0);
  return tree_view_;
}

LRESULT TreeView::OnNotify(int w_param, LPNMHDR l_param) {
  switch (l_param->code) {
    case TVN_GETDISPINFO: {
      // Windows is requesting more information about an item.
      // WARNING: At the time this is called the tree_item of the NodeDetails
      // in the maps is NULL.
      DCHECK(model_);
      NMTVDISPINFO* info = reinterpret_cast<NMTVDISPINFO*>(l_param);
      const NodeDetails* details =
          GetNodeDetailsByID(static_cast<int>(info->item.lParam));
      if (info->item.mask & TVIF_CHILDREN)
        info->item.cChildren = model_->GetChildCount(details->node);
      if (info->item.mask & TVIF_TEXT) {
        std::wstring text = details->node->GetTitle();
        DCHECK(info->item.cchTextMax);

        // Adjust the string direction if such adjustment is required.
        std::wstring localized_text;
        if (l10n_util::AdjustStringForLocaleDirection(text, &localized_text))
          text.swap(localized_text);

        wcsncpy_s(info->item.pszText, info->item.cchTextMax, text.c_str(),
                  _TRUNCATE);
      }
      // Instructs windows to cache the values for this node.
      info->item.mask |= TVIF_DI_SETITEM;
      // Return value ignored.
      return 0;
    }

    case TVN_ITEMEXPANDING: {
      // Notification that a node is expanding. If we haven't populated the
      // tree view with the contents of the model, we do it here.
      DCHECK(model_);
      NMTREEVIEW* info = reinterpret_cast<NMTREEVIEW*>(l_param);
      NodeDetails* details =
          GetNodeDetailsByID(static_cast<int>(info->itemNew.lParam));
      if (!details->loaded_children) {
        details->loaded_children = true;
        for (int i = 0; i < model_->GetChildCount(details->node); ++i)
          CreateItem(details->tree_item, TVI_LAST,
                       model_->GetChild(details->node, i));
      }
      // Return FALSE to allow the item to be expanded.
      return FALSE;
    }

    case TVN_SELCHANGED:
      if (controller_)
        controller_->OnTreeViewSelectionChanged(this);
      break;

    case TVN_BEGINLABELEDIT: {
      NMTVDISPINFO* info = reinterpret_cast<NMTVDISPINFO*>(l_param);
      NodeDetails* details =
          GetNodeDetailsByID(static_cast<int>(info->item.lParam));
      // Return FALSE to allow editing.
      if (!controller_ || controller_->CanEdit(this, details->node)) {
        editing_node_ = details->node;
        return FALSE;
      }
      return TRUE;
    }

    case TVN_ENDLABELEDIT: {
      NMTVDISPINFO* info = reinterpret_cast<NMTVDISPINFO*>(l_param);
      if (info->item.pszText) {
        // User accepted edit.
        NodeDetails* details =
            GetNodeDetailsByID(static_cast<int>(info->item.lParam));
        model_->SetTitle(details->node, info->item.pszText);
        editing_node_ = NULL;
        // Return FALSE so that the tree item doesn't change its text (if the
        // model changed the value, it should have sent out notification which
        // will have updated the value).
        return FALSE;
      }
      editing_node_ = NULL;
      // Return value ignored.
      return 0;
    }

    default:
      break;
  }
  return 0;
}

bool TreeView::OnKeyDown(int virtual_key_code) {
  if (virtual_key_code == VK_F2) {
    if (!GetEditingNode()) {
      TreeModelNode* selected_node = GetSelectedNode();
      if (selected_node)
        StartEditing(selected_node);
    }
    return true;
  } else if (virtual_key_code == VK_RETURN && !process_enter_) {
    Container* vc = GetContainer();
    DCHECK(vc);
    FocusManager* fm = FocusManager::GetFocusManager(vc->GetHWND());
    DCHECK(fm);
    Accelerator accelerator(Accelerator(static_cast<int>(virtual_key_code),
                                        win_util::IsShiftPressed(),
                                        win_util::IsCtrlPressed(),
                                        win_util::IsAltPressed()));
    fm->ProcessAccelerator(accelerator, true);
    return true;
  }
  return false;
}

void TreeView::OnContextMenu(const CPoint& location) {
  if (GetContextMenuController()) {
    if (location.x == -1 && location.y == -1) {
      // Indicates the user pressed the context menu key.
      bool valid_loc = false;
      int x, y;
      if (GetSelectedNode()) {
        RECT bounds;
        if (TreeView_GetItemRect(tree_view_,
                                 GetNodeDetails(GetSelectedNode())->tree_item,
                                 &bounds, TRUE)) {
          x = bounds.left;
          y = bounds.top + (bounds.bottom - bounds.top) / 2;
          valid_loc = true;
        }
      } else if (show_context_menu_only_when_node_selected_) {
        return;
      }
      if (!valid_loc) {
        x = width() / 2;
        y = height() / 2;
      }
      gfx::Point screen_loc(x, y);
      ConvertPointToScreen(this, &screen_loc);
      GetContextMenuController()->ShowContextMenu(this, screen_loc.x(),
                                                  screen_loc.y(), false);
    } else if (!show_context_menu_only_when_node_selected_) {
      GetContextMenuController()->ShowContextMenu(this, location.x, location.y,
                                                  true);
    } else if (GetSelectedNode()) {
      // Make sure the mouse is over the selected node.
      TVHITTESTINFO hit_info;
      gfx::Point local_loc(location);
      ConvertPointToView(NULL, this, &local_loc);
      hit_info.pt.x = local_loc.x();
      hit_info.pt.y = local_loc.y();
      HTREEITEM hit_item = TreeView_HitTest(tree_view_, &hit_info);
      if (hit_item &&
          GetNodeDetails(GetSelectedNode())->tree_item == hit_item &&
          (hit_info.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT |
                            TVHT_ONITEMINDENT)) != 0) {
        GetContextMenuController()->ShowContextMenu(this, location.x,
                                                    location.y, true);
      }
    }
  }
}

void TreeView::ExpandAll(TreeModelNode* node) {
  DCHECK(node);
  // Expand the node.
  if (node != model_->GetRoot() || root_shown_)
    TreeView_Expand(tree_view_, GetNodeDetails(node)->tree_item, TVE_EXPAND);
  // And recursively expand all the children.
  for (int i = model_->GetChildCount(node) - 1; i >= 0; --i) {
    TreeModelNode* child = model_->GetChild(node, i);
    ExpandAll(child);
  }
}

void TreeView::DeleteRootItems() {
  HTREEITEM root = TreeView_GetRoot(tree_view_);
  if (root) {
    if (root_shown_) {
      RecursivelyDelete(GetNodeDetailsByTreeItem(root));
    } else {
      HTREEITEM node;
      while ((node = TreeView_GetChild(tree_view_, root))) {
        RecursivelyDelete(GetNodeDetailsByTreeItem(node));
      }
    }
  }
}

void TreeView::CreateRootItems() {
  DCHECK(model_);
  TreeModelNode* root = model_->GetRoot();
  if (root_shown_) {
    CreateItem(NULL, TVI_LAST, root);
  } else {
    for (int i = 0; i < model_->GetChildCount(root); ++i)
      CreateItem(NULL, TVI_LAST, model_->GetChild(root, i));
  }
}

void TreeView::CreateItem(HTREEITEM parent_item,
                          HTREEITEM after,
                          TreeModelNode* node) {
  DCHECK(node);
  TVINSERTSTRUCT insert_struct = {0};
  insert_struct.hParent = parent_item;
  insert_struct.hInsertAfter = after;
  insert_struct.itemex.mask = TVIF_PARAM | TVIF_CHILDREN | TVIF_TEXT |
                              TVIF_SELECTEDIMAGE;
  // Call us back for the text.
  insert_struct.itemex.pszText = LPSTR_TEXTCALLBACK;
  // And the number of children.
  insert_struct.itemex.cChildren = I_CHILDRENCALLBACK;
  // Index in the image list for the image when the node is selected.
  insert_struct.itemex.iSelectedImage = 1;
  int node_id = next_id_++;
  insert_struct.itemex.lParam = node_id;

  // Invoking TreeView_InsertItem triggers OnNotify to be called. As such,
  // we set the map entries before adding the item.
  NodeDetails* node_details = new NodeDetails(node_id, node);

  node_to_details_map_[node] = node_details;
  id_to_details_map_[node_id] = node_details;

  node_details->tree_item = TreeView_InsertItem(tree_view_, &insert_struct);
}

void TreeView::RecursivelyDelete(NodeDetails* node) {
  DCHECK(node);
  HTREEITEM item = node->tree_item;
  DCHECK(item);

  // Recurse through children.
  for (HTREEITEM child = TreeView_GetChild(tree_view_, item);
       child ; child = TreeView_GetNextSibling(tree_view_, child)) {
    RecursivelyDelete(GetNodeDetailsByTreeItem(child));
  }

  TreeView_DeleteItem(tree_view_, item);

  // finally, it is safe to delete the data for this node.
  id_to_details_map_.erase(node->id);
  node_to_details_map_.erase(node->node);
  delete node;
}

TreeView::NodeDetails* TreeView::GetNodeDetailsByTreeItem(HTREEITEM tree_item) {
  DCHECK(tree_view_ && tree_item);
  TV_ITEM tv_item = {0};
  tv_item.hItem = tree_item;
  tv_item.mask = TVIF_PARAM;
  if (TreeView_GetItem(tree_view_, &tv_item))
    return GetNodeDetailsByID(static_cast<int>(tv_item.lParam));
  return NULL;
}

LRESULT CALLBACK TreeView::TreeWndProc(HWND window,
                                       UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  TreeViewWrapper* wrapper = reinterpret_cast<TreeViewWrapper*>(
      GetWindowLongPtr(window, GWLP_USERDATA));
  DCHECK(wrapper);
  TreeView* tree = wrapper->tree_view;
  if (message == WM_RBUTTONDOWN && tree->select_on_right_mouse_down_) {
    TVHITTESTINFO hit_info;
    hit_info.pt.x = GET_X_LPARAM(l_param);
    hit_info.pt.y = GET_Y_LPARAM(l_param);
    HTREEITEM hit_item = TreeView_HitTest(window, &hit_info);
    if (hit_item && (hit_info.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT |
                                       TVHT_ONITEMINDENT)) != 0)
      TreeView_SelectItem(tree->tree_view_, hit_item);
    // Fall through and let the default handler process as well.
  }
  WNDPROC handler = tree->original_handler_;
  DCHECK(handler);
  return CallWindowProc(handler, window, message, w_param, l_param);
}

}  // namespace views

