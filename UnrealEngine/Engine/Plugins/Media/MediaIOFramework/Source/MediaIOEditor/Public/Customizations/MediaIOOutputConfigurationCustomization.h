// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/MediaIOCustomizationBase.h"
#include "Input/Reply.h"
#include "MediaIOCoreDefinitions.h"

/**
 * Implements a details view customization for the FMediaIOOutputConfiguration
 */
class MEDIAIOEDITOR_API FMediaIOOutputConfigurationCustomization : public FMediaIOCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:
	virtual TAttribute<FText> GetContentText() override;
	virtual TSharedRef<SWidget> HandleSourceComboButtonMenuContent() override;

	void OnSelectionChanged(FMediaIOOutputConfiguration SelectedItem);
	FReply OnButtonClicked() const;

	TWeakPtr<SWidget> PermutationSelector;
	FMediaIOOutputConfiguration SelectedConfiguration;
};
