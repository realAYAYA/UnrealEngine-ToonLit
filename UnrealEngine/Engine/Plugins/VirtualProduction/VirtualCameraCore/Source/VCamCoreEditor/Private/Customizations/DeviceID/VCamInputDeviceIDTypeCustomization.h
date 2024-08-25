// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyUtilities;

template<typename T>
struct TOptional;

namespace UE::VCamCoreEditor::Private
{
	/** Customizes FVCamInputDeviceID to display a button the user can use to listen for input from any device and numeric box for manual entry */
	class FVCamInputDeviceIDTypeCustomization : public IPropertyTypeCustomization
	{
	public:

		/** Util that just creates an instance */
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IPropertyTypeCustomization Interface
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override { }
		//~ End IPropertyTypeCustomization Interface

	private:
		
		/** Holds a handle to the property being edited. */
		TSharedPtr<IPropertyHandle> PropertyHandle;
		/** Needed to force refresh */
		TSharedPtr<IPropertyUtilities> Utils;

		/** Reads the current input device ID from PropertyHandle */
		TOptional<int32> GetCurrentInputDeviceID() const;
		/** Called when SInputDeviceSelector has determined a new input device ID. */
		void OnInputDeviceIDChanged(int32 InputDeviceID);
	};
}
