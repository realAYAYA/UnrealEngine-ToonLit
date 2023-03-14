// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MediaOutput.h"

#include "DisplayClusterConfigurationTypes_Media.generated.h"


/*
 * Media settings
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMedia
{
	GENERATED_BODY()

public:
	/** Enable/disable in-cluster media sharing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Sharing")
	bool bMediaSharing = false;

	/** When in-cluster media sharing us used, the cluster node specified here will be used as a source (Tx node). The others will be Tx'es. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Sharing")
	FString MediaSharingNode;

	/** Media source to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Input")
	TObjectPtr<UMediaSource> MediaSource = nullptr;

	/** Media output to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Output")
	TObjectPtr<UMediaOutput> MediaOutput = nullptr;

public:
	/** Returns true if media sharing is used */
	bool IsMediaSharingUsed() const
	{
		return bMediaSharing && MediaSource && MediaOutput && !MediaSharingNode.IsEmpty();
	}
};


/*
 * Global media settings
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationGlobalMediaSettings
{
	GENERATED_BODY()

public:
	/** Media latency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media", meta = (ClampMin = "0", ClampMax = "9", UIMin = "0", UIMax = "9"))
	int32 Latency = 0;
};
