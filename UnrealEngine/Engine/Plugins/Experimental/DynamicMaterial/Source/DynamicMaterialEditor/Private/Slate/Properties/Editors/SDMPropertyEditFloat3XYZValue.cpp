// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat3XYZValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat3XYZ.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorSettings.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SNullWidget.h"
 
#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat3XYZValue"

TSharedPtr<SWidget> SDMPropertyEditFloat3XYZValue::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3XYZValue)
{
	return SNew(SDMPropertyEditFloat3XYZValue, Cast<UDMMaterialValueFloat3XYZ>(InFloat3XYZValue))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat3XYZValue::Construct(const FArguments& InArgs, UDMMaterialValueFloat3XYZ* InFloat3XYZValue)
{
	ensure(IsValid(InFloat3XYZValue));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(3)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat3XYZValue)
	);
}
 
UDMMaterialValueFloat3XYZ* SDMPropertyEditFloat3XYZValue::GetFloat3XYZValue() const
{
	return Cast<UDMMaterialValueFloat3XYZ>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat3XYZValue::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex >= 0 && InIndex <= 2);
 
	UDMMaterialValueFloat3XYZ* Float3XYZValue = GetFloat3XYZValue();

	if (!IsValid(Float3XYZValue) || !Float3XYZValue->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	const FFloatInterval& ValueRange = Float3XYZValue->GetValueRange();
	static const TArray<FText> Labels = {
		LOCTEXT("X", "X:"),
		LOCTEXT("Y", "Y:"),
		LOCTEXT("Z", "Z:")
	};
 
	return AddWidgetLabel(
		Labels[InIndex],
		CreateSpinBox(
			TAttribute<float>::CreateSP(this, &SDMPropertyEditFloat3XYZValue::GetSpinBoxValue, InIndex),
			SSpinBox<float>::FOnValueChanged::CreateSP(this, &SDMPropertyEditFloat3XYZValue::OnSpinBoxValueChanged, InIndex),
			LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Vector)"),
			Float3XYZValue->HasValueRange() ? &ValueRange : nullptr
		)
	);
}

float SDMPropertyEditFloat3XYZValue::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex >= 0 && InIndex <= 2);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditFloat3XYZValue::GetSpinBoxValue(int32 InComponent) const
{
	ensure(InComponent >= 0 && InComponent <= 2);
 
	UDMMaterialValueFloat3XYZ* Float3XYZValue = GetFloat3XYZValue();

	if (!IsValid(Float3XYZValue) || !Float3XYZValue->IsComponentValid())
	{
		return 0;
	}
 
	return Float3XYZValue->GetValue()[InComponent];
}
 
void SDMPropertyEditFloat3XYZValue::OnSpinBoxValueChanged(float InNewValue, int32 InComponent) const
{
	ensure(InComponent >= 0 && InComponent <= 2);
 
	UDMMaterialValueFloat3XYZ* Float3XYZValue = GetFloat3XYZValue();
	
	if (IsValid(Float3XYZValue) && Float3XYZValue->IsComponentValid() 
		&& FMath::IsNearlyEqual(Float3XYZValue->GetValue()[InComponent], InNewValue) == false)
	{
		FVector NewTotalValue = Float3XYZValue->GetValue();
		NewTotalValue[InComponent] = InNewValue;
		Float3XYZValue->SetValue(NewTotalValue);
	}
}
 
#undef LOCTEXT_NAMESPACE
