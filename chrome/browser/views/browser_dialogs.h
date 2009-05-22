// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_

#include "base/gfx/native_widget_types.h"

// This file contains functions for running a variety of browser dialogs and
// popups. The dialogs here are the ones that the caller does not need to
// access the class of the popup. It allows us to break dependencies by
// allowing the callers to not depend on the classes implementing the dialogs.

class Browser;
class GURL;
class HtmlDialogUIDelegate;
class InfoBubbleDelegate;
class Profile;
class TabContents;

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
class Window;
}  // namespace views

namespace browser {

// Shows the "Report a problem with this page" dialog box. See BugReportView.
void ShowBugReportView(views::Widget* parent,
                       Profile* profile,
                       TabContents* tab);

// Shows the "Clear browsing data" dialog box. See ClearBrowsingDataView.
void ShowClearBrowsingDataView(views::Widget* parent,
                               Profile* profile);

// Shows the "Select profile" dialog. See SelectProfileDialog.
void ShowSelectProfileDialog();

// Shows the "Importer" dialog. See ImporterView.
void ShowImporterView(views::Widget* parent,
                      Profile* profile);

// Shows or hides the global bookmark bubble for the star button.
void ShowBookmarkBubbleView(views::Window* parent,
                            const gfx::Rect& bounds,
                            InfoBubbleDelegate* delegate,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked);
void HideBookmarkBubbleView();
bool IsBookmarkBubbleViewShowing();

// Shows the about dialog. See AboutChromeView.
void ShowAboutChromeView(views::Widget* parent,
                         Profile* profile);

// Shows an HTML dialog. See HtmlDialogView.
void ShowHtmlDialogView(gfx::NativeWindow parent, Browser* browser,
                        HtmlDialogUIDelegate* delegate);

}  // namespace browser

#endif  // CHROME_BROWSER_VIEWS_BROWSER_DIALOGS_H_
