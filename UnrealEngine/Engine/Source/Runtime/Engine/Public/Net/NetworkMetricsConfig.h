// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NetworkMetricsConfig.generated.h"

UENUM()
enum class ENetworkMetricEnableMode : uint8
{
	EnableForAllReplication,
	EnableForIrisOnly,
	EnableForNonIrisOnly
};

USTRUCT()
struct FNetworkMetricConfig
{
	GENERATED_BODY()

public:
	/** The name of the metric to register the listener. */
	UPROPERTY()
	FName MetricName;

	/** A sub-class of UNetworkMetricBaseListener. */
	UPROPERTY()
	TSoftClassPtr<class UNetworkMetricsBaseListener> Class;

	UPROPERTY()
	ENetworkMetricEnableMode EnableMode = ENetworkMetricEnableMode::EnableForAllReplication;
};

UCLASS(Config=Engine)
class UNetworkMetricsConfig : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(Config)
	TArray<FNetworkMetricConfig> Listeners;
};
