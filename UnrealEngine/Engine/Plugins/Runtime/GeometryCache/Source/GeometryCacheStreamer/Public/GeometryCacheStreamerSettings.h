// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UObject/ObjectMacros.h"

#include "GeometryCacheStreamerSettings.generated.h"

/** Settings for the GeometryCache streamer */
UCLASS(config = Engine, meta = (DisplayName = "Geometry Cache"))
class GEOMETRYCACHESTREAMER_API UGeometryCacheStreamerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	 
	UGeometryCacheStreamerSettings();

	/** The amount of animation (in seconds) to stream ahead of time (per stream) */
	UPROPERTY(config, EditAnywhere, Category = "Geometry Cache Streamer", meta = (DisplayName = "Look-Ahead Buffer (in seconds)", ClampMin = "0.01", ClampMax = "3600.0"))
	float LookAheadBuffer;

	/** The maximum total amount of streamed data allowed in memory (for all streams) */
	UPROPERTY(config, EditAnywhere, Category = "Geometry Cache Streamer", meta = (DisplayName = "Maximum Memory Allowed (in MB)", ClampMin = "1.0", ClampMax = "262144.0"))
	float MaxMemoryAllowed;
};
