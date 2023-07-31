// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2DDynamic.h"
#include "PixelStreamingVideoInput.h"
#include "PixelStreamingStreamerInput.generated.h"

UCLASS(NotBlueprintType, NotBlueprintable, Category = "PixelStreaming", META = (DisplayName = "Streamer Input"))
class PIXELSTREAMINGBLUEPRINT_API UPixelStreamingStreamerInput : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	TSharedPtr<FPixelStreamingVideoInput> GetVideoInput() { return VideoInput; }

protected:
	TSharedPtr<FPixelStreamingVideoInput> VideoInput;
};
