// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/Async/AbilityAsync_WaitGameplayTagCountChanged.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"

void UAbilityAsync_WaitGameplayTagCountChanged::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC && ShouldBroadcastDelegates())
	{
		GameplayTagCountChangedHandle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange).AddUObject(this, &UAbilityAsync_WaitGameplayTagCountChanged::GameplayTagCallback);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("%s: AbilitySystemComponent is nullptr! Could not register for gameplay tag event with Tag = %s."), *GetName(), *Tag.GetTagName().ToString());
		EndAction();
	}
}

void UAbilityAsync_WaitGameplayTagCountChanged::EndAction()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC && GameplayTagCountChangedHandle.IsValid())
	{
		ASC->RegisterGameplayTagEvent(Tag).Remove(GameplayTagCountChangedHandle);
		GameplayTagCountChangedHandle.Reset();
	}
	Super::EndAction();
}

void UAbilityAsync_WaitGameplayTagCountChanged::GameplayTagCallback(const FGameplayTag InTag, int32 NewCount)
{
	if (ShouldBroadcastDelegates())
	{
		TagCountChanged.Broadcast(NewCount);
	}
	else
	{
		EndAction();
	}
}

UAbilityAsync_WaitGameplayTagCountChanged* UAbilityAsync_WaitGameplayTagCountChanged::WaitGameplayTagCountChangedOnActor(AActor* TargetActor, FGameplayTag Tag)
{
	UAbilityAsync_WaitGameplayTagCountChanged* MyObj = NewObject<UAbilityAsync_WaitGameplayTagCountChanged>();
	MyObj->SetAbilityActor(TargetActor);
	MyObj->Tag = Tag;

	return MyObj;
}
