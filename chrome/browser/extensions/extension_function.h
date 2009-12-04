// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_

#include <string>
#include <list>

#include "base/values.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"

class ExtensionFunctionDispatcher;
class Profile;
class QuotaLimitHeuristic;

#define EXTENSION_FUNCTION_VALIDATE(test) do { \
    if (!(test)) { \
      bad_message_ = true; \
      return false; \
    } \
  } while (0)

#define DECLARE_EXTENSION_FUNCTION_NAME(name) \
  public: static const char* function_name() { return name; }

// Abstract base class for extension functions the ExtensionFunctionDispatcher
// knows how to dispatch to.
//
// TODO(aa): This will have to become reference counted when we introduce
// APIs that live beyond a single stack frame.
class ExtensionFunction : public base::RefCounted<ExtensionFunction> {
 public:
  ExtensionFunction() : request_id_(-1), name_(""), has_callback_(false) {}

  // Specifies the name of the function.
  void set_name(const std::string& name) { name_ = name; }
  const std::string name() const { return name_; }

  // Specifies the raw arguments to the function, as a JSON value.
  virtual void SetArgs(const Value* args) = 0;

  // Retrieves the results of the function as a JSON-encoded string (may
  // be empty).
  virtual const std::string GetResult() = 0;

  // Retrieves any error string from the function.
  virtual const std::string GetError() = 0;

  // Returns a quota limit heuristic suitable for this function.
  // No quota limiting by default.
  virtual void GetQuotaLimitHeuristics(
      std::list<QuotaLimitHeuristic*>* heuristics) const {}

  void set_dispatcher_peer(ExtensionFunctionDispatcher::Peer* peer) {
    peer_ = peer;
  }
  ExtensionFunctionDispatcher* dispatcher() const {
    return peer_->dispatcher_;
  }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int request_id() { return request_id_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; }
  bool has_callback() { return has_callback_; }

  // Execute the API. Clients should call set_raw_args() and
  // set_request_id() before calling this method. Derived classes should be
  // ready to return raw_result() and error() before returning from this
  // function.
  virtual void Run() = 0;

 protected:
  friend class base::RefCounted<ExtensionFunction>;

  virtual ~ExtensionFunction() {}

  // Gets the extension that called this function. This can return NULL for
  // async functions.
  Extension* GetExtension() {
    if (dispatcher())
      return dispatcher()->GetExtension();
    else
      return NULL;
  }

  // The peer to the dispatcher that will service this extension function call.
  scoped_refptr<ExtensionFunctionDispatcher::Peer> peer_;

  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // The name of this function.
  std::string name_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};

// Base class for an extension function that runs asynchronously *relative to
// the browser's UI thread*.
// Note that once Run() returns, dispatcher() can be NULL, so be sure to
// NULL-check.
// TODO(aa) Remove this extra level of inheritance once the browser stops
// parsing JSON (and instead uses custom serialization of Value objects).
class AsyncExtensionFunction : public ExtensionFunction {
 public:
  AsyncExtensionFunction() : args_(NULL), bad_message_(false) {}

  virtual void SetArgs(const Value* args);
  virtual const std::string GetResult();
  virtual const std::string GetError() { return error_; }
  virtual void Run() {
    if (!RunImpl())
      SendResponse(false);
  }

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

 protected:
  virtual ~AsyncExtensionFunction() {}

  void SendResponse(bool success);

  const ListValue* args_as_list() {
    return static_cast<ListValue*>(args_.get());
  }
  const DictionaryValue* args_as_dictionary() {
    return static_cast<DictionaryValue*>(args_.get());
  }

  // Note: After Run() returns, dispatcher() can be NULL.  Since these getters
  // rely on dispatcher(), make sure it is valid before using them.
  std::string extension_id();
  Profile* profile() const;

  // The arguments to the API. Only non-null if argument were specified.
  scoped_ptr<Value> args_;

  // The result of the API. This should be populated by the derived class before
  // SendResponse() is called.
  scoped_ptr<Value> result_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // Any class that gets a malformed message should set this to true before
  // returning.  The calling renderer process will be killed.
  bool bad_message_;

  DISALLOW_COPY_AND_ASSIGN(AsyncExtensionFunction);
};

// A SyncExtensionFunction is an ExtensionFunction that runs synchronously
// *relative to the browser's UI thread*. Note that this has nothing to do with
// running synchronously relative to the extension process. From the extension
// process's point of view, the function is still asynchronous.
//
// This kind of function is convenient for implementing simple APIs that just
// need to interact with things on the browser UI thread.
class SyncExtensionFunction : public AsyncExtensionFunction {
 public:
  SyncExtensionFunction() {}

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

  virtual void Run() {
    SendResponse(RunImpl());
  }

 protected:
  virtual ~SyncExtensionFunction() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExtensionFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
