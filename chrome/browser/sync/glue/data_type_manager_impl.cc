// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"

static const int kStartOrderStart = -1;

namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::AUTOFILL
};

}  // namespace

namespace browser_sync {

DataTypeManagerImpl::DataTypeManagerImpl(
    const DataTypeController::TypeMap& controllers)
    : controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      current_type_(kStartOrderStart) {
  DCHECK(arraysize(kStartOrder) > 0);
  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    DCHECK_EQ(DataTypeController::NOT_RUNNING, (*it).second->state());
  }
}

void DataTypeManagerImpl::Start(StartCallback* start_callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (state_ != STOPPED) {
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  state_ = STARTING;
  start_callback_.reset(start_callback);
  current_type_ = kStartOrderStart;
  StartNextType();
}

void DataTypeManagerImpl::StartNextType() {
  // Ensure that the current type has indeed started.
  DCHECK(current_type_ == kStartOrderStart ||
         controllers_[kStartOrder[current_type_]]->state() ==
           DataTypeController::RUNNING);

  // Find the next startable type.
  while (current_type_ < static_cast<int>(arraysize(kStartOrder)) - 1) {
    current_type_++;
    syncable::ModelType type = kStartOrder[current_type_];
    if (IsEnabled(type)) {
      LOG(INFO) << "Starting " << controllers_[type]->name();
      controllers_[type]->Start(
          true,
          NewCallback(this, &DataTypeManagerImpl::TypeStartCallback));
      return;
    }
  }

  // No more startable types found, we must be done.
  DCHECK_EQ(state_, STARTING);
  state_ = STARTED;
  start_callback_->Run(OK);
  start_callback_.reset();
}

void DataTypeManagerImpl::TypeStartCallback(
    DataTypeController::StartResult result) {
  // When the data type controller invokes this callback, it must be
  // on the UI thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // If we reach this callback while stopping, this means that the
  // current data type was stopped while still starting up.  Now that
  // the data type is aborted, we can finish stop.
  if (state_ == STOPPING) {
    FinishStop();
    start_callback_->Run(DataTypeManager::ABORTED);
    start_callback_.reset();
    return;
  }

  // If the type started normally, continue to the next type.
  syncable::ModelType type = kStartOrder[current_type_];
  if (result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN) {
    LOG(INFO) << "Started " << controllers_[type]->name();
    StartNextType();
    return;
  }

  // Any other result is a fatal error.  Shut down any types we've
  // managed to start up to this point and pass the result to the
  // callback.
  LOG(INFO) << "Failed " << controllers_[type]->name();
  FinishStop();
  StartResult start_result = DataTypeManager::ABORTED;
  switch (result) {
    case DataTypeController::ABORTED:
      start_result = DataTypeManager::ABORTED;
      break;
    case DataTypeController::ASSOCIATION_FAILED:
      start_result = DataTypeManager::ASSOCIATION_FAILED;
      break;
    default:
      NOTREACHED();
      break;
  }
  start_callback_->Run(start_result);
  start_callback_.reset();
}

void DataTypeManagerImpl::Stop() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (state_ == STOPPED)
    return;

  // If we are currently starting, then the current type is in a
  // partially started state.  Abort the startup of the current type
  // and continue shutdown when the abort completes.
  if (state_ == STARTING) {
    state_ = STOPPING;
    syncable::ModelType type = kStartOrder[current_type_];
    controllers_[type]->Stop();
    return;
  }

  state_ = STOPPING;
  FinishStop();
}

void DataTypeManagerImpl::FinishStop() {
  DCHECK(state_== STARTING || state_ == STOPPING);
  // Simply call the Stop() method on all running data types.
  for (unsigned int i = 0; i < arraysize(kStartOrder); ++i) {
    syncable::ModelType type = kStartOrder[i];
    if (IsRegistered(type) &&
        controllers_[type]->state() == DataTypeController::RUNNING) {
      controllers_[type]->Stop();
      LOG(INFO) << "Stopped " << controllers_[type]->name();
    }
  }
  state_ = STOPPED;
}

bool DataTypeManagerImpl::IsRegistered(syncable::ModelType type) {
  return controllers_.count(type) == 1;
}

bool DataTypeManagerImpl::IsEnabled(syncable::ModelType type) {
  return IsRegistered(type) && controllers_[type]->enabled();
}

}  // namespace browser_sync
