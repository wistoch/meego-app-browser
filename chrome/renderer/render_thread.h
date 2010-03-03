// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_THREAD_H_
#define CHROME_RENDERER_RENDER_THREAD_H_

#include <set>
#include <string>
#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/css_colors.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/renderer/renderer_histogram_snapshots.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "ipc/ipc_platform_file.h"

class AppCacheDispatcher;
class CookieMessageFilter;
class DBMessageFilter;
class DevToolsAgentFilter;
class FilePath;
class ListValue;
class NullableString16;
class RenderDnsMaster;
class RendererHistogram;
class RendererWebDatabaseObserver;
class RendererWebKitClientImpl;
class SpellCheck;
class SkBitmap;
class UserScriptSlave;
class URLPattern;

struct ContentSettings;
struct RendererPreferences;
struct ViewMsg_DOMStorageEvent_Params;
struct ViewMsg_New_Params;
struct WebPreferences;

namespace WebKit {
class WebStorageEventDispatcher;
}

// The RenderThreadBase is the minimal interface that a RenderView/Widget
// expects from a render thread. The interface basically abstracts a way to send
// and receive messages.
//
// TODO(brettw) this should be refactored like RenderProcess/RenderProcessImpl:
// This class should be named RenderThread and the implementation below should
// be RenderThreadImpl. The ::current() getter on the impl should then be moved
// here so we can provide another implementation of RenderThread for tests
// without having to check for NULL all the time.
class RenderThreadBase {
 public:
  virtual ~RenderThreadBase() {}

  virtual bool Send(IPC::Message* msg) = 0;

  // Called to add or remove a listener for a particular message routing ID.
  // These methods normally get delegated to a MessageRouter.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener) = 0;
  virtual void RemoveRoute(int32 routing_id) = 0;

  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;

  // Called by a RenderWidget when it is hidden or restored.
  virtual void WidgetHidden() = 0;
  virtual void WidgetRestored() = 0;
};

// The RenderThread class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding TabContents).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class RenderThread : public RenderThreadBase,
                     public ChildThread {
 public:
  // Grabs the IPC channel name from the command line.
  RenderThread();
  // Constructor that's used when running in single process mode.
  explicit RenderThread(const std::string& channel_name);
  virtual ~RenderThread();

  // Returns the one render thread for this process.  Note that this should only
  // be accessed when running on the render thread itself
  //
  // TODO(brettw) this should be on the abstract base class instead of here,
  // and return the base class' interface instead. Currently this causes
  // problems with testing. See the comment above RenderThreadBase above.
  static RenderThread* current();

  // Returns the routing ID of the RenderWidget containing the current script
  // execution context (corresponding to WebFrame::frameForCurrentContext).
  static int32 RoutingIDForCurrentContext();

  // Overridden from RenderThreadBase.
  virtual bool Send(IPC::Message* msg);
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  virtual void RemoveRoute(int32 routing_id);
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void WidgetHidden();
  virtual void WidgetRestored();

  // These methods modify how the next message is sent.  Normally, when sending
  // a synchronous message that runs a nested message loop, we need to suspend
  // callbacks into WebKit.  This involves disabling timers and deferring
  // resource loads.  However, there are exceptions when we need to customize
  // the behavior.
  void DoNotSuspendWebKitSharedTimer();
  void DoNotNotifyWebKitOfModalLoop();

  VisitedLinkSlave* visited_link_slave() const {
    return visited_link_slave_.get();
  }

  UserScriptSlave* user_script_slave() const {
    return user_script_slave_.get();
  }

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  SpellCheck* spellchecker() const {
    return spellchecker_.get();
  }

  CookieMessageFilter* cookie_message_filter() const {
    return cookie_message_filter_.get();
  }

  bool plugin_refresh_allowed() const { return plugin_refresh_allowed_; }

  bool is_extension_process() const { return is_extension_process_; }

  // Do DNS prefetch resolution of a hostname.
  void Resolve(const char* name, size_t length);

  // Send all the Histogram data to browser.
  void SendHistograms(int sequence_number);

  // Invokes InformHostOfCacheStats after a short delay.  Used to move this
  // bookkeeping operation off the critical latency path.
  void InformHostOfCacheStatsLater();

  // Sends a message to the browser to close all connections.
  void CloseCurrentConnections();

  // Sends a message to the browser to enable or disable the disk cache.
  void SetCacheMode(bool enabled);

  // Update the list of active extensions that will be reported when we crash.
  void UpdateActiveExtensions();

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  void Init();

  void OnUpdateVisitedLinks(base::SharedMemoryHandle table);
  void OnAddVisitedLinks(const VisitedLinkSlave::Fingerprints& fingerprints);
  void OnResetVisitedLinks();
  void OnSetZoomLevelForCurrentHost(const std::string& host, int zoom_level);
  void OnSetContentSettingsForCurrentHost(
      const std::string& host, const ContentSettings& content_settings);
  void OnUpdateUserScripts(base::SharedMemoryHandle table,
                           bool only_inject_incognito);
  void OnSetExtensionFunctionNames(const std::vector<std::string>& names);
  void OnPageActionsUpdated(const std::string& extension_id,
      const std::vector<std::string>& page_actions);
  void OnDOMStorageEvent(const ViewMsg_DOMStorageEvent_Params& params);
  void OnExtensionSetAPIPermissions(
      const std::string& extension_id,
      const std::vector<std::string>& permissions);
  void OnExtensionSetHostPermissions(
      const GURL& extension_url,
      const std::vector<URLPattern>& permissions);
  void OnExtensionSetIncognitoEnabled(
      const std::string& extension_id,
      bool enabled);
  void OnSetNextPageID(int32 next_page_id);
  void OnSetCSSColors(const std::vector<CSSColors::CSSColorMapping>& colors);
  void OnCreateNewView(const ViewMsg_New_Params& params);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
  void OnSetCacheCapacities(size_t min_dead_capacity,
                            size_t max_dead_capacity,
                            size_t capacity);
  void OnGetCacheResourceStats();

  // Send all histograms to browser.
  void OnGetRendererHistograms(int sequence_number);

  // Send tcmalloc info to browser.
  void OnGetRendererTcmalloc();
  void OnGetV8HeapStats();

  void OnExtensionMessageInvoke(const std::string& function_name,
                                const ListValue& args);
  void OnPurgeMemory();
  void OnPurgePluginListCache(bool reload_pages);

  void OnInitSpellChecker(IPC::PlatformFileForTransit bdict_file,
                          const std::vector<std::string>& custom_words,
                          const std::string& language,
                          bool auto_spell_correct);
  void OnSpellCheckWordAdded(const std::string& word);
  void OnSpellCheckEnableAutoSpellCorrect(bool enable);

  // Gather usage statistics from the in-memory cache and inform our host.
  // These functions should be call periodically so that the host can make
  // decisions about how to allocation resources using current information.
  void InformHostOfCacheStats();

  // We initialize WebKit as late as possible.
  void EnsureWebKitInitialized();

  // A task we invoke periodically to assist with idle cleanup.
  void IdleHandler();

  // Schedule a call to IdleHandler with the given initial delay.
  void ScheduleIdleHandler(double initial_delay_s);

  // These objects live solely on the render thread.
  scoped_ptr<ScopedRunnableMethodFactory<RenderThread> > task_factory_;
  scoped_ptr<VisitedLinkSlave> visited_link_slave_;
  scoped_ptr<UserScriptSlave> user_script_slave_;
  scoped_ptr<RenderDnsMaster> dns_master_;
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_refptr<DevToolsAgentFilter> devtools_agent_filter_;
  scoped_ptr<RendererHistogramSnapshots> histogram_snapshots_;
  scoped_ptr<RendererWebKitClientImpl> webkit_client_;
  scoped_ptr<WebKit::WebStorageEventDispatcher> dom_storage_event_dispatcher_;
  scoped_ptr<RendererWebDatabaseObserver> renderer_web_database_observer_;
  scoped_ptr<SpellCheck> spellchecker_;

  // Used on the renderer and IPC threads.
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<CookieMessageFilter> cookie_message_filter_;

#if defined(OS_POSIX)
  scoped_refptr<IPC::ChannelProxy::MessageFilter>
      suicide_on_channel_error_filter_;
#endif

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  // Is there a pending task for doing CacheStats.
  bool cache_stats_task_pending_;

  // The count of RenderWidgets running through this thread.
  int widget_count_;

  // The count of hidden RenderWidgets running through this thread.
  int hidden_widget_count_;

  // The current value of the idle notification timer delay.
  double idle_notification_delay_in_s_;

  // True if this renderer is running extensions.
  bool is_extension_process_;

  bool do_not_suspend_webkit_shared_timer_;
  bool do_not_notify_webkit_of_modal_loop_;
  bool did_notify_webkit_of_modal_loop_;

  // Timer that periodically calls IdleHandler.
  base::RepeatingTimer<RenderThread> idle_timer_;

  // Same as above, but on a longer timer and will run even if the process is
  // not idle, to ensure that IdleHandle gets called eventually.
  base::RepeatingTimer<RenderThread> forced_idle_timer_;

  DISALLOW_COPY_AND_ASSIGN(RenderThread);
};

#endif  // CHROME_RENDERER_RENDER_THREAD_H_
