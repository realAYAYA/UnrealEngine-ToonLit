// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionFindSlotTask.generated.h"

class USmartObjectSubsystem;

UENUM()
enum class EGameplayInteractionSlotReferenceType : uint8
{
	/** Consider all slots on the Reference Slot's Smart Object. Match by activity tag. */
	ByActivityTag,
	
	/** Consider slots linked via Link Annotations on Reference Slot. Match by link's tag. */
	ByLinkTag,
};

USTRUCT()
struct FGameplayInteractionFindSlotTaskInstanceData
{
	GENERATED_BODY()

	/** Slot to use as reference to find the result slot. */
	UPROPERTY(EditAnywhere, Category="Input")
	FSmartObjectSlotHandle ReferenceSlot;

	/** Result slot. */
	UPROPERTY(EditAnywhere, Category="Output")
	FSmartObjectSlotHandle ResultSlot;
};


/**
 * Task to find a Smart Object slot based on a reference slot.
 * The search can look up slots in the whole Smart Object based on Activity tags,
 * or use Smart Object Slot Link annotations on the reference slot.
 */
USTRUCT(meta = (DisplayName = "Find Slot", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionFindSlotTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionFindSlotTask();
	
	using FInstanceDataType = FGameplayInteractionFindSlotTaskInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	bool UpdateResult(const FStateTreeExecutionContext& Context) const;

	/** Specified which slots to consider when finding the slot. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	EGameplayInteractionSlotReferenceType ReferenceType = EGameplayInteractionSlotReferenceType::ByLinkTag;

	/** Tag to use to lookup the slot. */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FGameplayTag FindByTag;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
