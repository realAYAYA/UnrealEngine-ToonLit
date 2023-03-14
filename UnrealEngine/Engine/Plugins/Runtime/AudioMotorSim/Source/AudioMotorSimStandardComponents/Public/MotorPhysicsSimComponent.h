// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "IAudioMotorSim.h"
#include "MotorPhysicsSimComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGearChanged, int32, NewGear);

// Uses throttle input to run a physics simulation to drive RPM and shift gears when needed
UCLASS(ClassGroup = "AudioMotorSim", meta = (BlueprintSpawnableComponent))
class UMotorPhysicsSimComponent : public UAudioMotorSimComponent
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force")
	float Weight = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force")
	float EngineTorque = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force")
	float BrakingHorsePower = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	TArray<float> GearRatios = { 3.5f, 2.f, 1.4f, 1.f, .7f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	float ClutchedGearRatio = 10.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	bool bUseInfiniteGears = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	bool bAlwaysDownshiftToZerothGear = false;
	
	// how much to scale gear ratio per gear past the max gear
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	float InfiniteGearRatio = 0.9f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	float UpShiftMaxRpm = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gears")
	float DownShiftStartRpm = 0.94f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
	float ClutchedForceModifier = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
	float EngineGearRatio = 50.f;

	// How much of the torque is loss due to engine friction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
	float EngineFriction = 0.66f; 

	// Coefficient of Rolling Resistance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
	float GroundFriction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
	float WindResistancePerVelocity = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsSim")
	float ThrottleInterpolationTime = 0.050f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsSim")
	float RpmInterpSpeed = 0.0f;
	
	UPROPERTY(BlueprintAssignable, Category = "PhysicsSim")
	FOnGearChanged OnGearChangedEvent;

	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;
	virtual void Reset() override;

private:
	float CalcRpm(const float InGearRatio, const float InSpeed) const;

	float CalcVelocity(const float InGearRatio, const float InRpm) const;

	float GetGearRatio(const int32 InGear, const bool bInClutched) const;

	float InterpGearRatio(const int32 InGear) const;
};