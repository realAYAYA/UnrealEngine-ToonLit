// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActiveGameplayEffectHandle)

namespace GlobalActiveGameplayEffectHandles
{
	static TMap<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>	Map;
}

void FActiveGameplayEffectHandle::ResetGlobalHandleMap()
{
	GlobalActiveGameplayEffectHandles::Map.Reset();
}

FActiveGameplayEffectHandle FActiveGameplayEffectHandle::GenerateNewHandle(UAbilitySystemComponent* OwningComponent)
{
	static int32 GHandleID=0;
	FActiveGameplayEffectHandle NewHandle(GHandleID++);

	TWeakObjectPtr<UAbilitySystemComponent> WeakPtr(OwningComponent);

	GlobalActiveGameplayEffectHandles::Map.Add(NewHandle, WeakPtr);

	return NewHandle;
}

UAbilitySystemComponent* FActiveGameplayEffectHandle::GetOwningAbilitySystemComponent() const
{
	TWeakObjectPtr<UAbilitySystemComponent>* Ptr = GlobalActiveGameplayEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;
}

void FActiveGameplayEffectHandle::RemoveFromGlobalMap()
{
	GlobalActiveGameplayEffectHandles::Map.Remove(*this);
}
