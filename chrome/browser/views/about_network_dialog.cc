// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/about_network_dialog.h"

#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/views/standard_layout.h"
#include "chrome/views/grid_layout.h"
#include "chrome/views/text_button.h"
#include "chrome/views/text_field.h"
#include "chrome/views/window.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

namespace {

// We don't localize this UI since this is a developer-only feature.
const wchar_t kStartTrackingLabel[] = L"Start tracking";
const wchar_t kStopTrackingLabel[] = L"Stop tracking";
const wchar_t kShowCurrentLabel[] = L"Show Current";
const wchar_t kClearLabel[] = L"Clear";

// The singleton dialog box. This is non-NULL when a dialog is active so we
// know not to create a new one.
AboutNetworkDialog* active_dialog = NULL;

// Returns a string representing the URL, handling the case where the spec
// is invalid.
std::wstring StringForURL(const GURL& url) {
  if (url.is_valid())
    return UTF8ToWide(url.spec());
  return UTF8ToWide(url.possibly_invalid_spec()) + L" (invalid)";
}

std::wstring URLForJob(URLRequestJob* job) {
  URLRequest* request = job->request();
  if (request)
    return StringForURL(request->url());
  return std::wstring(L"(orphaned)");
}

// JobTracker ------------------------------------------------------------------

// A JobTracker is allocated to monitor network jobs running on the IO
// thread.  This allows the NetworkStatusView to remain single-threaded.
class JobTracker : public URLRequestJobTracker::JobObserver,
                   public base::RefCountedThreadSafe<JobTracker> {
 public:
  JobTracker(AboutNetworkDialog* view);
  ~JobTracker();

  // Called by the NetworkStatusView on the main application thread.
  void StartTracking();
  void StopTracking();
  void ReportStatus();

  // URLRequestJobTracker::JobObserver methods (called on the IO thread):
  virtual void OnJobAdded(URLRequestJob* job);
  virtual void OnJobRemoved(URLRequestJob* job);
  virtual void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                             int status_code);
  virtual void OnBytesRead(URLRequestJob* job, int byte_count);

  // The JobTracker may be deleted after NetworkStatusView is deleted.
  void DetachView() { view_ = NULL; }

 private:
  void InvokeOnIOThread(void (JobTracker::*method)());

  // Called on the IO thread
  void OnStartTracking();
  void OnStopTracking();
  void OnReportStatus();
  void AppendText(const std::wstring& text);

  // Called on the main thread
  void OnAppendText(const std::wstring& text);

  AboutNetworkDialog* view_;
  MessageLoop* view_message_loop_;
};

// main thread:
JobTracker::JobTracker(AboutNetworkDialog* view)
    : view_(view),
      view_message_loop_(MessageLoop::current()) {
}

JobTracker::~JobTracker() {
}

// main thread:
void JobTracker::InvokeOnIOThread(void (JobTracker::*m)()) {
  base::Thread* thread = g_browser_process->io_thread();
  if (!thread)
    return;
  thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this, m));
}

// main thread:
void JobTracker::StartTracking() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  DCHECK(view_);
  InvokeOnIOThread(&JobTracker::OnStartTracking);
}

// main thread:
void JobTracker::StopTracking() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  // The tracker should not be deleted before it is removed from observer
  // list.
  AddRef();
  InvokeOnIOThread(&JobTracker::OnStopTracking);
}

// main thread:
void JobTracker::ReportStatus() {
  DCHECK(MessageLoop::current() == view_message_loop_);
  InvokeOnIOThread(&JobTracker::OnReportStatus);
}

// main thread:
void JobTracker::OnAppendText(const std::wstring& text) {
  DCHECK(MessageLoop::current() == view_message_loop_);
  if (view_ && view_->tracking())
    view_->AppendText(text);
}

// IO thread:
void JobTracker::AppendText(const std::wstring& text) {
  DCHECK(MessageLoop::current() != view_message_loop_);
  view_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &JobTracker::OnAppendText, text));
}

// IO thread:
void JobTracker::OnStartTracking() {
  DCHECK(MessageLoop::current() != view_message_loop_);
  g_url_request_job_tracker.AddObserver(this);
}

// IO thread:
void JobTracker::OnStopTracking() {
  DCHECK(MessageLoop::current() != view_message_loop_);
  g_url_request_job_tracker.RemoveObserver(this);
  // Balance the AddRef() in StopTracking() called in main thread.
  Release();
}

// IO thread:
void JobTracker::OnReportStatus() {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"\r\n===== Active Job Summary =====\r\n");

  URLRequestJobTracker::JobIterator begin_job =
      g_url_request_job_tracker.begin();
  URLRequestJobTracker::JobIterator end_job = g_url_request_job_tracker.end();
  int orphaned_count = 0;
  int regular_count = 0;
  for (URLRequestJobTracker::JobIterator cur = begin_job;
       cur != end_job; ++cur) {
    URLRequestJob* job = (*cur);
    URLRequest* request = job->request();
    if (!request) {
      orphaned_count++;
      continue;
    }

    regular_count++;

    // active state
    if (job->is_done())
      text.append(L"  Done:   ");
    else
      text.append(L"  Active: ");

    // URL
    text.append(StringForURL(request->url()));
    text.append(L"\r\n");
  }

  if (regular_count == 0)
    text.append(L"  (No active jobs)\r\n");

  if (orphaned_count) {
    wchar_t buf[64];
    swprintf(buf, arraysize(buf), L"  %d orphaned jobs\r\n", orphaned_count);
    text.append(buf);
  }

  text.append(L"=====\r\n\r\n");
  AppendText(text);
}

// IO thread:
void JobTracker::OnJobAdded(URLRequestJob* job) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"+ New job : ");
  text.append(URLForJob(job));
  text.append(L"\r\n");
  AppendText(text);
}

// IO thread:
void JobTracker::OnJobRemoved(URLRequestJob* job) {
  DCHECK(MessageLoop::current() != view_message_loop_);
}

// IO thread:
void JobTracker::OnJobDone(URLRequestJob* job,
                           const URLRequestStatus& status) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text;
  if (status.is_success()) {
    text.assign(L"- Complete: ");
  } else if (status.status() == URLRequestStatus::CANCELED) {
    text.assign(L"- Canceled: ");
  } else if (status.status() == URLRequestStatus::HANDLED_EXTERNALLY) {
    text.assign(L"- Handled externally: ");
  } else {
    wchar_t buf[32];
    swprintf(buf, arraysize(buf), L"Failed with %d: ", status.os_error());
    text.assign(buf);
  }

  text.append(URLForJob(job));
  text.append(L"\r\n");
  AppendText(text);
}

// IO thread:
void JobTracker::OnJobRedirect(URLRequestJob* job,
                               const GURL& location,
                               int status_code) {
  DCHECK(MessageLoop::current() != view_message_loop_);

  std::wstring text(L"- Redirect: ");
  text.append(URLForJob(job));
  text.append(L"\r\n  ");

  wchar_t buf[16];
  swprintf(buf, arraysize(buf), L"(%d) to: ", status_code);
  text.append(buf);

  text.append(StringForURL(location));
  text.append(L"\r\n");
  AppendText(text);
}

void JobTracker::OnBytesRead(URLRequestJob* job, int byte_count) {
}

// The singleton job tracker associated with the dialog.
JobTracker* tracker = NULL;

}  // namespace

// AboutNetworkDialog ----------------------------------------------------------

AboutNetworkDialog::AboutNetworkDialog()
    : track_toggle_(NULL),
      show_button_(NULL),
      clear_button_(NULL),
      tracking_(false) {
  SetupControls();
  tracker = new JobTracker(this);
  tracker->AddRef();
}

AboutNetworkDialog::~AboutNetworkDialog() {
  active_dialog = NULL;
  tracker->Release();
  tracker = NULL;
}

// static
void AboutNetworkDialog::RunDialog() {
  if (!active_dialog) {
    active_dialog = new AboutNetworkDialog;
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), active_dialog)->Show();
  } else {
    // TOOD(brettw) it would be nice to focus the existing window.
  }
}

void AboutNetworkDialog::SetupButtonColumnSet(views::ColumnSet* set) {
  set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                 33.33f, views::GridLayout::FIXED, 0, 0);
  set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                 33.33f, views::GridLayout::FIXED, 0, 0);
  set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                 33.33f, views::GridLayout::FIXED, 0, 0);
}

void AboutNetworkDialog::AddButtonControlsToLayout(views::GridLayout* layout) {
  track_toggle_ = new views::TextButton(kStartTrackingLabel);
  track_toggle_->SetListener(this, 1);
  show_button_ = new views::TextButton(kShowCurrentLabel);
  show_button_->SetListener(this, 2);
  clear_button_ = new views::TextButton(kClearLabel);
  clear_button_->SetListener(this, 3);

  layout->AddView(track_toggle_);
  layout->AddView(show_button_);
  layout->AddView(clear_button_);
}

void AboutNetworkDialog::ButtonPressed(views::BaseButton* button) {
  if (button == track_toggle_) {
    if (tracking_) {
      track_toggle_->SetText(kStartTrackingLabel);
      tracking_ = false;
      tracker->StopTracking();
    } else {
      track_toggle_->SetText(kStopTrackingLabel);
      tracking_ = true;
      tracker->StartTracking();
    }
    track_toggle_->SchedulePaint();
  } else if (button == show_button_) {
    tracker->ReportStatus();
  } else if (button == clear_button_) {
    text_field()->SetText(std::wstring());
  }
}
