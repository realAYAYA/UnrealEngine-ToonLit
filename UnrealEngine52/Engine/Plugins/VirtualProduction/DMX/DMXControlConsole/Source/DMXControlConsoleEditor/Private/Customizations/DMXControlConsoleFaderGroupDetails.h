// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

struct EVisibility;
class FReply;
class IPropertyUtilities;


/** Details Customization for DMX DMX Control Console */
class FDMXControlConsoleFaderGroupDetails
	: public IDetailCustomization
{
public:
	/** Makes an instance of this Details Customization */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FDMXControlConsoleFaderGroupDetails>();
	}

	//~ Begin of IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
	//~ End of IDetailCustomization interface

private:
	/** Forces refresh of the entire Details View */
	void ForceRefresh() const;

	/** True if at least one selected Fader Group have any Fixture Patch bound */
	bool DoSelectedFaderGroupsHaveAnyFixturePatches() const;

	/** Called when Clear button is clicked */
	FReply OnClearButtonClicked();

	/** Gets current selected FaderGroup Fixture Patch name */
	FText GetFixturePatchText() const;

	/** Gets visibility attribute of the Editor Color Property */
	EVisibility GetEditorColorVisibility() const;

	/** Gets visibility attribute of the Clear button */
	EVisibility GetClearButtonVisibility() const;

	/** Property Utilities for this Details Customization layout */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
