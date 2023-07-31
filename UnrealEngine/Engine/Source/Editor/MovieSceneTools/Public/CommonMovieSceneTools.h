// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "FrameNumberNumericInterface.h"
#include "Layout/Geometry.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "SequencerSectionPainter.h"
#include "TimeToPixel.h"

class FSequencerSectionPainter;
struct FFrameNumberInterface;

/**
 * Draw a frame time next to the scrub handle
 *
 * @param InPainter Structure that affords common painting operations
 * @param CurrentTime Current time of the scrub handle
 * @param FrameTime Frame time to draw
 * @param FrameNumberInterface (optional) Interface to control the display format and/or frame rate conversion of the drawn frame time.
 *            If not provided, the frame time will be drawn as a frame number without any subframe.
 */
void DrawFrameTimeHint(FSequencerSectionPainter& InPainter, const FFrameTime& CurrentTime, const FFrameTime& FrameTime, const FFrameNumberInterface* FrameNumberInterface = nullptr);
