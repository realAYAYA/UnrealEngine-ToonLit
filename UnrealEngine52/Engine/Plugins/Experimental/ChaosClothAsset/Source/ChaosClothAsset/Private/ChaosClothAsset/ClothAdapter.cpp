// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAdapter.h"

namespace UE::Chaos::ClothAsset
{
	FClothConstAdapter::FClothConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection)
		: ClothCollection(InClothCollection)
	{
		check(ClothCollection.IsValid());
	}

	FClothConstAdapter::FClothConstAdapter(const FClothLodConstAdapter& ClothLodConstAdapter)
		: ClothCollection(ClothLodConstAdapter.GetClothCollection())
	{
	}

	FClothConstAdapter::FClothConstAdapter(const FClothPatternConstAdapter& ClothPatternConstAdapter)
		: ClothCollection(ClothPatternConstAdapter.GetClothCollection())
	{
	}

	FClothLodConstAdapter FClothConstAdapter::GetLod(int32 LodIndex) const
	{
		return FClothLodConstAdapter(ClothCollection, LodIndex);
	}

	FClothAdapter::FClothAdapter(const TSharedPtr<FClothCollection>& InClothCollection)
		: FClothConstAdapter(InClothCollection)
	{
	}

	int32 FClothAdapter::AddLod()
	{
		const int32 LodIndex = GetClothCollection()->AddElements(1, FClothCollection::LodsGroup);

		FClothLodAdapter(GetClothCollection(), LodIndex).SetDefaults();

		return LodIndex;
	}

	FClothLodAdapter FClothAdapter::GetLod(int32 LodIndex)
	{
		return FClothLodAdapter(GetClothCollection(), LodIndex);
	}

	void FClothAdapter::Reset()
	{
		const int32 NumLods = GetNumLods();
		for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
		{
			GetLod(LodIndex).Reset();
		}
		GetClothCollection()->EmptyGroup(FClothCollection::LodsGroup);
	}
}  // End namespace UE::Chaos::ClothAsset
