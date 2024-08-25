// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextMeshComponent.h"
#include "GenerationTools.h"
#include "Engine/World.h"
#include "Graph/AnimNext_LODPose.h"
#include "ReferencePose.h"

UAnimNextMeshComponent::UAnimNextMeshComponent()
{
	PrimaryComponentTick.bRunOnAnyThread = true;
}

void UAnimNextMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// Simple tick mimicking a minimal tick of SkinnedMeshComponent
	bRecentlyRendered = (GetLastRenderTime() > GetWorld()->TimeSeconds - 1.0f);
	UpdateLODStatus();
}

void UAnimNextMeshComponent::CompleteAndDispatch(TConstArrayView<FBoneIndexType> InParentIndices, TConstArrayView<FBoneIndexType> InRequiredBoneIndices, TConstArrayView<FTransform> InLocalSpaceTransforms)
{
	// Fill the component space transform buffer
	TArrayView<FTransform> ComponentSpaceTransforms = GetEditableComponentSpaceTransforms();
	if(ComponentSpaceTransforms.Num() > 0)
	{
		UE::AnimNext::FGenerationTools::ConvertLocalSpaceToComponentSpace(InParentIndices, InLocalSpaceTransforms, InRequiredBoneIndices, ComponentSpaceTransforms);

		// Flag buffer for flip
		bNeedToFlipSpaceBaseBuffers = true;
		bHasValidBoneTransform = false;
		FlipEditableSpaceBases();
		bHasValidBoneTransform = true;

		InvalidateCachedBounds();
		UpdateBounds();

		// Send updated transforms to the renderer
		SendRenderDynamicData_Concurrent();
	}
}

FAnimNextGraphReferencePose UAnimNextMeshComponent::GetReferencePose()
{
	UE::AnimNext::FDataHandle RefPoseHandle = UE::AnimNext::FDataRegistry::Get()->GetOrGenerateReferencePose(this);
	return FAnimNextGraphReferencePose(RefPoseHandle);
}