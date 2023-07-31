// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingSimulationInteractor.h"
#include "ClothingSimulationNv.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Math/MathFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClothingSimulationInteractorNv.generated.h"

class FClothingSimulationContextNv;
class FClothingSimulationNv;
class IClothingSimulation;
class IClothingSimulationContext;
class UObject;
struct FFrame;

// Command signature for handling synced command buffer
DECLARE_DELEGATE_TwoParams(NvInteractorCommand, FClothingSimulationNv*, FClothingSimulationContextNv*)

class UE_DEPRECATED(5.1, "NvCloth is no longer a supported clothing simlation provider, prefer the Chaos simulation moving forward.") UClothingSimulationInteractorNv;
UCLASS(BlueprintType)
class CLOTHINGSYSTEMRUNTIMENV_API UClothingSimulationInteractorNv : public UClothingSimulationInteractor
{
	GENERATED_BODY()

public:

	// UClothingSimulationInteractor Interface
	virtual void Sync(IClothingSimulation* InSimulation, IClothingSimulationContext* InContext) override;

	virtual void PhysicsAssetUpdated() override;
	virtual void ClothConfigUpdated() override;
	virtual void SetAnimDriveSpringStiffness(float InStiffness) override;
	virtual void EnableGravityOverride(const FVector& InVector) override;
	virtual void DisableGravityOverride() override;

	// TODO: These new functions are currently unimplemented
	virtual void SetNumIterations(int32 /*NumIterations*/) override {}
	virtual void SetMaxNumIterations(int32 /*MaxNumIterations*/) override {}
	virtual void SetNumSubsteps(int32 /*NumSubsteps*/) override {}
	//////////////////////////////////////////////////////////////////////////

	// Set the stiffness of the resistive damping force for anim drive
	UFUNCTION(BlueprintCallable, Category=ClothingSimulation)
	void SetAnimDriveDamperStiffness(float InStiffness);

protected:

	virtual UClothingInteractor* CreateClothingInteractor() override { return nullptr; }

private:
};
