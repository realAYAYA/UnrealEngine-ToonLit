// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothPatternAdapter.h"
#include "Modules/ModuleManager.h"

namespace UE::Chaos::ClothAsset
{
	FClothPatternConstAdapter::FClothPatternConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection, int32 InLodIndex, int32 InPatternIndex)
		: ClothCollection(InClothCollection)
		, LodIndex(InLodIndex)
		, PatternIndex(InPatternIndex)
	{
		check(ClothCollection.IsValid());
		check(LodIndex >= 0 && LodIndex < ClothCollection->NumElements(FClothCollection::LodsGroup));
		check(PatternIndex >= 0 && PatternIndex < ClothCollection->NumElements(FClothCollection::PatternsGroup));
	}

	TConstArrayView<float> FClothPatternConstAdapter::GetWeightMap(const FName& Name) const
	{
		const int32 ElementIndex = GetElementIndex();
		const int32 SimVerticesStart = GetClothCollection()->SimVerticesStart[ElementIndex];
		if (SimVerticesStart != INDEX_NONE)
		{
			const int32 SimVerticesEnd = GetClothCollection()->SimVerticesEnd[ElementIndex];
			check(SimVerticesEnd != INDEX_NONE);

			const TManagedArray<float>& ManagedArray = GetClothCollection()->GetAttribute<float>(Name, FClothCollection::SimVerticesGroup);

			return TConstArrayView<float>(ManagedArray.GetData() + SimVerticesStart, SimVerticesEnd - SimVerticesStart + 1);
		}

		return TConstArrayView<float>();
	}

	FClothPatternAdapter::FClothPatternAdapter(const TSharedPtr<FClothCollection>& InClothCollection, int32 InLodIndex, int32 InPatternIndex)
		: FClothPatternConstAdapter(InClothCollection, InLodIndex, InPatternIndex)
	{
	}

	void FClothPatternAdapter::Reset()
	{
		TArray<int32> Array;
		const TArrayView<int32>& ArrayView = static_cast<const TArrayView<int32>&>(TArrayView<int32>(Array));

		const int32 ElementIndex = GetElementIndex();

		GetClothCollection()->RemoveElements(FClothCollection::SimVerticesGroup, GetNumSimVertices(), GetClothCollection()->SimVerticesStart[ElementIndex]);
		GetClothCollection()->RemoveElements(FClothCollection::SimFacesGroup, GetNumSimFaces(), GetClothCollection()->SimFacesStart[ElementIndex]);
		GetClothCollection()->RemoveElements(FClothCollection::RenderVerticesGroup, GetNumRenderVertices(), GetClothCollection()->RenderVerticesStart[ElementIndex]);
		GetClothCollection()->RemoveElements(FClothCollection::RenderFacesGroup, GetNumRenderFaces(), GetClothCollection()->WrapDeformerStart[ElementIndex]);

		SetDefaults();
	}

	int32 FClothPatternAdapter::SetNumSimVertices(int32 InNumSimVertices)
	{
		return GetClothCollection()->SetNumElements(
			InNumSimVertices,
			FClothCollection::SimVerticesGroup,
			GetClothCollection()->SimVerticesStart,
			GetClothCollection()->SimVerticesEnd,
			GetElementIndex());
	}

	int32 FClothPatternAdapter::SetNumSimFaces(int32 InNumSimFaces)
	{
		return GetClothCollection()->SetNumElements(
			InNumSimFaces,
			FClothCollection::SimFacesGroup,
			GetClothCollection()->SimFacesStart,
			GetClothCollection()->SimFacesEnd,
			GetElementIndex());
	}

	int32 FClothPatternAdapter::SetNumRenderVertices(int32 InNumRenderVertices)
	{
		return GetClothCollection()->SetNumElements(
			InNumRenderVertices,
			FClothCollection::RenderVerticesGroup,
			GetClothCollection()->RenderVerticesStart,
			GetClothCollection()->RenderVerticesEnd,
			GetElementIndex());
	}

	int32 FClothPatternAdapter::SetNumRenderFaces(int32 InNumRenderFaces)
	{
		return GetClothCollection()->SetNumElements(
			InNumRenderFaces,
			FClothCollection::RenderFacesGroup,
			GetClothCollection()->RenderFacesStart,
			GetClothCollection()->RenderFacesEnd,
			GetElementIndex());
	}

	void FClothPatternAdapter::SetDefaults()
	{
		const int32 ElementIndex = GetElementIndex();

		GetClothCollection()->SimVerticesStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->SimVerticesEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->SimFacesStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->SimFacesEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->RenderVerticesStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->RenderVerticesEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->RenderFacesStart[ElementIndex] = INDEX_NONE;
		GetClothCollection()->RenderFacesEnd[ElementIndex] = INDEX_NONE;
		GetClothCollection()->NumWeights[ElementIndex] = 0;
		GetClothCollection()->StatusFlags[ElementIndex] = 0;
		GetClothCollection()->SimMaterialIndex[ElementIndex] = 0;
	}

	void FClothPatternAdapter::Initialize(const TArray<FVector2f>& Positions, const TArray<FVector3f>& RestPositions, const TArray<uint32>& Indices)
	{
		const int32 NumSimVertices = Positions.Num();
		check(NumSimVertices == RestPositions.Num());

		const int32 SimVerticesStart = SetNumSimVertices(NumSimVertices);

		for (int32 Index = 0; Index < NumSimVertices; ++Index)
		{
			const int32 CollectionVertexIndex = SimVerticesStart + Index;
			GetClothCollection()->SimPosition[CollectionVertexIndex] = Positions[Index];
			GetClothCollection()->SimRestPosition[CollectionVertexIndex] = RestPositions[Index];
			GetClothCollection()->SimRestNormal[CollectionVertexIndex] = FVector3f::ZeroVector;
		}

		const int32 NumSimFaces = (uint32)Indices.Num() / 3;
		check(NumSimFaces * 3 == Indices.Num());

		const int32 SimFacesStart = SetNumSimFaces(NumSimFaces);

		// Face indices always index from the first vertex of the LOD, but these indices are indexed from the start of the pattern and need to be offset
		const int32 LodSimVerticesStart = GetClothCollection()->SimVerticesStart[GetClothCollection()->PatternStart[GetLodIndex()]];
		const int32 Offset = SimVerticesStart - LodSimVerticesStart;

		for (int32 Index = 0; Index < NumSimFaces; ++Index)
		{
			// Indices from the start of the collection
			const int32 CollectionVertexIndex0 = SimVerticesStart + Indices[Index * 3];
			const int32 CollectionVertexIndex1 = SimVerticesStart + Indices[Index * 3 + 1];
			const int32 CollectionVertexIndex2 = SimVerticesStart + Indices[Index * 3 + 2];

			// Indices from the start of the LOD
			const int32 LodVertexIndex0 = CollectionVertexIndex0 - LodSimVerticesStart;
			const int32 LodVertexIndex1 = CollectionVertexIndex1 - LodSimVerticesStart;
			const int32 LodVertexIndex2 = CollectionVertexIndex2 - LodSimVerticesStart;

			GetClothCollection()->SimIndices[SimFacesStart + Index] = FIntVector3(LodVertexIndex0, LodVertexIndex1, LodVertexIndex2);

			// Calculate face normal contribution
			const FVector3f& Pos0 = GetClothCollection()->SimRestPosition[CollectionVertexIndex0];
			const FVector3f& Pos1 = GetClothCollection()->SimRestPosition[CollectionVertexIndex1];
			const FVector3f& Pos2 = GetClothCollection()->SimRestPosition[CollectionVertexIndex2];
			const FVector3f Normal = (Pos1 - Pos0).Cross(Pos2 - Pos0).GetSafeNormal();
			GetClothCollection()->SimRestNormal[CollectionVertexIndex0] += Normal;
			GetClothCollection()->SimRestNormal[CollectionVertexIndex1] += Normal;
			GetClothCollection()->SimRestNormal[CollectionVertexIndex2] += Normal;
		}

		// Normalize normals
		for (int32 Index = 0; Index < NumSimVertices; ++Index)
		{
			const int32 CollectionVertexIndex = SimVerticesStart + Index;
			GetClothCollection()->SimRestNormal[CollectionVertexIndex] =
				GetClothCollection()->SimRestNormal[CollectionVertexIndex].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
		}
	}

	TArrayView<float> FClothPatternAdapter::GetWeightMap(const FName& Name)
	{
		const int32 ElementIndex = GetElementIndex();
		const int32 SimVerticesStart = GetClothCollection()->SimVerticesStart[ElementIndex];
		if (SimVerticesStart != INDEX_NONE)
		{
			const int32 SimVerticesEnd = GetClothCollection()->SimVerticesEnd[ElementIndex];
			check(SimVerticesEnd != INDEX_NONE);

			TManagedArray<float>& ManagedArray = GetClothCollection()->ModifyAttribute<float>(Name, FClothCollection::SimVerticesGroup);

			return TArrayView<float>(ManagedArray.GetData() + SimVerticesStart, SimVerticesEnd - SimVerticesStart + 1);
		}
		return TArrayView<float>();
	}
}  // End namespace UE::Chaos::ClothAsset
