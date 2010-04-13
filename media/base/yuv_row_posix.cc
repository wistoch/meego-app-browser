// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_row.h"

#ifdef _DEBUG
#include "base/logging.h"
#else
#define DCHECK(a)
#endif

extern "C" {
#if USE_MMX

#define RGBY(i) { \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  0 \
}

#define RGBU(i) { \
  static_cast<int16>(2.018 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(-0.391 * 64 * (i - 128) + 0.5), \
  0, \
  static_cast<int16>(256 * 64 - 1) \
}

#define RGBV(i) { \
  0, \
  static_cast<int16>(-0.813 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(1.596 * 64 * (i - 128) + 0.5), \
  0 \
}

#define MMX_ALIGNED(var) var __attribute__((aligned(16)))


MMX_ALIGNED(int16 kCoefficientsRgbY[768][4]) = {
  RGBY(0x00), RGBY(0x01), RGBY(0x02), RGBY(0x03),
  RGBY(0x04), RGBY(0x05), RGBY(0x06), RGBY(0x07),
  RGBY(0x08), RGBY(0x09), RGBY(0x0A), RGBY(0x0B),
  RGBY(0x0C), RGBY(0x0D), RGBY(0x0E), RGBY(0x0F),
  RGBY(0x10), RGBY(0x11), RGBY(0x12), RGBY(0x13),
  RGBY(0x14), RGBY(0x15), RGBY(0x16), RGBY(0x17),
  RGBY(0x18), RGBY(0x19), RGBY(0x1A), RGBY(0x1B),
  RGBY(0x1C), RGBY(0x1D), RGBY(0x1E), RGBY(0x1F),
  RGBY(0x20), RGBY(0x21), RGBY(0x22), RGBY(0x23),
  RGBY(0x24), RGBY(0x25), RGBY(0x26), RGBY(0x27),
  RGBY(0x28), RGBY(0x29), RGBY(0x2A), RGBY(0x2B),
  RGBY(0x2C), RGBY(0x2D), RGBY(0x2E), RGBY(0x2F),
  RGBY(0x30), RGBY(0x31), RGBY(0x32), RGBY(0x33),
  RGBY(0x34), RGBY(0x35), RGBY(0x36), RGBY(0x37),
  RGBY(0x38), RGBY(0x39), RGBY(0x3A), RGBY(0x3B),
  RGBY(0x3C), RGBY(0x3D), RGBY(0x3E), RGBY(0x3F),
  RGBY(0x40), RGBY(0x41), RGBY(0x42), RGBY(0x43),
  RGBY(0x44), RGBY(0x45), RGBY(0x46), RGBY(0x47),
  RGBY(0x48), RGBY(0x49), RGBY(0x4A), RGBY(0x4B),
  RGBY(0x4C), RGBY(0x4D), RGBY(0x4E), RGBY(0x4F),
  RGBY(0x50), RGBY(0x51), RGBY(0x52), RGBY(0x53),
  RGBY(0x54), RGBY(0x55), RGBY(0x56), RGBY(0x57),
  RGBY(0x58), RGBY(0x59), RGBY(0x5A), RGBY(0x5B),
  RGBY(0x5C), RGBY(0x5D), RGBY(0x5E), RGBY(0x5F),
  RGBY(0x60), RGBY(0x61), RGBY(0x62), RGBY(0x63),
  RGBY(0x64), RGBY(0x65), RGBY(0x66), RGBY(0x67),
  RGBY(0x68), RGBY(0x69), RGBY(0x6A), RGBY(0x6B),
  RGBY(0x6C), RGBY(0x6D), RGBY(0x6E), RGBY(0x6F),
  RGBY(0x70), RGBY(0x71), RGBY(0x72), RGBY(0x73),
  RGBY(0x74), RGBY(0x75), RGBY(0x76), RGBY(0x77),
  RGBY(0x78), RGBY(0x79), RGBY(0x7A), RGBY(0x7B),
  RGBY(0x7C), RGBY(0x7D), RGBY(0x7E), RGBY(0x7F),
  RGBY(0x80), RGBY(0x81), RGBY(0x82), RGBY(0x83),
  RGBY(0x84), RGBY(0x85), RGBY(0x86), RGBY(0x87),
  RGBY(0x88), RGBY(0x89), RGBY(0x8A), RGBY(0x8B),
  RGBY(0x8C), RGBY(0x8D), RGBY(0x8E), RGBY(0x8F),
  RGBY(0x90), RGBY(0x91), RGBY(0x92), RGBY(0x93),
  RGBY(0x94), RGBY(0x95), RGBY(0x96), RGBY(0x97),
  RGBY(0x98), RGBY(0x99), RGBY(0x9A), RGBY(0x9B),
  RGBY(0x9C), RGBY(0x9D), RGBY(0x9E), RGBY(0x9F),
  RGBY(0xA0), RGBY(0xA1), RGBY(0xA2), RGBY(0xA3),
  RGBY(0xA4), RGBY(0xA5), RGBY(0xA6), RGBY(0xA7),
  RGBY(0xA8), RGBY(0xA9), RGBY(0xAA), RGBY(0xAB),
  RGBY(0xAC), RGBY(0xAD), RGBY(0xAE), RGBY(0xAF),
  RGBY(0xB0), RGBY(0xB1), RGBY(0xB2), RGBY(0xB3),
  RGBY(0xB4), RGBY(0xB5), RGBY(0xB6), RGBY(0xB7),
  RGBY(0xB8), RGBY(0xB9), RGBY(0xBA), RGBY(0xBB),
  RGBY(0xBC), RGBY(0xBD), RGBY(0xBE), RGBY(0xBF),
  RGBY(0xC0), RGBY(0xC1), RGBY(0xC2), RGBY(0xC3),
  RGBY(0xC4), RGBY(0xC5), RGBY(0xC6), RGBY(0xC7),
  RGBY(0xC8), RGBY(0xC9), RGBY(0xCA), RGBY(0xCB),
  RGBY(0xCC), RGBY(0xCD), RGBY(0xCE), RGBY(0xCF),
  RGBY(0xD0), RGBY(0xD1), RGBY(0xD2), RGBY(0xD3),
  RGBY(0xD4), RGBY(0xD5), RGBY(0xD6), RGBY(0xD7),
  RGBY(0xD8), RGBY(0xD9), RGBY(0xDA), RGBY(0xDB),
  RGBY(0xDC), RGBY(0xDD), RGBY(0xDE), RGBY(0xDF),
  RGBY(0xE0), RGBY(0xE1), RGBY(0xE2), RGBY(0xE3),
  RGBY(0xE4), RGBY(0xE5), RGBY(0xE6), RGBY(0xE7),
  RGBY(0xE8), RGBY(0xE9), RGBY(0xEA), RGBY(0xEB),
  RGBY(0xEC), RGBY(0xED), RGBY(0xEE), RGBY(0xEF),
  RGBY(0xF0), RGBY(0xF1), RGBY(0xF2), RGBY(0xF3),
  RGBY(0xF4), RGBY(0xF5), RGBY(0xF6), RGBY(0xF7),
  RGBY(0xF8), RGBY(0xF9), RGBY(0xFA), RGBY(0xFB),
  RGBY(0xFC), RGBY(0xFD), RGBY(0xFE), RGBY(0xFF),

  // Chroma U table.
  RGBU(0x00), RGBU(0x01), RGBU(0x02), RGBU(0x03),
  RGBU(0x04), RGBU(0x05), RGBU(0x06), RGBU(0x07),
  RGBU(0x08), RGBU(0x09), RGBU(0x0A), RGBU(0x0B),
  RGBU(0x0C), RGBU(0x0D), RGBU(0x0E), RGBU(0x0F),
  RGBU(0x10), RGBU(0x11), RGBU(0x12), RGBU(0x13),
  RGBU(0x14), RGBU(0x15), RGBU(0x16), RGBU(0x17),
  RGBU(0x18), RGBU(0x19), RGBU(0x1A), RGBU(0x1B),
  RGBU(0x1C), RGBU(0x1D), RGBU(0x1E), RGBU(0x1F),
  RGBU(0x20), RGBU(0x21), RGBU(0x22), RGBU(0x23),
  RGBU(0x24), RGBU(0x25), RGBU(0x26), RGBU(0x27),
  RGBU(0x28), RGBU(0x29), RGBU(0x2A), RGBU(0x2B),
  RGBU(0x2C), RGBU(0x2D), RGBU(0x2E), RGBU(0x2F),
  RGBU(0x30), RGBU(0x31), RGBU(0x32), RGBU(0x33),
  RGBU(0x34), RGBU(0x35), RGBU(0x36), RGBU(0x37),
  RGBU(0x38), RGBU(0x39), RGBU(0x3A), RGBU(0x3B),
  RGBU(0x3C), RGBU(0x3D), RGBU(0x3E), RGBU(0x3F),
  RGBU(0x40), RGBU(0x41), RGBU(0x42), RGBU(0x43),
  RGBU(0x44), RGBU(0x45), RGBU(0x46), RGBU(0x47),
  RGBU(0x48), RGBU(0x49), RGBU(0x4A), RGBU(0x4B),
  RGBU(0x4C), RGBU(0x4D), RGBU(0x4E), RGBU(0x4F),
  RGBU(0x50), RGBU(0x51), RGBU(0x52), RGBU(0x53),
  RGBU(0x54), RGBU(0x55), RGBU(0x56), RGBU(0x57),
  RGBU(0x58), RGBU(0x59), RGBU(0x5A), RGBU(0x5B),
  RGBU(0x5C), RGBU(0x5D), RGBU(0x5E), RGBU(0x5F),
  RGBU(0x60), RGBU(0x61), RGBU(0x62), RGBU(0x63),
  RGBU(0x64), RGBU(0x65), RGBU(0x66), RGBU(0x67),
  RGBU(0x68), RGBU(0x69), RGBU(0x6A), RGBU(0x6B),
  RGBU(0x6C), RGBU(0x6D), RGBU(0x6E), RGBU(0x6F),
  RGBU(0x70), RGBU(0x71), RGBU(0x72), RGBU(0x73),
  RGBU(0x74), RGBU(0x75), RGBU(0x76), RGBU(0x77),
  RGBU(0x78), RGBU(0x79), RGBU(0x7A), RGBU(0x7B),
  RGBU(0x7C), RGBU(0x7D), RGBU(0x7E), RGBU(0x7F),
  RGBU(0x80), RGBU(0x81), RGBU(0x82), RGBU(0x83),
  RGBU(0x84), RGBU(0x85), RGBU(0x86), RGBU(0x87),
  RGBU(0x88), RGBU(0x89), RGBU(0x8A), RGBU(0x8B),
  RGBU(0x8C), RGBU(0x8D), RGBU(0x8E), RGBU(0x8F),
  RGBU(0x90), RGBU(0x91), RGBU(0x92), RGBU(0x93),
  RGBU(0x94), RGBU(0x95), RGBU(0x96), RGBU(0x97),
  RGBU(0x98), RGBU(0x99), RGBU(0x9A), RGBU(0x9B),
  RGBU(0x9C), RGBU(0x9D), RGBU(0x9E), RGBU(0x9F),
  RGBU(0xA0), RGBU(0xA1), RGBU(0xA2), RGBU(0xA3),
  RGBU(0xA4), RGBU(0xA5), RGBU(0xA6), RGBU(0xA7),
  RGBU(0xA8), RGBU(0xA9), RGBU(0xAA), RGBU(0xAB),
  RGBU(0xAC), RGBU(0xAD), RGBU(0xAE), RGBU(0xAF),
  RGBU(0xB0), RGBU(0xB1), RGBU(0xB2), RGBU(0xB3),
  RGBU(0xB4), RGBU(0xB5), RGBU(0xB6), RGBU(0xB7),
  RGBU(0xB8), RGBU(0xB9), RGBU(0xBA), RGBU(0xBB),
  RGBU(0xBC), RGBU(0xBD), RGBU(0xBE), RGBU(0xBF),
  RGBU(0xC0), RGBU(0xC1), RGBU(0xC2), RGBU(0xC3),
  RGBU(0xC4), RGBU(0xC5), RGBU(0xC6), RGBU(0xC7),
  RGBU(0xC8), RGBU(0xC9), RGBU(0xCA), RGBU(0xCB),
  RGBU(0xCC), RGBU(0xCD), RGBU(0xCE), RGBU(0xCF),
  RGBU(0xD0), RGBU(0xD1), RGBU(0xD2), RGBU(0xD3),
  RGBU(0xD4), RGBU(0xD5), RGBU(0xD6), RGBU(0xD7),
  RGBU(0xD8), RGBU(0xD9), RGBU(0xDA), RGBU(0xDB),
  RGBU(0xDC), RGBU(0xDD), RGBU(0xDE), RGBU(0xDF),
  RGBU(0xE0), RGBU(0xE1), RGBU(0xE2), RGBU(0xE3),
  RGBU(0xE4), RGBU(0xE5), RGBU(0xE6), RGBU(0xE7),
  RGBU(0xE8), RGBU(0xE9), RGBU(0xEA), RGBU(0xEB),
  RGBU(0xEC), RGBU(0xED), RGBU(0xEE), RGBU(0xEF),
  RGBU(0xF0), RGBU(0xF1), RGBU(0xF2), RGBU(0xF3),
  RGBU(0xF4), RGBU(0xF5), RGBU(0xF6), RGBU(0xF7),
  RGBU(0xF8), RGBU(0xF9), RGBU(0xFA), RGBU(0xFB),
  RGBU(0xFC), RGBU(0xFD), RGBU(0xFE), RGBU(0xFF),

  // Chroma V table.
  RGBV(0x00), RGBV(0x01), RGBV(0x02), RGBV(0x03),
  RGBV(0x04), RGBV(0x05), RGBV(0x06), RGBV(0x07),
  RGBV(0x08), RGBV(0x09), RGBV(0x0A), RGBV(0x0B),
  RGBV(0x0C), RGBV(0x0D), RGBV(0x0E), RGBV(0x0F),
  RGBV(0x10), RGBV(0x11), RGBV(0x12), RGBV(0x13),
  RGBV(0x14), RGBV(0x15), RGBV(0x16), RGBV(0x17),
  RGBV(0x18), RGBV(0x19), RGBV(0x1A), RGBV(0x1B),
  RGBV(0x1C), RGBV(0x1D), RGBV(0x1E), RGBV(0x1F),
  RGBV(0x20), RGBV(0x21), RGBV(0x22), RGBV(0x23),
  RGBV(0x24), RGBV(0x25), RGBV(0x26), RGBV(0x27),
  RGBV(0x28), RGBV(0x29), RGBV(0x2A), RGBV(0x2B),
  RGBV(0x2C), RGBV(0x2D), RGBV(0x2E), RGBV(0x2F),
  RGBV(0x30), RGBV(0x31), RGBV(0x32), RGBV(0x33),
  RGBV(0x34), RGBV(0x35), RGBV(0x36), RGBV(0x37),
  RGBV(0x38), RGBV(0x39), RGBV(0x3A), RGBV(0x3B),
  RGBV(0x3C), RGBV(0x3D), RGBV(0x3E), RGBV(0x3F),
  RGBV(0x40), RGBV(0x41), RGBV(0x42), RGBV(0x43),
  RGBV(0x44), RGBV(0x45), RGBV(0x46), RGBV(0x47),
  RGBV(0x48), RGBV(0x49), RGBV(0x4A), RGBV(0x4B),
  RGBV(0x4C), RGBV(0x4D), RGBV(0x4E), RGBV(0x4F),
  RGBV(0x50), RGBV(0x51), RGBV(0x52), RGBV(0x53),
  RGBV(0x54), RGBV(0x55), RGBV(0x56), RGBV(0x57),
  RGBV(0x58), RGBV(0x59), RGBV(0x5A), RGBV(0x5B),
  RGBV(0x5C), RGBV(0x5D), RGBV(0x5E), RGBV(0x5F),
  RGBV(0x60), RGBV(0x61), RGBV(0x62), RGBV(0x63),
  RGBV(0x64), RGBV(0x65), RGBV(0x66), RGBV(0x67),
  RGBV(0x68), RGBV(0x69), RGBV(0x6A), RGBV(0x6B),
  RGBV(0x6C), RGBV(0x6D), RGBV(0x6E), RGBV(0x6F),
  RGBV(0x70), RGBV(0x71), RGBV(0x72), RGBV(0x73),
  RGBV(0x74), RGBV(0x75), RGBV(0x76), RGBV(0x77),
  RGBV(0x78), RGBV(0x79), RGBV(0x7A), RGBV(0x7B),
  RGBV(0x7C), RGBV(0x7D), RGBV(0x7E), RGBV(0x7F),
  RGBV(0x80), RGBV(0x81), RGBV(0x82), RGBV(0x83),
  RGBV(0x84), RGBV(0x85), RGBV(0x86), RGBV(0x87),
  RGBV(0x88), RGBV(0x89), RGBV(0x8A), RGBV(0x8B),
  RGBV(0x8C), RGBV(0x8D), RGBV(0x8E), RGBV(0x8F),
  RGBV(0x90), RGBV(0x91), RGBV(0x92), RGBV(0x93),
  RGBV(0x94), RGBV(0x95), RGBV(0x96), RGBV(0x97),
  RGBV(0x98), RGBV(0x99), RGBV(0x9A), RGBV(0x9B),
  RGBV(0x9C), RGBV(0x9D), RGBV(0x9E), RGBV(0x9F),
  RGBV(0xA0), RGBV(0xA1), RGBV(0xA2), RGBV(0xA3),
  RGBV(0xA4), RGBV(0xA5), RGBV(0xA6), RGBV(0xA7),
  RGBV(0xA8), RGBV(0xA9), RGBV(0xAA), RGBV(0xAB),
  RGBV(0xAC), RGBV(0xAD), RGBV(0xAE), RGBV(0xAF),
  RGBV(0xB0), RGBV(0xB1), RGBV(0xB2), RGBV(0xB3),
  RGBV(0xB4), RGBV(0xB5), RGBV(0xB6), RGBV(0xB7),
  RGBV(0xB8), RGBV(0xB9), RGBV(0xBA), RGBV(0xBB),
  RGBV(0xBC), RGBV(0xBD), RGBV(0xBE), RGBV(0xBF),
  RGBV(0xC0), RGBV(0xC1), RGBV(0xC2), RGBV(0xC3),
  RGBV(0xC4), RGBV(0xC5), RGBV(0xC6), RGBV(0xC7),
  RGBV(0xC8), RGBV(0xC9), RGBV(0xCA), RGBV(0xCB),
  RGBV(0xCC), RGBV(0xCD), RGBV(0xCE), RGBV(0xCF),
  RGBV(0xD0), RGBV(0xD1), RGBV(0xD2), RGBV(0xD3),
  RGBV(0xD4), RGBV(0xD5), RGBV(0xD6), RGBV(0xD7),
  RGBV(0xD8), RGBV(0xD9), RGBV(0xDA), RGBV(0xDB),
  RGBV(0xDC), RGBV(0xDD), RGBV(0xDE), RGBV(0xDF),
  RGBV(0xE0), RGBV(0xE1), RGBV(0xE2), RGBV(0xE3),
  RGBV(0xE4), RGBV(0xE5), RGBV(0xE6), RGBV(0xE7),
  RGBV(0xE8), RGBV(0xE9), RGBV(0xEA), RGBV(0xEB),
  RGBV(0xEC), RGBV(0xED), RGBV(0xEE), RGBV(0xEF),
  RGBV(0xF0), RGBV(0xF1), RGBV(0xF2), RGBV(0xF3),
  RGBV(0xF4), RGBV(0xF5), RGBV(0xF6), RGBV(0xF7),
  RGBV(0xF8), RGBV(0xF9), RGBV(0xFA), RGBV(0xFB),
  RGBV(0xFC), RGBV(0xFD), RGBV(0xFE), RGBV(0xFF),
};

#undef RGBY
#undef RGBU
#undef RGBV
#undef MMX_ALIGNED

#if defined(ARCH_CPU_X86_64)

// AMD64 ABI uses register paremters.
void FastConvertYUVToRGB32Row(const uint8* y_buf,  // rdi
                              const uint8* u_buf,  // rsi
                              const uint8* v_buf,  // rdx
                              uint8* rgb_buf,      // rcx
                              int width) {         // r8
  asm(
  "jmp    convertend\n"
"convertloop:"
  "movzb  (%1),%%r10\n"
  "add    $0x1,%1\n"
  "movzb  (%2),%%r11\n"
  "add    $0x1,%2\n"
  "movq   2048(%5,%%r10,8),%%xmm0\n"
  "movzb  (%0),%%r10\n"
  "movq   4096(%5,%%r11,8),%%xmm1\n"
  "movzb  0x1(%0),%%r11\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movq   (%5,%%r10,8),%%xmm2\n"
  "add    $0x2,%0\n"
  "movq   (%5,%%r11,8),%%xmm3\n"
  "paddsw %%xmm0,%%xmm2\n"
  "paddsw %%xmm0,%%xmm3\n"
  "shufps $0x44,%%xmm3,%%xmm2\n"
  "psraw  $0x6,%%xmm2\n"
  "packuswb %%xmm2,%%xmm2\n"
  "movq   %%xmm2,0x0(%3)\n"
  "add    $0x8,%3\n"
"convertend:"
  "sub    $0x2,%4\n"
  "jns    convertloop\n"

"convertnext:"
  "add    $0x1,%4\n"
  "js     convertdone\n"

  "movzb  (%1),%%r10\n"
  "movq   2048(%5,%%r10,8),%%xmm0\n"
  "movzb  (%2),%%r10\n"
  "movq   4096(%5,%%r10,8),%%xmm1\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movzb  (%0),%%r10\n"
  "movq   (%5,%%r10,8),%%xmm1\n"
  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"
"convertdone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY)  // %5
  : "memory", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3"
);
}

void ScaleYUVToRGB32Row(const uint8* y_buf,  // rdi
                        const uint8* u_buf,  // rsi
                        const uint8* v_buf,  // rdx
                        uint8* rgb_buf,      // rcx
                        int width,           // r8
                        int scaled_dx) {     // r9
  asm(
  "xor    %%r11,%%r11\n"
  "sub    $0x2,%4\n"
  "js     scalenext\n"

"scaleloop:"
  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"
  "movzb  (%1,%%r10,1),%%rax\n"
  "movq   2048(%5,%%rax,8),%%xmm0\n"
  "movzb  (%2,%%r10,1),%%rax\n"
  "movq   4096(%5,%%rax,8),%%xmm1\n"
  "lea    (%%r11,%6),%%r10\n"
  "sar    $0x10,%%r11\n"
  "movzb  (%0,%%r11,1),%%rax\n"
  "paddsw %%xmm1,%%xmm0\n"
  "movq   (%5,%%rax,8),%%xmm1\n"
  "lea    (%%r10,%6),%%r11\n"
  "sar    $0x10,%%r10\n"
  "movzb  (%0,%%r10,1),%%rax\n"
  "movq   (%5,%%rax,8),%%xmm2\n"
  "paddsw %%xmm0,%%xmm1\n"
  "paddsw %%xmm0,%%xmm2\n"
  "shufps $0x44,%%xmm2,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movq   %%xmm1,0x0(%3)\n"
  "add    $0x8,%3\n"
  "sub    $0x2,%4\n"
  "jns    scaleloop\n"

"scalenext:"
  "add    $0x1,%4\n"
  "js     scaledone\n"

  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"
  "movzb  (%1,%%r10,1),%%rax\n"
  "movq   2048(%5,%%rax,8),%%xmm0\n"
  "movzb  (%2,%%r10,1),%%rax\n"
  "movq   4096(%5,%%rax,8),%%xmm1\n"
  "paddsw %%xmm1,%%xmm0\n"
  "sar    $0x10,%%r11\n"
  "movzb  (%0,%%r11,1),%%rax\n"
  "movq   (%5,%%rax,8),%%xmm1\n"
  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"

"scaledone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY),  // %5
    "r"(static_cast<long>(scaled_dx))  // %6
  : "memory", "r10", "r11", "rax", "xmm0", "xmm1", "xmm2"
);
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int scaled_dx) {
  asm(
  "xor    %%r11,%%r11\n"
  "sub    $0x2,%4\n"
  "js     .lscalenext\n"

".lscaleloop:"
  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"

  "movzb  (%1, %%r10, 1), %%r13 \n"
  "movzb  1(%1, %%r10, 1), %%r14 \n"
  "mov    %%r11, %%rax \n"
  "and    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $17, %%r13 \n"
  "movq   2048(%5,%%r13,8), %%xmm0\n"

  "movzb  (%2, %%r10, 1), %%r13 \n"
  "movzb  1(%2, %%r10, 1), %%r14 \n"
  "mov    %%r11, %%rax \n"
  "and    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0x1fffe, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $17, %%r13 \n"
  "movq   4096(%5,%%r13,8), %%xmm1\n"

  "mov    %%r11, %%rax \n"
  "lea    (%%r11,%6),%%r10\n"
  "sar    $0x10,%%r11\n"
  "paddsw %%xmm1,%%xmm0\n"

  "movzb  (%0, %%r11, 1), %%r13 \n"
  "movzb  1(%0, %%r11, 1), %%r14 \n"
  "and    $0xffff, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0xffff, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $16, %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm1\n"

  "mov    %%r10, %%rax \n"
  "lea    (%%r10,%6),%%r11\n"
  "sar    $0x10,%%r10\n"

  "movzb  (%0,%%r10,1), %%r13 \n"
  "movzb  1(%0,%%r10,1), %%r14 \n"
  "and    $0xffff, %%rax \n"
  "imul   %%rax, %%r14 \n"
  "xor    $0xffff, %%rax \n"
  "imul   %%rax, %%r13 \n"
  "add    %%r14, %%r13 \n"
  "shr    $16, %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm2\n"

  "paddsw %%xmm0,%%xmm1\n"
  "paddsw %%xmm0,%%xmm2\n"
  "shufps $0x44,%%xmm2,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movq   %%xmm1,0x0(%3)\n"
  "add    $0x8,%3\n"
  "sub    $0x2,%4\n"
  "jns    .lscaleloop\n"

".lscalenext:"
  "add    $0x1,%4\n"
  "js     .lscaledone\n"

  "mov    %%r11,%%r10\n"
  "sar    $0x11,%%r10\n"

  "movzb  (%1,%%r10,1), %%r13 \n"
  "movq   2048(%5,%%r13,8),%%xmm0\n"

  "movzb  (%2,%%r10,1), %%r13 \n"
  "movq   4096(%5,%%r13,8),%%xmm1\n"

  "paddsw %%xmm1,%%xmm0\n"
  "sar    $0x10,%%r11\n"

  "movzb  (%0,%%r11,1), %%r13 \n"
  "movq   (%5,%%r13,8),%%xmm1\n"

  "paddsw %%xmm0,%%xmm1\n"
  "psraw  $0x6,%%xmm1\n"
  "packuswb %%xmm1,%%xmm1\n"
  "movd   %%xmm1,0x0(%3)\n"

".lscaledone:"
  :
  : "r"(y_buf),  // %0
    "r"(u_buf),  // %1
    "r"(v_buf),  // %2
    "r"(rgb_buf),  // %3
    "r"(width),  // %4
    "r" (kCoefficientsRgbY),  // %5
    "r"(static_cast<long>(scaled_dx))  // %6
  : "memory", "r10", "r11", "r13", "r14", "rax", "xmm0", "xmm1", "xmm2"
);
}

#else // !AMD64

// PIC version is slower because less registers are available, so
// non-PIC is used on platforms where it is possible.

#if !defined(__PIC__)

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width);

  asm(
  ".global FastConvertYUVToRGB32Row\n"
"FastConvertYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "jmp    convertend\n"

"convertloop:"
  "movzbl (%edi),%eax\n"
  "add    $0x1,%edi\n"
  "movzbl (%esi),%ebx\n"
  "add    $0x1,%esi\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%ebx,8),%mm0\n"
  "movzbl 0x1(%edx),%ebx\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "add    $0x2,%edx\n"
  "movq   kCoefficientsRgbY(,%ebx,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"convertend:"
  "sub    $0x2,%ecx\n"
  "jns    convertloop\n"

  "and    $0x1,%ecx\n"
  "je     convertdone\n"

  "movzbl (%edi),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "movzbl (%esi),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"
"convertdone:"
  "popa\n"
  "ret\n"
);


void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int scaled_dx);

  asm(
  ".global ScaleYUVToRGB32Row\n"
"ScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "xor    %ebx,%ebx\n"
  "jmp    scaleend\n"

"scaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"scaleend:"
  "sub    $0x2,%ecx\n"
  "jns    scaleloop\n"

  "and    $0x1,%ecx\n"
  "je     scaledone\n"

  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY+2048(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw kCoefficientsRgbY+4096(,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   kCoefficientsRgbY(,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"

"scaledone:"
  "popa\n"
  "ret\n"
);

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int scaled_dx);

  asm(
  ".global LinearScaleYUVToRGB32Row\n"
"LinearScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x30(%esp),%ebp\n"
  "xor    %ebx,%ebx\n"

  // width = width * scaled_dx + ebx
  "mov    0x34(%esp), %ecx\n"
  "imull  0x38(%esp), %ecx\n"
  "addl   %ebx, %ecx\n"
  "mov    %ecx, 0x34(%esp)\n"

  "jmp    .lscaleend\n"

".lscaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%edi,%eax,1),%ecx\n"
  "movzbl 1(%edi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "movq   kCoefficientsRgbY+2048(,%ecx,8),%mm0\n"

  "mov    0x2c(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "paddsw kCoefficientsRgbY+4096(,%ecx,8),%mm0\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   kCoefficientsRgbY(,%ecx,8),%mm1\n"

  "cmp    0x34(%esp), %ebx\n"
  "jge    .lscalelastpixel\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   kCoefficientsRgbY(,%ecx,8),%mm2\n"

  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"

".lscaleend:"
  "cmp    0x34(%esp), %ebx\n"
  "jl     .lscaleloop\n"
  "popa\n"
  "ret\n"

".lscalelastpixel:"
  "paddsw %mm0, %mm1\n"
  "psraw $6, %mm1\n"
  "packuswb %mm1, %mm1\n"
  "movd %mm1, (%ebp)\n"
  "popa\n"
  "ret\n"
);

#else // __PIC__

extern void PICConvertYUVToRGB32Row(const uint8* y_buf,
                                    const uint8* u_buf,
                                    const uint8* v_buf,
                                    uint8* rgb_buf,
                                    int width,
                                    int16 *kCoefficientsRgbY);
  __asm__(
"_PICConvertYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x38(%esp),%ecx\n"

  "jmp    .Lconvertend\n"

".Lconvertloop:"
  "movzbl (%edi),%eax\n"
  "add    $0x1,%edi\n"
  "movzbl (%esi),%ebx\n"
  "add    $0x1,%esi\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "paddsw 4096(%ecx,%ebx,8),%mm0\n"
  "movzbl 0x1(%edx),%ebx\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "add    $0x2,%edx\n"
  "movq   0(%ecx,%ebx,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
".Lconvertend:"
  "sub    $0x2,0x34(%esp)\n"
  "jns    .Lconvertloop\n"

  "and    $0x1,0x34(%esp)\n"
  "je     .Lconvertdone\n"

  "movzbl (%edi),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "movzbl (%esi),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "movzbl (%edx),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"
".Lconvertdone:\n"
  "popa\n"
  "ret\n"
);

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  PICConvertYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width,
                          &kCoefficientsRgbY[0][0]);
}

extern void PICScaleYUVToRGB32Row(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width,
                               int scaled_dx,
                               int16 *kCoefficientsRgbY);

  __asm__(
"_PICScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x28(%esp),%edi\n"
  "mov    0x2c(%esp),%esi\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x3c(%esp),%ecx\n"
  "xor    %ebx,%ebx\n"
  "jmp    Lscaleend\n"

"Lscaleloop:"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm2\n"
  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"
"Lscaleend:"
  "sub    $0x2,0x34(%esp)\n"
  "jns    Lscaleloop\n"

  "and    $0x1,0x34(%esp)\n"
  "je     Lscaledone\n"

  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%edi,%eax,1),%eax\n"
  "movq   2048(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"
  "movzbl (%esi,%eax,1),%eax\n"
  "paddsw 4096(%ecx,%eax,8),%mm0\n"
  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%eax\n"
  "movq   0(%ecx,%eax,8),%mm1\n"
  "paddsw %mm0,%mm1\n"
  "psraw  $0x6,%mm1\n"
  "packuswb %mm1,%mm1\n"
  "movd   %mm1,0x0(%ebp)\n"

"Lscaledone:"
  "popa\n"
  "ret\n"
);


void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int scaled_dx) {
  PICScaleYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width, scaled_dx,
                        &kCoefficientsRgbY[0][0]);
}

void PICLinearScaleYUVToRGB32Row(const uint8* y_buf,
                                 const uint8* u_buf,
                                 const uint8* v_buf,
                                 uint8* rgb_buf,
                                 int width,
                                 int scaled_dx,
                                 int16 *kCoefficientsRgbY);

  asm(
"_PICLinearScaleYUVToRGB32Row:\n"
  "pusha\n"
  "mov    0x24(%esp),%edx\n"
  "mov    0x30(%esp),%ebp\n"
  "mov    0x34(%esp),%ecx\n"
  "mov    0x3c(%esp),%edi\n"
  "xor    %ebx,%ebx\n"

  // width = width * scaled_dx + ebx
  "mov    0x34(%esp), %ecx\n"
  "imull  0x38(%esp), %ecx\n"
  "addl   %ebx, %ecx\n"
  "mov    %ecx, 0x34(%esp)\n"

  "jmp    .lscaleend\n"

".lscaleloop:"
  "mov    0x28(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "movq   2048(%edi,%ecx,8),%mm0\n"

  "mov    0x2c(%esp),%esi\n"
  "mov    %ebx,%eax\n"
  "sar    $0x11,%eax\n"

  "movzbl (%esi,%eax,1),%ecx\n"
  "movzbl 1(%esi,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "andl   $0x1fffe, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0x1fffe, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $17, %ecx \n"
  "paddsw 4096(%edi,%ecx,8),%mm0\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   (%edi,%ecx,8),%mm1\n"

  "cmp    0x34(%esp), %ebx\n"
  "jge    .lscalelastpixel\n"

  "mov    %ebx,%eax\n"
  "sar    $0x10,%eax\n"
  "movzbl (%edx,%eax,1),%ecx\n"
  "movzbl 1(%edx,%eax,1),%esi\n"
  "mov    %ebx,%eax\n"
  "add    0x38(%esp),%ebx\n"
  "andl   $0xffff, %eax \n"
  "imul   %eax, %esi \n"
  "xorl   $0xffff, %eax \n"
  "imul   %eax, %ecx \n"
  "addl   %esi, %ecx \n"
  "shrl   $16, %ecx \n"
  "movq   (%edi,%ecx,8),%mm2\n"

  "paddsw %mm0,%mm1\n"
  "paddsw %mm0,%mm2\n"
  "psraw  $0x6,%mm1\n"
  "psraw  $0x6,%mm2\n"
  "packuswb %mm2,%mm1\n"
  "movntq %mm1,0x0(%ebp)\n"
  "add    $0x8,%ebp\n"

".lscaleend:"
  "cmp    %ebx, 0x34(%esp)\n"
  "jg     .lscaleloop\n"
  "popa\n"
  "ret\n"

".lscalelastpixel:"
  "paddsw %mm0, %mm1\n"
  "psraw $6, %mm1\n"
  "packuswb %mm1, %mm1\n"
  "movd %mm1, (%ebp)\n"
  "popa\n"
  "ret\n"
);

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int scaled_dx) {
  PICLinearScaleYUVToRGB32Row(y_buf, u_buf, v_buf, rgb_buf, width, scaled_dx,
                              &kCoefficientsRgbY[0][0]);
}

#endif // !__PIC__

#endif // !AMD64

#else  // USE_MMX

// Reference version of YUV converter.
static const int kClipTableSize = 256;
static const int kClipOverflow = 288;  // Cb max is 535.

static uint8 kRgbClipTable[kClipOverflow +
                           kClipTableSize +
                           kClipOverflow] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 288 underflow values
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // clipped to 0.
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // Unclipped values.
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
  0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
  0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
  0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
  0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 288 overflow values
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // clipped to 255.
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// Clip an rgb channel value to 0..255 range.
// Source is signed fixed point 8.8.
// Table allows for values to underflow or overflow by 128.
// Therefore source range is -128 to 384.
// Output clips to unsigned 0 to 255.
static inline uint32 clip(int32 value) {
  DCHECK(((value >> 8) + kClipOverflow) >= 0);
  DCHECK(((value >> 8) + kClipOverflow) <
         (kClipOverflow + kClipTableSize + kClipOverflow));
  return static_cast<uint32>(kRgbClipTable[((value) >> 8) + kClipOverflow]);
}

static inline void YuvPixel(uint8 y,
                            uint8 u,
                            uint8 v,
                            uint8* rgb_buf) {
  int32 d = static_cast<int32>(u) - 128;
  int32 e = static_cast<int32>(v) - 128;

  int32 cb =   (516 * d + 128);
  int32 cg = (- 100 * d - 208 * e + 128);
  int32 cr =             (409 * e + 128);

  int32 C298a = ((static_cast<int32>(y) - 16) * 298 + 128);
  *reinterpret_cast<uint32*>(rgb_buf) = (clip(C298a + cb)) |
                                        (clip(C298a + cg) << 8) |
                                        (clip(C298a + cr) << 16) |
                                        (0xff000000);
}

void FastConvertYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width) {
  for (int x = 0; x < width; x += 2) {
    uint8 u = u_buf[x >> 1];
    uint8 v = v_buf[x >> 1];
    uint8 y0 = y_buf[x];
    YuvPixel(y0, u, v, rgb_buf);
    if ((x + 1) < width) {
      uint8 y1 = y_buf[x + 1];
      YuvPixel(y1, u, v, rgb_buf + 4);
    }
    rgb_buf += 8;  // Advance 2 pixels.
  }
}

// 16.16 fixed point is used.  A shift by 16 isolates the integer.
// A shift by 17 is used to further subsample the chrominence channels.
// & 0xffff isolates the fixed point fraction.  >> 2 to get the upper 2 bits,
// for 1/65536 pixel accurate interpolation.
void ScaleYUVToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width,
                        int scaled_dx) {
  int scaled_x = 0;
  for (int x = 0; x < width; ++x) {
    uint8 u = u_buf[scaled_x >> 17];
    uint8 v = v_buf[scaled_x >> 17];
    uint8 y0 = y_buf[scaled_x >> 16];
    YuvPixel(y0, u, v, rgb_buf);
    rgb_buf += 4;
    scaled_x += scaled_dx;
  }
}

void LinearScaleYUVToRGB32Row(const uint8* y_buf,
                              const uint8* u_buf,
                              const uint8* v_buf,
                              uint8* rgb_buf,
                              int width,
                              int dx) {
  for (int x = 0; x < width * dx; x += dx) {
    int y0 = y_buf[x >> 16];
    int y1 = y_buf[(x >> 16) + 1];
    int u0 = u_buf[(x >> 17)];
    int u1 = u_buf[(x >> 17) + 1];
    int v0 = v_buf[(x >> 17)];
    int v1 = v_buf[(x >> 17) + 1];
    int y = ((x & 65535) * y1 + ((x & 65535) ^ 65535) * y0) >> 16;
    int u = ((x & 65535) * u1 + ((x & 65535) ^ 65535) * u0) >> 16;
    int v = ((x & 65535) * v1 + ((x & 65535) ^ 65535) * v0) >> 16;
    YuvPixel(y, u, v, rgb_buf);
    rgb_buf += 4;
  }
}

#endif  // USE_MMX
}  // extern "C"
