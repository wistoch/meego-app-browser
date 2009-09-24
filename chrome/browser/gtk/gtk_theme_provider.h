// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
#define CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "app/gfx/color_utils.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/owned_widget_gtk.h"

class CairoCachedSurface;
class Profile;

typedef struct _GdkDisplay GdkDisplay;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

// Specialization of BrowserThemeProvider which supplies system colors.
class GtkThemeProvider : public BrowserThemeProvider,
                         public NotificationObserver {
 public:
  // Returns GtkThemeProvider, casted from our superclass.
  static GtkThemeProvider* GetFrom(Profile* profile);

  GtkThemeProvider();
  virtual ~GtkThemeProvider();

  // Calls |observer|.Observe() for the browser theme with this provider as the
  // source.
  void InitThemesFor(NotificationObserver* observer);

  // Overridden from BrowserThemeProvider:
  //
  // Sets that we aren't using the system theme, then calls
  // BrowserThemeProvider's implementation.
  virtual void Init(Profile* profile);
  virtual void SetTheme(Extension* extension);
  virtual void UseDefaultTheme();
  virtual void SetNativeTheme();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Creates a GtkChromeButton instance, registered with this theme provider,
  // with a "destroy" signal to remove it from our internal list when it goes
  // away.
  GtkWidget* BuildChromeButton();

  // Whether we should use the GTK system theme.
  bool UseGtkTheme();

  // A wrapper around ThemeProvider::GetColor, transforming the result to a
  // GdkColor.
  GdkColor GetGdkColor(int id);

  // A weighted average between the text color and the background color of a
  // label. Used for borders between GTK stuff and the webcontent.
  GdkColor GetBorderColor();

  // Expose the inner widgets. Only used for testing.
  GtkWidget* fake_window() { return fake_window_; }
  GtkWidget* fake_label() { return fake_label_.get(); }

  // Returns a CairoCachedSurface for a particular Display. CairoCachedSurfaces
  // (hopefully) live on the X server, instead of the client so we don't have
  // to send the image to the server on each expose.
  CairoCachedSurface* GetSurfaceNamed(int id, GtkWidget* widget_on_display);

  // These functions do not add a ref to the returned pixbuf, and it should not be
  // unreffed.
  // If |native| is true, get the GTK_STOCK version of the icon.
  static GdkPixbuf* GetFolderIcon(bool native);
  static GdkPixbuf* GetDefaultFavicon(bool native);

 protected:
  // Possibly creates a theme specific version of theme_toolbar_default.
  // (minimally acceptable version right now, which is just a fill of the bg
  // color; this should instead invoke gtk_draw_box(...) for complex theme
  // engines.)
  virtual SkBitmap* LoadThemeBitmap(int id);

 private:
  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // If use_gtk_ is true, completely ignores this call. Otherwise passes it to
  // the superclass.
  virtual void SaveThemeBitmap(const std::string resource_name, int id);

  // Additionally frees the CairoCachedSurfaces.
  virtual void FreePlatformCaches();

  // Handles signal from GTK that our theme has been changed.
  static void OnStyleSet(GtkWidget* widget,
                         GtkStyle* previous_style,
                         GtkThemeProvider* provider);

  void LoadGtkValues();

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(const char* id, GdkColor* color);
  void SetThemeTintFromGtk(const char* id, GdkColor* color,
                           const color_utils::HSL& default_tint);

  // Split out from FreePlatformCaches so it can be called in our destructor;
  // FreePlatformCaches() is called from the BrowserThemeProvider's destructor,
  // but by the time ~BrowserThemeProvider() is run, the vtable no longer
  // points to GtkThemeProvider's version.
  void FreePerDisplaySurfaces();

  // A notification from the GtkChromeButton GObject destructor that we should
  // remove it from our internal list.
  static void OnDestroyChromeButton(GtkWidget* button,
                                    GtkThemeProvider* provider);

  // Whether we should be using gtk rendering.
  bool use_gtk_;

  // GtkWidgets that exist only so we can look at their properties (and take
  // their colors).
  GtkWidget* fake_window_;
  OwnedWidgetGtk fake_label_;

  // A list of all GtkChromeButton instances. We hold on to these to notify
  // them of theme changes.
  std::vector<GtkWidget*> chrome_buttons_;

  // Cairo surfaces for each GdkDisplay.
  typedef std::map<int, CairoCachedSurface*> CairoCachedSurfaceMap;
  typedef std::map<GdkDisplay*, CairoCachedSurfaceMap> PerDisplaySurfaceMap;
  PerDisplaySurfaceMap per_display_surfaces_;

  // This is a dummy widget that only exists so we have something to pass to
  // gtk_widget_render_icon().
  static GtkWidget* icon_widget_;

  // The default folder icon and default bookmark icon for the GTK theme.
  // These are static because the system can only have one theme at a time.
  // They are cached when they are requested the first time, and cleared when
  // the system theme changes.
  static GdkPixbuf* default_folder_icon_;
  static GdkPixbuf* default_bookmark_icon_;
};

#endif  // CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
