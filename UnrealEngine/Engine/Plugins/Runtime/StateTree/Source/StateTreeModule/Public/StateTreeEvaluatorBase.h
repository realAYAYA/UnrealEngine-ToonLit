// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeNodeBase.h"
#include "StateTreeEvaluatorBase.generated.h"

struct FStateTreeExecutionContext;

/**
 * Base struct of StateTree Evaluators.
 * Evaluators calculate and expose data to be used for decision making in a StateTree.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeEvaluatorBase : public FStateTreeNodeBase
{
	GENERATED_BODY()
	
	/**
	 * Called when StateTree is started.
	 * @param Context Reference to current execution context.
	 */
	virtual void TreeStart(FStateTreeExecutionContext& Context) const {}

	/**
	 * Called when StateTree is stopped.
	 * @param Context Reference to current execution context.
	 */
	virtual void TreeStop(FStateTreeExecutionContext& Context) const {}

	/**
	 * Called each frame to update the evaluator.
	 * @param Context Reference to current execution context.
	 * @param DeltaTime Time since last StateTree tick, or 0 if called during preselection.
	 */
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const {}

#if WITH_GAMEPLAY_DEBUGGER
	virtual void AppendDebugInfoString(FString& DebugString, const FStateTreeExecutionContext& Context) const;
#endif // WITH_GAMEPLAY_DEBUGGER
};

/**
* Base class (namespace) for all common Evaluators that are generally applicable.
* This allows schemas to safely include all Evaluators child of this struct. 
*/
USTRUCT(Meta=(Hidden))
struct STATETREEMODULE_API FStateTreeEvaluatorCommonBase : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()
};