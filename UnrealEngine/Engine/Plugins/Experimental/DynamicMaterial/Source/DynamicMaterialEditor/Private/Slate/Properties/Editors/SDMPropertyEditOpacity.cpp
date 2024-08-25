// Copyright Epic Games, Inc.All Rights Reserved.

#include "SDMPropertyEditOpacity.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "DynamicMaterialEditorSettings.h"
#include "Slate/SDMEditor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SDMPropertyEditOpacity"

void SDMPropertyEditOpacity::Construct(const FArguments& InArgs, const TSharedPtr<SWidget>& InWidget, UDMMaterialValueFloat1* InOpacityValue)
{
	if (ensure(IsValid(InOpacityValue)))
	{
		Super::Construct(
			Super::FArguments(),
			SDMEditor::GetPropertyHandle(InWidget.Get(), InOpacityValue, UDMMaterialValue::ValueName).PropertyHandle
		);
	}
}

TSharedRef<SWidget> SDMPropertyEditOpacity::GetComponentWidget(int32 InIndex)
{
	ensure(InIndex == 0);

	static const FFloatInterval OpacityInterval(0.f, 1.f);

	return CreateSpinBox(
		TAttribute<float>::CreateSP(this, &SDMPropertyEditOpacity::GetFloatValue),
		FOnFloatValueChanged::CreateSP(this, &SDMPropertyEditOpacity::OnValueChanged),
		LOCTEXT("TransactionDescription", "Material Designer Value Scrubbing (Opacity)"),
		&OpacityInterval
	);
}

float SDMPropertyEditOpacity::GetMaxWidthForWidget(int32 InIndex) const
{
	// Disable
	return TNumericLimits<float>::Max();
}

#undef LOCTEXT_NAMESPACE
