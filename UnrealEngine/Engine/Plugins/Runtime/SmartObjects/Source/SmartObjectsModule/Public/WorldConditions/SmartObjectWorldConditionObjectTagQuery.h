// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "WorldConditions/SmartObjectWorldConditionBase.h"
#include "SmartObjectWorldConditionObjectTagQuery.generated.h"

/**
 * World condition to match Smart Object's runtime tags.
 */

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectWorldConditionObjectTagQueryState
{
	GENERATED_BODY()
	
	FDelegateHandle DelegateHandle;
};

USTRUCT(meta=(DisplayName="Match Runtime Object Tags"))
struct SMARTOBJECTSMODULE_API FSmartObjectWorldConditionObjectTagQuery : public FSmartObjectWorldConditionBase
{
	GENERATED_BODY()

	using FStateType = FSmartObjectWorldConditionObjectTagQueryState;

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
	FWorldConditionContextDataRef ObjectHandleRef;

public:	
	/** Smart Object's runtime tags needs to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTagQuery TagQuery;
};
