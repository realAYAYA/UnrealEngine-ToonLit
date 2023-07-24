// Copyright Epic Games, Inc. All Rights Reserved.
#include "AnimToTextureSkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace AnimToTexture_Private
{

int32 GetVertices(const UStaticMesh* StaticMesh, const int32 LODIndex,
	TArray<FVector3f>& OutPositions, TArray<FVector3f>& OutNormals)
{
	check(StaticMesh);
	OutPositions.Reset();
	OutNormals.Reset();

	if (!StaticMesh->IsMeshDescriptionValid(LODIndex))
	{
		return INDEX_NONE;
	}

	// Get Mesh Description
	const FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	check(MeshDescription);
	
	// Get MeshDescription Vertices
	const FVertexArray& Vertices = MeshDescription->Vertices();
	const int32 NumVertices = Vertices.Num();
	OutPositions.SetNum(NumVertices);
	OutNormals.SetNumZeroed(NumVertices);
	
	for (const FVertexID& VertexID : Vertices.GetElementIDs())
	{
		const int32 VertexIndex = VertexID.GetValue();

		// Get Vertex Position
		OutPositions[VertexIndex] = MeshDescription->GetVertexPosition(VertexID);

		// Get Avg Vertex Normals
		const TArrayView<const FVertexInstanceID> VertexInstances = MeshDescription->GetVertexVertexInstanceIDs(VertexID);
		for (const FVertexInstanceID VertexInstanceID : VertexInstances)
		{
			const FVector3f VertexInstanceNormal = MeshDescription->VertexInstanceAttributes().GetAttribute<FVector3f>(VertexInstanceID, MeshAttribute::VertexInstance::Normal);
			OutNormals[VertexIndex] += VertexInstanceNormal / VertexInstances.Num();
		}
	}
	
	return NumVertices;
}

int32 GetVertices(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, 
	TArray<FVector3f>& OutPositions)
{
	check(SkeletalMesh);
	OutPositions.Reset();
	
	const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	check(RenderData);

	if (!RenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		return INDEX_NONE;
	}

	// Get LOD Data
	const FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];

	// Get Total Num of Vertices (for all sections)
	const int32 NumVertices = LODRenderData.GetNumVertices();
	OutPositions.SetNumUninitialized(NumVertices);
	
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		OutPositions[VertexIndex] = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);
	};

	return NumVertices;
}

int32 GetTriangles(const USkeletalMesh* SkeletalMesh, const int32 LODIndex,
	TArray<FIntVector3>& OutTriangles)
{
	check(SkeletalMesh);
	OutTriangles.Reset();

	const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	check(RenderData);

	if (!RenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		return INDEX_NONE;
	}

	// Get LOD Data
	const FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];

	// Get Indices
	const FMultiSizeIndexContainer& IndexBuffer = LODRenderData.MultiSizeIndexContainer;
	TArray<uint32> IndexArray;
	IndexBuffer.GetIndexBuffer(IndexArray);

	// Allocate Triangles
	const uint32 NumTriangles = IndexArray.Num() / 3;
	OutTriangles.SetNumUninitialized(NumTriangles);

	for (uint32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
	{
		OutTriangles[TriangleIndex].X = IndexArray[TriangleIndex * 3];
		OutTriangles[TriangleIndex].Y = IndexArray[TriangleIndex * 3 + 1];
		OutTriangles[TriangleIndex].Z = IndexArray[TriangleIndex * 3 + 2];
	}

	return NumTriangles;
};

int32 GetNumBones(const USkeletalMesh* SkeletalMesh)
{
	check(SkeletalMesh);
	
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	return RefSkeleton.GetRawBoneNum();
}

/**
* Copied from FAnimationRuntime::GetComponentSpaceTransform
*/
void GetRefBoneTransforms(const USkeletalMesh* SkeletalMesh, TArray<FTransform>& Transforms)
{
	check(SkeletalMesh);
	Transforms.Reset();

	// Get Reference Skeleton
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const TArray<FTransform>& BoneSpaceTransforms = RefSkeleton.GetRawRefBonePose(); // Get only raw bones (no virtual)
	
	const int32 NumTransforms = BoneSpaceTransforms.Num();
	Transforms.SetNumUninitialized(NumTransforms);

	for (int32 BoneIndex = 0; BoneIndex < NumTransforms; ++BoneIndex)
	{
		// initialize to identity since some of them don't have tracks
		int32 IterBoneIndex = BoneIndex;
		FTransform ComponentSpaceTransform = BoneSpaceTransforms[BoneIndex];

		do
		{
			//const int32 ParentIndex = RefSkeleton.GetParentIndex(IterBoneIndex);
			const int32 ParentIndex = RefSkeleton.GetRawParentIndex(IterBoneIndex); // Get only raw bones (no virtual)
			if (ParentIndex != INDEX_NONE)
			{
				ComponentSpaceTransform *= BoneSpaceTransforms[ParentIndex];
			}

			IterBoneIndex = ParentIndex;
		} while (RefSkeleton.IsValidIndex(IterBoneIndex));

		// 
		Transforms[BoneIndex] = ComponentSpaceTransform;
	}
}

bool HasBone(const USkeletalMesh* SkeletalMesh, const FName& Bone)
{
	check(SkeletalMesh);
	
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();

	for (const FMeshBoneInfo& BoneInfo: RefSkeleton.GetRawRefBoneInfo())
	{
		if (BoneInfo.Name == Bone)
		{
			return true;
		}
	}
	return false;
}


void GetBoneNames(const USkeletalMesh* SkeletalMesh, TArray<FName>& OutNames)
{
	check(SkeletalMesh);
	OutNames.Reset();
	
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	
	const int32 NumRawBones = RefSkeleton.GetRawBoneNum();
	OutNames.SetNumUninitialized(NumRawBones);

	for (int32 BoneIndex = 0; BoneIndex < NumRawBones; ++BoneIndex)
	{
		OutNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);
	}
};

void GetSkinWeights(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, 
	TArray<VertexSkinWeightMax>& OutSkinWeights)
{
	check(SkeletalMesh);
	OutSkinWeights.Reset();

	// Get Render Data
	const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	check(RenderData);

	if (!RenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid LODIndex: %i"), LODIndex)
		return;
	}

	// Get LOD Data
	const FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];

	// Get Weights
	const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = LODRenderData.GetSkinWeightVertexBuffer();
	check(SkinWeightVertexBuffer);

	// Get Weights from Buffer.
	// this is size of number of vertices.
	TArray<FSkinWeightInfo> SkinWeightsInfo;
	SkinWeightVertexBuffer->GetSkinWeights(SkinWeightsInfo);

	// Allocated SkinWeightData
	OutSkinWeights.SetNumUninitialized(SkinWeightsInfo.Num());

	// Loop thru vertices
	for (int32 VertexIndex = 0; VertexIndex < SkinWeightsInfo.Num(); VertexIndex++)
	{
		// Find Section From Global Vertex Index
		// NOTE: BoneMap is stored by Section.
		int32 OutSectionIndex;
		int32 OutSectionVertexIndex;
		LODRenderData.GetSectionFromVertexIndex(VertexIndex, OutSectionIndex, OutSectionVertexIndex);

		// Get Section for Vertex.
		const FSkelMeshRenderSection& RenderSection = LODRenderData.RenderSections[OutSectionIndex];
		
		// Get Vertex Weights
		const FSkinWeightInfo& SkinWeightInfo = SkinWeightsInfo[VertexIndex];

		// Store Weights
		for (int32 Index = 0; Index < MAX_TOTAL_INFLUENCES; Index++)
		{
			const uint8& BoneWeight = SkinWeightInfo.InfluenceWeights[Index];
			const uint16& BoneIndex = SkinWeightInfo.InfluenceBones[Index];
			const uint16& MeshBoneIndex = RenderSection.BoneMap[BoneIndex];

			OutSkinWeights[VertexIndex].BoneWeights[Index] = BoneWeight;
			OutSkinWeights[VertexIndex].MeshBoneIndices[Index] = MeshBoneIndex;
		}
	}
}

void ReduceSkinWeights(const TArray<VertexSkinWeightMax>& InSkinWeights, 
	TArray<VertexSkinWeightFour>& OutSkinWeights)
{
	OutSkinWeights.SetNumUninitialized(InSkinWeights.Num());

	for (int32 VertexIndex = 0; VertexIndex < InSkinWeights.Num(); VertexIndex++)
	{
		const VertexSkinWeightMax& InVertexSkinWeight = InSkinWeights[VertexIndex];
		VertexSkinWeightFour& OutVertexSkinWeight = OutSkinWeights[VertexIndex];

		float TotalBoneWeight = 0.f;
		for (int32 Index = 0; Index < 4; Index++)
		{
			TotalBoneWeight += (float)InVertexSkinWeight.BoneWeights[Index] / 255.f;
		}

		// We assume Weights are sorted
		for (int32 Index = 0; Index < 4; Index++)
		{
			OutVertexSkinWeight.BoneWeights[Index] = (uint8)FMath::RoundToInt((float)InVertexSkinWeight.BoneWeights[Index] / TotalBoneWeight);
			OutVertexSkinWeight.MeshBoneIndices[Index] = InVertexSkinWeight.MeshBoneIndices[Index];
		}
	}
}

void GetSkinnedVertices(const USkeletalMeshComponent* SkeletalMeshComponent, const int32 LODIndex,
	TArray<FVector3f>& OutPositions)
{
	check(SkeletalMeshComponent);
	OutPositions.Reset();

	// Get SkeletalMesh
	const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
	check(SkeletalMesh);

	// Get Matrices
	TArray<FMatrix44f> RefToLocals;
	SkeletalMeshComponent->CacheRefToLocalMatrices(RefToLocals);

	// Get Ref-Pose Vertices
	TArray<FVector3f> Vertices;
	const int32 NumVertices = GetVertices(SkeletalMesh, LODIndex, Vertices);
	OutPositions.SetNumUninitialized(NumVertices);

	// TODO: Add Morph Deltas to Vertices.

	// Get Weights
	TArray<VertexSkinWeightMax> SkinWeights;
	GetSkinWeights(SkeletalMesh, LODIndex, SkinWeights);

	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		const FVector3f& Vertex = Vertices[VertexIndex];
		const VertexSkinWeightMax& Weights = SkinWeights[VertexIndex];

		FVector4f SkinnedVertex(0);
		for (int32 Index = 0; Index < MAX_TOTAL_INFLUENCES; Index++)
		{
			const uint8& BoneWeight = Weights.BoneWeights[Index];
			const uint16& MeshBoneIndex = Weights.MeshBoneIndices[Index];

			// Get Matrix
			const FMatrix44f& RefToLocal = RefToLocals[MeshBoneIndex];

			const float Weight = (float)BoneWeight / 255.f;
			SkinnedVertex += RefToLocal.TransformPosition(Vertex) * Weight;
		}

		OutPositions[VertexIndex] = SkinnedVertex;
	};
};

} // end namespace AnimToTexture_Private
