// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshUtilitiesEngine.h"

#include "Engine/SkeletalMesh.h"
#include "GPUSkinPublicDefs.h"
#include "MeshUtilitiesCommon.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

// Find the most dominant bone for each vertex
int32 GetDominantBoneIndex(FSoftSkinVertex* SoftVert)
{
	uint16 MaxWeightBone = 0;
	uint16 MaxWeightWeight = 0;

	for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; i++)
	{
		if (SoftVert->InfluenceWeights[i] > MaxWeightWeight)
		{
			MaxWeightWeight = SoftVert->InfluenceWeights[i];
			MaxWeightBone = SoftVert->InfluenceBones[i];
		}
	}

	return MaxWeightBone;
}

void FMeshUtilitiesEngine::CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant)
{
	FSkeletalMeshModel* ImportedResource = SkeletalMesh->GetImportedModel();
	if (ImportedResource->LODModels.Num() == 0)
		return;

	SkeletalMesh->CalculateInvRefMatrices();
	check(SkeletalMesh->GetRefSkeleton().GetRawBoneNum() == SkeletalMesh->GetRefBasesInvMatrix().Num());

	Infos.Empty();
	Infos.AddZeroed(SkeletalMesh->GetRefSkeleton().GetRawBoneNum());

	FSkeletalMeshLODModel* LODModel = &ImportedResource->LODModels[0];
	for (int32 SectionIndex = 0; SectionIndex < LODModel->Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel->Sections[SectionIndex];
		for (int32 i = 0; i < Section.SoftVertices.Num(); i++)
		{
			FSoftSkinVertex* SoftVert = &Section.SoftVertices[i];

			if (bOnlyDominant)
			{
				int32 BoneIndex = Section.BoneMap[GetDominantBoneIndex(SoftVert)];

				FVector3f LocalPos = SkeletalMesh->GetRefBasesInvMatrix()[BoneIndex].TransformPosition(SoftVert->Position);
				Infos[BoneIndex].Positions.Add(LocalPos);

				FVector3f LocalNormal = SkeletalMesh->GetRefBasesInvMatrix()[BoneIndex].TransformVector(SoftVert->TangentZ);
				Infos[BoneIndex].Normals.Add(LocalNormal);
			}
			else
			{
				for (int32 j = 0; j < MAX_TOTAL_INFLUENCES; j++)
				{
					if (SoftVert->InfluenceWeights[j] > 0)
					{
						int32 BoneIndex = Section.BoneMap[SoftVert->InfluenceBones[j]];

						FVector3f LocalPos = SkeletalMesh->GetRefBasesInvMatrix()[BoneIndex].TransformPosition(SoftVert->Position);
						Infos[BoneIndex].Positions.Add(LocalPos);

						FVector3f LocalNormal = SkeletalMesh->GetRefBasesInvMatrix()[BoneIndex].TransformVector(SoftVert->TangentZ);
						Infos[BoneIndex].Normals.Add(LocalNormal);
					}
				}
			}
		}
	}
}