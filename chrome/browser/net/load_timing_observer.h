// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_
#define CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_

#include "base/hash_tables.h"
#include "base/time.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "net/base/net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

// LoadTimingObserver watches the NetLog event stream and collects the network
// timing information.
class LoadTimingObserver : public ChromeNetLog::Observer {
 public:
  struct URLRequestRecord {
    URLRequestRecord();

    webkit_glue::ResourceLoaderBridge::LoadTimingInfo timing;
    uint32 connect_job_id;
    uint32 socket_log_id;
    bool socket_reused;
    base::TimeTicks base_ticks;
  };

  struct ConnectJobRecord {
    base::TimeTicks dns_start;
    base::TimeTicks dns_end;
  };

  LoadTimingObserver();
  ~LoadTimingObserver();

  URLRequestRecord* GetURLRequestRecord(uint32 source_id);

  // Observer implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);
 private:
  void OnAddURLRequestEntry(net::NetLog::EventType type,
                            const base::TimeTicks& time,
                            const net::NetLog::Source& source,
                            net::NetLog::EventPhase phase,
                            net::NetLog::EventParameters* params);

  void OnAddConnectJobEntry(net::NetLog::EventType type,
                            const base::TimeTicks& time,
                            const net::NetLog::Source& source,
                            net::NetLog::EventPhase phase,
                            net::NetLog::EventParameters* params);

  URLRequestRecord* CreateURLRequestRecord(uint32 source_id);
  void DeleteURLRequestRecord(uint32 source_id);

  typedef base::hash_map<uint32, URLRequestRecord> URLRequestToRecordMap;
  typedef base::hash_map<uint32, ConnectJobRecord> ConnectJobToRecordMap;
  URLRequestToRecordMap url_request_to_record_;
  ConnectJobToRecordMap connect_job_to_record_;

  DISALLOW_COPY_AND_ASSIGN(LoadTimingObserver);
};

#endif  // CHROME_BROWSER_NET_LOAD_TIMING_OBSERVER_H_
