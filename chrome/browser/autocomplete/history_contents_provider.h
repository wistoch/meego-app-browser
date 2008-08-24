// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H__
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H__

#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/history/history.h"

// HistoryContentsProvider is an AutocompleteProvider that provides results from
// the contents (body and/or title) of previously visited pages.  Results are
// obtained asynchronously from the history service.
class HistoryContentsProvider : public AutocompleteProvider {
 public:
  HistoryContentsProvider(ACProviderListener* listener, Profile* profile)
      : AutocompleteProvider(listener, profile, "HistoryContents"),
        history_service_(NULL),
        have_results_(false) {
    DCHECK(profile);
  }

#ifdef UNIT_TEST
  HistoryContentsProvider(ACProviderListener* listener,
                          HistoryService* history_service)
      : AutocompleteProvider(listener, NULL, "HistoryContents"),
        history_service_(history_service),
        have_results_(false) {
  }
#endif

  // As necessary asks the history service for the relevant results. When
  // done SetResults is invoked.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     bool synchronous_only);

  virtual void Stop();

  // Returns the total number of matches available in the database, up to
  // kMaxMatchCount, whichever is smaller.
  // Return value is only valid if done() returns true.
  size_t db_match_count() const { return db_match_count_; }

  // The maximum match count we'll report. If the db_match_count is greater
  // than this, it will be clamped to this result.
  static const int kMaxMatchCount = 50;

 private:
  void QueryComplete(HistoryService::Handle handle,
                     history::QueryResults* results);

  // Converts each MatchingPageResult in results_ to an AutocompleteMatch and
  // adds it to matches_.
  void ConvertResults();

  // Creates and returns an AutocompleteMatch from a MatchingPageResult.
  AutocompleteMatch ResultToMatch(const history::URLResult& result,
                                  int score);

  // Adds ACMatchClassifications to match from the offset positions in
  // page_result.
  void ClassifyDescription(const history::URLResult& result,
                           AutocompleteMatch* match) const;

  // Calculates and returns the relevance of a particular result. See the
  // chart in autocomplete.h for the list of values this returns.
  int CalculateRelevance(const history::URLResult& result);

  CancelableRequestConsumerT<int, 0> request_consumer_;

  // This is only non-null for testing, otherwise the HistoryService from the
  // Profile is used.
  HistoryService* history_service_;

  // The number of times we're returned each different type of result. These are
  // used by CalculateRelevance. Initialized in Start.
  int star_title_count_;
  int star_contents_count_;
  int title_count_;
  int contents_count_;

  // Current autocomplete input type.
  AutocompleteInput::Type input_type_;

  // Results from most recent query. These are cached so we don't have to
  // re-issue queries for "minor changes" (which don't affect this provider).
  history::QueryResults results_;

  // Whether results_ is valid (so we can tell invalid apart from empty).
  bool have_results_;

  // Current query string.
  std::wstring query_;

  // Total number of matches available in the database.
  int db_match_count_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryContentsProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_CONTENTS_PROVIDER_H__

