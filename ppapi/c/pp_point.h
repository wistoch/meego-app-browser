/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_POINT_H_
#define PPAPI_C_PP_POINT_H_

/**
 * @file
 * This file defines the API to create a 2 dimensional point.
 * 0,0 is the upper-left starting coordinate.
 */

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 *
 * @addtogroup Structs
 * @{
 */

/**
 * The PP_Point structure defines the x and y coordinates of a point.
 */
struct PP_Point {
  /**
   * This value represents the horizontal coordinate of a point, starting with 0
   * as the left-most coordinate.
   */
  int32_t x;

  /**
   * This value represents the vertical coordinate of a point, starting with 0
   * as the top-most coordinate.
   */
  int32_t y;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Point, 8);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakePoint() creates a PP_Point given the x and y coordinates as int32_t
 * values.
 * @param[in] x An int32_t value representing a horizontal coordinate of a
 * point, starting with 0 as the left-most coordinate.
 * @param[in] y An int32_t value representing a vertical coordinate of a point,
 * starting with 0 as the top-most coordinate.
 * @return A PP_Point structure.
 */
PP_INLINE struct PP_Point PP_MakePoint(int32_t x, int32_t y) {
  struct PP_Point ret;
  ret.x = x;
  ret.y = y;
  return ret;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_POINT_H_ */

