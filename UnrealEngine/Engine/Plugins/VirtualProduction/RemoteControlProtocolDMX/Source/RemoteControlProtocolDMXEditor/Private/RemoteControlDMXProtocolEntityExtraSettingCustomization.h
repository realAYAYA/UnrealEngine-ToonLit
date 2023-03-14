// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"

#include "CoreMinimal.h"

class SDMXPortSelector;

class IPropertyUtilities;

/**
 * Type Customization for Protocol DMX editor
 */
class FRemoteControlDMXProtocolEntityExtraSettingCustomization final : public IPropertyTypeCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	
	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> InStructProperty, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> InStructProperty, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	//~ End IPropertyTypeCustomization Interface

private:
	/** Called when the input port changed externally */
	void OnInputPortChanged();

	/** On fixture signal format property value change handler */
	void OnFixtureSignalFormatChange();
	
	/** On starting channel property value change handler */
	void OnStartingChannelChange();

	/** Called when a port was selected */
	void OnPortSelected();

	/**
	 * Checking if starting channel plus fixture signal format size fits into DMX_UNIVERSE_SIZE.
	 * If that is fit do nothing, otherwise remove decreasing the starting channel by overhead value.
	 */
	void CheckAndApplyStartingChannelValue();

	/** Returns the Guid of the Input Port in use */
	FGuid GetPortGuid() const;

	/** Sets the Guid of the Input Port to use */
	void SetPortGuid(const FGuid& PortGuid);

	/** Returns if the port selector should be enabled */
	bool GetIsPortSelectorEnabled();

private:
	/** Property handle for the bUseDefaultInputPort property */
	TSharedPtr<IPropertyHandle> UseDefaultInputPortHandle;

	/** Signal format handle. 1,2,3 or 4 bytes signal format */
	TSharedPtr<IPropertyHandle> FixtureSignalFormatHandle;

	/** Starting channel handle */
	TSharedPtr<IPropertyHandle> StartingChannelPropertyHandle;

	/** Handle for the InputPortId property */
	TSharedPtr<IPropertyHandle> InputPortIdHandle;

	/** Widget to select a port */
	TSharedPtr<SDMXPortSelector> PortSelector;

	/** Property utilities for this customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
