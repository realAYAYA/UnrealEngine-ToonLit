// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMeterTypes.generated.h"

USTRUCT(BlueprintType)
struct FMeterChannelInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AudioMeter)
	float MeterValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AudioMeter)
	float PeakValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AudioMeter)
	float ClippingValue = 0.0f;
};

inline bool operator==(const FMeterChannelInfo& lhs, const FMeterChannelInfo& rhs)
{
	return FMath::IsNearlyEqual(lhs.MeterValue, rhs.MeterValue) &&
		FMath::IsNearlyEqual(lhs.PeakValue, rhs.PeakValue) &&
		FMath::IsNearlyEqual(lhs.ClippingValue, rhs.ClippingValue);
}
