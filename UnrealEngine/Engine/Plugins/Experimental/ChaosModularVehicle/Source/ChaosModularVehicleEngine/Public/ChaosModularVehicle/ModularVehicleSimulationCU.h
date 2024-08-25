// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "ChaosModularVehicle/ModularVehicleInputRate.h"
#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/SimModulesInclude.h"
#include "ModularVehicleBuilder.h"

struct FModularVehicleAsyncInput;
struct FChaosSimModuleManagerAsyncOutput;

namespace Chaos
{
	class FClusterUnionPhysicsProxy;
}


class CHAOSMODULARVEHICLEENGINE_API FModularVehicleSimulationCU
{
public:
	FModularVehicleSimulationCU(bool InUsingNetworkPhysicsPrediction, int8 InNetMode)
		: bUsingNetworkPhysicsPrediction(InUsingNetworkPhysicsPrediction)
		, NetMode(InNetMode)
	{
	}

	virtual ~FModularVehicleSimulationCU()
	{
		SimModuleTree.Reset();
	}

	void Initialize(TUniquePtr<Chaos::FSimModuleTree>& InSimModuleTree);
	void Terminate();

	/** Update called from Physics Thread */
	virtual void Simulate(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, IPhysicsProxyBase* Proxy);
	virtual void Simulate_ClusterUnion(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, Chaos::FClusterUnionPhysicsProxy* Proxy);

	void ApplyDeferredForces(FGeometryCollectionPhysicsProxy* RigidHandle);
	void ApplyDeferredForces(Chaos::FClusterUnionPhysicsProxy* Proxy);

	void PerformAdditionalSimWork(UWorld* InWorld, const FModularVehicleAsyncInput& InputData, Chaos::FClusterUnionPhysicsProxy* Proxy, Chaos::FAllInputs& AllInputs);

	void FillOutputState(FModularVehicleAsyncOutput& Output);

	Chaos::FControlInputs& AccessControlInputs();

	const TUniquePtr<Chaos::FSimModuleTree>& GetSimComponentTree() const {
		Chaos::EnsureIsInPhysicsThreadContext();
		return SimModuleTree;
		}

	TUniquePtr<Chaos::FSimModuleTree>& AccessSimComponentTree() {
		return SimModuleTree; 
		}

	TArray<FModularVehicleInputRate>& AccessInputInterpolation() { return InputInterpolation; }


	TUniquePtr<Chaos::FSimModuleTree> SimModuleTree;	/* Simulation modules stored in tree structure */
	TArray<FModularVehicleInputRate> InputInterpolation;
	Chaos::FAllInputs SimInputData;
	bool bUsingNetworkPhysicsPrediction;

	/** Current control inputs that is being used on the PT */
	FModularVehicleInputs VehicleInputs;

	int8 NetMode;

};