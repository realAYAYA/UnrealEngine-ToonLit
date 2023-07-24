// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothLodAdapter.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Const cloth adapter object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothConstAdapter
	{
	public:
		FClothConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection);
		FClothConstAdapter(const FClothLodConstAdapter& ClothLodConstAdapter);
		FClothConstAdapter(const FClothPatternConstAdapter& ClothPatternConstAdapter);
		virtual ~FClothConstAdapter() = default;

		FClothConstAdapter(const FClothConstAdapter& Other) : ClothCollection(Other.ClothCollection) {}
		FClothConstAdapter(FClothConstAdapter&& Other) : ClothCollection(MoveTemp(Other.ClothCollection)) {}
		FClothConstAdapter& operator=(const FClothConstAdapter& Other) { ClothCollection = Other.ClothCollection; return *this; }
		FClothConstAdapter& operator=(FClothConstAdapter&& Other) { ClothCollection = MoveTemp(Other.ClothCollection); return *this; }

		/** Return the specified LOD. */
		FClothLodConstAdapter GetLod(int32 LodIndex) const;

		/** Return the number of LODs contained in this Cloth. */
		int32 GetNumLods() const { return ClothCollection->NumElements(FClothCollection::LodsGroup); }

		/** Return the underlaying cloth collection this adapter has been created with. */
		const TSharedPtr<const FClothCollection>& GetClothCollection() const { return ClothCollection; }

	private:
		TSharedPtr<const FClothCollection> ClothCollection;
	};

	/**
	 * Cloth adapter object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothAdapter final : public FClothConstAdapter
	{
	public:
		FClothAdapter(const TSharedPtr<FClothCollection>& InClothCollection);
		virtual ~FClothAdapter() override = default;

		FClothAdapter(const FClothAdapter& Other) : FClothConstAdapter(Other) {}
		FClothAdapter(FClothAdapter&& Other) : FClothConstAdapter(MoveTemp(Other)) {}
		FClothAdapter& operator=(const FClothAdapter& Other) { FClothConstAdapter::operator=(Other); return *this; }
		FClothAdapter& operator=(FClothAdapter&& Other) { FClothConstAdapter::operator=(MoveTemp(Other)); return *this; }

		/** Add a new LOD to this cloth. */
		int32 AddLod();

		/** Return the specified LOD. */
		FClothLodAdapter GetLod(int32 LodIndex);

		/** Add a new LOD to this cloth, and return the cloth LOD adapter set to its index. */
		FClothLodAdapter AddGetLod() { return GetLod(AddLod()); }

		/** Remove all LODs from this cloth. */
		void Reset();

		/** Add a new weight map to this cloth. Access is then done per pattern. */
		void AddWeightMap(const FName& Name) { GetClothCollection()->AddAttribute<float>(Name, FClothCollection::SimVerticesGroup); }

		/** Remove a weight map from this cloth. */
		void RemoveWeightMap(const FName& Name) { GetClothCollection()->RemoveAttribute(Name, FClothCollection::SimVerticesGroup); }

		/** Return the underlaying cloth collection this adapter has been created with. */
		TSharedPtr<FClothCollection> GetClothCollection() { return ConstCastSharedPtr<FClothCollection>(FClothConstAdapter::GetClothCollection()); }
	};
}  // End namespace UE::Chaos::ClothAsset
