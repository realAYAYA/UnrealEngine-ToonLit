// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"


/** Details customization for the DMMXFixturePatch class */
class FDMXProtocolSettingsDetails
	: public IDetailCustomization
{
public:
	/** Constructor */
	FDMXProtocolSettingsDetails();

	/** Creates a detail customization instance */
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface

private:
	/** True if the DMX Engine Plugin is enabled */
	const bool bIsDMXEnginePluginEnabled;
};
