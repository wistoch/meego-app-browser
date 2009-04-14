// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
#define CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_

#include <list>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/common/notification_observer.h"
#include "googleurl/src/gurl.h"

namespace IPC {
class Message;
}

class MessageLoop;
class WorkerProcessHost;
class ResourceMessageFilter;

class WorkerService : public NotificationObserver {
 public:
  // Returns the WorkerService singleton.
  static WorkerService* GetInstance();

  // Creates a dedicated worker.  Returns true on success.
  bool CreateDedicatedWorker(const GURL &url,
                             int render_view_route_id,
                             ResourceMessageFilter* filter,
                             int renderer_route_id);

  // Called by ResourceMessageFilter when a message from the renderer comes that
  // should be forwarded to the worker process.
  void ForwardMessage(const IPC::Message& message);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

 private:
  friend struct DefaultSingletonTraits<WorkerService>;

  WorkerService();
  ~WorkerService();

  // Returns a WorkerProcessHost object if one exists for the given domain, or
  // NULL if there are no such workers yet.
  WorkerProcessHost* GetProcessForDomain(const GURL& url);

  // Returns a WorkerProcessHost based on a strategy of creating one worker per
  // core.
  WorkerProcessHost* GetProcessToFillUpCores();

  // Returns the WorkerProcessHost from the existing set that has the least
  // number of worker instance running.
  WorkerProcessHost* GetLeastLoadedWorker();

  int next_worker_route_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerService);
};

#endif  // CHROME_BROWSER_WORKER_HOST__WORKER_SERVICE_H_
