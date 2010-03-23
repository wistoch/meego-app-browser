// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_manager_view.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/keyboard_codes.h"
#include "base/thread.h"
#include "chrome/browser/bookmarks/bookmark_folder_tree_model.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_table_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/views/bookmark_editor_view.h"
#include "chrome/browser/views/bookmark_folder_tree_view.h"
#include "chrome/browser/views/bookmark_table_view.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"
#include "gfx/skia_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/single_split_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// If non-null, there is an open editor and this is the window it is contained
// in it.
static views::Window* open_window = NULL;
// And this is the manager contained in it.
static BookmarkManagerView* manager = NULL;

// Delay, in ms, between when the user types and when we run the search.
static const int kSearchDelayMS = 200;

static const int kOrganizeMenuButtonID = 1;
static const int kToolsMenuButtonID = 2;

// Background color.
static const SkColor kBackgroundColorTop = SkColorSetRGB(242, 247, 253);
static const SkColor kBackgroundColorBottom = SkColorSetRGB(223, 234, 248);
static const int kBackgroundGradientHeight = 28;

namespace {

// Observer installed on the importer. When done importing the newly created
// folder is selected in the bookmark manager.
class ImportObserverImpl : public ImportObserver {
 public:
  explicit ImportObserverImpl(Profile* profile) : profile_(profile) {
    BookmarkModel* model = profile->GetBookmarkModel();
    initial_other_count_ = model->other_node()->GetChildCount();
  }

  virtual void ImportCanceled() {
    delete this;
  }

  virtual void ImportComplete() {
    // We aren't needed anymore.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);

    BookmarkManagerView* manager = BookmarkManagerView::current();
    if (!manager || manager->profile() != profile_)
      return;

    BookmarkModel* model = profile_->GetBookmarkModel();
    int other_count = model->other_node()->GetChildCount();
    if (other_count == initial_other_count_ + 1) {
      const BookmarkNode* imported_node =
          model->other_node()->GetChild(initial_other_count_);
      manager->SelectInTree(imported_node);
      manager->ExpandAll(imported_node);
    }
  }

 private:
  Profile* profile_;
  // Number of children in the other bookmarks folder at the time we were
  // created.
  int initial_other_count_;

  DISALLOW_COPY_AND_ASSIGN(ImportObserverImpl);
};

// Converts a virtual keycode into the CutCopyPasteType.
BookmarkManagerView::CutCopyPasteType KeyCodeToCutCopyPaste(
    base::KeyboardCode keycode) {
  switch (keycode) {
    case base::VKEY_INSERT:
      if (GetKeyState(VK_CONTROL) < 0)
        return BookmarkManagerView::COPY;
      if (GetKeyState(VK_SHIFT) < 0)
        return BookmarkManagerView::PASTE;
      return BookmarkManagerView::NONE;

    case base::VKEY_DELETE:
      if (GetKeyState(VK_SHIFT) < 0)
        return BookmarkManagerView::CUT;
      return BookmarkManagerView::NONE;

    case base::VKEY_C:
      if (GetKeyState(VK_CONTROL) < 0)
        return BookmarkManagerView::COPY;
      return BookmarkManagerView::NONE;

    case base::VKEY_V:
      if (GetKeyState(VK_CONTROL) < 0)
        return BookmarkManagerView::PASTE;
      return BookmarkManagerView::NONE;

    case base::VKEY_X:
      if (GetKeyState(VK_CONTROL) < 0)
        return BookmarkManagerView::CUT;
      return BookmarkManagerView::NONE;

    default:
      return BookmarkManagerView::NONE;
  }
}

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowBookmarkManagerView(Profile* profile) {
  BookmarkManagerView::Show(profile);
}

}  // namespace browser

// BookmarkManager -------------------------------------------------------------

void BookmarkManager::SelectInTree(Profile* profile, const BookmarkNode* node) {
  if (manager && manager->profile() == profile)
    manager->SelectInTree(node);
}

void BookmarkManager::Show(Profile* profile) {
  BookmarkManagerView::Show(profile);
}

// -----------------------------------------------------------------------------

BookmarkManagerView::BookmarkManagerView(Profile* profile)
    : profile_(profile->GetOriginalProfile()),
      table_view_(NULL),
      tree_view_(NULL),
      sync_status_button_(NULL),
      sync_service_(NULL),
      sync_relogin_required_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(search_factory_(this)) {
  views::Label* search_label = new views::Label(
      l10n_util::GetString(IDS_BOOKMARK_MANAGER_SEARCH_TITLE));
  search_tf_ = new views::Textfield();
  search_tf_->set_default_width_in_chars(30);
  search_tf_->SetAccessibleName(search_label->GetText());

  table_view_ = new BookmarkTableView(profile_, NULL);
  table_view_->SetObserver(this);
  table_view_->SetContextMenuController(this);

  tree_view_ = new BookmarkFolderTreeView(profile_, NULL);
  tree_view_->SetController(this);
  tree_view_->SetContextMenuController(this);

  views::MenuButton* organize_menu_button = new views::MenuButton(
      NULL, l10n_util::GetString(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU),
      this, true);
  organize_menu_button->SetID(kOrganizeMenuButtonID);

  views::MenuButton* tools_menu_button = new views::MenuButton(
      NULL, l10n_util::GetString(IDS_BOOKMARK_MANAGER_TOOLS_MENU),
      this, true);
  tools_menu_button->SetID(kToolsMenuButtonID);

  split_view_ = new views::SingleSplitView(tree_view_, table_view_,
      views::SingleSplitView::HORIZONTAL_SPLIT);
  split_view_->set_resize_leading_on_bounds_change(false);
  split_view_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColorBottom));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int top_id = 1;
  const int split_cs_id = 2;
  layout->SetInsets(2, 0, 0, 0);  // 2px padding above content.
  views::ColumnSet* column_set = layout->AddColumnSet(top_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 3);  // 3px padding at end of row.

  column_set = layout->AddColumnSet(split_cs_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, top_id);
  layout->AddView(organize_menu_button);
  layout->AddView(tools_menu_button);
  sync_status_button_ = new views::TextButton(this, std::wstring());
  layout->AddView(sync_status_button_);
  layout->AddView(search_label);
  layout->AddView(search_tf_);

  layout->AddPaddingRow(0, 3);  // 3px padding between rows.

  layout->StartRow(1, split_cs_id);
  layout->AddView(split_view_);

  // Press Ctrl-W to close bookmark manager window.
  AddAccelerator(views::Accelerator(base::VKEY_W, false, true, false));

  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (!bookmark_model->IsLoaded())
    bookmark_model->AddObserver(this);

  if (profile_->GetProfileSyncService()) {
    sync_service_ = profile_->GetProfileSyncService();
    sync_service_->AddObserver(this);
    UpdateSyncStatus();
  }
}

BookmarkManagerView::~BookmarkManagerView() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  if (!GetBookmarkModel()->IsLoaded()) {
    GetBookmarkModel()->RemoveObserver(this);
  } else {
    // The models are deleted before the views. Make sure we set the models of
    // the views to NULL so that they aren't left holding a reference to a
    // deleted model.
    table_view_->SetModel(NULL);
    tree_view_->SetModel(NULL);
  }
  manager = NULL;
  open_window = NULL;

  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

// static
void BookmarkManagerView::Show(Profile* profile) {
  if (!profile->GetBookmarkModel())
    return;

  if (open_window != NULL) {
    open_window->Activate();
    return;
  }

  // Both of these will be deleted when the dialog closes.
  manager = new BookmarkManagerView(profile);

  // Create the window.
  open_window = views::Window::CreateChromeWindow(NULL, gfx::Rect(), manager);
  // Let the manager know it's parented.
  manager->PrepareForShow();
  // And show it.
  open_window->Show();

  // Give initial focus to the search field.
  manager->search_tf_->RequestFocus();
}

// static
BookmarkManagerView* BookmarkManagerView::current() {
  return manager;
}

void BookmarkManagerView::SelectInTree(const BookmarkNode* node) {
  if (!node)
    return;

  const BookmarkNode* parent = node->is_url() ? node->GetParent() : node;
  FolderNode* folder_node = tree_model_->GetFolderNodeForBookmarkNode(parent);
  if (!folder_node) {
    NOTREACHED();
    return;
  }

  tree_view_->SetSelectedNode(folder_node);

  if (node->is_url()) {
    int index = table_model_->IndexOfNode(node);
    if (index != -1)
      table_view_->Select(index);
    // TODO(sky): this doesn't work when invoked from add page.
    table_view_->RequestFocus();
  }
}

void BookmarkManagerView::ExpandAll(const BookmarkNode* node) {
  const BookmarkNode* parent = node->is_url() ? node->GetParent() : node;
  FolderNode* folder_node = tree_model_->GetFolderNodeForBookmarkNode(parent);
  if (!folder_node) {
    NOTREACHED();
    return;
  }
  tree_view_->ExpandAll(folder_node);
}

const BookmarkNode* BookmarkManagerView::GetSelectedFolder() {
  return tree_view_->GetSelectedBookmarkNode();
}

std::vector<const BookmarkNode*> BookmarkManagerView::GetSelectedTableNodes() {
  std::vector<const BookmarkNode*> nodes;
  for (views::TableView::iterator i = table_view_->SelectionBegin();
       i != table_view_->SelectionEnd(); ++i) {
    nodes.push_back(table_model_->GetNodeForRow(*i));
  }
  // TableViews iterator iterates in reverse order. Reverse the nodes so they
  // are opened in visual order.
  std::reverse(nodes.begin(), nodes.end());
  return nodes;
}

void BookmarkManagerView::PaintBackground(gfx::Canvas* canvas) {
  canvas->drawColor(kBackgroundColorBottom, SkXfermode::kSrc_Mode);

  SkPaint paint;
  paint.setShader(gfx::CreateGradientShader(0, kBackgroundGradientHeight,
      kBackgroundColorTop,
      kBackgroundColorBottom))->safeUnref();
  canvas->FillRectInt(0, 0, width(), kBackgroundGradientHeight, paint);
}

gfx::Size BookmarkManagerView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_BOOKMARK_MANAGER_DIALOG_WIDTH_CHARS,
      IDS_BOOKMARK_MANAGER_DIALOG_HEIGHT_LINES));
}

std::wstring BookmarkManagerView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BOOKMARK_MANAGER_TITLE);
}

std::wstring BookmarkManagerView::GetWindowName() const {
  return prefs::kBookmarkManagerPlacement;
}

void BookmarkManagerView::WindowClosing() {
  g_browser_process->local_state()->SetInteger(
      prefs::kBookmarkManagerSplitLocation, split_view_->divider_offset());
}

void BookmarkManagerView::OnStateChanged() {
  UpdateSyncStatus();
}

bool BookmarkManagerView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  // Ctrl-W to close bookmark manager.
  DCHECK(accelerator.GetKeyCode() == 'W' && accelerator.IsCtrlDown());
  window()->Close();
  return true;
}

void BookmarkManagerView::OnDoubleClick() {
  std::vector<const BookmarkNode*> nodes = GetSelectedTableNodes();
  if (nodes.empty())
    return;
  if (nodes.size() == 1 && nodes[0]->is_folder()) {
    // Double click on a folder descends into the folder.
    SelectInTree(nodes[0]);
    return;
  }
  // TODO(sky): OnDoubleClick needs a handle to the current mouse event so that
  // we can use
  // event_utils::DispositionFromEventFlags(sender->mouse_event_flags()) .
  bookmark_utils::OpenAll(
      GetWidget()->GetNativeView(), profile_, NULL, nodes, CURRENT_TAB);
}

void BookmarkManagerView::OnMiddleClick() {
  std::vector<const BookmarkNode*> nodes = GetSelectedTableNodes();
  if (nodes.empty())
    return;
  if (nodes.size() == 1 && nodes[0]->is_folder()) {
    // Middle clicking on a folder results in no action.
    return;
  }

  bookmark_utils::OpenAll(
      GetWidget()->GetNativeView(), profile_, NULL, nodes, NEW_FOREGROUND_TAB);
}

void BookmarkManagerView::OnTableViewDelete(views::TableView* table) {
  std::vector<const BookmarkNode*> nodes = GetSelectedTableNodes();
  if (nodes.empty())
    return;
  for (size_t i = 0; i < nodes.size(); ++i) {
    GetBookmarkModel()->Remove(nodes[i]->GetParent(),
                               nodes[i]->GetParent()->IndexOfChild(nodes[i]));
  }
}

void BookmarkManagerView::OnKeyDown(base::KeyboardCode keycode) {
  switch (keycode) {
    case base::VKEY_RETURN: {
      std::vector<const BookmarkNode*> selected_nodes = GetSelectedTableNodes();
      if (selected_nodes.size() == 1 && selected_nodes[0]->is_folder()) {
        SelectInTree(selected_nodes[0]);
      } else {
        bookmark_utils::OpenAll(
            GetWidget()->GetNativeView(), profile_, NULL, selected_nodes,
            CURRENT_TAB);
      }
      break;
    }

    case base::VKEY_BACK: {
      const BookmarkNode* selected_folder = GetSelectedFolder();
      if (selected_folder != NULL &&
          selected_folder->GetParent() != GetBookmarkModel()->root_node()) {
        SelectInTree(selected_folder->GetParent());
      }
      break;
    }

    default:
      OnCutCopyPaste(KeyCodeToCutCopyPaste(keycode), true);
      break;
  }
}

void BookmarkManagerView::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  TreeModelNode* node = tree_view_->GetSelectedNode();

  BookmarkTableModel* new_table_model = NULL;
  const BookmarkNode* table_parent_node = NULL;
  bool is_search = false;

  if (node) {
    switch (tree_model_->GetNodeType(node)) {
      case BookmarkFolderTreeModel::BOOKMARK:
        table_parent_node = tree_model_->TreeNodeAsBookmarkNode(node);
        new_table_model =
            BookmarkTableModel::CreateBookmarkTableModelForFolder(
                profile_->GetBookmarkModel(),
                table_parent_node);
        break;

      case BookmarkFolderTreeModel::RECENTLY_BOOKMARKED:
        new_table_model = BookmarkTableModel::CreateRecentlyBookmarkedModel(
            profile_->GetBookmarkModel());
        break;

      case BookmarkFolderTreeModel::SEARCH:
        is_search = true;
        search_factory_.RevokeAll();
        new_table_model = CreateSearchTableModel();
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  SetTableModel(new_table_model, table_parent_node, is_search);
}

void BookmarkManagerView::OnTreeViewKeyDown(base::KeyboardCode keycode) {
  switch (keycode) {
    case base::VKEY_DELETE: {
      const BookmarkNode* node = GetSelectedFolder();
      if (!node || node->GetParent() == GetBookmarkModel()->root_node())
        return;

      const BookmarkNode* parent = node->GetParent();
      GetBookmarkModel()->Remove(parent, parent->IndexOfChild(node));
      break;
    }

    default:
      OnCutCopyPaste(KeyCodeToCutCopyPaste(keycode), false);
      break;
  }
}

void BookmarkManagerView::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  if (sender == sync_status_button_) {
    if (sync_relogin_required_) {
      DCHECK(sync_service_);
      sync_service_->ShowLoginDialog();
    } else {
      UserMetrics::RecordAction(UserMetricsAction("BookmarkManager_Sync"),
                                profile_);
      sync_ui_util::OpenSyncMyBookmarksDialog(
          profile_, ProfileSyncService::START_FROM_BOOKMARK_MANAGER);
    }
  }
}

void BookmarkManagerView::Loaded(BookmarkModel* model) {
  model->RemoveObserver(this);
  LoadedImpl();
}

void BookmarkManagerView::ContentsChanged(views::Textfield* sender,
                                          const std::wstring& new_contents) {
  search_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      search_factory_.NewRunnableMethod(&BookmarkManagerView::PerformSearch),
      kSearchDelayMS);
}

bool BookmarkManagerView::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& key) {
  if (key.GetKeyboardCode() == base::VKEY_RETURN) {
    PerformSearch();
    search_tf_->SelectAll();
  }

  return false;
}

void BookmarkManagerView::ShowContextMenu(views::View* source,
                                          const gfx::Point& p,
                                          bool is_mouse_gesture) {
  DCHECK(source == table_view_ || source == tree_view_);
  bool is_table = (source == table_view_);
  ShowMenu(p, is_table ?
      BookmarkContextMenuControllerViews::BOOKMARK_MANAGER_TABLE :
      BookmarkContextMenuControllerViews::BOOKMARK_MANAGER_TREE);
}

void BookmarkManagerView::RunMenu(views::View* source, const gfx::Point& pt) {
  // TODO(glen): when you change the buttons around and what not, futz with
  // this to make it look good. If you end up keeping padding numbers make them
  // constants.
  if (!GetBookmarkModel()->IsLoaded())
    return;

  gfx::Point menu_pt(pt);
  menu_pt.Offset(UILayoutIsRightToLeft() ?
      (source->width() - 5) : (-source->width() + 5), 2);
  if (source->GetID() == kOrganizeMenuButtonID) {
    ShowMenu(menu_pt,
        BookmarkContextMenuControllerViews::BOOKMARK_MANAGER_ORGANIZE_MENU);
  } else if (source->GetID() == kToolsMenuButtonID) {
    ShowToolsMenu(menu_pt);
  } else {
    NOTREACHED();
  }
}

void BookmarkManagerView::ExecuteCommand(int id) {
  switch (id) {
    case IDS_BOOKMARK_MANAGER_IMPORT_MENU:
      UserMetrics::RecordAction(UserMetricsAction("BookmarkManager_Import"),
                                profile_);
      ShowImportBookmarksFileChooser();
      break;

    case IDS_BOOKMARK_MANAGER_EXPORT_MENU:
      UserMetrics::RecordAction(UserMetricsAction("BookmarkManager_Export"),
                                profile_);
      ShowExportBookmarksFileChooser();
      break;

    default:
      NOTREACHED();
      break;
  }
}

void BookmarkManagerView::FileSelected(const FilePath& path,
                                       int index,
                                       void* params) {
  int id = reinterpret_cast<int>(params);
  if (id == IDS_BOOKMARK_MANAGER_IMPORT_MENU) {
    // ImporterHost is ref counted and will delete itself when done.
    ImporterHost* host = new ImporterHost();
    ProfileInfo profile_info;
    profile_info.browser_type = importer::BOOKMARKS_HTML;
    profile_info.source_path = path.ToWStringHack();
    StartImportingWithUI(GetWidget()->GetNativeView(), importer::FAVORITES,
                         host, profile_info, profile_,
                         new ImportObserverImpl(profile()), false);
  } else if (id == IDS_BOOKMARK_MANAGER_EXPORT_MENU) {
    bookmark_html_writer::WriteBookmarks(profile(), path, NULL);
  } else {
    NOTREACHED();
  }
}

void BookmarkManagerView::FileSelectionCanceled(void* params) {
  select_file_dialog_ = NULL;
}

BookmarkTableModel* BookmarkManagerView::CreateSearchTableModel() {
  std::wstring search_text = search_tf_->text();
  if (search_text.empty())
    return NULL;
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  return BookmarkTableModel::CreateSearchTableModel(
      GetBookmarkModel(), search_text, languages);
}

void BookmarkManagerView::SetTableModel(BookmarkTableModel* new_table_model,
                                        const BookmarkNode* parent_node,
                                        bool is_search) {
  // Be sure and reset the model on the view before updating table_model_.
  // Otherwise the view will attempt to use the deleted model when we set the
  // new one.
  table_view_->SetModel(NULL);
  table_view_->SetShowPathColumn(!parent_node);
  table_view_->SetModel(new_table_model);
  table_view_->set_parent_node(parent_node);
  table_model_.reset(new_table_model);
  if (!is_search || (new_table_model && new_table_model->RowCount() > 0)) {
    table_view_->SetAltText(std::wstring());
  } else if (search_tf_->text().empty()) {
    table_view_->SetAltText(
        l10n_util::GetString(IDS_BOOKMARK_MANAGER_NO_SEARCH_TEXT));
  } else {
    table_view_->SetAltText(
        l10n_util::GetStringF(IDS_BOOKMARK_MANAGER_NO_RESULTS,
                              search_tf_->text()));
  }
}

void BookmarkManagerView::PerformSearch() {
  search_factory_.RevokeAll();
  // Reset the controller, otherwise when we change the selection we'll get
  // notified and update the model twice.
  tree_view_->SetController(NULL);
  tree_view_->SetSelectedNode(tree_model_->search_node());
  tree_view_->SetController(this);
  SetTableModel(CreateSearchTableModel(), NULL, true);
}

void BookmarkManagerView::PrepareForShow() {
  // Restore the split location, but don't let it get too small (or big),
  // otherwise users might inadvertently not see the divider.
  int split_x = g_browser_process->local_state()->GetInteger(
      prefs::kBookmarkManagerSplitLocation);
  if (split_x == -1) {
    // First time running the bookmark manager, give a third of the width to
    // the tree.
    split_x = split_view_->width() / 3;
  }
  int min_split_size = split_view_->width() / 8;
  // Make sure the user can see both the tree/table.
  split_x = std::min(split_view_->width() - min_split_size,
                     std::max(min_split_size, split_x));
  split_view_->set_divider_offset(split_x);
  if (!GetBookmarkModel()->IsLoaded()) {
    search_tf_->SetReadOnly(true);
    return;
  }

  LoadedImpl();
}

void BookmarkManagerView::LoadedImpl() {
  BookmarkModel* bookmark_model = GetBookmarkModel();
  const BookmarkNode* bookmark_bar_node = bookmark_model->GetBookmarkBarNode();
  table_model_.reset(
      BookmarkTableModel::CreateBookmarkTableModelForFolder(bookmark_model,
                                                            bookmark_bar_node));
  table_view_->SetModel(table_model_.get());
  table_view_->set_parent_node(bookmark_bar_node);

  tree_model_.reset(new BookmarkFolderTreeModel(bookmark_model));
  tree_view_->SetModel(tree_model_.get());

  tree_view_->ExpandAll();

  tree_view_->SetSelectedNode(
      tree_model_->GetFolderNodeForBookmarkNode(bookmark_bar_node));

  search_tf_->SetReadOnly(false);
  search_tf_->SetController(this);

  Layout();
  SchedulePaint();
}

BookmarkModel* BookmarkManagerView::GetBookmarkModel() const {
  return profile_->GetBookmarkModel();
}

void BookmarkManagerView::ShowMenu(
    const gfx::Point& p,
    BookmarkContextMenuControllerViews::ConfigurationType config) {
  if (!GetBookmarkModel()->IsLoaded())
    return;

  const BookmarkNode* parent = GetSelectedFolder();
  std::vector<const BookmarkNode*> nodes;
  typedef BookmarkContextMenuControllerViews BCMCV;  // Such a long name!
  if (config == BCMCV::BOOKMARK_MANAGER_TABLE ||
      (config == BCMCV::BOOKMARK_MANAGER_ORGANIZE_MENU &&
       table_view_->HasFocus())) {
    nodes = GetSelectedTableNodes();
    if (!parent) {
      config = (config == BCMCV::BOOKMARK_MANAGER_TABLE) ?
          BCMCV::BOOKMARK_MANAGER_TABLE_OTHER :
          BCMCV::BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER;
    }
  } else {
    if (parent)
      nodes.push_back(parent);
  }
  BookmarkContextMenu menu(GetWidget()->GetNativeView(), profile_, NULL, parent,
                           nodes, config);
  menu.RunMenuAt(p);
}

void BookmarkManagerView::OnCutCopyPaste(CutCopyPasteType type,
                                         bool from_table) {
  if (type == CUT || type == COPY) {
    std::vector<const BookmarkNode*> nodes;
    if (from_table) {
      nodes = GetSelectedTableNodes();
    } else {
      const BookmarkNode* node = GetSelectedFolder();
      if (!node || node->GetParent() == GetBookmarkModel()->root_node())
        return;
      nodes.push_back(node);
    }
    if (nodes.empty())
      return;

    bookmark_utils::CopyToClipboard(GetBookmarkModel(), nodes, type == CUT);
  } else if (type == PASTE) {
    int index = from_table ? table_view_->FirstSelectedRow() : -1;
    if (index != -1)
      index++;
    bookmark_utils::PasteFromClipboard(GetBookmarkModel(), GetSelectedFolder(),
                                       index);
  }
}

void BookmarkManagerView::ShowToolsMenu(const gfx::Point& p) {
  views::MenuItemView menu(this);
  menu.AppendMenuItemWithLabel(
          IDS_BOOKMARK_MANAGER_IMPORT_MENU,
          l10n_util::GetString(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
  menu.AppendMenuItemWithLabel(
          IDS_BOOKMARK_MANAGER_EXPORT_MENU,
          l10n_util::GetString(IDS_BOOKMARK_MANAGER_EXPORT_MENU));
  views::MenuItemView::AnchorPosition anchor = UILayoutIsRightToLeft() ?
      views::MenuItemView::TOPRIGHT : views::MenuItemView::TOPLEFT;
  menu.RunMenuAt(GetWidget()->GetNativeView(), NULL, gfx::Rect(p, gfx::Size()),
                 anchor, true);
}

void BookmarkManagerView::ShowImportBookmarksFileChooser() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("htm"));
  file_type_info.include_all_files = true;
  select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE, std::wstring(),
      FilePath(FILE_PATH_LITERAL("bookmarks.html")), &file_type_info, 0,
      std::wstring(), GetWidget()->GetNativeView(),
      reinterpret_cast<void*>(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
}

void BookmarkManagerView::ShowExportBookmarksFileChooser() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_type_info.include_all_files = true;
  select_file_dialog_ = SelectFileDialog::Create(this);
  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE, std::wstring(),
      FilePath(FILE_PATH_LITERAL("bookmarks.html")), &file_type_info, 0,
      L"html", GetWidget()->GetNativeView(),
      reinterpret_cast<void*>(IDS_BOOKMARK_MANAGER_EXPORT_MENU));
}

void BookmarkManagerView::UpdateSyncStatus() {
  DCHECK(sync_service_);
  string16 status_label;
  string16 link_label;
  sync_relogin_required_ = sync_ui_util::GetStatusLabels(sync_service_,
      &status_label, &link_label) == sync_ui_util::SYNC_ERROR;

  if (sync_relogin_required_) {
    sync_status_button_->SetText(
        l10n_util::GetString(IDS_SYNC_BOOKMARK_BAR_ERROR));
    // The tooltip is the only way we have to display text explaining the error
    // to the user.
    sync_status_button_->SetTooltipText(
        l10n_util::GetString(IDS_SYNC_BOOKMARK_BAR_ERROR_DESC));
    sync_status_button_->SetAccessibleName(
        l10n_util::GetString(IDS_ACCNAME_SYNC_ERROR_BUTTON));
    sync_status_button_->SetIcon(
        *ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING));
    sync_status_button_->GetParent()->Layout();
    return;
  }

  if (sync_service_->HasSyncSetupCompleted()) {
      string16 username = sync_service_->GetAuthenticatedUsername();
      status_label = l10n_util::GetStringFUTF16(IDS_SYNC_NTP_SYNCED_TO,
                                                username);
  } else if (sync_service_->SetupInProgress()) {
    status_label = l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
  } else {
    status_label = l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL);
  }
  sync_status_button_->SetText(status_label);
  sync_status_button_->SetTooltipText(L"");
  sync_status_button_->SetAccessibleName(L"");
  sync_status_button_->SetIcon(SkBitmap());
  sync_status_button_->GetParent()->Layout();
}
