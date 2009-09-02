// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_

#include "app/gfx/canvas.h"
#include "app/slide_animation.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/views/browser_bubble.h"
#include "views/view.h"

class Browser;
namespace views {
  class Label;
  class MouseEvent;
}

// A shelf that contains Extension toolstrips.
class ExtensionShelf : public views::View,
                       public ExtensionContainer,
                       public ExtensionShelfModelObserver,
                       public AnimationDelegate,
                       public NotificationObserver {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // Get the current model.
  ExtensionShelfModel* model() { return model_; }

  // Returns whether the extension shelf is detached from the Chrome frame.
  bool IsDetachedStyle();

  // Toggles a preference for whether to always show the extension shelf.
  static void ToggleWhenExtensionShelfVisible(Profile* profile);

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void SetAccessibleName(const std::wstring& name);

  // ExtensionContainer
  virtual void OnExtensionMouseEvent(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);

  // ExtensionShelfModelObserver
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripDraggingFrom(ExtensionHost* toolstrip, int index);
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index);
  virtual void ToolstripChanged(ExtensionShelfModel::iterator toolstrip);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();
  virtual void ShelfModelDeleting();

  // AnimationDelegate
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // View
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  class Toolstrip;
  friend class Toolstrip;
  class PlaceholderView;

  // Dragging toolstrips
  void DropExtension(Toolstrip* handle, const gfx::Point& pt, bool cancel);

  // Expand the specified toolstrip, navigating to |url| if non-empty,
  // and setting the |height|.
  void ExpandToolstrip(ExtensionHost* host, const GURL& url, int height);

  // Collapse the specified toolstrip, navigating to |url| if non-empty.
  void CollapseToolstrip(ExtensionHost* host, const GURL& url);

  // Inits the background bitmap.
  void InitBackground(gfx::Canvas* canvas, const SkRect& subset);

  // Returns the Toolstrip at |x| coordinate.  If |x| is out of bounds, returns
  // NULL.
  Toolstrip* ToolstripAtX(int x);

  // Returns the Toolstrip at |index|.
  Toolstrip* ToolstripAtIndex(int index);

  // Returns the toolstrip associated with |view|.
  Toolstrip* ToolstripForView(ExtensionView* view);

  // Loads initial state from |model_|.
  void LoadFromModel();

  // This method computes the bounds for the extension shelf items. If
  // |compute_bounds_only| = TRUE, the bounds for the items are just computed,
  // but are not set. This mode is used by GetPreferredSize() to obtain the
  // desired bounds. If |compute_bounds_only| = FALSE, the bounds are set.
  gfx::Size LayoutItems(bool compute_bounds_only);

  // Returns whether the extension shelf always shown (checks pref value).
  bool IsAlwaysShown();

  // Returns whether the extension shelf is being displayed over the new tab
  // page.
  bool OnNewTabPage();

  NotificationRegistrar registrar_;

  // Background bitmap to draw under extension views.
  SkBitmap background_;

  // The browser this extension shelf belongs to.
  Browser* browser_;

  // The model representing the toolstrips on the shelf.
  ExtensionShelfModel* model_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // Animation controlling showing and hiding of the shelf.
  scoped_ptr<SlideAnimation> size_animation_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
