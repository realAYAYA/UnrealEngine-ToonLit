// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationBackstopConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationBackstopConfigNode)

FChaosClothAssetSimulationBackstopConfigNode::FChaosClothAssetSimulationBackstopConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&BackstopDistance.WeightMap);
	RegisterInputConnection(&BackstopRadius.WeightMap);
}

void FChaosClothAssetSimulationBackstopConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this, &BackstopDistance);
	PropertyHelper.SetPropertyWeighted(this, &BackstopRadius);
}
