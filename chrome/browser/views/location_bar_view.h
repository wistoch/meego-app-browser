// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_

#include <string>
#include <map>
#include <vector>

#include "base/task.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/native/native_view_host.h"
#include "views/painter.h"

#if defined(OS_WIN)
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#else
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#endif

class Browser;
class CommandUpdater;
class ContentSettingImageModel;
class ExtensionAction;
class ExtensionPopup;
class GURL;
class Profile;

/////////////////////////////////////////////////////////////////////////////
//
// LocationBarView class
//
//   The LocationBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
/////////////////////////////////////////////////////////////////////////////
class LocationBarView : public LocationBar,
                        public LocationBarTesting,
                        public views::View,
                        public views::DragController,
                        public AutocompleteEditController {
 public:
  class Delegate {
   public:
    // Should return the current tab contents.
    virtual TabContents* GetTabContents() = 0;

    // Called by the location bar view when the user starts typing in the edit.
    // This forces our security style to be UNKNOWN for the duration of the
    // editing.
    virtual void OnInputInProgress(bool in_progress) = 0;
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    SELECTED_TEXT,
    DEEMPHASIZED_TEXT,
    SECURITY_TEXT,
  };

  LocationBarView(Profile* profile,
                  CommandUpdater* command_updater,
                  ToolbarModel* model,
                  Delegate* delegate,
                  bool popup_window_mode);
  virtual ~LocationBarView();

  void Init();

  // Returns whether this instance has been initialized by callin Init. Init can
  // only be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Returns the appropriate color for the desired kind, based on the user's
  // system theme.
  static SkColor GetColor(ToolbarModel::SecurityLevel security_level,
                          ColorKind kind);

  // Updates the location bar.  We also reset the bar's permanent text and
  // security style, and, if |tab_for_state_restoring| is non-NULL, also restore
  // saved state that the tab holds.
  void Update(const TabContents* tab_for_state_restoring);

  void SetProfile(Profile* profile);
  Profile* profile() const { return profile_; }

  // Returns the current TabContents.
  TabContents* GetTabContents() const;

  // Sets |preview_enabled| for the PageAction View associated with this
  // |page_action|. If |preview_enabled| is true, the view will display the
  // PageActions icon even though it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction *page_action,
                                   bool preview_enabled);

  // Retrieves the PageAction View which is associated with |page_action|.
  views::View* GetPageActionView(ExtensionAction* page_action);

  // Toggles the star on or off.
  void SetStarToggled(bool on);

  // Shows the bookmark bubble.
  void ShowStarBubble(const GURL& url, bool newly_bookmarked);

  // Sizing functions
  virtual gfx::Size GetPreferredSize();

  // Layout and Painting functions
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);

  // No focus border for the location bar, the caret is enough.
  virtual void PaintFocusBorder(gfx::Canvas* canvas) { }

  // Called when any ancestor changes its size, asks the AutocompleteEditModel
  // to close its popup.
  virtual void VisibleBoundsInRootChanged();

#if defined(OS_WIN)
  // Event Handlers
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
#endif

  // AutocompleteEditController
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    PageTransition::Type transition,
                                    const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual void OnKillFocus();
  virtual void OnSetFocus();
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

  // Overridden from views::View:
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);

  // Overridden from views::DragController:
  virtual void WriteDragData(View* sender,
                             const gfx::Point& press_pt,
                             OSExchangeData* data);
  virtual int GetDragOperations(View* sender, const gfx::Point& p);
  virtual bool CanStartDrag(View* sender,
                            const gfx::Point& press_pt,
                            const gfx::Point& p);

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(FirstRun::BubbleType bubble_type);
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdateContentSettingsIcons();
  virtual void UpdatePageActions();
  virtual void InvalidatePageActions();
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual const AutocompleteEditView* location_entry() const {
    return location_entry_.get();
  }
  virtual AutocompleteEditView* location_entry() {
    return location_entry_.get();
  }
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Overridden from LocationBarTesting:
  virtual int PageActionCount() { return page_action_views_.size(); }
  virtual int PageActionVisibleCount();
  virtual ExtensionAction* GetPageAction(size_t index);
  virtual ExtensionAction* GetVisiblePageAction(size_t index);
  virtual void TestPageActionPressed(size_t index);

  static const int kVertMargin;

 protected:
  void Focus();

 private:
  // This helper class is kept as a member by classes that need to show the Page
  // Info dialog on click, to encapsulate that logic in one place.
  class ClickHandler {
   public:
    explicit ClickHandler(const views::View* owner,
                          const LocationBarView* location_bar);

    void OnMouseReleased(const views::MouseEvent& event, bool canceled);

   private:
    const views::View* owner_;
    const LocationBarView* location_bar_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ClickHandler);
  };

  // LocationIconView is used to display an icon to the left of the edit field.
  // This shows the user's current action while editing, the page security
  // status on https pages, or a globe for other URLs.
  class LocationIconView : public views::ImageView {
   public:
    explicit LocationIconView(const LocationBarView* location_bar);
    virtual ~LocationIconView();

    // Overridden from view.
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

   private:
    ClickHandler click_handler_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(LocationIconView);
  };

  // View used to draw a bubble to the left of the address, containing an icon
  // and a label.  We use this as a base for the classes that handle the EV
  // bubble and tab-to-search UI.
  class IconLabelBubbleView : public views::View {
   public:
    IconLabelBubbleView(const int background_images[],
                        int contained_image,
                        const SkColor& color);
    virtual ~IconLabelBubbleView();

    void SetFont(const gfx::Font& font);
    void SetLabel(const std::wstring& label);

    virtual void Paint(gfx::Canvas* canvas);
    virtual gfx::Size GetPreferredSize();
    virtual void Layout();

   protected:
     gfx::Size GetNonLabelSize();

   private:
    // For painting the background.
    views::HorizontalPainter background_painter_;

    // The contents of the bubble.
    views::ImageView image_;
    views::Label label_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(IconLabelBubbleView);
  };

  // EVBubbleView displays the EV Bubble.
  class EVBubbleView : public IconLabelBubbleView {
   public:
    EVBubbleView(const int background_images[],
                 int contained_image,
                 const SkColor& color,
                 const LocationBarView* location_bar);
    virtual ~EVBubbleView();

    // Overridden from view.
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

   private:
    ClickHandler click_handler_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(EVBubbleView);
  };

  // SelectedKeywordView displays the tab-to-search UI.
  class SelectedKeywordView : public IconLabelBubbleView {
   public:
    SelectedKeywordView(const int background_images[],
                        int contained_image,
                        const SkColor& color,
                        Profile* profile);
    virtual ~SelectedKeywordView();

    void SetFont(const gfx::Font& font);

    virtual gfx::Size GetPreferredSize();
    virtual gfx::Size GetMinimumSize();
    virtual void Layout();

    // The current keyword, or an empty string if no keyword is displayed.
    void SetKeyword(const std::wstring& keyword);
    std::wstring keyword() const { return keyword_; }

    void set_profile(Profile* profile) { profile_ = profile; }

   private:
    // Returns the truncated version of description to use.
    std::wstring CalculateMinString(const std::wstring& description);

    // The keyword we're showing. If empty, no keyword is selected.
    // NOTE: we don't cache the TemplateURL as it is possible for it to get
    // deleted out from under us.
    std::wstring keyword_;

    // These labels are never visible.  They are used to size the view.  One
    // label contains the complete description of the keyword, the second
    // contains a truncated version of the description, for if there is not
    // enough room to display the complete description.
    views::Label full_label_;
    views::Label partial_label_;

    Profile* profile_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(SelectedKeywordView);
  };

  // KeywordHintView is used to display a hint to the user when the selected
  // url has a corresponding keyword.
  //
  // Internally KeywordHintView uses two labels to render the text, and draws
  // the tab image itself.
  //
  // NOTE: This should really be called LocationBarKeywordHintView, but I
  // couldn't bring myself to use such a long name.
  class KeywordHintView : public views::View {
   public:
    explicit KeywordHintView(Profile* profile);
    virtual ~KeywordHintView();

    void SetFont(const gfx::Font& font);

    void SetColor(const SkColor& color);

    void SetKeyword(const std::wstring& keyword);
    std::wstring keyword() const { return keyword_; }

    virtual void Paint(gfx::Canvas* canvas);
    virtual gfx::Size GetPreferredSize();
    // The minimum size is just big enough to show the tab.
    virtual gfx::Size GetMinimumSize();
    virtual void Layout();

    void set_profile(Profile* profile) { profile_ = profile; }

   private:
    views::Label leading_label_;
    views::Label trailing_label_;

    // The keyword.
    std::wstring keyword_;

    Profile* profile_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(KeywordHintView);
  };

  class ContentSettingImageView : public views::ImageView,
                                  public InfoBubbleDelegate {
   public:
    ContentSettingImageView(ContentSettingsType content_type,
                            const LocationBarView* parent,
                            Profile* profile);
    virtual ~ContentSettingImageView();

    void set_profile(Profile* profile) { profile_ = profile; }
    void UpdateFromTabContents(const TabContents* tab_contents);

   private:
    // views::ImageView overrides:
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
    virtual void VisibilityChanged(View* starting_from, bool is_visible);

    // InfoBubbleDelegate overrides:
    virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                   bool closed_by_escape);
    virtual bool CloseOnEscape();

    scoped_ptr<ContentSettingImageModel> content_setting_image_model_;

    // The owning LocationBarView.
    const LocationBarView* parent_;

    // The currently active profile.
    Profile* profile_;

    // The currently shown info bubble if any.
    InfoBubble* info_bubble_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingImageView);
  };
  typedef std::vector<ContentSettingImageView*> ContentSettingViews;

  // PageActionImageView is used to display the icon for a given PageAction
  // and notify the extension when the icon is clicked.
  class PageActionImageView : public views::ImageView,
      public ImageLoadingTracker::Observer,
      public ExtensionContextMenuModel::PopupDelegate,
      public ExtensionPopup::Observer {
   public:
    PageActionImageView(LocationBarView* owner,
                        Profile* profile,
                        ExtensionAction* page_action);
    virtual ~PageActionImageView();

    ExtensionAction* page_action() { return page_action_; }

    int current_tab_id() { return current_tab_id_; }

    void set_preview_enabled(bool preview_enabled) {
      preview_enabled_ = preview_enabled;
    }

    // Overridden from view.
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

    // Overridden from ImageLoadingTracker.
    virtual void OnImageLoaded(
        SkBitmap* image, ExtensionResource resource, int index);

    // Overridden from ExtensionContextMenuModelModel::Delegate
    virtual void InspectPopup(ExtensionAction* action);

    // Overridden from ExtensionPopup::Observer
    virtual void ExtensionPopupClosed(ExtensionPopup* popup);

    // Called to notify the PageAction that it should determine whether to be
    // visible or hidden. |contents| is the TabContents that is active, |url|
    // is the current page URL.
    void UpdateVisibility(TabContents* contents, const GURL& url);

    // Either notify listeners or show a popup depending on the page action.
    void ExecuteAction(int button, bool inspect_with_devtools);

   private:
    // Hides the active popup, if there is one.
    void HidePopup();

    // The location bar view that owns us.
    LocationBarView* owner_;

    // The current profile (not owned by us).
    Profile* profile_;

    // The PageAction that this view represents. The PageAction is not owned by
    // us, it resides in the extension of this particular profile.
    ExtensionAction* page_action_;

    // A cache of bitmaps the page actions might need to show, mapped by path.
    typedef std::map<std::string, SkBitmap> PageActionMap;
    PageActionMap page_action_icons_;

    // The context menu for this page action.
    scoped_refptr<ExtensionContextMenuModel> context_menu_contents_;
    scoped_ptr<views::Menu2> context_menu_menu_;

    // The object that is waiting for the image loading to complete
    // asynchronously.
    ImageLoadingTracker tracker_;

    // The tab id we are currently showing the icon for.
    int current_tab_id_;

    // The URL we are currently showing the icon for.
    GURL current_url_;

    // The string to show for a tooltip;
    std::string tooltip_;

    // This is used for post-install visual feedback. The page_action icon
    // is briefly shown even if it hasn't been enabled by it's extension.
    bool preview_enabled_;

    // The current popup and the button it came from.  NULL if no popup.
    ExtensionPopup* popup_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(PageActionImageView);
  };
  friend class PageActionImageView;

  class PageActionWithBadgeView;
  friend class PageActionWithBadgeView;
  typedef std::vector<PageActionWithBadgeView*> PageActionViews;

  class StarView : public views::ImageView, public InfoBubbleDelegate {
   public:
    explicit StarView(CommandUpdater* command_updater);
    virtual ~StarView();

    // Toggles the star on or off.
    void SetToggled(bool on);

   private:
    // views::ImageView overrides:
    virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);

    // InfoBubbleDelegate overrides:
    virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                   bool closed_by_escape);
    virtual bool CloseOnEscape();

    // The CommandUpdater for the Browser object that owns the location bar.
    CommandUpdater* command_updater_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(StarView);
  };

  // Returns the height in pixels of the margin at the top of the bar.
  int TopMargin() const;

  // Returns the amount of horizontal space (in pixels) out of
  // |location_bar_width| that is not taken up by the actual text in
  // location_entry_.
  int AvailableWidth(int location_bar_width);

  // Returns whether the |available_width| is large enough to contain a view
  // with preferred width |pref_width| at its preferred size. If this returns
  // true, the preferred size should be used. If this returns false, the
  // minimum size of the view should be used.
  bool UsePref(int pref_width, int available_width);

  // If View fits in the specified region, it is made visible and the
  // bounds are adjusted appropriately. If the View does not fit, it is
  // made invisible.
  void LayoutView(bool leading, views::View* view, int available_width,
                  gfx::Rect* bounds);

  // Update the visibility state of the Content Blocked icons to reflect what is
  // actually blocked on the current page.
  void RefreshContentSettingViews();

  // Delete all page action views that we have created.
  void DeletePageActionViews();

  // Update the views for the Page Actions, to reflect state changes for
  // PageActions.
  void RefreshPageActionViews();

  // Sets the visibility of view to new_vis.
  void ToggleVisibility(bool new_vis, views::View* view);

#if defined(OS_WIN)
  // Helper for the Mouse event handlers that does all the real work.
  void OnMouseEvent(const views::MouseEvent& event, UINT msg);
#endif

  // Helper to show the first run info bubble.
  void ShowFirstRunBubbleInternal(FirstRun::BubbleType bubble_type);

  // Current browser. Not owned by us.
  Browser* browser_;

  // Current profile. Not owned by us.
  Profile* profile_;

  // The Autocomplete Edit field.
#if defined(OS_WIN)
  scoped_ptr<AutocompleteEditViewWin> location_entry_;
#else
  scoped_ptr<AutocompleteEditViewGtk> location_entry_;
#endif

  // The CommandUpdater for the Browser object that corresponds to this View.
  CommandUpdater* command_updater_;

  // The model.
  ToolbarModel* model_;

  // Our delegate.
  Delegate* delegate_;

  // This is the string of text from the autocompletion session that the user
  // entered or selected.
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation
  PageTransition::Type transition_;

  // Font used by edit and some of the hints.
  gfx::Font font_;

  // An icon to the left of the edit field.
  LocationIconView location_icon_view_;

  // A bubble displayed for EV HTTPS sites.
  EVBubbleView ev_bubble_view_;

  // Location_entry view wrapper
  views::NativeViewHost* location_entry_view_;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_.
  // These autocollapse when the edit needs the room.

  // Shown if the user has selected a keyword.
  SelectedKeywordView selected_keyword_view_;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView keyword_hint_view_;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The page action icon views.
  PageActionViews page_action_views_;

  // The star.
  StarView star_view_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // Used schedule a task for the first run info bubble.
  ScopedRunnableMethodFactory<LocationBarView> first_run_bubble_;

  // Storage of string needed for accessibility.
  std::wstring accessible_name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationBarView);
};

#endif  // CHROME_BROWSER_VIEWS_LOCATION_BAR_VIEW_H_
