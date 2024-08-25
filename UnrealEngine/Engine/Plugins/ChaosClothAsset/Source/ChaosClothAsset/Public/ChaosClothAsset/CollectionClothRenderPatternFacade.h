// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FManagedArrayCollection;

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth Asset collection render pattern facade class to access cloth render pattern data.
	 * Constructed from FCollectionClothConstFacade.
	 * Const access (read only) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothRenderPatternConstFacade
	{
	public:
		FCollectionClothRenderPatternConstFacade() = delete;

		FCollectionClothRenderPatternConstFacade(const FCollectionClothRenderPatternConstFacade&) = delete;
		FCollectionClothRenderPatternConstFacade& operator=(const FCollectionClothRenderPatternConstFacade&) = delete;

		FCollectionClothRenderPatternConstFacade(FCollectionClothRenderPatternConstFacade&&) = default;
		FCollectionClothRenderPatternConstFacade& operator=(FCollectionClothRenderPatternConstFacade&&) = default;

		virtual ~FCollectionClothRenderPatternConstFacade() = default;

		/** Return the render deformer number of influences for this pattern. */
		int32 GetRenderDeformerNumInfluences() const;
		/** Return the render material for this pattern. */
		const FString& GetRenderMaterialPathName() const;

		//~ Render Vertices Group
		// Note: Use the FCollectionClothConstFacade accessors instead of these for the array indices to match the RenderIndices values
		/** Return the total number of render vertices for this pattern. */
		int32 GetNumRenderVertices() const;
		/** Return the render vertices offset for this pattern in the render vertices for the collection. */
		int32 GetRenderVerticesOffset() const;
		TConstArrayView<FVector3f> GetRenderPosition() const;
		TConstArrayView<FVector3f> GetRenderNormal() const;
		TConstArrayView<FVector3f> GetRenderTangentU() const;
		TConstArrayView<FVector3f> GetRenderTangentV() const;
		TConstArrayView<TArray<FVector2f>> GetRenderUVs() const;
		TConstArrayView<FLinearColor> GetRenderColor() const;
		TConstArrayView<TArray<int32>> GetRenderBoneIndices() const;
		TConstArrayView<TArray<float>> GetRenderBoneWeights() const;
		TConstArrayView<TArray<FVector4f>> GetRenderDeformerPositionBaryCoordsAndDist() const;
		TConstArrayView<TArray<FVector4f>> GetRenderDeformerNormalBaryCoordsAndDist() const;
		TConstArrayView<TArray<FVector4f>> GetRenderDeformerTangentBaryCoordsAndDist() const;
		TConstArrayView<TArray<FIntVector3>> GetRenderDeformerSimIndices3D() const;
		TConstArrayView<TArray<float>> GetRenderDeformerWeight() const;
		TConstArrayView<float> GetRenderDeformerSkinningBlend() const;

		//~ Render Faces Group
		// Note: RenderIndices points to the collection arrays, not the pattern arrays
		int32 GetNumRenderFaces() const;
		/** Return the render faces offset for this pattern in the render faces */
		int32 GetRenderFacesOffset() const;
		TConstArrayView<FIntVector3> GetRenderIndices() const;

		bool IsEmpty() const;

		/** Return the Pattern index this facade has been created with. */
		int32 GetPatternIndex() const { return PatternIndex; }

	protected:
		friend class FCollectionClothRenderPatternFacade;  // For other instances access
		friend class FCollectionClothConstFacade;
		FCollectionClothRenderPatternConstFacade(const TSharedRef<const class FClothCollection>& InClothCollection, int32 InPatternIndex);

		static constexpr int32 GetBaseElementIndex() { return 0; }
		int32 GetElementIndex() const { return GetBaseElementIndex() + PatternIndex; }

		TSharedRef<const class FClothCollection> ClothCollection;
		int32 PatternIndex;
	};

	/**
	 * Cloth Asset collection render pattern facade class to access cloth render pattern data.
	 * Constructed from FCollectionClothFacade.
	 * Non-const access (read/write) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothRenderPatternFacade final : public FCollectionClothRenderPatternConstFacade
	{
	public:
		FCollectionClothRenderPatternFacade() = delete;

		FCollectionClothRenderPatternFacade(const FCollectionClothRenderPatternFacade&) = delete;
		FCollectionClothRenderPatternFacade& operator=(const FCollectionClothRenderPatternFacade&) = delete;

		FCollectionClothRenderPatternFacade(FCollectionClothRenderPatternFacade&&) = default;
		FCollectionClothRenderPatternFacade& operator=(FCollectionClothRenderPatternFacade&&) = default;

		virtual ~FCollectionClothRenderPatternFacade() override = default;

		/** Remove all geometry from this cloth pattern. */
		void Reset();

		/** Initialize from another render pattern. Assumes all indices match between source and target. */
		void Initialize(const FCollectionClothRenderPatternConstFacade& Other, int32 SimVertex3DOffset);

		/** Initialize from another render pattern. Assumes all indices match between source and target. */
		UE_DEPRECATED(5.4, "Use Initialize with the SimVertex3DOffset instead")
		void Initialize(const FCollectionClothRenderPatternConstFacade& Other) { Initialize(Other, 0); }

		/** Set the render deformer number of influences for this pattern. */
		void SetRenderDeformerNumInfluences(int32 NumInfluences);
		/** Set the render material for this pattern. */
		void SetRenderMaterialPathName(const FString& PathName);

		//~ Render Vertices Group
		// Note: Use the FCollectionClothFacade accessors instead of these for the array indices to match the RenderIndices values
		/** Grow or shrink the space reserved for render vertices for this pattern within the cloth collection and return its start index. */
		void SetNumRenderVertices(int32 NumRenderVertices);
		void RemoveRenderVertices(const TArray<int32>& SortedDeletionList);
		TArrayView<FVector3f> GetRenderPosition();
		TArrayView<FVector3f> GetRenderNormal();
		TArrayView<FVector3f> GetRenderTangentU();
		TArrayView<FVector3f> GetRenderTangentV();
		TArrayView<TArray<FVector2f>> GetRenderUVs();
		TArrayView<FLinearColor> GetRenderColor();
		TArrayView<TArray<int32>> GetRenderBoneIndices();
		TArrayView<TArray<float>> GetRenderBoneWeights();
		TArrayView<TArray<FVector4f>> GetRenderDeformerPositionBaryCoordsAndDist();
		TArrayView<TArray<FVector4f>> GetRenderDeformerNormalBaryCoordsAndDist();
		TArrayView<TArray<FVector4f>> GetRenderDeformerTangentBaryCoordsAndDist();
		TArrayView<TArray<FIntVector3>> GetRenderDeformerSimIndices3D();
		TArrayView<TArray<float>> GetRenderDeformerWeight();
		TArrayView<float> GetRenderDeformerSkinningBlend();

		//~ Render Faces Group
		/** Grow or shrink the space reserved for render faces for this pattern within the cloth collection and return its start index. */
		void SetNumRenderFaces(int32 NumRenderFaces);
		void RemoveRenderFaces(const TArray<int32>& SortedDeletionList);
		TArrayView<FIntVector3> GetRenderIndices();

	protected:
		friend class FCollectionClothFacade;
		FCollectionClothRenderPatternFacade(const TSharedRef<class FClothCollection>& InClothCollection, int32 InPatternIndex);

		void SetDefaults();

		TSharedRef<class FClothCollection> GetClothCollection() { return ConstCastSharedRef<class FClothCollection>(ClothCollection); }
	};
}  // End namespace UE::Chaos::ClothAsset
