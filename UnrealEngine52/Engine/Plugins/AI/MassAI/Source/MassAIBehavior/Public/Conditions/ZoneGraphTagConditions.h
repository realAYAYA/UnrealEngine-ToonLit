// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphTagConditions.generated.h"

/**
* ZoneGraph Tag condition.
*/

USTRUCT()
struct MASSAIBEHAVIOR_API FZoneGraphTagFilterConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	FZoneGraphTagMask Tags = FZoneGraphTagMask::None;
};

USTRUCT(DisplayName="ZoneGraphTagFilter Compare")
struct MASSAIBEHAVIOR_API FZoneGraphTagFilterCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FZoneGraphTagFilterConditionInstanceData;

	FZoneGraphTagFilterCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	FZoneGraphTagFilter Filter;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

/**
* ZoneGraph Tag condition.
*/

USTRUCT()
struct MASSAIBEHAVIOR_API FZoneGraphTagMaskConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FZoneGraphTagMask Left = FZoneGraphTagMask::None;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FZoneGraphTagMask Right = FZoneGraphTagMask::None;
};

USTRUCT(DisplayName="ZoneGraphTagMask Compare")
struct MASSAIBEHAVIOR_API FZoneGraphTagMaskCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FZoneGraphTagMaskConditionInstanceData;

	FZoneGraphTagMaskCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	EZoneLaneTagMaskComparison Operator = EZoneLaneTagMaskComparison::Any;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};

/**
* ZoneGraph Tag condition.
*/

USTRUCT()
struct MASSAIBEHAVIOR_API FZoneGraphTagConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FZoneGraphTag Left = FZoneGraphTag::None;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FZoneGraphTag Right = FZoneGraphTag::None;
};

USTRUCT(DisplayName="ZoneGraphTag Compare")
struct MASSAIBEHAVIOR_API FZoneGraphTagCondition : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FZoneGraphTagConditionInstanceData;

	FZoneGraphTagCondition() = default;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;
};
