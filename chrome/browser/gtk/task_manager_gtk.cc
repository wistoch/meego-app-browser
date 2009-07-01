// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/task_manager_gtk.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// The task manager window default size.
const int kDefaultWidth = 460;
const int kDefaultHeight = 270;

// The resource id for the 'End process' button.
const gint kTaskManagerResponseKill = 1;

enum TaskManagerColumn {
  kTaskManagerPage,
  kTaskManagerPhysicalMem,
  kTaskManagerSharedMem,
  kTaskManagerPrivateMem,
  kTaskManagerCPU,
  kTaskManagerNetwork,
  kTaskManagerProcessID,
  kTaskManagerGoatsTeleported,
  kTaskManagerColumnCount,
};

TaskManagerColumn TaskManagerResourceIDToColumnID(int id) {
  switch (id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:
      return kTaskManagerPage;
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return kTaskManagerPhysicalMem;
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return kTaskManagerSharedMem;
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return kTaskManagerPrivateMem;
    case IDS_TASK_MANAGER_CPU_COLUMN:
      return kTaskManagerCPU;
    case IDS_TASK_MANAGER_NET_COLUMN:
      return kTaskManagerNetwork;
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return kTaskManagerProcessID;
    default:
      NOTREACHED();
      return static_cast<TaskManagerColumn>(-1);
  }
}

// Shows or hides a treeview column.
void TreeViewColumnSetVisible(GtkWidget* treeview, TaskManagerColumn colid,
                              bool visible) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview),
                                                       colid);
  gtk_tree_view_column_set_visible(column, visible);
}

// Inserts a column with a column id of |colid| and |name|.
void TreeViewInsertColumnWithName(GtkWidget* treeview,
                                  int colid, const char* name) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
                                              name, renderer, "text",
                                              colid, NULL);
}

// Loads the column name from |resid| and uses the corresponding
// TaskManagerColumn value as the column id to insert into the treeview.
void TreeViewInsertColumn(GtkWidget* treeview, int resid) {
  TreeViewInsertColumnWithName(treeview, TaskManagerResourceIDToColumnID(resid),
                               l10n_util::GetStringUTF8(resid).c_str());
}

// Get the row number corresponding to |path|.
gint GetRowNumForPath(GtkTreePath* path) {
  gint* indices = gtk_tree_path_get_indices(path);
  if (!indices) {
    NOTREACHED();
    return -1;
  }
  return indices[0];
}

}  // namespace

TaskManagerGtk::TaskManagerGtk()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()),
    dialog_(NULL),
    treeview_(NULL),
    process_list_(NULL),
    process_count_(0) {
  Init();
}

// static
TaskManagerGtk* TaskManagerGtk::instance_ = NULL;

TaskManagerGtk::~TaskManagerGtk() {
  task_manager_->OnWindowClosed();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, TaskManagerModelObserver implementation:

void TaskManagerGtk::OnModelChanged() {
  // Nothing to do.
}

void TaskManagerGtk::OnItemsChanged(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = start; i < start + length; i++) {
    SetRowDataFromModel(i, &iter);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(process_list_), &iter);
  }
}

void TaskManagerGtk::OnItemsAdded(int start, int length) {
  GtkTreeIter iter;
  if (start == 0) {
    gtk_list_store_prepend(process_list_, &iter);
  } else if (start >= process_count_) {
    gtk_list_store_append(process_list_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &sibling,
                                  NULL, start);
    gtk_list_store_insert_before(process_list_, &iter, &sibling);
  }

  SetRowDataFromModel(start, &iter);

  for (int i = start + 1; i < start + length; i++) {
    gtk_list_store_insert_after(process_list_, &iter, &iter);
    SetRowDataFromModel(i, &iter);
  }

  process_count_ += length;
}

void TaskManagerGtk::OnItemsRemoved(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = 0; i < length; i++) {
    // |iter| is moved to the next valid node when the current node is removed.
    gtk_list_store_remove(process_list_, &iter);
  }

  process_count_ -= length;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, public:

// static
void TaskManagerGtk::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    gtk_window_present(GTK_WINDOW(instance_->dialog_));
  } else {
    instance_ = new TaskManagerGtk;
    instance_->model_->StartUpdating();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, private:

void TaskManagerGtk::Init() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_TITLE).c_str(),
      // Task Manager window is shared between all browsers.
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_KILL).c_str(),
      kTaskManagerResponseKill,
      NULL);

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  g_signal_connect(G_OBJECT(dialog_), "response", G_CALLBACK(OnResponse), this);

  CreateTaskManagerTreeview();
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(treeview_), TRUE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview_),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

  // Hide some columns by default
  TreeViewColumnSetVisible(treeview_, kTaskManagerSharedMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerPrivateMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerProcessID, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerGoatsTeleported, false);

  // |selection| is owned by |treeview_|.
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect(G_OBJECT(selection), "changed",
                   G_CALLBACK(OnSelectionChanged), this);

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), treeview_);

  gtk_window_resize(GTK_WINDOW(dialog_), kDefaultWidth, kDefaultHeight);
  gtk_widget_show_all(dialog_);

  model_->SetObserver(this);
}

void TaskManagerGtk::CreateTaskManagerTreeview() {
  treeview_ = gtk_tree_view_new();

  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PAGE_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_CPU_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_NET_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);

  TreeViewInsertColumnWithName(treeview_, kTaskManagerGoatsTeleported,
                               "Goats Teleported");

  process_list_ = gtk_list_store_new(kTaskManagerColumnCount,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_),
                          GTK_TREE_MODEL(process_list_));
  g_object_unref(process_list_);
}

std::string TaskManagerGtk::GetModelText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return WideToUTF8(model_->GetResourceTitle(row));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourcePrivateMemory(row));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceSharedMemory(row));

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourcePhysicalMemory(row));

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceCPUUsage(row));

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return WideToUTF8(model_->GetResourceNetworkUsage(row));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:  // Process ID
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceProcessId(row));

    case kTaskManagerGoatsTeleported:  // Goats Teleported!
      return WideToUTF8(model_->GetResourceGoatsTeleported(row));

    default:
      return WideToUTF8(model_->GetResourceStatsValue(row, col_id));
  }
}

void TaskManagerGtk::SetRowDataFromModel(int row, GtkTreeIter* iter) {
  std::string page = GetModelText(row, IDS_TASK_MANAGER_PAGE_COLUMN);
  std::string phys_mem = GetModelText(
      row, IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  std::string shared_mem = GetModelText(
      row, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  std::string priv_mem = GetModelText(row, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  std::string cpu = GetModelText(row, IDS_TASK_MANAGER_CPU_COLUMN);
  std::string net = GetModelText(row, IDS_TASK_MANAGER_NET_COLUMN);
  std::string procid = GetModelText(row, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
  std::string goats = GetModelText(row, kTaskManagerGoatsTeleported);
  gtk_list_store_set(process_list_, iter,
                     kTaskManagerPage, page.c_str(),
                     kTaskManagerPhysicalMem, phys_mem.c_str(),
                     kTaskManagerSharedMem, shared_mem.c_str(),
                     kTaskManagerPrivateMem, priv_mem.c_str(),
                     kTaskManagerCPU, cpu.c_str(),
                     kTaskManagerNetwork, net.c_str(),
                     kTaskManagerProcessID, procid.c_str(),
                     kTaskManagerGoatsTeleported, goats.c_str(),
                     -1);
}

void TaskManagerGtk::KillSelectedProcesses() {
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    int row = GetRowNumForPath(reinterpret_cast<GtkTreePath*>(item->data));
    task_manager_->KillProcess(row);
  }
  g_list_free(paths);
}

// static
void TaskManagerGtk::OnResponse(GtkDialog* dialog, gint response_id,
                                TaskManagerGtk* task_manager) {
  if (response_id == GTK_RESPONSE_DELETE_EVENT) {
    instance_ = NULL;
    delete task_manager;
  } else if (response_id == kTaskManagerResponseKill) {
    task_manager->KillSelectedProcesses();
  }
}

// static
void TaskManagerGtk::OnSelectionChanged(GtkTreeSelection* selection,
                                        TaskManagerGtk* task_manager) {
  bool selection_contains_browser_process = false;

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    int row = GetRowNumForPath(reinterpret_cast<GtkTreePath*>(item->data));
    if (task_manager->task_manager_->IsBrowserProcess(row)) {
      selection_contains_browser_process = true;
      break;
    }
  }
  g_list_free(paths);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(task_manager->dialog_),
                                    kTaskManagerResponseKill,
                                    !selection_contains_browser_process);
}
