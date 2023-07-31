// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEnumTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorCommon.h"

#include "SEnumCombo.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SSpinBox.h"

class SNiagaraEnumParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraEnumParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const UEnum* Enum)
	{
		SNiagaraParameterEditor::Construct(SNiagaraParameterEditor::FArguments()
			.MinimumDesiredWidth(DefaultInputSize)
			.MaximumDesiredWidth(2 * DefaultInputSize));

		ChildSlot
		[
			SNew(SEnumComboBox, Enum)
			.CurrentValue(this, &SNiagaraEnumParameterEditor::GetValue)
			.ContentPadding(FMargin(2, 0))
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			.OnEnumSelectionChanged(SEnumComboBox::FOnEnumSelectionChanged::CreateSP(this, &SNiagaraEnumParameterEditor::ValueChanged))
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetIntStruct(), TEXT("Struct type not supported."));
		IntValue = ((FNiagaraInt32*)Struct->GetStructMemory())->Value;
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetIntStruct(), TEXT("Struct type not supported."));
		((FNiagaraInt32*)Struct->GetStructMemory())->Value = IntValue;
	}

private:
	int32 GetValue() const
	{
		return IntValue;
	}

	void ValueChanged(int32 Value, ESelectInfo::Type SelectionType)
	{
		IntValue = Value;
		ExecuteOnValueChanged();
	}

private:
	int32 IntValue;
};

bool FNiagaraEditorEnumTypeUtilities::CanProvideDefaultValue() const
{
	return true;
}

void FNiagaraEditorEnumTypeUtilities::UpdateVariableWithDefaultValue(FNiagaraVariable& Variable) const
{
	const UEnum* Enum = Variable.GetType().GetEnum();
	checkf(Enum != nullptr, TEXT("Variable is not an enum type."));

	FNiagaraInt32 EnumIntValue;
	EnumIntValue.Value = Enum->GetValueByIndex(0);

	Variable.SetValue<FNiagaraInt32>(EnumIntValue);
}

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorEnumTypeUtilities::CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const
{
	return SNew(SNiagaraEnumParameterEditor, ParameterType.GetEnum());
}

bool FNiagaraEditorEnumTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorEnumTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate a default value string for an unallocated variable."));

	const UEnum* Enum = AllocatedVariable.GetType().GetEnum();
	checkf(Enum != nullptr, TEXT("Variable is not an enum type."));

	int64 EnumValue = AllocatedVariable.GetValue<int32>();
	if(Enum->IsValidEnumValue(EnumValue))
	{
		return Enum->GetNameStringByValue(EnumValue);
	}
	else
	{
		FString ReplacementForInvalidValue = Enum->GetNameStringByIndex(0);
		UE_LOG(LogNiagaraEditor, Error, TEXT("Error getting default value for enum pin.  Enum value %i is not supported for enum type %s.  Using value %s"),
			EnumValue, *AllocatedVariable.GetType().GetName(), *ReplacementForInvalidValue);
		return ReplacementForInvalidValue;
	}
}

bool FNiagaraEditorEnumTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	const UEnum* Enum = Variable.GetType().GetEnum();
	checkf(Enum != nullptr, TEXT("Variable is not an enum type."));

	FNiagaraInt32 EnumValue;
	EnumValue.Value = (int32)Enum->GetValueByNameString(StringValue);
	if(EnumValue.Value != INDEX_NONE)
	{
		Variable.AllocateData();
		Variable.SetValue<FNiagaraInt32>(EnumValue);
		return true;
	}
	return false;
}

bool FNiagaraEditorEnumTypeUtilities::CanSetValueFromDisplayName() const
{
	return true;
}

bool FNiagaraEditorEnumTypeUtilities::SetValueFromDisplayName(const FText& TextValue, FNiagaraVariable& Variable) const
{
	const UEnum* Enum = Variable.GetType().GetEnum();
	checkf(Enum != nullptr, TEXT("Variable is not an enum type."));

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < Enum->NumEnums(); i++)
	{
		if (TextValue.CompareTo(Enum->GetDisplayNameTextByIndex(i)) == 0)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex != INDEX_NONE)
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (int32)Enum->GetValueByIndex(FoundIndex);
		Variable.AllocateData();
		Variable.SetValue<FNiagaraInt32>(EnumValue);
		return true;
	}
	return false;
}

FText FNiagaraEditorEnumTypeUtilities::GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate search text for an unallocated variable."));

	const UEnum* Enum = AllocatedVariable.GetType().GetEnum();
	checkf(Enum != nullptr, TEXT("Variable is not an enum type."));

	const int32 EnumNameIndex = Enum->GetIndexByValue(AllocatedVariable.GetValue<int32>());
	return Enum->GetDisplayNameTextByIndex(EnumNameIndex);
}

FText FNiagaraEditorEnumTypeUtilities::GetStackDisplayText(const FNiagaraVariable& Variable) const
{
	return Variable.GetType().GetEnum()->GetDisplayNameTextByValue(Variable.GetValue<int32>());
}
