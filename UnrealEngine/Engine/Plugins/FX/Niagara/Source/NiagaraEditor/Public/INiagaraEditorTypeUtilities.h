// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Internationalization/Text.h"

class FStructOnScope;
class SNiagaraParameterEditor;
class SWidget;
struct FNiagaraTypeDefinition;
struct FNiagaraVariable;

class INiagaraEditorTypeUtilities
{
public:
	DECLARE_DELEGATE(FNotifyValueChanged);
public:
	virtual ~INiagaraEditorTypeUtilities() {}

	virtual bool CanProvideDefaultValue() const = 0;

	virtual void UpdateVariableWithDefaultValue(FNiagaraVariable& Variable) const = 0;

	virtual bool CanCreateParameterEditor() const = 0;

	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const = 0;

	virtual bool CanCreateDataInterfaceEditor() const = 0;

	virtual TSharedPtr<SWidget> CreateDataInterfaceEditor(UObject* DataInterface, FNotifyValueChanged DataInterfaceChangedHandler) const = 0;

	virtual bool CanHandlePinDefaults() const = 0;

	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const = 0;

	virtual bool SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const = 0;

	virtual bool CanSetValueFromDisplayName() const = 0;

	virtual bool SetValueFromDisplayName(const FText& TextValue, FNiagaraVariable& Variable) const = 0;

	virtual FText GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const = 0;

	virtual FText GetStackDisplayText(const FNiagaraVariable& Variable) const = 0;
};

class FNiagaraEditorTypeUtilities : public INiagaraEditorTypeUtilities, public TSharedFromThis<FNiagaraEditorTypeUtilities, ESPMode::ThreadSafe>
{
public:
	DECLARE_DELEGATE(FNotifyValueChanged);
public:
	//~ INiagaraEditorTypeUtilities
	virtual bool CanProvideDefaultValue() const override { return false; }
	virtual void UpdateVariableWithDefaultValue(FNiagaraVariable& Variable) const override { }
	virtual bool CanCreateParameterEditor() const override { return false; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const override { return TSharedPtr<SNiagaraParameterEditor>(); }
	virtual bool CanCreateDataInterfaceEditor() const override { return false; };
	virtual TSharedPtr<SWidget> CreateDataInterfaceEditor(UObject* DataInterface, FNotifyValueChanged DataInterfaceChangedHandler) const override { return TSharedPtr<SWidget>(); }
	virtual bool CanHandlePinDefaults() const override { return false; }
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const override { return FString(); }
	virtual bool SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override { return false; }
	virtual bool CanSetValueFromDisplayName() const override { return false; }
	virtual bool SetValueFromDisplayName(const FText& TextValue, FNiagaraVariable& Variable) const override { return false; }
	virtual FText GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const override { return FText(); }
	virtual FText GetStackDisplayText(const FNiagaraVariable& Variable) const override
	{
		FString DefaultString = GetPinDefaultStringFromValue(Variable);
		return FText::FromString(DefaultString.IsEmpty() ? "[?]" : DefaultString);
	}
};