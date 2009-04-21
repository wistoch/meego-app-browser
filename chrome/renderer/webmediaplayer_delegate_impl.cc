// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/webmediaplayer_delegate_impl.h"

#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/media/data_source_impl.h"
#include "chrome/renderer/media/video_renderer_impl.h"
#include "chrome/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#if defined(OS_WIN)
// FFmpeg is not ready for Linux and Mac yet.
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"

using WebKit::WebRect;
using WebKit::WebSize;

/////////////////////////////////////////////////////////////////////////////
// Task to be posted on main thread that fire WebMediaPlayer methods.

class NotifyWebMediaPlayerTask : public CancelableTask {
 public:
  NotifyWebMediaPlayerTask(WebMediaPlayerDelegateImpl* delegate,
                           WebMediaPlayerMethod method)
      : delegate_(delegate),
        method_(method) {}

  virtual void Run() {
    if (delegate_) {
      (delegate_->web_media_player()->*(method_))();
      delegate_->DidTask(this);
    }
  }

  virtual void Cancel() {
    delegate_ = NULL;
  }

 private:
  WebMediaPlayerDelegateImpl* delegate_;
  WebMediaPlayerMethod method_;

  DISALLOW_COPY_AND_ASSIGN(NotifyWebMediaPlayerTask);
};

/////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerDelegateImpl implementation

WebMediaPlayerDelegateImpl::WebMediaPlayerDelegateImpl(RenderView* view)
    : network_state_(webkit_glue::WebMediaPlayer::EMPTY),
      ready_state_(webkit_glue::WebMediaPlayer::HAVE_NOTHING),
      main_loop_(NULL),
      filter_factory_(new media::FilterFactoryCollection()),
      video_renderer_(NULL),
      web_media_player_(NULL),
      view_(view),
      tasks_(kLastTaskIndex) {
  // TODO(hclam): Add filter factory for demuxer and decoders.
#if defined(OS_WIN)
  // FFmpeg is not ready for Linux and Mac yet.
  filter_factory_->AddFactory(media::FFmpegDemuxer::CreateFilterFactory());
  filter_factory_->AddFactory(media::FFmpegAudioDecoder::CreateFactory());
  filter_factory_->AddFactory(media::FFmpegVideoDecoder::CreateFactory());
#endif
  filter_factory_->AddFactory(AudioRendererImpl::CreateFactory(
      view_->audio_message_filter()));
  filter_factory_->AddFactory(VideoRendererImpl::CreateFactory(this));
  filter_factory_->AddFactory(DataSourceImpl::CreateFactory(this));
}

WebMediaPlayerDelegateImpl::~WebMediaPlayerDelegateImpl() {
  pipeline_.Stop();

  // Cancel all tasks posted on the main_loop_.
  CancelAllTasks();

  // After cancelling all tasks, we are sure there will be no calls to
  // web_media_player_, so we are safe to delete it.
  if (web_media_player_) {
    delete web_media_player_;
  }

  // Finally tell the main_loop_ we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
}

void WebMediaPlayerDelegateImpl::Initialize(
    webkit_glue::WebMediaPlayer* media_player) {
  DCHECK(!web_media_player_);
  web_media_player_ = media_player;

  // Saves the current message loop.
  DCHECK(!main_loop_);
  main_loop_ = MessageLoop::current();

  // Also we want to be notified of main_loop_ destruction.
  main_loop_->AddDestructionObserver(this);
}

void WebMediaPlayerDelegateImpl::Load(const GURL& url) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // Initialize the pipeline
  pipeline_.Start(filter_factory_.get(), url.spec(),
      NewCallback(this, &WebMediaPlayerDelegateImpl::DidInitializePipeline));
}

void WebMediaPlayerDelegateImpl::CancelLoad() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): Calls to render_view_ to stop resource load
}

void WebMediaPlayerDelegateImpl::Play() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): We should restore the previous playback rate rather than
  // having it at 1.0.
  pipeline_.SetPlaybackRate(1.0f);
}

void WebMediaPlayerDelegateImpl::Pause() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetPlaybackRate(0.0f);
}

void WebMediaPlayerDelegateImpl::Stop() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // We can fire Stop() multiple times.
  pipeline_.Stop();
}

void WebMediaPlayerDelegateImpl::Seek(float seconds) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.Seek(base::TimeDelta::FromSeconds(static_cast<int64>(seconds)));
}

void WebMediaPlayerDelegateImpl::SetEndTime(float seconds) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): add method call when it has been implemented.
  return;
}

void WebMediaPlayerDelegateImpl::SetPlaybackRate(float rate) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetPlaybackRate(rate);
}

void WebMediaPlayerDelegateImpl::SetVolume(float volume) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  pipeline_.SetVolume(volume);
}

void WebMediaPlayerDelegateImpl::SetVisible(bool visible) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

bool WebMediaPlayerDelegateImpl::IsTotalBytesKnown() {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetTotalBytes() != 0;
}

bool WebMediaPlayerDelegateImpl::IsVideo() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  return width != 0 && height != 0;
}

size_t WebMediaPlayerDelegateImpl::GetWidth() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  return width;
}

size_t WebMediaPlayerDelegateImpl::GetHeight() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  return height;
}

bool WebMediaPlayerDelegateImpl::IsPaused() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerDelegateImpl::IsSeeking() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return false;
}

float WebMediaPlayerDelegateImpl::GetDuration() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetDuration().InSecondsF());
}

float WebMediaPlayerDelegateImpl::GetCurrentTime() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetTime().InSecondsF());
}


float WebMediaPlayerDelegateImpl::GetPlayBackRate() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetPlaybackRate();
}

float WebMediaPlayerDelegateImpl::GetVolume() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetVolume();
}

int WebMediaPlayerDelegateImpl::GetDataRate() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

float WebMediaPlayerDelegateImpl::GetMaxTimeBuffered() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return static_cast<float>(pipeline_.GetBufferedTime().InSecondsF());
}

float WebMediaPlayerDelegateImpl::GetMaxTimeSeekable() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  // TODO(hclam): add this method when pipeline has this method implemented.
  return 0.0f;
}

int64 WebMediaPlayerDelegateImpl::GetBytesLoaded() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetBufferedBytes();
}

int64 WebMediaPlayerDelegateImpl::GetTotalBytes() const {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  return pipeline_.GetTotalBytes();
}

void WebMediaPlayerDelegateImpl::SetSize(const WebSize& size) {
  DCHECK(main_loop_ && MessageLoop::current() == main_loop_);

  if (video_renderer_) {
    // TODO(scherkus): Change API to use SetSize().
    video_renderer_->SetRect(gfx::Rect(0, 0, size.width, size.height));
  }
}

void WebMediaPlayerDelegateImpl::Paint(skia::PlatformCanvas *canvas,
                                       const WebRect& rect) {
  if (video_renderer_) {
    video_renderer_->Paint(canvas, rect);
  }
}

void WebMediaPlayerDelegateImpl::WillDestroyCurrentMessageLoop() {
  pipeline_.Stop();
}

void WebMediaPlayerDelegateImpl::DidInitializePipeline(bool successful) {
  if (successful) {
    // Since we have initialized the pipeline, we should be able to play it.
    // And we skip LOADED_METADATA state and starting with LOADED_FIRST_FRAME.
    ready_state_ = webkit_glue::WebMediaPlayer::HAVE_ENOUGH_DATA;
    network_state_ = webkit_glue::WebMediaPlayer::LOADED;
  } else {
    // TODO(hclam): should use pipeline_.GetError() to determine the state
    // properly and reports error using MediaError.
    ready_state_ = webkit_glue::WebMediaPlayer::HAVE_NOTHING;
    network_state_ = webkit_glue::WebMediaPlayer::NETWORK_ERROR;
  }

  PostTask(kNetworkStateTaskIndex,
           &webkit_glue::WebMediaPlayer::NotifyNetworkStateChange);
  PostTask(kReadyStateTaskIndex,
           &webkit_glue::WebMediaPlayer::NotifyReadyStateChange);
}

void WebMediaPlayerDelegateImpl::SetVideoRenderer(
    VideoRendererImpl* video_renderer) {
  video_renderer_ = video_renderer;
}

void WebMediaPlayerDelegateImpl::DidTask(CancelableTask* task) {
  AutoLock auto_lock(task_lock_);

  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i] == task) {
      tasks_[i] = NULL;
      return;
    }
  }
  NOTREACHED();
}

void WebMediaPlayerDelegateImpl::CancelAllTasks() {
  AutoLock auto_lock(task_lock_);
  // Loop through the list of tasks and cancel tasks that are still alive.
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i])
      tasks_[i]->Cancel();
  }
}

void WebMediaPlayerDelegateImpl::PostTask(int index,
                                          WebMediaPlayerMethod method) {
  DCHECK(main_loop_);

  AutoLock auto_lock(task_lock_);
  if(!tasks_[index]) {
    CancelableTask* task = new NotifyWebMediaPlayerTask(this, method);
    tasks_[index] = task;
    main_loop_->PostTask(FROM_HERE, task);
  }
}

void WebMediaPlayerDelegateImpl::PostRepaintTask() {
  PostTask(kRepaintTaskIndex, &webkit_glue::WebMediaPlayer::Repaint);
}
