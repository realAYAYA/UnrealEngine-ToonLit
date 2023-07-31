// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeSchema.generated.h"

/**
 * Schema describing which inputs, evaluators, and tasks a StateTree can contain.
 * Each StateTree asset saves the schema class name in asset data tags, which can be
 * used to limit which StatTree assets can be selected per use case, i.e.:
 *
 *	UPROPERTY(EditDefaultsOnly, Category = AI, meta=(RequiredAssetDataTags="Schema=StateTreeSchema_SupaDupa"))
 *	UStateTree* StateTree;
 *
 */
UCLASS(Abstract)
class STATETREEMODULE_API UStateTreeSchema : public UObject
{
	GENERATED_BODY()

public:

	/** @return True if specified struct is supported */
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const { return false; }

	/** @return True if specified class is supported */
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const { return false; }

	/** @return True if specified struct/class is supported as external data */
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const { return false; }

	/**
	 * Helper function to check if a class is any of the Blueprint extendable item classes (Eval, Task, Condition).
	 * Can be used to quickly accept all of those classes in IsClassAllowed().
	 * @return True if the class is a StateTree item Blueprint base class.
	 */
	bool IsChildOfBlueprintBase(const UClass* InClass) const;

	/** @return List of context objects (UObjects or UScriptStructs) enforced by the schema. They must be provided at runtime through the execution context. */
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const { return {}; }

#if WITH_EDITOR
	
	/** @return True if enter conditions are allowed. */
	virtual bool AllowEnterConditions() const { return true; }

	/** @return True if evaluators are allowed. */
	virtual bool AllowEvaluators() const { return true; }

	/** @return True if multiple tasks are allowed. */
	virtual bool AllowMultipleTasks() const { return true; }
	
#endif // WITH_EDITOR
};
