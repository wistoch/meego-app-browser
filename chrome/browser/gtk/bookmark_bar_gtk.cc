// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bar_gtk.h"

#include <vector>

#include "app/gfx/text_elider.h"
#include "app/gtk_dnd_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/pickle.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/bookmark_context_menu.h"
#include "chrome/browser/gtk/bookmark_menu_controller_gtk.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The showing height of the bar.
const int kBookmarkBarHeight = 29;

// The height of the bar when it is "hidden". It is never completely hidden
// because even when it is closed it forms the bottom few pixels of the toolbar.
const int kBookmarkBarMinimumHeight = 4;

// Left-padding for the instructional text.
const int kInstructionsPadding = 6;

// Color of the instructional text.
const GdkColor kInstructionsColor = GDK_COLOR_RGB(128, 128, 142);

// Middle color of the separator gradient.
const double kSeparatorColor[] =
    { 194.0 / 255.0, 205.0 / 255.0, 212.0 / 212.0 };
// Top color of the separator gradient.
const double kTopBorderColor[] =
    { 222.0 / 255.0, 234.0 / 255.0, 248.0 / 255.0 };

// The targets accepted by the toolbar and folder buttons for DnD.
const int kDestTargetList[] = { GtkDndUtil::CHROME_BOOKMARK_ITEM,
                                GtkDndUtil::CHROME_NAMED_URL,
                                GtkDndUtil::TEXT_URI_LIST,
                                GtkDndUtil::TEXT_PLAIN, -1 };

// Acceptable drag actions for the bookmark bar drag destinations.
const GdkDragAction kDragAction =
    GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY);

void SetToolBarStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-bookmark-toolbar\" {"
      "  xthickness = 0\n"
      "  ythickness = 0\n"
      "  GtkWidget::focus-padding = 0\n"
      "  GtkContainer::border-width = 0\n"
      "  GtkToolBar::internal-padding = 0\n"
      "  GtkToolBar::shadow-type = GTK_SHADOW_NONE\n"
      "}\n"
      "widget \"*chrome-bookmark-toolbar\" style \"chrome-bookmark-toolbar\"");
}

}  // namespace

BookmarkBarGtk::BookmarkBarGtk(Profile* profile, Browser* browser,
                               BrowserWindowGtk* window)
    : profile_(NULL),
      page_navigator_(NULL),
      browser_(browser),
      window_(window),
      model_(NULL),
      instructions_(NULL),
      dragged_node_(NULL),
      toolbar_drop_item_(NULL),
      theme_provider_(GtkThemeProvider::GetFrom(profile)),
      show_instructions_(true) {
  Init(profile);
  SetProfile(profile);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

BookmarkBarGtk::~BookmarkBarGtk() {
  if (model_)
    model_->RemoveObserver(this);

  RemoveAllBookmarkButtons();
  bookmark_toolbar_.Destroy();
  event_box_.Destroy();
}

void BookmarkBarGtk::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  RemoveAllBookmarkButtons();

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  // TODO(erg): Handle extensions

  model_ = profile_->GetBookmarkModel();
  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  // else case: we'll receive notification back from the BookmarkModel when done
  // loading, then we'll populate the bar.
}

void BookmarkBarGtk::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
}

void BookmarkBarGtk::Init(Profile* profile) {
  event_box_.Own(gtk_event_box_new());
  // Make the event box transparent so themes can use transparent backgrounds.
  if (!theme_provider_->UseGtkTheme())
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  g_signal_connect(event_box_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressed), this);

  bookmark_hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), bookmark_hbox_);

  instructions_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(instructions_), 0, 0,
                            kInstructionsPadding, 0);
  g_signal_connect(instructions_, "destroy", G_CALLBACK(gtk_widget_destroyed),
                   &instructions_);
  GtkWidget* instructions_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_BOOKMARKS_NO_ITEMS).c_str());
  gtk_widget_modify_fg(instructions_label, GTK_STATE_NORMAL,
                       &kInstructionsColor);
  gtk_container_add(GTK_CONTAINER(instructions_), instructions_label);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), instructions_,
                     FALSE, FALSE, 0);

  gtk_drag_dest_set(instructions_,
      GtkDestDefaults(GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION),
      NULL, 0, kDragAction);
  GtkDndUtil::SetDestTargetList(instructions_, kDestTargetList);
  g_signal_connect(instructions_, "drag-data-received",
                   G_CALLBACK(&OnDragReceived), this);

  gtk_widget_set_app_paintable(widget(), TRUE);
  g_signal_connect(G_OBJECT(widget()), "expose-event",
                   G_CALLBACK(&OnEventBoxExpose), this);

  bookmark_toolbar_.Own(gtk_toolbar_new());
  SetToolBarStyle();
  gtk_widget_set_name(bookmark_toolbar_.get(), "chrome-bookmark-toolbar");
  gtk_widget_set_app_paintable(bookmark_toolbar_.get(), TRUE);
  g_signal_connect(bookmark_toolbar_.get(), "expose-event",
                   G_CALLBACK(&OnToolbarExpose), this);
  g_signal_connect(bookmark_toolbar_.get(), "size-allocate",
                   G_CALLBACK(&OnToolbarSizeAllocate), this);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), bookmark_toolbar_.get(),
                     TRUE, TRUE, 0);

  overflow_button_ = theme_provider_->BuildChromeButton();
  g_object_set_data(G_OBJECT(overflow_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  SetOverflowButtonAppearance();
  ConnectFolderButtonEvents(overflow_button_);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), overflow_button_,
                     FALSE, FALSE, 0);

  gtk_drag_dest_set(bookmark_toolbar_.get(), GTK_DEST_DEFAULT_DROP,
                    NULL, 0, kDragAction);
  GtkDndUtil::SetDestTargetList(bookmark_toolbar_.get(), kDestTargetList);
  g_signal_connect(bookmark_toolbar_.get(), "drag-motion",
                   G_CALLBACK(&OnToolbarDragMotion), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-leave",
                   G_CALLBACK(&OnToolbarDragLeave), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-data-received",
                   G_CALLBACK(&OnDragReceived), this);

  GtkWidget* vseparator = gtk_vseparator_new();
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), vseparator,
                     FALSE, FALSE, 0);
  g_signal_connect(vseparator, "expose-event",
                   G_CALLBACK(OnSeparatorExpose), this);

  // We pack the button manually (rather than using gtk_button_set_*) so that
  // we can have finer control over its label.
  other_bookmarks_button_ = theme_provider_->BuildChromeButton();
  ConnectFolderButtonEvents(other_bookmarks_button_);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), other_bookmarks_button_,
                     FALSE, FALSE, 0);

  gtk_widget_set_size_request(event_box_.get(), -1, kBookmarkBarMinimumHeight);

  slide_animation_.reset(new SlideAnimation(this));

  ViewIDUtil::SetID(widget(), VIEW_ID_BOOKMARK_BAR);
}

void BookmarkBarGtk::Show(bool animate) {
  gtk_widget_show_all(bookmark_hbox_);
  if (animate) {
    slide_animation_->Show();
  } else {
    slide_animation_->Reset(1);
    AnimationProgressed(slide_animation_.get());
  }

  // Maybe show the instructions
  if (show_instructions_) {
    gtk_widget_show(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }
}

void BookmarkBarGtk::Hide(bool animate) {
  // Sometimes we get called without a matching call to open. If that happens
  // then force hide.
  if (slide_animation_->IsShowing() && animate) {
    slide_animation_->Hide();
  } else {
    gtk_widget_hide(bookmark_hbox_);
    slide_animation_->Reset(0);
    AnimationProgressed(slide_animation_.get());
  }
}

int BookmarkBarGtk::GetHeight() {
  return event_box_->allocation.height;
}

bool BookmarkBarGtk::IsAnimating() {
  return slide_animation_->IsAnimating();
}

bool BookmarkBarGtk::OnNewTabPage() {
  return (browser_ && browser_->GetSelectedTabContents() &&
          browser_->GetSelectedTabContents()->IsBookmarkBarAlwaysVisible());
}

void BookmarkBarGtk::Loaded(BookmarkModel* model) {
  // If |instructions_| has been nulled, we are in the middle of browser
  // shutdown. Do nothing.
  if (!instructions_)
    return;

  RemoveAllBookmarkButtons();
  CreateAllBookmarkButtons();
}

void BookmarkBarGtk::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // The bookmark model should never be deleted before us. This code exists
  // to check for regressions in shutdown code and not crash.
  NOTREACHED();

  // Do minimal cleanup, presumably we'll be deleted shortly.
  model_->RemoveObserver(this);
  model_ = NULL;
}

void BookmarkBarGtk::BookmarkNodeMoved(BookmarkModel* model,
                                       const BookmarkNode* old_parent,
                                       int old_index,
                                       const BookmarkNode* new_parent,
                                       int new_index) {
  BookmarkNodeRemoved(model, old_parent, old_index, NULL);
  BookmarkNodeAdded(model, new_parent, new_index);
}

void BookmarkBarGtk::BookmarkNodeAdded(BookmarkModel* model,
                                       const BookmarkNode* parent,
                                       int index) {
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());

  GtkToolItem* item = CreateBookmarkToolItem(parent->GetChild(index));
  gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()),
                     item, index);

  SetInstructionState();
  SetChevronState();
}

void BookmarkBarGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                         const BookmarkNode* parent,
                                         int old_index,
                                         const BookmarkNode* node) {
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(old_index >= 0 && old_index < GetBookmarkButtonCount());

  GtkWidget* to_remove = GTK_WIDGET(gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), old_index));
  gtk_container_remove(GTK_CONTAINER(bookmark_toolbar_.get()),
                       to_remove);

  SetInstructionState();
  SetChevronState();
}

void BookmarkBarGtk::BookmarkNodeChanged(BookmarkModel* model,
                                         const BookmarkNode* node) {
  if (node->GetParent() != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->GetBookmarkBarNode()->IndexOfChild(node);
  DCHECK(index != -1);

  GtkToolItem* item = gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), index);
  GtkWidget* button = gtk_bin_get_child(GTK_BIN(item));
  bookmark_utils::ConfigureButtonForNode(node, model, button, theme_provider_);
  SetChevronState();
}

void BookmarkBarGtk::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                               const BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void BookmarkBarGtk::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  if (node != model_->GetBookmarkBarNode())
    return;  // We only care about reordering of the bookmark bar node.

  // Purge and rebuild the bar.
  RemoveAllBookmarkButtons();
  CreateAllBookmarkButtons();
}

void BookmarkBarGtk::CreateAllBookmarkButtons() {
  const BookmarkNode* node = model_->GetBookmarkBarNode();
  DCHECK(node && model_->other_node());

  // Create a button for each of the children on the bookmark bar.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    GtkToolItem* item = CreateBookmarkToolItem(node->GetChild(i));
    gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()), item, -1);
  }

  bookmark_utils::ConfigureButtonForNode(model_->other_node(),
      model_, other_bookmarks_button_, theme_provider_);

  SetInstructionState();
  SetChevronState();
}

void BookmarkBarGtk::SetInstructionState() {
  show_instructions_ = (model_->GetBookmarkBarNode()->GetChildCount() == 0);
  if (show_instructions_) {
    gtk_widget_show_all(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }
}

void BookmarkBarGtk::SetChevronState() {
  int extra_space = 0;

  if (GTK_WIDGET_VISIBLE(overflow_button_))
    extra_space = overflow_button_->allocation.width;

  int overflow_idx = GetFirstHiddenBookmark(extra_space);
  if (overflow_idx == -1)
    gtk_widget_hide(overflow_button_);
  else
    gtk_widget_show_all(overflow_button_);
}

void BookmarkBarGtk::RemoveAllBookmarkButtons() {
  gtk_util::RemoveAllChildren(bookmark_toolbar_.get());
}

int BookmarkBarGtk::GetBookmarkButtonCount() {
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_toolbar_.get()));
  int count = g_list_length(children);
  g_list_free(children);
  return count;
}

void BookmarkBarGtk::SetOverflowButtonAppearance() {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(overflow_button_));
  if (former_child)
    gtk_widget_destroy(former_child);

  GtkWidget* new_child = theme_provider_->UseGtkTheme() ?
      gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE) :
      gtk_image_new_from_pixbuf(ResourceBundle::GetSharedInstance().
          GetRTLEnabledPixbufNamed(IDR_BOOKMARK_BAR_CHEVRONS));

  gtk_container_add(GTK_CONTAINER(overflow_button_), new_child);
  SetChevronState();
}

int BookmarkBarGtk::GetFirstHiddenBookmark(int extra_space) {
  int rv = 0;
  bool overflow = false;
  GList* toolbar_items =
      gtk_container_get_children(GTK_CONTAINER(bookmark_toolbar_.get()));
  for (GList* iter = toolbar_items; iter; iter = g_list_next(iter)) {
    GtkWidget* tool_item = reinterpret_cast<GtkWidget*>(iter->data);
    if (tool_item->allocation.x + tool_item->allocation.width >
        bookmark_toolbar_.get()->allocation.width + extra_space) {
      overflow = true;
      break;
    }
    rv++;
  }

  g_list_free(toolbar_items);

  if (!overflow)
    return -1;

  return rv;
}

bool BookmarkBarGtk::IsAlwaysShown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

void BookmarkBarGtk::AnimationProgressed(const Animation* animation) {
  DCHECK_EQ(animation, slide_animation_.get());

  gint height = animation->GetCurrentValue() *
      (kBookmarkBarHeight - kBookmarkBarMinimumHeight) +
      kBookmarkBarMinimumHeight;
  gtk_widget_set_size_request(event_box_.get(), -1, height);
}

void BookmarkBarGtk::AnimationEnded(const Animation* animation) {
  DCHECK_EQ(animation, slide_animation_.get());

  if (!slide_animation_->IsShowing())
    gtk_widget_hide(bookmark_hbox_);
}

void BookmarkBarGtk::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED) {
    if (model_) {
      // Regenerate the bookmark bar with all new objects with their theme
      // properties set correctly for the new theme.
      RemoveAllBookmarkButtons();
      CreateAllBookmarkButtons();
    } else {
      DLOG(ERROR) << "Received a theme change notification while we "
                  << "don't have a BookmarkModel. Taking no action.";
    }

    // When using the GTK+ theme, we need to have the event box be visible so
    // buttons don't get a halo color from the background.  When using Chromium
    // themes, we want to let the background show through the toolbar.
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()),
                                     theme_provider_->UseGtkTheme());

    SetOverflowButtonAppearance();
  }
}

GtkWidget* BookmarkBarGtk::CreateBookmarkButton(const BookmarkNode* node) {
  GtkWidget* button = theme_provider_->BuildChromeButton();
  bookmark_utils::ConfigureButtonForNode(node, model_, button, theme_provider_);

  // The tool item is also a source for dragging
  gtk_drag_source_set(button, GDK_BUTTON1_MASK,
                      NULL, 0, GDK_ACTION_MOVE);
  int target_mask = GtkDndUtil::CHROME_BOOKMARK_ITEM;
  if (node->is_url())
    target_mask |= GtkDndUtil::TEXT_URI_LIST;
  GtkDndUtil::SetSourceTargetListFromCodeMask(button, target_mask);
  g_signal_connect(G_OBJECT(button), "drag-begin",
                   G_CALLBACK(&OnButtonDragBegin), this);
  g_signal_connect(G_OBJECT(button), "drag-end",
                   G_CALLBACK(&OnButtonDragEnd), this);
  g_signal_connect(G_OBJECT(button), "drag-data-get",
                   G_CALLBACK(&OnButtonDragGet), this);
  // We deliberately don't connect to "drag-data-delete" because the action of
  // moving a button will regenerate all the contents of the bookmarks bar
  // anyway.

  if (node->is_url()) {
    // Connect to 'button-release-event' instead of 'clicked' because we need
    // access to the modifier keys and we do different things on each
    // button.
    g_signal_connect(G_OBJECT(button), "button-press-event",
                     G_CALLBACK(OnButtonPressed), this);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(OnClicked), this);
    gtk_util::SetButtonTriggersNavigation(button);
  } else {
    // TODO(erg): This button can also be a drop target.
    ConnectFolderButtonEvents(button);
  }

  return button;
}

GtkToolItem* BookmarkBarGtk::CreateBookmarkToolItem(const BookmarkNode* node) {
  GtkWidget* button = CreateBookmarkButton(node);
  g_object_set_data(G_OBJECT(button), "left-align-popup",
                    reinterpret_cast<void*>(true));

  GtkToolItem* item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(item), button);
  gtk_widget_show_all(GTK_WIDGET(item));

  return item;
}

void BookmarkBarGtk::ConnectFolderButtonEvents(GtkWidget* widget) {
  gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, kDragAction);
  GtkDndUtil::SetDestTargetList(widget, kDestTargetList);
  g_signal_connect(widget, "drag-data-received",
                   G_CALLBACK(&OnDragReceived), this);

  // Connect to 'button-release-event' instead of 'clicked' because we need
  // access to the modifier keys and we do different things on each
  // button.
  g_signal_connect(G_OBJECT(widget), "button-press-event",
                   G_CALLBACK(OnButtonPressed), this);
  g_signal_connect(G_OBJECT(widget), "clicked",
                   G_CALLBACK(OnFolderClicked), this);

  ViewIDUtil::SetID(widget, VIEW_ID_BOOKMARK_MENU);
}

const BookmarkNode* BookmarkBarGtk::GetNodeForToolButton(GtkWidget* widget) {
  // First check to see if |button| is special cased.
  if (widget == other_bookmarks_button_)
    return model_->other_node();
  else if (widget == event_box_.get() || widget == overflow_button_)
    return model_->GetBookmarkBarNode();

  // Search the contents of |bookmark_toolbar_| for the corresponding widget
  // and find its index.
  GtkWidget* item_to_find = gtk_widget_get_parent(widget);
  int index_to_use = -1;
  int index = 0;
  GList* children = gtk_container_get_children(
      GTK_CONTAINER(bookmark_toolbar_.get()));
  for (GList* item = children; item; item = item->next, index++) {
    if (item->data == item_to_find) {
      index_to_use = index;
      break;
    }
  }
  g_list_free(children);

  if (index_to_use != -1)
    return model_->GetBookmarkBarNode()->GetChild(index_to_use);

  return NULL;
}

void BookmarkBarGtk::PopupMenuForNode(GtkWidget* sender,
                                      const BookmarkNode* node,
                                      GdkEventButton* event) {
  if (!model_->IsLoaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = NULL;
  std::vector<const BookmarkNode*> nodes;
  if (sender == other_bookmarks_button_) {
    parent = model_->GetBookmarkBarNode();
    nodes.push_back(parent);
  } else if (sender != bookmark_toolbar_.get()) {
    nodes.push_back(node);
    parent = node->GetParent();
  } else {
    parent = model_->GetBookmarkBarNode();
    nodes.push_back(parent);
  }

  current_context_menu_.reset(new BookmarkContextMenu(
                                  sender, profile_, browser_, page_navigator_,
                                  parent, nodes,
                                  BookmarkContextMenu::BOOKMARK_BAR));
  current_context_menu_->PopupAsContext(event->time);
}

// static
gboolean BookmarkBarGtk::OnButtonPressed(GtkWidget* sender,
                                         GdkEventButton* event,
                                         BookmarkBarGtk* bar) {
  if (event->button == 3) {
    const BookmarkNode* node = bar->GetNodeForToolButton(sender);
    DCHECK(node);
    DCHECK(bar->page_navigator_);
    bar->PopupMenuForNode(sender, node, event);
  }

  return FALSE;
}

// static
void BookmarkBarGtk::OnClicked(GtkWidget* sender,
                               BookmarkBarGtk* bar) {
  const BookmarkNode* node = bar->GetNodeForToolButton(sender);
  DCHECK(node);
  DCHECK(bar->page_navigator_);

  GdkEventButton* event =
      reinterpret_cast<GdkEventButton*>(gtk_get_current_event());

  if (node->is_url()) {
    bar->page_navigator_->OpenURL(
        node->GetURL(), GURL(),
        event_utils::DispositionFromEventFlags(event->state),
        PageTransition::AUTO_BOOKMARK);
  } else {
    bookmark_utils::OpenAll(
        sender, bar->profile_, bar->page_navigator_, node,
        event_utils::DispositionFromEventFlags(event->state));
  }

  UserMetrics::RecordAction(L"ClickedBookmarkBarURLButton", bar->profile_);
}

// static
void BookmarkBarGtk::OnButtonDragBegin(GtkWidget* button,
                                       GdkDragContext* drag_context,
                                       BookmarkBarGtk* bar) {
  // The parent tool item might be removed during the drag. Ref it so |button|
  // won't get destroyed.
  g_object_ref(button->parent);

  const BookmarkNode* node = bar->GetNodeForToolButton(button);
  DCHECK(!bar->dragged_node_);
  bar->dragged_node_ = node;
  DCHECK(bar->dragged_node_);

  GtkWidget* window = bookmark_utils::GetDragRepresentation(
      node, bar->model_, bar->theme_provider_);
  gint x, y;
  gtk_widget_get_pointer(button, &x, &y);
  gtk_drag_set_icon_widget(drag_context, window, x, y);

  // Hide our node.
  gtk_widget_hide(button);
}

// static
void BookmarkBarGtk::OnButtonDragEnd(GtkWidget* button,
                                     GdkDragContext* drag_context,
                                     BookmarkBarGtk* bar) {
  gtk_widget_show(button);

  if (bar->toolbar_drop_item_) {
    g_object_unref(bar->toolbar_drop_item_);
    bar->toolbar_drop_item_ = NULL;
  }

  DCHECK(bar->dragged_node_);
  bar->dragged_node_ = NULL;

  g_object_unref(button->parent);
}

// static
void BookmarkBarGtk::OnButtonDragGet(GtkWidget* widget, GdkDragContext* context,
                                     GtkSelectionData* selection_data,
                                     guint target_type, guint time,
                                     BookmarkBarGtk* bar) {
  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(widget);
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           bar->profile_);
}

// static
void BookmarkBarGtk::OnFolderClicked(GtkWidget* sender,
                                     BookmarkBarGtk* bar) {
  const BookmarkNode* node = bar->GetNodeForToolButton(sender);
  DCHECK(node);
  DCHECK(bar->page_navigator_);

  int start_child_idx = 0;
  if (sender == bar->overflow_button_)
    start_child_idx = bar->GetFirstHiddenBookmark(0);

  bar->current_menu_.reset(
      new BookmarkMenuController(bar->browser_, bar->profile_,
                                 bar->page_navigator_,
                                 GTK_WINDOW(gtk_widget_get_toplevel(sender)),
                                 node,
                                 start_child_idx,
                                 false));
  GdkEventButton* event =
      reinterpret_cast<GdkEventButton*>(gtk_get_current_event());
  bar->current_menu_->Popup(sender, event->button, event->time);
}

// static
gboolean BookmarkBarGtk::OnToolbarExpose(GtkWidget* widget,
                                         GdkEventExpose* event,
                                         BookmarkBarGtk* bar) {
  // A GtkToolbar's expose handler first draws a box. We don't want that so we
  // need to propagate the expose event to all the container's children.
  GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
  for (GList* item = children; item; item = item->next) {
    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   GTK_WIDGET(item->data),
                                   event);
  }
  g_list_free(children);

  return TRUE;
}

// static
gboolean BookmarkBarGtk::OnToolbarDragMotion(GtkToolbar* toolbar,
                                             GdkDragContext* context,
                                             gint x,
                                             gint y,
                                             guint time,
                                             BookmarkBarGtk* bar) {
  GdkAtom target_type =
      gtk_drag_dest_find_target(GTK_WIDGET(toolbar), context, NULL);
  if (target_type == GDK_NONE) {
    // We shouldn't act like a drop target when something that we can't deal
    // with is dragged over the toolbar.
    return FALSE;
  }

  if (!bar->toolbar_drop_item_) {
    if (bar->dragged_node_) {
      bar->toolbar_drop_item_ = bar->CreateBookmarkToolItem(bar->dragged_node_);
      g_object_ref_sink(GTK_OBJECT(bar->toolbar_drop_item_));
    } else {
      // Create a fake item the size of other_node().
      //
      // TODO(erg): Maybe somehow figure out the real size for the drop target?
      bar->toolbar_drop_item_ =
          bar->CreateBookmarkToolItem(bar->model_->other_node());
      g_object_ref_sink(GTK_OBJECT(bar->toolbar_drop_item_));
    }
  }

  if (bar->toolbar_drop_item_) {
    gint index = gtk_toolbar_get_drop_index(toolbar, x, y);
    gtk_toolbar_set_drop_highlight_item(toolbar,
                                        GTK_TOOL_ITEM(bar->toolbar_drop_item_),
                                        index);
  }

  if (target_type ==
      GtkDndUtil::GetAtomForTarget(GtkDndUtil::CHROME_BOOKMARK_ITEM)) {
    gdk_drag_status(context, GDK_ACTION_MOVE, time);
  } else {
    gdk_drag_status(context, GDK_ACTION_COPY, time);
  }

  return TRUE;
}

// static
void BookmarkBarGtk::OnToolbarDragLeave(GtkToolbar* toolbar,
                                        GdkDragContext* context,
                                        guint time,
                                        BookmarkBarGtk* bar) {
  if (bar->toolbar_drop_item_) {
    g_object_unref(bar->toolbar_drop_item_);
    bar->toolbar_drop_item_ = NULL;
  }

  gtk_toolbar_set_drop_highlight_item(toolbar, NULL, 0);
}

// static
void BookmarkBarGtk::OnToolbarSizeAllocate(GtkWidget* widget,
                                           GtkAllocation* allocation,
                                           BookmarkBarGtk* bar) {
  bar->SetChevronState();
}

// static
void BookmarkBarGtk::OnDragReceived(GtkWidget* widget,
                                    GdkDragContext* context,
                                    gint x, gint y,
                                    GtkSelectionData* selection_data,
                                    guint target_type, guint time,
                                    BookmarkBarGtk* bar) {
  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  const BookmarkNode* dest_node;
  gint index;
  if (widget == bar->bookmark_toolbar_.get()) {
    dest_node = bar->model_->GetBookmarkBarNode();
    index = gtk_toolbar_get_drop_index(
      GTK_TOOLBAR(bar->bookmark_toolbar_.get()), x, y);
  } else if (widget == bar->instructions_) {
    dest_node = bar->model_->GetBookmarkBarNode();
    index = 0;
  } else {
    dest_node = bar->GetNodeForToolButton(widget);
    index = dest_node->GetChildCount();
  }

  switch (target_type) {
    case GtkDndUtil::CHROME_BOOKMARK_ITEM: {
      std::vector<const BookmarkNode*> nodes =
          bookmark_utils::GetNodesFromSelection(context, selection_data,
                                                target_type,
                                                bar->profile_,
                                                &delete_selection_data,
                                                &dnd_success);
      DCHECK(!nodes.empty());
      for (std::vector<const BookmarkNode*>::iterator it = nodes.begin();
           it != nodes.end(); ++it) {
        bar->model_->Move(*it, dest_node, index);
        index = dest_node->IndexOfChild(*it) + 1;
      }
      break;
    }

    case GtkDndUtil::CHROME_NAMED_URL: {
      dnd_success = bookmark_utils::CreateNewBookmarkFromNamedUrl(
          selection_data, bar->model_, dest_node, index);
      break;
    }

    case GtkDndUtil::TEXT_URI_LIST: {
      dnd_success = bookmark_utils::CreateNewBookmarksFromURIList(
          selection_data, bar->model_, dest_node, index);
      break;
    }

    case GtkDndUtil::TEXT_PLAIN: {
      guchar* text = gtk_selection_data_get_text(selection_data);
      GURL url(reinterpret_cast<char*>(text));
      g_free(text);
      // TODO(estade): It would be nice to head this case off at drag motion,
      // so that it doesn't look like we can drag onto the bookmark bar.
      if (!url.is_valid())
        break;
      std::string title = bookmark_utils::GetNameForURL(url);
      bar->model_->AddURL(dest_node, index, UTF8ToWide(title), url);
      dnd_success = TRUE;
      break;
    }
  }

  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

// static
gboolean BookmarkBarGtk::OnEventBoxExpose(GtkWidget* widget,
                                          GdkEventExpose* event,
                                          BookmarkBarGtk* bar) {
  // Paint the background theme image.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_clip(cr);
  gfx::Point tabstrip_origin =
      bar->window_->tabstrip()->GetTabStripOriginForWidget(widget);

  GtkThemeProvider* theme_provider = bar->theme_provider_;
  GdkPixbuf* toolbar_background = theme_provider->GetPixbufNamed(
      IDR_THEME_TOOLBAR);
  gdk_cairo_set_source_pixbuf(cr, toolbar_background, tabstrip_origin.x(),
                              tabstrip_origin.y());
  // We tile the toolbar background in both directions.
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr,
      tabstrip_origin.x(),
      tabstrip_origin.y(),
      event->area.x + event->area.width - tabstrip_origin.x(),
      event->area.y + event->area.height - tabstrip_origin.y());
  cairo_fill(cr);
  cairo_destroy(cr);

  return FALSE;  // Propagate expose to children.
}

// static
gboolean BookmarkBarGtk::OnSeparatorExpose(GtkWidget* widget,
                                           GdkEventExpose* event,
                                           BookmarkBarGtk* bar) {
  if (bar->theme_provider_->UseGtkTheme())
    return FALSE;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                      event->area.width, event->area.height);
  cairo_clip(cr);

  GdkColor bottom_color =
      bar->theme_provider_->GetGdkColor(BrowserThemeProvider::COLOR_TOOLBAR);
  double bottom_color_rgb[] = {
      static_cast<double>(bottom_color.red / 257) / 255.0,
      static_cast<double>(bottom_color.green / 257) / 255.0,
      static_cast<double>(bottom_color.blue / 257) / 255.0, };

  cairo_pattern_t* pattern =
      cairo_pattern_create_linear(widget->allocation.x, widget->allocation.y,
                                  widget->allocation.x,
                                  widget->allocation.y +
                                  widget->allocation.height);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.0,
      kTopBorderColor[0], kTopBorderColor[1], kTopBorderColor[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.5,
      kSeparatorColor[0], kSeparatorColor[1], kSeparatorColor[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0,
      bottom_color_rgb[0], bottom_color_rgb[1], bottom_color_rgb[2]);
  cairo_set_source(cr, pattern);

  double start_x = 0.5 + widget->allocation.x;
  cairo_new_path(cr);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, start_x, widget->allocation.y);
  cairo_line_to(cr, start_x,
                    widget->allocation.y + widget->allocation.height);
  cairo_stroke(cr);
  cairo_destroy(cr);
  cairo_pattern_destroy(pattern);

  return TRUE;
}
