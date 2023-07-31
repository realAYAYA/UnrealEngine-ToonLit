// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraIntegerTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"

#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"

class SNiagaraIntegerParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraIntegerParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraParameterEditor::Construct(SNiagaraParameterEditor::FArguments()
			.MinimumDesiredWidth(DefaultInputSize)
			.MaximumDesiredWidth(DefaultInputSize));

		ChildSlot
		[
			SNew(SSpinBox<int32>)
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			.MinValue(TOptional<int32>())
			.MaxValue(TOptional<int32>())
			.MaxSliderValue(TOptional<int32>())
			.MinSliderValue(TOptional<int32>())
			.Delta(0.001f)
			.Value(this, &SNiagaraIntegerParameterEditor::GetValue)
			.OnValueChanged(this, &SNiagaraIntegerParameterEditor::ValueChanged)
			.OnValueCommitted(this, &SNiagaraIntegerParameterEditor::ValueCommitted)
			.OnBeginSliderMovement(this, &SNiagaraIntegerParameterEditor::BeginSliderMovement)
			.OnEndSliderMovement(this, &SNiagaraIntegerParameterEditor::EndSliderMovement)
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

	virtual bool CanChangeContinuously() const override { return true; }

private:
	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(int32 Value)
	{
		ExecuteOnEndValueChange();
	}

	int32 GetValue() const
	{
		return IntValue;
	}

	void ValueChanged(int32 Value)
	{
		IntValue = Value;
		ExecuteOnValueChanged();
	}

	void ValueCommitted(int32 Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ValueChanged(Value);
		}
	}

private:
	int32 IntValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorIntegerTypeUtilities::CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const
{
	return SNew(SNiagaraIntegerParameterEditor);
}

bool FNiagaraEditorIntegerTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorIntegerTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate a default value string for an unallocated variable."));
	return LexToString(AllocatedVariable.GetValue<FNiagaraInt32>().Value);
}

bool FNiagaraEditorIntegerTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	FNiagaraInt32 IntegerValue;
	if(LexTryParseString(IntegerValue.Value, *StringValue) || !Variable.IsDataAllocated())
	{
		Variable.SetValue<FNiagaraInt32>(IntegerValue);
		return true;
	}
	return false;
}

FText FNiagaraEditorIntegerTypeUtilities::GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	return FText::FromString(GetPinDefaultStringFromValue(AllocatedVariable));
}
