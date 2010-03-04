// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_

class Balloon;
class Notification;
class Profile;

namespace gfx {
class Size;
}  // namespace gfx

class BalloonCollection {
 public:
  class BalloonSpaceChangeListener {
   public:
    virtual ~BalloonSpaceChangeListener() {}

    // Called when there is more or less space for balloons due to
    // monitor size changes or balloons disappearing.
    virtual void OnBalloonSpaceChanged() = 0;
  };

  static BalloonCollection* Create();

  BalloonCollection()
      : space_change_listener_(NULL) {
  }

  virtual ~BalloonCollection() {}

  // Adds a new balloon for the specified notification.
  virtual void Add(const Notification& notification,
                   Profile* profile) = 0;

  // Removes a balloon from the collection if present.  Returns
  // true if anything was removed.
  virtual bool Remove(const Notification& notification) = 0;

  // Is there room to add another notification?
  virtual bool HasSpace() const = 0;

  // Request the resizing of a balloon.
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size) = 0;

  // Update for new screen dimensions.
  virtual void DisplayChanged() = 0;

  // Inform the collection that a balloon was closed.
  virtual void OnBalloonClosed(Balloon* source) = 0;

  BalloonSpaceChangeListener* space_change_listener() {
    return space_change_listener_;
  }
  void set_space_change_listener(BalloonSpaceChangeListener* listener) {
    space_change_listener_ = listener;
  }

 protected:
  // Non-owned pointer to an object listening for space changes.
  BalloonSpaceChangeListener* space_change_listener_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
