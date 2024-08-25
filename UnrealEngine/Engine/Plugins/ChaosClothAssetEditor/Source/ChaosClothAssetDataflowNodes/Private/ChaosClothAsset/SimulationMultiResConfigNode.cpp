// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMultiResConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMultiResConfigNode)

FChaosClothAssetSimulationMultiResConfigNode::FChaosClothAssetSimulationMultiResConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&MultiResStiffness.WeightMap);
	RegisterInputConnection(&MultiResVelocityTargetStiffness.WeightMap);
}

void FChaosClothAssetSimulationMultiResConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	if (bIsFineLOD)
	{
		PropertyHelper.SetProperty(this, &MultiResCoarseLODIndex);
		PropertyHelper.SetPropertyBool(this, &bMultiResUseXPBD);
		PropertyHelper.SetPropertyWeighted(this, &MultiResStiffness);
		PropertyHelper.SetPropertyWeighted(this, &MultiResVelocityTargetStiffness);
	}
	if (bIsCoarseMultiResLOD)
	{
		PropertyHelper.SetPropertyBool(this, &bIsCoarseMultiResLOD);
	}
}

