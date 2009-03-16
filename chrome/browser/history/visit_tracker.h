// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_VISIT_TRACKER_H__
#define CHROME_BROWSER_HISTORY_VISIT_TRACKER_H__

#include <list>
#include <map>

#include "base/basictypes.h"
#include "chrome/browser/history/history_types.h"

namespace history {

// Tracks history transitions between pages. The history backend uses this to
// link up page transitions to form a chain of page visits, and to set the
// transition type properly.
//
// This class is not thread safe.
class VisitTracker {
 public:
  VisitTracker();
  ~VisitTracker();

  // Notifications -------------------------------------------------------------

  void AddVisit(const void* host,
                int32 page_id,
                const GURL& url,
                VisitID visit_id);

  // When a RenderProcessHost is destroyed, we want to clear out our saved
  // transitions/visit IDs for it.
  void NotifyRenderProcessHostDestruction(const void* host);

  // Querying ------------------------------------------------------------------

  // Returns the visit ID for the transition given information about the visit
  // supplied by the renderer. We will return 0 if there is no appropriate
  // referring visit.
  VisitID GetLastVisit(const void* host, int32 page_id, const GURL& url);

 private:
  struct Transition {
    GURL url;          // URL that the event happened to.
    int32 page_id;     // ID generated by the render process host.
    VisitID visit_id;  // Visit ID generated by history.
  };
  typedef std::vector<Transition> TransitionList;
  typedef std::map<const void*, TransitionList*> HostList;

  // Expires oldish items in the given transition list. This keeps the list
  // size small by removing items that are unlikely to be needed, which is
  // important for GetReferrer which does brute-force searches of this list.
  void CleanupTransitionList(TransitionList* transitions);

  // Maps render view hosts to lists of recent transitions.
  HostList hosts_;

  DISALLOW_EVIL_CONSTRUCTORS(VisitTracker);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_VISIT_TRACKER_H__
