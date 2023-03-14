// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotorPhysicsSimComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MotorPhysicsSimComponent)

void UMotorPhysicsSimComponent::Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo)
{
	if(ensure(Weight > 0.f && EngineGearRatio > 0.f) == false)
	{
		return;
	}
		
	const int32 MaxGear = GearRatios.Num() - 1;
	const float GearRatio = GetGearRatio(RuntimeInfo.Gear, Input.bClutchEngaged);
	const float Throttle = FMath::Abs(Input.Throttle);
	
	const float SpeedKmh = AudioMotorSim::CmsToKmh(Input.Speed);

	int32& Gear = RuntimeInfo.Gear;
	const int32 PrevGear = Gear;
	
	const float ClutchModifier = Input.bClutchEngaged ? ClutchedForceModifier : 1.f;
	
	//Forces in Newtons.
	const float CurrentEngineTorque = EngineTorque * Throttle * GearRatio * ClutchModifier;
	
	const float BrakingForce = Input.Brake * BrakingHorsePower;
	const float WindResistance = WindResistancePerVelocity * FMath::Square(SpeedKmh);
	const float FrictionResistance = Weight * GroundFriction * Input.SurfaceFrictionModifier;
	const float EngineResistance = CurrentEngineTorque * FMath::Max(Input.MotorFrictionModifier - Throttle, 0.0f) * EngineFriction;
	
	const float NetForce = CurrentEngineTorque - BrakingForce - WindResistance - FrictionResistance - EngineResistance;

	// Convert Rpm to velocity to apply forces
	float VirtualSpeedKmh =  CalcVelocity(GearRatio, RuntimeInfo.Rpm);
	VirtualSpeedKmh += ((NetForce / Weight) * Input.DeltaTime);
	VirtualSpeedKmh = FMath::Max(VirtualSpeedKmh, 0.0f);

	// Now working backwards, what is the engine RPM as a results of this velocity and gear ratio.
	float Rpm = CalcRpm(GearRatio, VirtualSpeedKmh);

	// Now determine if we need to shift.
	if (Input.bCanShift && (Rpm > UpShiftMaxRpm))
	{
		if (Gear < MaxGear || bUseInfiniteGears)
		{
			++Gear;
		}

		Rpm = CalcRpm(GetGearRatio(Gear, Input.bClutchEngaged), VirtualSpeedKmh);
	}
	// If the RPM that we'd be at if we downshift is below DownShiftRPM, then it makes sense to downshift.
	else if (Gear >= 1 && DownShiftStartRpm > CalcRpm(GetGearRatio(Gear - 1, Input.bClutchEngaged), VirtualSpeedKmh))
	{
		Gear = bAlwaysDownshiftToZerothGear ? 0 : Gear - 1;
		Rpm = DownShiftStartRpm;
	}

	if(Gear != PrevGear)
	{
		OnGearChangedEvent.Broadcast(Gear);
	}

	RuntimeInfo.Rpm = FMath::FInterpTo(RuntimeInfo.Rpm, Rpm, Input.DeltaTime, RpmInterpSpeed);
}

float UMotorPhysicsSimComponent::CalcRpm(const float InGearRatio, const float InSpeed) const
{
	return InSpeed * InGearRatio / EngineGearRatio;
}

float UMotorPhysicsSimComponent::CalcVelocity(const float InGearRatio, const float InRpm) const
{
	check(InGearRatio > 0.f);
	return InRpm * EngineGearRatio / InGearRatio;
}

float UMotorPhysicsSimComponent::GetGearRatio(const int32 InGear, const bool bInClutched) const
{
	if(bInClutched)
	{
		return ClutchedGearRatio;
	}
	
	int32 Gear = InGear;
	if(bUseInfiniteGears == false)
	{
		Gear = FMath::Clamp(Gear, 0, GearRatios.Num());
	}
	
	return InterpGearRatio(Gear);
}

float UMotorPhysicsSimComponent::InterpGearRatio(const int32 InGear) const
{
	const int32 MaxGear = GearRatios.Num() - 1;
	if (InGear <= MaxGear)
	{
		return GearRatios[InGear];
	}

	return GearRatios[MaxGear] * FMath::Pow(InfiniteGearRatio, InGear - MaxGear);
}

void UMotorPhysicsSimComponent::Reset()
{
}
