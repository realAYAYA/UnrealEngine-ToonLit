// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "WorldConditions/SmartObjectWorldConditionBase.h"
#include "SmartObjectWorldConditionSlotTagQuery.generated.h"

/**
 * World condition to match Smart Object slots's runtime tags.
 */

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectWorldConditionSlotTagQueryState
{
	GENERATED_BODY()

	FSmartObjectSlotHandle SlotHandle;
	
	FDelegateHandle DelegateHandle;
};

USTRUCT(meta=(DisplayName="Match Runtime Slot Tags"))
struct SMARTOBJECTSMODULE_API FSmartObjectWorldConditionSlotTagQuery : public FSmartObjectWorldConditionBase
{
	GENERATED_BODY()

	using FStateType = FSmartObjectWorldConditionSlotTagQueryState;

protected:
#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual TObjectPtr<const UStruct>* GetRuntimeStateType() const override
	{
		static TObjectPtr<const UStruct> Ptr{FStateType::StaticStruct()};
		return &Ptr;
	}
	virtual bool Initialize(const UWorldConditionSchema& Schema) override;
	virtual bool Activate(const FWorldConditionContext& Context) const override;
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
	virtual void Deactivate(const FWorldConditionContext& Context) const override;

	FWorldConditionContextDataRef SubsystemRef;
	FWorldConditionContextDataRef SlotHandleRef;

	/** Smart Object Slot's runtime tags needs to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTagQuery TagQuery;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "SmartObjectRuntime.h"
#endif
