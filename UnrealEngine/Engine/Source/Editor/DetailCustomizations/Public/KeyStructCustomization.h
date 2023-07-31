// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "InputCoreTypes.h"
#include "Misc/Optional.h"
#include "PropertyHandle.h"
#include "SKeySelector.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;
struct FKey;

/**
 * Implements a details panel customization for FKey structures.
 * As  "Key"				<SKeySelector>
 */
class DETAILCUSTOMIZATIONS_API FKeyStructCustomization
	: public IPropertyTypeCustomization
{
public:
	// IPropertyTypeCustomization interface

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override { };
	virtual bool ShouldInlineKey() const override { return true; }

	// Helper variant that generates the key struct in the header and appends a single button at the end
	// TODO: Is there a better way?
	void CustomizeHeaderOnlyWithButton(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils, TSharedRef<SWidget> Button);

	/** Sets a bool for whether the key selector should be enabled or not */
	void SetEnableKeySelector(bool bKeySelectorEnabled);
	/** Sets a bool for whether the information Icon for a combo trigger is displayed or not */ 
	void SetDisplayIcon(bool bShouldDisplayIcon)
	{
		bDisplayIcon = bShouldDisplayIcon;
	}

	/** Gets bEnableKeySelector bool */
	bool GetEnableKeySelector() const
	{
		return bEnableKeySelector;
	}
	
	/** Gets bDisplayIcon bool */
    bool GetDisplayIcon() const
    {
    	return bDisplayIcon;
    }

public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for Keys.
	 */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance( );

protected:
	/** Whether the information icon for combo triggers is displayed or not */
	bool bDisplayIcon = false;
	/** Whether the key selector should be enabled or not */
	bool bEnableKeySelector = true;

	/** Gets the current Key being edited. */
	TOptional<FKey> GetCurrentKey() const;

	/** Updates the property when a new key is selected. */
	void OnKeyChanged(TSharedPtr<FKey> SelectedKey);

	/** Holds a handle to the property being edited. */
	TSharedPtr<IPropertyHandle> PropertyHandle;
	
	/** shared pointer to the Key Selector. */
    TSharedPtr<SKeySelector> KeySelector = nullptr;
};
