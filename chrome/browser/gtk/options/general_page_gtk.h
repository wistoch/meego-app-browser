// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/gtk_signal.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/shell_integration.h"
#include "googleurl/src/gurl.h"

class AccessibleWidgetHelper;
class Profile;
class ListStoreFavIconLoader;

class GeneralPageGtk : public OptionsPageBase,
                       public TemplateURLModelObserver,
                       public ShellIntegration::DefaultBrowserObserver {
 public:
  explicit GeneralPageGtk(Profile* profile);
  ~GeneralPageGtk();

  GtkWidget* get_page_widget() const { return page_; }

 private:
  // Overridden from OptionsPageBase
  virtual void NotifyPrefChanged(const std::wstring* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // Initialize the option group widgets, return their container
  GtkWidget* InitStartupGroup();
  GtkWidget* InitHomepageGroup();
  GtkWidget* InitDefaultSearchGroup();
  GtkWidget* InitDefaultBrowserGroup();

  // Saves the startup preference from the values in the ui
  void SaveStartupPref();

  // Fill the startup_custom_pages_model_
  void PopulateCustomUrlList(const std::vector<GURL>& urls);

  // Fill a single row in the startup_custom_pages_model_
  void PopulateCustomUrlRow(const GURL& url, GtkTreeIter *iter);

  // Set the custom url list using the pages currently open
  void SetCustomUrlListFromCurrentPages();

  // Callback from UrlPickerDialogGtk, for adding custom urls manually.
  // If a single row in the list is selected, the new url will be inserted
  // before that row.  Otherwise the new row will be added to the end.
  void OnAddCustomUrl(const GURL& url);

  // Removes urls that are currently selected
  void RemoveSelectedCustomUrls();

  // Retrieve entries from the startup_custom_pages_model_
  std::vector<GURL> GetCustomUrlList() const;

  // Overridden from TemplateURLModelObserver.
  // Populates the default search engine combobox from the model.
  virtual void OnTemplateURLModelChanged();

  // Set the default search engine pref to the combo box active item.
  void SetDefaultSearchEngineFromComboBox();

  // Set the default search engine combo box state.
  void EnableDefaultSearchEngineComboBox(bool enable);

  // Sets the home page preferences for kNewTabPageIsHomePage and kHomePage.
  // If a blank string is passed in we revert to using NewTab page as the Home
  // page. When setting the Home Page to NewTab page, we preserve the old value
  // of kHomePage (we don't overwrite it).
  void SetHomepage(const GURL& homepage);

  // Sets the home page pref using the value in the entry box
  void SetHomepageFromEntry();

  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupRadioToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupAddCustomPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupRemoveCustomPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupUseCurrentPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnNewTabIsHomePageToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnHomepageUseUrlEntryChanged);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnShowHomeButtonToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnDefaultSearchEngineChanged);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void,
                       OnDefaultSearchManageEnginesClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnBrowserUseAsDefaultClicked);

  CHROMEG_CALLBACK_0(GeneralPageGtk, void, OnStartupPagesSelectionChanged,
                     GtkTreeSelection*);

  // Enables/Disables the controls associated with the custom start pages
  // option if that preference is not selected.
  void EnableCustomHomepagesControls(bool enable);

  // ShellIntegration::DefaultBrowserObserver implementation.
  virtual void SetDefaultBrowserUIState(
      ShellIntegration::DefaultBrowserUIState state);

  // Widgets of the startup group
  GtkWidget* startup_homepage_radio_;
  GtkWidget* startup_last_session_radio_;
  GtkWidget* startup_custom_radio_;
  GtkWidget* startup_custom_pages_tree_;
  GtkListStore* startup_custom_pages_model_;
  GtkTreeSelection* startup_custom_pages_selection_;
  GtkWidget* startup_add_custom_page_button_;
  GtkWidget* startup_remove_custom_page_button_;
  GtkWidget* startup_use_current_page_button_;

  // Widgets and prefs of the homepage group
  GtkWidget* homepage_use_newtab_radio_;
  GtkWidget* homepage_use_url_radio_;
  GtkWidget* homepage_use_url_entry_;
  GtkWidget* homepage_show_home_button_checkbox_;
  BooleanPrefMember new_tab_page_is_home_page_;
  StringPrefMember homepage_;
  BooleanPrefMember show_home_button_;

  // Widgets and data of the default search group
  GtkWidget* default_search_engine_combobox_;
  GtkListStore* default_search_engines_model_;
  GtkWidget* default_search_manage_engines_button_;
  TemplateURLModel* template_url_model_;

  // Widgets of the default browser group
  GtkWidget* default_browser_status_label_;
  GtkWidget* default_browser_use_as_default_button_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Flag to ignore gtk callbacks while we are populating default search urls.
  bool default_search_initializing_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  // Used in loading favicons.
  CancelableRequestConsumer fav_icon_consumer_;

  // Helper to load the favicon pixbufs into the |startup_custom_pages_model_|.
  scoped_ptr<ListStoreFavIconLoader> favicon_loader_;

  // The helper object that performs default browser set/check tasks.
  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(GeneralPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
