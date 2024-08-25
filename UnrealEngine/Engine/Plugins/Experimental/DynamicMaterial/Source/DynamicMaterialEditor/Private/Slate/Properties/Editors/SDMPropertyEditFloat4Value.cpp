// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "SDMPropertyEditFloat4Value.h"
#include "Components/MaterialValues/DMMaterialValueFloat4.h"
#include "DMPrivate.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat4Value"

TSharedPtr<SWidget> SDMPropertyEditFloat4Value::CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InFloat4Value)
{
	return SNew(SDMPropertyEditFloat4Value, Cast<UDMMaterialValueFloat4>(InFloat4Value))
		.ComponentEditWidget(InComponentEditWidget);
}
 
void SDMPropertyEditFloat4Value::Construct(const FArguments& InArgs, UDMMaterialValueFloat4* InFloat4Value)
{
	ensure(IsValid(InFloat4Value));

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyMaterialValue(InFloat4Value)
	);
}
 
UDMMaterialValueFloat4* SDMPropertyEditFloat4Value::GetFloat4Value() const
{
	return Cast<UDMMaterialValueFloat4>(GetValue());
}
 
TSharedRef<SWidget> SDMPropertyEditFloat4Value::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);
 
	UDMMaterialValueFloat4* Float4Value = GetFloat4Value();

	if (!IsValid(Float4Value) || !Float4Value->IsComponentValid())
	{
		return SNullWidget::NullWidget;
	}
 
	const FFloatInterval& ValueRange = Float4Value->GetValueRange();
 
	return CreateColorPicker(
		/* bUseAlpha */ true,
		TAttribute<FLinearColor>::CreateSP(this, &SDMPropertyEditFloat4Value::GetColor),
		FOnLinearColorValueChanged::CreateSP(this, &SDMPropertyEditFloat4Value::OnColorValueChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (RGBA)"),
		Float4Value->HasValueRange() ? &ValueRange : nullptr
	);
}
 
FLinearColor SDMPropertyEditFloat4Value::GetColor() const
{
	UDMMaterialValueFloat4* Float4Value = GetFloat4Value();

	if (!IsValid(Float4Value) || !Float4Value->IsComponentValid())
	{
		return FLinearColor::Black;
	}
 
	return Float4Value->GetValue();
}
 
void SDMPropertyEditFloat4Value::OnColorValueChanged(FLinearColor InNewColor) const
{
	UDMMaterialValueFloat4* Float4Value = GetFloat4Value();
	
	if (IsValid(Float4Value) && Float4Value->IsComponentValid())
	{
		const FLinearColor& CurrentColor = Float4Value->GetValue();

		const bool bSameValue = FMath::IsNearlyEqual(InNewColor.R, CurrentColor.R)
			&& FMath::IsNearlyEqual(InNewColor.G, CurrentColor.G)
			&& FMath::IsNearlyEqual(InNewColor.B, CurrentColor.B)
			&& FMath::IsNearlyEqual(InNewColor.A, CurrentColor.A);

		if (!bSameValue)
		{
			Float4Value->SetValue(InNewColor);
		}
	}
}
 
#undef LOCTEXT_NAMESPACE
