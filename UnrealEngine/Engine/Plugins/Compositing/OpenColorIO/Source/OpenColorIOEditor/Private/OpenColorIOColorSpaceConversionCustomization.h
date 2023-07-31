// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
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

private:
	void AddDestinationModeRow(IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils);
	void AddPropertyRow(FDetailWidgetRow& InWidgetRow, TSharedRef<IPropertyHandle> InChildHandle, IPropertyTypeCustomizationUtils& InCustomizationUtils, bool bIsDisplayViewRow) const;
	
	TSharedRef<SWidget> HandleColorSpaceComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const;
	TSharedRef<SWidget> HandleDisplayViewComboButtonMenuContent(TSharedPtr<IPropertyHandle> InPropertyHandle) const;

	void SetDestinationMode(bool bInIsDestinationDisplayView);
	/** Controls visibility for widgets when the destination is a color space. */
	EVisibility ShouldShowDestinationColorSpace() const;

	/** Controls visibility for widgets when the destination is a display-view. */
	EVisibility ShouldShowDestinationDisplayView() const;

	/** Pointer to the ColorConversion struct property handle. */
	TSharedPtr<IPropertyHandle> ColorConversionProperty;
	
	/** Pointer to the ColorConversion struct member SourceColorSpace property handle. */
	TSharedPtr<IPropertyHandle> SourceColorSpaceProperty;
	
	/** Pointer to the ColorConversion struct member DestinationColorSpace property handle. */
	TSharedPtr<IPropertyHandle> DestinationColorSpaceProperty;

	/** Pointer to the ColorConversion struct member DestinationColorSpace property handle. */
	TSharedPtr<IPropertyHandle> DestinationDisplayViewProperty;

	bool bIsDestinationDisplayView = false;
};
