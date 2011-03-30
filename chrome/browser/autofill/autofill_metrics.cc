// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/metrics/histogram.h"

AutofillMetrics::AutofillMetrics() {
}

AutofillMetrics::~AutofillMetrics() {
}

void AutofillMetrics::Log(CreditCardInfoBarMetric metric) const {
  DCHECK(metric < NUM_CREDIT_CARD_INFO_BAR_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_CREDIT_CARD_INFO_BAR_METRICS);
}

void AutofillMetrics::Log(ServerQueryMetric metric) const {
  DCHECK(metric < NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

void AutofillMetrics::Log(QualityMetric metric,
                          const std::string& experiment_id) const {
  DCHECK(metric < NUM_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  UMA_HISTOGRAM_ENUMERATION(histogram_name, metric, NUM_QUALITY_METRICS);
}

void AutofillMetrics::LogStoredProfileCount(size_t num_profiles) const {
  UMA_HISTOGRAM_COUNTS("Autofill.StoredProfileCount", num_profiles);
}

void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) const {
  printf("LogAddressSuggestionsCount(%d)\n", (int)num_suggestions);
  UMA_HISTOGRAM_COUNTS("Autofill.AddressSuggestionsCount", num_suggestions);
}

