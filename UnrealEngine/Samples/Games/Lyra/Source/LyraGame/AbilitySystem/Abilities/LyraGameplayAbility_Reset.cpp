// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/LyraGameplayAbility_Reset.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Character/LyraCharacter.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Delegates/Delegate.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "LyraGameplayTags.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_Reset)

ULyraGameplayAbility_Reset::ULyraGameplayAbility_Reset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	UGameplayTagsManager::Get().CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleDelegate::CreateUObject(this, &ThisClass::DoneAddingNativeTags));
}

void ULyraGameplayAbility_Reset::DoneAddingNativeTags()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Add the ability trigger tag as default to the CDO.
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = FLyraGameplayTags::Get().GameplayEvent_RequestReset;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void ULyraGameplayAbility_Reset::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	ULyraAbilitySystemComponent* LyraASC = CastChecked<ULyraAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());

	FGameplayTagContainer AbilityTypesToIgnore;
	AbilityTypesToIgnore.AddTag(FLyraGameplayTags::Get().Ability_Behavior_SurvivesDeath);

	// Cancel all abilities and block others from starting.
	LyraASC->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);

	SetCanBeCanceled(false);

	// Execute the reset from the character
	if (ALyraCharacter* LyraChar = Cast<ALyraCharacter>(CurrentActorInfo->AvatarActor.Get()))
	{
		LyraChar->Reset();
	}

	// Let others know a reset has occurred
	FLyraPlayerResetMessage Message;
	Message.OwnerPlayerState = CurrentActorInfo->OwnerActor.Get();
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(FLyraGameplayTags::Get().GameplayEvent_Reset, Message);

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const bool bReplicateEndAbility = true;
	const bool bWasCanceled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCanceled);
}

