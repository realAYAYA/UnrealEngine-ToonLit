// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class FInterchangeMaterialXPipelineSettingsCustomization : public IPropertyTypeCustomization
{
public:

	/**
	 * Creates an instance of this class
	 */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

protected:

	bool OnShouldFilterAssetStandardSurface(const FAssetData & InAssetData);
	bool OnShouldFilterAssetStandardSurfaceTransmission(const FAssetData& InAssetData);
};