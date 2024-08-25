// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMaxDistanceConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMaxDistanceConfigNode)

FChaosClothAssetSimulationMaxDistanceConfigNode::FChaosClothAssetSimulationMaxDistanceConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&MaxDistance.WeightMap);
}

void FChaosClothAssetSimulationMaxDistanceConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this, &MaxDistance, {}, ECollectionPropertyFlags::Intrinsic);  // Intrinsic since the deformer weights needs to be recalculated
}
