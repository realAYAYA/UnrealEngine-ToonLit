// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraTypes.h"

class SNiagaraParameterEditor;

/** Niagara editor utilities for the float type. */
class FNiagaraEditorFloatTypeUtilities : public FNiagaraEditorTypeUtilities
{
public:
	//~ INiagaraEditorTypeUtilities interface.
	virtual bool CanCreateParameterEditor() const override { return true; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const override;
	virtual bool CanHandlePinDefaults() const override;
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const override;
	virtual bool SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override;
	virtual FText GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const override;
	virtual FText GetStackDisplayText(const FNiagaraVariable& Variable) const override { return FText::AsNumber(Variable.GetValue<float>()); };
};