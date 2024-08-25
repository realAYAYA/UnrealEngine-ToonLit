// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/WeightedValue.h"
#include "ChaosClothAsset/ClothDataflowTools.h"

namespace UE::Chaos::ClothAsset
{
	void FWeightMapTools::MakeWeightMapName(FString& InOutString)
	{
		FClothDataflowTools::MakeCollectionName(InOutString);
	}
}  // End namespace UE::Chaos::ClothAsset
