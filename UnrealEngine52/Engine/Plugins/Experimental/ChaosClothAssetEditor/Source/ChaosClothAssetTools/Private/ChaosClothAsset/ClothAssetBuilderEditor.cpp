// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAssetBuilderEditor.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Utils/ClothingMeshUtils.h"
#include "BoneWeights.h"
#include "MeshUtilities.h"
#include "PointWeightMap.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothAssetBuilderEditor)

void UClothAssetBuilderEditor::BuildLod(FSkeletalMeshLODModel& LODModel, const UChaosClothAsset& ClothAsset, int32 LodIndex) const
{
	// Start from an empty LODModel
	LODModel.Empty();

	// Clear the mesh infos, none are stored on this asset
	LODModel.ImportedMeshInfos.Empty();
	LODModel.MaxImportVertex = 0;

	// Set 1 texture coordinate
	LODModel.NumTexCoords = 1;

	// Init the size of the vertex buffer
	LODModel.NumVertices = 0;

	// Build the section/faces map from the LOD patterns
	TMap<int32, TArray<int32>> SectionFacesMap;
	SectionFacesMap.Reserve(ClothAsset.GetMaterials().Num());

	const TSharedPtr<const UE::Chaos::ClothAsset::FClothCollection> ClothCollection = ClothAsset.GetClothCollection();
	const int32 PatternStart = ClothCollection->PatternStart[LodIndex];
	const int32 PatternEnd = ClothCollection->PatternEnd[LodIndex];

	for (int32 PatternIndex = PatternStart; PatternIndex <= PatternEnd; ++PatternIndex)
	{
		const int32 RenderFacesStart = ClothCollection->RenderFacesStart[PatternIndex];
		const int32 RenderFacesEnd = ClothCollection->RenderFacesEnd[PatternIndex];

		for (int32 RenderFaceIndex = RenderFacesStart; RenderFaceIndex <= RenderFacesEnd; ++RenderFaceIndex)
		{
			const int32 MaterialIndex = ClothCollection->RenderMaterialIndex[RenderFaceIndex];

			TArray<int32>& SectionFaces = SectionFacesMap.FindOrAdd(MaterialIndex);
			SectionFaces.Add(RenderFaceIndex);
		}
	}

	// Initialize the remapping array to start at LodRenderVerticesStart with the maximum number of vertices used in this LOD
	const int32 LodRenderVerticesStart = ClothCollection->RenderVerticesStart[PatternStart];
	const int32 LodRenderVerticesEnd = ClothCollection->RenderVerticesEnd[PatternEnd];
	const int32 NumLodRenderVertices = LodRenderVerticesEnd - LodRenderVerticesStart + 1;

	TArray<uint32> LodRenderIndexRemap;
	LodRenderIndexRemap.SetNumUninitialized(NumLodRenderVertices);

	// Keep track of the active bone indices for this LOD model
	TSet<FBoneIndexType> ActiveBoneIndices;
	ActiveBoneIndices.Reserve(ClothAsset.RefSkeleton.GetNum());

	// Load the mesh utilities module used to optimized the index buffer
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	// Build the sim mesh descriptor for creation of the sections' mesh to mesh mapping data
	const ClothingMeshUtils::ClothMeshDesc SourceMesh(
		GetSimPositions(ClothAsset, LodIndex),
		GetSimIndices(ClothAsset, LodIndex));  // Let it calculate the averaged normals as to match the simulation data output

	const int32 NumLodSimVertices = GetNumVertices(ClothAsset, LodIndex);

	// Retrieve the MaxDistance map
	FPointWeightMap MaxDistances;
	MaxDistances.Initialize(NumLodSimVertices);
	for (int32 Index = 0; Index < NumLodSimVertices; ++Index)
	{
		MaxDistances[Index] = 200.f;
	}

	// Populate this LOD's sections and the LOD index buffer
	const int32 NumSections = SectionFacesMap.Num();
	LODModel.Sections.SetNum(NumSections);

	int32 SectionIndex = 0;
	int32 BaseIndex = 0;
	for (const TPair<int32, TArray<int32>>& SectionFaces : SectionFacesMap)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		Section.OriginalDataSectionIndex = SectionIndex++;

		const int32 MaterialIndex = SectionFaces.Key;
		const TArray<int32>& Faces = SectionFaces.Value;
		const int32 NumFaces = Faces.Num();
		const int32 NumIndices = NumFaces * 3;

		// Build the section face data (indices)
		Section.MaterialIndex = (uint16)MaterialIndex;

		Section.BaseIndex = (uint32)BaseIndex;
		BaseIndex += NumIndices;

		Section.NumTriangles = (uint32)NumFaces;

		TArray<uint32> Indices;
		TSet<int32> UniqueIndicesSet;
		Indices.SetNumUninitialized(NumIndices);
		for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
		{
			const FIntVector3& RenderIndices = ClothCollection->RenderIndices[Faces[FaceIndex]];
			for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
			{
				const int32 RenderIndex = RenderIndices[VertexIndex];
				Indices[FaceIndex * 3 + VertexIndex] = (uint32)RenderIndex;
				UniqueIndicesSet.Add(RenderIndex);
			}
		}

		MeshUtilities.CacheOptimizeIndexBuffer(Indices);

		LODModel.IndexBuffer.Append(MoveTemp(Indices));

		// Build the section vertex data from the unique indices
		TArray<int32> UniqueIndices = UniqueIndicesSet.Array();
		const int32 NumVertices = UniqueIndices.Num();

		Section.SoftVertices.SetNumUninitialized(NumVertices);
		Section.NumVertices = NumVertices;
		Section.BaseVertexIndex = LODModel.NumVertices;
		LODModel.NumVertices += (uint32)NumVertices;

		TSet<FBoneIndexType> SectionBoneMap;
		SectionBoneMap.Reserve(ClothAsset.RefSkeleton.GetNum());

		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			const int32 RenderIndex = UniqueIndices[VertexIndex];
			LODModel.MaxImportVertex = FMath::Max(LODModel.MaxImportVertex, RenderIndex);

			LodRenderIndexRemap[RenderIndex] = Section.BaseVertexIndex + (uint32)VertexIndex;

			FSoftSkinVertex& SoftVertex = Section.SoftVertices[VertexIndex];

			SoftVertex.Position = ClothCollection->RenderPosition[RenderIndex];
			SoftVertex.TangentX = ClothCollection->RenderTangentU[RenderIndex];
			SoftVertex.TangentY = ClothCollection->RenderTangentV[RenderIndex];
			SoftVertex.TangentZ = ClothCollection->RenderNormal[RenderIndex];

			constexpr bool bSRGB = false; // Avoid linear to srgb conversion
			SoftVertex.Color = ClothCollection->RenderColor[RenderIndex].ToFColor(bSRGB);

			const TArray<FVector2f>& RenderUVs = ClothCollection->RenderUVs[RenderIndex];
			for (int32 TexCoord = 0; TexCoord < FMath::Min(RenderUVs.Num(), (int32)MAX_TEXCOORDS); ++TexCoord)
			{
				SoftVertex.UVs[TexCoord] = RenderUVs[TexCoord];
			}

			for (int32 Influence = 0; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
			{
				const FBoneIndexType InfluenceBone = (FBoneIndexType)0;  // TODO: Set the correct bone influence from the cloth collection data
				const int16 InfluenceWeight = (Influence == 0) ? UE::AnimationCore::MaxRawBoneWeight : 0;

				SoftVertex.InfluenceBones[Influence] = InfluenceBone;
				SoftVertex.InfluenceWeights[Influence] = InfluenceWeight;

				if (InfluenceWeight)
				{
					SectionBoneMap.Add(InfluenceBone);
				}
			}
		}

		// Remap the LOD indices with the new vertex indices
		for (uint32& RenderIndex : LODModel.IndexBuffer)
		{
			RenderIndex = LodRenderIndexRemap[RenderIndex];
		}

		// Update the section bone map
		Section.BoneMap = SectionBoneMap.Array();
		ActiveBoneIndices.Append(MoveTemp(SectionBoneMap));

		// Update max bone influences
		Section.CalcMaxBoneInfluences();
		Section.CalcUse16BitBoneIndex();

		// Setup clothing data
		Section.ClothMappingDataLODs.SetNum(1);  // TODO: LODBias maps for raytracing

		Section.ClothingData.AssetLodIndex = LodIndex;
		Section.ClothingData.AssetGuid = ClothAsset.AssetGuid;  // There is only one cloth asset,
		Section.CorrespondClothAssetIndex = 0;       // this one

		TArray<FVector3f> RenderPositions;
		TArray<FVector3f> RenderNormals;
		TArray<FVector3f> RenderTangents;
		for (const FSoftSkinVertex& SoftVert : Section.SoftVertices)
		{
			RenderPositions.Add(SoftVert.Position);
			RenderNormals.Add(SoftVert.TangentZ);
			RenderTangents.Add(SoftVert.TangentX);
		}

		const ClothingMeshUtils::ClothMeshDesc TargetMesh(RenderPositions, 
			RenderNormals, 
			RenderTangents,
			LODModel.IndexBuffer);

		ClothingMeshUtils::GenerateMeshToMeshVertData(
			Section.ClothMappingDataLODs[0],
			TargetMesh,
			SourceMesh,
			&MaxDistances,
			ClothAsset.bSmoothTransition,
			ClothAsset.bUseMultipleInfluences,
			ClothAsset.SkinningKernelRadius);

		// Save the original indices for the newly added vertices
		LODModel.MeshToImportVertexMap.Append(MoveTemp(UniqueIndices));

		// Compute the overlapping vertices map (inspired from MeshUtilities::BuildSkeletalMesh)
		const TArray<FSoftSkinVertex>& SoftVertices = Section.SoftVertices;

		typedef TPair<float, int32> FIndexAndZ;  // Acceleration structure, list of vertex Z / index pairs
		TArray<FIndexAndZ> IndexAndZs;
		IndexAndZs.Reserve(NumVertices);
		for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			const FVector3f& Position = SoftVertices[VertexIndex].Position;

			const float Z = 0.30f * Position.X + 0.33f * Position.Y + 0.37f * Position.Z;
			IndexAndZs.Emplace(Z, VertexIndex);
		}
		IndexAndZs.Sort([](const FIndexAndZ& A, const FIndexAndZ& B) { return A.Key < B.Key; });

		for (int32 Index0 = 0; Index0 < IndexAndZs.Num(); ++Index0)
		{
			const float Z0 = IndexAndZs[Index0].Key;
			const uint32 VertexIndex0 = IndexAndZs[Index0].Value;
			const FVector3f& Position0 = SoftVertices[VertexIndex0].Position;

			// Only need to search forward, since we add pairs both ways
			for (int32 Index1 = Index0 + 1; Index1 < IndexAndZs.Num() && FMath::Abs(IndexAndZs[Index1].Key - Z0) <= THRESH_POINTS_ARE_SAME; ++Index1)
			{
				const uint32 VertexIndex1 = IndexAndZs[Index1].Value;
				const FVector3f& Position1 = SoftVertices[VertexIndex1].Position;

				if (PointsEqual(Position0, Position1))
				{
					// Add to the overlapping map
					TArray<int32>& SrcValueArray = Section.OverlappingVertices.FindOrAdd(VertexIndex0);
					SrcValueArray.Add(VertexIndex1);

					TArray<int32>& IterValueArray = Section.OverlappingVertices.FindOrAdd(VertexIndex1);
					IterValueArray.Add(VertexIndex0);
				}
			}
		}

		// Copy to user section data, otherwise the section data set above would get lost when the user section gets synced
		FSkelMeshSourceSectionUserData::GetSourceSectionUserData(LODModel.UserSectionsData, Section);
	}


	// Update the active bone indices on the LOD model
	LODModel.ActiveBoneIndices = ActiveBoneIndices.Array();

	// Ensure parent exists with incoming active bone indices, and the result should be sorted
	ClothAsset.RefSkeleton.EnsureParentsExistAndSort(LODModel.ActiveBoneIndices);

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel, ClothAsset.RefSkeleton, nullptr);
}
