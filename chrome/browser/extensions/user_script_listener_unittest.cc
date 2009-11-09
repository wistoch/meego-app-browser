// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/testing_profile.h"
#include "ipc/ipc_message.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_interfaces.h"

// A simple test URLRequestJob. We don't care what it does, only that whether it
// starts and finishes.
class SimpleTestJob : public URLRequestTestJob {
 public:
  explicit SimpleTestJob(URLRequest* request)
    : URLRequestTestJob(request, test_headers(), test_data_1(), true) {}
 private:
  ~SimpleTestJob() {}
};

class MockUserScriptMaster : public UserScriptMaster {
 public:
  explicit MockUserScriptMaster(const FilePath& script_dir)
      : UserScriptMaster(script_dir) {}

  virtual void StartScan() {
    // Do nothing. We want to manually control when scans occur.
  }
  void TestStartScan() {
    UserScriptMaster::StartScan();
  }

 private:
  ~MockUserScriptMaster() {}
};

class MockIOThread : public ChromeThread {
 public:
  MockIOThread() : ChromeThread(ChromeThread::IO) {}
  virtual ~MockIOThread() { Stop(); }

 private:
  virtual void Init() {
    service_.reset(new NotificationService());
  }
  virtual void CleanUp() {
    ChromePluginLib::UnloadAllPlugins();
    service_.reset();
  }

  scoped_ptr<NotificationService> service_;
};

// A class to help with making and handling resource requests.
class ResourceDispatcherHostTester
    : public ResourceDispatcherHost::Receiver,
      public URLRequest::Interceptor,
      public base::RefCountedThreadSafe<ResourceDispatcherHostTester> {
 public:
  explicit ResourceDispatcherHostTester()
      : Receiver(ChildProcessInfo::RENDER_PROCESS, -1) {
    URLRequest::RegisterRequestInterceptor(this);
  }
  virtual ~ResourceDispatcherHostTester() {
    URLRequest::UnregisterRequestInterceptor(this);
  }

  void MakeTestRequest(int request_id, const GURL& url) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &ResourceDispatcherHostTester::MakeTestRequestOnIOThread,
            request_id, url));
    MessageLoop::current()->Run();  // wait for Quit from IO thread
  }

  void WaitForScan(MockUserScriptMaster* master) {
    master->TestStartScan();
    MessageLoop::current()->RunAllPending();  // run the scan
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &ResourceDispatcherHostTester::RunPending));
    MessageLoop::current()->Run();  // wait for Quit from IO thread
  }

  bool IsRequestStarted(int request_id) {
    return std::find(started_requests_.begin(), started_requests_.end(),
                     request_id) != started_requests_.end();
  }

  bool IsRequestComplete(int request_id) {
    return std::find(completed_requests_.begin(), completed_requests_.end(),
                     request_id) != completed_requests_.end();
  }

 private:
  // URLRequest::Interceptor
  virtual URLRequestJob* MaybeIntercept(URLRequest* request) {
    return new SimpleTestJob(request);
  }

  // ResourceDispatcherHost::Receiver implementation
  virtual bool Send(IPC::Message* msg) {
    IPC_BEGIN_MESSAGE_MAP(ResourceDispatcherHostTester, *msg)
      IPC_MESSAGE_HANDLER(ViewMsg_Resource_ReceivedResponse, OnReceivedResponse)
      IPC_MESSAGE_HANDLER(ViewMsg_Resource_RequestComplete, OnRequestComplete)
    IPC_END_MESSAGE_MAP()
    delete msg;
    return true;
  }

  URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data) {
    return NULL;
  }

  // TODO(mpcomplete): Some of this stuff is copied from
  // resource_dispatcher_host_unittest.cc. Consider sharing.
  static ViewHostMsg_Resource_Request CreateResourceRequest(const char* method,
                                                            const GURL& url) {
    ViewHostMsg_Resource_Request request;
    request.method = std::string(method);
    request.url = url;
    request.first_party_for_cookies = url;  // bypass third-party cookie
                                            // blocking
    request.resource_type = ResourceType::MAIN_FRAME;
    request.load_flags = 0;
    // init the rest to default values to prevent getting UMR.
    request.frame_origin = "null";
    request.main_frame_origin = "null";
    request.origin_child_id = 0;
    request.request_context = 0;
    request.appcache_host_id = appcache::kNoHostId;
    return request;
  }

  void RunPending() {
    MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->RunAllPending();
    MessageLoop::current()->SetNestableTasksAllowed(false);

    // return control to UI thread.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, new MessageLoop::QuitTask());
  }

  void MakeTestRequestOnIOThread(int request_id, const GURL& url) {
    ViewHostMsg_Resource_Request request = CreateResourceRequest("GET", url);
    ViewHostMsg_RequestResource msg(0, request_id, request);
    bool msg_was_ok;
    host_.OnMessageReceived(msg, this, &msg_was_ok);
    RunPending();
  }

  void OnReceivedResponse(int request_id,
                          const ResourceResponseHead& response_head) {
    started_requests_.push_back(request_id);
  }

  void OnRequestComplete(int request_id,
                         const URLRequestStatus& status,
                         const std::string& security_info) {
    completed_requests_.push_back(request_id);
  }

  ResourceDispatcherHost host_;

  // Note: these variables are accessed on both threads, but since we only
  // one thread executes at a time, they are safe.
  std::vector<int> started_requests_;
  std::vector<int> completed_requests_;
};

class ExtensionTestingProfile : public TestingProfile {
 public:
  ExtensionTestingProfile() {
  }

  FilePath GetExtensionsInstallDir() {
    return GetPath().AppendASCII(ExtensionsService::kInstallDirectoryName);
  }

  void InitializeExtensionsService() {
    DCHECK(!GetExtensionsService());
    service_ = new ExtensionsService(this,
                                     CommandLine::ForCurrentProcess(),
                                     GetPrefs(),
                                     GetExtensionsInstallDir(),
                                     false);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_prompts(false);
    service_->ClearProvidersForTesting();
    service_->Init();
  }

  void ShutdownExtensionsService() {
    service_ = NULL;
  }

  virtual ExtensionsService* GetExtensionsService() {
    return service_.get();
  }

 private:
  scoped_refptr<ExtensionsService> service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTestingProfile);
};

class UserScriptListenerTest : public testing::Test {
 public:
  virtual void SetUp() {
    ui_thread_.reset(new ChromeThread(ChromeThread::UI, &loop_));
    file_thread_.reset(new ChromeThread(ChromeThread::FILE, &loop_));
    io_thread_.reset(new MockIOThread());
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
    DCHECK(io_thread_->message_loop());
    DCHECK(io_thread_->IsRunning());

    FilePath install_dir = profile_.GetPath()
        .AppendASCII(ExtensionsService::kInstallDirectoryName);

    resource_tester_ = new ResourceDispatcherHostTester();

    master_ = new MockUserScriptMaster(profile_.GetExtensionsInstallDir());

    profile_.InitializeExtensionsService();
  }

  virtual void TearDown() {
    // Shutdown ExtensionsService now, so that it can be destroyed while
    // ChromeThreads and MessageLoop are still around (they are used in the
    // ExtensionsService destruction process).
    profile_.ShutdownExtensionsService();
    io_thread_.reset();
    file_thread_.reset();
    ui_thread_.reset();
    resource_tester_ = NULL;
  }

 protected:
  ExtensionTestingProfile profile_;
  MessageLoopForUI loop_;
  scoped_ptr<ChromeThread> ui_thread_;
  scoped_ptr<ChromeThread> file_thread_;
  scoped_ptr<MockIOThread> io_thread_;
  scoped_refptr<ResourceDispatcherHostTester> resource_tester_;
  scoped_refptr<MockUserScriptMaster> master_;
};

// Loads a single extension and ensures that requests to URLs with content
// scripts are delayed.
TEST_F(UserScriptListenerTest, SingleExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");
  FilePath ext1 = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");
  profile_.GetExtensionsService()->LoadExtension(ext1);
  loop_.RunAllPending();
  ASSERT_EQ(1u, profile_.GetExtensionsService()->extensions()->size());

  // Our extension has a content script on google.com. That request should be
  // delayed.
  resource_tester_->MakeTestRequest(0, GURL("http://google.com/"));
  resource_tester_->MakeTestRequest(1, GURL("http://yahoo.com/"));

  EXPECT_FALSE(resource_tester_->IsRequestStarted(0));
  EXPECT_TRUE(resource_tester_->IsRequestStarted(1));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(1));

  // After scanning, the user scripts should be ready and the request can
  // go through.
  resource_tester_->WaitForScan(master_.get());

  EXPECT_TRUE(resource_tester_->IsRequestStarted(0));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(0));
}

// Loads a single extension and ensures that requests to URLs with content
// scripts are delayed.
TEST_F(UserScriptListenerTest, UnloadExtension) {
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions");

  FilePath ext1 = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");
  profile_.GetExtensionsService()->LoadExtension(ext1);
  loop_.RunAllPending();
  ASSERT_EQ(1u, profile_.GetExtensionsService()->extensions()->size());

  FilePath ext2 = extensions_path
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
      .AppendASCII("1.0");
  profile_.GetExtensionsService()->LoadExtension(ext2);
  loop_.RunAllPending();
  ASSERT_EQ(2u, profile_.GetExtensionsService()->extensions()->size());

  // Our extension has a content script on google.com. That request should be
  // delayed.
  resource_tester_->MakeTestRequest(0, GURL("http://google.com/"));
  resource_tester_->MakeTestRequest(1, GURL("http://yahoo.com/"));

  EXPECT_FALSE(resource_tester_->IsRequestStarted(0));
  EXPECT_TRUE(resource_tester_->IsRequestStarted(1));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(1));

  // Unload the first extension and run a scan. Request should complete.
  profile_.GetExtensionsService()->UnloadExtension(
      "behllobkkfkfnphdnhnkndlbkcpglgmj");
  resource_tester_->WaitForScan(master_.get());

  EXPECT_TRUE(resource_tester_->IsRequestStarted(0));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(0));

  // Make the same requests, and they should complete instantly.
  resource_tester_->MakeTestRequest(2, GURL("http://google.com/"));
  resource_tester_->MakeTestRequest(3, GURL("http://yahoo.com/"));

  EXPECT_TRUE(resource_tester_->IsRequestStarted(2));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(2));
  EXPECT_TRUE(resource_tester_->IsRequestStarted(3));
  EXPECT_TRUE(resource_tester_->IsRequestComplete(3));
}
