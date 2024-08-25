// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

struct EVisibility;
namespace ESelectInfo { enum Type : int; }
class IDetailLayoutBuilder;
class STextComboBox;
class UCustomizableObjectNodeObjectGroup;

class FCustomizableObjectNodeObjectGroupDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	/** Fills the name options array */
	void GenerateChildrenObjectNames();

	/** Sets the selected default value */
	void OnSetDefaultValue(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	
	/** Determines the visibility of the default value selector */
	EVisibility DefaultValueSelectorVisibility() const;

private:

	/** Pointer to the original group node */
	UCustomizableObjectNodeObjectGroup* NodeGroup = nullptr;

	/** Combobox to select the defaul value of the node group */
	TSharedPtr<STextComboBox> DefaultValueSelector;

	/** Combobox options */
	TArray< TSharedPtr<FString> > ChildrenNameOptions;

	/** Initially selected option */
	TSharedPtr<FString> InitialNameOption;

};
