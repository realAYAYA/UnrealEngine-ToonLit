// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include "MediaVideoDecoderOutput.h"

#include "Templates/RefCounting.h"
#include "Containers/Array.h"
#include "PixelFormat.h"

class FVideoDecoderOutputApple : public FVideoDecoderOutput
{
public:
	virtual const TArray<uint8>& GetBuffer() const = 0;
	virtual uint32 GetStride() const = 0;
	virtual CVImageBufferRef GetImageBuffer() const = 0;
};

#endif
