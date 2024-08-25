// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "Param/ParamType.h"

namespace UE::AnimNext::Editor
{

class FParamNamePropertyTypeIdentifier : public IPropertyTypeIdentifier
{
private:
	virtual bool IsPropertyTypeCustomized(const IPropertyHandle& InPropertyHandle) const override;
};

class FParamNamePropertyTypeCustomization : public IPropertyTypeCustomization
{
private:
	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	void Refresh();
	
private:
	TWeakPtr<IPropertyHandle> WeakPropertyHandle;
	FName CachedName = NAME_None;
	FText CachedNameText;
	FAnimNextParamType CachedType;
};

}
