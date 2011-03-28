/*
 * Copyright (c) 2010, Intel Corporation. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 * copyright notice, this list of conditions and the following disclaimer 
 * in the documentation and/or other materials provided with the 
 * distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 * contributors may be used to endorse or promote products derived from 
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CHROME_BROWSER_QT_NEW_TAB_UI_QT_H_
#define CHROME_BROWSER_QT_NEW_TAB_UI_QT_H_

#include "base/callback.h"
#include "base/command_line.h"
#include "base/md5.h"
#include "base/singleton.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "content/browser/cancelable_request.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/ui/meegotouch/browser_window_qt.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"


#include <QObject>
#include <QDeclarativeEngine>
#include <QDeclarativeView>
#include <QDeclarativeContext>
#include <QDeclarativeItem>
#include <QAbstractListModel>

class Profile;
class NewTabUIQtImpl;
class MaxViewModel;
class MaxViewImageProvider;
class ThumbnailEntry;
class FaviconEntry;

class NewTabUIQt : public TabStripModelObserver,
		   public TabRestoreServiceObserver  {

  public:
   friend class MaxViewModel;
 
   struct MostVisitedPage {
      int id;
      string16 title;
      GURL url;
      GURL thumbnail_url;
      GURL favicon_url;
   };

   NewTabUIQt(Browser* browser, BrowserWindowQt* window);

    ~NewTabUIQt();

    void AboutToShow();

    void Hide();

    bool isShowing();

    MaxViewImageProvider* getImageProviderByName(QString name);

    Profile* getProfile() { return browser_->profile(); }

    //void openWebPage(int index);
    Browser* getBrowser() { return browser_; }

    // Observer callback for TabRestoreServiceObserver. Sends data on
    // recently closed tabs to the javascript side of this page to
    // display to the user.
    virtual void TabRestoreServiceChanged(TabRestoreService* service);

    // Observer callback to notice when our associated TabRestoreService
    // is destroyed.
    virtual void TabRestoreServiceDestroyed(TabRestoreService* service);
 protected:
    // TabStripModelObserver implementation:
    virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
    virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
    virtual void TabChangedAt(TabContents* contents, int index,
                            TabChangeType change_type);
  private:
    bool shouldDisplay();

    void handleTabStatusChanged();

    void updateDataModel();

    void StartQueryForMostVisited();
  // Callback for TopSites.
    void OnMostVisitedURLsAvailable(const history::MostVisitedURLList &data);
  // Callback from the history system when the most visited list is available.
    void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                                    std::vector<PageUsageData*>* data);
    void dump(std::vector<PageUsageData*>* data); 

    void RegisterGetRecentlyClosedTab();

    bool TabToValue(const TabRestoreService::Tab& tab, PageUsageData* value);
    
    bool EnsureTabIsUnique(const PageUsageData* value, std::set<string16>* unique_items);
    
    std::vector<PageUsageData*>* syncWithPinnedPage(std::vector<PageUsageData*>* data);

    void HandleAddPinnedURL(PageUsageData* data, int index);

    void AddPinnedURL(const MostVisitedPage& page, int index);

    void RemovePinnedURL(const GURL& url);

    bool GetPinnedURLAtIndex(int index, MostVisitedPage* page);

    std::string GetDictionaryKeyForURL(const std::string& url);
    
    void SetMostVisistedPage(DictionaryValue* dict, const MostVisitedPage& page);
    
  private:
    Browser* browser_;

    BrowserWindowQt* window_;

    NewTabUIQtImpl* impl_;

    MaxViewModel* mostVisitedModel_;

    MaxViewModel* recentlyClosedModel_;

    TabRestoreService* tab_restore_service_;

    bool isShowing_;

    bool isAboutToShow_;

    MaxViewImageProvider* mostVisitedImageProvider_;

    MaxViewImageProvider* recentlyClosedImageProvider_;

    DictionaryValue* pinned_urls_;

    // Our consumer for the history service.
    CancelableRequestConsumerTSimple<PageUsageData*> cancelable_consumer_;

    CancelableRequestConsumer topsites_consumer_;

};


class NewTabUIQtImpl : public QObject {
  Q_OBJECT;
  public:
    NewTabUIQtImpl() {}
    ~NewTabUIQtImpl() {} 

  void show() {
    emit showNewTab();
  }
  
  void hide() {
    emit hideNewTab();
  }

  Q_SIGNALS:
    void showNewTab();

    void hideNewTab();

  public Q_SLOTS:
    //void openWebPage(int index);

//    void updateNewTab();

  private:
    NewTabUIQt* new_tab_;

};


// The number of most visited pages we show.
const size_t kMostVisitedPages = 8;
// The number of days of history we consider for most visited entries.
const int kMostVisitedScope = 90;

class MaxViewModel : public QAbstractListModel {
  Q_OBJECT;
  public:
    enum MaxViewRule {
	UrlRule = Qt::UserRole + 1,
	TitleRule,
        ThumbnailRule,
        FaviconRule,
  	IndexRule
    };

    MaxViewModel(NewTabUIQt* tab, std::vector<PageUsageData*>* data, QString name);
    virtual ~MaxViewModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    void updateContent(std::vector<PageUsageData*>* data);
    void beginReset();
    void endReset();
    void clear();
    GURL getItemURL(int index);

  public Q_SLOTS:
    bool getCollapsedState() { return collapsedState; };
    void setCollapsedState(bool state) { collapsedState = state; };
    void openWebPage(int index);
    QString GetCategoryName();
    void swap(int from, int to);
    void bringToFront(int i);
    int getId(int index);

  private:
    QList<PageUsageData*> siteInfoList_;
    // list to hold entries
    QList<ThumbnailEntry*> thumbnailList_;
    QList<FaviconEntry*> faviconList_;
    int returnedImages_;
    bool collapsedState;
    NewTabUIQt* new_tab_;
    QString name_;
    //Add for force reloading image for QML
    int updateTimes_;
};

#endif
