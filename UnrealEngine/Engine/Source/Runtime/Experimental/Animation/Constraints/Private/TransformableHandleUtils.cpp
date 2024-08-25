// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformableHandleUtils.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace TransformableHandleUtils
{
	
void TickDependantComponents(const USceneComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	static constexpr bool bIncludeFromChildActors = true;

	const AActor* Parent = InComponent->GetOwner();
	while (Parent)
	{
		Parent->ForEachComponent<USkeletalMeshComponent>(bIncludeFromChildActors, &TickSkeletalMeshComponent);
		Parent = Parent->GetAttachParentActor();
	}
}

void TickSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (!InSkeletalMeshComponent)
	{
		return;
	}

	// avoid re-entrant animation evaluation
	if (InSkeletalMeshComponent->IsPostEvaluatingAnimation())
	{
		return;
	}

	static constexpr float DeltaTime = 0.03f;
	static constexpr bool bNeedsValidRootMotion = false;
	
	InSkeletalMeshComponent->TickAnimation(DeltaTime, bNeedsValidRootMotion);
	InSkeletalMeshComponent->RefreshBoneTransforms();
	InSkeletalMeshComponent->RefreshFollowerComponents();
	InSkeletalMeshComponent->UpdateComponentToWorld();
	InSkeletalMeshComponent->FinalizeBoneTransform();
	InSkeletalMeshComponent->MarkRenderTransformDirty();
	InSkeletalMeshComponent->MarkRenderDynamicDataDirty();
}
	
}