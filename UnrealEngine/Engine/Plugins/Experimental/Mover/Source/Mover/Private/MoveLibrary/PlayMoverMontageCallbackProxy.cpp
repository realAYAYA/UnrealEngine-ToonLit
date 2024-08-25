// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/PlayMoverMontageCallbackProxy.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayMoverMontageCallbackProxy)

UPlayMoverMontageCallbackProxy::UPlayMoverMontageCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPlayMoverMontageCallbackProxy* UPlayMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
	class UMoverComponent* InMoverComponent,
	class UAnimMontage* MontageToPlay,
	float PlayRate,
	float StartingPosition,
	FName StartingSection)
{
	USkeletalMeshComponent* SkelMeshComp = InMoverComponent ? InMoverComponent->GetOwner()->GetComponentByClass<USkeletalMeshComponent>() : nullptr;

	UPlayMoverMontageCallbackProxy* Proxy = NewObject<UPlayMoverMontageCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->PlayMoverMontage(InMoverComponent, SkelMeshComp, MontageToPlay, PlayRate, StartingPosition, StartingSection);

	return Proxy;
}


bool UPlayMoverMontageCallbackProxy::PlayMoverMontage(
	UMoverComponent* InMoverComponent,
	USkeletalMeshComponent* InSkeletalMeshComponent,
	UAnimMontage* MontageToPlay,
	float PlayRate,
	float StartingPosition,
	FName StartingSection)
{
	bool bDidPlay = PlayMontage(InSkeletalMeshComponent, MontageToPlay, PlayRate, StartingPosition, StartingSection);

	if (bDidPlay && PlayRate != 0.f && MontageToPlay->HasRootMotion())
	{
		if (UAnimInstance* AnimInstance = InSkeletalMeshComponent->GetAnimInstance())
		{
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
			{
				// Listen for possible ways the montage could end
				OnCompleted.AddUniqueDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);
				OnInterrupted.AddUniqueDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);

				// Disable the actual animation-driven root motion, in favor of our own layered move
				MontageInstance->PushDisableRootMotion();

				const float StartingMontagePosition = MontageInstance->GetPosition();	// position in seconds, disregarding PlayRate

				// Queue a layered move to perform the same anim root motion over the same time span
				TSharedPtr<FLayeredMove_AnimRootMotion> AnimRootMotionMove = MakeShared<FLayeredMove_AnimRootMotion>();
				AnimRootMotionMove->Montage = MontageToPlay;
				AnimRootMotionMove->PlayRate = PlayRate;
				AnimRootMotionMove->StartingMontagePosition = StartingMontagePosition;
				
				float RemainingUnscaledMontageSeconds(0.f);

				if (PlayRate > 0.f)
				{
					// playing forwards, so working towards the end of the montage
					RemainingUnscaledMontageSeconds = MontageToPlay->GetPlayLength() - StartingMontagePosition;
				}
				else
				{
					// playing backwards, so working towards the start of the montage
					RemainingUnscaledMontageSeconds = StartingMontagePosition;	
				}

				AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(PlayRate)) * 1000.f;

				InMoverComponent->QueueLayeredMove(AnimRootMotionMove);
			}
		}
	}

	return bDidPlay;
}

void UPlayMoverMontageCallbackProxy::OnMoverMontageEnded(FName IgnoredNotifyName)
{
	// TODO: this is where we'd want to schedule the ending of the associated move, whether the montage instance was interrupted or ended

	UnbindMontageDelegates();
}

void UPlayMoverMontageCallbackProxy::UnbindMontageDelegates()
{
	OnCompleted.RemoveDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);
	OnInterrupted.RemoveDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);
}


void UPlayMoverMontageCallbackProxy::BeginDestroy()
{
	UnbindMontageDelegates();

	Super::BeginDestroy();
}
