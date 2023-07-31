// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealBakeHelpers.h"

#include "BoneIndices.h"
#include "ClothLODData.h"
#include "ClothPhysicalMeshData.h"
#include "ClothingAsset.h"
#include "ClothingAssetBase.h"
#include "Containers/Array.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "GPUSkinPublicDefs.h"
#include "HAL/PlatformCrt.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "PackedNormal.h"
#include "PointWeightMap.h"
#include "RawIndexBuffer.h"
#include "Rendering/MultiSizeIndexContainer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "SkeletalMeshTypes.h"
#include "StaticMeshResources.h"
#include "Templates/Casts.h"
#include "Templates/Decay.h"
#include "Templates/UnrealTemplate.h"


//-------------------------------------------------------------------------------------------------
void FUnrealBakeHelpers::BakeHelper_RegenerateImportedModel(USkeletalMesh* SkeletalMesh)
{
#if WITH_EDITORONLY_DATA
	FSkeletalMeshRenderData* SkelResource = SkeletalMesh->GetResourceForRendering();
	if (!SkelResource)
	{
		return;
	}
	
	for (UClothingAssetBase* ClothingAssetBase : SkeletalMesh->GetMeshClothingAssets())
	{
		if (!ClothingAssetBase)
		{
			continue;
		}

		UClothingAssetCommon* ClothAsset = Cast<UClothingAssetCommon>(ClothingAssetBase);

		if (!ClothAsset)
		{
			continue;
		}

		if (!ClothAsset->LodData.Num())
		{
			continue;
		}

		for ( FClothLODDataCommon& ClothLodData : ClothAsset->LodData )
		{
			ClothLodData.PointWeightMaps.Empty(16);
			for (TPair<uint32, FPointWeightMap>& WeightMap : ClothLodData.PhysicalMeshData.WeightMaps)
			{
				if (WeightMap.Value.Num())
				{
					FPointWeightMap& PointWeightMap = ClothLodData.PointWeightMaps.AddDefaulted_GetRef();
					PointWeightMap.Initialize(WeightMap.Value, WeightMap.Key);
				}
			}
		}
	}

	FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
	ImportedModel->bGuidIsHash = false;
	ImportedModel->SkeletalMeshModelGUID = FGuid::NewGuid();

	ImportedModel->LODModels.Empty();

	int32 OriginalIndex = 0;
	for (int32 LODIndex = 0; LODIndex < SkelResource->LODRenderData.Num(); ++LODIndex)
	{
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());

		FSkeletalMeshLODRenderData& LODModel = SkelResource->LODRenderData[LODIndex];
		int32 CurrentSectionInitialVertex = 0;

		ImportedModel->LODModels[LODIndex].ActiveBoneIndices = LODModel.ActiveBoneIndices;
		ImportedModel->LODModels[LODIndex].NumTexCoords = LODModel.GetNumTexCoords();
		ImportedModel->LODModels[LODIndex].RequiredBones = LODModel.RequiredBones;
		ImportedModel->LODModels[LODIndex].NumVertices = LODModel.GetNumVertices();

		// Indices
		int indexCount = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();
		ImportedModel->LODModels[LODIndex].IndexBuffer.SetNum(indexCount);
		for (int i = 0; i < indexCount; ++i)
		{
			ImportedModel->LODModels[LODIndex].IndexBuffer[i] = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get(i);
		}

		ImportedModel->LODModels[LODIndex].Sections.SetNum(LODModel.RenderSections.Num());

		for (int SectionIndex = 0; SectionIndex < LODModel.RenderSections.Num(); ++SectionIndex)
		{
			const FSkelMeshRenderSection& RenderSection = LODModel.RenderSections[SectionIndex];
			FSkelMeshSection& ImportedSection = ImportedModel->LODModels[LODIndex].Sections[SectionIndex];

			ImportedSection.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
			ImportedSection.ClothingData = RenderSection.ClothingData;
		
			if (RenderSection.ClothMappingDataLODs.Num())
			{
				ImportedSection.ClothMappingDataLODs.SetNum(1);
				ImportedSection.ClothMappingDataLODs[0] = RenderSection.ClothMappingDataLODs[0];
			}

			// Vertices
			ImportedSection.NumVertices = RenderSection.NumVertices;
			ImportedSection.SoftVertices.Empty(RenderSection.NumVertices);
			ImportedSection.SoftVertices.AddUninitialized(RenderSection.NumVertices);
			ImportedSection.bUse16BitBoneIndex = LODModel.DoesVertexBufferUse16BitBoneIndex();

			for (uint32 i = 0; i < RenderSection.NumVertices; ++i)
			{
				const FPositionVertex* PosPtr = static_cast<const FPositionVertex*>(LODModel.StaticVertexBuffers.PositionVertexBuffer.GetVertexData());
				PosPtr += (CurrentSectionInitialVertex + i);

				check(!LODModel.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis());
				const FPackedNormal* TangentPtr = static_cast<const FPackedNormal*>(LODModel.StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentData());
				TangentPtr += ((CurrentSectionInitialVertex + i) * 2);

				check(LODModel.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs());
 
                using UVsVectorType = typename TDecay<decltype(DeclVal<FSoftSkinVertex>().UVs[0])>::Type;

				const UVsVectorType* TexCoordPosPtr = static_cast<const UVsVectorType*>(LODModel.StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordData());
				const uint32 NumTexCoords = LODModel.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
				TexCoordPosPtr += ((CurrentSectionInitialVertex + i) * NumTexCoords);

				FSoftSkinVertex& Vertex = ImportedSection.SoftVertices[i];
				for (int32 j = 0; j < RenderSection.MaxBoneInfluences; ++j)
				{
					Vertex.InfluenceBones[j] = LODModel.SkinWeightVertexBuffer.GetBoneIndex(CurrentSectionInitialVertex + i, j);
					Vertex.InfluenceWeights[j] = LODModel.SkinWeightVertexBuffer.GetBoneWeight(CurrentSectionInitialVertex + i, j);
				}
				
				for (int32 j = RenderSection.MaxBoneInfluences; j < MAX_TOTAL_INFLUENCES; ++j)
				{
					Vertex.InfluenceBones[j] = 0;
					Vertex.InfluenceWeights[j] = 0;
				}


				Vertex.Color = FColor::White;

				Vertex.Position = PosPtr->Position;

				Vertex.TangentX = TangentPtr[0].ToFVector3f();
				Vertex.TangentZ = TangentPtr[1].ToFVector3f();
				float TangentSign = TangentPtr[1].Vector.W == 0 ? -1.f : 1.f;
				Vertex.TangentY = FVector3f::CrossProduct(Vertex.TangentZ, Vertex.TangentX) * TangentSign;

				Vertex.UVs[0] = TexCoordPosPtr[0];
				Vertex.UVs[1] = NumTexCoords > 1 ? TexCoordPosPtr[1] : UVsVectorType::ZeroVector;
				Vertex.UVs[2] = NumTexCoords > 2 ? TexCoordPosPtr[2] : UVsVectorType::ZeroVector;
				Vertex.UVs[3] = NumTexCoords > 3 ? TexCoordPosPtr[3] : UVsVectorType::ZeroVector;
			}

			CurrentSectionInitialVertex += RenderSection.NumVertices;

			// Triangles
			ImportedSection.NumTriangles = RenderSection.NumTriangles;
			ImportedSection.BaseIndex = RenderSection.BaseIndex;
			ImportedSection.BaseVertexIndex = RenderSection.BaseVertexIndex;
			ImportedSection.BoneMap = RenderSection.BoneMap;
			ImportedSection.MaterialIndex = RenderSection.MaterialIndex;
			ImportedSection.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
			ImportedSection.OriginalDataSectionIndex = OriginalIndex++;

			FSkelMeshSourceSectionUserData& SectionUserData = ImportedModel->LODModels[LODIndex].UserSectionsData.FindOrAdd(ImportedSection.OriginalDataSectionIndex);

			SectionUserData.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
			SectionUserData.ClothingData.AssetGuid = RenderSection.ClothingData.AssetGuid;
			SectionUserData.ClothingData.AssetLodIndex = RenderSection.ClothingData.AssetLodIndex;
		}

		ImportedModel->LODModels[LODIndex].SyncronizeUserSectionsDataArray();

		// DDC keys
		const USkeletalMeshLODSettings* LODSettings = SkeletalMesh->GetLODSettings();
		const bool bValidLODSettings = LODSettings && LODSettings->GetNumberOfSettings() > LODIndex;
		const FSkeletalMeshLODGroupSettings* SkeletalMeshLODGroupSettings = bValidLODSettings ? &LODSettings->GetSettingsForLODLevel(LODIndex) : nullptr;

		FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
		LODInfo->BuildGUID = LODInfo->ComputeDeriveDataCacheKey(SkeletalMeshLODGroupSettings);

		ImportedModel->LODModels[LODIndex].BuildStringID = ImportedModel->LODModels[LODIndex].GetLODModelDeriveDataKey();

	}
#endif
}
