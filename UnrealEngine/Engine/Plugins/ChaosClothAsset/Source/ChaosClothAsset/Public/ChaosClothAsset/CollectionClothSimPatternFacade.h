// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE::Chaos::ClothAsset
{
	/** Integral and vector types valid as index type in initializations. */
	template<typename T> struct TIsIndexType { static constexpr bool Value = false; };
	template<> struct TIsIndexType<int32> { static constexpr bool Value = true; };
	template<> struct TIsIndexType<uint32> { static constexpr bool Value = true; };
	template<> struct TIsIndexType<FIntVector3> { static constexpr bool Value = true; };

	/**
	 * Cloth Asset collection sim pattern facade class to access cloth sim pattern data.
	 * Constructed from FCollectionClothConstFacade.
	 * Const access (read only) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothSimPatternConstFacade
	{
	public:
		FCollectionClothSimPatternConstFacade() = delete;

		FCollectionClothSimPatternConstFacade(const FCollectionClothSimPatternConstFacade&) = delete;
		FCollectionClothSimPatternConstFacade& operator=(const FCollectionClothSimPatternConstFacade&) = delete;

		FCollectionClothSimPatternConstFacade(FCollectionClothSimPatternConstFacade&&) = default;
		FCollectionClothSimPatternConstFacade& operator=(FCollectionClothSimPatternConstFacade&&) = default;

		virtual ~FCollectionClothSimPatternConstFacade() = default;

		//~ Sim Vertices 2D Group
		// Note: Use the FCollectionClothConstFacade accessors instead of these for the array indices to match the SimIndices2D values
		/** Return the total number of simulation vertices for this pattern. */
		int32 GetNumSimVertices2D() const;
		/** Return the simulation vertices offset for this pattern in the simulation vertices for the collection. */
		int32 GetSimVertices2DOffset() const;
		TConstArrayView<FVector2f> GetSimPosition2D() const;
		TConstArrayView<int32> GetSimVertex3DLookup() const;

		//~ Sim Faces Group
		// Note: SimIndices points to the collection arrays, not the pattern arrays
		int32 GetNumSimFaces() const;
		/** Return the simulation faces offset for this pattern in the simulation faces. */
		int32 GetSimFacesOffset() const;
		TConstArrayView<FIntVector3> GetSimIndices2D() const;
		TConstArrayView<FIntVector3> GetSimIndices3D() const;

		/** Whether or not this pattern is empty. */
		bool IsEmpty() const;

		/** Return the Pattern index this facade has been created with. */
		int32 GetPatternIndex() const { return PatternIndex; }
		
		/** Return the Pattern index this facade has been created with. */
        int32 GetFabricIndex() const;

	protected:
		friend class FCollectionClothSimPatternFacade;  // For other instances access
		friend class FCollectionClothConstFacade;
		FCollectionClothSimPatternConstFacade(const TSharedRef<const class FClothCollection>& InClothCollection, int32 InPatternIndex);

		static constexpr int32 GetBaseElementIndex() { return 0; }
		int32 GetElementIndex() const { return GetBaseElementIndex() + PatternIndex; }

		TSharedRef<const class FClothCollection> ClothCollection;
		int32 PatternIndex;
	};

	/**
	 * Cloth Asset collection sim pattern facade class to access cloth sim pattern data.
	 * Constructed from FCollectionClothFacade.
	 * Non-const access (read/write) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothSimPatternFacade final : public FCollectionClothSimPatternConstFacade
	{
	public:
		FCollectionClothSimPatternFacade() = delete;

		FCollectionClothSimPatternFacade(const FCollectionClothSimPatternFacade&) = delete;
		FCollectionClothSimPatternFacade& operator=(const FCollectionClothSimPatternFacade&) = delete;

		FCollectionClothSimPatternFacade(FCollectionClothSimPatternFacade&&) = default;
		FCollectionClothSimPatternFacade& operator=(FCollectionClothSimPatternFacade&&) = default;

		virtual ~FCollectionClothSimPatternFacade() override = default;

		/** Remove all geometry from this cloth pattern. */
		void Reset();

		/** Initialize the cloth pattern using the specified 3D and 2D positions, and topology. */
		template<typename IndexType, TEMPLATE_REQUIRES(TIsIndexType<IndexType>::Value)>
		void Initialize(const TArray<FVector2f>& Positions2D, const TArray<FVector3f>& Positions3D, const TArray<IndexType>& Indices, const int32 FabricIndex = INDEX_NONE);

		/** Initialize this pattern using another pattern collection. */
		void Initialize(const FCollectionClothSimPatternConstFacade& Other, const int32 SimVertex3DOffset, const int32 FabricsOffset = 0);

		//~ Sim Vertices 2D Group
		// Note: Use the FCollectionClothConstFacade accessors instead of these for the array indices to match the SimIndices2D values
		TArrayView<FVector2f> GetSimPosition2D();

		/** This will remove the 2D vertices, but the associated seams and 3D vertices will still exist, and point to INDEX_NONE */
		void RemoveSimVertices2D(int32 NumSimVertices);
		void RemoveAllSimVertices2D() { RemoveSimVertices2D(GetNumSimVertices2D()); }
		void RemoveSimVertices2D(const TArray<int32>& SortedDeletionList);

		//~ Sim Faces Group
		// Note: SimIndices points to the collection arrays, not the pattern arrays
		/** Grow or shrink the space reserved for simulation faces for this pattern within the cloth collection and return its start index. */
		void SetNumSimFaces(int32 NumSimFaces);
		TArrayView<FIntVector3> GetSimIndices2D();
		TArrayView<FIntVector3> GetSimIndices3D();
		void RemoveSimFaces(const TArray<int32>& SortedDeletionList);

		/** Set the fabric index used by this pattern */
		void SetFabricIndex(const int32 InFabricIndex);

	private:
		friend class FCollectionClothFacade;
		FCollectionClothSimPatternFacade(const TSharedRef<class FClothCollection>& InClothCollection, int32 InPatternIndex);

		void SetDefaults();

		/** Append this many 2D and 3D SimVertices which correspond to each other. Returns {2D start index (local pattern index), 3D start index}. */
		FIntVector2 AppendSimVertices(int32 NumSimVertices);

		void SetNumSimVertices2D(int32 NumSimVertices);
		TArrayView<int32> GetSimVertex3DLookup();

		TSharedRef<class FClothCollection> GetClothCollection() { return ConstCastSharedRef<class FClothCollection>(ClothCollection); }
	};
}  // End namespace UE::Chaos::ClothAsset
