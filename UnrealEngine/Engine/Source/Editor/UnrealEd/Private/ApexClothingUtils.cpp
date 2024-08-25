// Copyright Epic Games, Inc. All Rights Reserved.

#include "ApexClothingUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "EditorDirectories.h"
#include "Engine/SkeletalMesh.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SkeletalMeshModel.h"

#include "ClothingAssetFactory.h"
#include "PhysicsPublic.h"

DEFINE_LOG_CATEGORY_STATIC(LogApexClothingUtils, Log, All);

#define LOCTEXT_NAMESPACE "ApexClothingUtils"

namespace ApexClothingUtils
{

//enforces a call of "OnRegister" to update vertex factories
void ReregisterSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->GetSkeletalMeshAsset() == SkelMesh )
		{
			MeshComponent->ReregisterComponent();
		}
	}
}

void RefreshSkelMeshComponents(USkeletalMesh* SkelMesh)
{
	for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
	{
		USkeletalMeshComponent* MeshComponent = *It;
		if( MeshComponent && 
			!MeshComponent->IsTemplate() &&
			MeshComponent->GetSkeletalMeshAsset() == SkelMesh )
		{
			MeshComponent->RecreateRenderState_Concurrent();
		}
	}
}

void RestoreAllClothingSections(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 AssetIndex)
{
	if(FSkeletalMeshModel* Resource = SkelMesh->GetImportedModel())
	{
		for(FSkeletalMeshLODModel& LodModel : Resource->LODModels)
		{
			for(FSkelMeshSection& Section : LodModel.Sections)
			{
				if(Section.HasClothingData())
				{
					ClothingAssetUtils::ClearSectionClothingData(Section);
					if (FSkelMeshSourceSectionUserData* UserSectionData = LodModel.UserSectionsData.Find(Section.OriginalDataSectionIndex))
					{
						UserSectionData->CorrespondClothAssetIndex = INDEX_NONE;
						UserSectionData->ClothingData.AssetLodIndex = INDEX_NONE;
						UserSectionData->ClothingData.AssetGuid = FGuid();
					}
				}
			}
		}
	}
}

void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bReleaseAsset, bool bRecreateSkelMeshComponent)
{
	FSkeletalMeshModel* ImportedResource= SkelMesh->GetImportedModel();
	int32 NumLODs = ImportedResource->LODModels.Num();

	for(int32 LODIdx=0; LODIdx < NumLODs; LODIdx++)
	{
		RestoreAllClothingSections(SkelMesh, LODIdx, AssetIndex);
	}

	SkelMesh->ClothingAssets_DEPRECATED.RemoveAt(AssetIndex);	//have to remove the asset from the array so that new actors are not created for asset pending deleting

	SkelMesh->PostEditChange(); // update derived data

	ReregisterSkelMeshComponents(SkelMesh);

	if(bRecreateSkelMeshComponent)
	{
		// Refresh skeletal mesh components
		RefreshSkelMeshComponents(SkelMesh);
	}
}

} // namespace ApexClothingUtils

#undef LOCTEXT_NAMESPACE
