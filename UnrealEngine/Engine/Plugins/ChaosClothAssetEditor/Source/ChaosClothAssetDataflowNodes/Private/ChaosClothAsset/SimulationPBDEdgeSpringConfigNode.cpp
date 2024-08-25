// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPBDEdgeSpringConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPBDEdgeSpringConfigNode)

FChaosClothAssetSimulationPBDEdgeSpringConfigNode::FChaosClothAssetSimulationPBDEdgeSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&EdgeSpringStiffness.WeightMap);
}

void FChaosClothAssetSimulationPBDEdgeSpringConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this,
		&EdgeSpringStiffness,
		{
			FName(TEXT("XPBDEdgeSpringStiffness")),       // Existing properties to warn against
			FName(TEXT("XPBDAnisoStretchStiffnessWarp"))  //
		});
}
