// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDebugNode.h"

#include "Styling/AppStyle.h"

#if WITH_EDITOR
FText UMovieGraphDebugSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText NodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_DebugNode", "Debug Settings");
	return NodeName;
}

FText UMovieGraphDebugSettingNode::GetMenuCategory() const
{
	static const FText NodeCategory_Globals = NSLOCTEXT("MovieGraphNodes", "NodeCategory_Globals", "Globals");
	return NodeCategory_Globals;
}

FLinearColor UMovieGraphDebugSettingNode::GetNodeTitleColor() const
{
	static const FLinearColor NodeColor = FLinearColor(0.849f, 0.f, 0.350f);
	return NodeColor;
}

FSlateIcon UMovieGraphDebugSettingNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Debug");

	OutColor = FLinearColor::White;
	return Icon;
}
#endif // WITH_EDITOR