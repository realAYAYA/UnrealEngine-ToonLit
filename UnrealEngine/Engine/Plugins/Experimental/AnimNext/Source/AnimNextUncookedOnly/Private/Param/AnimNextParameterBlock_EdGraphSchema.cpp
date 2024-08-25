// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock_EdGraphSchema.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEntry.h"

#define LOCTEXT_NAMESPACE "AnimNextGraph_EdGraphSchema"

void UAnimNextParameterBlock_EdGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);
	
	if(UAnimNextRigVMAssetEntry* AssetEntry = Cast<UAnimNextRigVMAssetEntry>(Graph.GetOuter()))
	{
		DisplayInfo.DisplayName = FText::Format(LOCTEXT("GraphTabTitleFormat", "{0}: {1}"), FText::FromName(AssetEntry->GetEntryName()), FText::FromName(AssetEntry->GetTypedOuter<UAnimNextRigVMAsset>()->GetFName()));
		DisplayInfo.Tooltip = FText::Format(LOCTEXT("GraphTabTooltipFormat", "{0} in:\n{1}"), FText::FromName(AssetEntry->GetEntryName()), FText::FromString(AssetEntry->GetTypedOuter<UAnimNextRigVMAsset>()->GetPathName()));
	}
}

#undef LOCTEXT_NAMESPACE