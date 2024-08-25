// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class SBox;

class FAvaLinearGradientSettingsCustomization : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// BEGIN IPropertyTypeCustomization interface
	
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
		class IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	bool IsCustomDirectionEnabled() const;

	// END IPropertyTypeCustomization interface

	static FLinearColor GetLinearColorFromProperty(const TSharedPtr<IPropertyHandle>& InColorPropertyHandle);

private:
	
	TSharedPtr<IPropertyHandle> Direction_Handle;
	TSharedPtr<SBox> HeaderContentBox;
};
