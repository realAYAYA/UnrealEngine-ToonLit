// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraFloatTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"

#include "Widgets/Input/SSpinBox.h"

class SNiagaraFloatParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraFloatParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraParameterEditor::Construct(SNiagaraParameterEditor::FArguments()
			.MinimumDesiredWidth(DefaultInputSize)
			.MaximumDesiredWidth(DefaultInputSize));

		ChildSlot
		[
			SNew(SSpinBox<float>)
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			.MinValue(TOptional<float>())
			.MaxValue(TOptional<float>())
			.MaxSliderValue(TOptional<float>())
			.MinSliderValue(TOptional<float>())
			.Delta(0.0f)
			.Value(this, &SNiagaraFloatParameterEditor::GetValue)
			.OnValueChanged(this, &SNiagaraFloatParameterEditor::ValueChanged)
			.OnValueCommitted(this, &SNiagaraFloatParameterEditor::ValueCommitted)
			.OnBeginSliderMovement(this, &SNiagaraFloatParameterEditor::BeginSliderMovement)
			.OnEndSliderMovement(this, &SNiagaraFloatParameterEditor::EndSliderMovement)
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetFloatStruct(), TEXT("Struct type not supported."));
		FloatValue = ((FNiagaraFloat*)Struct->GetStructMemory())->Value;
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetFloatStruct(), TEXT("Struct type not supported."));
		((FNiagaraFloat*)Struct->GetStructMemory())->Value = FloatValue;
	}

	virtual bool CanChangeContinuously() const override { return true; }

private:
	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(float Value)
	{
		ExecuteOnEndValueChange();
	}

	float GetValue() const
	{
		return FloatValue;
	}

	void ValueChanged(float Value)
	{
		FloatValue = Value;
		ExecuteOnValueChanged();
	}

	void ValueCommitted(float Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ValueChanged(Value);
		}
	}

private:
	float FloatValue;
};

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorFloatTypeUtilities::CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const
{
	return SNew(SNiagaraFloatParameterEditor);
}

bool FNiagaraEditorFloatTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorFloatTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate a default value string for an unallocated variable."));
	return LexToString(AllocatedVariable.GetValue<FNiagaraFloat>().Value);
}

bool FNiagaraEditorFloatTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	FNiagaraFloat FloatValue;
	if (LexTryParseString(FloatValue.Value, *StringValue) || !Variable.IsDataAllocated())
	{
		Variable.SetValue<FNiagaraFloat>(FloatValue);
		return true;
	}
	return false;
}

FText FNiagaraEditorFloatTypeUtilities::GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	return FText::FromString(GetPinDefaultStringFromValue(AllocatedVariable));
}
