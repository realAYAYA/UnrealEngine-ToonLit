// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Editors/SDMPropertyEditFloat.h"
#include "DynamicMaterialEditorSettings.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditFloat"

void SDMPropertyEditFloat::Construct(const FArguments& InArgs, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
{
	ensure(InPropertyHandle.IsValid());

	FloatInterval = InArgs._FloatInterval;

	SDMPropertyEdit::Construct(
		SDMPropertyEdit::FArguments()
			.InputCount(1)
			.ComponentEditWidget(InArgs._ComponentEditWidget)
			.PropertyHandle(InPropertyHandle)
	);
}

TSharedRef<SWidget> SDMPropertyEditFloat::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	return CreateSpinBox(
		TAttribute<float>::CreateSP(this, &SDMPropertyEditFloat::GetFloatValue),
		FOnFloatValueChanged::CreateSP(this, &SDMPropertyEditFloat::OnValueChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Float)"),
		FloatInterval
	);
}

float SDMPropertyEditFloat::GetMaxWidthForWidget(int32 InIndex) const
{
	ensure(InIndex == 0);

	return UDynamicMaterialEditorSettings::Get()->MaxFloatSliderWidth;
}

float SDMPropertyEditFloat::GetFloatValue() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		float Value;
		PropertyHandle->GetValue(Value);
		return Value;
	}

	return 0.f;
}

void SDMPropertyEditFloat::OnValueChanged(float InNewValue) const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		float CurrentValue = 0.f;

		if (PropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Fail)
		{
			return;
		}

		if (FMath::IsNearlyEqual(CurrentValue, InNewValue))
		{
			return;
		}

		PropertyHandle->SetValue(InNewValue, EPropertyValueSetFlags::NotTransactable | EPropertyValueSetFlags::InteractiveChange);
	}
}

#undef LOCTEXT_NAMESPACE
