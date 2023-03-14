// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"


struct FMediaProfileSettingsCustomizationOptions;
class IDetailLayoutBuilder;


/**
 *
 */
class FMediaProfileSettingsCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//~ End IDetailCustomization Interface

private:
	// Setup the proxies for the media profile
	FReply OnConfigureClicked();

	FSlateColor GetBorderColor() const;
	int32 GetConfigurationStateAsInt() const;

	void Configure(const FMediaProfileSettingsCustomizationOptions& SettingOptions);

	bool bNotConfigured;
	bool bNeedFixup;
};
