// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_
#define NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_

#include <string>

#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/balsa_visitor_interface.h"
#include "net/tools/flip_server/output_ordering.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/sm_interface.h"

namespace net {

class BalsaFrame;
class DataFrame;
class EpollServer;
class FlipAcceptor;
class MemoryCache;

class HttpSM : public BalsaVisitorInterface,
               public SMInterface {
 public:
  HttpSM(SMConnection* connection,
         SMInterface* sm_spdy_interface,
         EpollServer* epoll_server,
         MemoryCache* memory_cache,
         FlipAcceptor* acceptor);
  virtual ~HttpSM();

 private:
  virtual void ProcessBodyInput(const char *input, size_t size) {}
  virtual void ProcessBodyData(const char *input, size_t size);
  virtual void ProcessHeaderInput(const char *input, size_t size) {}
  virtual void ProcessTrailerInput(const char *input, size_t size) {}
  virtual void ProcessHeaders(const BalsaHeaders& headers);
  virtual void ProcessRequestFirstLine(const char* line_input,
                                       size_t line_length,
                                       const char* method_input,
                                       size_t method_length,
                                       const char* request_uri_input,
                                       size_t request_uri_length,
                                       const char* version_input,
                                       size_t version_length) {}
  virtual void ProcessResponseFirstLine(const char *line_input,
                                        size_t line_length,
                                        const char *version_input,
                                        size_t version_length,
                                        const char *status_input,
                                        size_t status_length,
                                        const char *reason_input,
                                        size_t reason_length) {}
  virtual void ProcessChunkLength(size_t chunk_length) {}
  virtual void ProcessChunkExtensions(const char *input, size_t size) {}
  virtual void HeaderDone() {}
  virtual void MessageDone();
  virtual void HandleHeaderError(BalsaFrame* framer) { HandleError(); }
  virtual void HandleHeaderWarning(BalsaFrame* framer) {}
  virtual void HandleChunkingError(BalsaFrame* framer) { HandleError(); }
  virtual void HandleBodyError(BalsaFrame* framer) { HandleError(); }

  void HandleError();

 public:
  void InitSMInterface(SMInterface* sm_spdy_interface,
                       int32 server_idx);

  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        std::string server_ip,
                        std::string server_port,
                        std::string remote_ip,
                        bool use_ssl);

  size_t ProcessReadInput(const char* data, size_t len);
  size_t ProcessWriteInput(const char* data, size_t len);
  bool MessageFullyRead() const;
  void SetStreamID(uint32 stream_id) { stream_id_ = stream_id; }
  bool Error() const;
  const char* ErrorAsString() const;
  void Reset();
  void ResetForNewInterface(int32 server_idx) {}
  void ResetForNewConnection();
  void Cleanup();
  int PostAcceptHook() { return 1; }

  void NewStream(uint32 stream_id, uint32 priority,
                 const std::string& filename);
  void AddToOutputOrder(const MemCacheIter& mci);
  void SendEOF(uint32 stream_id);
  void SendErrorNotFound(uint32 stream_id);
  void SendOKResponse(uint32 stream_id, std::string* output);
  size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers);
  size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers);
  void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                     uint32 flags, bool compress);
  BalsaFrame* spdy_framer() { return http_framer_; }

 private:
  void SendEOFImpl(uint32 stream_id);
  void SendErrorNotFoundImpl(uint32 stream_id);
  void SendOKResponseImpl(uint32 stream_id, std::string* output);
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         uint32 flags, bool compress);
  void EnqueueDataFrame(DataFrame* df);
  void GetOutput();
 private:
  uint64 seq_num_;
  BalsaFrame* http_framer_;
  BalsaHeaders headers_;
  uint32 stream_id_;
  int32 server_idx_;

  SMConnection* connection_;
  SMInterface* sm_spdy_interface_;
  OutputList* output_list_;
  OutputOrdering output_ordering_;
  MemoryCache* memory_cache_;
  FlipAcceptor* acceptor_;
};

}  // namespace

#endif  // NET_TOOLS_FLIP_SERVER_HTTP_INTERFACE_
