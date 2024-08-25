// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class USmartObjectDefinition;
class IPropertyUtilities;

/**
 * Type customization for FSmartObjectDefinitionReference.
 */
class FSmartObjectDefinitionReferenceDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;

private:

	void OnParametersChanged(const USmartObjectDefinition& SmartObjectDefinition) const;
	
	/**
	 * Synchronizes parameters with SmartObject Definition asset parameters if needed.
	 * @param DefinitionToSync Optional SmartObject Definition asset used to filter which parameters should be synced. A null value indicates that all parameters should be synced.   
	 */
	void SyncParameters(const USmartObjectDefinition* DefinitionToSync = nullptr) const;

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> ParametersHandle;
	TSharedPtr<IPropertyUtilities> PropUtils;
};
