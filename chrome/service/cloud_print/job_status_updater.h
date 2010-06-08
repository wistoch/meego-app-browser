// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
#define CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "chrome/service/cloud_print/printer_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

// Periodically monitors the status of a local print job and updates the
// cloud print server accordingly. When the job has been completed this
// object releases the reference to itself which should cause it to
// self-destruct.
class JobStatusUpdater : public base::RefCountedThreadSafe<JobStatusUpdater>,
                         public URLFetcher::Delegate {
 public:
  class Delegate {
   public:
    virtual bool OnJobCompleted(JobStatusUpdater* updater) = 0;
  };

  JobStatusUpdater(const std::string& printer_name,
                   const std::string& job_id,
                   cloud_print::PlatformJobId& local_job_id,
                   const std::string& auth_token,
                   const GURL& cloud_print_server_url,
                   Delegate* delegate);
  // Checks the status of the local print job and sends an update.
  void UpdateStatus();
  void Stop();
  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source, const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  std::string printer_name_;
  std::string job_id_;
  cloud_print::PlatformJobId local_job_id_;
  cloud_print::PrintJobDetails last_job_details_;
  scoped_ptr<URLFetcher> request_;
  std::string auth_token_;
  GURL cloud_print_server_url_;
  Delegate* delegate_;
  // A flag that is set to true in Stop() and will ensure the next scheduled
  // task will do nothing.
  bool stopped_;
  DISALLOW_COPY_AND_ASSIGN(JobStatusUpdater);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef JobStatusUpdater::Delegate JobStatusUpdaterDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_

