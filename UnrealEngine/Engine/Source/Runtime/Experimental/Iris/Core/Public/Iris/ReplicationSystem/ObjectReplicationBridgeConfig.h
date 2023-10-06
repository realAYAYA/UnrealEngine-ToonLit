// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "UObject/ObjectMacros.h"
//#include "UObject/NameTypes.h"
#include "ObjectReplicationBridgeConfig.generated.h"

USTRUCT()
struct FObjectReplicationBridgePollConfig
{
	GENERATED_BODY()

	/**
	 * Instances of this class, specified by its fully qualified path, should use the supplied poll frame period to check for modified replicated properties.
	 * Object and Actor are forbidden class names for performance reasons.
	 */
	UPROPERTY()
	FName ClassName;

	/**
	 * How many times per second should we poll for modified replicated properties.
	 * The value will be converted into a frame count based on the current TickRate up to a maximum of 255 frames
	 * This means the slowest poll frequency is 255*MaxTickRate (ex: 8.5secs at 30hz)
	 * If set to 0 it means we poll the object every frame.
	 */
	UPROPERTY()
	float PollFrequency = 0.0f;
	

	/** Whether instances of subclasses should also use this poll period. */
	UPROPERTY()
	bool bIncludeSubclasses = true;
};

USTRUCT()
struct FObjectReplicationBridgeFilterConfig
{
	GENERATED_BODY()

	/** Instances of this class should use the filter supplied. */
	UPROPERTY()
	FName ClassName;

	/** The name of the filter to set on the class instances. */
	UPROPERTY()
	FName DynamicFilterName;
};

USTRUCT()
struct FObjectReplicationBridgePrioritizerConfig
{
	GENERATED_BODY()

	/** Instances of this class and its subclasses, specified by its fully qualified path, should use the prioritizer supplied. */
	UPROPERTY()
	FName ClassName;

	/** The name of the prioritizer to set on the class instances. "Default" can be used to specify the default spatial prioritizer. */
	UPROPERTY()
	FName PrioritizerName;

	/** Whether this prioritizer should be used for all instances of this class and subclasses, regardless of bAlwaysRelevant and bOnlyRelevantToOwner settings on instance. */
	UPROPERTY()
	bool bForceEnableOnAllInstances = false;
};

USTRUCT()
struct FObjectReplicationBridgeDeltaCompressionConfig
{
	GENERATED_BODY()

	/** Instances of this class or derived from this class should use delta compression */
	UPROPERTY()
	FName ClassName;

	/** Set to true if delta compression should be enabled for instances derived from this class. */
	UPROPERTY()
	bool bEnableDeltaCompression = true;
};

UCLASS(transient, config=Engine)
class UObjectReplicationBridgeConfig : public UObject
{
	GENERATED_BODY()

public:

	IRISCORE_API static const UObjectReplicationBridgeConfig* GetConfig();

	IRISCORE_API TConstArrayView<FObjectReplicationBridgePollConfig> GetPollConfigs() const;
	IRISCORE_API TConstArrayView<FObjectReplicationBridgeFilterConfig> GetFilterConfigs() const;
	IRISCORE_API TConstArrayView<FObjectReplicationBridgePrioritizerConfig> GetPrioritizerConfigs() const;
	IRISCORE_API TConstArrayView<FObjectReplicationBridgeDeltaCompressionConfig> GetDeltaCompressionConfigs() const;
	FName GetDefaultSpatialFilterName() const;
	FName GetRequiredNetDriverChannelClassName() const;

protected:
	UObjectReplicationBridgeConfig();

private:
	/**
	 * Which classes should override how often they're polled for modified replicated properties.
	 * A config for a class deeper in the class hierarchy has precedence over a more generic class.
	 * By default an Actor's NetUpdateFrequency is used to calculate how often it should be polled.
	 */
	UPROPERTY(Config)
	TArray<FObjectReplicationBridgePollConfig> PollConfigs;

	/**
	 * Which classes should apply a certain filter. Subclasses will inherit the settings unless
	 * they have different relevancy or spatial behavior.
	 */
	UPROPERTY(Config)
	TArray<FObjectReplicationBridgeFilterConfig> FilterConfigs;

	/**
	 * Which classes should apply a certain prioritizer. Subclasses will inherit the settings.
	 * Instances with fixed priorities will ignore any config.
	 */
	UPROPERTY(Config)
	TArray<FObjectReplicationBridgePrioritizerConfig> PrioritizerConfigs;

	/**
	 * Which classes should enable deltacompression. Derived classes will get the same behavior unless overidden
	 */
	UPROPERTY(Config)
	TArray<FObjectReplicationBridgeDeltaCompressionConfig> DeltaCompressionConfigs;

	/**
	 * The name of the filter to apply objects that can have spatial filtering applied.
	 */
	UPROPERTY(Config)
	FName DefaultSpatialFilterName;

	/**
	 * The name of the channel class required for object replication to work.
	 */
	UPROPERTY(Config)
	FName RequiredNetDriverChannelClassName;
};

inline FName UObjectReplicationBridgeConfig::GetDefaultSpatialFilterName() const
{
	return DefaultSpatialFilterName;
}

inline FName UObjectReplicationBridgeConfig::GetRequiredNetDriverChannelClassName() const
{
	return RequiredNetDriverChannelClassName;
}
