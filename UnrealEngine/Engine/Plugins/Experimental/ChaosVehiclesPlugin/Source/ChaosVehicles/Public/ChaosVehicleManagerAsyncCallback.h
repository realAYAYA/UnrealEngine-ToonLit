// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsPublic.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"

class UChaosVehicleMovementComponent;

DECLARE_STATS_GROUP(TEXT("ChaosVehicleManager"), STATGROUP_ChaosVehicleManager, STATGROUP_Advanced);

enum EChaosAsyncVehicleDataType : int8
{
	AsyncInvalid,
	AsyncDefault,
};

struct CHAOSVEHICLES_API FWheelsOutput
{
	FWheelsOutput()
		: InContact(false)
		, SteeringAngle(0.f)
		, AngularPosition(0.f)
		, AngularVelocity(0.f)
		, WheelRadius(0.f)
		, LateralAdhesiveLimit(0.f)
		, LongitudinalAdhesiveLimit(0.f)
		, SlipAngle(0.f)
		, bIsSlipping(false)
		, SlipMagnitude(0.f)
		, bIsSkidding(false)
		, SkidMagnitude(0.f)
		, SkidNormal(FVector(1,0,0))
		, SuspensionOffset(0.f)
		, SpringForce(0.f)
		, NormalizedSuspensionLength(0.f)
		, bBlockingHit(false)
		, ImpactPoint(FVector::ZeroVector)
		, PhysMaterial(nullptr)
	{
	}

	// wheels
	bool InContact;
	float SteeringAngle;
	float AngularPosition;
	float AngularVelocity;
	float WheelRadius;

	float LateralAdhesiveLimit;
	float LongitudinalAdhesiveLimit;

	float SlipAngle;
	bool bIsSlipping;
	float SlipMagnitude;
	bool bIsSkidding;
	float SkidMagnitude;
	FVector SkidNormal;

	// suspension related
	float SuspensionOffset;
	float SpringForce;
	float NormalizedSuspensionLength;

	bool bBlockingHit;
	FVector ImpactPoint;
	TWeakObjectPtr<UPhysicalMaterial> PhysMaterial;
};

/**
 * Per Vehicle Output State from Physics Thread to Game Thread                            
 */
struct CHAOSVEHICLES_API FPhysicsVehicleOutput
{
	FPhysicsVehicleOutput()
		: CurrentGear(0)
		, TargetGear(0)
		, EngineRPM(0.f)
		, EngineTorque(0.f)
		, TransmissionRPM(0.f)
		, TransmissionTorque(0.f)
	{
	}

	TArray<FWheelsOutput> Wheels;
	int32 CurrentGear;
	int32 TargetGear;
	float EngineRPM;
	float EngineTorque;
	float TransmissionRPM;
	float TransmissionTorque;
};

/**
 * Per Vehicle Input State from Game Thread to Physics Thread
 */                             
struct CHAOSVEHICLES_API FChaosVehicleAsyncInput
{
	const EChaosAsyncVehicleDataType Type;
	UChaosVehicleMovementComponent* Vehicle;
	
	Chaos::FSingleParticlePhysicsProxy* Proxy;

	/** 
	* Vehicle simulation running on the Physics Thread
	*/
	virtual TUniquePtr<struct FChaosVehicleAsyncOutput> Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const = 0;

	virtual void ApplyDeferredForces(Chaos::FRigidBodyHandle_Internal* RigidHandle) const = 0;

	FChaosVehicleAsyncInput(EChaosAsyncVehicleDataType InType = EChaosAsyncVehicleDataType::AsyncInvalid)
		: Type(InType)
		, Vehicle(nullptr)
	{
		Proxy = nullptr;	//indicates async/sync task not needed
	}

	virtual ~FChaosVehicleAsyncInput() = default;
};

struct FChaosVehicleManagerAsyncInput : public Chaos::FSimCallbackInput
{
	TArray<TUniquePtr<FChaosVehicleAsyncInput>> VehicleInputs;

	TWeakObjectPtr<UWorld> World;
	int32 Timestamp = INDEX_NONE;

	void Reset()
	{
		VehicleInputs.Reset();
		World.Reset();
	}
};

/**
 * Async Output Data
 */
struct CHAOSVEHICLES_API FChaosVehicleAsyncOutput
{
	const EChaosAsyncVehicleDataType Type;
	bool bValid;	// indicates no work was done
	FPhysicsVehicleOutput VehicleSimOutput;

	FChaosVehicleAsyncOutput(EChaosAsyncVehicleDataType InType = EChaosAsyncVehicleDataType::AsyncInvalid)
		: Type(InType)
		, bValid(false)
	{ }

	virtual ~FChaosVehicleAsyncOutput() = default;
};


/**
 * Async Output for all of the vehicles handled by this Vehicle Manager
 */
struct FChaosVehicleManagerAsyncOutput : public Chaos::FSimCallbackOutput
{
	TArray<TUniquePtr<FChaosVehicleAsyncOutput>> VehicleOutputs;
	int32 Timestamp = INDEX_NONE;

	void Reset()
	{
		VehicleOutputs.Reset();
	}
};

/**
 * Async callback from the Physics Engine where we can perform our vehicle simulation
 */
class CHAOSVEHICLES_API FChaosVehicleManagerAsyncCallback : public Chaos::TSimCallbackObject<FChaosVehicleManagerAsyncInput, FChaosVehicleManagerAsyncOutput, Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::ContactModification>
{
private:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifications) override;
};
