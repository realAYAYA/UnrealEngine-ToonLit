// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsPublic.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "TransmissionSystem.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "SimModule/SimulationModuleBase.h"

#include "ChaosSimModuleManagerAsyncCallback.generated.h"

class UModularVehicleComponent;
class UModularVehicleBaseComponent;
class FGeometryCollectionPhysicsProxy;

DECLARE_STATS_GROUP(TEXT("ChaosSimModuleManager"), STATGROUP_ChaosSimModuleManager, STATGROUP_Advanced);


struct FSimModuleDebugParams
{
	bool EnableMultithreading = false;
	bool EnableNetworkStateData = true;
};

enum EChaosAsyncVehicleDataType : int8
{
	AsyncInvalid,
	AsyncDefault,
};


struct FModuleTransform
{
	int TransforIndex;
	FTransform Transform;
};

/** Vehicle inputs from the player controller */
USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleInputs
{
	GENERATED_USTRUCT_BODY()

		FModularVehicleInputs()
		: Steering(0.f)
		, Throttle(0.f)
		, Brake(0.f)
		, Handbrake(0.f)
		, Pitch(0.f)
		, Roll(0.f)
		, Yaw(0.f)
		, Boost(0.f)
		, Drift(0.f)
		, Reverse(false)
		, KeepAwake(false)
		{}

	// Steering output to physics system. Range -1...1
	UPROPERTY()
	float Steering;

	// Accelerator output to physics system. Range 0...1
	UPROPERTY()
	float Throttle;

	// Brake output to physics system. Range 0...1
	UPROPERTY()
	float Brake;

	// Handbrake output to physics system. Range 0...1
	UPROPERTY()
	float Handbrake;

	// Body Pitch output to physics system. Range -1...1
	UPROPERTY()
	float Pitch;

	// Body Roll output to physics system. Range -1...1
	UPROPERTY()
	float Roll;

	// Body Yaw output to physics system. Range -1...1
	UPROPERTY()
	float Yaw;

	// Boost output to physics system. Range -1...1
	UPROPERTY()
	float Boost;

	// Boost output to physics system. Range 0...1
	UPROPERTY()
	float Drift;

	// Reversing state
	UPROPERTY()
	bool Reverse;

	// Keep vehicle awake
	UPROPERTY()
	bool KeepAwake;

};


/** Vehicle input data that will be used in the input history to be applied while simulating */
USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FNetworkModularVehicleInputs : public FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()

	/** List of incoming control inputs coming from the local client */
	UPROPERTY()
	FModularVehicleInputs VehicleInputs;

	/**  Apply the data onto the network physics component */
	virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two input */
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
};

template<>
struct TStructOpsTypeTraits<FNetworkModularVehicleInputs> : public TStructOpsTypeTraitsBase2<FNetworkModularVehicleInputs>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/** Vehicle state data that will be used in the state history to rewind the simulation */
USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FNetworkModularVehicleStates : public FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()

	Chaos::FModuleNetDataArray ModuleData;

	/**  Apply the data onto the network physics component */
	virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two states */
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
};

template<>
struct TStructOpsTypeTraits<FNetworkModularVehicleStates> : public TStructOpsTypeTraitsBase2<FNetworkModularVehicleStates>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/**
 * Per Vehicle Output State from Physics Thread to Game Thread
 */
struct CHAOSMODULARVEHICLEENGINE_API FPhysicsVehicleOutput
{
	FPhysicsVehicleOutput()
	{
	}

	~FPhysicsVehicleOutput()
	{
		Clean();
	}

	void Clean()
	{
		for (Chaos::FSimOutputData* Data : SimTreeOutputData)
		{
			delete Data;
		}
		SimTreeOutputData.Empty();
	}

	TArray<Chaos::FSimOutputData*> SimTreeOutputData;
};

struct CHAOSMODULARVEHICLEENGINE_API FPhysicsModularVehicleTraits
{
	using InputsType = FNetworkModularVehicleInputs;
	using StatesType = FNetworkModularVehicleStates;
};

// #TBD
struct FGameStateInputs
{
	//bool IsInWater;
	//bool IsInAir
};


/**
 * Per Vehicle input State from Game Thread to Physics Thread
 */
struct CHAOSMODULARVEHICLEENGINE_API FPhysicsModularVehicleInputs
{
	FPhysicsModularVehicleInputs()
		: TraceParams()
		, TraceCollisionResponse()
	{
	}
	mutable FNetworkModularVehicleInputs NetworkInputs;
	mutable FCollisionQueryParams TraceParams;
	mutable FCollisionResponseContainer TraceCollisionResponse;
	mutable FGameStateInputs StateInputs;
};

/**
 * Per Vehicle Input State from Game Thread to Physics Thread
 */
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleAsyncInput
{
	FModularVehicleAsyncInput(EChaosAsyncVehicleDataType InType = EChaosAsyncVehicleDataType::AsyncInvalid)
		: Type(InType)
		, Vehicle(nullptr)
	{
		Proxy = nullptr;	//indicates async/sync task not needed
	}

	virtual ~FModularVehicleAsyncInput() = default;

	/**
	* Vehicle simulation running on the Physics Thread
	*/
	virtual TUniquePtr<struct FModularVehicleAsyncOutput> Simulate(UWorld* World, const float DeltaSeconds, const float TotalSeconds, bool& bWakeOut) const;
	virtual void ApplyDeferredForces() const;
	virtual void ProcessInputs();

	void SetVehicle(UModularVehicleBaseComponent* VehicleIn) { Vehicle = VehicleIn; }
	UModularVehicleBaseComponent* GetVehicle() { return Vehicle; }

	const EChaosAsyncVehicleDataType Type;
	IPhysicsProxyBase* Proxy;

	FPhysicsModularVehicleInputs PhysicsInputs;

private:

	UModularVehicleBaseComponent* Vehicle;

};

struct FChaosSimModuleManagerAsyncInput : public Chaos::FSimCallbackInput
{
	TArray<TUniquePtr<FModularVehicleAsyncInput>> VehicleInputs;

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
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleAsyncOutput
{
	const EChaosAsyncVehicleDataType Type;
	bool bValid;	// indicates no work was done
	FPhysicsVehicleOutput VehicleSimOutput;

	FModularVehicleAsyncOutput(EChaosAsyncVehicleDataType InType = EChaosAsyncVehicleDataType::AsyncInvalid)
		: Type(InType)
		, bValid(false)
	{ }

	virtual ~FModularVehicleAsyncOutput()
	{
		VehicleSimOutput.Clean();
	}
};


/**
 * Async Output for all of the vehicles handled by this Vehicle Manager
 */
struct FChaosSimModuleManagerAsyncOutput : public Chaos::FSimCallbackOutput
{
	TArray<TUniquePtr<FModularVehicleAsyncOutput>> VehicleOutputs;
	int32 Timestamp = INDEX_NONE;

	void Reset()
	{
		VehicleOutputs.Reset();
	}
};

/**
 * Async callback from the Physics Engine where we can perform our vehicle simulation
 */
class CHAOSMODULARVEHICLEENGINE_API FChaosSimModuleManagerAsyncCallback : public Chaos::TSimCallbackObject<FChaosSimModuleManagerAsyncInput, FChaosSimModuleManagerAsyncOutput
	, Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::Rewind | Chaos::ESimCallbackOptions::ContactModification>
{
public:
	virtual FName GetFNameForStatId() const override;
private:
	virtual void ProcessInputs_Internal(int32 PhysicsStep) override;
	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifications) override;
};
