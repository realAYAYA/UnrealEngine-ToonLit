// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowContent.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "PreviewScene.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowContent)

UDataflowBaseContent::UDataflowBaseContent()
{
}

void UDataflowBaseContent::SetLastModifiedTimestamp(Dataflow::FTimestamp InTimestamp) 
{ 
	if (InTimestamp.IsInvalid() || LastModifiedTimestamp < InTimestamp)
	{
		LastModifiedTimestamp = InTimestamp; 
		bIsDirty = true;
	}
}

void UDataflowBaseContent::BuildBaseContent(TObjectPtr<UObject> ContentOwner)
{
	DataflowContext = MakeShared<Dataflow::FEngineContext>(ContentOwner, DataflowAsset, FPlatformTime::Cycles64());
	LastModifiedTimestamp = DataflowContext->GetTimestamp();
}

UDataflowSkeletalContent::UDataflowSkeletalContent() : Super()
{
}

void UDataflowSkeletalContent::RegisterWorldContent(FPreviewScene* PreviewScene, AActor* RootActor)
{
	Super::RegisterWorldContent(PreviewScene, RootActor);

	SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(RootActor);
	SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
	
	if(SkeletalMesh)
	{
		SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);

		Skeleton = SkeletalMesh->GetSkeleton();
		UpdateAnimationInstance();
	}
	SkeletalMeshComponent->UpdateBounds();
	PreviewScene->AddComponent(SkeletalMeshComponent, SkeletalMeshComponent->GetRelativeTransform());
}

void UDataflowSkeletalContent::UpdateAnimationInstance()
{
	if (AnimationAsset && SkeletalMesh && (AnimationAsset->GetSkeleton() == SkeletalMesh->GetSkeleton()))
	{
		AnimationNodeInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
		AnimationNodeInstance->SetAnimationAsset(AnimationAsset);

		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkeletalMeshComponent->InitAnim(true);
		SkeletalMeshComponent->AnimationData.PopulateFrom(AnimationNodeInstance);
		SkeletalMeshComponent->AnimScriptInstance = AnimationNodeInstance;
		SkeletalMeshComponent->AnimScriptInstance->InitializeAnimation();

#if WITH_EDITOR
		SkeletalMeshComponent->ValidateAnimation();
#endif
	}
	else
	{
		SkeletalMeshComponent->Stop();
		SkeletalMeshComponent->AnimationData = FSingleAnimationPlayData();
		SkeletalMeshComponent->AnimScriptInstance = nullptr;
		AnimationNodeInstance = nullptr;
		AnimationAsset = nullptr;
	}
}
	
void UDataflowSkeletalContent::UnregisterWorldContent(FPreviewScene* PreviewScene)
{
	Super::UnregisterWorldContent(PreviewScene);
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->TransformUpdated.RemoveAll(this);
		PreviewScene->RemoveComponent(SkeletalMeshComponent);
	}
}

void UDataflowSkeletalContent::SetSkeletalMesh(const TObjectPtr<USkeletalMesh>& SkeletalMeshAsset)
{
	SkeletalMesh = SkeletalMeshAsset;
	if(SkeletalMesh && SkeletalMesh->GetSkeleton())
	{
		Skeleton = SkeletalMesh->GetSkeleton();
	}
	if(SkeletalMeshComponent)
	{
 		SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);

 		UpdateAnimationInstance();
	}
	bIsDirty = true;
}

void UDataflowSkeletalContent::SetAnimationAsset(const TObjectPtr<UAnimationAsset>& SkeletalAnimationAsset)
{
	AnimationAsset = SkeletalAnimationAsset;
	if(SkeletalMeshComponent)
	{
 		UpdateAnimationInstance();
	}
	bIsDirty = true;
}

void UDataflowSkeletalContent::SetSkeleton(const TObjectPtr<USkeleton>& SkeletonAsset)
{
	if(SkeletonAsset)
	{
		Skeleton = SkeletonAsset;
		if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
		{
			SetSkeletalMesh(nullptr);
		}
	}
	bIsDirty = true;
}

#if WITH_EDITOR

void UDataflowSkeletalContent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, SkeletalMesh))
	{
		SetSkeletalMesh(SkeletalMesh);
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, AnimationAsset))
	{
		SetAnimationAsset(AnimationAsset);
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, Skeleton))
	{
		SetSkeleton(Skeleton);
	}
}
#endif //if WITH_EDITOR

void UDataflowSkeletalContent::AddContentObjects(FReferenceCollector& Collector)
{
	Super::AddContentObjects(Collector);
	
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(AnimationNodeInstance);
}

FVector2f UDataflowSkeletalContent::GetSimulationRange() const
{
	if(AnimationNodeInstance)
	{
		return FVector2f(0.0f, AnimationNodeInstance->GetLength());
	}
	return FVector2f(0.0f, 0.0f);
}


