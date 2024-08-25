// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditBoolValue.h"
#include "Components/MaterialValues/DMMaterialValueBool.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditBoolValue"
 
TSharedPtr<SWidget> SDMPropertyEditBoolValue::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InBoolValue)
{
	return SNew(SDMPropertyEditBoolValue, Cast<UDMMaterialValueBool>(InBoolValue))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditBoolValue::Construct(const FArguments& InArgs, UDMMaterialValueBool* InBoolValue)
{
	ensure(IsValid(InBoolValue));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InBoolValue)
	);
}
 
UDMMaterialValueBool* SDMPropertyEditBoolValue::GetBoolValue() const
{
	return Cast<UDMMaterialValueBool>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditBoolValue::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);
 
	UDMMaterialValueBool* BoolValue = GetBoolValue();

	if (!IsValid(BoolValue) || !BoolValue->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	return CreateCheckbox(
		TAttribute<ECheckBoxState>::CreateSP(this, &SDMPropertyEditBoolValue::IsChecked),
		FOnCheckStateChanged::CreateSP(this, &SDMPropertyEditBoolValue::OnValueToggled)
	);
}
 
ECheckBoxState SDMPropertyEditBoolValue::IsChecked() const
{
	UDMMaterialValueBool* BoolValue = GetBoolValue();

	if (!IsValid(BoolValue) || !BoolValue->IsComponentValid())
	{
		return ECheckBoxState::Unchecked;
	}
 
	return BoolValue->GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
 
void SDMPropertyEditBoolValue::OnValueToggled(ECheckBoxState InNewState)
{
	UDMMaterialValueBool* BoolValue = GetBoolValue();
	const bool bNewValue = InNewState == ECheckBoxState::Checked;
	
	if (IsValid(BoolValue) && BoolValue->IsComponentValid() && BoolValue->GetValue() != bNewValue)
	{
		StartTransaction(LOCTEXT("TransactionDescription", "Material Designer Value Toggled (Checkbox)"));
		BoolValue->SetValue(bNewValue);
		EndTransaction();
	}
}

#undef LOCTEXT_NAMESPACE
