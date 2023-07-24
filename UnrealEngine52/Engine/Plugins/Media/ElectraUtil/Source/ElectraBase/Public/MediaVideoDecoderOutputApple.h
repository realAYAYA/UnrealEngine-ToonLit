// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Missing defines around platform specific code

#include "MediaVideoDecoderOutput.h"

#include "Templates/RefCounting.h"
#include "Containers/Array.h"
#include "PixelFormat.h"

class FVideoDecoderOutputApple : public FVideoDecoderOutput
{
public:
	virtual uint32 GetStride() const = 0;
	virtual CVImageBufferRef GetImageBuffer() const = 0;
};

