// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeNodeBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeConditionBase.generated.h"

struct FStateTreeExecutionContext;

enum class EStateTreeCompare : uint8
{
	Default,
	Invert,
};

/**
 * Base struct for all conditions.
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeConditionBase : public FStateTreeNodeBase
{
	GENERATED_BODY()
	
	/** @return True if the condition passes. */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const { return false; }

	UPROPERTY()
	EStateTreeConditionOperand Operand = EStateTreeConditionOperand::And;

	UPROPERTY()
	int8 DeltaIndent = 0;

	UPROPERTY()
	EStateTreeConditionEvaluationMode EvaluationMode = EStateTreeConditionEvaluationMode::Evaluated;
};

/**
 * Base class (namespace) for all common Conditions that are generally applicable.
 * This allows schemas to safely include all conditions child of this struct. 
 */
USTRUCT(meta = (Hidden))
struct STATETREEMODULE_API FStateTreeConditionCommonBase : public FStateTreeConditionBase
{
	GENERATED_BODY()
};

/** Helper macro to define instance data as simple constructible. */
#define STATETREE_POD_INSTANCEDATA(Type) \
template <> struct TIsPODType<Type> { enum { Value = true }; }; \
template<> \
struct TStructOpsTypeTraits<Type> : public TStructOpsTypeTraitsBase2<Type> \
{ \
	enum \
	{ \
		WithZeroConstructor = true, \
		WithNoDestructor = true, \
	}; \
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyBindings.h"
#endif
