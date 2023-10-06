// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActiveGameplayEffectHandle.generated.h"

class UAbilitySystemComponent;

/**
 * This handle is required for things outside of FActiveGameplayEffectsContainer to refer to a specific active GameplayEffect
 *	For example if a skill needs to create an active effect and then destroy that specific effect that it created, it has to do so
 *	through a handle. a pointer or index into the active list is not sufficient. These are not synchronized between clients and server.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FActiveGameplayEffectHandle
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectHandle()
		: Handle(INDEX_NONE),
		bPassedFiltersAndWasExecuted(false)
	{
	}

	FActiveGameplayEffectHandle(int32 InHandle)
		: Handle(InHandle),
		bPassedFiltersAndWasExecuted(true)
	{
	}

	/** True if this is tracking an active ongoing gameplay effect */
	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	/** True if this applied a gameplay effect. This can be true when it is not valid if it applied an instant effect */
	bool WasSuccessfullyApplied() const
	{
		return bPassedFiltersAndWasExecuted;
	}

	/** Creates a new handle, will be set to successfully applied */
	static FActiveGameplayEffectHandle GenerateNewHandle(UAbilitySystemComponent* OwningComponent);

	/** Resets the map that supports GetOwningAbilitySystemComponent */
	static void ResetGlobalHandleMap();

	/** Returns the ability system component that created this handle */
	UAbilitySystemComponent* GetOwningAbilitySystemComponent() const;

	/** Remove this from the GetOwningAbilitySystemComponent map */
	void RemoveFromGlobalMap();

	bool operator==(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FActiveGameplayEffectHandle& InHandle)
	{
		return InHandle.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

private:

	UPROPERTY()
	int32 Handle;

	UPROPERTY()
	bool bPassedFiltersAndWasExecuted;
};