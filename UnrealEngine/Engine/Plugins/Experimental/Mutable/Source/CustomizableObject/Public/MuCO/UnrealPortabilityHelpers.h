// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "Engine/SkeletalMesh.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkinWeightVertexBuffer.h"

// Main engine branch should have 1 here. 
// Projects with API modifications may set it 0.
#define MUTABLE_CLEAN_ENGINE_BRANCH		1

//-------------------------------------------------------------------------------------------------
// Helpers to ease portability across unreal engine versions
//-------------------------------------------------------------------------------------------------

typedef FSkeletalMeshLODRenderData Helper_LODDataType;
typedef FSkelMeshRenderSection Helper_SkelMeshRenderSection;

inline FSkeletalMeshRenderData* Helper_GetResourceForRendering(const USkeletalMesh* Mesh)
{
	return Mesh->GetResourceForRendering();
}

inline TIndirectArray<FSkeletalMeshLODRenderData>* Helper_GetLODDataPtr(FSkeletalMeshRenderData* Resource)
{
	return &(Resource->LODRenderData);
}

inline TIndirectArray<FSkeletalMeshLODRenderData>& Helper_GetLODData(FSkeletalMeshRenderData* Resource)
{
	return Resource->LODRenderData;
}

inline const TIndirectArray<FSkeletalMeshLODRenderData>& Helper_GetLODData(const FSkeletalMeshRenderData* Resource)
{
	return Resource->LODRenderData;
}

inline TIndirectArray<FSkeletalMeshLODRenderData>& Helper_GetLODData(const USkinnedAsset* Mesh)
{
	return Mesh->GetResourceForRendering()->LODRenderData;
}

inline TArray<FSkelMeshRenderSection>& Helper_GetLODRenderSections(const USkinnedAsset* Mesh, int LODIndex)
{
	return  Mesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections;
}

#if WITH_EDITORONLY_DATA
inline FSkeletalMeshModel* Helper_GetImportedModel(const USkinnedAsset* Mesh)
{
	return Mesh->GetImportedModel();
}
#endif

inline int32 Helper_LODGetTotalFaces(const FSkeletalMeshLODRenderData* Resource)
{
	return Resource->GetTotalFaces();
}

inline uint32 Helper_LODGetNumTexCoords(const FSkeletalMeshLODRenderData* Resource)
{
	return Resource->GetNumTexCoords();
}

inline const TArray<FSkelMeshRenderSection>* Helper_LODGetSections(const FSkeletalMeshLODRenderData* Resource)
{
	return &(Resource->RenderSections);
}

inline TArray<struct FSkeletalMeshLODInfo>& Helper_GetLODInfoArray(USkinnedAsset* Mesh)
{
	return Mesh->GetLODInfoArray();
}

inline const TArray<struct FSkeletalMeshLODInfo>& Helper_GetLODInfoArray(const USkinnedAsset* Mesh)
{
	return Mesh->GetLODInfoArray();
}

inline int Helper_GetLODNum(const USkinnedAsset* Mesh)
{
	return Mesh->GetLODNum();
}

inline const TArray<FBoneReference>& Helper_GetLODInfoBonesToRemove(const USkinnedAsset* Mesh, int LODIndex)
{
	return Mesh->GetLODInfo(LODIndex)->BonesToRemove;
}

inline TArray<FBoneReference>& Helper_GetLODInfoBonesToRemove(USkinnedAsset* Mesh, int LODIndex)
{
	return Mesh->GetLODInfo(LODIndex)->BonesToRemove;
}

inline FString Helper_GetSavedDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
}

