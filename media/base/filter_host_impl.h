// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of FilterHost.

#ifndef MEDIA_BASE_FILTER_HOST_IMPL_H_
#define MEDIA_BASE_FILTER_HOST_IMPL_H_

#include "base/task.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline_impl.h"

namespace media {

class FilterHostImpl : public FilterHost {
 public:
  // FilterHost interface.
  virtual const PipelineStatus* GetPipelineStatus() const;
  virtual void SetTimeUpdateCallback(Callback1<base::TimeDelta>::Type* cb);
  virtual void InitializationComplete();
  virtual void PostTask(Task* task);
  virtual void Error(PipelineError error);
  virtual void SetTime(base::TimeDelta time);
  virtual void SetDuration(base::TimeDelta duration);
  virtual void SetBufferedTime(base::TimeDelta buffered_time);
  virtual void SetTotalBytes(int64 total_bytes);
  virtual void SetBufferedBytes(int64 buffered_bytes);
  virtual void SetVideoSize(size_t width, size_t height);

  // These methods are public, but are intended for use by the
  // PipelineThread class only.

  // Creates a FilterHostImpl object and populates the filter_type_ member
  // by calling the Filter class's static filter_type() method.  This ensures
  // that the GetFilter method can safely cast the filter interface from the
  // MediaFilter base class interface to the specific Filter interface.
  template <class Filter>
  FilterHostImpl(PipelineThread* pipeline_thread, Filter* filter)
      : pipeline_thread_(pipeline_thread),
        filter_type_(Filter::filter_type()),
        filter_(filter),
        stopped_(false) {
  }
  ~FilterHostImpl() {}

  // If this FilterHost contains a filter of the specifed Filter class, then
  // this method returns a pointer to the interface, otherwise it returns NULL.
  template <class Filter>
  Filter* GetFilter() const {
    Filter* filter = NULL;
    if (Filter::filter_type() == filter_type_) {
      filter = reinterpret_cast<Filter*>(media_filter());
    }
    return filter;
  }

  // Call the filter if it has registered a time update callback if the filter
  // has registered one though the FilterHost::SetTimeUpdateCallback method.
  void RunTimeUpdateCallback(base::TimeDelta time);

  // Stops the filter.
  void Stop();

  // Used by the PipelineThread to call Seek and SetRate methods on filters.
  MediaFilter* media_filter() const { return filter_; }

 private:
  PipelineImpl* pipeline() const { return pipeline_thread_->pipeline(); }

  // PipelineThread that owns this FilterHostImpl.
  PipelineThread* const pipeline_thread_;

  // The FilterType of the filter this host contains.
  FilterType const filter_type_;

  // A pointer to the filter's MediaFilter base interface.
  scoped_refptr<MediaFilter> filter_;

  // An optional callback that will be called when the time is updated.
  scoped_ptr<Callback1<base::TimeDelta>::Type> time_update_callback_;

  // Used to avoid calling Filter's Stop method multiplie times.  It is also
  // used to prevent a filter that has been stopped from calling PostTask.
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(FilterHostImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_IMPL_H_
