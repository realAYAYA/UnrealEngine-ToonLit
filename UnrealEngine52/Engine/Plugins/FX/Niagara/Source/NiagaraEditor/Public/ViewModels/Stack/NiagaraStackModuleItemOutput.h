// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraTypes.h"
#include "NiagaraStackModuleItemOutput.generated.h"

class UNiagaraNodeFunctionCall;

/** Represents a single module Output in the module stack view model. */
UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItemOutput : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackModuleItemOutput();

	/** 
	 * Sets the Output data for this entry.
	 * @param InFunctionCallNode The function call node representing the module in the stack graph which owns this Output.
	 * @param InOutputParameterHandle The Namespace.Name handle for the Output to the owning module.
	 */
	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraNodeFunctionCall& InFunctionCallNode, FName InOutputParameterHandle, FNiagaraTypeDefinition InOutputType);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetTooltipText() const override;
	virtual bool GetIsEnabled() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	/** Gets the parameter handle which defined this Output in the module. */
	const FNiagaraParameterHandle& GetOutputParameterHandle() const;

	/** Gets the assigned parameter handle as displayable text. */
	FText GetOutputParameterHandleText() const;

	virtual const FCollectedUsageData& GetCollectedUsageData() const override;

private:
	/** The function call node which represents the emitter which owns this Output */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> FunctionCallNode;

	/** The parameter handle which defined this Output in the module graph. */
	FNiagaraParameterHandle OutputParameterHandle;

	/** The name of this Output for display in the UI. */
	FText DisplayName;

	/** The niagara type definition for this input. */
	FNiagaraTypeDefinition OutputType;
};
