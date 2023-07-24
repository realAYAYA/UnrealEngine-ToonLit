// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OpenColorIOColorSpace.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SOpenColorIOColorSpacePicker.h"
#include "Widgets/SWidget.h"

/**
 * Implements a details view customization for the FOpenColorIOColorConversionSettings
 */
class FOpenColorIOColorConversionSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FOpenColorIOColorConversionSettingsCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

	void OnSourceColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView& NewDisplayView);
	void OnDestinationColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView& NewDisplayView);

protected:
	/** Read configuration settings into local selection objects.*/
	void ApplyConfigurationToSelection();
	/** Apply transform selections onto the configuration object.*/
	void ApplySelectionToConfiguration();

private:
	/** Callback to reset the configuration in the transform source/destination pickers. */
	void OnConfigurationReset();

	/** Pointer to the struct SourceColorSpace property handle. */
	TSharedPtr<IPropertyHandle> SourceColorSpaceProperty;
	
	/** Pointer to the struct DestinationColorSpace property handle. */
	TSharedPtr<IPropertyHandle> DestinationColorSpaceProperty;

	/** Pointer to the struct DestinationDisplayView property handle. */
	TSharedPtr<IPropertyHandle> DestinationDisplayViewProperty;

	/** Pointer to the struct DisplayViewDirection property handle. */
	TSharedPtr<IPropertyHandle> DisplayViewDirectionProperty;

	/** ColorSpace pickers reference to update them when config asset is changed */
	TStaticArray<TSharedPtr<SOpenColorIOColorSpacePicker>, 2> TransformPicker;
	/** Intermediate selection objects. This is done to allow inverted display-view selections (when enabled in settings). */
	TStaticArray<FOpenColorIOPickerSelection, 2> TransformSelection;

	/** Raw pointer to the conversion settings struct. */
	struct FOpenColorIOColorConversionSettings* ColorSpaceConversion = nullptr;
};
