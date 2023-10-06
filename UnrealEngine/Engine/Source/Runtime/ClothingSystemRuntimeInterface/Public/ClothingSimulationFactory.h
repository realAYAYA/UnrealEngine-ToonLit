// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ClothingSimulationInterface.h"
#include "Containers/ArrayView.h"
#include "Features/IModularFeature.h"
#include "Misc/AssertionMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClothingSimulationFactory.generated.h"

class FName;
class IClothingSimulation;
class UClass;
class UClothConfigBase;
class UClothingAssetBase;
class UClothingSimulationInteractor;
class UEnum;

// An interface for a class that will provide default simulation factory classes
// Used by modules wanting to override clothing simulation to provide their own implementation
class IClothingSimulationFactoryClassProvider : public IModularFeature
{
public:

	// The feature name to register against for providers
	static CLOTHINGSYSTEMRUNTIMEINTERFACE_API const FName FeatureName;

	// Called by the engine to get the default clothing simulation factory to use
	// for skeletal mesh components (see USkeletalMeshComponent constructor).
	// Returns Factory class for simulations or nullptr to disable clothing simulation
	UE_DEPRECATED(4.25, "GetDefaultSimulationFactoryClass() has been deprecated. Use IClothingSimulationFactoryClassProvider::GetSimulationFactoryClass() or UClothingSimulationFactory::GetDefaultClothingSimulationFactoryClass() instead.")
	virtual UClass* GetDefaultSimulationFactoryClass() { return nullptr; }

	// Called by the engine to get the clothing simulation factory associated with this
	// provider for skeletal mesh components (see USkeletalMeshComponent constructor).
	// Returns Factory class for simulations or nullptr to disable clothing simulation
	virtual TSubclassOf<class UClothingSimulationFactory> GetClothingSimulationFactoryClass() const = 0;
};

// Any clothing simulation factory should derive from this interface object to interact with the engine
UCLASS(Abstract, MinimalAPI)
class UClothingSimulationFactory : public UObject
{
	GENERATED_BODY()

public:
	// Return the default clothing simulation factory class as set by the build or by
	// the p.Cloth.DefaultClothingSimulationFactoryClass console variable if any available.
	// Otherwise return the last registered factory.
	static CLOTHINGSYSTEMRUNTIMEINTERFACE_API TSubclassOf<class UClothingSimulationFactory> GetDefaultClothingSimulationFactoryClass();

	// Create a simulation object for a skeletal mesh to use (see IClothingSimulation)
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual IClothingSimulation* CreateSimulation()
	PURE_VIRTUAL(UClothingSimulationFactory::CreateSimulation, return nullptr;);

	// Destroy a simulation object, guaranteed to be a pointer returned from CreateSimulation for this factory
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual void DestroySimulation(IClothingSimulation* InSimulation)
	PURE_VIRTUAL(UClothingSimulationFactory::DestroySimulation, );

	// Given an asset, decide whether this factory can create a simulation to use the data inside
	// (return false if data is invalid or missing in the case of custom data)
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual bool SupportsAsset(UClothingAssetBase* InAsset)
	PURE_VIRTUAL(UClothingSimulationFactory::SupportsAsset, return false;);

	// Whether or not we provide an interactor object to manipulate the simulation at runtime.
	// If true is returned then CreateInteractor *must* create a valid object to handle this
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual bool SupportsRuntimeInteraction()
	PURE_VIRTUAL(UClothingSimulationFactory::SupportsRuntimeInteraction, return false;);

	// Creates the runtime interactor object for a clothing simulation. This object will
	// receive events allowing it to write data to the simulation context in a safe manner
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual UClothingSimulationInteractor* CreateInteractor()
	PURE_VIRTUAL(UClothingSimulationFactory::CreateInteractor, return nullptr;);

	// Return the cloth config type for this cloth factory
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual TArrayView<const TSubclassOf<UClothConfigBase>> GetClothConfigClasses() const
	PURE_VIRTUAL(UClothingSimulationFactory::GetClothConfigClasses, return TArrayView<const TSubclassOf<UClothConfigBase>>(););

	// Return an enum of the weight map targets that can be used with this simulation.
	CLOTHINGSYSTEMRUNTIMEINTERFACE_API virtual const UEnum* GetWeightMapTargetEnum() const
	PURE_VIRTUAL(UClothingSimulationFactory::GetWeightMapTargetEnum, return nullptr;);
};
