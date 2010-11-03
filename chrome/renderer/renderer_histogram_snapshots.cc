// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_histogram_snapshots.h"

#include <ctype.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"

// TODO(raman): Before renderer shuts down send final snapshot lists.

using base::Histogram;
using base::StatisticsRecorder;

RendererHistogramSnapshots::RendererHistogramSnapshots()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        renderer_histogram_snapshots_factory_(this)) {
}

RendererHistogramSnapshots::~RendererHistogramSnapshots() {
}

// Send data quickly!
void RendererHistogramSnapshots::SendHistograms(int sequence_number) {
  RenderThread::current()->message_loop()->PostTask(FROM_HERE,
      renderer_histogram_snapshots_factory_.NewRunnableMethod(
          &RendererHistogramSnapshots::UploadAllHistrograms, sequence_number));
}

void RendererHistogramSnapshots::UploadAllHistrograms(int sequence_number) {
  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);

  HistogramPickledList pickled_histograms;

  for (StatisticsRecorder::Histograms::iterator it = histograms.begin();
       histograms.end() != it;
       it++) {
    (*it)->SetFlags(Histogram::kIPCSerializationSourceFlag);
    UploadHistrogram(**it, &pickled_histograms);
  }
  // Send the sequence number and list of pickled histograms over synchronous
  // IPC.
  RenderThread::current()->Send(
      new ViewHostMsg_RendererHistograms(
          sequence_number, pickled_histograms));
}

// Extract snapshot data, remember what we've seen so far, and then send off the
// delta to the browser.
void RendererHistogramSnapshots::UploadHistrogram(
    const Histogram& histogram,
    HistogramPickledList* pickled_histograms) {
  // Get up-to-date snapshot of sample stats.
  Histogram::SampleSet snapshot;
  histogram.SnapshotSample(&snapshot);
  const std::string& histogram_name = histogram.histogram_name();

  int corruption = histogram.FindCorruption(snapshot);
  if (corruption) {
    NOTREACHED();
    // Don't send corrupt data to the browser.
    UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesRenderer",
                              corruption, Histogram::NEVER_EXCEEDED_VALUE);
    typedef std::map<std::string, int> ProblemMap;
    static ProblemMap* inconsistencies = new ProblemMap;
    int old_corruption = (*inconsistencies)[histogram_name];
    if (old_corruption == (corruption | old_corruption))
      return;  // We've already seen this corruption for this histogram.
    (*inconsistencies)[histogram_name] |= corruption;
    UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesRendererUnique",
                              corruption, Histogram::NEVER_EXCEEDED_VALUE);
    return;
  }

  // Find the already sent stats, or create an empty set.
  LoggedSampleMap::iterator it = logged_samples_.find(histogram_name);
  Histogram::SampleSet* already_logged;
  if (logged_samples_.end() == it) {
    // Add new entry.
    already_logged = &logged_samples_[histogram.histogram_name()];
    already_logged->Resize(histogram);  // Complete initialization.
  } else {
    already_logged = &(it->second);
    // Deduct any stats we've already logged from our snapshot.
    snapshot.Subtract(*already_logged);
  }

  // Snapshot now contains only a delta to what we've already_logged.

  if (snapshot.TotalCount() > 0) {
    UploadHistogramDelta(histogram, snapshot, pickled_histograms);
    // Add new data into our running total.
    already_logged->Add(snapshot);
  }
}

void RendererHistogramSnapshots::UploadHistogramDelta(
    const Histogram& histogram,
    const Histogram::SampleSet& snapshot,
    HistogramPickledList* pickled_histograms) {
  DCHECK(0 != snapshot.TotalCount());
  snapshot.CheckSize(histogram);

  std::string histogram_info =
      Histogram::SerializeHistogramInfo(histogram, snapshot);
  pickled_histograms->push_back(histogram_info);
}
