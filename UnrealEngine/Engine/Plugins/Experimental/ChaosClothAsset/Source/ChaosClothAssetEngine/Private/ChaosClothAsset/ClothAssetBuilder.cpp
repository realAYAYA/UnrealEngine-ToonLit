// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAssetBuilder.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothSimulationModel.h"

int32 UClothAssetBuilder::GetNumVertices(const UChaosClothAsset& ClothAsset, int32 LodIndex)
{
	const TSharedPtr<const FChaosClothSimulationModel> ClothSimulationModel = ClothAsset.GetClothSimulationModel();
	return ClothSimulationModel && ClothSimulationModel->IsValidLodIndex(LodIndex) ?
		ClothSimulationModel->GetNumVertices(LodIndex) :
		0;
}

TConstArrayView<FVector3f> UClothAssetBuilder::GetSimPositions(const UChaosClothAsset& ClothAsset, int32 LodIndex)
{
	const TSharedPtr<const FChaosClothSimulationModel> ClothSimulationModel = ClothAsset.GetClothSimulationModel();
	return ClothSimulationModel && ClothSimulationModel->IsValidLodIndex(LodIndex) ?
		ClothSimulationModel->GetPositions(LodIndex) :
		TConstArrayView<FVector3f>();
}

TConstArrayView<uint32> UClothAssetBuilder::GetSimIndices(const UChaosClothAsset& ClothAsset, int32 LodIndex)
{
	const TSharedPtr<const FChaosClothSimulationModel> ClothSimulationModel = ClothAsset.GetClothSimulationModel();
	return ClothSimulationModel && ClothSimulationModel->IsValidLodIndex(LodIndex) ?
		ClothSimulationModel->GetIndices(LodIndex) :
		TConstArrayView<uint32>();

}
