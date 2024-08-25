// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationDampingConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationDampingConfigNode)

FChaosClothAssetSimulationDampingConfigNode::FChaosClothAssetSimulationDampingConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&DampingCoefficientWeighted.WeightMap);
}

void FChaosClothAssetSimulationDampingConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(TEXT("DampingCoefficient"), DampingCoefficientWeighted);
	PropertyHelper.SetProperty(this, &LocalDampingCoefficient);
}

void FChaosClothAssetSimulationDampingConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (DampingCoefficient_DEPRECATED != DeprecatedDampingCoefficientValue)
	{
		DampingCoefficientWeighted.Low = DampingCoefficientWeighted.High = DampingCoefficient_DEPRECATED;
		DampingCoefficient_DEPRECATED = DeprecatedDampingCoefficientValue;
	}
}
