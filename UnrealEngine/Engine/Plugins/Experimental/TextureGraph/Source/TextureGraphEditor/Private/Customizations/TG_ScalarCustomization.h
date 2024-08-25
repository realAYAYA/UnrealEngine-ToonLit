// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSlider.h"
#include "Editor.h"
#include "STG_TextureHistogram.h"


#define LOCTEXT_NAMESPACE "FTextureGraphEditorModule"

class FTG_ScalarTypeIdentifier : public IPropertyTypeIdentifier
{
public:
	virtual bool IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const override
	{
		return PropertyHandle.HasMetaData(TG_MetadataSpecifiers::MD_ScalarEditor);
	}
};

class FTG_ScalarCustomization : public IPropertyTypeCustomization
{
	bool bIsUsingSlider = false;
	TSharedPtr<IPropertyHandle> ScalarHandle;

public:
	static TSharedRef<IPropertyTypeCustomization> Create()
	{
		return MakeShareable(new FTG_ScalarCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		ScalarHandle = PropertyHandle;
		check(ScalarHandle.IsValid());

		float FloatValue;
		ScalarHandle->GetValue(FloatValue);

		TWeakPtr<IPropertyHandle> ScalarHandlePtr = ScalarHandle;

		// Get min/max metadata values if defined
		float MinAllowedValue = 0.0f;
		float MinUIValue = 0.0f;
		float MaxAllowedValue = 1.0f;
		float MaxUIValue = 1.0f;
		if (ScalarHandle->HasMetaData(TEXT("ClampMin")))
		{
			MinAllowedValue = ScalarHandle->GetFloatMetaData(TEXT("ClampMin"));
		}
		if (ScalarHandle->HasMetaData(TEXT("UIMin")))
		{
			MinUIValue = ScalarHandle->GetFloatMetaData(TEXT("UIMin"));
		}
		if (ScalarHandle->HasMetaData(TEXT("ClampMax")))
		{
			MaxAllowedValue = ScalarHandle->GetFloatMetaData(TEXT("ClampMax"));
		}
		if (ScalarHandle->HasMetaData(TEXT("UIMax")))
		{
			MaxUIValue = ScalarHandle->GetFloatMetaData(TEXT("UIMax"));
		}

		// Build the  ui
		HeaderRow
			.NameContent()
			[
				SNew(STextBlock)
				.Text(PropertyHandle->GetPropertyDisplayName())
				.Font(CustomizationUtils.GetRegularFont())
			]
			.ValueContent()
			.MinDesiredWidth(STG_TextureHistogram::PreferredWidth)
			[
				SNew(SSlider)
				.Value(this, &FTG_ScalarCustomization::OnGetValue, ScalarHandlePtr)
				.MinValue(MinAllowedValue)
				.MaxValue(MaxAllowedValue)
				.OnValueChanged(this, &FTG_ScalarCustomization::OnValueChanged, ScalarHandlePtr)
				.OnMouseCaptureBegin(this, &FTG_ScalarCustomization::OnBeginSliderMovement)
				.OnMouseCaptureEnd(this, &FTG_ScalarCustomization::OnEndSliderMovement)
			];
	}

	float OnGetValue(TWeakPtr<IPropertyHandle> HandleWeakPtr) const
	{
		auto ValueSharedPtr = HandleWeakPtr.Pin();
		float Value = 0;
		if (ValueSharedPtr.IsValid())
		{
			ensure(ValueSharedPtr->GetValue(Value) == FPropertyAccess::Success);
		}
		return Value;
	}

	void OnBeginSliderMovement()
	{
		bIsUsingSlider = true;
		GEditor->BeginTransaction(LOCTEXT("SetScalarProperty", "Set Scalar Property"));
	}

	void OnValueChanged(float NewValue, TWeakPtr<IPropertyHandle> HandleWeakPtr)
	{
		if (bIsUsingSlider)
		{
			auto HandleSharedPtr = HandleWeakPtr.Pin();
			if (HandleSharedPtr.IsValid())
			{
				ensure(HandleSharedPtr->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success);
			}
		}
	}

	void OnEndSliderMovement()
	{
		bIsUsingSlider = false;
		GEditor->EndTransaction();
	}


	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
	}
};

#undef LOCTEXT_NAMESPACE