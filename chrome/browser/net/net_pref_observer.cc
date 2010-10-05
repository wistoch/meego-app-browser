// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

NetPrefObserver::NetPrefObserver(PrefService* prefs) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  dns_prefetching_enabled_.Init(prefs::kDnsPrefetchingEnabled, prefs, this);
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void NetPrefObserver::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  chrome_browser_net::EnablePredictor(*dns_prefetching_enabled_);
}
