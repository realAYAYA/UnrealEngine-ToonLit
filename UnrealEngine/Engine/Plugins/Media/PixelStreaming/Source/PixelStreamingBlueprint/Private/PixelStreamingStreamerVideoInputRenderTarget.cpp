// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStreamerVideoInputRenderTarget.h"
#include "PixelStreamingVideoInputRenderTarget.h"

UPixelStreamingStreamerVideoInputRenderTarget::UPixelStreamingStreamerVideoInputRenderTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<FPixelStreamingVideoInput> UPixelStreamingStreamerVideoInputRenderTarget::GetVideoInput()
{
	if (!VideoInput)
	{
		VideoInput = FPixelStreamingVideoInputRenderTarget::Create(Target);
	}
	return VideoInput;
}
