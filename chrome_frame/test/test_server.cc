// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/registry.h"
#include "base/string_util.h"

#include "chrome_frame/test/test_server.h"

#include "net/base/winsock_init.h"
#include "net/http/http_util.h"

namespace test_server {
const char kDefaultHeaderTemplate[] =
    "HTTP/1.1 %hs\r\n"
    "Connection: close\r\n"
    "Content-Type: %hs\r\n"
    "Content-Length: %i\r\n\r\n";
const char kStatusOk[] = "200 OK";
const char kStatusNotFound[] = "404 Not Found";
const char kDefaultContentType[] = "text/html; charset=UTF-8";

void Request::ParseHeaders(const std::string& headers) {
  size_t pos = headers.find("\r\n");
  DCHECK(pos != std::string::npos);
  if (pos != std::string::npos) {
    headers_ = headers.substr(pos + 2);

    StringTokenizer tokenizer(headers.begin(), headers.begin() + pos, " ");
    std::string* parse[] = { &method_, &path_, &version_ };
    int field = 0;
    while (tokenizer.GetNext() && field < arraysize(parse)) {
      parse[field++]->assign(tokenizer.token_begin(),
                             tokenizer.token_end());
    }
  }

  // Check for content-length in case we're being sent some data.
  net::HttpUtil::HeadersIterator it(headers_.begin(), headers_.end(),
                                    "\r\n");
  while (it.GetNext()) {
    if (LowerCaseEqualsASCII(it.name(), "content-length")) {
       content_length_ = StringToInt(it.values().c_str());
       break;
    }
  }
}

bool Connection::CheckRequestReceived() {
  bool ready = false;
  if (request_.method().length()) {
    // Headers have already been parsed.  Just check content length.
    ready = (data_.size() >= request_.content_length());
  } else {
    size_t index = data_.find("\r\n\r\n");
    if (index != std::string::npos) {
      // Parse the headers before returning and chop them of the
      // data buffer we've already received.
      std::string headers(data_.substr(0, index + 2));
      request_.ParseHeaders(headers);
      data_.erase(0, index + 4);
      ready = (data_.size() >= request_.content_length());
    }
  }

  return ready;
}

bool FileResponse::GetContentType(std::string* content_type) const {
  size_t length = ContentLength();
  char buffer[4096];
  void* data = NULL;

  if (length) {
    // Create a copy of the first few bytes of the file.
    // If we try and use the mapped file directly, FindMimeFromData will crash
    // 'cause it cheats and temporarily tries to write to the buffer!
    length = std::min(arraysize(buffer), length);
    memcpy(buffer, file_->data(), length);
    data = buffer;
  }

  LPOLESTR mime_type = NULL;
  FindMimeFromData(NULL, file_path_.value().c_str(), data, length, NULL,
                   FMFD_DEFAULT, &mime_type, 0);
  if (mime_type) {
    *content_type = WideToASCII(mime_type);
    ::CoTaskMemFree(mime_type);
  }

  return content_type->length() > 0;
}

void FileResponse::WriteContents(ListenSocket* socket) const {
  DCHECK(file_.get());
  if (file_.get()) {
    socket->Send(reinterpret_cast<const char*>(file_->data()),
                 file_->length(), false);
  }
}

size_t FileResponse::ContentLength() const {
  if (file_.get() == NULL) {
    file_.reset(new file_util::MemoryMappedFile());
    if (!file_->Initialize(file_path_)) {
      NOTREACHED();
      file_.reset();
    }
  }
  return file_.get() ? file_->length() : 0;
}

bool RedirectResponse::GetCustomHeaders(std::string* headers) const {
  *headers = StringPrintf("HTTP/1.1 302 Found\r\n"
                          "Connection: close\r\n"
                          "Content-Length: 0\r\n"
                          "Content-Type: text/html\r\n"
                          "Location: %hs\r\n\r\n", redirect_url_.c_str());
  return true;
}

SimpleWebServer::SimpleWebServer(int port) {
  CHECK(MessageLoop::current()) << "SimpleWebServer requires a message loop";
  net::EnsureWinsockInit();
  AddResponse(&quit_);
  server_ = ListenSocket::Listen("127.0.0.1", port, this);
  DCHECK(server_.get() != NULL);
}

SimpleWebServer::~SimpleWebServer() {
  ConnectionList::const_iterator it;
  for (it = connections_.begin(); it != connections_.end(); it++)
    delete (*it);
  connections_.clear();
}

void SimpleWebServer::AddResponse(Response* response) {
  responses_.push_back(response);
}

Response* SimpleWebServer::FindResponse(const Request& request) const {
  std::list<Response*>::const_iterator it;
  for (it = responses_.begin(); it != responses_.end(); it++) {
    Response* response = (*it);
    if (response->Matches(request)) {
      return response;
    }
  }
  return NULL;
}

Connection* SimpleWebServer::FindConnection(const ListenSocket* socket) const {
  ConnectionList::const_iterator it;
  for (it = connections_.begin(); it != connections_.end(); it++) {
    if ((*it)->IsSame(socket)) {
      return (*it);
    }
  }
  return NULL;
}

void SimpleWebServer::DidAccept(ListenSocket* server,
                                ListenSocket* connection) {
  connections_.push_back(new Connection(connection));
}

void SimpleWebServer::DidRead(ListenSocket* connection,
                              const std::string& data) {
  Connection* c = FindConnection(connection);
  DCHECK(c);
  c->AddData(data);
  if (c->CheckRequestReceived()) {
    const Request& request = c->request();
    Response* response = FindResponse(request);
    if (response) {
      std::string headers;
      if (!response->GetCustomHeaders(&headers)) {
        std::string content_type;
        if (!response->GetContentType(&content_type))
          content_type = kDefaultContentType;
        headers = StringPrintf(kDefaultHeaderTemplate, kStatusOk,
                               content_type.c_str(), response->ContentLength());
      }

      connection->Send(headers, false);
      response->WriteContents(connection);
      response->IncrementAccessCounter();
    } else {
      std::string payload = "sorry, I can't find " + request.path();
      std::string headers(StringPrintf(kDefaultHeaderTemplate, kStatusNotFound,
                                       kDefaultContentType, payload.length()));
      connection->Send(headers, false);
      connection->Send(payload, false);
    }
  }
}

void SimpleWebServer::DidClose(ListenSocket* sock) {
  // To keep the historical list of connections reasonably tidy, we delete
  // 404's when the connection ends.
  Connection* c = FindConnection(sock);
  DCHECK(c);
  if (!FindResponse(c->request())) {
    // extremely inefficient, but in one line and not that common... :)
    connections_.erase(std::find(connections_.begin(), connections_.end(), c));
    delete c;
  }
}

}  // namespace test_server
