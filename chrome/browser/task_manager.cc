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

#include "chrome/browser/task_manager.h"

#include "base/process_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/standard_layout.h"
#include "chrome/browser/task_manager_resource_providers.h"
#include "chrome/browser/tab_util.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/accelerator.h"
#include "chrome/views/background.h"
#include "chrome/views/link.h"
#include "chrome/views/native_button.h"
#include "chrome/views/window.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

#include "generated_resources.h"

// The task manager window default size.
static const int kDefaultWidth = 460;
static const int kDefaultHeight = 270;

// The delay between updates of the information (in ms).
static const int kUpdateTimeMs = 1000;

// An id for the most important column, made sufficiently large so as not to
// collide with anything else.
static const int64 kNuthMagicNumber = 1737350766;
static const int kBitMask = 0x7FFFFFFF;
static const int kGoatsTeleportedColumn =
    (94024 * kNuthMagicNumber) & kBitMask;

////////////////////////////////////////////////////////////////////////////////
// TaskManagerUpdateTask class.
//
// Used to periodically updates the task manager contents.
//
////////////////////////////////////////////////////////////////////////////////

class TaskManagerUpdateTask : public Task {
 public:
  explicit TaskManagerUpdateTask(TaskManagerTableModel* model) : model_(model) {
  }
  void Run() {
    if (model_) model_->Refresh();
  }

 private:
  TaskManagerTableModel* model_;
  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerUpdateTask);
};

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel class
////////////////////////////////////////////////////////////////////////////////

// static
int TaskManagerTableModel::goats_teleported_ = 0;

TaskManagerTableModel::TaskManagerTableModel(TaskManager* task_manager)
    : observer_(NULL),
      timer_(NULL),
      ui_loop_(MessageLoop::current()),
      is_updating_(false) {

  TaskManagerBrowserProcessResourceProvider* browser_provider =
      new TaskManagerBrowserProcessResourceProvider(task_manager);
  browser_provider->AddRef();
  providers_.push_back(browser_provider);
  TaskManagerWebContentsResourceProvider* wc_provider =
      new TaskManagerWebContentsResourceProvider(task_manager);
  wc_provider->AddRef();
  providers_.push_back(wc_provider);
  TaskManagerPluginProcessResourceProvider* plugin_provider =
      new TaskManagerPluginProcessResourceProvider(task_manager);
  plugin_provider->AddRef();
  providers_.push_back(plugin_provider);
  update_task_.reset(new TaskManagerUpdateTask(this));
}

TaskManagerTableModel::~TaskManagerTableModel() {
  DCHECK(timer_ == NULL);
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->Release();
  }
}

int TaskManagerTableModel::RowCount() {
  return static_cast<int>(resources_.size());
}

std::wstring TaskManagerTableModel::GetText(int row, int col_id) {
  // Let's find out if we are the first item in our group.
  TaskManager::Resource* resource = resources_[row];
  ResourceList* group = group_map_[resource->GetProcess()];
  DCHECK(group && !group->empty());
  bool first_in_group = ((*group)[0] == resource);
  process_util::ProcessMetrics* process_metrics = NULL;
  if (first_in_group) {
    MetricsMap::iterator iter = metrics_map_.find(resource->GetProcess());
    DCHECK(iter != metrics_map_.end());
    process_metrics = iter->second;
  }

  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return resource->GetTitle();

    // Only the first item from a group shows the process info.
    case IDS_TASK_MANAGER_NET_COLUMN: {  // Net
      int64 net_usage = GetNetworkUsageForResource(resources_[row]);
      if (net_usage == 0 && !resource->SupportNetworkUsage()) {
        return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
      } else {
        if (net_usage == 0)
          return std::wstring(L"0");
        return FormatSpeed(net_usage, GetByteDisplayUnits(net_usage), true);
      }
    }

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (first_in_group)
        return IntToWString(process_metrics->GetCPUUsage());
      return std::wstring();

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      // We report committed (working set + paged) private usage. This is NOT
      // going to match what Windows Task Manager shows (which is working set).
      if (first_in_group) {
        size_t private_kbytes = process_metrics->GetPrivateBytes() / 1024;
        return l10n_util::GetStringF(IDS_TASK_MANAGER_MEM_CELL_TEXT,
                                     FormatNumber(private_kbytes));
      }
      return std::wstring();

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (first_in_group) {
        process_util::WorkingSetKBytes ws_usage;
        process_metrics->GetWorkingSetKBytes(&ws_usage);
        size_t shared_kbytes = ws_usage.shared;
        return l10n_util::GetStringF(IDS_TASK_MANAGER_MEM_CELL_TEXT,
                                     FormatNumber(shared_kbytes));
      }
      return std::wstring();

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      // Memory = working_set.private + working_set.shareable.
      // We exclude the shared memory.
      if (first_in_group) {
        size_t total_kbytes = process_metrics->GetWorkingSetSize() / 1024;
        process_util::WorkingSetKBytes ws_usage;
        process_metrics->GetWorkingSetKBytes(&ws_usage);
        total_kbytes -= ws_usage.shared;
        return l10n_util::GetStringF(IDS_TASK_MANAGER_MEM_CELL_TEXT,
                                     FormatNumber(total_kbytes));
      }
      return std::wstring();

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      if (first_in_group)
        return IntToWString(process_util::GetProcId(resource->GetProcess()));
      return std::wstring();
    case kGoatsTeleportedColumn:  // Goats Teleported.
      goats_teleported_ += rand();
      return FormatNumber(goats_teleported_);

    default:
      StatsTable* table = StatsTable::current();
      if (table != NULL) {
        const wchar_t* counter = table->GetRowName(col_id);
        if (counter != NULL && counter[0] != '\0') {
          int val = table->GetCounterValue(counter,
              process_util::GetProcId(resource->GetProcess()));
          return IntToWString(val);
        } else {
          NOTREACHED() << "Invalid column.";
        }
      }
      return std::wstring(L"0");
  }
}

SkBitmap TaskManagerTableModel::GetIcon(int row) {
  DCHECK(row < RowCount());
  return resources_[row]->GetIcon();
}

void TaskManagerTableModel::GetGroupRangeForItem(int item,
    ChromeViews::GroupRange* range) {
  DCHECK((item >= 0) && (item < RowCount())) <<
      " invalid item "<< item << " (items count=" << RowCount() << ")";

  TaskManager::Resource* resource = resources_[item];
  GroupMap::iterator group_iter = group_map_.find(resource->GetProcess());
  DCHECK(group_iter != group_map_.end());
  ResourceList* group = group_iter->second;
  DCHECK(group);
  if (group->size() == 1) {
    range->start = item;
    range->length = 1;
  } else {
    ResourceList::iterator iter =
        std::find(resources_.begin(), resources_.end(), (*group)[0]);
    DCHECK(iter != resources_.end());
    range->start = static_cast<int>(iter - resources_.begin());
    range->length = static_cast<int>(group->size());
  }
}

HANDLE TaskManagerTableModel::GetProcessAt(int index) {
  DCHECK(index < RowCount());
  return resources_[index]->GetProcess();
}

void TaskManagerTableModel::StartUpdating() {
  DCHECK(!is_updating_);
  is_updating_ = true;
  DCHECK(timer_ == NULL);
  TimerManager* tm = MessageLoop::current()->timer_manager();
  timer_ = tm->StartTimer(kUpdateTimeMs, update_task_.get(), true);

  // Register jobs notifications so we can compute network usage (it must be
  // done from the IO thread).
  Thread* thread = g_browser_process->io_thread();
  if (thread)
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskManagerTableModel::RegisterForJobDoneNotifications));

  // Notify resource providers that we are updating.
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StartUpdating();
  }
}

void TaskManagerTableModel::StopUpdating() {
  DCHECK(is_updating_);
  is_updating_ = false;
  MessageLoop::current()->timer_manager()->StopTimer(timer_);
  delete timer_;
  timer_ = NULL;

  // Notify resource providers that we are done updating.
  for (ResourceProviderList::const_iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StopUpdating();
  }

  // Unregister jobs notification (must be done from the IO thread).
  Thread* thread = g_browser_process->io_thread();
  if (thread)
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskManagerTableModel::UnregisterForJobDoneNotifications));
}

void TaskManagerTableModel::AddResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  providers_.push_back(provider);
}

void TaskManagerTableModel::RemoveResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  ResourceProviderList::iterator iter = std::find(providers_.begin(),
                                                  providers_.end(),
                                                  provider);
  DCHECK(iter != providers_.end());
  providers_.erase(iter);
}

void TaskManagerTableModel::RegisterForJobDoneNotifications() {
  g_url_request_job_tracker.AddObserver(this);
}

void TaskManagerTableModel::UnregisterForJobDoneNotifications() {
  g_url_request_job_tracker.RemoveObserver(this);
}

void TaskManagerTableModel::AddResource(TaskManager::Resource* resource) {
  HANDLE process = resource->GetProcess();

  ResourceList* group_entries = NULL;
  GroupMap::const_iterator group_iter = group_map_.find(process);
  int new_entry_index = 0;
  if (group_iter == group_map_.end()) {
    group_entries = new ResourceList();
    group_map_[process] = group_entries;
    group_entries->push_back(resource);

    // Not part of a group, just put at the end of the list.
    resources_.push_back(resource);
    new_entry_index = static_cast<int>(resources_.size() - 1);
  } else {
    group_entries = group_iter->second;
    group_entries->push_back(resource);

    // Insert the new entry right after the last entry of its group.
    ResourceList::iterator iter =
        std::find(resources_.begin(),
                  resources_.end(),
                  (*group_entries)[group_entries->size() - 2]);
    DCHECK(iter != resources_.end());
    new_entry_index = static_cast<int>(iter - resources_.begin());
    resources_.insert(++iter, resource);
  }
  process_util::ProcessMetrics* pm =
      process_util::ProcessMetrics::CreateProcessMetrics(process);
  metrics_map_[process] = pm;

  // Notify the table that the contents have changed for it to redraw.
  DCHECK(observer_);
  observer_->OnItemsAdded(new_entry_index, 1);
}

void TaskManagerTableModel::RemoveResource(TaskManager::Resource* resource) {
  HANDLE process = resource->GetProcess();

  // Find the associated group.
  GroupMap::iterator group_iter = group_map_.find(process);
  DCHECK(group_iter != group_map_.end());
  ResourceList* group_entries = group_iter->second;

  // Remove the entry from the group map.
  ResourceList::iterator iter = std::find(group_entries->begin(),
                                          group_entries->end(),
                                          resource);
  DCHECK(iter != group_entries->end());
  group_entries->erase(iter);

  // If there are no more entries for that process, do the clean-up.
  if (group_entries->empty()) {
    delete group_entries;
    group_map_.erase(process);

    // Nobody is using this process, we don't need the process metrics anymore.
    MetricsMap::iterator pm_iter = metrics_map_.find(process);
    DCHECK(pm_iter != metrics_map_.end());
    if (pm_iter != metrics_map_.end()) {
      delete pm_iter->second;
      metrics_map_.erase(process);
    }
  }

  // Remove the entry from the model list.
  iter = std::find(resources_.begin(), resources_.end(), resource);
  DCHECK(iter != resources_.end());
  int index = static_cast<int>(iter - resources_.begin());
  resources_.erase(iter);

  // Remove the entry from the network maps.
  ResourceValueMap::iterator net_iter =
      current_byte_count_map_.find(resource);
  if (net_iter != current_byte_count_map_.end())
    current_byte_count_map_.erase(net_iter);
  net_iter = displayed_network_usage_map_.find(resource);
  if (net_iter != displayed_network_usage_map_.end())
    displayed_network_usage_map_.erase(net_iter);

  // Notify the table that the contents have changed.
  observer_->OnItemsRemoved(index, 1);
}

void TaskManagerTableModel::Clear() {
  int size = RowCount();
  if (size > 0) {
    resources_.clear();

    // Clear the groups.
    for (GroupMap::iterator iter = group_map_.begin();
         iter != group_map_.end(); ++iter) {
      delete iter->second;
    }
    group_map_.clear();

    // Clear the process metrics.
    for (MetricsMap::iterator iter = metrics_map_.begin();
         iter != metrics_map_.end(); ++iter) {
      delete iter->second;
    }
    metrics_map_.clear();

    // Clear the network maps.
    current_byte_count_map_.clear();
    displayed_network_usage_map_.clear();

    observer_->OnItemsRemoved(0, size);
  }
}

// Called by the timer when we need to refresh the row contents.
void TaskManagerTableModel::Refresh() {
  // Compute the new network usage values.
  displayed_network_usage_map_.clear();
  for (ResourceValueMap::iterator iter = current_byte_count_map_.begin();
       iter != current_byte_count_map_.end();
       ++iter) {
    if (kUpdateTimeMs > 1000) {
      int divider = (kUpdateTimeMs / 1000);
      displayed_network_usage_map_[iter->first] = iter->second / divider;
    } else {
      displayed_network_usage_map_[iter->first] = iter->second *
          (1000 / kUpdateTimeMs);
    }

    // Then we reset the current byte count.
    iter->second = 0;
  }
  if (resources_.size() > 0)
    observer_->OnItemsChanged(0, RowCount());
}

void TaskManagerTableModel::SetObserver(
    ChromeViews::TableModelObserver* observer) {
  observer_ = observer;
}

int64 TaskManagerTableModel::GetNetworkUsageForResource(
    TaskManager::Resource* resource) {
  ResourceValueMap::iterator iter = displayed_network_usage_map_.find(resource);
  if (iter == displayed_network_usage_map_.end())
    return 0;
  return iter->second;
}

void TaskManagerTableModel::BytesRead(BytesReadParam param) {
  if (!is_updating_) {
    // A notification sneaked in while we were stopping the updating, just
    // ignore it.
    return;
  }

  if (param.byte_count == 0) {
    // Nothing to do if no bytes were actually read.
    return;
  }

  // TODO(jcampan): this should be improved once we have a better way of
  // linking a network notification back to the object that initiated it.
  TaskManager::Resource* resource;
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); iter++) {
    resource = (*iter)->GetResource(param.origin_pid,
                                    param.render_process_host_id,
                                    param.routing_id);
    if (resource)
      break;
  }
  if (resource == NULL) {
    // We may not have that resource anymore (example: close a tab while a
    // a network resource is being retrieved), in which case we just ignore the
    // notification.
    return;
  }

  // We do support network usage, mark the resource as such so it can report 0
  // instead of N/A.
  if (!resource->SupportNetworkUsage())
    resource->SetSupportNetworkUsage();

  ResourceValueMap::const_iterator iter_res =
      current_byte_count_map_.find(resource);
  if (iter_res == current_byte_count_map_.end())
    current_byte_count_map_[resource] = param.byte_count;
  else
    current_byte_count_map_[resource] = iter_res->second + param.byte_count;
}


// In order to retrieve the network usage, we register for URLRequestJob
// notifications. Every time we get notified some bytes were read we bump a
// counter of read bytes for the associated resource. When the timer ticks,
// we'll compute the actual network usage (see the Refresh method).
void TaskManagerTableModel::OnJobAdded(URLRequestJob* job) {
}

void TaskManagerTableModel::OnJobRemoved(URLRequestJob* job) {
}

void TaskManagerTableModel::OnJobDone(URLRequestJob* job,
                                      const URLRequestStatus& status) {
}

void TaskManagerTableModel::OnJobRedirect(URLRequestJob* job,
                                          const GURL& location,
                                          int status_code) {
}

void TaskManagerTableModel::OnBytesRead(URLRequestJob* job, int byte_count) {
  int render_process_host_id, routing_id;
  if (tab_util::GetTabContentsID(job->request(),
                                 &render_process_host_id, &routing_id)) {
    // This happens in the IO thread, post it to the UI thread.
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskManagerTableModel::BytesRead,
        BytesReadParam(job->request()->origin_pid(),
                       render_process_host_id, routing_id,
                       byte_count)));
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerContents class
//
// The view containing the different widgets.
//
////////////////////////////////////////////////////////////////////////////////

class TaskManagerContents : public ChromeViews::View,
                            public ChromeViews::NativeButton::Listener,
                            public ChromeViews::TableViewObserver,
                            public ChromeViews::LinkController,
                            public ChromeViews::ContextMenuController,
                            public Menu::Delegate {
 public:
  TaskManagerContents(TaskManager* task_manager,
                      TaskManagerTableModel* table_model);
  virtual ~TaskManagerContents();

  void Init(TaskManagerTableModel* table_model);
  virtual void Layout();
  virtual void GetPreferredSize(CSize* out);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void ViewHierarchyChanged(bool is_add, ChromeViews::View* parent,
                                    ChromeViews::View* child);
  void GetSelection(std::vector<int>* selection);

  // NativeButton::Listener implementation.
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // ChromeViews::TableViewObserver implementation.
  virtual void OnSelectionChanged();

  // ChromeViews::LinkController implementation.
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // Called by the column picker to pick up any new stat counters that
  // may have appeared since last time.
  void UpdateStatsCounters();

  // Menu::Delegate
  virtual void ShowContextMenu(ChromeViews::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  scoped_ptr<ChromeViews::NativeButton> kill_button_;
  scoped_ptr<ChromeViews::Link> about_memory_link_;
  ChromeViews::GroupTableView* tab_table_;

  TaskManager* task_manager_;

  // all possible columns, not necessarily visible
  std::vector<ChromeViews::TableColumn> columns_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerContents);
};

TaskManagerContents::TaskManagerContents(TaskManager* task_manager,
                                         TaskManagerTableModel* table_model)
    : task_manager_(task_manager) {
  Init(table_model);
}

TaskManagerContents::~TaskManagerContents() {
}

void TaskManagerContents::Init(TaskManagerTableModel* table_model) {
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_PAGE_COLUMN,
          ChromeViews::TableColumn::LEFT, -1, 1));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_CPU_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_NET_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));
  columns_.push_back(
      ChromeViews::TableColumn(
          IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
          ChromeViews::TableColumn::RIGHT, -1, 0));

  tab_table_ = new ChromeViews::GroupTableView(table_model,
                                               columns_,
                                               ChromeViews::ICON_AND_TEXT,
                                               false, true, true);

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROCESS_ID_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);

  UpdateStatsCounters();
  ChromeViews::TableColumn col(kGoatsTeleportedColumn, L"Goats Teleported",
                               ChromeViews::TableColumn::RIGHT, -1, 0);
  columns_.push_back(col);
  tab_table_->AddColumn(col);
  tab_table_->SetObserver(this);
  SetContextMenuController(this);
  kill_button_.reset(new ChromeViews::NativeButton(
      l10n_util::GetString(IDS_TASK_MANAGER_KILL)));
  kill_button_->SetListener(this);
  about_memory_link_.reset(new ChromeViews::Link(
      l10n_util::GetString(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK)));
  about_memory_link_->SetController(this);

  AddChildView(tab_table_);

  // Makes sure our state is consistent.
  OnSelectionChanged();
}

void TaskManagerContents::UpdateStatsCounters() {
  StatsTable* stats = StatsTable::current();
  if (stats != NULL) {
    int max = stats->GetMaxCounters();
    // skip the first row (it's header data)
    for (int i = 1; i < max; i++) {
      const wchar_t* row = stats->GetRowName(i);
      if (row != NULL && row[0] != L'\0' && !tab_table_->HasColumn(i)) {
        // TODO(erikkay): Use l10n to get display names for stats.  Right
        // now we're just displaying the internal counter name.  Perhaps
        // stat names not in the string table would be filtered out.
        // TODO(erikkay): Width is hard-coded right now, so many column
        // names are clipped.
        ChromeViews::TableColumn col(i, row, ChromeViews::TableColumn::RIGHT,
                                     90, 0);
        columns_.push_back(col);
        tab_table_->AddColumn(col);
      }
    }
  }
}

void TaskManagerContents::DidChangeBounds(const CRect& previous,
                                          const CRect& current) {
  Layout();
}

void TaskManagerContents::ViewHierarchyChanged(bool is_add,
                                               ChromeViews::View* parent,
                                               ChromeViews::View* child) {
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(kill_button_.get());
      parent->AddChildView(about_memory_link_.get());
    } else {
      parent->RemoveChildView(kill_button_.get());
      parent->RemoveChildView(about_memory_link_.get());
      // Note that these items aren't deleted here, since this object is owned
      // by the TaskManager, whose lifetime surpasses the window, and the next
      // time we are inserted into a window these items will need to be valid.
    }
  }
}

void TaskManagerContents::Layout() {
  // kPanelHorizMargin is too big.
  const int kTableButtonSpacing = 12;
  CRect bounds;
  GetLocalBounds(&bounds, true);
  int x = bounds.left;
  int y = bounds.top;

  CSize size;
  kill_button_->GetPreferredSize(&size);
  int prefered_width = size.cx;
  int prefered_height = size.cy;

  tab_table_->SetBounds(
      x + kPanelHorizMargin,
      y + kPanelVertMargin,
      bounds.Width() - 2 * kPanelHorizMargin,
      bounds.Height() - 2 * kPanelVertMargin - prefered_height);

  // y-coordinate of button top left.
  CRect parent_bounds;
  GetParent()->GetLocalBounds(&parent_bounds, false);
  int y_buttons = parent_bounds.bottom - prefered_height - kButtonVEdgeMargin;

  kill_button_->SetBounds(
      x + bounds.Width() - prefered_width - kPanelHorizMargin,
      y_buttons,
      prefered_width,
      prefered_height);

  about_memory_link_->GetPreferredSize(&size);
  int link_prefered_width = size.cx;
  int link_prefered_height = size.cy;
  // center between the two buttons horizontally, and line up with
  // bottom of buttons vertically.
  int link_y_offset = std::max(0, prefered_height - link_prefered_height) / 2;
  about_memory_link_->SetBounds(
      x + kPanelHorizMargin,
      y_buttons + prefered_height - link_prefered_height - link_y_offset,
      link_prefered_width,
      link_prefered_height);
}

void TaskManagerContents::GetPreferredSize(CSize* out) {
  out->cx = kDefaultWidth;
  out->cy = kDefaultHeight;
}

void TaskManagerContents::GetSelection(std::vector<int>* selection) {
  DCHECK(selection);
  for (ChromeViews::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    // The TableView returns the selection starting from the end.
    selection->insert(selection->begin(), *iter);
  }
}

// NativeButton::Listener implementation.
void TaskManagerContents::ButtonPressed(ChromeViews::NativeButton* sender) {
  if (sender == kill_button_)
    task_manager_->KillSelectedProcesses();
}

// ChromeViews::TableViewObserver implementation.
void TaskManagerContents::OnSelectionChanged() {
  kill_button_->SetEnabled(!task_manager_->BrowserProcessIsSelected() &&
                           tab_table_->SelectedRowCount() > 0);
}

// ChromeViews::LinkController implementation
void TaskManagerContents::LinkActivated(ChromeViews::Link* source,
                                        int event_flags) {
  DCHECK(source == about_memory_link_);
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenURL(GURL("about:memory"), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
}

void TaskManagerContents::ShowContextMenu(ChromeViews::View* source,
                                          int x,
                                          int y,
                                          bool is_mouse_gesture) {
  UpdateStatsCounters();
  Menu menu(this, Menu::TOPLEFT, source->GetViewContainer()->GetHWND());
  for (std::vector<ChromeViews::TableColumn>::iterator i =
       columns_.begin(); i != columns_.end(); ++i) {
    menu.AppendMenuItem(i->id, i->title, Menu::CHECKBOX);
  }
  menu.RunMenuAt(x, y);
}

bool TaskManagerContents::IsItemChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

void TaskManagerContents::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

////////////////////////////////////////////////////////////////////////////////
// TaskManager class
////////////////////////////////////////////////////////////////////////////////

// static
void TaskManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
}

TaskManager::TaskManager() : window_(NULL) {
  table_model_ = new TaskManagerTableModel(this);
  contents_.reset(new TaskManagerContents(this, table_model_));
}

TaskManager::~TaskManager() {
}

// static
void TaskManager::Open() {
  TaskManager* task_manager = GetInstance();
  if (task_manager->window_) {
    task_manager->window_->MoveToFront(true);
  } else {
    task_manager->window_ =
        ChromeViews::Window::CreateChromeWindow(
            NULL, gfx::Rect(), task_manager->contents_.get(), task_manager);
    task_manager->table_model_->StartUpdating();
    task_manager->window_->Show();
  }
}

void TaskManager::Close() {
  window_ = NULL;
  table_model_->StopUpdating();
  table_model_->Clear();
}

bool TaskManager::BrowserProcessIsSelected() {
  if (!contents_.get())
    return false;
  std::vector<int> selection;
  contents_->GetSelection(&selection);
  for (std::vector<int>::const_iterator iter = selection.begin();
       iter != selection.end(); ++iter) {
    // If some of the selection is out of bounds, ignore. This may happen when
    // killing a process that manages several pages.
    if (*iter >= table_model_->RowCount())
      continue;
    if (table_model_->GetProcessAt(*iter) == GetCurrentProcess())
      return true;
  }
  return false;
}

void TaskManager::KillSelectedProcesses() {
  std::vector<int> selection;
  contents_->GetSelection(&selection);
  for (std::vector<int>::const_iterator iter = selection.begin();
       iter != selection.end(); ++iter) {
    HANDLE process = table_model_->GetProcessAt(*iter);
    DCHECK(process);
    TerminateProcess(process, 0);
  }
}

void TaskManager::AddResourceProvider(ResourceProvider* provider) {
  table_model_->AddResourceProvider(provider);
}

void TaskManager::RemoveResourceProvider(ResourceProvider* provider) {
  table_model_->RemoveResourceProvider(provider);
}

void TaskManager::AddResource(Resource* resource) {
  table_model_->AddResource(resource);
}

void TaskManager::RemoveResource(Resource* resource) {
  table_model_->RemoveResource(resource);
}

// DialogDelegate implementation:
bool TaskManager::CanResize() const {
  return true;
}

bool TaskManager::CanMaximize() const {
  return true;
}

bool TaskManager::ShouldShowWindowIcon() const {
  return false;
}

bool TaskManager::IsAlwaysOnTop() const {
  return true;
}

bool TaskManager::HasAlwaysOnTopMenu() const {
  return true;
};

std::wstring TaskManager::GetWindowTitle() const {
  return l10n_util::GetString(IDS_TASK_MANAGER_TITLE);
}

void TaskManager::SaveWindowPosition(const CRect& bounds,
                                     bool maximized,
                                     bool always_on_top) {
  window_->SaveWindowPositionToPrefService(g_browser_process->local_state(),
                                           prefs::kTaskManagerWindowPlacement,
                                           bounds, maximized, always_on_top);
}

bool TaskManager::RestoreWindowPosition(CRect* bounds,
                                        bool* maximized,
                                        bool* always_on_top) {
  return window_->RestoreWindowPositionFromPrefService(
      g_browser_process->local_state(),
      prefs::kTaskManagerWindowPlacement,
      bounds, maximized, always_on_top);
}

int TaskManager::GetDialogButtons() const {
  return DIALOGBUTTON_NONE;
}

void TaskManager::WindowClosing() {
  // Remove the view from its parent to trigger the contents'
  // ViewHierarchyChanged notification to unhook the extra buttons from the
  // non-client view.
  contents_->GetParent()->RemoveChildView(contents_.get());
  Close();
}

// static
TaskManager* TaskManager::GetInstance() {
  return Singleton<TaskManager>::get();
}
