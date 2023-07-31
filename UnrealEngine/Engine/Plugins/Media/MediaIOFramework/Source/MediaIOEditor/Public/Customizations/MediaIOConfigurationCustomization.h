// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/MediaIOCustomizationBase.h"
#include "Input/Reply.h"
#include "MediaIOCoreDefinitions.h"

/**
 * Implements a details view customization for the FMediaIOConfiguration
 */
class MEDIAIOEDITOR_API FMediaIOConfigurationCustomization : public FMediaIOCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:
	virtual TAttribute<FText> GetContentText() override;
	virtual TSharedRef<SWidget> HandleSourceComboButtonMenuContent() override;

	ECheckBoxState GetAutoCheckboxState() const;
	void SetAutoCheckboxState(ECheckBoxState CheckboxState);
	void OnSelectionChanged(FMediaIOConfiguration SelectedItem);
	FReply OnButtonClicked();
	bool ShowAdvancedColumns(FName ColumnName, const TArray<FMediaIOConfiguration>& UniquePermutationsForThisColumn) const;
	bool IsAutoDetected() const;
	void SetIsAutoDetected(bool Value);

private:
	TWeakPtr<SWidget> PermutationSelector;
	FMediaIOConfiguration SelectedConfiguration;
	bool bAutoDetectFormat = true;
};
