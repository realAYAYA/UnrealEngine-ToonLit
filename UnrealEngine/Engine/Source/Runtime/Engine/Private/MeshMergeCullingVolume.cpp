// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/MeshMergeCullingVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshMergeCullingVolume)

AMeshMergeCullingVolume::AMeshMergeCullingVolume(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{
	bNotForClientOrServer = true;

	bColored = true;
	BrushColor = FColor(45, 225, 45);
}


