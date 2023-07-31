// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "IAudioMotorSim.h"
#include "RevLimiterMotorSimComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRevLimiterStateChanged, bool, bNewState);

// Temporarily cuts throttle and reduces RPM when drifting or in the air
UCLASS(ClassGroup = "AudioMotorSim", meta = (BlueprintSpawnableComponent))
class URevLimiterMotorSimComponent : public UAudioMotorSimComponent
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevLimiter")
	float LimitTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevLimiter")
	float DecelScale = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevLimiter")
	float AirMaxThrottleTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevLimiter")
	float SideSpeedThreshold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RevLimiter")
	float LimiterMaxRpm = 0.f;

	UPROPERTY(BlueprintAssignable)
	FOnRevLimiterStateChanged OnRevLimiterStateChanged;

private:
	// Time remaining where the limiter is forcing throttle down
	float TimeRemaining = 0.f;
	float TimeInAir = 0.f;

	bool bActive = false;

public:
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;
	virtual void Reset() override;
};