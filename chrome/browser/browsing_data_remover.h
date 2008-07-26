// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_BROWSING_DATA_REMOVER_H__
#define CHROME_BROWSER_BROWSING_DATA_REMOVER_H__

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/common/notification_service.h"

class MessageLoop;
class Profile;

// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...

class BrowsingDataRemover : public NotificationObserver {
 public:
  // Mask used for Remove.

  // In addition to visits, this removes keywords and the last session.
  static const int REMOVE_HISTORY = 1 << 0;
  static const int REMOVE_DOWNLOADS = 1 << 1;
  static const int REMOVE_COOKIES = 1 << 2;
  static const int REMOVE_PASSWORDS = 1 << 3;
  static const int REMOVE_CACHE = 1 << 4;

  // Observer is notified when the removal is done. Done means keywords have
  // been deleted, cache cleared and all other tasks scheduled.
  class Observer {
   public:
    virtual void OnBrowsingDataRemoverDone() = 0;
  };

  // Creates a BrowsingDataRemover to remove browser data from the specified
  // profile in the specified time range. Use Remove to initiate the removal.
  BrowsingDataRemover(Profile* profile, Time delete_begin, Time delete_end);
  ~BrowsingDataRemover();

  // Removes the specified items related to browsing.
  void Remove(int remove_mask);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when history deletion is done.
  void OnHistoryDeletionDone();

 private:
  // NotificationObserver method. Callback when TemplateURLModel has finished
  // loading. Deletes the entries from the model, and if we're not waiting on
  // anything else notifies observers and deletes this BrowsingDataRemover.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // If we're not waiting on anything, notifies observers and deletes this
  // object.
  void NotifyAndDeleteIfDone();

  // Callback when the cache has been deleted. Invokes NotifyAndDeleteIfDone.
  void ClearedCache();

  // Invoked on the IO thread to delete from the cache.
  void ClearCacheOnIOThread(Time delete_begin,
                            Time delete_end,
                            MessageLoop* ui_loop);

  // Returns true if we're all done.
  bool all_done() {
    return !waiting_for_keywords_ && !waiting_for_clear_cache_ &&
           !waiting_for_clear_history_;
  }

  // Profile we're to remove from.
  Profile* profile_;

  // Start time to delete from.
  const Time delete_begin_;

  // End time to delete to.
  const Time delete_end_;

  // True if Remove has been invoked.
  bool removing_;

  // True if we're waiting for the TemplateURLModel to finish loading.
  bool waiting_for_keywords_;

  // True if we're waiting for the history to be deleted.
  bool waiting_for_clear_history_;

  // True if we're waiting for the cache to be cleared.
  bool waiting_for_clear_cache_;

  ObserverList<Observer> observer_list_;

  // Used if we need to clear history.
  CancelableRequestConsumer request_consumer_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowsingDataRemover);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_REMOVER_H__
