// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing browser's settings that apply to the renderer or its
// webview.  These differ from WebPreferences since they apply to Chromium's
// glue layer rather than applying to just WebKit.
//
// Adding new values to this class probably involves updating
// common/render_messages.h, browser/browser.cc, etc.

#ifndef CHROME_COMMON_RENDERER_PREFERENCES_H_
#define CHROME_COMMON_RENDERER_PREFERENCES_H_

enum RendererPreferencesHintingEnum {
  RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT = 0,
  RENDERER_PREFERENCES_HINTING_NONE,
  RENDERER_PREFERENCES_HINTING_SLIGHT,
  RENDERER_PREFERENCES_HINTING_MEDIUM,
  RENDERER_PREFERENCES_HINTING_FULL,
};

enum RendererPreferencesSubpixelRenderingEnum {
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT = 0,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR,
};

struct RendererPreferences {
  // Whether the renderer's current browser context accept drops from the OS
  // that result in navigations away from the current page.
  bool can_accept_load_drops;

  // Whether text should be antialiased.
  // Currently only used by Linux.
  bool should_antialias_text;

  // The level of hinting to use when rendering text.
  // Currently only used by Linux.
  RendererPreferencesHintingEnum hinting;

  // The type of subpixel rendering to use for text.
  // Currently only used by Linux.
  RendererPreferencesSubpixelRenderingEnum subpixel_rendering;

  RendererPreferences()
      : can_accept_load_drops(true),
        should_antialias_text(true),
        hinting(RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT),
        subpixel_rendering(
            RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT) {
  }
};

#endif  // CHROME_COMMON_RENDERER_PREFERENCES_H_
