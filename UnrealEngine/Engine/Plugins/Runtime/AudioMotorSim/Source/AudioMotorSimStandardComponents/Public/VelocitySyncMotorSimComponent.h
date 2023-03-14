// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "IAudioMotorSim.h"
#include "Curves/CurveFloat.h"
#include "VelocitySyncMotorSimComponent.generated.h"

// Sets Rpm directly from speed using a curve, if under a speed threshold or if the throttle is released for a period of time
UCLASS(ClassGroup = "AudioMotorSim", meta = (BlueprintSpawnableComponent))
class UVelocitySyncMotorSimComponent : public UAudioMotorSimComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocitySync")
	float NoThrottleTime = 0.1f;

	// Speed below which gears will be ignored, and RPM will sync directly to velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocitySync")
	float SpeedThreshold = 200.f;

	UPROPERTY(EditAnywhere, Category = "VelocitySync")
	FRuntimeFloatCurve SpeedToRpmCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocitySync")
	float InterpSpeed = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocitySync")
	float InterpTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VelocitySync")
	float FirstGearThrottleThreshold = 0.5f;
	
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;
	virtual void Reset() override;

private:
	float NoThrottleTimeElapsed = 0.f;
	float InterpTimeLeft = 0.f;
};