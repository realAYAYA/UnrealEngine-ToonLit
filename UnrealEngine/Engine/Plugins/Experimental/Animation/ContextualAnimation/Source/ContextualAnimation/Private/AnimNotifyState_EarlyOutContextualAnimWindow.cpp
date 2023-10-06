// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_EarlyOutContextualAnimWindow.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ContextualAnimSceneActorComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_EarlyOutContextualAnimWindow)

UAnimNotifyState_EarlyOutContextualAnimWindow::UAnimNotifyState_EarlyOutContextualAnimWindow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UAnimNotifyState_EarlyOutContextualAnimWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if(MeshComp)
	{
		ACharacter* CharacterOwner = Cast<ACharacter>(MeshComp->GetOwner());
		if(CharacterOwner && CharacterOwner->IsLocallyControlled())
		{
			if(UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
			{
				if(!MoveComp->GetLastInputVector().IsNearlyZero())
				{
					UContextualAnimSceneActorComponent* Comp = CharacterOwner->FindComponentByClass<UContextualAnimSceneActorComponent>();
					if (Comp)
					{
						Comp->EarlyOutContextualAnimScene();
					}
				}
			}
		}
		
	}
}

FString UAnimNotifyState_EarlyOutContextualAnimWindow::GetNotifyName_Implementation() const
{
	return FString(TEXT("Early Out Contextual Anim"));
}
