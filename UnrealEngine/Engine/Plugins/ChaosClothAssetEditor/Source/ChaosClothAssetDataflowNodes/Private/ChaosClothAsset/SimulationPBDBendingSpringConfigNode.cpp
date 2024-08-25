// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPBDBendingSpringConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPBDBendingSpringConfigNode)

FChaosClothAssetSimulationPBDBendingSpringConfigNode::FChaosClothAssetSimulationPBDBendingSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&BendingSpringStiffness.WeightMap);
}

void FChaosClothAssetSimulationPBDBendingSpringConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this,
		&BendingSpringStiffness,
		{
			FName(TEXT("BendingElementStiffness")),       // Existing properties to warn against
			FName(TEXT("XPBDBendingSpringStiffness")),    //
			FName(TEXT("XPBDBendingElementStiffness")),   //
			FName(TEXT("XPBDAnisoBendingStiffnessWarp"))  //
		});
}
