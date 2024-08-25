// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationLongRangeAttachmentConfigNode.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationLongRangeAttachmentConfigNode)

FChaosClothAssetSimulationLongRangeAttachmentConfigNode::FChaosClothAssetSimulationLongRangeAttachmentConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&FixedEndWeightMap);
	RegisterInputConnection(&TetherStiffness.WeightMap);
	RegisterInputConnection(&TetherScale.WeightMap);
}

void FChaosClothAssetSimulationLongRangeAttachmentConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this, &TetherStiffness);
	PropertyHelper.SetPropertyWeighted(this, &TetherScale);
	PropertyHelper.SetPropertyBool(this, &bUseGeodesicTethers);
	PropertyHelper.SetPropertyString(this, &FixedEndWeightMap);
}

void FChaosClothAssetSimulationLongRangeAttachmentConfigNode::EvaluateClothCollection(Dataflow::FContext& Context, const TSharedRef<FManagedArrayCollection>& ClothCollection) const
{
	const FString InFixedEndWeightMapString = GetValue<FString>(Context, &FixedEndWeightMap);
	const FName InFixedEndWeightMap(InFixedEndWeightMapString);
	UE::Chaos::ClothAsset::FClothEngineTools::GenerateTethers(ClothCollection, InFixedEndWeightMap, bUseGeodesicTethers);
}
