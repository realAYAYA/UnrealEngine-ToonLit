// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraBoolTypeEditorUtilities.h"
#include "NiagaraTypes.h"
#include "SNiagaraParameterEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"

class SNiagaraBoolParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraBoolParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SNiagaraBoolParameterEditor::GetCheckState)
				.OnCheckStateChanged(this, &SNiagaraBoolParameterEditor::OnCheckStateChanged)
			]
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetBoolStruct(), TEXT("Struct type not supported."));
		bBoolValue = ((FNiagaraBool*)Struct->GetStructMemory())->GetValue();
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		// Note that while bool conventionally have false = 0 and true = 1 (or any non-zero value), Niagara internally uses true == -1. Make 
		// sure to enforce this convention when setting the value in memory.
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetBoolStruct(), TEXT("Struct type not supported."));
		((FNiagaraBool*)Struct->GetStructMemory())->SetValue(bBoolValue);
	}

private:
	ECheckBoxState GetCheckState() const
	{
		return bBoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void OnCheckStateChanged(ECheckBoxState InCheckState)
	{
		bBoolValue = InCheckState == ECheckBoxState::Checked;
		ExecuteOnValueChanged();
	}

private:
	bool bBoolValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorBoolTypeUtilities::CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const
{
	return SNew(SNiagaraBoolParameterEditor);
}

bool FNiagaraEditorBoolTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorBoolTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate a default value string for an unallocated variable."));
	return LexToString(AllocatedVariable.GetValue<FNiagaraBool>().GetValue());
}

bool FNiagaraEditorBoolTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	bool bBoolValue = false;
	if (LexTryParseString(bBoolValue, *StringValue) || !Variable.IsDataAllocated())
	{
		FNiagaraBool BoolValue;
		BoolValue.SetValue(bBoolValue);
		Variable.SetValue<FNiagaraBool>(BoolValue);
		return true;
	}
	return false;
}
