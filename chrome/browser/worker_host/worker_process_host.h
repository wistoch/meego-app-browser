// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
#define CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_

#include <list>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/common/child_process_host.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"

class WorkerProcessHost : public ChildProcessHost {
 public:
  // Contains information about each worker instance, needed to forward messages
  // between the renderer and worker processes.
  class WorkerInstance {
   public:
    WorkerInstance(const GURL& url,
                   bool is_shared,
                   const string16& name,
                   int renderer_id,
                   int render_view_route_id,
                   int worker_route_id);

    // Unique identifier for a worker client.
    typedef std::pair<IPC::Message::Sender*, int> SenderInfo;

    // APIs to manage the sender list for a given instance.
    void AddSender(IPC::Message::Sender* sender, int sender_route_id);
    void RemoveSender(IPC::Message::Sender* sender, int sender_route_id);
    void RemoveSenders(IPC::Message::Sender* sender);
    bool HasSender(IPC::Message::Sender* sender, int sender_route_id) const;
    int NumSenders() const { return senders_.size(); }
    // Returns the single sender (must only be one).
    SenderInfo GetSender() const;

    // Checks if this WorkerInstance matches the passed url/name params
    // (per the comparison algorithm in the WebWorkers spec). This API only
    // applies to shared workers.
    bool Matches(const GURL& url, const string16& name) const;

    // Adds a document to a shared worker's document set.
    void AddToDocumentSet(IPC::Message::Sender* parent,
                          unsigned long long document_id);

    // Checks to see if a document is in a shared worker's document set.
    bool IsInDocumentSet(IPC::Message::Sender* parent,
                         unsigned long long document_id) const;

    // Removes a specific document from a shared worker's document set when
    // that document is detached.
    void RemoveFromDocumentSet(IPC::Message::Sender* parent,
                               unsigned long long document_id);

    // Copies the document set from one instance to another
    void CopyDocumentSet(const WorkerInstance& instance) {
      document_set_ = instance.document_set_;
    };

    // Invoked when a render process exits, to remove all associated documents
    // from a shared worker's document set.
    void RemoveAllAssociatedDocuments(IPC::Message::Sender* parent);

    bool IsDocumentSetEmpty() const { return document_set_.empty(); }


    // Accessors
    bool is_shared() const { return shared_; }
    bool is_closed() const { return closed_; }
    void set_closed(bool closed) { closed_ = closed; }
    const GURL& url() const { return url_; }
    const string16 name() const { return name_; }
    int renderer_id() const { return renderer_id_; }
    int render_view_route_id() const { return render_view_route_id_; }
    int worker_route_id() const { return worker_route_id_; }

   private:
    // Unique identifier for an associated document.
    typedef std::pair<IPC::Message::Sender*, unsigned long long> DocumentInfo;
    typedef std::list<DocumentInfo> DocumentSet;
    // Set of all senders (clients) associated with this worker.
    typedef std::list<SenderInfo> SenderList;
    GURL url_;
    bool shared_;
    bool closed_;
    string16 name_;
    int renderer_id_;
    int render_view_route_id_;
    int worker_route_id_;
    SenderList senders_;
    DocumentSet document_set_;
  };

  explicit WorkerProcessHost(ResourceDispatcherHost* resource_dispatcher_host_);
  ~WorkerProcessHost();

  // Starts the process.  Returns true iff it succeeded.
  bool Init();

  // Creates a worker object in the process.
  void CreateWorker(const WorkerInstance& instance);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message, IPC::Message::Sender* sender);

  void SenderShutdown(IPC::Message::Sender* sender);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(IPC::Message::Sender* sender,
                        unsigned long long document_id);

 protected:
  friend class WorkerService;

  typedef std::list<WorkerInstance> Instances;
  const Instances& instances() const { return instances_; }
  Instances& mutable_instances() { return instances_; }

 private:
  // ResourceDispatcherHost::Receiver implementation:
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  // Called when a message arrives from the worker process.
  void OnMessageReceived(const IPC::Message& message);

  // Called when the app invokes close() from within worker context.
  void OnWorkerContextClosed(int worker_route_id);

  // Called if a worker tries to connect to a shared worker.
  void OnLookupSharedWorker(const GURL& url,
                            const string16& name,
                            unsigned long long document_id,
                            int* route_id,
                            bool* url_error);

  // Given a Sender, returns the callback that generates a new routing id.
  static CallbackWithReturnValue<int>::Type* GetNextRouteIdCallback(
      IPC::Message::Sender* sender);

  // Relays a message to the given endpoint.  Takes care of parsing the message
  // if it contains a message port and sending it a valid route id.
  static void RelayMessage(const IPC::Message& message,
                           IPC::Message::Sender* sender,
                           int route_id,
                           CallbackWithReturnValue<int>::Type* next_route_id);

  virtual bool CanShutdown() { return instances_.empty(); }

  // Updates the title shown in the task manager.
  void UpdateTitle();

  void OnCreateWorker(const GURL& url,
                      bool is_shared,
                      const string16& name,
                      int render_view_route_id,
                      int* route_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& message);

  Instances instances_;

  // A callback to create a routing id for the associated worker process.
  scoped_ptr<CallbackWithReturnValue<int>::Type> next_route_id_callback_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHost);
};

#endif  // CHROME_BROWSER_WORKER_HOST_WORKER_PROCESS_HOST_H_
