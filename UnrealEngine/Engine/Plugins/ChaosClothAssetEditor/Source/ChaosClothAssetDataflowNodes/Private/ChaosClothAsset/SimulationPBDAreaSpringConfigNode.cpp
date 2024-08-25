// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPBDAreaSpringConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPBDAreaSpringConfigNode)

FChaosClothAssetSimulationPBDAreaSpringConfigNode::FChaosClothAssetSimulationPBDAreaSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&AreaSpringStiffness.WeightMap);
}

void FChaosClothAssetSimulationPBDAreaSpringConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this,
		&AreaSpringStiffness,
		{
			FName(TEXT("XPBDAreaSpringStiffness"))  // Existing properties to warn against
		});
}
