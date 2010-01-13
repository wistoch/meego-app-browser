// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/i18n/number_formatting.h"
#include "base/process_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_request_tracking.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/task_manager_resource_providers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mach_broker_mac.h"
#endif

namespace {

// The delay between updates of the information (in ms).
#if defined(OS_MACOSX)
// Match Activity Monitor's default refresh rate.
const int kUpdateTimeMs = 2000;
#else
const int kUpdateTimeMs = 1000;
#endif

template <class T>
int ValueCompare(T value1, T value2) {
  if (value1 < value2)
    return -1;
  if (value1 == value2)
    return 0;
  return 1;
}

std::wstring FormatStatsSize(const WebKit::WebCache::ResourceTypeStat& stat) {
  return l10n_util::GetStringF(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      FormatBytes(stat.size, DATA_UNITS_KIBIBYTE, false),
      FormatBytes(stat.liveSize, DATA_UNITS_KIBIBYTE, false));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TaskManagerModel class
////////////////////////////////////////////////////////////////////////////////

TaskManagerModel::TaskManagerModel(TaskManager* task_manager)
    : update_state_(IDLE) {

  TaskManagerBrowserProcessResourceProvider* browser_provider =
      new TaskManagerBrowserProcessResourceProvider(task_manager);
  browser_provider->AddRef();
  providers_.push_back(browser_provider);
  TaskManagerTabContentsResourceProvider* wc_provider =
      new TaskManagerTabContentsResourceProvider(task_manager);
  wc_provider->AddRef();
  providers_.push_back(wc_provider);
  TaskManagerChildProcessResourceProvider* child_process_provider =
      new TaskManagerChildProcessResourceProvider(task_manager);
  child_process_provider->AddRef();
  providers_.push_back(child_process_provider);
  TaskManagerExtensionProcessResourceProvider* extension_process_provider =
      new TaskManagerExtensionProcessResourceProvider(task_manager);
  extension_process_provider->AddRef();
  providers_.push_back(extension_process_provider);
}

TaskManagerModel::~TaskManagerModel() {
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->Release();
  }
}

int TaskManagerModel::ResourceCount() const {
  return resources_.size();
}

void TaskManagerModel::AddObserver(TaskManagerModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TaskManagerModel::RemoveObserver(TaskManagerModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::wstring TaskManagerModel::GetResourceTitle(int index) const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetTitle();
}

std::wstring TaskManagerModel::GetResourceNetworkUsage(int index) const {
  DCHECK(index < ResourceCount());
  int64 net_usage = GetNetworkUsage(resources_[index]);
  if (net_usage == -1)
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  if (net_usage == 0)
    return std::wstring(L"0");
  std::wstring net_byte =
      FormatSpeed(net_usage, GetByteDisplayUnits(net_usage), true);
  // Force number string to have LTR directionality.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&net_byte);
  return net_byte;
}

std::wstring TaskManagerModel::GetResourceCPUUsage(int index) const {
  DCHECK(index < ResourceCount());
  return StringPrintf(
#if defined(OS_MACOSX)
      // Activity Monitor shows %cpu with one decimal digit -- be
      // consistent with that.
      L"%.1f",
#else
      L"%.0f",
#endif
      GetCPUUsage(resources_[index]));
}

std::wstring TaskManagerModel::GetResourcePrivateMemory(int index) const {
  size_t private_mem;
  if (!GetPrivateMemory(index, &private_mem))
    return L"N/A";
  return GetMemCellText(private_mem);
}

std::wstring TaskManagerModel::GetResourceSharedMemory(int index) const {
  size_t shared_mem;
  if (!GetSharedMemory(index, &shared_mem))
      return L"N/A";
  return GetMemCellText(shared_mem);
}

std::wstring TaskManagerModel::GetResourcePhysicalMemory(int index) const {
  size_t phys_mem;
  GetPhysicalMemory(index, &phys_mem);
  return GetMemCellText(phys_mem);
}

std::wstring TaskManagerModel::GetResourceProcessId(int index) const {
  DCHECK(index < ResourceCount());
  return IntToWString(base::GetProcId(resources_[index]->GetProcess()));
}

std::wstring TaskManagerModel::GetResourceStatsValue(int index, int col_id)
    const {
  DCHECK(index < ResourceCount());
  return IntToWString(GetStatsValue(resources_[index], col_id));
}

std::wstring TaskManagerModel::GetResourceGoatsTeleported(int index) const {
  // See design doc at http://go/at-teleporter for more information.
  DCHECK(index < ResourceCount());
  int goats_teleported = rand() & 15;
  return UTF16ToWide(base::FormatNumber(goats_teleported));
}

std::wstring TaskManagerModel::GetResourceWebCoreImageCacheSize(
    int index) const {
  DCHECK(index < ResourceCount());
  if (!resources_[index]->ReportsCacheStats())
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  const WebKit::WebCache::ResourceTypeStats stats(
      resources_[index]->GetWebCoreCacheStats());
  return FormatStatsSize(stats.images);
}

std::wstring TaskManagerModel::GetResourceWebCoreScriptsCacheSize(
    int index) const {
  DCHECK(index < ResourceCount());
  if (!resources_[index]->ReportsCacheStats())
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  const WebKit::WebCache::ResourceTypeStats stats(
      resources_[index]->GetWebCoreCacheStats());
  return FormatStatsSize(stats.scripts);
}

std::wstring TaskManagerModel::GetResourceWebCoreCSSCacheSize(
    int index) const {
  DCHECK(index < ResourceCount());
  if (!resources_[index]->ReportsCacheStats())
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  const WebKit::WebCache::ResourceTypeStats stats(
      resources_[index]->GetWebCoreCacheStats());
  return FormatStatsSize(stats.cssStyleSheets);
}

std::wstring TaskManagerModel::GetResourceSqliteMemoryUsed(int index) const {
  DCHECK(index < ResourceCount());
  if (!resources_[index]->ReportsSqliteMemoryUsed())
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return GetMemCellText(resources_[index]->SqliteMemoryUsedBytes());
}

std::wstring TaskManagerModel::GetResourceV8MemoryAllocatedSize(
    int index) const {
  if (!resources_[index]->ReportsV8MemoryStats())
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return l10n_util::GetStringF(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      FormatBytes(resources_[index]->GetV8MemoryAllocated(),
                  DATA_UNITS_KIBIBYTE,
                  false),
      FormatBytes(resources_[index]->GetV8MemoryUsed(),
                  DATA_UNITS_KIBIBYTE,
                  false));
}

bool TaskManagerModel::IsResourceFirstInGroup(int index) const {
  DCHECK(index < ResourceCount());
  TaskManager::Resource* resource = resources_[index];
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList* group = iter->second;
  return ((*group)[0] == resource);
}

SkBitmap TaskManagerModel::GetResourceIcon(int index) const {
  DCHECK(index < ResourceCount());
  SkBitmap icon = resources_[index]->GetIcon();
  if (!icon.isNull())
    return icon;

  static SkBitmap* default_icon = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_DEFAULT_FAVICON);
  return *default_icon;
}

std::pair<int, int> TaskManagerModel::GetGroupRangeForResource(int index)
    const {
  DCHECK(index < ResourceCount());
  TaskManager::Resource* resource = resources_[index];
  GroupMap::const_iterator group_iter =
      group_map_.find(resource->GetProcess());
  DCHECK(group_iter != group_map_.end());
  ResourceList* group = group_iter->second;
  DCHECK(group);
  if (group->size() == 1) {
    return std::make_pair(index, 1);
  } else {
    ResourceList::const_iterator iter =
        std::find(resources_.begin(), resources_.end(), (*group)[0]);
    DCHECK(iter != resources_.end());
    return std::make_pair(iter - resources_.begin(), group->size());
  }
}

int TaskManagerModel::CompareValues(int row1, int row2, int col_id) const {
  DCHECK(row1 < ResourceCount() && row2 < ResourceCount());
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN: {
      // Let's do the default, string compare on the resource title.
      static icu::Collator* collator = NULL;
      if (!collator) {
        UErrorCode create_status = U_ZERO_ERROR;
        collator = icu::Collator::createInstance(create_status);
        if (!U_SUCCESS(create_status)) {
          collator = NULL;
          NOTREACHED();
        }
      }
      string16 title1 = WideToUTF16(GetResourceTitle(row1));
      string16 title2 = WideToUTF16(GetResourceTitle(row2));
      UErrorCode compare_status = U_ZERO_ERROR;
      UCollationResult compare_result = collator->compare(
          static_cast<const UChar*>(title1.c_str()),
          static_cast<int>(title1.length()),
          static_cast<const UChar*>(title2.c_str()),
          static_cast<int>(title2.length()),
          compare_status);
      DCHECK(U_SUCCESS(compare_status));
      return compare_result;
    }

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare<int64>(GetNetworkUsage(resources_[row1]),
                                 GetNetworkUsage(resources_[row2]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare<double>(GetCPUUsage(resources_[row1]),
                                  GetCPUUsage(resources_[row2]));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN: {
      size_t value1;
      size_t value2;
      if (!GetPrivateMemory(row1, &value1) || !GetPrivateMemory(row2, &value2))
        return 0;
      return ValueCompare<size_t>(value1, value2);
    }

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN: {
      size_t value1;
      size_t value2;
      if (!GetSharedMemory(row1, &value1) || !GetSharedMemory(row2, &value2))
        return 0;
      return ValueCompare<size_t>(value1, value2);
    }

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN: {
      size_t value1;
      size_t value2;
      if (!GetPhysicalMemory(row1, &value1) ||
          !GetPhysicalMemory(row2, &value2))
        return 0;
      return ValueCompare<size_t>(value1, value2);
    }

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN: {
      int proc1_id = base::GetProcId(resources_[row1]->GetProcess());
      int proc2_id = base::GetProcId(resources_[row2]->GetProcess());
      return ValueCompare<int>(proc1_id, proc2_id);
    }

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      WebKit::WebCache::ResourceTypeStats stats1 = { { 0 } };
      WebKit::WebCache::ResourceTypeStats stats2 = { { 0 } };
      if (resources_[row1]->ReportsCacheStats())
        stats1 = resources_[row1]->GetWebCoreCacheStats();
      if (resources_[row2]->ReportsCacheStats())
        stats2 = resources_[row2]->GetWebCoreCacheStats();
      if (col_id == IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN)
        return ValueCompare<size_t>(stats1.images.size, stats2.images.size);
      if (col_id == IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN)
        return ValueCompare<size_t>(stats1.scripts.size, stats2.scripts.size);
      return ValueCompare<size_t>(stats1.cssStyleSheets.size,
                                  stats2.cssStyleSheets.size);
    }

    default:
      return ValueCompare<int>(GetStatsValue(resources_[row1], col_id),
                               GetStatsValue(resources_[row2], col_id));
  }
}

base::ProcessHandle TaskManagerModel::GetResourceProcessHandle(int index)
    const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetProcess();
}

TabContents* TaskManagerModel::GetResourceTabContents(int index) const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetTabContents();
}

const Extension* TaskManagerModel::GetResourceExtension(int index) const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetExtension();
}

int64 TaskManagerModel::GetNetworkUsage(TaskManager::Resource* resource)
    const {
  int64 net_usage = GetNetworkUsageForResource(resource);
  if (net_usage == 0 && !resource->SupportNetworkUsage())
    return -1;
  return net_usage;
}

double TaskManagerModel::GetCPUUsage(TaskManager::Resource* resource) const {
  CPUUsageMap::const_iterator iter =
      cpu_usage_map_.find(resource->GetProcess());
  if (iter == cpu_usage_map_.end())
    return 0;
  return iter->second;
}

bool TaskManagerModel::GetPrivateMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessMetrics* process_metrics;
  if (!GetProcessMetricsForRow(index, &process_metrics))
    return false;
  *result = process_metrics->GetPrivateBytes();
  // On Linux (so far) and win XP, this is not supported and returns 0.
  // Remove with crbug.com/23258
  if (*result == 0)
    return false;
  return true;
}

bool TaskManagerModel::GetSharedMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessMetrics* process_metrics;
  if (!GetProcessMetricsForRow(index, &process_metrics))
    return false;
  base::WorkingSetKBytes ws_usage;
  if (!process_metrics->GetWorkingSetKBytes(&ws_usage))
    return false;
  *result = ws_usage.shared * 1024;
  return true;
}

bool TaskManagerModel::GetPhysicalMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessMetrics* process_metrics;
  if (!GetProcessMetricsForRow(index, &process_metrics))
    return false;
  base::WorkingSetKBytes ws_usage;
  if (!process_metrics->GetWorkingSetKBytes(&ws_usage))
    return false;

  // Memory = working_set.private + working_set.shareable.
  // We exclude the shared memory.
  size_t total_bytes = process_metrics->GetWorkingSetSize();
  total_bytes -= ws_usage.shared * 1024;
  *result = total_bytes;
  return true;
}

int TaskManagerModel::GetStatsValue(const TaskManager::Resource* resource,
                                         int col_id) const {
  StatsTable* table = StatsTable::current();
  if (table != NULL) {
    const char* counter = table->GetRowName(col_id);
    if (counter != NULL && counter[0] != '\0') {
      return table->GetCounterValue(counter,
          base::GetProcId(resource->GetProcess()));
     } else {
        NOTREACHED() << "Invalid column.";
     }
  }
  return 0;
}

std::wstring TaskManagerModel::GetMemCellText(int64 number) const {
#if !defined(OS_MACOSX)
  std::wstring str = UTF16ToWide(base::FormatNumber(number / 1024));

  // Adjust number string if necessary.
  l10n_util::AdjustStringForLocaleDirection(str, &str);
  return l10n_util::GetStringF(IDS_TASK_MANAGER_MEM_CELL_TEXT, str);
#else
  // System expectation is to show "100 KB", "200 MB", etc.
  // TODO(thakis): Switch to metric units (as opposed to powers of two).
  return FormatBytes(
      number, GetByteDisplayUnits(number), /* show_units=*/true);
#endif
}

void TaskManagerModel::StartUpdating() {
  DCHECK_NE(TASK_PENDING, update_state_);

  // If update_state_ is STOPPING, it means a task is still pending.  Setting
  // it to TASK_PENDING ensures the tasks keep being posted (by Refresh()).
  if (update_state_ == IDLE) {
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          NewRunnableMethod(this, &TaskManagerModel::Refresh),
          kUpdateTimeMs);
  }
  update_state_ = TASK_PENDING;

  // Register jobs notifications so we can compute network usage (it must be
  // done from the IO thread).
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
         this, &TaskManagerModel::RegisterForJobDoneNotifications));

  // Notify resource providers that we are updating.
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StartUpdating();
  }
}

void TaskManagerModel::StopUpdating() {
  DCHECK_EQ(TASK_PENDING, update_state_);
  update_state_ = STOPPING;

  // Notify resource providers that we are done updating.
  for (ResourceProviderList::const_iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StopUpdating();
  }

  // Unregister jobs notification (must be done from the IO thread).
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &TaskManagerModel::UnregisterForJobDoneNotifications));
}

void TaskManagerModel::AddResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  providers_.push_back(provider);
}

void TaskManagerModel::RemoveResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  ResourceProviderList::iterator iter = std::find(providers_.begin(),
                                                  providers_.end(),
                                                  provider);
  DCHECK(iter != providers_.end());
  providers_.erase(iter);
}

void TaskManagerModel::RegisterForJobDoneNotifications() {
  g_url_request_job_tracker.AddObserver(this);
}

void TaskManagerModel::UnregisterForJobDoneNotifications() {
  g_url_request_job_tracker.RemoveObserver(this);
}

void TaskManagerModel::AddResource(TaskManager::Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

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

  // Create the ProcessMetrics for this process if needed (not in map).
  if (metrics_map_.find(process) == metrics_map_.end()) {
    base::ProcessMetrics* pm =
#if !defined(OS_MACOSX)
        base::ProcessMetrics::CreateProcessMetrics(process);
#else
        base::ProcessMetrics::CreateProcessMetrics(process,
                                                   MachBroker::instance());
#endif

    metrics_map_[process] = pm;
  }

  // Notify the table that the contents have changed for it to redraw.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsAdded(new_entry_index, 1));
}

void TaskManagerModel::RemoveResource(TaskManager::Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

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
    // And we don't need the CPU usage anymore either.
    CPUUsageMap::iterator cpu_iter = cpu_usage_map_.find(process);
    if (cpu_iter != cpu_usage_map_.end())
      cpu_usage_map_.erase(cpu_iter);
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
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsRemoved(index, 1));
}

void TaskManagerModel::Clear() {
  int size = ResourceCount();
  if (size > 0) {
    resources_.clear();

    // Clear the groups.
    for (GroupMap::iterator iter = group_map_.begin();
         iter != group_map_.end(); ++iter) {
      delete iter->second;
    }
    group_map_.clear();

    // Clear the process related info.
    for (MetricsMap::iterator iter = metrics_map_.begin();
         iter != metrics_map_.end(); ++iter) {
      delete iter->second;
    }
    metrics_map_.clear();
    cpu_usage_map_.clear();

    // Clear the network maps.
    current_byte_count_map_.clear();
    displayed_network_usage_map_.clear();

    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsRemoved(0, size));
  }
}

void TaskManagerModel::NotifyResourceTypeStats(
    base::ProcessId renderer_id,
    const WebKit::WebCache::ResourceTypeStats& stats) {
  for (ResourceList::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (base::GetProcId((*it)->GetProcess()) == renderer_id) {
      (*it)->NotifyResourceTypeStats(stats);
    }
  }
}

void TaskManagerModel::NotifyV8HeapStats(base::ProcessId renderer_id,
                                         size_t v8_memory_allocated,
                                         size_t v8_memory_used) {
  for (ResourceList::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (base::GetProcId((*it)->GetProcess()) == renderer_id) {
      (*it)->NotifyV8HeapStats(v8_memory_allocated, v8_memory_used);
    }
  }
}

void TaskManagerModel::Refresh() {
  DCHECK_NE(IDLE, update_state_);

  if (update_state_ == STOPPING) {
    // We have been asked to stop.
    update_state_ = IDLE;
    return;
  }

  // Compute the CPU usage values.
  // Note that we compute the CPU usage for all resources (instead of doing it
  // lazily) as process_util::GetCPUUsage() returns the CPU usage since the last
  // time it was called, and not calling it everytime would skew the value the
  // next time it is retrieved (as it would be for more than 1 cycle).
  cpu_usage_map_.clear();
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
    base::ProcessHandle process = (*iter)->GetProcess();
    CPUUsageMap::iterator cpu_iter = cpu_usage_map_.find(process);
    if (cpu_iter != cpu_usage_map_.end())
      continue;  // Already computed.

    MetricsMap::iterator metrics_iter = metrics_map_.find(process);
    DCHECK(metrics_iter != metrics_map_.end());
    cpu_usage_map_[process] = metrics_iter->second->GetCPUUsage();
  }

  // Compute the new network usage values.
  displayed_network_usage_map_.clear();
  for (ResourceValueMap::iterator iter = current_byte_count_map_.begin();
       iter != current_byte_count_map_.end(); ++iter) {
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

  // Let resources update themselves if they need to.
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
     (*iter)->Refresh();
  }

  if (!resources_.empty()) {
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsChanged(0, ResourceCount()));
  }

  // Schedule the next update.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &TaskManagerModel::Refresh),
      kUpdateTimeMs);
}

int64 TaskManagerModel::GetNetworkUsageForResource(
    TaskManager::Resource* resource) const {
  ResourceValueMap::const_iterator iter =
      displayed_network_usage_map_.find(resource);
  if (iter == displayed_network_usage_map_.end())
    return 0;
  return iter->second;
}

void TaskManagerModel::BytesRead(BytesReadParam param) {
  if (update_state_ != TASK_PENDING) {
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
  TaskManager::Resource* resource = NULL;
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    resource = (*iter)->GetResource(param.origin_child_id,
                                    param.render_process_host_child_id,
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
void TaskManagerModel::OnJobAdded(URLRequestJob* job) {
}

void TaskManagerModel::OnJobRemoved(URLRequestJob* job) {
}

void TaskManagerModel::OnJobDone(URLRequestJob* job,
                                 const URLRequestStatus& status) {
}

void TaskManagerModel::OnJobRedirect(URLRequestJob* job,
                                     const GURL& location,
                                     int status_code) {
}

void TaskManagerModel::OnBytesRead(URLRequestJob* job, int byte_count) {
  int render_process_host_child_id = -1, routing_id = -1;
  ResourceDispatcherHost::RenderViewForRequest(job->request(),
                                               &render_process_host_child_id,
                                               &routing_id);
  // This happens in the IO thread, post it to the UI thread.
  int origin_child_id =
      chrome_browser_net::GetOriginProcessUniqueIDForRequest(job->request());
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &TaskManagerModel::BytesRead,
          BytesReadParam(origin_child_id,
          render_process_host_child_id,
          routing_id, byte_count)));
}

bool TaskManagerModel::GetProcessMetricsForRow(
    int row, base::ProcessMetrics** proc_metrics) const {
  DCHECK(row < ResourceCount());
  *proc_metrics = NULL;

  MetricsMap::const_iterator iter =
      metrics_map_.find(resources_[row]->GetProcess());
  if (iter == metrics_map_.end())
    return false;
  *proc_metrics = iter->second;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManager class
////////////////////////////////////////////////////////////////////////////////

// static
void TaskManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
}

TaskManager::TaskManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(new TaskManagerModel(this))) {
}

TaskManager::~TaskManager() {
}

bool TaskManager::IsBrowserProcess(int index) const {
  // If some of the selection is out of bounds, ignore. This may happen when
  // killing a process that manages several pages.
  return index < model_->ResourceCount() &&
      model_->GetResourceProcessHandle(index) ==
      base::GetCurrentProcessHandle();
}

void TaskManager::KillProcess(int index) {
  base::ProcessHandle process = model_->GetResourceProcessHandle(index);
  DCHECK(process);
  if (process != base::GetCurrentProcessHandle())
    base::KillProcess(process, base::PROCESS_END_KILLED_BY_USER, false);
}

void TaskManager::ActivateProcess(int index) {
  // GetResourceTabContents returns a pointer to the relevant tab contents for
  // the resource.  If the index doesn't correspond to a Tab (i.e. refers to
  // the Browser process or a plugin), GetTabContents will return NULL.
  TabContents* chosen_tab_contents = model_->GetResourceTabContents(index);
  if (chosen_tab_contents)
    chosen_tab_contents->Activate();
}

void TaskManager::AddResourceProvider(ResourceProvider* provider) {
  model_->AddResourceProvider(provider);
}

void TaskManager::RemoveResourceProvider(ResourceProvider* provider) {
  model_->RemoveResourceProvider(provider);
}

void TaskManager::AddResource(Resource* resource) {
  model_->AddResource(resource);
}

void TaskManager::RemoveResource(Resource* resource) {
  model_->RemoveResource(resource);
}

void TaskManager::OnWindowClosed() {
  model_->StopUpdating();
  model_->Clear();
}

// static
TaskManager* TaskManager::GetInstance() {
  return Singleton<TaskManager>::get();
}

void TaskManager::OpenAboutMemory() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenURL(GURL(chrome::kAboutMemoryURL), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
  // In case the browser window is minimzed, show it. If this is an application
  // or popup, we can only have one tab, hence we need to process this in a
  // tabbed browser window. Currently, |browser| is pointing to the application,
  // popup window. Therefore, we have to retrieve the last active tab again,
  // since a new window has been used.
  if (browser->type() & Browser::TYPE_APP_POPUP) {
    browser = BrowserList::GetLastActive();
    DCHECK(browser);
  }
  browser->window()->Show();
}
