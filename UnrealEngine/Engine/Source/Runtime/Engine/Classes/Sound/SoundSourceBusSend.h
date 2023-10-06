// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "SoundSourceBusSend.generated.h"

class USoundSourceBus;
class UAudioBus;

UENUM(BlueprintType)
enum class ESourceBusSendLevelControlMethod : uint8
{
	// A send based on linear interpolation between a distance range and send-level range
	Linear,

	// A send based on a supplied curve
	CustomCurve,

	// A manual send level (Uses the specified constant send level value. Useful for 2D sounds.)
	Manual,
};

USTRUCT(BlueprintType)
struct FSoundSourceBusSendInfo
{
	GENERATED_USTRUCT_BODY()

	/*
		Manual: Use Send Level only
		Linear: Interpolate between Min and Max Send Levels based on listener distance (between Distance Min and Distance Max)
		Custom Curve: Use the float curve to map Send Level to distance (0.0-1.0 on curve maps to Distance Min - Distance Max)
	*/
	UPROPERTY(EditAnywhere, Category = BusSend)
	ESourceBusSendLevelControlMethod SourceBusSendLevelControlMethod;

	// A source Bus to send the audio to. Source buses sonify (make audible) the audio sent to it and are themselves sounds which take up a voice slot in the audio engine.
	UPROPERTY(EditAnywhere, Category = BusSend)
	TObjectPtr<USoundSourceBus> SoundSourceBus;

	// An audio bus to send the audio to. Audio buses can be used to route audio to DSP effects or other purposes. E.g. side-chaining, analysis, etc. Audio buses are not audible unless hooked up to a source bus.
	UPROPERTY(EditAnywhere, Category = BusSend)
	TObjectPtr<UAudioBus> AudioBus;

	// The amount of audio to send to the bus.
	UPROPERTY(EditAnywhere, Category = BusSend, meta = (DisplayName = "Manual Send Level"))
	float SendLevel;

	// The amount to send to the bus when sound is located at a distance equal to value specified in the min send distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend)
	float MinSendLevel;

	// The amount to send to the bus when sound is located at a distance equal to value specified in the max send distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend)
	float MaxSendLevel;

	// The distance at which the min send Level is sent to the bus
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend)
	float MinSendDistance;

	// The distance at which the max send level is sent to the bus
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend)
	float MaxSendDistance;

	// The custom curve to use for distance-based bus send level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BusSend)
	FRuntimeFloatCurve CustomSendLevelCurve;

	FSoundSourceBusSendInfo()
		: SourceBusSendLevelControlMethod(ESourceBusSendLevelControlMethod::Manual)
		, SoundSourceBus(nullptr)
		, AudioBus(nullptr)
		, SendLevel(1.0f)
		, MinSendLevel(0.0f)
		, MaxSendLevel(1.0f)
		, MinSendDistance(100.0f)
		, MaxSendDistance(1000.0f)
	{
	}
};
