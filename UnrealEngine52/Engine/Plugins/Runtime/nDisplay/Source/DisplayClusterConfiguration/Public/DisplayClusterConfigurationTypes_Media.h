// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterConfigurationTypes_Base.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MediaOutput.h"

#include "DisplayClusterConfigurationTypes_Media.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/*
 * Media settings for viewports and backbuffer
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMedia
{
	GENERATED_BODY()

public:
	/** Enable/disable media */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	bool bEnable = false;

	/** Media source to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaSource> MediaSource;

	/** Media output to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaOutput> MediaOutput;

#if WITH_EDITORONLY_DATA

	UE_DEPRECATED(5.2, "This property has been deprecated")
	UPROPERTY()
	FString MediaSharingNode_DEPRECATED;

#endif // WITH_EDITORONLY_DATA
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS


/*
 * Input media group
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaInputGroup
{
	GENERATED_BODY()

public:
	/** Cluster nodes that use media source below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Media, meta = (ClusterItemType = ClusterNodes))
	FDisplayClusterConfigurationClusterItemReferenceList ClusterNodes;

	/** Media source to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaSource> MediaSource;
};


/*
 * Output media group
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaOutputGroup
{
	GENERATED_BODY()

public:
	/** Cluster nodes that export media via MediaOutput below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Media, meta = (ClusterItemType = ClusterNodes))
	FDisplayClusterConfigurationClusterItemReferenceList ClusterNodes;

	/** Media output to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaOutput> MediaOutput;
};


/*
 * Media settings for ICVFX cameras
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaICVFX
{
	GENERATED_BODY()

public:
	/** Enable/disable media */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	bool bEnable = false;

	/** Media output mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	TArray<FDisplayClusterConfigurationMediaOutputGroup> MediaOutputGroups;

	/** Media input mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	TArray<FDisplayClusterConfigurationMediaInputGroup> MediaInputGroups;

public:
	/** Returns true if a specific cluster node has media source assigned */
	bool IsMediaInputAssigned(const FString& InNodeId) const;

	/** Returns true if a specific cluster node has media source assigned */
	bool IsMediaOutputAssigned(const FString& InNodeId) const;

	/** Returns media source bound to a specific cluster node */
	UMediaSource* GetMediaSource(const FString& InNodeId) const;

	/** Returns media output bound to a specific cluster node */
	UMediaOutput* GetMediaOutput(const FString& InNodeId) const;

	UE_DEPRECATED(5.2, "This function has beend deprecated.")
	bool IsMediaSharingNode(const FString& InNodeId) const
	{
		return false;
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
