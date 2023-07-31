// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/SkeletalMeshImportTestFunctions.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshImportTestFunctions)


UClass* USkeletalMeshImportTestFunctions::GetAssociatedAssetType() const
{
	return USkeletalMesh::StaticClass();
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckImportedSkeletalMeshCount(const TArray<USkeletalMesh*>& Meshes, int32 ExpectedNumberOfImportedSkeletalMeshes)
{
	FInterchangeTestFunctionResult Result;
	if (Meshes.Num() != ExpectedNumberOfImportedSkeletalMeshes)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d skeletal meshes, imported %d."), ExpectedNumberOfImportedSkeletalMeshes, Meshes.Num()));
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckRenderVertexCount(USkeletalMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfRenderVertices)
{
	FInterchangeTestFunctionResult Result;

	// @todo: add separate test function for checking the source data vertex count?
	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		int32 RealVertexCount = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].GetNumVertices();
		if (RealVertexCount != ExpectedNumberOfRenderVertices)
		{
			Result.AddError(FString::Printf(TEXT("For LOD %d, expected %d vertices, imported %d."), LodIndex, ExpectedNumberOfRenderVertices, RealVertexCount));
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckLodCount(USkeletalMesh* Mesh, int32 ExpectedNumberOfLods)
{
	FInterchangeTestFunctionResult Result;

	int32 NumLODs = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (NumLODs != ExpectedNumberOfLods)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d LODs, imported %d."), ExpectedNumberOfLods, NumLODs));
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckMaterialSlotCount(USkeletalMesh* Mesh, int32 ExpectedNumberOfMaterialSlots)
{
	FInterchangeTestFunctionResult Result;

	int32 NumMaterials = Mesh->GetMaterials().Num();
	if (NumMaterials != ExpectedNumberOfMaterialSlots)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d materials, imported %d."), ExpectedNumberOfMaterialSlots, NumMaterials));
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckSectionCount(USkeletalMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfSections)
{
	FInterchangeTestFunctionResult Result;

	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		int32 NumSections = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].RenderSections.Num();
		if (NumSections != ExpectedNumberOfSections)
		{
			Result.AddError(FString::Printf(TEXT("For LOD %d, expected %d sections, imported %d."), LodIndex, ExpectedNumberOfSections, NumSections));
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckTriangleCountInSection(USkeletalMesh* Mesh, int32 LodIndex, int32 SectionIndex, int32 ExpectedNumberOfTriangles)
{
	FInterchangeTestFunctionResult Result;

	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		int32 NumSections = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].RenderSections.Num();
		if (SectionIndex >= NumSections)
		{
			Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain section index %d."), SectionIndex));
		}
		else
		{
			int32 NumberOfTriangles = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].RenderSections[SectionIndex].NumTriangles;
			if (NumberOfTriangles != ExpectedNumberOfTriangles)
			{
				Result.AddError(FString::Printf(TEXT("For LOD %d, section index %d, expected %d triangles, imported %d."), LodIndex, SectionIndex, ExpectedNumberOfTriangles, NumberOfTriangles));
			}
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckUVChannelCount(USkeletalMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfUVChannels)
{
	FInterchangeTestFunctionResult Result;

	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		int32 NumUVs = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].GetNumTexCoords();
		if (NumUVs != ExpectedNumberOfUVChannels)
		{
			Result.AddError(FString::Printf(TEXT("For LOD %d, expected %d UVs, imported %d."), LodIndex, ExpectedNumberOfUVChannels, NumUVs));
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckSectionMaterialName(USkeletalMesh* Mesh, int32 LodIndex, int32 SectionIndex, const FString& ExpectedMaterialName)
{
	FInterchangeTestFunctionResult Result;

	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		const TArray<FSkelMeshRenderSection>& Sections = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].RenderSections;
		if (SectionIndex >= Sections.Num())
		{
			Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain section index %d (imported %d)."), SectionIndex, Sections.Num()));
		}
		else
		{
			int32 MaterialIndex = Sections[SectionIndex].MaterialIndex;

			const TArray<FSkeletalMaterial>& SkeletalMaterials = Mesh->GetMaterials();
			if (!SkeletalMaterials.IsValidIndex(MaterialIndex) || SkeletalMaterials[MaterialIndex].MaterialInterface == nullptr)
			{
				Result.AddError(FString::Printf(TEXT("The section references a non-existent material (index %d)."), MaterialIndex));
			}
			else
			{
				FString MaterialName = SkeletalMaterials[MaterialIndex].MaterialInterface->GetName();
				if (MaterialName != ExpectedMaterialName)
				{
					Result.AddError(FString::Printf(TEXT("For LOD %d section %d, expected material name %s, imported %s."), LodIndex, SectionIndex, *ExpectedMaterialName, *MaterialName));
				}
			}
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckVertexIndexPosition(USkeletalMesh* Mesh, int32 LodIndex, int32 VertexIndex, const FVector& ExpectedVertexPosition)
{
	FInterchangeTestFunctionResult Result;

	int32 ImportedLods = Mesh->GetResourceForRendering()->LODRenderData.Num();
	if (LodIndex >= ImportedLods)
	{
		Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain LOD index %d (imported %d)."), LodIndex, ImportedLods));
	}
	else
	{
		int32 VertexCount = Mesh->GetResourceForRendering()->LODRenderData[LodIndex].StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
		if (VertexIndex >= VertexCount)
		{
			Result.AddError(FString::Printf(TEXT("The imported mesh doesn't contain vertex index %d (imported %d)."), VertexIndex, VertexCount));
		}
		else
		{
			FVector VertexPosition = FVector(Mesh->GetResourceForRendering()->LODRenderData[LodIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex));
			if (!VertexPosition.Equals(ExpectedVertexPosition))
			{
				Result.AddError(FString::Printf(TEXT("For LOD %d vertex index %d, expected position %s, imported %s."), LodIndex, VertexIndex, *ExpectedVertexPosition.ToString(), *VertexPosition.ToString()));
			}
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckBoneCount(USkeletalMesh* Mesh, int32 ExpectedNumberOfBones)
{
	FInterchangeTestFunctionResult Result;

	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (Skeleton == nullptr)
	{
		if (ExpectedNumberOfBones != 0)
		{
			Result.AddError(FString::Printf(TEXT("No skeleton found - but expected %d bones"), ExpectedNumberOfBones));
		}
	}
	else
	{
		int32 NumberOfBones = Skeleton->GetReferenceSkeleton().GetNum();
		if (NumberOfBones != ExpectedNumberOfBones)
		{
			Result.AddError(FString::Printf(TEXT("Expected %d bones, imported %d."), ExpectedNumberOfBones, NumberOfBones));
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckBonePosition(USkeletalMesh* Mesh, int32 BoneIndex, const FVector& ExpectedBonePosition)
{
	FInterchangeTestFunctionResult Result;

	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (Skeleton == nullptr)
	{
		Result.AddError(TEXT("No skeleton found."));
	}
	else
	{
		int32 NumberOfBones = Skeleton->GetReferenceSkeleton().GetNum();
		if (BoneIndex >= NumberOfBones)
		{
			Result.AddError(FString::Printf(TEXT("Expected bone index %d, but only imported %d bones."), BoneIndex, NumberOfBones));
		}
		else
		{
			FVector BonePosition = Mesh->GetRefSkeleton().GetRefBonePose()[BoneIndex].GetLocation();
			if (!BonePosition.Equals(ExpectedBonePosition))
			{
				Result.AddError(FString::Printf(TEXT("For bone index %d, expected position %s, imported %s."), BoneIndex, *ExpectedBonePosition.ToString(), *BonePosition.ToString()));
			}
		}
	}

	return Result;
}


FInterchangeTestFunctionResult USkeletalMeshImportTestFunctions::CheckSkinnedVertexCountForBone(USkeletalMesh* Mesh, const FString& BoneName, bool bTestFirstAlternateProfile, int32 ExpectedSkinnedVertexCount)
{
	FInterchangeTestFunctionResult Result;

	int32 BoneIndex = Mesh->GetRefSkeleton().FindBoneIndex(*BoneName);
	if (!Mesh->GetRefSkeleton().IsValidIndex(BoneIndex))
	{
		Result.AddError(FString::Printf(TEXT("Could not find bone '%s'."), *BoneName));
	}
	else
	{
		if (Mesh->GetImportedModel() && Mesh->GetImportedModel()->LODModels.IsValidIndex(0))
		{
			int32 SkinnedVerticesForBone = 0;
			auto IncrementInfluence = [&SkinnedVerticesForBone, &BoneIndex](const FSkelMeshSection& Section,
																			const FBoneIndexType(&InfluenceBones)[MAX_TOTAL_INFLUENCES],
																			const uint8(&InfluenceWeights)[MAX_TOTAL_INFLUENCES])
			{
				for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
				{
					if (InfluenceWeights[InfluenceIndex] == 0)
					{
						// Influences are sorted by weight so no need to go further then a zero weight
						break;
					}
					if (Section.BoneMap[InfluenceBones[InfluenceIndex]] == BoneIndex)
					{
						SkinnedVerticesForBone++;
						break;
					}
				}
			};

			if (!bTestFirstAlternateProfile)
			{
				for (const FSkelMeshSection& Section : Mesh->GetImportedModel()->LODModels[0].Sections)
				{
					const int32 SectionVertexCount = Section.SoftVertices.Num();
					// Find the number of vertices skinned to this bone
					for (int32 SectionVertexIndex = 0; SectionVertexIndex < SectionVertexCount; ++SectionVertexIndex)
					{
						const FSoftSkinVertex& Vertex = Section.SoftVertices[SectionVertexIndex];
						IncrementInfluence(Section, Vertex.InfluenceBones, Vertex.InfluenceWeights);
					}
				}
			}
			else
			{
				if (Mesh->GetSkinWeightProfiles().Num() > 0)
				{
					const int32 TotalVertexCount = Mesh->GetImportedModel()->LODModels[0].NumVertices;
					const FSkinWeightProfileInfo& SkinWeightProfile = Mesh->GetSkinWeightProfiles()[0];
					const FImportedSkinWeightProfileData& SkinWeightData = Mesh->GetImportedModel()->LODModels[0].SkinWeightProfiles.FindChecked(SkinWeightProfile.Name);

					if (SkinWeightData.SkinWeights.Num() != TotalVertexCount)
					{
						Result.AddError(TEXT("Unable to find alternate skinning profile, please uncheck the 'test alternate profile' box."));
					}
					else
					{
						int32 TotalVertexIndex = 0;
						for (const FSkelMeshSection& Section : Mesh->GetImportedModel()->LODModels[0].Sections)
						{
							const int32 SectionVertexCount = Section.SoftVertices.Num();
							//Find the number of vertex skin by this bone
							for (int32 SectionVertexIndex = 0; SectionVertexIndex < SectionVertexCount; ++SectionVertexIndex, ++TotalVertexIndex)
							{
								const FRawSkinWeight& SkinWeight = SkinWeightData.SkinWeights[TotalVertexIndex];
								IncrementInfluence(Section, SkinWeight.InfluenceBones, SkinWeight.InfluenceWeights);
							}
						}
					}
				}
			}

			if (SkinnedVerticesForBone != ExpectedSkinnedVertexCount)
			{
				Result.AddError(FString::Printf(TEXT("For bone '%s', expected %d vertices, imported %d."), *BoneName, ExpectedSkinnedVertexCount, SkinnedVerticesForBone));
			}
		}
		else
		{
			Result.AddError(TEXT("No valid mesh geometry found to find the vertex count"));
		}
	}

	return Result;
}


