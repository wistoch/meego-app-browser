// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_handler.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"

PrinterJobHandler::PrinterJobHandler(
    const cloud_print::PrinterBasicInfo& printer_info,
    const std::string& printer_id,
    const std::string& caps_hash,
    const std::string& auth_token,
    Delegate* delegate)
    : printer_info_(printer_info),
      printer_id_(printer_id),
      auth_token_(auth_token),
      last_caps_hash_(caps_hash),
      delegate_(delegate),
      local_job_id_(-1),
      next_response_handler_(NULL),
      server_error_count_(0),
      print_thread_("Chrome_CloudPrintJobPrintThread"),
      shutting_down_(false),
      server_job_available_(false),
      printer_update_pending_(true),
      printer_delete_pending_(false),
      task_in_progress_(false) {
}

bool PrinterJobHandler::Initialize() {
  if (cloud_print::IsValidPrinter(printer_info_.printer_name)) {
    printer_change_notifier_.StartWatching(printer_info_.printer_name, this);
    NotifyJobAvailable();
  } else {
    // This printer does not exist any more. Delete it from the server.
    OnPrinterDeleted();
  }
  return true;
}

PrinterJobHandler::~PrinterJobHandler() {
  printer_change_notifier_.StopWatching();
}

void PrinterJobHandler::Reset() {
  print_data_url_.clear();
  job_details_.Clear();
  request_.reset();
  print_thread_.Stop();
}

void PrinterJobHandler::Start() {
  if (task_in_progress_) {
    // Multiple Starts can get posted because of multiple notifications
    // We want to ignore the other ones that happen when a task is in progress.
    return;
  }
  Reset();
  if (!shutting_down_) {
    // Check if we have work to do.
    if (HavePendingTasks()) {
      if (printer_delete_pending_) {
        printer_delete_pending_ = false;
        task_in_progress_ = true;
        MakeServerRequest(
            CloudPrintHelpers::GetUrlForPrinterDelete(printer_id_),
            &PrinterJobHandler::HandlePrinterDeleteResponse);
      }
      if (!task_in_progress_ && printer_update_pending_) {
        printer_update_pending_ = false;
        task_in_progress_ = UpdatePrinterInfo();
      }
      if (!task_in_progress_ && server_job_available_) {
        task_in_progress_ = true;
        server_job_available_ = false;
        // We need to fetch any pending jobs for this printer
        MakeServerRequest(CloudPrintHelpers::GetUrlForJobFetch(printer_id_),
                          &PrinterJobHandler::HandleJobMetadataResponse);
      }
    }
  }
}

void PrinterJobHandler::Stop() {
  task_in_progress_ = false;
  Reset();
  if (HavePendingTasks()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::NotifyJobAvailable() {
  server_job_available_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

bool PrinterJobHandler::UpdatePrinterInfo() {
  // We need to update the parts of the printer info that have changed
  // (could be printer name, description, status or capabilities).
  cloud_print::PrinterBasicInfo printer_info;
  printer_change_notifier_.GetCurrentPrinterInfo(&printer_info);
  cloud_print::PrinterCapsAndDefaults printer_caps;
  std::string post_data;
  std::string mime_boundary;
  if (cloud_print::GetPrinterCapsAndDefaults(printer_info.printer_name,
                                             &printer_caps)) {
    std::string caps_hash = MD5String(printer_caps.printer_capabilities);
    CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);
    if (caps_hash != last_caps_hash_) {
      // Hashes don't match, we need to upload new capabilities (the defaults
      // go for free along with the capabilities)
      last_caps_hash_ = caps_hash;
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterCapsValue, printer_caps.printer_capabilities,
          mime_boundary, printer_caps.caps_mime_type, &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterDefaultsValue, printer_caps.printer_defaults,
          mime_boundary, printer_caps.defaults_mime_type,
          &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          WideToUTF8(kPrinterCapsHashValue).c_str(), caps_hash, mime_boundary,
          std::string(), &post_data);
    }
  }
  if (printer_info.printer_name != printer_info_.printer_name) {
    CloudPrintHelpers::AddMultipartValueForUpload(kPrinterNameValue,
                                                  printer_info.printer_name,
                                                  mime_boundary,
                                                  std::string(), &post_data);
  }
  if (printer_info.printer_description != printer_info_.printer_description) {
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterDescValue, printer_info.printer_description, mime_boundary,
        std::string() , &post_data);
  }
  if (printer_info.printer_status != printer_info_.printer_status) {
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterStatusValue, StringPrintf("%d", printer_info.printer_status),
        mime_boundary, std::string(), &post_data);
  }
  printer_info_ = printer_info;
  bool ret = false;
  if (!post_data.empty()) {
    // Terminate the request body
    post_data.append("--" + mime_boundary + "--\r\n");
    std::string mime_type("multipart/form-data; boundary=");
    mime_type += mime_boundary;
    request_.reset(
        new URLFetcher(CloudPrintHelpers::GetUrlForPrinterUpdate(printer_id_),
                       URLFetcher::POST, this));
    CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
    request_->set_upload_data(mime_type, post_data);
    next_response_handler_ = &PrinterJobHandler::HandlePrinterUpdateResponse;
    request_->Start();
    ret = true;
  }
  return ret;
}

// URLFetcher::Delegate implementation.
void PrinterJobHandler::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  if (!shutting_down_) {
    DCHECK(source == request_.get());
    // We need a next response handler because we are strictly a sequential
    // state machine. We need each response handler to tell us which state to
    //  advance to next.
    DCHECK(next_response_handler_);
    if (!(this->*next_response_handler_)(source, url, status,
                                         response_code, cookies, data)) {
      // By contract, if the response handler returns false, it wants us to
      // retry the request (upto the usual limit after which we give up and
      // send the state machine to the Stop state);
      HandleServerError(url);
    }
  }
}

// JobStatusUpdater::Delegate implementation
bool PrinterJobHandler::OnJobCompleted(JobStatusUpdater* updater) {
  bool ret = false;
  for (JobStatusUpdaterList::iterator index = job_status_updater_list_.begin();
       index != job_status_updater_list_.end(); index++) {
    if (index->get() == updater) {
      job_status_updater_list_.erase(index);
      ret = true;
      break;
    }
  }
  return ret;
}

  // cloud_print::PrinterChangeNotifier::Delegate implementation
void PrinterJobHandler::OnPrinterAdded() {
  // Should never get this notification for a printer
  NOTREACHED();
}

void PrinterJobHandler::OnPrinterDeleted() {
  printer_delete_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::OnPrinterChanged() {
  printer_update_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::OnJobChanged() {
  // Some job on the printer changed. Loop through all our JobStatusUpdaters
  // and have them check for updates.
  for (JobStatusUpdaterList::iterator index = job_status_updater_list_.begin();
       index != job_status_updater_list_.end(); index++) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(index->get(),
                                     &JobStatusUpdater::UpdateStatus));
  }
}

bool PrinterJobHandler::HandlePrinterUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  bool ret = false;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (status.is_success() && (response_code == 200)) {
    bool succeeded = false;
    DictionaryValue* response_dict = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
    // If we get valid JSON back, we are done.
    if (NULL != response_dict) {
      ret = true;
    }
  }
  if (ret) {
    // We are done here. Go to the Stop state
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  } else {
    // Since we failed to update the server, set the flag again.
    printer_update_pending_ = true;
  }
  return ret;
}

bool PrinterJobHandler::HandlePrinterDeleteResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  bool ret = false;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (status.is_success() && (response_code == 200)) {
    bool succeeded = false;
    DictionaryValue* response_dict = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
    // If we get valid JSON back, we are done.
    if (NULL != response_dict) {
      ret = true;
    }
  }
  if (ret) {
    // The printer has been deleted. Shutdown the handler class.
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Shutdown));
  } else {
    // Since we failed to update the server, set the flag again.
    printer_delete_pending_ = true;
  }
  return ret;
}

bool PrinterJobHandler::HandleJobMetadataResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  bool succeeded = false;
  DictionaryValue* response_dict = NULL;
  CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
  if (NULL == response_dict) {
    // If we did not get a valid JSON response, we need to retry.
    return false;
  }
  Task* next_task = NULL;
  if (succeeded) {
    ListValue* job_list = NULL;
    response_dict->GetList(kJobListValue, &job_list);
    if (job_list) {
      // Even though it is a job list, for now we are only interested in the
      // first job
      DictionaryValue* job_data = NULL;
      if (job_list->GetDictionary(0, &job_data)) {
        job_data->GetString(kIdValue, &job_details_.job_id_);
        job_data->GetString(kTitleValue, &job_details_.job_title_);
        std::string print_ticket_url;
        job_data->GetString(kTicketUrlValue, &print_ticket_url);
        job_data->GetString(kFileUrlValue, &print_data_url_);
        next_task = NewRunnableMethod(
              this, &PrinterJobHandler::MakeServerRequest,
              GURL(print_ticket_url.c_str()),
              &PrinterJobHandler::HandlePrintTicketResponse);
      }
    }
  }
  if (!next_task) {
    // If we got a valid JSON but there were no jobs, we are done
    next_task = NewRunnableMethod(this, &PrinterJobHandler::Stop);
  }
  delete response_dict;
  DCHECK(next_task);
  MessageLoop::current()->PostTask(FROM_HERE, next_task);
  return true;
}

bool PrinterJobHandler::HandlePrintTicketResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  if (cloud_print::ValidatePrintTicket(printer_info_.printer_name, data)) {
    job_details_.print_ticket_ = data;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &PrinterJobHandler::MakeServerRequest,
                          GURL(print_data_url_.c_str()),
                          &PrinterJobHandler::HandlePrintDataResponse));
  } else {
    // The print ticket was not valid. We are done here.
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::JobFailed,
                                     INVALID_JOB_DATA));
  }
  return true;
}

bool PrinterJobHandler::HandlePrintDataResponse(const URLFetcher* source,
                                                const GURL& url,
                                                const URLRequestStatus& status,
                                                int response_code,
                                                const ResponseCookies& cookies,
                                                const std::string& data) {
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  Task* next_task = NULL;
  if (file_util::CreateTemporaryFile(&job_details_.print_data_file_path_)) {
    int ret = file_util::WriteFile(job_details_.print_data_file_path_,
                                   data.c_str(),
                                   data.length());
    source->response_headers()->GetMimeType(
        &job_details_.print_data_mime_type_);
    DCHECK(ret == static_cast<int>(data.length()));
    if (ret == static_cast<int>(data.length())) {
      next_task = NewRunnableMethod(this, &PrinterJobHandler::StartPrinting);
    }
  }
  // If there was no task allocated above, then there was an error in
  // saving the print data, bail out here.
  if (!next_task) {
    next_task = NewRunnableMethod(this, &PrinterJobHandler::JobFailed,
                                  JOB_DOWNLOAD_FAILED);
  }
  MessageLoop::current()->PostTask(FROM_HERE, next_task);
  return true;
}

void PrinterJobHandler::StartPrinting() {
  // We are done with the request object for now.
  request_.reset();
  if (!shutting_down_) {
    if (!print_thread_.Start()) {
      JobFailed(PRINT_FAILED);
    } else {
      print_thread_.message_loop()->PostTask(
          FROM_HERE, NewRunnableFunction(&PrinterJobHandler::DoPrint,
                                         job_details_,
                                         printer_info_.printer_name, this,
                                         MessageLoop::current()));
    }
  }
}

void PrinterJobHandler::JobFailed(PrintJobError error) {
  if (!shutting_down_) {
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_ERROR, error);
  }
}

void PrinterJobHandler::JobSpooled(cloud_print::PlatformJobId local_job_id) {
  if (!shutting_down_) {
    local_job_id_ = local_job_id;
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_IN_PROGRESS, SUCCESS);
    print_thread_.Stop();
  }
}

void PrinterJobHandler::Shutdown() {
  Reset();
  shutting_down_ = true;
  while (!job_status_updater_list_.empty()) {
    // Calling Stop() will cause the OnJobCompleted to be called which will
    // remove the updater object from the list.
    job_status_updater_list_.front()->Stop();
  }
  if (delegate_) {
    delegate_->OnPrinterJobHandlerShutdown(this, printer_id_);
  }
}

void PrinterJobHandler::HandleServerError(const GURL& url) {
  Task* task_to_retry = NewRunnableMethod(this,
                                          &PrinterJobHandler::MakeServerRequest,
                                          url, next_response_handler_);
  Task* task_on_give_up = NewRunnableMethod(this, &PrinterJobHandler::Stop);
  CloudPrintHelpers::HandleServerError(&server_error_count_, kMaxRetryCount,
                                       -1, kBaseRetryInterval, task_to_retry,
                                       task_on_give_up);
}

void PrinterJobHandler::UpdateJobStatus(cloud_print::PrintJobStatus status,
                                        PrintJobError error) {
  if (!shutting_down_) {
    if (!job_details_.job_id_.empty()) {
      ResponseHandler response_handler = NULL;
      if (error == SUCCESS) {
        response_handler =
            &PrinterJobHandler::HandleSuccessStatusUpdateResponse;
      } else {
        response_handler =
            &PrinterJobHandler::HandleFailureStatusUpdateResponse;
      }
      MakeServerRequest(
          CloudPrintHelpers::GetUrlForJobStatusUpdate(job_details_.job_id_,
                                                      status),
          response_handler);
    }
  }
}

bool PrinterJobHandler::HandleSuccessStatusUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  // The print job has been spooled locally. We now need to create an object
  // that monitors the status of the job and updates the server.
  scoped_refptr<JobStatusUpdater> job_status_updater =
      new JobStatusUpdater(printer_info_.printer_name, job_details_.job_id_,
                           local_job_id_, auth_token_, this);
  job_status_updater_list_.push_back(job_status_updater);
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(job_status_updater.get(),
                                   &JobStatusUpdater::UpdateStatus));
  bool succeeded = false;
  CloudPrintHelpers::ParseResponseJSON(data, &succeeded, NULL);
  if (succeeded) {
    // Since we just printed successfully, we want to look for more jobs.
    server_job_available_ = true;
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return true;
}

bool PrinterJobHandler::HandleFailureStatusUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return true;
}

void PrinterJobHandler::MakeServerRequest(const GURL& url,
                                          ResponseHandler response_handler) {
  if (!shutting_down_) {
    request_.reset(new URLFetcher(url, URLFetcher::GET, this));
    server_error_count_ = 0;
    CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
    // Set up the next response handler
    next_response_handler_ = response_handler;
    request_->Start();
  }
}

bool PrinterJobHandler::HavePendingTasks() {
  return server_job_available_ || printer_update_pending_ ||
         printer_delete_pending_;
}


void PrinterJobHandler::DoPrint(const JobDetails& job_details,
                                const std::string& printer_name,
                                PrinterJobHandler* job_handler,
                                MessageLoop* job_message_loop) {
  DCHECK(job_handler);
  DCHECK(job_message_loop);
  cloud_print::PlatformJobId job_id = -1;
  if (cloud_print::SpoolPrintJob(job_details.print_ticket_,
                                 job_details.print_data_file_path_,
                                 job_details.print_data_mime_type_,
                                 printer_name,
                                 job_details.job_title_, &job_id)) {
    job_message_loop->PostTask(FROM_HERE,
                               NewRunnableMethod(job_handler,
                                                 &PrinterJobHandler::JobSpooled,
                                                 job_id));
  } else {
    job_message_loop->PostTask(FROM_HERE,
                               NewRunnableMethod(job_handler,
                                                 &PrinterJobHandler::JobFailed,
                                                 PRINT_FAILED));
  }
}

