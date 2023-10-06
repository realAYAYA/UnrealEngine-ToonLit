// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaOverlaySample.h"
#include "MediaObjectPool.h"


/**
 * Implements a media subtitle sample.
 */
class MEDIAIOCORE_API FMediaIOCoreSubtitleSampleBase
	: public IMediaOverlaySample
	, public IMediaPoolable
{
};
