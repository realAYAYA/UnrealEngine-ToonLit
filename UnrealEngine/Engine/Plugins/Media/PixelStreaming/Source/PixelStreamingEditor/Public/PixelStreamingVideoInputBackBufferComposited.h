// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInputRHI.h"
#include "Widgets/SWindow.h"
#include "RHI.h"
#include "Delegates/IDelegateInstance.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindowDefinition.h"


/*
 * Use this if you want to send the full UE editor as video input.
 */
class PIXELSTREAMINGEDITOR_API FPixelStreamingVideoInputBackBufferComposited : public FPixelStreamingVideoInputRHI
{
public:
	static TSharedPtr<FPixelStreamingVideoInputBackBufferComposited> Create();
	virtual ~FPixelStreamingVideoInputBackBufferComposited();

private:
	FPixelStreamingVideoInputBackBufferComposited();

	void OnBackBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);

	FDelegateHandle DelegateHandle;

	TArray<TSharedRef<SWindow>> TopLevelWindows;
	TMap<SWindow*, FTextureRHIRef> TopLevelWindowTextures;

	FCriticalSection TopLevelWindowsCriticalSection;
	FTexture2DRHIRef CompositedFrame;
	TSharedPtr<FIntPoint> CompositedFrameSize;
	bool bRecreateTexture = false;
	FIntPoint DefaultSize = FIntPoint(1, 1);
};