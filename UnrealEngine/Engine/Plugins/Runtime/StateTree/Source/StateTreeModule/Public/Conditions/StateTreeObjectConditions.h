// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeConditionBase.h"
#include "StateTreeObjectConditions.generated.h"

USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectIsValidConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UObject> Object = nullptr;
};

/**
 * Condition testing if specified object is valid.
 */
USTRUCT(DisplayName = "Object Is Valid", Category = "Object")
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


USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectEqualsConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UObject> Left = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UObject> Right = nullptr;
};

/**
 * Condition testing if two object pointers point to the same object.
 */
USTRUCT(DisplayName = "Object Equals", Category = "Object")
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

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};


USTRUCT()
struct STATETREEMODULE_API FStateTreeObjectIsChildOfClassConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UObject> Object = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UClass> Class = nullptr;
};

/**
 * Condition testing if object is child of specified class.
 */
USTRUCT(DisplayName = "Object Class Is", Category = "Object")
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

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};
