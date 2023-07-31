// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "StateTreeConditionBase.h"
#include "StateTreeNodeBlueprintBase.h"
#include "StateTreeConditionBlueprintBase.generated.h"

struct FStateTreeExecutionContext;

/*
 * Base class for Blueprint based Conditions. 
 */
UCLASS(Abstract, Blueprintable)
class STATETREEMODULE_API UStateTreeConditionBlueprintBase : public UStateTreeNodeBlueprintBase
{
	GENERATED_BODY()
public:
	UStateTreeConditionBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent)
	bool ReceiveTestCondition() const;

protected:
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const;

	friend struct FStateTreeBlueprintConditionWrapper;

	uint8 bHasTestCondition : 1;
};

/**
 * Wrapper for Blueprint based Conditions.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeBlueprintConditionWrapper : public FStateTreeConditionBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return ConditionClass; };
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY()
	TSubclassOf<UStateTreeConditionBlueprintBase> ConditionClass = nullptr;
};
