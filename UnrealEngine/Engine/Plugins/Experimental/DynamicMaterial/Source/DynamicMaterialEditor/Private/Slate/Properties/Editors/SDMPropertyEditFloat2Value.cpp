// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat2Value.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorSettings.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SNullWidget.h"
 
#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat2Value"

TSharedPtr<SWidget> SDMPropertyEditFloat2Value::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat2Value)
{
	return SNew(SDMPropertyEditFloat2Value, Cast<UDMMaterialValueFloat2>(InFloat2Value))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat2Value::Construct(const FArguments& InArgs, UDMMaterialValueFloat2* InFloat2Value)
{
	ensure(IsValid(InFloat2Value));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(2)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat2Value)
	);
}
 
UDMMaterialValueFloat2* SDMPropertyEditFloat2Value::GetFloat2Value() const
{
	return Cast<UDMMaterialValueFloat2>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat2Value::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0 || InIndex == 1);
 
	UDMMaterialValueFloat2* Float2Value = GetFloat2Value();

	if (!IsValid(Float2Value) || !Float2Value->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	const FFloatInterval& ValueRange = Float2Value->GetValueRange();
	static const TArray<FText> Labels = {
		LOCTEXT("X", "X:"),
		LOCTEXT("Y", "Y:")
	};
 
	return AddWidgetLabel(
		Labels[InIndex],
		CreateSpinBox(
			TAttribute<float>::CreateSP(this, &SDMPropertyEditFloat2Value::GetSpinBoxValue, InIndex),
			SSpinBox<float>::FOnValueChanged::CreateSP(this, &SDMPropertyEditFloat2Value::OnSpinBoxValueChanged, InIndex),
			LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (UV)"),
			Float2Value->HasValueRange() ? &ValueRange : nullptr
		)
	);
}

float SDMPropertyEditFloat2Value::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex == 0 || InIndex == 1);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditFloat2Value::GetSpinBoxValue(int32 InComponent) const
{
	ensure(InComponent == 0 || InComponent == 1);
 
	UDMMaterialValueFloat2* Float2Value = GetFloat2Value();

	if (!IsValid(Float2Value) || !Float2Value->IsComponentValid())
	{
		return 0;
	}
 
	return Float2Value->GetValue()[InComponent];
}
 
void SDMPropertyEditFloat2Value::OnSpinBoxValueChanged(float InNewValue, int32 InComponent) const
{
	ensure(InComponent == 0 || InComponent == 1);
 
	UDMMaterialValueFloat2* Float2Value = GetFloat2Value();

	if (IsValid(Float2Value) && Float2Value->IsComponentValid() 
		&& FMath::IsNearlyEqual(Float2Value->GetValue()[InComponent], InNewValue) == false)
	{
		FVector2D NewTotalValue = Float2Value->GetValue();
		NewTotalValue[InComponent] = InNewValue;
		Float2Value->SetValue(NewTotalValue);
	}
}
 
#undef LOCTEXT_NAMESPACE
