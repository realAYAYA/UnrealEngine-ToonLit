// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeNodeBase.generated.h"

struct FStateTreeLinker;

/**
 * Base struct of StateTree Conditions, Evaluators, and Tasks.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeNodeBase
{
	GENERATED_BODY()

	FStateTreeNodeBase() = default;

	virtual ~FStateTreeNodeBase() {}

	/** @return Struct that represents the runtime data of the node. */
	virtual const UStruct* GetInstanceDataType() const { return nullptr; };

	/**
	 * Called when the StateTree asset is linked. Allows to resolve references to other StateTree data.
	 * @see TStateTreeExternalDataHandle
	 * @param Linker Reference to the linker
	 * @return true if linking succeeded. 
	 */
	[[nodiscard]] virtual bool Link(FStateTreeLinker& Linker) { return true; }

	/**
	 * Called during State Tree compilation, allows to modify and validate the node and instance data.
	 * The method is called with node and instance that is duplicated during compilation and used at runtime (it's different than the data used in editor).  
	 * @param InstanceDataView Pointer to the instance data.
	 * @param ValidationMessages Any messages to report during validation. Displayed as errors if the validation result is Invalid, else as warnings.
	 * @return Validation result based on if the validation succeeded or not. Returning Invalid will fail compilation and messages will be displayed as errors.
	 */
	virtual EDataValidationResult Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages) { return EDataValidationResult::NotValidated; }
	
	/** Name of the node. */
	UPROPERTY(EditDefaultsOnly, Category = "", meta=(EditCondition = "false", EditConditionHides))
	FName Name;

	/** Property binding copy batch handle. */
	UPROPERTY()
	FStateTreeIndex16 BindingsBatch = FStateTreeIndex16::Invalid;

	/** The runtime data's data view index in the StateTreeExecutionContext, and source struct index in property binding. */
	UPROPERTY()
	FStateTreeIndex16 DataViewIndex = FStateTreeIndex16::Invalid;

	/** Index in runtime instance storage. */
	UPROPERTY()
	FStateTreeIndex16 InstanceIndex = FStateTreeIndex16::Invalid;

	/** True if the instance is an UObject. */
	UPROPERTY()
	uint8 bInstanceIsObject : 1;
};
