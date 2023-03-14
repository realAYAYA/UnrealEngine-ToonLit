// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingVideoInputRHI.h"
#include "Delegates/IDelegateInstance.h"
#include "UnrealClient.h"


/*
 * Use this if you want to send the UE primary scene viewport as video input - will only work in editor.
 */
class PIXELSTREAMINGEDITOR_API FPixelStreamingVideoInputViewport : public FPixelStreamingVideoInputRHI
{
public:
	static TSharedPtr<FPixelStreamingVideoInputViewport> Create();
	virtual ~FPixelStreamingVideoInputViewport();

private:
	FPixelStreamingVideoInputViewport() = default;

	void OnViewportRendered(FViewport* Viewport);

	FDelegateHandle DelegateHandle;

	FName TargetViewportType = FName(FString(TEXT("SceneViewport")));
};
