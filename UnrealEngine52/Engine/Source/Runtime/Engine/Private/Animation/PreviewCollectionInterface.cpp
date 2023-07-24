// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/PreviewCollectionInterface.h"
#include "Templates/SubclassOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PreviewCollectionInterface)

UPreviewCollectionInterface::UPreviewCollectionInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void IPreviewCollectionInterface::GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList) const
{
	TArray<TSubclassOf<UAnimInstance>> AnimBP;
	GetPreviewSkeletalMeshes(OutList, AnimBP);
}

