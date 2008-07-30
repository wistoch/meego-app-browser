// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_VIEWS_INFO_BUBBLE_H__
#define CHROME_BROWSER_VIEWS_INFO_BUBBLE_H__

#include "chrome/common/slide_animation.h"
#include "chrome/views/hwnd_view_container.h"

// InfoBubble is used to display an arbitrary view above all other windows.
// Think of InfoBubble as a tooltip that allows you to embed an arbitrary view
// in the tooltip. Additionally the InfoBubble renders an arrow pointing at
// the region the info bubble is providing the information about.
//
// To use an InfoBubble invoke Show and it'll take care of the rest. InfoBubble
// (or rather ContentView) insets the content view for you, so that the
// content typically shouldn't have any additional margins around the view.

class BrowserWindow;
class InfoBubble;

class InfoBubbleDelegate {
 public:
  // Called when the InfoBubble is closing and is about to be deleted.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble) = 0;

  // Whether the InfoBubble should be closed when the Esc key is pressed.
  virtual bool CloseOnEscape() = 0;
};

class InfoBubble : public ChromeViews::HWNDViewContainer,
                   public ChromeViews::AcceleratorTarget,
                   public AnimationDelegate {
 public:
  // Shows the InfoBubble. The InfoBubble is parented to parent_hwnd, contains
  // the View content and positioned relative to the screen position
  // position_relative_to. Show takes ownership of content and deletes the
  // create InfoBubble when another window is activated. You can explicitly
  // close the bubble by invoking Close.  A delegate may optionally be provided
  // to be notified when the InfoBubble is closed and to prevent the InfoBubble
  // from being closed when the Escape key is pressed (which is the default
   // behavior if there is no delegate).
  static InfoBubble* Show(HWND parent_hwnd,
                          const gfx::Rect& position_relative_to,
                          ChromeViews::View* content,
                          InfoBubbleDelegate* delegate);

  InfoBubble();
  virtual ~InfoBubble();

  // Creates the InfoBubble.
  void Init(HWND parent_hwnd,
            const gfx::Rect& position_relative_to,
            ChromeViews::View* content);

  // Sets the delegate for that InfoBubble.
  void SetDelegate(InfoBubbleDelegate* delegate) { delegate_ = delegate; }

  // The InfoBubble is automatically closed when it loses activation status.
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);

  // Return our rounded window shape.
  virtual void OnSize(UINT param, const CSize& size);

  // Overridden to notify the owning ChromeFrame the bubble is closing.
  virtual void Close();

  // AcceleratorTarget method:
  virtual bool AcceleratorPressed(const ChromeViews::Accelerator& accelerator);

  // AnimationDelegate Implementation
  virtual void AnimationProgressed(const Animation* animation);

 protected:

  // InfoBubble::CreateContentView() creates one of these. ContentView houses
  // the supplied content as its only child view, renders the arrow/border of
  // the bubble and sizes the content.
  class ContentView : public ChromeViews::View {
   public:
    // Possible edges the arrow is aligned along.
    enum ArrowEdge {
      TOP_LEFT     = 0,
      TOP_RIGHT    = 1,
      BOTTOM_LEFT  = 2,
      BOTTOM_RIGHT = 3
    };

    // Creates the ContentView. The supplied view is added as the only child of
    // the ContentView.
    ContentView(ChromeViews::View* content, InfoBubble* host);

    virtual ~ContentView() {}

    // Returns the bounds for the window to contain this view.
    //
    // This invokes the method of the same name that doesn't take an HWND, if
    // the returned bounds don't fit on the monitor containing parent_hwnd,
    // the arrow edge is adjusted.
    virtual gfx::Rect CalculateWindowBounds(
        HWND parent_hwnd,
        const gfx::Rect& position_relative_to);

    // Sets the edge the arrow is rendered at.
    void SetArrowEdge(ArrowEdge arrow_edge) { arrow_edge_ = arrow_edge; }

    // Returns the preferred size, which is the sum of the preferred size of
    // the content and the border/arrow.
    virtual void GetPreferredSize(CSize* pref);

    // Positions the content relative to the border.
    virtual void Layout();

    virtual void DidChangeBounds(const CRect& previous, const CRect& current) {
      Layout();
    }

    // Return the mask for the content view.
    HRGN GetMask(const CSize& size);

    // Paints the background and arrow appropriately.
    virtual void Paint(ChromeCanvas* canvas);

    // Returns true if the arrow is positioned along the top edge of the
    // view. If this returns false the arrow is positioned along the bottom
    // edge.
    bool IsTop() { return (arrow_edge_ & 2) == 0; }

    // Returns true if the arrow is positioned along the left edge of the
    // view. If this returns false the arrow is positioned along the right edge.
    bool IsLeft() { return (arrow_edge_ & 1) == 0; }

   private:

    // Returns the bounds for the window containing us based on the current
    // arrow edge.
    gfx::Rect CalculateWindowBounds(const gfx::Rect& position_relative_to);

    // Edge to draw the arrow at.
    ArrowEdge arrow_edge_;

    // The bubble we're in.
    InfoBubble* host_;

    DISALLOW_EVIL_CONSTRUCTORS(ContentView);
  };

  // Creates and return a new ContentView containing content.
  virtual ContentView* CreateContentView(ChromeViews::View* content);

  // Returns the BrowserWindow that owns this InfoBubble.
  BrowserWindow* GetHostingWindow();

 private:
  // The delegate notified when the InfoBubble is closed.
  InfoBubbleDelegate* delegate_;

  // The content view contained by the infobubble.
  ContentView* content_view_;

  // The fade-in animation.
  scoped_ptr<SlideAnimation> fade_animation_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_INFO_BUBBLE_H__
