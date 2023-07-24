// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionConditions.generated.h"

class USmartObjectSubsystem;

UENUM()
enum class EGameplayInteractionMatchSlotTagSource : uint8
{
	/** Test slot definition activity Tags. */
	ActivityTags,

	/** Test slot Runtime tags. */
	RuntimeTags,
};

USTRUCT()
struct FGameplayInteractionMatchSlotTagsConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle Slot;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagContainer TagsToMatch;
};

/**
 * Condition to check if Gameplay Tags on a Smart Object slot match the specified tags.
 */
USTRUCT(DisplayName="Match Slot Tags", Category="Gameplay Interactions|Smart Object")
struct FGameplayInteractionSlotTagsMatchCondition : public FGameplayInteractionStateTreeCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayInteractionMatchSlotTagsConditionInstanceData;

	FGameplayInteractionSlotTagsMatchCondition() = default;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	EGameplayInteractionMatchSlotTagSource Source = EGameplayInteractionMatchSlotTagSource::RuntimeTags;

	UPROPERTY(EditAnywhere, Category = "Condition")
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
	
	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};


USTRUCT()
struct FGameplayInteractionQuerySlotTagsConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle Slot;
};

/**
 * Condition to check if Gameplay Tags on a Smart Object slot match the Gameplay Tag query.
 */
USTRUCT(DisplayName="Query Slot Tags", Category="Gameplay Interactions|Smart Object")
struct FGameplayInteractionQuerySlotTagCondition : public FGameplayInteractionStateTreeCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayInteractionQuerySlotTagsConditionInstanceData;

	FGameplayInteractionQuerySlotTagCondition() = default;
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	EGameplayInteractionMatchSlotTagSource Source = EGameplayInteractionMatchSlotTagSource::RuntimeTags;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagQuery TagQuery;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};

USTRUCT()
struct FGameplayInteractionIsSlotHandleValidConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Input")
	FSmartObjectSlotHandle Slot;
};

/**
 * Condition to check if a Smart Object slot handle is valid. 
 */
USTRUCT(DisplayName="Is Slot Handle Valid", Category="Gameplay Interactions|Smart Object")
struct FGameplayInteractionIsSlotHandleValidCondition : public FGameplayInteractionStateTreeCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayInteractionIsSlotHandleValidConditionInstanceData;

	FGameplayInteractionIsSlotHandleValidCondition() = default;
	
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
