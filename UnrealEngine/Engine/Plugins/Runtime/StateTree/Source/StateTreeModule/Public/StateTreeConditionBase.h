// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeNodeBase.h"
#include "StateTreeExecutionContext.h"
#if WITH_EDITOR
#include "StateTreePropertyBindings.h"
#endif
#include "StateTreeConditionBase.generated.h"

#if WITH_EDITOR
struct IStateTreeBindingLookup;
struct FStateTreeEditorPropertyPath;
#endif

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

#if WITH_EDITOR
	/**
	 * Called when binding of any of the properties in the condition changes.
	 * @param ID ID of the item, can be used make property paths to this item.
	 * @param InstanceData view to the instance data, can be struct or class. 
	 * @param SourcePath Source path of the new binding.
	 * @param TargetPath Target path of the new binding (the property in the condition).
	 * @param BindingLookup Reference to binding lookup which can be used to reason about property paths.
	 */
	virtual void OnBindingChanged(const FGuid& ID, FStateTreeDataView InstanceData, const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath, const IStateTreeBindingLookup& BindingLookup) {}
#endif
	
	/** @return True if the condition passes. */
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const { return false; }

	UPROPERTY()
	EStateTreeConditionOperand Operand = EStateTreeConditionOperand::And;

	UPROPERTY()
	int8 DeltaIndent = 0;
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
