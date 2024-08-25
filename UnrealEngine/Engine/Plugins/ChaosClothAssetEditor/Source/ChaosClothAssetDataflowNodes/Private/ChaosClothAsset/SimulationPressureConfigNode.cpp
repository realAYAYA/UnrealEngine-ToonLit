// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPressureConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPressureConfigNode)

FChaosClothAssetSimulationPressureConfigNode::FChaosClothAssetSimulationPressureConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&Pressure.WeightMap);
}

void FChaosClothAssetSimulationPressureConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("Pressure")), Pressure, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
	{
		return FabricFacade.GetPressure();
	}, {});
}
