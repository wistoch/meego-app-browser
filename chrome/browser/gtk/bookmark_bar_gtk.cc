// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bar_gtk.h"

#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/pickle.h"
#include "chrome/browser/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/bookmark_menu_controller_gtk.h"
#include "chrome/browser/gtk/bookmark_tree_model.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/dnd_registry.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The height of the bar.
const int kBookmarkBarHeight = 33;

// Maximum number of characters on a bookmark button.
const size_t kMaxCharsOnAButton = 15;

// Dictionary key used to store a BookmarkNode* on a GtkWidget.
const char kBookmarkNode[] = "bookmark-node";

// Mime types for DnD. Used to synchronize across applications.
const char kInternalURIType[] = "application/x-chrome-bookmark-item";

// Left-padding for the instructional text.
const int kInstructionsPadding = 6;

// Color of the button text, taken from TextButtonView.
const GdkColor kEnabledColor = GDK_COLOR_RGB(6, 45, 117);
const GdkColor kDisabledColor = GDK_COLOR_RGB(161, 161, 146);
// TextButtonView uses 255, 255, 255 with opacity of 200. We don't support
// transparent text though, so just use a slightly lighter version of
// kEnabledColor.
const GdkColor kHighlightColor = GDK_COLOR_RGB(56, 95, 167);

// Color of the instructional text.
const GdkColor kInstructionsColor = GDK_COLOR_RGB(128, 128, 142);

// Only used for the background of the drag widget.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);

// Recursively search for label among the children of |widget|.
void SearchForLabel(GtkWidget* widget, gpointer data) {
  if (GTK_IS_LABEL(widget)) {
    *reinterpret_cast<GtkWidget**>(data) = widget;
  } else if (GTK_IS_CONTAINER(widget)) {
    gtk_container_foreach(GTK_CONTAINER(widget), SearchForLabel, data);
  }
}

// This function is a temporary hack to fix fonts on dark system themes.
// NOTE: this makes assumptions about GtkButton internals. Also, it only works
// if you call it after the last time you edit the button.
// TODO(estade): remove this function.
void SetButtonTextColors(GtkWidget* button) {
  GtkWidget* label;
  gtk_container_foreach(GTK_CONTAINER(button), SearchForLabel, &label);
  if (label) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &kEnabledColor);
    gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, &kEnabledColor);
    gtk_widget_modify_fg(label, GTK_STATE_PRELIGHT, &kHighlightColor);
    gtk_widget_modify_fg(label, GTK_STATE_INSENSITIVE, &kDisabledColor);
  }
}

}  // namespace

BookmarkBarGtk::BookmarkBarGtk(Profile* profile, Browser* browser)
    : profile_(NULL),
      page_navigator_(NULL),
      browser_(browser),
      model_(NULL),
      instructions_(NULL),
      dragged_node_(NULL),
      toolbar_drop_item_(NULL),
      show_instructions_(true) {
  Init(profile);
  SetProfile(profile);
}

BookmarkBarGtk::~BookmarkBarGtk() {
  if (model_)
    model_->RemoveObserver(this);

  RemoveAllBookmarkButtons();
  bookmark_toolbar_.Destroy();
  bookmark_hbox_.Destroy();
}

void BookmarkBarGtk::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  RemoveAllBookmarkButtons();

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  gtk_widget_set_sensitive(other_bookmarks_button_, false);

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
  // Load the default images from the resource bundle.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* folder_icon = rb.GetPixbufNamed(IDR_BOOKMARK_BAR_FOLDER);

  bookmark_hbox_.Own(gtk_hbox_new(FALSE, 0));

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
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_.get()), instructions_,
                     FALSE, FALSE, 0);

  gtk_widget_set_app_paintable(bookmark_hbox_.get(), TRUE);
  g_signal_connect(G_OBJECT(bookmark_hbox_.get()), "expose-event",
                   G_CALLBACK(&OnHBoxExpose), this);

  bookmark_toolbar_.Own(gtk_toolbar_new());
  gtk_widget_set_app_paintable(bookmark_toolbar_.get(), TRUE);
  g_signal_connect(G_OBJECT(bookmark_toolbar_.get()), "expose-event",
                   G_CALLBACK(&OnToolbarExpose), this);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_.get()), bookmark_toolbar_.get(),
                     TRUE, TRUE, 0);

  gtk_drag_dest_set(bookmark_toolbar_.get(), GTK_DEST_DEFAULT_DROP,
                    bookmark_utils::kTargetTable,
                    bookmark_utils::kTargetTableSize,
                    GDK_ACTION_MOVE);
  g_signal_connect(bookmark_toolbar_.get(), "drag-motion",
                   G_CALLBACK(&OnToolbarDragMotion), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-leave",
                   G_CALLBACK(&OnToolbarDragLeave), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-drop",
                   G_CALLBACK(&OnToolbarDragDrop), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-data-received",
                   G_CALLBACK(&OnToolbarDragReceived), this);
  g_signal_connect(bookmark_toolbar_.get(), "button-press-event",
                   G_CALLBACK(&OnButtonPressed), this);

  gtk_box_pack_start(GTK_BOX(bookmark_hbox_.get()), gtk_vseparator_new(),
                     FALSE, FALSE, 0);

  other_bookmarks_button_ = gtk_chrome_button_new();
  ConnectFolderButtonEvents(other_bookmarks_button_);
  gtk_button_set_label(
      GTK_BUTTON(other_bookmarks_button_),
      l10n_util::GetStringUTF8(IDS_BOOMARK_BAR_OTHER_BOOKMARKED).c_str());
  gtk_button_set_image(GTK_BUTTON(other_bookmarks_button_),
                       gtk_image_new_from_pixbuf(folder_icon));
  // Set the proper text colors.
  SetButtonTextColors(other_bookmarks_button_);

  gtk_box_pack_start(GTK_BOX(bookmark_hbox_.get()), other_bookmarks_button_,
                     FALSE, FALSE, 0);
  gtk_widget_set_size_request(bookmark_hbox_.get(), -1, 0);

  slide_animation_.reset(new SlideAnimation(this));
}

void BookmarkBarGtk::AddBookmarkbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), bookmark_hbox_.get(), FALSE, FALSE, 0);
}

void BookmarkBarGtk::Show() {
  gtk_widget_show_all(bookmark_hbox_.get());
  slide_animation_->Show();

  // Maybe show the instructions
  if (show_instructions_) {
    gtk_widget_show(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }
}

void BookmarkBarGtk::Hide() {
  // Sometimes we get called without a matching call to open. If that happens
  // then force hide.
  if (slide_animation_->IsShowing())
    slide_animation_->Hide();
  else
    gtk_widget_hide(bookmark_hbox_.get());
}

int BookmarkBarGtk::GetHeight() {
  return bookmark_hbox_->allocation.height;
}

bool BookmarkBarGtk::IsClosing() {
  return slide_animation_->IsClosing();
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

  BookmarkNode* node = model_->GetBookmarkBarNode();
  DCHECK(node && model_->other_node());
  CreateAllBookmarkButtons(node);

  gtk_widget_set_sensitive(other_bookmarks_button_, true);
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
                                       BookmarkNode* old_parent,
                                       int old_index,
                                       BookmarkNode* new_parent,
                                       int new_index) {
  BookmarkNodeRemoved(model, old_parent, old_index);
  BookmarkNodeAdded(model, new_parent, new_index);
}

void BookmarkBarGtk::BookmarkNodeAdded(BookmarkModel* model,
                                       BookmarkNode* parent,
                                       int index) {
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index <= GetBookmarkButtonCount());

  gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()),
                     CreateBookmarkToolItem(parent->GetChild(index)),
                     index);

  SetInstructionState(parent);
}

void BookmarkBarGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                         BookmarkNode* parent,
                                         int index) {
  if (parent != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  DCHECK(index >= 0 && index < GetBookmarkButtonCount());

  GtkWidget* to_remove = GTK_WIDGET(gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), index));
  gtk_container_remove(GTK_CONTAINER(bookmark_toolbar_.get()),
                       to_remove);

  SetInstructionState(parent);
}

void BookmarkBarGtk::BookmarkNodeChanged(BookmarkModel* model,
                                         BookmarkNode* node) {
  if (node->GetParent() != model_->GetBookmarkBarNode()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model_->GetBookmarkBarNode()->IndexOfChild(node);
  DCHECK(index != -1);

  GtkToolItem* item = gtk_toolbar_get_nth_item(
      GTK_TOOLBAR(bookmark_toolbar_.get()), index);
  GtkWidget* button = gtk_bin_get_child(GTK_BIN(item));
  ConfigureButtonForNode(node, button);
}

void BookmarkBarGtk::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                               BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void BookmarkBarGtk::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                   BookmarkNode* node) {
  if (node != model_->GetBookmarkBarNode())
    return;  // We only care about reordering of the bookmark bar node.

  // Purge and rebuild the bar.
  RemoveAllBookmarkButtons();
  CreateAllBookmarkButtons(node);
}

void BookmarkBarGtk::CreateAllBookmarkButtons(BookmarkNode* node) {
  // Create a button for each of the children on the bookmark bar.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    GtkToolItem* item = CreateBookmarkToolItem(node->GetChild(i));
    gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()), item, -1);
  }

  SetInstructionState(node);
}

void BookmarkBarGtk::SetInstructionState(BookmarkNode* boomarks_bar_node) {
  show_instructions_ = (boomarks_bar_node->GetChildCount() == 0);
  if (show_instructions_) {
    gtk_widget_show_all(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }
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

bool BookmarkBarGtk::IsAlwaysShown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

void BookmarkBarGtk::AnimationProgressed(const Animation* animation) {
  DCHECK_EQ(animation, slide_animation_.get());

  gtk_widget_set_size_request(bookmark_hbox_.get(), -1,
                              animation->GetCurrentValue() *
                              kBookmarkBarHeight);
}

void BookmarkBarGtk::AnimationEnded(const Animation* animation) {
  DCHECK_EQ(animation, slide_animation_.get());

  if (!slide_animation_->IsShowing())
    gtk_widget_hide(bookmark_hbox_.get());
}

void BookmarkBarGtk::ConfigureButtonForNode(BookmarkNode* node,
                                            GtkWidget* button) {
  std::string tooltip = BuildTooltip(node);
  if (!tooltip.empty())
    gtk_widget_set_tooltip_text(button, tooltip.c_str());

  // TODO(erg): Consider a soft maximum instead of this hard 15.
  std::wstring title = node->GetTitle();
  title = title.substr(0, std::min(title.size(), kMaxCharsOnAButton));
  gtk_button_set_label(GTK_BUTTON(button), WideToUTF8(title).c_str());

  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model_);
  gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);

  SetButtonTextColors(button);
  g_object_set_data(G_OBJECT(button), kBookmarkNode,
                    reinterpret_cast<void*>(node));
}

GtkWidget* BookmarkBarGtk::CreateBookmarkButton(BookmarkNode* node) {
  GtkWidget* button = gtk_chrome_button_new();
  ConfigureButtonForNode(node, button);

  // The tool item is also a source for dragging
  gtk_drag_source_set(button, GDK_BUTTON1_MASK,
                      bookmark_utils::kTargetTable,
                      bookmark_utils::kTargetTableSize,
                      GDK_ACTION_MOVE);
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
    g_signal_connect(G_OBJECT(button), "button-release-event",
                     G_CALLBACK(OnButtonReleased), this);
  } else {
    // TODO(erg): This button can also be a drop target.
    ConnectFolderButtonEvents(button);
  }

  return button;
}

GtkToolItem* BookmarkBarGtk::CreateBookmarkToolItem(BookmarkNode* node) {
  GtkWidget* button = CreateBookmarkButton(node);
  g_object_set_data(G_OBJECT(button), "left-align-popup",
                    reinterpret_cast<void*>(true));

  GtkToolItem* item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(item), button);
  gtk_widget_show_all(GTK_WIDGET(item));

  return item;
}

void BookmarkBarGtk::ConnectFolderButtonEvents(GtkWidget* widget) {
  // Connect to 'button-release-event' instead of 'clicked' because we need
  // access to the modifier keys and we do different things on each
  // button.
  g_signal_connect(G_OBJECT(widget), "button-press-event",
                   G_CALLBACK(OnButtonPressed), this);
  g_signal_connect(G_OBJECT(widget), "button-release-event",
                   G_CALLBACK(OnFolderButtonReleased), this);
}

std::string BookmarkBarGtk::BuildTooltip(BookmarkNode* node) {
  // TODO(erg): Actually build the tooltip. For now, we punt and just return
  // the URL.
  return node->GetURL().possibly_invalid_spec();
}

BookmarkNode* BookmarkBarGtk::GetNodeForToolButton(GtkWidget* widget) {
  // First check to see if |button| is special cased.
  if (widget == other_bookmarks_button_)
    return model_->other_node();
  else if (widget == bookmark_toolbar_.get())
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

void BookmarkBarGtk::InitBackground() {
  if (background_ninebox_.get())
    return;

  background_ninebox_.reset(new NineBox(
      browser_->profile()->GetThemeProvider(),
      0, IDR_THEME_TOOLBAR, 0, 0, 0, 0, 0, 0, 0));
}

void BookmarkBarGtk::PopupMenuForNode(GtkWidget* sender, BookmarkNode* node,
                                      GdkEventButton* event) {
  if (!model_->IsLoaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  BookmarkNode* parent = NULL;
  std::vector<BookmarkNode*> nodes;
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
  if (sender != bar->other_bookmarks_button_ &&
      sender != bar->bookmark_toolbar_.get())
    bar->ignore_button_release_ = false;

  if (event->button == 3) {
    BookmarkNode* node = bar->GetNodeForToolButton(sender);
    DCHECK(node);
    DCHECK(bar->page_navigator_);
    bar->PopupMenuForNode(sender, node, event);
  }

  return FALSE;
}

// static
gboolean BookmarkBarGtk::OnButtonReleased(GtkWidget* sender,
                                          GdkEventButton* event,
                                          BookmarkBarGtk* bar) {
  if (bar->ignore_button_release_) {
    // Don't handle this message; it was a drag.
    bar->ignore_button_release_ = false;
    return FALSE;
  }

  BookmarkNode* node = bar->GetNodeForToolButton(sender);
  DCHECK(node);
  DCHECK(bar->page_navigator_);

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

  // Allow other handlers to run so the button state is updated correctly.
  return FALSE;
}

// static
void BookmarkBarGtk::OnButtonDragBegin(GtkWidget* button,
                                       GdkDragContext* drag_context,
                                       BookmarkBarGtk* bar) {
  // The parent tool item might be removed during the drag. Ref it so |button|
  // won't get destroyed.
  g_object_ref(button->parent);

  // Signal to any future OnButtonReleased calls that we're dragging instead of
  // pressing.
  bar->ignore_button_release_ = true;

  BookmarkNode* node = bar->GetNodeForToolButton(button);
  DCHECK(!bar->dragged_node_);
  bar->dragged_node_ = node;
  DCHECK(bar->dragged_node_);

  // Build a windowed representation for our button.
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &kBackgroundColor);
  gtk_widget_realize(window);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  gtk_widget_show(frame);

  GtkWidget* floating_button = gtk_chrome_button_new();
  bar->ConfigureButtonForNode(node, floating_button);
  gtk_container_add(GTK_CONTAINER(frame), floating_button);
  gtk_widget_show(floating_button);

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
  if (bar->toolbar_drop_item_) {
    g_object_unref(bar->toolbar_drop_item_);
    bar->toolbar_drop_item_ = NULL;
  }

  DCHECK(bar->dragged_node_);
  bar->dragged_node_ = NULL;
}

// static
void BookmarkBarGtk::OnButtonDragGet(GtkWidget* widget, GdkDragContext* context,
                                     GtkSelectionData* selection_data,
                                     guint target_type, guint time,
                                     BookmarkBarGtk* bar) {
  BookmarkNode* node =
      reinterpret_cast<BookmarkNode*>(
          g_object_get_data(G_OBJECT(widget), kBookmarkNode));
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           bar->profile_);
}

// static
gboolean BookmarkBarGtk::OnFolderButtonReleased(GtkWidget* sender,
                                                GdkEventButton* event,
                                                BookmarkBarGtk* bar) {
  if (bar->ignore_button_release_) {
    // Don't handle this message; it was a drag.
    bar->ignore_button_release_ = false;
    return FALSE;
  }

  BookmarkNode* node = bar->GetNodeForToolButton(sender);
  DCHECK(node);
  DCHECK(bar->page_navigator_);

  bar->current_menu_.reset(
      new BookmarkMenuController(bar->browser_, bar->profile_,
                                 bar->page_navigator_,
                                 GTK_WINDOW(gtk_widget_get_toplevel(sender)),
                                 node,
                                 0,
                                 false));
  bar->current_menu_->Popup(sender, event->button, event->time);

  // Allow other handlers to run so the button state is updated correctly.
  return FALSE;
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

  // TODO(estade): Improve support for drags from outside this particular
  // bookmark bar.
  if (!bar->toolbar_drop_item_ && bar->dragged_node_) {
    bar->toolbar_drop_item_ = bar->CreateBookmarkToolItem(bar->dragged_node_);
    g_object_ref_sink(GTK_OBJECT(bar->toolbar_drop_item_));
  }

  if (bar->toolbar_drop_item_) {
    gint index = gtk_toolbar_get_drop_index(toolbar, x, y);
    gtk_toolbar_set_drop_highlight_item(toolbar,
                                        GTK_TOOL_ITEM(bar->toolbar_drop_item_),
                                        index);
  }

  gdk_drag_status(context, GDK_ACTION_MOVE, time);

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
gboolean BookmarkBarGtk::OnToolbarDragDrop(
    GtkWidget* widget, GdkDragContext* context,
    gint x, gint y, guint time,
    BookmarkBarGtk* bar) {
  gboolean is_valid_drop_site = FALSE;

  if (context->targets) {
    GdkAtom target_type =
        GDK_POINTER_TO_ATOM(g_list_nth_data(context->targets,
                                            dnd::X_CHROME_BOOKMARK_ITEM));
    gtk_drag_get_data(widget, context, target_type, time);

    is_valid_drop_site = TRUE;
  }

  return is_valid_drop_site;
}

// static
void BookmarkBarGtk::OnToolbarDragReceived(GtkWidget* widget,
                                           GdkDragContext* context,
                                           gint x, gint y,
                                           GtkSelectionData* selection_data,
                                           guint target_type, guint time,
                                           BookmarkBarGtk* bar) {
  gboolean dnd_success = FALSE;
  gboolean delete_selection_data = FALSE;

  std::vector<BookmarkNode*> nodes =
      bookmark_utils::GetNodesFromSelection(context, selection_data,
                                            target_type,
                                            bar->profile_,
                                            &delete_selection_data,
                                            &dnd_success);
  DCHECK(!nodes.empty());
  for (std::vector<BookmarkNode*>::iterator it = nodes.begin();
       it != nodes.end(); ++it) {
    gint index = gtk_toolbar_get_drop_index(
        GTK_TOOLBAR(bar->bookmark_toolbar_.get()), x, y);
    bar->model_->Move(*it, bar->model_->GetBookmarkBarNode(), index++);
  }

  gtk_drag_finish(context, dnd_success, delete_selection_data, time);
}

// static
gboolean BookmarkBarGtk::OnHBoxExpose(GtkWidget* widget,
                                      GdkEventExpose* event,
                                      BookmarkBarGtk* bar) {
  // Paint the background theme image.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_clip(cr);
  bar->InitBackground();
  bar->background_ninebox_->RenderTopCenterStrip(cr, event->area.x,
                                                 0, event->area.width);
  cairo_destroy(cr);

  return FALSE;  // Propagate expose to children.
}
