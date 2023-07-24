// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArrayCollection.h"

// TODO:
// - Transition Sim LOD data
// - Move wrap deformer into struct similar to FMeshToMeshVertData

namespace UE::Chaos::ClothAsset
{
	/**
	 * Tailored Cloth Asset Collection containing draping and pattern information.
	 */
	class CHAOSCLOTHASSET_API FClothCollection : public FManagedArrayCollection
	{
	public:
		typedef FManagedArrayCollection Super;

		FClothCollection();
		FClothCollection(FClothCollection&) = delete;
		FClothCollection& operator=(const FClothCollection&) = delete;
		FClothCollection(FClothCollection&&) = default;
		FClothCollection& operator=(FClothCollection&&) = default;

		using Super::Serialize;
		void Serialize(FArchive& Ar);

		/** Set the number of elements to one of the groups that have start/end indices while maintaining the correct order of the data. */
		int32 SetNumElements(int32 InNumElements, const FName& GroupName, TManagedArray<int32>& StartArray, TManagedArray<int32>& EndArray, int32 StartEndIndex);

		/** Get the number of elements to one of the groups that have start/end indices. */
		int32 GetNumElements(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex) const;

		template<typename T>
		inline TConstArrayView<T> GetElements(const TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex) const;
		template<typename T>
		inline TArrayView<T> GetElements(TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex);

		int32 GetPatternsNumElements(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex) const;

		template<typename T>
		inline TConstArrayView<T> GetPatternsElements(const TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex) const;
		template<typename T>
		inline TArrayView<T> GetPatternsElements(TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex);

		template<bool bStart = true, bool bEnd = true>
		inline TTuple<int32, int32> GetPatternsElementsStartEnd(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex) const;

		// Attribute groups, predefined data member of the collection
		static const FName SimVerticesGroup;  // Contains patterns' 2D positions, 3D draped position (rest)
		static const FName SimFacesGroup;  // Contains indices to sim vertex
		static const FName RenderVerticesGroup;  // Contains pattern's 3D render model
		static const FName RenderFacesGroup;  // Contains indices to render vertex
		static const FName WrapDeformersGroup;  // Contains the wrap deformers cloth capture information
		static const FName PatternsGroup;  // Contains pattern relationships to other groups
		static const FName SeamsGroup;  // Contains pairs of stitched sim vertex indices and matching pattern indices
		static const FName TethersGroup;  // Tethers information
		static const FName TetherBatchesGroup;  // Tethers parallel batch processing information
		static const FName LodsGroup;  // Lod split information

		// Sim Vertices Group
		TManagedArray<FVector2f> SimPosition;
		TManagedArray<FVector3f> SimRestPosition;
		TManagedArray<FVector3f> SimRestNormal;  // Used for capture, maxdistance, backstop authoring ...etc
		TManagedArray<int32> SimNumBoneInfluences;
		TManagedArray<TArray<int32>> SimBoneIndices;
		TManagedArray<TArray<float>> SimBoneWeights;

		// Sim Faces Group
		TManagedArray<FIntVector3> SimIndices;  // The indices point to the elements in the Sim Vertices arrays but don't include the LOD start offset

		// Render Vertices Group
		TManagedArray<FVector3f> RenderPosition;
		TManagedArray<FVector3f> RenderNormal;
		TManagedArray<FVector3f> RenderTangentU;
		TManagedArray<FVector3f> RenderTangentV;
		TManagedArray<TArray<FVector2f>> RenderUVs;
		TManagedArray<FLinearColor> RenderColor;
		TManagedArray<int32> RenderNumBoneInfluences;
		TManagedArray<TArray<int32>> RenderBoneIndices;
		TManagedArray<TArray<float>> RenderBoneWeights;

		// Render Faces Group
		TManagedArray<FIntVector3> RenderIndices;  // The indices point to the elements in the Render Vertices arrays but don't include the LOD start offset
		TManagedArray<int32> RenderMaterialIndex;  // Render material per triangle

		// TODO: FMeshToMeshVertData
		//// Wrap Deformers Group (render mesh captured, sim mesh capturing)
		//TManagedArray<FVector4f> CapturedPosition;  // Barycentric coordinate and distance for captured render positions
		//TManagedArray<FVector4f> CapturedNormal;  // Barycentric coordinate and distance for captured render normals
		//TManagedArray<FVector4f> CapturedTangent;  // Barycentric coordinate and distance for captured render tangents
		//TManagedArray<int32> CapturingFace;  // Sim triangle index
		//TManagedArray<float> CapturingWeight;  // Weight for when using multiple capture influence per point, 1.0 otherwise

		// Patterns Group
		TManagedArray<int32> SimVerticesStart;
		TManagedArray<int32> SimVerticesEnd;
		TManagedArray<int32> SimFacesStart;
		TManagedArray<int32> SimFacesEnd;
		TManagedArray<int32> RenderVerticesStart;
		TManagedArray<int32> RenderVerticesEnd;
		TManagedArray<int32> RenderFacesStart;
		TManagedArray<int32> RenderFacesEnd;
		TManagedArray<int32> WrapDeformerStart;
		TManagedArray<int32> WrapDeformerEnd;
		TManagedArray<int32> NumWeights;  // Number of weights stored between WrapDeformerStart and WrapDeformerEnd
		TManagedArray<int32> StatusFlags;  // Whether this pattern should be rendered, simulated, ...etc
		TManagedArray<int32> SimMaterialIndex;  // Cloth material per pattern

		// Seam Group
		TManagedArray<FIntVector2> SeamPatterns;  // Stitched pattern indices (LOD based indices) for SeamIndicesFirst
		TManagedArray<TArray<FIntVector2>> SeamStitches;  // Stitched vertex indices pair (LOD based indices)

		// Tethers Group
		TManagedArray<int32> TetherKinematicIndex;
		TManagedArray<int32> TetherDynamicIndex;
		TManagedArray<float> TetherReferenceLength;

		// Tether Batches Group
		TManagedArray<int32> TetherStart;
		TManagedArray<int32> TetherEnd;

		// LOD Group
		TManagedArray<int32> PatternStart;
		TManagedArray<int32> PatternEnd;
		TManagedArray<int32> SeamStart;
		TManagedArray<int32> SeamEnd;
		TManagedArray<int32> TetherBatchStart;
		TManagedArray<int32> TetherBatchEnd;
		TManagedArray<int32> LodBiasDepth;  // The number of LODs covered by each Sim LOD (for the wrap deformer)

	protected:
		void Construct();
	};

	template<typename T>
	inline TConstArrayView<T> FClothCollection::GetElements(const TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex) const
	{
		const int32 Start = StartArray[StartEndIndex];
		const int32 End = EndArray[StartEndIndex];
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		return Start == INDEX_NONE ? TConstArrayView<T>() : TConstArrayView<T>(ElementArray.GetData() + Start, End - Start + 1);
	}

	template<typename T>
	inline TArrayView<T> FClothCollection::GetElements(TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex)
	{
		const int32 Start = StartArray[StartEndIndex];
		const int32 End = EndArray[StartEndIndex];
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		return Start == INDEX_NONE ? TArrayView<T>() : TArrayView<T>(ElementArray.GetData() + Start, End - Start + 1);
	}

	template<typename T>
	inline TConstArrayView<T> FClothCollection::GetPatternsElements(const TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex) const
	{
		const TTuple<int32, int32> StartEnd = GetPatternsElementsStartEnd(StartArray, EndArray, LodIndex);
		const int32 Start = StartEnd.Get<0>();
		const int32 End = StartEnd.Get<1>();

		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE
		return Start == INDEX_NONE ? TConstArrayView<T>() : TConstArrayView<T>(ElementArray.GetData() + Start, End - Start + 1);
	}

	template<typename T>
	inline TArrayView<T> FClothCollection::GetPatternsElements(TManagedArray<T>& ElementArray, const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex)
	{
		const TTuple<int32, int32> StartEnd = GetPatternsElementsStartEnd(StartArray, EndArray, LodIndex);
		const int32 Start = StartEnd.Get<0>();
		const int32 End = StartEnd.Get<1>();

		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE
		return Start == INDEX_NONE ? TArrayView<T>() : TArrayView<T>(ElementArray.GetData() + Start, End - Start + 1);
	}

	template<bool bStart, bool bEnd>
	inline TTuple<int32, int32> FClothCollection::GetPatternsElementsStartEnd(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex) const
	{
		const int32 LodPatternStart = PatternStart[LodIndex];
		const int32 LodPatternEnd = PatternEnd[LodIndex];

		int32 Start = INDEX_NONE;  // Find Start and End indices for the entire LOD minding empty patterns on the way
		int32 End = INDEX_NONE;
		for (int32 PatternIndex = LodPatternStart; PatternIndex <= LodPatternEnd; ++PatternIndex)
		{
			if (bStart && StartArray[PatternIndex] != INDEX_NONE)
			{
				Start = (Start == INDEX_NONE) ? StartArray[PatternIndex] : FMath::Min(Start, StartArray[PatternIndex]);
			}
			if (bEnd && EndArray[PatternIndex] != INDEX_NONE)
			{
				End = (End == INDEX_NONE) ? EndArray[PatternIndex] : FMath::Max(End, EndArray[PatternIndex]);
			}
		}
		return TTuple<int32, int32>(Start, End);
	}
}  // End namespace UE::Chaos::ClothAsset
