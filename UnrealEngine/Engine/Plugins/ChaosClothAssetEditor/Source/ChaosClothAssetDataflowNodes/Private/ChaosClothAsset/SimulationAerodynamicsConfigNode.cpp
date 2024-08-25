// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationAerodynamicsConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/SimulationBaseConfigNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationAerodynamicsConfigNode)

FChaosClothAssetSimulationAerodynamicsConfigNode::FChaosClothAssetSimulationAerodynamicsConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&Drag.WeightMap);
	RegisterInputConnection(&Lift.WeightMap);
}

void FChaosClothAssetSimulationAerodynamicsConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetProperty(this, &FluidDensity);
	PropertyHelper.SetProperty(this, &WindVelocity);
	
	PropertyHelper.SetSolverPropertyWeighted(FName(TEXT("Drag")), Drag, [](
				const UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade)-> float
	{
		return ClothFacade.GetSolverAirDamping();
	},{});

	PropertyHelper.SetSolverPropertyWeighted(FName(TEXT("Lift")), Lift, [](
				const UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade)-> float
	{
		return ClothFacade.GetSolverAirDamping();
	},{});
}
