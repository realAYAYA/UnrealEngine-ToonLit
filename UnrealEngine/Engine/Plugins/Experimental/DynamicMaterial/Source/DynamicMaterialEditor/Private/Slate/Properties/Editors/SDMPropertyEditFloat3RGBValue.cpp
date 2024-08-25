// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat3RGBValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat3RGB.h"
#include "DMPrivate.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat3RGBValue"

TSharedPtr<SWidget> SDMPropertyEditFloat3RGBValue::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat3RGBValue)
{
	return SNew(SDMPropertyEditFloat3RGBValue, Cast<UDMMaterialValueFloat3RGB>(InFloat3RGBValue))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat3RGBValue::Construct(const FArguments& InArgs, UDMMaterialValueFloat3RGB* InFloat3RGBValue)
{
	ensure(IsValid(InFloat3RGBValue));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat3RGBValue)
	);
}
 
UDMMaterialValueFloat3RGB* SDMPropertyEditFloat3RGBValue::GetFloat3RGBValue() const
{
	return Cast<UDMMaterialValueFloat3RGB>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat3RGBValue::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);
 
	UDMMaterialValueFloat3RGB* Float3RGBValue = GetFloat3RGBValue();

	if (!IsValid(Float3RGBValue) || !Float3RGBValue->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	const FFloatInterval& ValueRange = Float3RGBValue->GetValueRange();
 
	return CreateColorPicker(
		/* bUseAlpha */ false,
		TAttribute<FLinearColor>::CreateSP(this, &SDMPropertyEditFloat3RGBValue::GetColor),
		FOnLinearColorValueChanged::CreateSP(this, &SDMPropertyEditFloat3RGBValue::OnColorValueChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (RGB)"),
		Float3RGBValue->HasValueRange() ? &ValueRange : nullptr
	);
}
 
FLinearColor SDMPropertyEditFloat3RGBValue::GetColor() const
{
	UDMMaterialValueFloat3RGB* Float3RGBValue = GetFloat3RGBValue();

	if (!IsValid(Float3RGBValue) || !Float3RGBValue->IsComponentValid())
	{
		return FLinearColor::Black;
	}
 
	return Float3RGBValue->GetValue();
}
 
void SDMPropertyEditFloat3RGBValue::OnColorValueChanged(FLinearColor InNewColor) const
{
	UDMMaterialValueFloat3RGB* Float3RGBValue = GetFloat3RGBValue();

	if (IsValid(Float3RGBValue) && Float3RGBValue->IsComponentValid())
	{
		InNewColor.A = 1.f;
		const FLinearColor& CurrentColor = Float3RGBValue->GetValue();

		const bool bSameValue = FMath::IsNearlyEqual(InNewColor.R, CurrentColor.R)
			&& FMath::IsNearlyEqual(InNewColor.G, CurrentColor.G)
			&& FMath::IsNearlyEqual(InNewColor.B, CurrentColor.B);

		if (!bSameValue)
		{
			Float3RGBValue->SetValue(InNewColor);
		}
	}
}
 
#undef LOCTEXT_NAMESPACE
