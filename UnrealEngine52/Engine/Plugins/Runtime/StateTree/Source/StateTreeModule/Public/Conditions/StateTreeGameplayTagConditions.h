// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeConditionBase.h"
#include "StateTreeGameplayTagConditions.generated.h"

USTRUCT()
struct STATETREEMODULE_API FGameplayTagMatchConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer TagContainer;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag Tag;
};

/**
 * Gameplay Tag match condition.
 */
USTRUCT(DisplayName="Gameplay Tag Match", Category="Gameplay Tags")
struct STATETREEMODULE_API FGameplayTagMatchCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayTagMatchConditionInstanceData;

	FGameplayTagMatchCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};


USTRUCT()
struct STATETREEMODULE_API FGameplayTagContainerMatchConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayTagContainer TagContainer;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer OtherContainer;
};

/**
 * Gameplay Tag Container match condition.
 */
USTRUCT(DisplayName="Gameplay Tag Container Match", Category="Gameplay Tags")
struct STATETREEMODULE_API FGameplayTagContainerMatchCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayTagContainerMatchConditionInstanceData ;

	FGameplayTagContainerMatchCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bExactMatch = false;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};


USTRUCT()
struct STATETREEMODULE_API FGameplayTagQueryConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayTagContainer TagContainer;
};

/**
 * Gameplay Tag Query match condition.
 */
USTRUCT(DisplayName="Gameplay Tag Query", Category="Gameplay Tags")
struct STATETREEMODULE_API FGameplayTagQueryCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGameplayTagQueryConditionInstanceData;

	FGameplayTagQueryCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	FGameplayTagQuery TagQuery;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};
