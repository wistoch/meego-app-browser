// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_x11/gl_video_renderer.h"

#include <dlfcn.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"

GlVideoRenderer* GlVideoRenderer::instance_ = NULL;

GlVideoRenderer::GlVideoRenderer(Display* display, Window window)
    : display_(display),
      window_(window),
      new_frame_(false),
      gl_context_(NULL) {
}

GlVideoRenderer::~GlVideoRenderer() {
}

// static
bool GlVideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  return ParseMediaFormat(media_format, &width, &height);
}

void GlVideoRenderer::OnStop() {
  glXMakeCurrent(display_, 0, NULL);
  glXDestroyContext(display_, gl_context_);
}

static GLXContext InitGLContext(Display* display, Window window) {
  // Some versions of NVIDIA's GL libGL.so include a broken version of
  // dlopen/dlsym, and so linking it into chrome breaks it. So we dynamically
  // load it, and use glew to dynamically resolve symbols.
  // See http://code.google.com/p/chromium/issues/detail?id=16800
  void* handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    LOG(ERROR) << "Could not find libGL.so.1";
    return NULL;
  }
  if (glxewInit() != GLEW_OK) {
    LOG(ERROR) << "GLXEW failed initialization";
    return NULL;
  }

  XWindowAttributes attributes;
  XGetWindowAttributes(display, window, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo* visual_info_list = XGetVisualInfo(display, VisualIDMask,
                                                 &visual_info_template,
                                                 &visual_info_count);
  GLXContext context = NULL;
  for (int i = 0; i < visual_info_count && !context; ++i) {
    context = glXCreateContext(display, visual_info_list + i, 0,
                               True /* Direct rendering */);
  }

  XFree(visual_info_list);
  if (!context) {
    return NULL;
  }

  if (!glXMakeCurrent(display, window, context)) {
    glXDestroyContext(display, context);
    return NULL;
  }

  if (glewInit() != GLEW_OK) {
    LOG(ERROR) << "GLEW failed initialization";
    glXDestroyContext(display, context);
    return NULL;
  }

  if (!glewIsSupported("GL_VERSION_2_0")) {
    LOG(ERROR) << "GL implementation doesn't support GL version 2.0";
    glXDestroyContext(display, context);
    return NULL;
  }

  return context;
}

// Matrix used for the YUV to RGB conversion.
static const float kYUV2RGB[9] = {
  1.f, 0.f, 1.403f,
  1.f, -.344f, -.714f,
  1.f, 1.772f, 0.f,
};

// Vertices for a full screen quad.
static const float kVertices[8] = {
  -1.f, 1.f,
  -1.f, -1.f,
  1.f, 1.f,
  1.f, -1.f,
};

// Texture Coordinates mapping the entire texture.
static const float kTextureCoords[8] = {
  0, 0,
  0, 1,
  1, 0,
  1, 1,
};

// Pass-through vertex shader.
static const char kVertexShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "\n"
    "void main() {\n"
    "  interp_tc = in_tc;\n"
    "  gl_Position = in_pos;\n"
    "}\n";

// YUV to RGB pixel shader. Loads a pixel from each plane and pass through the
// matrix.
static const char kFragmentShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D y_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D v_tex;\n"
    "uniform mat3 yuv2rgb;\n"
    "\n"
    "void main() {\n"
    "  float y = texture2D(y_tex, interp_tc).x;\n"
    "  float u = texture2D(u_tex, interp_tc).r - .5;\n"
    "  float v = texture2D(v_tex, interp_tc).r - .5;\n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
    "}\n";

// Buffer size for compile errors.
static const unsigned int kErrorSize = 4096;

bool GlVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  if (!ParseMediaFormat(decoder->media_format(), &width_, &height_))
    return false;

  LOG(INFO) << "Initializing GL Renderer...";

  // Resize the window to fit that of the video.
  XResizeWindow(display_, window_, width_, height_);

  gl_context_ = InitGLContext(display_, window_);
  if (!gl_context_)
    return false;

  glMatrixMode(GL_MODELVIEW);

  // Create 3 textures, one for each plane, and bind them to different
  // texture units.
  glGenTextures(media::VideoSurface::kNumYUVPlanes, textures_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glEnable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glEnable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, textures_[2]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glEnable(GL_TEXTURE_2D);

  GLuint program = glCreateProgram();

  // Create our YUV->RGB shader.
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  const char* vs_source = kVertexShader;
  int vs_size = sizeof(kVertexShader);
  glShaderSource(vertex_shader, 1, &vs_source, &vs_size);
  glCompileShader(vertex_shader);
  int result = GL_FALSE;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(vertex_shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glAttachShader(program, vertex_shader);
  glDeleteShader(vertex_shader);

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  const char* ps_source = kFragmentShader;
  int ps_size = sizeof(kFragmentShader);
  glShaderSource(fragment_shader, 1, &ps_source, &ps_size);
  glCompileShader(fragment_shader);
  result = GL_FALSE;
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(fragment_shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glAttachShader(program, fragment_shader);
  glDeleteShader(fragment_shader);

  glLinkProgram(program);
  result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetProgramInfoLog(program, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glUseProgram(program);
  glDeleteProgram(program);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(program, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(program, "v_tex"), 2);
  int yuv2rgb_location = glGetUniformLocation(program, "yuv2rgb");
  glUniformMatrix3fv(yuv2rgb_location, 1, GL_TRUE, kYUV2RGB);

  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoords);

  // We are getting called on a thread. Release the context so that it can be
  // made current on the main thread.
  glXMakeCurrent(display_, 0, NULL);

  // Save this instance.
  DCHECK(!instance_);
  instance_ = this;
  return true;
}

void GlVideoRenderer::OnFrameAvailable() {
  AutoLock auto_lock(lock_);
  new_frame_ = true;
}

void GlVideoRenderer::Paint() {
  // Use |new_frame_| to prevent overdraw since Paint() is called more
  // often than needed. It is OK to lock only this flag and we don't
  // want to lock the whole function because this method takes a long
  // time to complete.
  {
    AutoLock auto_lock(lock_);
    if (!new_frame_)
      return;
    new_frame_ = false;
  }

  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);

  if (!video_frame)
    return;

  // Convert YUV frame to RGB.
  media::VideoSurface frame_in;
  if (video_frame->Lock(&frame_in)) {
    DCHECK(frame_in.format == media::VideoSurface::YV12 ||
           frame_in.format == media::VideoSurface::YV16);
    DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
           frame_in.strides[media::VideoSurface::kVPlane]);
    DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);

      if (glXGetCurrentContext() != gl_context_ ||
          glXGetCurrentDrawable() != window_) {
        glXMakeCurrent(display_, window_, gl_context_);
      }
      for (unsigned int i = 0; i < media::VideoSurface::kNumYUVPlanes; ++i) {
        unsigned int width = (i == media::VideoSurface::kYPlane) ?
            frame_in.width : frame_in.width / 2;
        unsigned int height = (i == media::VideoSurface::kYPlane ||
                               frame_in.format == media::VideoSurface::YV16) ?
            frame_in.height : frame_in.height / 2;
        glActiveTexture(GL_TEXTURE0 + i);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame_in.strides[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, frame_in.data[i]);
      }
    video_frame->Unlock();
  } else {
    NOTREACHED();
  }

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glXSwapBuffers(display_, window_);
}
