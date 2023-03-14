// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "StateTreeTypes.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeEvents.h"
#include "StateTreeNodeBlueprintBase.h"
#include "StateTreeEvaluatorBlueprintBase.generated.h"

struct FStateTreeExecutionContext;

/*
 * Base class for Blueprint based evaluators. 
 */
UCLASS(Abstract, Blueprintable)
class STATETREEMODULE_API UStateTreeEvaluatorBlueprintBase : public UStateTreeNodeBlueprintBase
{
	GENERATED_BODY()
public:
	UStateTreeEvaluatorBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "TreeStart"))
	void ReceiveTreeStart();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "TreeStop"))
	void ReceiveTreeStop();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick"))
	void ReceiveTick(const float DeltaTime);

protected:
	virtual void TreeStart(FStateTreeExecutionContext& Context);
	virtual void TreeStop(FStateTreeExecutionContext& Context);
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime);

	uint8 bHasTreeStart : 1;
	uint8 bHasTreeStop : 1;
	uint8 bHasTick : 1;

	friend struct FStateTreeBlueprintEvaluatorWrapper;
};

/**
 * Wrapper for Blueprint based Evaluators.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeBlueprintEvaluatorWrapper : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return EvaluatorClass; };
	
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	UPROPERTY()
	TSubclassOf<UStateTreeEvaluatorBlueprintBase> EvaluatorClass = nullptr;
};
