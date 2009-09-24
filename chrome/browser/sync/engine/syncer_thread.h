// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to run the syncer on a thread.
// This is the default implementation of SyncerThread whose Stop implementation
// does not support a timeout, but is greatly simplified.
#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_

#include <list>
#include <map>
#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/condition_variable.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/client_command_channel.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

class EventListenerHookup;

namespace syncable {
class DirectoryManager;
struct DirectoryManagerEvent;
}

namespace browser_sync {

class ModelSafeWorker;
class ServerConnectionManager;
class Syncer;
class TalkMediator;
class URLFactory;
struct ServerConnectionEvent;
struct SyncerEvent;
struct SyncerShutdownEvent;
struct TalkMediatorEvent;

class SyncerThreadFactory {
 public:
  // Creates a SyncerThread based on the default (or user-overridden)
  // implementation.  The thread does not start running until you call Start(),
  // which will cause it to check-and-wait for certain conditions to be met
  // (such as valid connection with Server established, syncable::Directory has
  // been opened) before performing an intial sync with a server.  It uses
  // |connection_manager| to detect valid connections, and |mgr| to detect the
  // opening of a Directory, which will cause it to create a Syncer object for
  // said Directory, and assign |model_safe_worker| to it.  |connection_manager|
  // and |mgr| should outlive the SyncerThread.  You must stop the thread by
  // calling Stop before destroying the object.  Stopping will first tear down
  // the Syncer object, allowing it to finish work in progress, before joining
  // the Stop-calling thread with the internal thread.
  static SyncerThread* Create(ClientCommandChannel* command_channel,
      syncable::DirectoryManager* mgr,
      ServerConnectionManager* connection_manager, AllStatus* all_status,
      ModelSafeWorker* model_safe_worker);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncerThreadFactory);
};

class SyncerThread : public base::RefCountedThreadSafe<SyncerThread> {
  FRIEND_TEST(SyncerThreadTest, CalculateSyncWaitTime);
  FRIEND_TEST(SyncerThreadTest, CalculatePollingWaitTime);
  FRIEND_TEST(SyncerThreadWithSyncerTest, Polling);
  FRIEND_TEST(SyncerThreadWithSyncerTest, Nudge);
  friend class SyncerThreadWithSyncerTest;
  friend class SyncerThreadFactory;
public:
  enum NudgeSource {
    kUnknown = 0,
    kNotification,
    kLocal,
    kContinuation
  };
  // Server can overwrite these values via client commands.
  // Standard short poll. This is used when XMPP is off.
  static const int kDefaultShortPollIntervalSeconds = 60;
  // Long poll is used when XMPP is on.
  static const int kDefaultLongPollIntervalSeconds = 3600;
  // 30 minutes by default. If exponential backoff kicks in, this is the
  // longest possible poll interval.
  static const int kDefaultMaxPollIntervalMs = 30 * 60 * 1000;

  virtual ~SyncerThread();

  virtual void WatchConnectionManager(ServerConnectionManager* conn_mgr);

  // Starts a syncer thread.
  // Returns true if it creates a thread or if there's currently a thread
  // running and false otherwise.
  virtual bool Start();

  // Stop processing. |max_wait| doesn't do anything in this version.
  virtual bool Stop(int max_wait);

  // Nudges the syncer to sync with a delay specified. This API is for access
  // from the SyncerThread's controller and will cause a mutex lock.
  virtual bool NudgeSyncer(int milliseconds_from_now, NudgeSource source);

  // Registers this thread to watch talk mediator events.
  virtual void WatchTalkMediator(TalkMediator* talk_mediator);

  virtual void WatchClientCommands(ClientCommandChannel* channel);

  virtual SyncerEventChannel* channel();

 protected:
  SyncerThread();  // Necessary for temporary pthreads-based PIMPL impl.
  SyncerThread(ClientCommandChannel* command_channel,
      syncable::DirectoryManager* mgr,
      ServerConnectionManager* connection_manager, AllStatus* all_status,
      ModelSafeWorker* model_safe_worker);
  virtual void ThreadMain();
  void ThreadMainLoop();

  virtual void SetConnected(bool connected) {
    DCHECK(!thread_.IsRunning());
    vault_.connected_ = connected;
  }

  virtual void SetSyncerPollingInterval(base::TimeDelta interval) {
    // TODO(timsteele): Use TimeDelta internally.
    syncer_polling_interval_ = static_cast<int>(interval.InSeconds());
  }
  virtual void SetSyncerShortPollInterval(base::TimeDelta interval) {
    // TODO(timsteele): Use TimeDelta internally.
    syncer_short_poll_interval_seconds_ =
        static_cast<int>(interval.InSeconds());
  }

  // Needed to emulate the behavior of pthread_create, which synchronously
  // started the thread and set the value of thread_running_ to true.
  // We can't quite match that because we asynchronously post the task,
  // which opens a window for Stop to get called before the task actually
  // makes it.  To prevent this, we block Start() until we're sure it's ok.
  base::WaitableEvent thread_main_started_;

  // Handle of the running thread.
  base::Thread thread_;

  typedef std::pair<base::TimeTicks, NudgeSource> NudgeObject;

  struct IsTimeTicksGreater {
    inline bool operator() (const NudgeObject& lhs, const NudgeObject& rhs) {
      return lhs.first > rhs.first;
    }
  };

  typedef std::priority_queue<NudgeObject, std::vector<NudgeObject>,
                              IsTimeTicksGreater> NudgeQueue;

  // Fields that are modified / accessed by multiple threads go in this struct
  // for clarity and explicitness.
  struct ProtectedFields {
    // False when we want to stop the thread.
    bool stop_syncer_thread_;

    Syncer* syncer_;

    // State of the server connection.
    bool connected_;

    // A queue of all scheduled nudges.  One insertion for every call to
    // NudgeQueue().
    NudgeQueue nudge_queue_;

    ProtectedFields()
        : stop_syncer_thread_(false), syncer_(NULL), connected_(false) {}
  } vault_;

  // Gets signaled whenever a thread outside of the syncer thread changes a
  // protected field in the vault_.
  ConditionVariable vault_field_changed_;

  // Used to lock everything in |vault_|.
  Lock lock_;

 private:
  // A few members to gate the rate at which we nudge the syncer.
  enum {
    kNudgeRateLimitCount = 6,
    kNudgeRateLimitTime = 180,
  };

  // Threshold multipler for how long before user should be considered idle.
  static const int kPollBackoffThresholdMultiplier = 10;

  friend void* RunSyncerThread(void* syncer_thread);
  void* Run();
  void HandleDirectoryManagerEvent(
      const syncable::DirectoryManagerEvent& event);
  void HandleSyncerEvent(const SyncerEvent& event);
  void HandleClientCommand(ClientCommandChannel::EventType event);

  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void HandleTalkMediatorEvent(const TalkMediatorEvent& event);

  void SyncMain(Syncer* syncer);

  // Calculates the next sync wait time in seconds.  last_poll_wait is the time
  // duration of the previous polling timeout which was used.
  // user_idle_milliseconds is updated by this method, and is a report of the
  // full amount of time since the last period of activity for the user.  The
  // continue_sync_cycle parameter is used to determine whether or not we are
  // calculating a polling wait time that is a continuation of an sync cycle
  // which terminated while the syncer still had work to do.
  virtual int CalculatePollingWaitTime(
      const AllStatus::Status& status,
      int last_poll_wait,  // in s
      int* user_idle_milliseconds,
      bool* continue_sync_cycle);
  // Helper to above function, considers effect of user idle time.
  virtual int CalculateSyncWaitTime(int last_wait, int user_idle_ms);

  // Sets the source value of the controlled syncer's updates_source value.
  // The initial sync boolean is updated if read as a sentinel.  The following
  // two methods work in concert to achieve this goal.
  void UpdateNudgeSource(bool* continue_sync_cycle,
                         bool* initial_sync);
  void SetUpdatesSource(bool nudged, NudgeSource nudge_source,
                        bool* initial_sync);

  // For unit tests only.
  virtual void DisableIdleDetection() { disable_idle_detection_ = true; }

  // State of the notification framework is tracked by these values.
  bool p2p_authenticated_;
  bool p2p_subscribed_;

  scoped_ptr<EventListenerHookup> client_command_hookup_;
  scoped_ptr<EventListenerHookup> conn_mgr_hookup_;
  const AllStatus* allstatus_;

  syncable::DirectoryManager* dirman_;
  ServerConnectionManager* scm_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  int syncer_short_poll_interval_seconds_;
  int syncer_long_poll_interval_seconds_;

  // The time we wait between polls in seconds. This is used as lower bound on
  // our wait time. Updated once per loop from the command line flag.
  int syncer_polling_interval_;

  // The upper bound on the nominal wait between polls in seconds. Note that
  // this bounds the "nominal" poll interval, while the the actual interval
  // also takes previous failures into account.
  int syncer_max_interval_;

  scoped_ptr<SyncerEventChannel> syncer_event_channel_;

  // This causes syncer to start syncing ASAP. If the rate of requests is too
  // high the request will be silently dropped.  mutex_ should be held when
  // this is called.
  void NudgeSyncImpl(int milliseconds_from_now, NudgeSource source);

  scoped_ptr<EventListenerHookup> talk_mediator_hookup_;
  ClientCommandChannel* const command_channel_;
  scoped_ptr<EventListenerHookup> directory_manager_hookup_;
  scoped_ptr<EventListenerHookup> syncer_events_;

  // Handles any tasks that will result in model changes (modifications of
  // syncable::Entries). Pass this to the syncer created and managed by |this|.
  // Only non-null in syncapi case.
  scoped_ptr<ModelSafeWorker> model_safe_worker_;

  // Useful for unit tests
  bool disable_idle_detection_;

  DISALLOW_COPY_AND_ASSIGN(SyncerThread);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_