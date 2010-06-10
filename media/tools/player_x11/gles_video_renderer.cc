// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_x11/gles_video_renderer.h"

#include <dlfcn.h>
#include <EGL/eglext.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "media/base/buffers.h"
#include "media/base/pipeline.h"
#include "media/base/filter_host.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"

GlesVideoRenderer* GlesVideoRenderer::instance_ = NULL;

GlesVideoRenderer::GlesVideoRenderer(Display* display, Window window)
    : egl_create_image_khr_(NULL),
      egl_destroy_image_khr_(NULL),
      display_(display),
      window_(window),
      egl_display_(NULL),
      egl_surface_(NULL),
      egl_context_(NULL),
      glx_thread_message_loop_(NULL) {
}

GlesVideoRenderer::~GlesVideoRenderer() {
}

// static
bool GlesVideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  bool uses_egl_image_ = false;
  return ParseMediaFormat(media_format, &width, &height, &uses_egl_image_);
}

void GlesVideoRenderer::OnStop() {
  // TODO(hclam): Context switching seems to be broek so the following
  // calls may fail. Need to fix them.
  eglMakeCurrent(egl_display_, EGL_NO_SURFACE,
                 EGL_NO_SURFACE, EGL_NO_CONTEXT);
  for (size_t i = 0; i < egl_frames_.size(); ++i) {
    scoped_refptr<media::VideoFrame> frame = egl_frames_[i].first;
    if (frame->private_buffer())
      egl_destroy_image_khr_(egl_display_, frame->private_buffer());
    if (egl_frames_[i].second)
      glDeleteTextures(1, &egl_frames_[i].second);
  }
  egl_frames_.clear();
  eglDestroyContext(egl_display_, egl_context_);
  eglDestroySurface(egl_display_, egl_surface_);
}

// Matrix used for the YUV to RGB conversion.
static const float kYUV2RGB[9] = {
  1.f, 1.f, 1.f,
  0.f, -.344f, 1.772f,
  1.403f, -.714f, 0.f,
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

// Texture Coordinates mapping the entire texture for EGL image.
static const float kTextureCoordsEgl[8] = {
  0, 1,
  0, 0,
  1, 1,
  1, 0,
};

// Pass-through vertex shader.
static const char kVertexShader[] =
    "precision highp float; precision highp int;\n"
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
    "precision mediump float; precision mediump int;\n"
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D y_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D v_tex;\n"
    "uniform mat3 yuv2rgb;\n"
    "uniform float half;\n"
    "\n"
    "void main() {\n"
    "  float y = texture2D(y_tex, interp_tc).x;\n"
    "  float u = texture2D(u_tex, interp_tc).r - half;\n"
    "  float v = texture2D(v_tex, interp_tc).r - half;\n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
    "}\n";

// Color shader for EGLImage.
static const char kFragmentShaderEgl[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D tex;\n"
    "\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, interp_tc);\n"
    "}\n";

// Buffer size for compile errors.
static const unsigned int kErrorSize = 4096;

bool GlesVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  if (!ParseMediaFormat(decoder->media_format(), &width_, &height_,
                        &uses_egl_image_))
    return false;

  LOG(INFO) << "Initializing GLES Renderer...";

  // Save this instance.
  DCHECK(!instance_);
  instance_ = this;
  return true;
}

void GlesVideoRenderer::OnFrameAvailable() {
  if (glx_thread_message_loop()) {
    glx_thread_message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &GlesVideoRenderer::Paint));
  }
}

void GlesVideoRenderer::Paint() {
  // Initialize GLES here to avoid context switching. Some drivers doesn't
  // like switching context between threads.
  static bool initialized = false;
  if (!initialized && !InitializeGles()) {
    initialized = true;
    host()->SetError(media::PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }
  initialized = true;

  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);

  if (!video_frame.get()) {
    return;
  }

  if (uses_egl_image_) {
    if (media::VideoFrame::TYPE_EGL_IMAGE == video_frame->type()) {

      GLuint texture = FindTexture(video_frame);
      if (texture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        eglSwapBuffers(egl_display_, egl_surface_);
      }
    }
    return;
  }

  // Convert YUV frame to RGB.
  DCHECK(video_frame->format() == media::VideoFrame::YV12 ||
         video_frame->format() == media::VideoFrame::YV16);
  DCHECK(video_frame->stride(media::VideoFrame::kUPlane) ==
         video_frame->stride(media::VideoFrame::kVPlane));
  DCHECK(video_frame->planes() == media::VideoFrame::kNumYUVPlanes);

  for (unsigned int i = 0; i < media::VideoFrame::kNumYUVPlanes; ++i) {
    unsigned int width = (i == media::VideoFrame::kYPlane) ?
        video_frame->width() : video_frame->width() / 2;
    unsigned int height = (i == media::VideoFrame::kYPlane ||
                          video_frame->format() == media::VideoFrame::YV16) ?
                          video_frame->height() : video_frame->height() / 2;
    glActiveTexture(GL_TEXTURE0 + i);
    // GLES2 supports a fixed set of unpack alignments that should match most
    // of the time what ffmpeg outputs.
    // TODO(piman): check if it is more efficient to prefer higher
    // alignments.
    unsigned int stride = video_frame->stride(i);
    uint8* data = video_frame->data(i);
    if (stride == width) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    } else if (stride == ((width + 1) & ~1)) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    } else if (stride == ((width + 3) & ~3)) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    } else if (stride == ((width + 7) & ~7)) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    } else {
      // Otherwise do it line-by-line.
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                   GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
      for (unsigned int y = 0; y < height; ++y) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1,
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
        data += stride;
      }
      continue;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
  }

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  eglSwapBuffers(egl_display_, egl_surface_);
}

// find if texture exists corresponding to video_frame
GLuint GlesVideoRenderer::FindTexture(
    scoped_refptr<media::VideoFrame> video_frame) {
  for (size_t i = 0; i < egl_frames_.size(); ++i) {
    scoped_refptr<media::VideoFrame> frame = egl_frames_[i].first;
    if (video_frame->private_buffer() == frame->private_buffer())
      return egl_frames_[i].second;
  }
  return NULL;
}

bool GlesVideoRenderer::InitializeGles() {
  // Resize the window to fit that of the video.
  XResizeWindow(display_, window_, width_, height_);

  egl_display_ = eglGetDisplay(display_);
  if (eglGetError() != EGL_SUCCESS) {
    DLOG(ERROR) << "eglGetDisplay failed.";
    return false;
  }

  EGLint major;
  EGLint minor;
  if (!eglInitialize(egl_display_, &major, &minor)) {
    DLOG(ERROR) << "eglInitialize failed.";
    return false;
  }
  DLOG(INFO) << "EGL vendor:" << eglQueryString(egl_display_, EGL_VENDOR);
  DLOG(INFO) << "EGL version:" << eglQueryString(egl_display_, EGL_VERSION);
  DLOG(INFO) << "EGL extensions:"
             << eglQueryString(egl_display_, EGL_EXTENSIONS);
  DLOG(INFO) << "EGL client apis:"
             << eglQueryString(egl_display_, EGL_CLIENT_APIS);

  EGLint attribs[] = {
    EGL_RED_SIZE,       5,
    EGL_GREEN_SIZE,     6,
    EGL_BLUE_SIZE,      5,
    EGL_DEPTH_SIZE,     16,
    EGL_STENCIL_SIZE,   0,
    EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
    EGL_NONE
  };

  EGLint num_configs = -1;
  if (!eglGetConfigs(egl_display_, NULL, 0, &num_configs)) {
    DLOG(ERROR) << "eglGetConfigs failed.";
    return false;
  }

  EGLConfig config;
  if (!eglChooseConfig(egl_display_, attribs, &config, 1, &num_configs)) {
    DLOG(ERROR) << "eglChooseConfig failed.";
    return false;
  }

  EGLint red_size, green_size, blue_size, alpha_size, depth_size, stencil_size;
  eglGetConfigAttrib(egl_display_, config, EGL_RED_SIZE, &red_size);
  eglGetConfigAttrib(egl_display_, config, EGL_GREEN_SIZE, &green_size);
  eglGetConfigAttrib(egl_display_, config, EGL_BLUE_SIZE, &blue_size);
  eglGetConfigAttrib(egl_display_, config, EGL_ALPHA_SIZE, &alpha_size);
  eglGetConfigAttrib(egl_display_, config, EGL_DEPTH_SIZE, &depth_size);
  eglGetConfigAttrib(egl_display_, config, EGL_STENCIL_SIZE, &stencil_size);
  DLOG(INFO) << "R,G,B,A: " << red_size << "," << green_size
             << "," << blue_size << "," << alpha_size << " bits";
  DLOG(INFO) << "Depth: " << depth_size << " bits, Stencil:" << stencil_size
             << "bits";

  egl_surface_ = eglCreateWindowSurface(egl_display_, config, window_, NULL);
  if (!egl_surface_) {
    DLOG(ERROR) << "eglCreateWindowSurface failed.";
    return false;
  }

  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_ = eglCreateContext(egl_display_, config,
                                  EGL_NO_CONTEXT, context_attribs);
  if (!egl_context_) {
    DLOG(ERROR) << "eglCreateContext failed.";
    eglDestroySurface(egl_display_, egl_surface_);
    return false;
  }

  if (eglMakeCurrent(egl_display_, egl_surface_,
                     egl_surface_, egl_context_) == EGL_FALSE) {
    eglDestroyContext(egl_display_, egl_context_);
    eglDestroySurface(egl_display_, egl_surface_);
    egl_display_ = NULL;
    egl_surface_ = NULL;
    egl_context_ = NULL;
    return false;
  }

  EGLint width;
  EGLint height;
  eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &width);
  eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &height);
  glViewport(0, 0, width_, height_);

  if (uses_egl_image_) {
    CreateTextureAndProgramEgl();
    return true;
  }

  CreateTextureAndProgramYuv2Rgb();

  // We are getting called on a thread. Release the context so that it can be
  // made current on the main thread.
  // TODO(hclam): Fix this if neccessary. Currently the following call fails
  // for some drivers.
  // eglMakeCurrent(egl_display_, EGL_NO_SURFACE,
  //                EGL_NO_SURFACE, EGL_NO_CONTEXT);
  return true;
}

void GlesVideoRenderer::CreateShader(GLuint program,
                                     GLenum type,
                                     const char* source,
                                     int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
}

void GlesVideoRenderer::LinkProgram(GLuint program) {
  glLinkProgram(program);
  int result = GL_FALSE;
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
}

void GlesVideoRenderer::CreateTextureAndProgramEgl() {
  if (!egl_create_image_khr_)
    egl_create_image_khr_ = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>
                            (eglGetProcAddress("eglCreateImageKHR"));
  if (!egl_destroy_image_khr_)
    egl_destroy_image_khr_ = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>
                             (eglGetProcAddress("eglDestroyImageKHR"));
  // TODO(wjia): get count from decoder.
  for (int i = 0; i < 4; i++) {
    GLuint texture;
    EGLint attrib = EGL_NONE;
    EGLImageKHR egl_image;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    egl_image = egl_create_image_khr_(
        egl_display_,
        egl_context_,
        EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(texture),
        &attrib);

    scoped_refptr<media::VideoFrame> video_frame;
    const base::TimeDelta kZero;
    media::VideoFrame:: CreatePrivateFrame(
        media::VideoFrame::TYPE_EGL_IMAGE,
        media::VideoFrame::RGB565,
        width_, height_, kZero, kZero,
        egl_image,
        &video_frame);
    egl_frames_.push_back(std::make_pair(video_frame, texture));
    GetDecoder()->FillThisBuffer(video_frame);
  }

  GLuint program = glCreateProgram();

  // Create shader for EGL image
  CreateShader(program, GL_VERTEX_SHADER,
               kVertexShader, sizeof(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShaderEgl, sizeof(kFragmentShaderEgl));
  LinkProgram(program);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "tex"), 0);

  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoordsEgl);
}

void GlesVideoRenderer::CreateTextureAndProgramYuv2Rgb() {
  // Create 3 textures, one for each plane, and bind them to different
  // texture units.
  glGenTextures(media::VideoFrame::kNumYUVPlanes, textures_);

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
  CreateShader(program, GL_VERTEX_SHADER,
               kVertexShader, sizeof(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShader, sizeof(kFragmentShader));
  LinkProgram(program);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(program, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(program, "v_tex"), 2);
  // WAR for some vendor's compiler issues for constant literal.
  glUniform1f(glGetUniformLocation(program, "half"), 0.5);
  int yuv2rgb_location = glGetUniformLocation(program, "yuv2rgb");
  glUniformMatrix3fv(yuv2rgb_location, 1, GL_FALSE, kYUV2RGB);

  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoords);
}
