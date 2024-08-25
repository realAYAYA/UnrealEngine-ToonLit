// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "Param/ParamType.h"

namespace UE::AnimNext::Editor
{

class FParamPropertyTypeCustomization : public IPropertyTypeCustomization
{
private:
	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	void Refresh();

private:
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> NamePropertyHandle;
	TSharedPtr<IPropertyHandle> TypePropertyHandle;
	FName CachedName = NAME_None;
	FText CachedNameText;
	FAnimNextParamType CachedType;
};

}
