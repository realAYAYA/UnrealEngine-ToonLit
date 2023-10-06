// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothRenderPatternFacade.h"
#include "ChaosClothAsset/CollectionClothSeamFacade.h"

namespace Chaos
{
class FChaosArchive;
}

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth Asset collection facade class focused on draping and pattern information.
	 * Const access (read only) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothConstFacade
	{
	public:
		explicit FCollectionClothConstFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothConstFacade() = delete;

		FCollectionClothConstFacade(const FCollectionClothConstFacade&) = delete;
		FCollectionClothConstFacade& operator=(const FCollectionClothConstFacade&) = delete;

		FCollectionClothConstFacade(FCollectionClothConstFacade&&) = default;
		FCollectionClothConstFacade& operator=(FCollectionClothConstFacade&&) = default;

		virtual ~FCollectionClothConstFacade() = default;

		/** Return whether the facade is defined on the collection. */
		bool IsValid() const;

		//~ LOD (single per collection) Group
		/** Return the physics asset path names used for this collection. */
		const FString& GetPhysicsAssetPathName() const;
		/** Return the skeleton asset path names used for this collection. */
		const FString& GetSkeletalMeshPathName() const;

		//~ Sim Vertices 2D Group
		/** Return the total number of 2D simulation vertices for this collection. */
		int32 GetNumSimVertices2D() const;
		TConstArrayView<FVector2f> GetSimPosition2D() const;
		TConstArrayView<int32> GetSimVertex3DLookup() const;

		//~ Sim Vertices 3D Group
		/** Return the total number of 3D simulation vertices for this collection. */
		int32 GetNumSimVertices3D() const;
		TConstArrayView<FVector3f> GetSimPosition3D() const;
		TConstArrayView<FVector3f> GetSimNormal() const;
		TConstArrayView<TArray<int32>> GetSimBoneIndices() const;
		TConstArrayView<TArray<float>> GetSimBoneWeights() const;
		TConstArrayView<TArray<int32>> GetTetherKinematicIndex() const;
		TConstArrayView<TArray<float>> GetTetherReferenceLength() const;
		TConstArrayView<TArray<int32>> GetSimVertex2DLookup() const;
		TConstArrayView<TArray<int32>> GetSeamStitchLookup() const;

		//~ Sim Faces Group
		/** Return the total number of simulation faces for this collection across all patterns. */
		int32 GetNumSimFaces() const;
		TConstArrayView<FIntVector3> GetSimIndices2D() const;
		TConstArrayView<FIntVector3> GetSimIndices3D() const;

		//~ Sim Patterns Group
		/** Return the number of patterns in this collection. */
		int32 GetNumSimPatterns() const;
		/** Return a pattern facade for the specified pattern index. */
		FCollectionClothSimPatternConstFacade GetSimPattern(int32 PatternIndex) const;
		/** Convenience to find which sim pattern a 2D vertex belongs to */
		int32 FindSimPatternByVertex2D(int32 Vertex2DIndex) const;
		/** Convenience to find which sim pattern a sim face belongs to */
		int32 FindSimPatternByFaceIndex(int32 FaceIndex) const;


		//~ Render Patterns Group
		/** Return the number of patterns in this collection. */
		int32 GetNumRenderPatterns() const;
		/** Return a pattern facade for the specified pattern index. */
		FCollectionClothRenderPatternConstFacade GetRenderPattern(int32 PatternIndex) const;
		/** Return a view of all the render materials used on this collection across all patterns. */
		TConstArrayView<FString> GetRenderMaterialPathName() const;
		/** Convenience to find which render pattern a render vertex belongs to */
		int32 FindRenderPatternByVertex(int32 VertexIndex) const;
		/** Convenience to find which render pattern a render face belongs to */
		int32 FindRenderPatternByFaceIndex(int32 FaceIndex) const;

		//~ Seam Group
		/** Return the number of seams in this collection. */
		int32 GetNumSeams() const;
		/** Return a seam facade for the specified pattern index. */
		FCollectionClothSeamConstFacade GetSeam(int32 SeamIndex) const;

		//~ Render Vertices Group
		/** Return the total number of render vertices for this collection. */
		int32 GetNumRenderVertices() const;
		TConstArrayView<FVector3f> GetRenderPosition() const;
		TConstArrayView<FVector3f> GetRenderNormal() const;
		TConstArrayView<FVector3f> GetRenderTangentU() const;
		TConstArrayView<FVector3f> GetRenderTangentV() const;
		TConstArrayView<TArray<FVector2f>> GetRenderUVs() const;
		TConstArrayView<FLinearColor> GetRenderColor() const;
		TConstArrayView<TArray<int32>> GetRenderBoneIndices() const;
		TConstArrayView<TArray<float>> GetRenderBoneWeights() const;

		//~ Render Faces Group
		int32 GetNumRenderFaces() const;
		TConstArrayView<FIntVector3> GetRenderIndices() const;

		/** Return whether this cloth collection has the specified weight map. */
		bool HasWeightMap(const FName& Name) const;

		/** Return the name of all user weight maps on this cloth collection. */
		TArray<FName> GetWeightMapNames() const;

		TConstArrayView<float> GetWeightMap(const FName& Name) const;

		void BuildSimulationMesh(TArray<FVector3f>& Positions, TArray<FVector3f>& Normals, TArray<uint32>& Indices, TArray<FVector2f>& PatternsPositions, TArray<uint32>& PatternsIndices, 
			TArray<uint32>& PatternToWeldedIndices, TArray<TArray<int32>>* OptionalWeldedToPatternIndices = nullptr) const;

	protected:
		TSharedRef<const class FClothCollection> ClothCollection;

		friend class FCollectionClothSeamFacade;
		friend class FCollectionClothSeamConstFacade;
		explicit FCollectionClothConstFacade(const TSharedRef<const class FClothCollection>& ClothCollection);
	};

	/**
	 * Cloth Asset collection facade class focused on draping and pattern information.
	 * Non-const access (read/write) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothFacade final : public FCollectionClothConstFacade
	{
	public:
		explicit FCollectionClothFacade(const TSharedRef<FManagedArrayCollection>& ManagedArrayCollection);

		FCollectionClothFacade() = delete;

		FCollectionClothFacade(const FCollectionClothFacade&) = delete;
		FCollectionClothFacade& operator=(const FCollectionClothFacade&) = delete;

		FCollectionClothFacade(FCollectionClothFacade&&) = default;
		FCollectionClothFacade& operator=(FCollectionClothFacade&&) = default;
		virtual ~FCollectionClothFacade() override = default;

		/** Create this facade's groups and attributes. */
		void DefineSchema();

		/** Remove all LODs from this cloth. */
		void Reset();

		/** Initialize the cloth using another cloth collection. */
		void Initialize(const FCollectionClothConstFacade& Other);

		/** Append data from another cloth collection. */
		void Append(const FCollectionClothConstFacade& Other);

		/** Append to this cloth another cloth. */
		//void Append(const FCollectionClothConstFacade& Other);

		//~ LOD (single per collection) Group
		/** Set the physics asset path name. */
		void SetPhysicsAssetPathName(const FString& PathName);
		/** Set the skeletal mesh asset path name and the reference skeleton that will be used with this asset. */
		void SetSkeletalMeshPathName(const FString& PathName);

		//~ Pattern Sim Vertices 2D Group
		/** SetNumSimVertices2D per pattern within pattern facade. */
		TArrayView<FVector2f> GetSimPosition2D();

		//~ Pattern Sim Vertices 3D Group
		TArrayView<FVector3f> GetSimPosition3D();
		TArrayView<FVector3f> GetSimNormal();
		TArrayView<TArray<int32>> GetSimBoneIndices();
		TArrayView<TArray<float>> GetSimBoneWeights();
		TArrayView<TArray<int32>> GetTetherKinematicIndex();
		TArrayView<TArray<float>> GetTetherReferenceLength();

		/** This will remove the 3D vertices, but the associated seams and 2D vertices will still exist, and point to INDEX_NONE */
		void RemoveSimVertices3D(int32 NumSimVertices);
		void RemoveAllSimVertices3D() { RemoveSimVertices3D(GetNumSimVertices3D()); }
		void RemoveSimVertices3D(const TArray<int32>& SortedDeletionList);
		/** Compact SimVertex2DLookup to remove any references to INDEX_NONE that may have been created by deleting 2D vertices. */
		void CompactSimVertex2DLookup();

		//~ Pattern Sim Faces Group
		/** SetNumSimFaces per pattern within pattern facade. */
		TArrayView<FIntVector3> GetSimIndices2D();
		TArrayView<FIntVector3> GetSimIndices3D();

		//~ Sim Patterns Group
		/** Set the new number of patterns to this cloth LOD. */
		void SetNumSimPatterns(int32 NumPatterns);
		/** Add a new pattern to this cloth LOD and return its index in the LOD pattern list. */
		int32 AddSimPattern();
		/** Return a pattern facade for the specified pattern index. */
		FCollectionClothSimPatternFacade GetSimPattern(int32 PatternIndex);
		/** Add a new pattern to this cloth LOD, and return the cloth pattern facade set to its index. */
		FCollectionClothSimPatternFacade AddGetSimPattern() { return GetSimPattern(AddSimPattern()); }
		/** Remove a sorted list of sim patterns. */
		void RemoveSimPatterns(const TArray<int32>& SortedDeletionList);

		//~ Render Patterns Group
		/** Set the new number of patterns to this cloth LOD. */
		void SetNumRenderPatterns(int32 NumPatterns);
		/** Add a new pattern to this cloth LOD and return its index in the LOD pattern list. */
		int32 AddRenderPattern();
		/** Return a pattern facade for the specified pattern index. */
		FCollectionClothRenderPatternFacade GetRenderPattern(int32 PatternIndex);
		/** Add a new pattern to this cloth LOD, and return the cloth pattern facade set to its index. */
		FCollectionClothRenderPatternFacade AddGetRenderPattern() { return GetRenderPattern(AddRenderPattern()); }
		/** Remove a sorted list of render patterns. */
		void RemoveRenderPatterns(const TArray<int32>& SortedDeletionList);
		/** Return a view of all the render materials used on this collection across all patterns. */
		TArrayView<FString> GetRenderMaterialPathName();

		//~ Seam Group
		/** Set the new number of seams to this cloth. */
		void SetNumSeams(int32 NumSeams);
		/** Add a new seam to this cloth and return its index in the seam list. */
		int32 AddSeam();
		/** Return a seam facade for the specified seam index. */
		FCollectionClothSeamFacade GetSeam(int32 SeamIndex);
		/** Add a new seam to this cloth and return the seam facade set to its index. */
		FCollectionClothSeamFacade AddGetSeam() { return GetSeam(AddSeam()); }
		/** Remove a sorted list of seams. */
		void RemoveSeams(const TArray<int32>& SortedDeletionList);

		//~ Render Vertices Group
		/** SetNumRenderVertices per pattern within pattern facade. */
		TArrayView<FVector3f> GetRenderPosition();
		TArrayView<FVector3f> GetRenderNormal();
		TArrayView<FVector3f> GetRenderTangentU();
		TArrayView<FVector3f> GetRenderTangentV();
		TArrayView<TArray<FVector2f>> GetRenderUVs();
		TArrayView<FLinearColor> GetRenderColor();
		TArrayView<TArray<int32>> GetRenderBoneIndices();
		TArrayView<TArray<float>> GetRenderBoneWeights();

		//~ Render Faces Group
		/** SetNumRenderFaces per pattern within pattern facade. */
		TArrayView<FIntVector3> GetRenderIndices();

		/** Add a new weight map to this cloth. Access is then done per pattern. */
		void AddWeightMap(const FName& Name);

		/** Remove a weight map from this cloth. */
		void RemoveWeightMap(const FName& Name);

		TArrayView<float> GetWeightMap(const FName& Name);

	private:
		void SetDefaults();

		TSharedRef<class FClothCollection> GetClothCollection() { return ConstCastSharedRef<class FClothCollection>(ClothCollection); }

		friend class FCollectionClothSeamFacade;
		friend class FCollectionClothSimPatternFacade;

		explicit FCollectionClothFacade(const TSharedRef<class FClothCollection>& InClothCollection);

		// These methods are private because they're managed by the FCollectionClothSeamFacade.
		//~ Sim Vertices 2D Group
		TArrayView<int32> GetSimVertex3DLookupPrivate();

		//~ Sim Vertices 3D Group
		TArrayView<TArray<int32>> GetSeamStitchLookupPrivate();
		TArrayView<TArray<int32>> GetSimVertex2DLookupPrivate();
	};
}  // End namespace UE::Chaos::ClothAsset
