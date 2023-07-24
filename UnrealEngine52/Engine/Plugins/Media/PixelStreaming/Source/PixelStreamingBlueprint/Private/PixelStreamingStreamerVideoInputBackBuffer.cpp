// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStreamerVideoInputBackBuffer.h"
#include "PixelStreamingVideoInputBackBuffer.h"

UPixelStreamingStreamerVideoInputBackBuffer::UPixelStreamingStreamerVideoInputBackBuffer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VideoInput = FPixelStreamingVideoInputBackBuffer::Create();
}
