// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "NiagaraTypes.h"

class UNiagaraNodeWithDynamicPins;
struct FNiagaraTypeDefinition;

/** A graph pin for adding additional pins a dynamic niagara node. */
class SNiagaraGraphPinAdd : public SGraphPin
{
	SLATE_BEGIN_ARGS(SNiagaraGraphPinAdd) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	
	/** Returns the owning node of this add pin. */
	UNiagaraNodeWithDynamicPins* GetOwningNode() { return OwningNode; }

private:
	TSharedRef<SWidget>	ConstructAddButton();

	TSharedRef<SWidget> OnGetAddButtonMenuContent();
	
	TSharedPtr<class SComboButton> AddButton;

private:
	UNiagaraNodeWithDynamicPins* OwningNode;
};