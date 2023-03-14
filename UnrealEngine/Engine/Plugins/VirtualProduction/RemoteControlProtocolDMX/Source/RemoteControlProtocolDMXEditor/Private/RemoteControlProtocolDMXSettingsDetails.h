// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
class SDMXPortSelector;

/**
 * Type Customization for Protocol DMX editor
 */
class FRemoteControlProtocolDMXSettingsDetails final : public IDetailCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface

private:
	/** Called when a port was selected */
	void OnPortSelected();

	/** Returns the Guid of the Input Port in use */
	FGuid GetPortGuid() const;

	/** Sets the Guid of the Input Port to use */
	void SetPortGuid(const FGuid& PortGuid);

private:
	/** Handle for the InputPortId property */
	TSharedPtr<IPropertyHandle> DefaultInputPortIdHandle;

	/** Widget to select a port */
	TSharedPtr<SDMXPortSelector> PortSelector;

	/** Property utilities for this customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
