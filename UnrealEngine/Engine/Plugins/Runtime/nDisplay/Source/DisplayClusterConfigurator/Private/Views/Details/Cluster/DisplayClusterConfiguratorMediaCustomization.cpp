// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Details/Cluster/DisplayClusterConfiguratorMediaCustomization.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfiguratorBlueprintEditor.h"
#include "DisplayClusterRootActor.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"


void FDisplayClusterConfiguratorMediaCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	OutputNodeProperty = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDisplayClusterConfigurationMedia, MediaSharingNode));
	OutputNodeProperty->MarkHiddenByCustomization();

	BuildClusterNodeOptionsList();

	// Create GUI representation
	{
		InChildBuilder.AddCustomRow(OutputNodeProperty->GetPropertyDisplayName())
			.NameContent()
			[
				OutputNodeProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SAssignNew(OutputNodeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&OutputNodeOptions)
				.OnGenerateWidget(this, &FDisplayClusterConfiguratorMediaCustomization::CreateComboWidget)
				.OnSelectionChanged(this, &FDisplayClusterConfiguratorMediaCustomization::OnNodeIdSelected)
				.ContentPadding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &FDisplayClusterConfiguratorMediaCustomization::GetSelectedNodeIdText)
				]
			];
	}

	// Set selected item
	FString CurrentNodeId;
	if (OutputNodeProperty->GetValue(CurrentNodeId))
	{
		TSharedPtr<FString>* FoundItem = OutputNodeOptions.FindByPredicate([CurrentNodeId](const TSharedPtr<FString>& Item)
		{
			return Item->Equals(CurrentNodeId, ESearchCase::IgnoreCase);
		});

		if (FoundItem)
		{
			OutputNodeComboBox->SetSelectedItem(*FoundItem);
		}
	}

	FDisplayClusterConfiguratorBaseTypeCustomization::CustomizeChildren(InPropertyHandle, InChildBuilder, CustomizationUtils);
}

void FDisplayClusterConfiguratorMediaCustomization::BuildClusterNodeOptionsList()
{
	ADisplayClusterRootActor* DCRA = nullptr;

	if (FDisplayClusterConfiguratorBlueprintEditor* Editor = FindBlueprintEditor())
	{
		DCRA = Editor->GetDefaultRootActor();
	}

	if (!DCRA)
	{
		DCRA = FindRootActor();
	}
	
	if (!DCRA)
	{
		return;
	}

	// Get cluster node IDs
	TArray<FString> NodeIds;
	if (const UDisplayClusterConfigurationData* const ConfigurationData = DCRA->GetConfigData())
	{
		ConfigurationData->Cluster->GetNodeIds(NodeIds);
	}

	// Sort the node IDs in alphabet order
	NodeIds.Sort();

	// Prepare combobox options
	OutputNodeOptions.Reset();
	for (const FString& NodeId : NodeIds)
	{
		if (!NodeId.IsEmpty())
		{
			OutputNodeOptions.Emplace(MakeShared<FString>(NodeId));
		}
	}
}


void FDisplayClusterConfiguratorMediaCustomization::OnNodeIdSelected(TSharedPtr<FString> PreviewNodeId, ESelectInfo::Type SelectInfo)
{
	const FString NewValue = PreviewNodeId.IsValid() ? *PreviewNodeId : FString();
	OutputNodeProperty->SetValue(NewValue);
}

FText FDisplayClusterConfiguratorMediaCustomization::GetSelectedNodeIdText() const
{
	TSharedPtr<FString> CurSelection = OutputNodeComboBox->GetSelectedItem();
	return FText::FromString(CurSelection.IsValid() ? *CurSelection : *FString());
}

TSharedRef<SWidget> FDisplayClusterConfiguratorMediaCustomization::CreateComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}
