// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkDrivenComponent.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkDrivenComponent)

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UDEPRECATED_LiveLinkDrivenComponent::UDEPRECATED_LiveLinkDrivenComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	bAutoActivate = true;
	
}

void UDEPRECATED_LiveLinkDrivenComponent::OnRegister()
{
	Super::OnRegister();
}

void UDEPRECATED_LiveLinkDrivenComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UDEPRECATED_LiveLinkDrivenComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (ILiveLinkClient* Client = ClientRef.GetClient())
	{
		if (const FLiveLinkSubjectFrame* Frame = Client->GetSubjectData(SubjectName))
		{
			if (bModifyActorTransform)
			{
				int32 ActorTransformIndex = 0;
				if (ActorTransformBone != NAME_None)
				{
					const int32 NewActorTransformBoneIndex = Frame->RefSkeleton.GetBoneNames().IndexOfByKey(ActorTransformBone);
					if (NewActorTransformBoneIndex != INDEX_NONE)
					{
						ActorTransformIndex = NewActorTransformBoneIndex;
					}
				}

				if (Frame->Transforms.IsValidIndex(ActorTransformIndex))
				{
					const FTransform& NewTransform = Frame->Transforms[ActorTransformIndex];
					if (AActor* Actor = GetOwner())
					{
						if (bSetRelativeLocation)
						{
							Actor->SetActorRelativeTransform(NewTransform);
						}
						else
						{
							Actor->SetActorTransform(NewTransform);
						}
					}
				}
			}
		}
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
