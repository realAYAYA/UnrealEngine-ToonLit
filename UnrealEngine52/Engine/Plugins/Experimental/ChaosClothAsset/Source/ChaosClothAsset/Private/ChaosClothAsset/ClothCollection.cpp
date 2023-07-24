// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothCollection.h"

namespace UE::Chaos::ClothAsset
{
	const FName UE::Chaos::ClothAsset::FClothCollection::SimVerticesGroup("SimVertices");
	const FName UE::Chaos::ClothAsset::FClothCollection::SimFacesGroup("SimFaces");
	const FName UE::Chaos::ClothAsset::FClothCollection::RenderVerticesGroup("RenderVertices");
	const FName UE::Chaos::ClothAsset::FClothCollection::RenderFacesGroup("RenderFaces");
	const FName UE::Chaos::ClothAsset::FClothCollection::WrapDeformersGroup("WrapDeformers");
	const FName UE::Chaos::ClothAsset::FClothCollection::PatternsGroup("Patterns");
	const FName UE::Chaos::ClothAsset::FClothCollection::SeamsGroup("Seams");
	const FName UE::Chaos::ClothAsset::FClothCollection::TethersGroup("Tethers");
	const FName UE::Chaos::ClothAsset::FClothCollection::TetherBatchesGroup("TetherBatches");
	const FName UE::Chaos::ClothAsset::FClothCollection::LodsGroup("Lods");

	FClothCollection::FClothCollection()
	{
		Construct();
	}

	void FClothCollection::Serialize(FArchive& Ar)
	{
		::Chaos::FChaosArchive ChaosArchive(Ar);
		Super::Serialize(ChaosArchive);
	}

	void FClothCollection::Construct()
	{
		// Dependencies
		FManagedArrayCollection::FConstructionParameters SimVerticesDependency(SimVerticesGroup);
		FManagedArrayCollection::FConstructionParameters SimFacesDependency(SimFacesGroup);
		FManagedArrayCollection::FConstructionParameters RenderVerticesDependency(RenderVerticesGroup);
		FManagedArrayCollection::FConstructionParameters RenderFacesDependency(RenderFacesGroup);
		FManagedArrayCollection::FConstructionParameters WrapDeformersDependency(WrapDeformersGroup);
		FManagedArrayCollection::FConstructionParameters PatternsDependency(PatternsGroup);
		FManagedArrayCollection::FConstructionParameters StitchingsDependency(SeamsGroup);
		FManagedArrayCollection::FConstructionParameters TethersDependency(TethersGroup);
		FManagedArrayCollection::FConstructionParameters TetherBatchesDependency(TetherBatchesGroup);

		// Sim Vertices Group
		AddExternalAttribute<FVector2f>("SimPosition", SimVerticesGroup, SimPosition);
		AddExternalAttribute<FVector3f>("SimRestPosition", SimVerticesGroup, SimRestPosition);
		AddExternalAttribute<FVector3f>("SimRestNormal", SimVerticesGroup, SimRestNormal);
		AddExternalAttribute<int32>("SimNumBoneInfluences", SimVerticesGroup, SimNumBoneInfluences);
		AddExternalAttribute<TArray<int32>>("SimBoneIndices", SimVerticesGroup, SimBoneIndices);
		AddExternalAttribute<TArray<float>>("SimBoneWeights", SimVerticesGroup, SimBoneWeights);

		// Sim Faces Group
		AddExternalAttribute<FIntVector3>("SimIndices", SimFacesGroup, SimIndices, SimVerticesDependency);

		// Render Vertices Group
		AddExternalAttribute<FVector3f>("RenderPosition", RenderVerticesGroup, RenderPosition);
		AddExternalAttribute<FVector3f>("RenderNormal", RenderVerticesGroup, RenderNormal);
		AddExternalAttribute<FVector3f>("RenderTangentU", RenderVerticesGroup, RenderTangentU);
		AddExternalAttribute<FVector3f>("RenderTangentV", RenderVerticesGroup, RenderTangentV);
		AddExternalAttribute<TArray<FVector2f>>("RenderUVs", RenderVerticesGroup, RenderUVs);
		AddExternalAttribute<FLinearColor>("RenderColor", RenderVerticesGroup, RenderColor);
		AddExternalAttribute<int32>("RenderNumBoneInfluences", RenderVerticesGroup, RenderNumBoneInfluences);
		AddExternalAttribute<TArray<int32>>("RenderBoneIndices", RenderVerticesGroup, RenderBoneIndices);
		AddExternalAttribute<TArray<float>>("RenderBoneWeights", RenderVerticesGroup, RenderBoneWeights);

		// Render Faces Group
		AddExternalAttribute<FIntVector3>("RenderIndices", RenderFacesGroup, RenderIndices, RenderVerticesDependency);
		AddExternalAttribute<int32>("RenderMaterialIndex", RenderFacesGroup, RenderMaterialIndex);

		// TODO: FMeshToMeshVertData

		// Patterns Group
		AddExternalAttribute<int32>("SimVerticesStart", PatternsGroup, SimVerticesStart, SimVerticesDependency);
		AddExternalAttribute<int32>("SimVerticesEnd", PatternsGroup, SimVerticesEnd, SimVerticesDependency);
		AddExternalAttribute<int32>("SimFacesStart", PatternsGroup, SimFacesStart, SimFacesDependency);
		AddExternalAttribute<int32>("SimFacesEnd", PatternsGroup, SimFacesEnd, SimFacesDependency);
		AddExternalAttribute<int32>("RenderVerticesStart", PatternsGroup, RenderVerticesStart, RenderVerticesDependency);
		AddExternalAttribute<int32>("RenderVerticesEnd", PatternsGroup, RenderVerticesEnd, RenderVerticesDependency);
		AddExternalAttribute<int32>("RenderFacesStart", PatternsGroup, RenderFacesStart, RenderFacesDependency);
		AddExternalAttribute<int32>("RenderFacesEnd", PatternsGroup, RenderFacesEnd, RenderFacesDependency);
		AddExternalAttribute<int32>("WrapDeformerStart", PatternsGroup, WrapDeformerStart, WrapDeformersDependency);
		AddExternalAttribute<int32>("WrapDeformerEnd", PatternsGroup, WrapDeformerEnd, WrapDeformersDependency);
		AddExternalAttribute<int32>("NumWeights", PatternsGroup, NumWeights);
		AddExternalAttribute<int32>("StatusFlags", PatternsGroup, StatusFlags);
		AddExternalAttribute<int32>("SimMaterialIndex", PatternsGroup, SimMaterialIndex);

		// Seams Group
		AddExternalAttribute<FIntVector2>("SeamPatterns", SeamsGroup, SeamPatterns, SimVerticesDependency);
		AddExternalAttribute<TArray<FIntVector2>>("SeamStitches", SeamsGroup, SeamStitches, SimVerticesDependency);

		// Tethers Group
		AddExternalAttribute<int32>("TetherKinematicIndex", TethersGroup, TetherKinematicIndex, SimVerticesDependency);
		AddExternalAttribute<int32>("TetherDynamicIndex", TethersGroup, TetherDynamicIndex, SimVerticesDependency);
		AddExternalAttribute<float>("TetherReferenceLength", TethersGroup, TetherReferenceLength);

		// Tether Batches Group
		AddExternalAttribute<int32>("TetherStart", TetherBatchesGroup, TetherStart, TethersDependency);
		AddExternalAttribute<int32>("TetherEnd", TetherBatchesGroup, TetherEnd, TethersDependency);

		// LOD Group
		AddExternalAttribute<int32>("PatternStart", LodsGroup, PatternStart, PatternsDependency);
		AddExternalAttribute<int32>("PatternEnd", LodsGroup, PatternEnd, PatternsDependency);
		AddExternalAttribute<int32>("StitchingStart", LodsGroup, SeamStart, StitchingsDependency);
		AddExternalAttribute<int32>("StitchingEnd", LodsGroup, SeamEnd, StitchingsDependency);
		AddExternalAttribute<int32>("TetherBatchStart", LodsGroup, TetherBatchStart, TetherBatchesDependency);
		AddExternalAttribute<int32>("TetherBatchEnd", LodsGroup, TetherBatchEnd, TetherBatchesDependency);
		AddExternalAttribute<int32>("LodBiasDepth", LodsGroup, LodBiasDepth);
	}

	int32 FClothCollection::SetNumElements(int32 InNumElements, const FName& GroupName, TManagedArray<int32>& StartArray, TManagedArray<int32>& EndArray, int32 StartEndIndex)
	{
		check(InNumElements >= 0);

		int32& Start = StartArray[StartEndIndex];
		int32& End = EndArray[StartEndIndex];
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		const int32 NumElements = (Start == INDEX_NONE) ? 0 : End - Start + 1;

		if (const int32 Delta = InNumElements - NumElements)
		{
			if (Delta > 0)
			{
				// Find a previous valid index range to insert after when the range is empty
				auto ComputeEnd = [&EndArray](int32 StartEndIndex)->int32
				{
					for (int32 Index = StartEndIndex; Index >= 0; --Index)
					{
						if (EndArray[Index] != INDEX_NONE)
						{
							return EndArray[Index];
						}
					}
					return INDEX_NONE;
				};

				// Grow the array
				const int32 Position = ComputeEnd(StartEndIndex) + 1;
				InsertElements(Delta, Position, GroupName);

				// Update Start/End
				if (!NumElements)
				{
					Start = Position;
				}
				End = Start + InNumElements - 1;
			}
			else
			{
				// Shrink the array
				const int32 Position = Start + InNumElements;
				RemoveElements(GroupName, -Delta, Position);

				// Update Start/End
				if (InNumElements)
				{
					End = Position - 1;
				}
				else
				{
					End = Start = INDEX_NONE;  // It is important to set the start & end to INDEX_NONE so that they never get automatically re-indexed by the managed array collection
				}
			}
		}
		return Start;
	}

	int32 FClothCollection::GetNumElements(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 StartEndIndex) const
	{
		const int32 Start = StartArray[StartEndIndex];
		const int32 End = EndArray[StartEndIndex];
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		return Start == INDEX_NONE ? 0 : End - Start + 1;
	}

	int32 FClothCollection::GetPatternsNumElements(const TManagedArray<int32>& StartArray, const TManagedArray<int32>& EndArray, int32 LodIndex) const
	{
		const TTuple<int32, int32> StartEnd = GetPatternsElementsStartEnd(StartArray, EndArray, LodIndex);
		const int32 Start = StartEnd.Get<0>();
		const int32 End = StartEnd.Get<1>();
		check(Start != INDEX_NONE || End == INDEX_NONE);  // Best to avoid situations where only one boundary of the range is set to INDEX_NONE

		return Start == INDEX_NONE ? 0 : End - Start + 1;
	}
} // End namespace UE::Chaos::ClothAsset
