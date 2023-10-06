// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileEditorData.h"
#include "PhysicsControlProfileAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "PhysicsControlProfileEditorData"

//======================================================================================================================
FPhysicsControlProfileEditorData::FPhysicsControlProfileEditorData()
{
}

//======================================================================================================================
void FPhysicsControlProfileEditorData::Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	PreviewScene = InPreviewScene;

	EditorSkelComp = nullptr;
	PhysicsControlComponent = nullptr;
	FSoftObjectPath PreviewMeshStringRef = PhysicsControlProfileAsset->PreviewSkeletalMesh.ToSoftObjectPath();

	// Support undo/redo
	PhysicsControlProfileAsset->SetFlags(RF_Transactional);
}

//======================================================================================================================
void FPhysicsControlProfileEditorData::CachePreviewMesh()
{
	USkeletalMesh* PreviewMesh = PhysicsControlProfileAsset->PreviewSkeletalMesh.LoadSynchronous();

	if (PreviewMesh == nullptr)
	{
		// Fall back to the default skeletal mesh in the EngineMeshes package.
		// This is statically loaded as the package is likely not fully loaded
		// (otherwise, it would have been found in the above iteration).
		PreviewMesh = (USkeletalMesh*)StaticLoadObject(USkeletalMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), NULL, LOAD_None, NULL);
		check(PreviewMesh);

		PhysicsControlProfileAsset->PreviewSkeletalMesh = PreviewMesh;

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("Error_PhysicsControlProfileAssetHasNoSkelMesh", "Warning: Physics Control Profile Asset has no skeletal mesh assigned.\nFor now, a simple default skeletal mesh ({0}) will be used.\nYou can fix this by opening the asset and choosing another skeletal mesh from the toolbar."),
			FText::FromString(PreviewMesh->GetFullName())));
	}
	else if (PreviewMesh->GetSkeleton() == nullptr)
	{
		// Fall back in the case of a deleted skeleton
		PreviewMesh = (USkeletalMesh*)StaticLoadObject(USkeletalMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), NULL, LOAD_None, NULL);
		check(PreviewMesh);

		PhysicsControlProfileAsset->PreviewSkeletalMesh = PreviewMesh;

		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("Error_PhysicsControlProfileAssetHasNoSkelMeshSkeleton", "Warning: Physics Control Profile Asset has a skeletal mesh with no skeleton assigned.\nFor now, a simple default skeletal mesh ({0}) will be used.\nYou can fix this by opening the asset and choosing another skeletal mesh from the toolbar, or repairing the skeleton."),
			FText::FromString(PreviewMesh->GetFullName())));
	}
}


#undef LOCTEXT_NAMESPACE
