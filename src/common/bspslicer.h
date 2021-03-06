/*
 * @file
 */

#ifndef BSPSLICER_H
#define BSPSLICER_H

#include "../shared/shared.h"
#include "../shared/typedefs.h"
#include "../common/tracing.h"

void SL_BSPSlice(const TR_TILE_TYPE *model, float thickness, int scale, bool singleContour, bool multipleContour);

#endif
