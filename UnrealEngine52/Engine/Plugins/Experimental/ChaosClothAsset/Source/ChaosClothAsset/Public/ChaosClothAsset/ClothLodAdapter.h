// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothPatternAdapter.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth LOD adapter const object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothLodConstAdapter
	{
	public:
		FClothLodConstAdapter(const TSharedPtr<const FClothCollection>& InClothCollection, int32 InLodIndex);
		FClothLodConstAdapter(const FClothPatternConstAdapter& ClothPatternConstAdapter);

		virtual ~FClothLodConstAdapter() = default;

		FClothLodConstAdapter(const FClothLodConstAdapter& Other) : ClothCollection(Other.ClothCollection), LodIndex(Other.LodIndex) {}
		FClothLodConstAdapter(FClothLodConstAdapter&& Other) : ClothCollection(MoveTemp(Other.ClothCollection)), LodIndex(Other.LodIndex) {}
		FClothLodConstAdapter& operator=(const FClothLodConstAdapter& Other) { ClothCollection = Other.ClothCollection; LodIndex = Other.LodIndex; return *this; }
		FClothLodConstAdapter& operator=(FClothLodConstAdapter&& Other) { ClothCollection = MoveTemp(Other.ClothCollection); LodIndex = Other.LodIndex; return *this; }

		FClothPatternConstAdapter GetPattern(int32  PatternIndex) const;

		// Patterns Group
		int32 GetNumPatterns() const { return GetClothCollection()->GetNumElements(GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetSimVerticesStart() const { return GetClothCollection()->GetElements(GetClothCollection()->SimVerticesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetSimVerticesEnd() const { return GetClothCollection()->GetElements(GetClothCollection()->SimVerticesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetSimFacesStart() const { return GetClothCollection()->GetElements(GetClothCollection()->SimFacesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetSimFacesEnd() const { return GetClothCollection()->GetElements(GetClothCollection()->SimFacesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderVerticesStart() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderVerticesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderVerticesEnd() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderVerticesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderFacesStart() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderFacesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TConstArrayView<int32> GetRenderFacesEnd() const { return GetClothCollection()->GetElements(GetClothCollection()->RenderFacesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }

		// Seams Group
		int32 GetNumSeams() const { return GetClothCollection()->GetNumElements(GetClothCollection()->SeamStart, GetClothCollection()->SeamEnd, GetElementIndex()); }
		TConstArrayView<FIntVector2> GetSeamPatterns() const { return GetClothCollection()->GetElements(GetClothCollection()->SeamPatterns, GetClothCollection()->SeamStart, GetClothCollection()->SeamEnd, GetElementIndex()); }
		TConstArrayView<TArray<FIntVector2>> GetSeamStitches() const { return GetClothCollection()->GetElements(GetClothCollection()->SeamStitches, GetClothCollection()->SeamStart, GetClothCollection()->SeamEnd, GetElementIndex()); }

		// Tethers Group
		int32 GetNumTetherBatches() const { return GetClothCollection()->GetNumElements(GetClothCollection()->TetherBatchStart, GetClothCollection()->TetherBatchEnd, GetElementIndex()); }
		// TODO: Tether API 

		// All patterns Sim Vertices Group, use these to access all pattern at once, and when using the SimIndices
		int32 GetPatternsSimVerticesStart() const { return GetClothCollection()->GetPatternsElementsStartEnd<true, false>(GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()).Get<0>(); }
		int32 GetPatternsSimVerticesEnd() const { return GetClothCollection()->GetPatternsElementsStartEnd<false, true>(GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()).Get<1>(); }
		TTuple<int32, int32> GetPatternsSimVerticesRange() const { return GetClothCollection()->GetPatternsElementsStartEnd<true, true>(GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		int32 GetPatternsNumSimVertices() const { return GetClothCollection()->GetPatternsNumElements(GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }

		TConstArrayView<FVector2f> GetPatternsSimPosition() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetPatternsSimRestPosition() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimRestPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetPatternsSimRestNormal() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimRestNormal, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetPatternsSimNumBoneInfluences() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimNumBoneInfluences, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<int32>> GetPatternsSimBoneIndices() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimBoneIndices, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<float>> GetPatternsSimBoneWeights() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimBoneWeights, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }

		// All patterns Sim Faces Group, use these to access all pattern at once
		TTuple<int32, int32> GetPatternsSimFacesRange() const { return GetClothCollection()->GetPatternsElementsStartEnd<true, true>(GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }
		int32 GetPatternsNumSimFaces() const { return GetClothCollection()->GetPatternsNumElements(GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }
		TConstArrayView<FIntVector3> GetPatternsSimIndices() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimIndices, GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }

		// All patterns Render Vertices Group, use these to access all pattern at once, and when using the RenderIndices
		int32 GetPatternsRenderVerticesStart() const { return GetClothCollection()->GetPatternsElementsStartEnd<true, false>(GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()).Get<0>(); }
		int32 GetPatternsRenderVerticesEnd() const { return GetClothCollection()->GetPatternsElementsStartEnd<false, true>(GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()).Get<1>(); }
		TTuple<int32, int32> GetPatternsRenderVerticesRange() const { return GetClothCollection()->GetPatternsElementsStartEnd(GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		int32 GetPatternsNumRenderVertices() const { return GetClothCollection()->GetPatternsNumElements(GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }

		TConstArrayView<FVector3f> GetPatternsRenderPosition() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderPosition, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetPatternsRenderNormal() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderNormal, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetPatternsRenderTangentU() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderTangentU, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FVector3f> GetPatternsRenderTangentV() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderTangentV, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<FVector2f>> GePatternstRenderUVs() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderUVs, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<FLinearColor> GetPatternsRenderColor() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderColor, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetPatternsRenderNumBoneInfluences() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderNumBoneInfluences, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<int32>> GetPatternsRenderBoneIndices() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderBoneIndices, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TConstArrayView<TArray<float>> GetPatternsRenderBoneWeights() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderBoneWeights, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }

		// All patterns Sim Faces Group, use these to access all pattern at once
		TTuple<int32, int32> GetPatternsRenderFacesRange() const { return GetClothCollection()->GetPatternsElementsStartEnd<true, true>(GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		int32 GetPatternsNumRenderFaces() const { return GetClothCollection()->GetPatternsNumElements(GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TConstArrayView<FIntVector3> GetPatternsRenderIndices() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderIndices, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TConstArrayView<int32> GetPatternsRenderMaterialIndex() const { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderMaterialIndex, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }

		/** Return the element index within the cloth collection. */
		int32 GetElementIndex() const { return LodIndex; }

		/** Return the LOD index this adapter has been created with. */
		int32 GetLodIndex() const { return LodIndex; }

		/** Return the underlaying cloth collection this adapter has been created with. */
		const TSharedPtr<const FClothCollection>& GetClothCollection() const { return ClothCollection; }

		/** Return the welded simulation mesh for this LOD. */
		void BuildSimulationMesh(TArray<FVector3f>& Positions, TArray<FVector3f>& Normals, TArray<uint32>& Indices) const;

	private:
		TSharedPtr<const FClothCollection> ClothCollection;
		int32 LodIndex;
	};

	/**
	 * Cloth LOD adapter object to provide a more convenient object oriented access to the cloth collection.
	 */
	class CHAOSCLOTHASSET_API FClothLodAdapter final : public FClothLodConstAdapter
	{
	public:
		FClothLodAdapter(const TSharedPtr<FClothCollection>& InClothCollection, int32 InLodIndex);
		virtual ~FClothLodAdapter() override = default;

		FClothLodAdapter(const FClothLodAdapter& Other) : FClothLodConstAdapter(Other) {}
		FClothLodAdapter(FClothLodAdapter&& Other) : FClothLodConstAdapter(MoveTemp(Other)) {}
		FClothLodAdapter& operator=(const FClothLodAdapter& Other) { FClothLodConstAdapter::operator=(Other); return *this; }
		FClothLodAdapter& operator=(FClothLodAdapter&& Other) { FClothLodConstAdapter::operator=(MoveTemp(Other)); return *this; }

		/** Add a new pattern to this cloth LOD. */
		int32 AddPattern();

		/** Return the specified pattern. */
		FClothPatternAdapter GetPattern(int32 PatternIndex);

		/** Add a new pattern to this cloth LOD, and return the cloth pattern adapter set to its index. */
		FClothPatternAdapter AddGetPattern() { return GetPattern(AddPattern()); }

		/** Remove all cloth patterns from this cloth LOD. */
		void Reset();

		/** Initialize the cloth LOD using the specified 3D triangle mesh and unwrapping it into a 2D geometry for generating the patterns. */
		void Initialize(const TArray<FVector3f>& Positions, const TArray<uint32>& Indices);

		/** Return the underlaying Cloth Collection this adapter has been created with. */
		TSharedPtr<FClothCollection> GetClothCollection() { return ConstCastSharedPtr<FClothCollection>(FClothLodConstAdapter::GetClothCollection()); }

		// Patterns Group
		TArrayView<int32> GetSimVerticesStart() { return GetClothCollection()->GetElements(GetClothCollection()->SimVerticesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetSimVerticesEnd() { return GetClothCollection()->GetElements(GetClothCollection()->SimVerticesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetSimFacesStart() { return GetClothCollection()->GetElements(GetClothCollection()->SimFacesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetSimFacesEnd() { return GetClothCollection()->GetElements(GetClothCollection()->SimFacesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderVerticesStart() { return GetClothCollection()->GetElements(GetClothCollection()->RenderVerticesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderVerticesEnd() { return GetClothCollection()->GetElements(GetClothCollection()->RenderVerticesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderFacesStart() { return GetClothCollection()->GetElements(GetClothCollection()->RenderFacesStart, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }
		TArrayView<int32> GetRenderFacesEnd() { return GetClothCollection()->GetElements(GetClothCollection()->RenderFacesEnd, GetClothCollection()->PatternStart, GetClothCollection()->PatternEnd, GetElementIndex()); }

		// Seams Group
		int32 SetNumSeams(int32 NumSeams);
		TArrayView<FIntVector2> GetSeamPatterns() { return GetClothCollection()->GetElements(GetClothCollection()->SeamPatterns, GetClothCollection()->SeamStart, GetClothCollection()->SeamEnd, GetElementIndex()); }
		TArrayView<TArray<FIntVector2>> GetSeamStitches() { return GetClothCollection()->GetElements(GetClothCollection()->SeamStitches, GetClothCollection()->SeamStart, GetClothCollection()->SeamEnd, GetElementIndex()); }

		// All patterns Sim Vertices Group, use these to access all pattern at once, and when using the SimIndices
		TArrayView<FVector2f> GetPatternsSimPosition() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetPatternsSimRestPosition() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimRestPosition, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetPatternsSimRestNormal() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimRestNormal, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<int32> GetPatternsSimNumBoneInfluences() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimNumBoneInfluences, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<int32>> GetPatternsSimBoneIndices() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimBoneIndices, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<float>> GetPatternsSimBoneWeights() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimBoneWeights, GetClothCollection()->SimVerticesStart, GetClothCollection()->SimVerticesEnd, GetElementIndex()); }

		// All patterns Sim Faces Group, use these to access all pattern at once
		TArrayView<FIntVector3> GetPatternsSimIndices() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->SimIndices, GetClothCollection()->SimFacesStart, GetClothCollection()->SimFacesEnd, GetElementIndex()); }

		// All patterns Render Vertices Group, use these to access all pattern at once, and when using the RenderIndices
		TArrayView<FVector3f> GetPatternsRenderPosition() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderPosition, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetPatternsRenderNormal() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderNormal, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetPatternsRenderTangentU() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderTangentU, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FVector3f> GetPatternsRenderTangentV() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderTangentV, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<FVector2f>> GePatternstRenderUVs() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderUVs, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<FLinearColor> GetPatternsRenderColor() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderColor, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<int32> GetPatternsRenderNumBoneInfluences() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderNumBoneInfluences, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<int32>> GetPatternsRenderBoneIndices() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderBoneIndices, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }
		TArrayView<TArray<float>> GetPatternsRenderBoneWeights() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderBoneWeights, GetClothCollection()->RenderVerticesStart, GetClothCollection()->RenderVerticesEnd, GetElementIndex()); }

		// All patterns Render Faces Group, use these to access all pattern at once
		TArrayView<FIntVector3> GetPatternsRenderIndices() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderIndices, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }
		TArrayView<int32> GetPatternsRenderMaterialIndex() { return GetClothCollection()->GetPatternsElements(GetClothCollection()->RenderMaterialIndex, GetClothCollection()->RenderFacesStart, GetClothCollection()->RenderFacesEnd, GetElementIndex()); }

	private:
		friend class FClothAdapter;

		void SetDefaults();
	};
}  // End namespace UE::Chaos::ClothAsset
