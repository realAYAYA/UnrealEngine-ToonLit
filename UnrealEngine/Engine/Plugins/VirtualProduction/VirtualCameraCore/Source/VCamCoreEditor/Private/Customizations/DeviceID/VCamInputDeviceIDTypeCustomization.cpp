// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputDeviceIDTypeCustomization.h"

#include "Input/VCamInputDeviceConfig.h"
#include "SInputDeviceSelector.h"

#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Misc/Optional.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FVCamInputDeviceIDTypeCustomization"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IPropertyTypeCustomization> FVCamInputDeviceIDTypeCustomization::MakeInstance()
	{
		return MakeShared<FVCamInputDeviceIDTypeCustomization>();
	}

	void FVCamInputDeviceIDTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		PropertyHandle = StructPropertyHandle;
		Utils = StructCustomizationUtils.GetPropertyUtilities();

		// create struct header
		HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(325.0f)
		[
			SNew(SInputDeviceSelector)
			.CurrentInputDeviceID(this, &FVCamInputDeviceIDTypeCustomization::GetCurrentInputDeviceID)
			.OnInputDeviceIDChanged(this, &FVCamInputDeviceIDTypeCustomization::OnInputDeviceIDChanged)
		];
	}

	TOptional<int32> FVCamInputDeviceIDTypeCustomization::GetCurrentInputDeviceID() const
	{
		TArray<void*> StructPtrs;
		PropertyHandle->AccessRawData(StructPtrs);

		if (StructPtrs.Num() > 0)
		{
			FVCamInputDeviceID* const SelectedKey = static_cast<FVCamInputDeviceID*>(StructPtrs[0]);
			return SelectedKey->DeviceId;
		}

		return INDEX_NONE;
	}
	
	void FVCamInputDeviceIDTypeCustomization::OnInputDeviceIDChanged(int32 InputDeviceID)
	{
		const FVCamInputDeviceID Value { InputDeviceID };
		// Otherwise ExportText will produce "()" for InputDeviceID == 0 ... which makes SetValueFromFormattedString fail
		const FVCamInputDeviceID DummyDefaults { -42 };
		
		FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(PropertyHandle->GetProperty());
		FString TextValue;
		StructProperty->Struct->ExportText(TextValue, &Value, &DummyDefaults, nullptr, PPF_None, nullptr);

		{
			FScopedTransaction Transaction(LOCTEXT("SetInputDevice", "Set input device ID"));
			PropertyHandle->NotifyPreChange();
			PropertyHandle->SetValueFromFormattedString(TextValue);
			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}

		// For some reason the numeric box does not update sometimes... force refresh fixes that and has no mayor downsides.
		Utils->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE