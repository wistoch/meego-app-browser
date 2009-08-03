// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

void SetEmptyDragIcon(GtkWidget* widget) {
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  gtk_drag_source_set_icon_pixbuf(widget, pixbuf);
  g_object_unref(pixbuf);
}

}  // namespace

class TabGtk::ContextMenuController : public MenuGtk::Delegate {
 public:
  explicit ContextMenuController(TabGtk* tab)
      : tab_(tab) {
    static const MenuCreateMaterial context_menu_blueprint[] = {
        { MENU_NORMAL, TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB,
            0, NULL, GDK_t, GDK_CONTROL_MASK, true },
        { MENU_SEPARATOR },
        { MENU_NORMAL, TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD,
            0, NULL, GDK_F5, 0, true },
        { MENU_NORMAL, TabStripModel::CommandDuplicate,
            IDS_TAB_CXMENU_DUPLICATE },
        { MENU_SEPARATOR },
        { MENU_NORMAL, TabStripModel::CommandCloseTab, IDS_TAB_CXMENU_CLOSETAB,
            0, NULL, GDK_w, GDK_CONTROL_MASK, true },
        { MENU_NORMAL, TabStripModel::CommandCloseOtherTabs,
          IDS_TAB_CXMENU_CLOSEOTHERTABS },
        { MENU_NORMAL, TabStripModel::CommandCloseTabsToRight,
            IDS_TAB_CXMENU_CLOSETABSTORIGHT },
        { MENU_NORMAL, TabStripModel::CommandCloseTabsOpenedBy,
            IDS_TAB_CXMENU_CLOSETABSOPENEDBY },
        { MENU_NORMAL, TabStripModel::CommandRestoreTab, IDS_RESTORE_TAB,
            0, NULL, GDK_t, GDK_CONTROL_MASK | GDK_SHIFT_MASK, true },
        { MENU_SEPARATOR },
        { MENU_NORMAL, TabStripModel::CommandTogglePinned,
            IDS_TAB_CXMENU_PIN_TAB },
        { MENU_END },
    };

    menu_.reset(new MenuGtk(this, context_menu_blueprint, NULL));
  }

  virtual ~ContextMenuController() {}

  void RunMenu() {
    menu_->PopupAsContext(gtk_get_current_event_time());
  }

  void Cancel() {
    tab_ = NULL;
    menu_->Cancel();
  }

 private:
  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const {
    if (!tab_)
      return false;

    return tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  virtual bool IsItemChecked(int command_id) const {
    if (!tab_ || command_id != TabStripModel::CommandTogglePinned)
      return false;
    return tab_->is_pinned();
  }

  virtual void ExecuteCommand(int command_id) {
    if (!tab_)
      return;

    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // The Tab the context menu was brought up for. Set to NULL when the menu
  // is canceled.
  TabGtk* tab_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

///////////////////////////////////////////////////////////////////////////////
// TabGtk, public:

TabGtk::TabGtk(TabDelegate* delegate)
    : TabRendererGtk(delegate->GetThemeProvider()),
      delegate_(delegate),
      closing_(false),
      dragging_(false) {
  event_box_ = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_), FALSE);
  gtk_drag_source_set(event_box_, GDK_BUTTON1_MASK,
                      NULL, 0, GDK_ACTION_MOVE);
  GtkDndUtil::SetSourceTargetListFromCodeMask(event_box_,
                                              GtkDndUtil::CHROME_TAB);
  g_signal_connect(G_OBJECT(event_box_), "button-press-event",
                   G_CALLBACK(OnMousePress), this);
  g_signal_connect(G_OBJECT(event_box_), "button-release-event",
                   G_CALLBACK(OnMouseRelease), this);
  g_signal_connect(G_OBJECT(event_box_), "enter-notify-event",
                   G_CALLBACK(OnEnterNotifyEvent), this);
  g_signal_connect(G_OBJECT(event_box_), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotifyEvent), this);
  g_signal_connect_after(G_OBJECT(event_box_), "drag-begin",
                           G_CALLBACK(OnDragBegin), this);
  g_signal_connect_after(G_OBJECT(event_box_), "drag-end",
                         G_CALLBACK(OnDragEnd), this);
  g_signal_connect_after(G_OBJECT(event_box_), "drag-failed",
                           G_CALLBACK(OnDragFailed), this);
  gtk_widget_add_events(event_box_,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_LEAVE_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  gtk_container_add(GTK_CONTAINER(event_box_), TabRendererGtk::widget());
  gtk_widget_show_all(event_box_);

  SetEmptyDragIcon(event_box_);
}

TabGtk::~TabGtk() {
  if (menu_controller_.get()) {
    // The menu is showing. Close the menu.
    menu_controller_->Cancel();

    // Invoke this so that we hide the highlight.
    ContextMenuClosed();
  }
}

// static
gboolean TabGtk::OnMousePress(GtkWidget* widget, GdkEventButton* event,
                              TabGtk* tab) {
  if (event->button == 1) {
    // Store whether or not we were selected just now... we only want to be
    // able to drag foreground tabs, so we don't start dragging the tab if
    // it was in the background.
    bool just_selected = !tab->IsSelected();
    if (just_selected) {
      tab->delegate_->SelectTab(tab);
    }
  } else if (event->button == 3) {
    tab->ShowContextMenu();
  }

  return TRUE;
}

// static
gboolean TabGtk::OnMouseRelease(GtkWidget* widget, GdkEventButton* event,
                                TabGtk* tab) {
  // Middle mouse up means close the tab, but only if the mouse is over it
  // (like a button).
  if (event->button == 2 &&
      event->x >= 0 && event->y >= 0 &&
      event->x < widget->allocation.width &&
      event->y < widget->allocation.height) {
    tab->delegate_->CloseTab(tab);
  }

  return TRUE;
}

// static
void TabGtk::OnDragBegin(GtkWidget* widget, GdkDragContext* context,
                         TabGtk* tab) {
  MessageLoopForUI::current()->AddObserver(tab);

  int x, y;
  gdk_window_get_pointer(tab->event_box_->window, &x, &y, NULL);

  // Make the mouse coordinate relative to the tab.
  x -= tab->bounds().x();
  y -= tab->bounds().y();

  tab->dragging_ = true;
  tab->delegate_->MaybeStartDrag(tab, gfx::Point(x, y));
}

// static
void TabGtk::OnDragEnd(GtkWidget* widget, GdkDragContext* context,
                       TabGtk* tab) {
  // Release our grab on the pointer.
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  gtk_grab_remove(tab->widget());

  tab->dragging_ = false;
  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  tab->delegate_->EndDrag(false);

  MessageLoopForUI::current()->RemoveObserver(tab);
}

// static
gboolean TabGtk::OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                              GtkDragResult result,
                              TabGtk* tab) {
  // TODO(jhawkins): Implement an EndDrag method that wraps up functionality
  // of OnDragEnd and OnDragFailed.  Take |result| into account for a canceled
  // drag action.
  OnDragEnd(widget, context, tab);
  return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, MessageLoop::Observer implementation:

void TabGtk::WillProcessEvent(GdkEvent* event) {
  // Nothing to do.
}

void TabGtk::DidProcessEvent(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY:
      delegate_->ContinueDrag(NULL);
      break;
    case GDK_GRAB_BROKEN:
      // If the user drags the mouse away from the dragged tab before the widget
      // is created, gtk loses the grab used for the drag and we're stuck in a
      // limbo where the drag is still active, but we don't get any
      // motion-notify-event signals.  Adding the grab back doesn't keep the
      // drag alive, but it does get us out of this bind by finishing the drag.
      if (delegate_->IsTabDetached(this)) {
        gdk_pointer_grab(widget()->window, FALSE, GDK_POINTER_MOTION_HINT_MASK,
                         NULL, NULL, GDK_CURRENT_TIME);
        gtk_grab_add(widget());
      }
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, TabRendererGtk overrides:

bool TabGtk::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

bool TabGtk::IsVisible() const {
  return GTK_WIDGET_FLAGS(event_box_) & GTK_VISIBLE;
}

void TabGtk::SetVisible(bool visible) const {
  if (visible) {
    gtk_widget_show(event_box_);
  } else {
    gtk_widget_hide(event_box_);
  }
}

void TabGtk::CloseButtonClicked() {
  delegate_->CloseTab(this);
}

void TabGtk::UpdateData(TabContents* contents, bool loading_only) {
  TabRendererGtk::UpdateData(contents, loading_only);
  std::wstring title = GetTitle();
  if (!title.empty()) {
    // Only show the tooltip if the title is truncated.
    gfx::Font font;
    if (font.GetStringWidth(title) > title_bounds().width()) {
      gtk_widget_set_tooltip_text(widget(), WideToUTF8(title).c_str());
    } else {
      gtk_widget_set_has_tooltip(widget(), FALSE);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, private:

void TabGtk::ShowContextMenu() {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu();
}

void TabGtk::ContextMenuClosed() {
  delegate()->StopAllHighlighting();
  menu_controller_.reset();
}
