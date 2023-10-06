// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothingSimulationInteractor.h"
#include "ClothingAssetBase.h"
#include "ClothingSimulationInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothingSimulationInteractor)

void UClothingSimulationInteractor::CreateClothingInteractor(const UClothingAssetBase* ClothingAsset, int32 ClothingId)
{
	if (ClothingAsset)
	{
		if (UClothingInteractor* const ClothingInteractor = CreateClothingInteractor())
		{
			ClothingInteractor->ClothingId = ClothingId;

			ClothingInteractors.Emplace(ClothingAsset->GetName(), ClothingInteractor);
		}
	}
}

void UClothingSimulationInteractor::DestroyClothingInteractors()
{
	ClothingInteractors.Reset();
}

UClothingInteractor* UClothingSimulationInteractor::GetClothingInteractor(const FString& ClothingAssetName) const
{
	if (const TObjectPtr<UClothingInteractor>* ClothingInteractor = ClothingInteractors.Find(FName(ClothingAssetName)))
	{
		return *ClothingInteractor;
	}

	// Returning the default object will still make this a valid ClothingInteractor pointer, but one with no type or interface
	return UClothingInteractor::StaticClass()->GetDefaultObject<UClothingInteractor>();
}

void UClothingSimulationInteractor::Sync(IClothingSimulation* Simulation, IClothingSimulationContext* Context)
{
	LastNumCloths = Simulation->GetNumCloths();
	LastNumKinematicParticles = Simulation->GetNumKinematicParticles();
	LastNumDynamicParticles = Simulation->GetNumDynamicParticles();
	LastNumIterations = Simulation->GetNumIterations();
	LastNumSubsteps = Simulation->GetNumSubsteps();
	LastSimulationTime = Simulation->GetSimulationTime();

	for (const auto& ClothingInteractor : UClothingSimulationInteractor::ClothingInteractors)
	{
		if (ClothingInteractor.Value)
		{
			ClothingInteractor.Value->Sync(Simulation);
		}
	}
}

