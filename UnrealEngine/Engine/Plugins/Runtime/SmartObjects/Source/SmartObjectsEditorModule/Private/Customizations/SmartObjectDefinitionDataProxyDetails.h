// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class USmartObjectDefinition;
class SComboButton;
class SWidget;
class FSmartObjectViewModel;

/**
 * Type customization for FSmartObjectDefinitionDataProxy.
 */

class FSmartObjectDefinitionDataProxyDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	FText GetDefinitionDataName() const;
	void OnCopy() const;
	void OnPaste() const;
	TSharedPtr<FSmartObjectViewModel> GetViewModel() const;
	FGuid GetItemID() const;

	TSharedRef<SWidget> GenerateStructPicker();
	void OnStructPicked(const UScriptStruct* InStruct);

	TSharedPtr<IPropertyUtilities> PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> DataPropertyHandle;
	TSharedPtr<SComboButton> ComboButton;
};
