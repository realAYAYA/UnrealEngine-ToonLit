// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphApplyCVarPresetNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

FString UMovieGraphApplyCVarPresetNode::GetNodeInstanceName() const
{
	if (ConsoleVariablePreset)
	{
		return ConsoleVariablePreset.GetObject()->GetName();
	}

	return FString();
}

EMovieGraphBranchRestriction UMovieGraphApplyCVarPresetNode::GetBranchRestriction() const
{
	return EMovieGraphBranchRestriction::Globals;
}

#if WITH_EDITOR
FText UMovieGraphApplyCVarPresetNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText ApplyCVarPresetNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_ApplyCVarPreset", "Apply CVar Preset");
	static const FText ApplyCVarPresetNodeDescription = NSLOCTEXT("MovieGraphNodes", "NodeDescription_ApplyCVarPreset", "Apply CVar Preset\n{0}");

	const FString InstanceName = GetNodeInstanceName();
	if (bGetDescriptive && !InstanceName.IsEmpty())
	{
		return FText::Format(ApplyCVarPresetNodeDescription, FText::FromString(InstanceName));
	}
	
	return ApplyCVarPresetNodeName;
}

FText UMovieGraphApplyCVarPresetNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "ApplyCVarPresetGraphNode_Category", "Utility");
}

FText UMovieGraphApplyCVarPresetNode::GetKeywords() const
{
	static const FText Keywords = NSLOCTEXT("MovieGraphNodes", "ApplyCVarPresetGraphNode_Keywords", "cvar console variable preset");
	return Keywords;
}

FLinearColor UMovieGraphApplyCVarPresetNode::GetNodeTitleColor() const
{
	static const FLinearColor ApplyCVarPresetNodeColor = FLinearColor(0.04f, 0.22f, 0.36f);
	return ApplyCVarPresetNodeColor;
}

FSlateIcon UMovieGraphApplyCVarPresetNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon ApplyCVarPresetIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.BrowseCVars");

	OutColor = FLinearColor::White;
	return ApplyCVarPresetIcon;
}

void UMovieGraphApplyCVarPresetNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphApplyCVarPresetNode, ConsoleVariablePreset))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}
#endif // WITH_EDITOR