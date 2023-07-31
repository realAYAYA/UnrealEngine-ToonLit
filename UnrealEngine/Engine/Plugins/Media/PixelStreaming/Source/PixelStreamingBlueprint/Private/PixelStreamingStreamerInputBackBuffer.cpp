// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStreamerInputBackBuffer.h"
#include "PixelStreamingVideoInputBackBuffer.h"

UPixelStreamingStreamerInputBackBuffer::UPixelStreamingStreamerInputBackBuffer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VideoInput = FPixelStreamingVideoInputBackBuffer::Create();
}
