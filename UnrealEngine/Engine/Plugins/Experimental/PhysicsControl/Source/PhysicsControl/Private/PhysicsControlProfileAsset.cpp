// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileAsset.h"
#include "Engine/SkeletalMesh.h"

//======================================================================================================================
UPhysicsControlProfileAsset::UPhysicsControlProfileAsset()
	: TestValue(1.0f)
{
}

//======================================================================================================================
void UPhysicsControlProfileAsset::Log()
{
	UE_LOG(LogTemp, Log, TEXT("%f"), TestValue)
}

#if WITH_EDITOR
//======================================================================================================================
const FName UPhysicsControlProfileAsset::GetPreviewMeshPropertyName()
{
	return GET_MEMBER_NAME_STRING_CHECKED(UPhysicsControlProfileAsset, PreviewSkeletalMesh);
};
#endif

//======================================================================================================================
void UPhysicsControlProfileAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
	PreviewSkeletalMesh = PreviewMesh;
}

//======================================================================================================================
USkeletalMesh* UPhysicsControlProfileAsset::GetPreviewMesh() const
{
	return PreviewSkeletalMesh.LoadSynchronous();
}
