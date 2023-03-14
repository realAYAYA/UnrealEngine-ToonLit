// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorSkeletalMeshLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EditorScriptingUtils.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "LODUtilities.h"
#include "Misc/CoreMisc.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkeletalMeshEditorSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorSkeletalMeshLibrary)

bool UDEPRECATED_EditorSkeletalMeshLibrary::RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount /*= 0*/, bool bRegenerateEvenIfImported /*= false*/, bool bGenerateBaseLOD /*= false*/)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return FLODUtilities::RegenerateLOD(SkeletalMesh, GetTargetPlatformManagerRef().GetRunningTargetPlatform(), NewLODCount, bRegenerateEvenIfImported, bGenerateBaseLOD);
}

int32 UDEPRECATED_EditorSkeletalMeshLibrary::GetNumVerts(USkeletalMesh* SkeletalMesh, int32 LODIndex)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->GetNumVerts(SkeletalMesh, LODIndex) : 0;
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::RenameSocket(USkeletalMesh* SkeletalMesh, FName OldName, FName NewName)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->RenameSocket(SkeletalMesh, OldName, NewName) : 0;
}
int32 UDEPRECATED_EditorSkeletalMeshLibrary::GetLODCount(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->GetLODCount(SkeletalMesh) : INDEX_NONE;
}

int32 UDEPRECATED_EditorSkeletalMeshLibrary::ImportLOD(USkeletalMesh* BaseMesh, const int32 LODIndex, const FString& SourceFilename)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->ImportLOD(BaseMesh, LODIndex, SourceFilename) : INDEX_NONE;
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::ReimportAllCustomLODs(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->ReimportAllCustomLODs(SkeletalMesh) : false;
}

void UDEPRECATED_EditorSkeletalMeshLibrary::GetLodBuildSettings(const USkeletalMesh* SkeletalMesh, const int32 LodIndex, FSkeletalMeshBuildSettings& OutBuildOptions)
{
	if (USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>())
	{
		SkeletalMeshEditorSubsystem->GetLodBuildSettings(SkeletalMesh, LodIndex, OutBuildOptions);
	}
}

void UDEPRECATED_EditorSkeletalMeshLibrary::SetLodBuildSettings(USkeletalMesh* SkeletalMesh, const int32 LodIndex, const FSkeletalMeshBuildSettings& BuildOptions)
{
	if (USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>())
	{
		SkeletalMeshEditorSubsystem->SetLodBuildSettings(SkeletalMesh, LodIndex, BuildOptions);
	}
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::RemoveLODs(USkeletalMesh* SkeletalMesh, TArray<int32> ToRemoveLODs)
{
	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkeletalMesh;
	int32 OriginalLODNumber = SkeletalMesh->GetLODNum();

	// Close the mesh editor to be sure the editor is showing the correct data after the LODs are removed.
	bool bMeshIsEdited = false;
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem->FindEditorForAsset(SkeletalMesh, false))
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(SkeletalMesh);
		bMeshIsEdited = true;
	}

	// Now iterate over all skeletal mesh components to add them to the UpdateContext
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if (SkelComp->GetSkeletalMeshAsset() == SkeletalMesh)
		{
			UpdateContext.AssociatedComponents.Add(SkelComp);
		}
	}

	FLODUtilities::RemoveLODs(UpdateContext, ToRemoveLODs);

	if (bMeshIsEdited)
	{
		AssetEditorSubsystem->OpenEditorForAsset(SkeletalMesh);
	}

	int32 FinalLODNumber = SkeletalMesh->GetLODNum();
	return (OriginalLODNumber-FinalLODNumber == ToRemoveLODs.Num());
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::StripLODGeometry(USkeletalMesh* SkeletalMesh, const int32 LODIndex, UTexture2D* TextureMask, const float Threshold)
{
	return FLODUtilities::StripLODGeometry(SkeletalMesh, LODIndex, TextureMask, Threshold);
}

UPhysicsAsset* UDEPRECATED_EditorSkeletalMeshLibrary::CreatePhysicsAsset(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->CreatePhysicsAsset(SkeletalMesh) : nullptr;
}

