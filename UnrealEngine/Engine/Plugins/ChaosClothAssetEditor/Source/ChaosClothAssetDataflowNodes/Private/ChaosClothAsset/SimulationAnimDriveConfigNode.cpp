// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationAnimDriveConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationAnimDriveConfigNode)

FChaosClothAssetSimulationAnimDriveConfigNode::FChaosClothAssetSimulationAnimDriveConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&AnimDriveStiffness.WeightMap);
	RegisterInputConnection(&AnimDriveDamping.WeightMap);
}

void FChaosClothAssetSimulationAnimDriveConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this, &AnimDriveStiffness);
	PropertyHelper.SetPropertyWeighted(this, &AnimDriveDamping);
}
