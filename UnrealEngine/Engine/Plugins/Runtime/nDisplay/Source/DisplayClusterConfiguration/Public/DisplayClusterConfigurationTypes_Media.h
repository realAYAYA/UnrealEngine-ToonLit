// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterConfigurationTypes_Base.h"
#include "DisplayClusterConfigurationTypes_MediaSync.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MediaOutput.h"

#include "DisplayClusterConfigurationTypes_Media.generated.h"


/*
 * Media input item
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaInput
{
	GENERATED_BODY()

public:
	/** Media source to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaSource> MediaSource;
};


/*
 * Media output item
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaOutput
{
	GENERATED_BODY()

public:
	/** Media output to use */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media")
	TObjectPtr<UMediaOutput> MediaOutput;

	/** Media output synchronization policy */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Media", meta = (DisplayName = "Capture Synchronization"))
	TObjectPtr<UDisplayClusterMediaOutputSynchronizationPolicy> OutputSyncPolicy;
};


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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	FDisplayClusterConfigurationMediaInput MediaInput;

	/** Media outputs to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	TArray<FDisplayClusterConfigurationMediaOutput> MediaOutputs;

public:
	/** Returns true if a media source assigned */
	bool IsMediaInputAssigned() const;

	/** Returns true if at least one media output assigned */
	bool IsMediaOutputAssigned() const;

#if WITH_EDITORONLY_DATA

protected:
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property has been deprecated"))
	FString MediaSharingNode_DEPRECATED;

public:
	UE_DEPRECATED(5.3, "This property has been deprecated. Please refer new MediaInput property.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property has been deprecated. Please refer new MediaInput property."))
	TObjectPtr<UMediaSource> MediaSource;

	UE_DEPRECATED(5.3, "This property has been deprecated. Please refer new MediaOutputs property.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property has been deprecated. Please refer new MediaOutputs property."))
	TObjectPtr<UMediaOutput> MediaOutput;

	UE_DEPRECATED(5.3, "This property has been deprecated.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property has been deprecated."))
	TObjectPtr<UDisplayClusterMediaOutputSynchronizationPolicy> OutputSyncPolicy;

#endif // WITH_EDITORONLY_DATA
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS


/*
 * Media input group (ICVFX)
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaInputGroup
	: public FDisplayClusterConfigurationMediaInput
{
	GENERATED_BODY()

public:
	/** Cluster nodes that use media source below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Media, meta = (ClusterItemType = ClusterNodes))
	FDisplayClusterConfigurationClusterItemReferenceList ClusterNodes;
};


/*
 * Media output group (ICVFX)
 */
USTRUCT(Blueprintable)
struct DISPLAYCLUSTERCONFIGURATION_API FDisplayClusterConfigurationMediaOutputGroup
	: public FDisplayClusterConfigurationMediaOutput
{
	GENERATED_BODY()

public:
	/** Cluster nodes that export media via MediaOutput below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Media, meta = (ClusterItemType = ClusterNodes))
	FDisplayClusterConfigurationClusterItemReferenceList ClusterNodes;
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

	/** Media input mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	TArray<FDisplayClusterConfigurationMediaInputGroup> MediaInputGroups;

	/** Media output mapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
	TArray<FDisplayClusterConfigurationMediaOutputGroup> MediaOutputGroups;

public:
	/** Returns true if a specific cluster node has media source assigned */
	bool IsMediaInputAssigned(const FString& NodeId) const;

	/** Returns true if a specific cluster node has media source assigned */
	bool IsMediaOutputAssigned(const FString& NodeId) const;

	/** Returns media source bound to a specific cluster node */
	UMediaSource* GetMediaSource(const FString& NodeId) const;

	/** Returns media outputs bound to a specific cluster node */
	TArray<FDisplayClusterConfigurationMediaOutputGroup> GetMediaOutputGroups(const FString& NodeId) const;

public:
	UE_DEPRECATED(5.3, "This function has beend deprecated. Please use GetMediaOutputGroups.")
	TArray<UMediaOutput*> GetMediaOutputs(const FString& NodeId) const
	{
		return {};
	}

	UE_DEPRECATED(5.3, "This function has beend deprecated. Please use GetMediaOutputGroups.")
	UDisplayClusterMediaOutputSynchronizationPolicy* GetOutputSyncPolicy(const FString& NodeId) const
	{
		return nullptr;
	}

	UE_DEPRECATED(5.3, "This function has beend deprecated. Please use GetMediaOutputGroups.")
	UMediaOutput* GetMediaOutput(const FString& NodeId) const
	{
		return nullptr;
	}

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
