// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphWarmUpSettingNode.h"
#include "Styling/AppStyle.h"

UMovieGraphWarmUpSettingNode::UMovieGraphWarmUpSettingNode()
	: NumWarmUpFrames(0)
	, bEmulateMotionBlur(false)
{}

EMovieGraphBranchRestriction UMovieGraphWarmUpSettingNode::GetBranchRestriction() const 
{ 
	return EMovieGraphBranchRestriction::Globals; 
}

#if WITH_EDITOR
FText UMovieGraphWarmUpSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText WarmUpSettingsNodeName = NSLOCTEXT("MoviePipelineGraph", "NodeName_WarmUpSettings", "Warm Up Settings");
	return WarmUpSettingsNodeName;
}

FText UMovieGraphWarmUpSettingNode::GetMenuCategory() const 
{
	return NSLOCTEXT("MoviePipelineGraph", "Settings_Category", "Settings");
}

FLinearColor UMovieGraphWarmUpSettingNode::GetNodeTitleColor() const 
{
	static const FLinearColor WarmUpSettingsColor = FLinearColor(0.854f, 0.509f, 0.039f);
	return WarmUpSettingsColor;
}

FSlateIcon UMovieGraphWarmUpSettingNode::GetIconAndTint(FLinearColor& OutColor) const 
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}
#endif // WITH_EDITOR