// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ClothCollection.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth pattern const adapter object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothPatternConstAdapter
	{
	public:
		FClothPatternConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection, int32 InLodIndex, int32 InPatternIndex);
		virtual ~FClothPatternConstAdapter() = default;

		FClothPatternConstAdapter(const FClothPatternConstAdapter& Other) : ClothCollection(Other.ClothCollection), LodIndex(Other.LodIndex), PatternIndex(Other.PatternIndex) {}
		FClothPatternConstAdapter(FClothPatternConstAdapter&& Other) : ClothCollection(MoveTemp(Other.ClothCollection)), LodIndex(Other.LodIndex), PatternIndex(Other.PatternIndex) {}
		FClothPatternConstAdapter& operator=(const FClothPatternConstAdapter& Other) { ClothCollection = Other.ClothCollection; LodIndex = Other.LodIndex; PatternIndex = Other.PatternIndex; return *this; }
		FClothPatternConstAdapter& operator=(FClothPatternConstAdapter&& Other) { ClothCollection = MoveTemp(Other.ClothCollection); LodIndex = Other.LodIndex; PatternIndex = Other.PatternIndex; return *this; }

		// Sim Vertices Group
		// Note: Use the FClothLodConstAdapter accessors instead of these for the array indices to match the SimIndices values
		int32 GetNumSimVertices() const { return GetClothCollection()->GetNumElements(GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector2f> GetSimPosition() const { return GetClothCollection()->GetElements(GetClothCollection()->SimPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetSimRestPosition() const { return GetClothCollection()->GetElements(GetClothCollection()->SimRestPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetSimRestNormal() const { return GetClothCollection()->GetElements(GetClothCollection()->SimRestNormal, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetSimNumBoneInfluences() const { return GetClothCollection()->GetElements(GetClothCollection()->SimNumBoneInfluences, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<int32>> GetSimBoneIndices() const { return GetClothCollection()->GetElements(GetClothCollection()->SimBoneIndices, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<float>> GetSimBoneWeights() const { return GetClothCollection()->GetElements(GetClothCollection()->SimBoneWeights, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }

		// Sim Faces Group, note: SimIndices points to the LOD arrays, not the pattern arrays
		int32 GetNumSimFaces() const { return GetClothCollection()->GetNumElements(GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }
		TConstArrayView<FIntVector3> GetSimIndices() const { return GetClothCollection()->GetElements(GetClothCollection()->SimIndices, GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }

		// Render Vertices Group
		// Note: Use the FClothLodConstAdapter accessors instead of these for the array indices to match the RenderIndices values
		int32 GetNumRenderVertices() const { return ClothCollection->GetNumElements(GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetRenderPosition() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderPosition, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetRenderNormal() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderNormal, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetRenderTangentU() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderTangentU, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetRenderTangentV() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderTangentV, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<FVector2f>> GetRenderUVs() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderUVs, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FLinearColor> GetRenderColor() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderColor, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderNumBoneInfluences() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderNumBoneInfluences, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<int32>> GetRenderBoneIndices() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderBoneIndices, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<float>> GetRenderBoneWeights() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderBoneWeights, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }

		// Render Faces Group, note: RenderIndices points to the LOD arrays, not the pattern arrays
		int32 GetNumRenderFaces() const { return GetClothCollection()->GetNumElements(GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TConstArrayView<FIntVector3> GetRenderIndices() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderIndices, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderMaterialIndex() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderMaterialIndex, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }

		// Wrap Deformers Group
		int32 GetNumWrapDeformers() const { return GetClothCollection()->GetNumElements(GetClothCollection()->WrapDeformerStart, GetClothCollection()->WrapDeformerEnd, GetElementIndex()); }

		/** Return the element index within the cloth collection. */
		int32 GetElementIndex() const { return ClothCollection->PatternStart[LodIndex] + PatternIndex; }

		/** Return the LOD index this adapter has been created with. */
		int32 GetLodIndex() const { return LodIndex; }

		/** Return the Pattern index this adapter has been created with. */
		int32 GetPatternIndex() const { return PatternIndex; }

		/**
		 * Return a const view on the specified vertex weightmap.
		 * The attribute must exist otherwise the function will assert.
		 * @see FClothAdapter::AddWeightMap, GetNumSimVertices.
		 */
		TConstArrayView<float> GetWeightMap(const FName& Name) const;

		/** Return the underlaying cloth collection this adapter has been created with. */
		const TSharedPtr<const FClothCollection>& GetClothCollection() const { return ClothCollection; }

	private:
		TSharedPtr<const FClothCollection> ClothCollection;
		int32 LodIndex;
		int32 PatternIndex;
	};

	/**
	 * Cloth pattern adapter object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothPatternAdapter final : public FClothPatternConstAdapter
	{
	public:
		FClothPatternAdapter(const TSharedPtr<FClothCollection>& InClothCollection, int32 InLodIndex, int32 InPatternIndex);
		virtual ~FClothPatternAdapter() override = default;

		FClothPatternAdapter(const FClothPatternAdapter& Other) : FClothPatternConstAdapter(Other) {}
		FClothPatternAdapter(FClothPatternAdapter&& Other) : FClothPatternConstAdapter(MoveTemp(Other)) {}
		FClothPatternAdapter& operator=(const FClothPatternAdapter& Other) { FClothPatternConstAdapter::operator=(Other); return *this; }
		FClothPatternAdapter& operator=(FClothPatternAdapter&& Other) { FClothPatternConstAdapter::operator=(MoveTemp(Other)); return *this; }

		/** Remove all geometry from this cloth Pattern. */
		void Reset();

		/** Grow or shrink the space reserved for simulation vertices for this pattern within the cloth collection and return its start index. */
		int32 SetNumSimVertices(int32 NumSimVertices);

		/** Grow or shrink the space reserved for simulation faces for this pattern within the cloth collection and return its start index. */
		int32 SetNumSimFaces(int32 InNumSimFaces);

		/** Grow or shrink the space reserved for render vertices for this pattern within the cloth collection and return its start index. */
		int32 SetNumRenderVertices(int32 NumRenderVertices);

		/** Grow or shrink the space reserved for render faces for this pattern within the cloth collection and return its start index. */
		int32 SetNumRenderFaces(int32 InNumRenderFaces);

		/** Initialize the cloth pattern using the specified 3D and 2D positions, and topology. */
		void Initialize(const TArray<FVector2f>& Positions, const TArray<FVector3f>& RestPositions, const TArray<uint32>& Indices);

		// Sim Vertices Group
		// Note: Use the FClothLodAdapter accessors instead of these for the array indices to match the SimIndices values
		TArrayView<FVector2f> GetSimPosition() { return GetClothCollection()->GetElements(GetClothCollection()->SimPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetSimRestPosition() { return GetClothCollection()->GetElements(GetClothCollection()->SimRestPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetSimRestNormal() { return GetClothCollection()->GetElements(GetClothCollection()->SimRestNormal, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<int32> GetSimNumBoneInfluences() { return GetClothCollection()->GetElements(GetClothCollection()->SimNumBoneInfluences, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<int32>> GetSimBoneIndices() { return GetClothCollection()->GetElements(GetClothCollection()->SimBoneIndices, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<float>> GetSimBoneWeights() { return GetClothCollection()->GetElements(GetClothCollection()->SimBoneWeights, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }

		// Sim Faces Group, note: SimIndices points to the LOD arrays, not the pattern arrays
		TArrayView<FIntVector3> GetSimIndices() { return GetClothCollection()->GetElements(GetClothCollection()->SimIndices, GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }

		// Render Vertices Group
		// Note: Use the FClothLodAdapter accessors instead of these for the array indices to match the RenderIndices values
		TArrayView<FVector3f> GetRenderPosition() { return GetClothCollection()->GetElements(GetClothCollection()->RenderPosition, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetRenderNormal() { return GetClothCollection()->GetElements(GetClothCollection()->RenderNormal, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetRenderTangentU() { return GetClothCollection()->GetElements(GetClothCollection()->RenderTangentU, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetRenderTangentV() { return GetClothCollection()->GetElements(GetClothCollection()->RenderTangentV, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<FVector2f>> GetRenderUVs() { return GetClothCollection()->GetElements(GetClothCollection()->RenderUVs, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FLinearColor> GetRenderColor() { return GetClothCollection()->GetElements(GetClothCollection()->RenderColor, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderNumBoneInfluences() { return GetClothCollection()->GetElements(GetClothCollection()->RenderNumBoneInfluences, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<int32>> GetRenderBoneIndices() { return GetClothCollection()->GetElements(GetClothCollection()->RenderBoneIndices, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<float>> GetRenderBoneWeights() { return GetClothCollection()->GetElements(GetClothCollection()->RenderBoneWeights, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }

		// Render Faces Group, note: RenderIndices points to the LOD arrays, not the pattern arrays
		TArrayView<FIntVector3> GetRenderIndices() { return GetClothCollection()->GetElements(GetClothCollection()->RenderIndices, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderMaterialIndex() { return GetClothCollection()->GetElements(GetClothCollection()->RenderMaterialIndex, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }

		/**
		 * Return a view on the specified vertex weightmap.
		 * The attribute must exist otherwise the function will assert.
		 * @see FClothAdapter::AddWeightMap, GetNumSimVertices.
		 */
		TArrayView<float> GetWeightMap(const FName& Name);

		/** Return the underlaying Cloth Collection this adapter has been created with. */
		TSharedPtr<FClothCollection> GetClothCollection() { return ConstCastSharedPtr<FClothCollection>(FClothPatternConstAdapter::GetClothCollection()); }

	private:
		friend class FClothLodAdapter;

		void SetDefaults();
	};
}  // End namespace UE::Chaos::ClothAsset
