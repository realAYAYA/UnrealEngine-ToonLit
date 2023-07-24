// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class SDMXControlConsoleEditorPortSelector;

class IPropertyUtilities;


/** Details Customization for DMX DMX Control Console */
class FDMXControlConsoleDataDetails
	: public IDetailCustomization
{
public:
	/** Makes an instance of this Details Customization */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FDMXControlConsoleDataDetails>();
	}

	//~ Begin of IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
	//~ End of IDetailCustomization interface

private:
	/** Generates the Port Selector for the DMX Control Console being edited */
	void GeneratePortSelectorRow(IDetailLayoutBuilder& InDetailLayout);

	/** Forces a refresh on the entire Details View */
	void ForceRefresh() const;

	/** Called when Port selection changes */
	void OnSelectedPortsChanged();

	/** Widget to handle Port selection */
	TSharedPtr<SDMXControlConsoleEditorPortSelector> PortSelector;

	/** Property Utilities for this Details Customization layout */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
