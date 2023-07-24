// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelCaptureCapturer.h"

/**
 * The interface that is queried when the Layered or Multi capturers wish to create specific
 * Capturers for a given format.
 */
class PIXELCAPTURE_API IPixelCaptureCapturerSource
{
public:
	/**
	 * Implement this to create an capture process that captures to the FinalFormat at
	 * the given scale. The source format should be known by whatever is fed into the
	 * capture system.
	 * @param FinalFormat The format the capture process should capture.
	 * @param FinalScale The frame scale the capture process should scale to.
	 * @return The FPixelCaptureCapturer that will do the work.
	 */
	virtual TSharedPtr<FPixelCaptureCapturer> CreateCapturer(int32 FinalFormat, float FinalScale) = 0;
};
