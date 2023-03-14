// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"

template<class T>
class SComboBox;


class FDisplayClusterConfiguratorMediaCustomization final
	: public FDisplayClusterConfiguratorBaseTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorMediaCustomization>();
	}

protected:
	//~ IPropertyTypeCustomization interface begin
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ IPropertyTypeCustomization interface end

private:
	/**  Generates internal array of cluster node IDs */
	void BuildClusterNodeOptionsList();

	/**  Processes combobox selection changes */
	void OnNodeIdSelected(TSharedPtr<FString> PreviewNodeId, ESelectInfo::Type SelectInfo);

	/**  Returns text of currently selected item */
	FText GetSelectedNodeIdText() const;

	/** Creates combobox widget */
	TSharedRef<SWidget> CreateComboWidget(TSharedPtr<FString> InItem);

private:
	/** OutputNode original property handler */
	TSharedPtr<IPropertyHandle> OutputNodeProperty;

	/** List of cluster node IDs for the combobox */
	TArray<TSharedPtr<FString>> OutputNodeOptions;

	/** Node ID selection combobox */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> OutputNodeComboBox;
};
