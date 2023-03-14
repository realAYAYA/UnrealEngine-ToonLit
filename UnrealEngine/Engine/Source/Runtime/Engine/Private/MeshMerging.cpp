// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/MeshMerging.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshMerging)

FMeshInstancingSettings::FMeshInstancingSettings()
	: ActorClassToUse(AActor::StaticClass())
	, InstanceReplacementThreshold(2)
	, bSkipMeshesWithVertexColors(true)
	, bUseHLODVolumes(true)
	, ISMComponentToUse(UInstancedStaticMeshComponent::StaticClass())
{
}

