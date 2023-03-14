// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInputRHI.h"
#include "Widgets/SWindow.h"
#include "RHI.h"
#include "Delegates/IDelegateInstance.h"

/*
 * Use this if you want to send the UE backbuffer as video input.
 */
class PIXELSTREAMING_API FPixelStreamingVideoInputBackBuffer : public FPixelStreamingVideoInputRHI
{
public:
	static TSharedPtr<FPixelStreamingVideoInputBackBuffer> Create();
	virtual ~FPixelStreamingVideoInputBackBuffer();

private:
	FPixelStreamingVideoInputBackBuffer() = default;

	void OnBackBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);

	FDelegateHandle DelegateHandle;
};
