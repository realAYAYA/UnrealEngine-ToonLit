// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "StateTreeConditionBase.h"
#include "StateTreeObjectConditions.generated.h"

/**
 * Condition testing if specified object is valid.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectIsValidConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UObject> Object = nullptr;
};

USTRUCT(DisplayName="Object Is Valid")
struct STATETREEMODULE_API FStateTreeObjectIsValidCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeObjectIsValidConditionInstanceData;

	FStateTreeObjectIsValidCondition() = default;
	explicit FStateTreeObjectIsValidCondition(const EStateTreeCompare InInverts)
		: bInvert(InInverts == EStateTreeCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

/**
 * Condition testing if two object pointers point to the same object.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectEqualsConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UObject> Left = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UObject> Right = nullptr;
};

USTRUCT(DisplayName="Object Equals")
struct STATETREEMODULE_API FStateTreeObjectEqualsCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeObjectEqualsConditionInstanceData;

	FStateTreeObjectEqualsCondition() = default;
	explicit FStateTreeObjectEqualsCondition(const EStateTreeCompare InInverts)
		: bInvert(InInverts == EStateTreeCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

/**
 * Condition testing if object is child of specified class.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectIsChildOfClassConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UObject> Object = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UClass> Class = nullptr;
};

USTRUCT(DisplayName="Object Class Is")
struct STATETREEMODULE_API FStateTreeObjectIsChildOfClassCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeObjectIsChildOfClassConditionInstanceData;

	FStateTreeObjectIsChildOfClassCondition() = default;
	explicit FStateTreeObjectIsChildOfClassCondition(const EStateTreeCompare InInverts)
		: bInvert(InInverts == EStateTreeCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};
