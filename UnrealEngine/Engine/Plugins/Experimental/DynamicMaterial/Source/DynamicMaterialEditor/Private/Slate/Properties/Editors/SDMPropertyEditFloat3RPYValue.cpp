// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat3RPYValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat3RPY.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorSettings.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SNullWidget.h"
 
#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat3RPYValue"

TSharedPtr<SWidget> SDMPropertyEditFloat3RPYValue::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3RPYValue)
{
	return SNew(SDMPropertyEditFloat3RPYValue, Cast<UDMMaterialValueFloat3RPY>(InFloat3RPYValue))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat3RPYValue::Construct(const FArguments& InArgs, UDMMaterialValueFloat3RPY* InFloat3RPYValue)
{
	ensure(IsValid(InFloat3RPYValue));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(3)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat3RPYValue)
	);
}
 
UDMMaterialValueFloat3RPY* SDMPropertyEditFloat3RPYValue::GetFloat3RPYValue() const
{
	return Cast<UDMMaterialValueFloat3RPY>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat3RPYValue::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex >= 0 && InIndex <= 2);
 
	UDMMaterialValueFloat3RPY* Float3RPYValue = GetFloat3RPYValue();

	if (!IsValid(Float3RPYValue) || !Float3RPYValue->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	EAxis::Type ComponentAxis = EAxis::None;
 
	switch (InIndex)
	{
		case 0:
			ComponentAxis = EAxis::X;
			break;
 
		case 1:
			ComponentAxis = EAxis::Y;
			break;
 
		case 2:
			ComponentAxis = EAxis::Z;
			break;
 
		default:
			checkNoEntry();
			break;
	}
 
	const FFloatInterval& ValueRange = Float3RPYValue->GetValueRange();
	static const TArray<FText> Labels = {
		LOCTEXT("R", "R:"),
		LOCTEXT("P", "P:"),
		LOCTEXT("Y", "Y:")
	};
 
	return AddWidgetLabel(
		Labels[InIndex],
		CreateSpinBox(
			TAttribute<float>::CreateSP(this, &SDMPropertyEditFloat3RPYValue::GetSpinBoxValue, ComponentAxis),
			SSpinBox<float>::FOnValueChanged::CreateSP(this, &SDMPropertyEditFloat3RPYValue::OnSpinBoxValueChanged, ComponentAxis),
			LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Rotator)"),
			Float3RPYValue->HasValueRange() ? &ValueRange : nullptr
		)
	);
}

float SDMPropertyEditFloat3RPYValue::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex >= 0 && InIndex <= 2);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditFloat3RPYValue::GetSpinBoxValue(EAxis::Type InAxis) const
{
	ensure(InAxis == EAxis::X || InAxis == EAxis::Y || InAxis == EAxis::Z);
 
	UDMMaterialValueFloat3RPY* Float3RPYValue = GetFloat3RPYValue();

	if (!IsValid(Float3RPYValue) || !Float3RPYValue->IsComponentValid())
	{
		return 0;
	}
 
	return Float3RPYValue->GetValue().GetComponentForAxis(InAxis);
}
 
void SDMPropertyEditFloat3RPYValue::OnSpinBoxValueChanged(float InNewValue, EAxis::Type InAxis) const
{
	ensure(InAxis == EAxis::X || InAxis == EAxis::Y || InAxis == EAxis::Z);
 
	UDMMaterialValueFloat3RPY* Float3RPYValue = GetFloat3RPYValue();
	
	if (IsValid(Float3RPYValue) && Float3RPYValue->IsComponentValid() 
		&& FMath::IsNearlyEqual(Float3RPYValue->GetValue().GetComponentForAxis(InAxis), InNewValue) == false)
	{
		FRotator NewTotalValue = Float3RPYValue->GetValue();
		NewTotalValue.SetComponentForAxis(InAxis, InNewValue);
		Float3RPYValue->SetValue(NewTotalValue);
	}
}
 
#undef LOCTEXT_NAMESPACE
