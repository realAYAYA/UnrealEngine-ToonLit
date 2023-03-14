// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Profile/MediaProfileSettingsCustomization.h"


/**
 *
 */
class FMediaProfileCustomization : public FMediaProfileSettingsCustomization
{
private:
	using Super = FMediaProfileSettingsCustomization;

public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//~ End IDetailCustomization Interface
};
