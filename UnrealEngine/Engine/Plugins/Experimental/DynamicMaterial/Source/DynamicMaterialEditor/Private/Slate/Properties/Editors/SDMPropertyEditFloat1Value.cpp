// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat1Value.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorSettings.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SNullWidget.h"
 
#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat1Value"

TSharedPtr<SWidget> SDMPropertyEditFloat1Value::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat1Value)
{
	return SNew(SDMPropertyEditFloat1Value, Cast<UDMMaterialValueFloat1>(InFloat1Value))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat1Value::Construct(const FArguments& InArgs, UDMMaterialValueFloat1* InFloat1Value)
{
	ensure(IsValid(InFloat1Value));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat1Value)
	);
}
 
UDMMaterialValueFloat1* SDMPropertyEditFloat1Value::GetFloat1Value() const
{
	return Cast<UDMMaterialValueFloat1>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat1Value::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);
 
	UDMMaterialValueFloat1* Float1Value = GetFloat1Value();

	if (!IsValid(Float1Value) || !Float1Value->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	const FFloatInterval& ValueRange = Float1Value->GetValueRange();
 
	return CreateSpinBox(
		TAttribute<float>::CreateSP(this, &SDMPropertyEditFloat1Value::GetSpinBoxValue),
		SSpinBox<float>::FOnValueChanged::CreateSP(this, &SDMPropertyEditFloat1Value::OnSpinBoxValueChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Float)"),
		Float1Value->HasValueRange() ? &ValueRange : nullptr
	);
}

float SDMPropertyEditFloat1Value::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex == 0);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditFloat1Value::GetSpinBoxValue() const
{
	UDMMaterialValueFloat1* Float1Value = GetFloat1Value();

	if (!IsValid(Float1Value) || !Float1Value->IsComponentValid())
	{
		return 0;
	}
 
	return Float1Value->GetValue();
}
 
void SDMPropertyEditFloat1Value::OnSpinBoxValueChanged(float InNewValue) const
{
	UDMMaterialValueFloat1* Float1Value = GetFloat1Value();
	
	if (IsValid(Float1Value) && Float1Value->IsComponentValid() 
		&& FMath::IsNearlyEqual(Float1Value->GetValue(), InNewValue) == false)
	{
		Float1Value->SetValue(InNewValue);
	}
}
 
#undef LOCTEXT_NAMESPACE
