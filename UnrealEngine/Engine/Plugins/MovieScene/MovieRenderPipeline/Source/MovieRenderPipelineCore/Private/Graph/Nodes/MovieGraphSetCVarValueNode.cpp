// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphSetCVarValueNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

#if WITH_EDITOR
FText UMovieGraphSetCVarValueNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText SetCVarNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_SetCVar", "Set CVar Value");
	static const FText SetCVarNodeDescription = NSLOCTEXT("MovieGraphNodes", "NodeDescription_SetCVar", "Set CVar Value\n{0}");

	if (bGetDescriptive && !Name.IsEmpty())
	{
		return FText::Format(SetCVarNodeDescription, FText::FromString(Name));
	}
	
	return SetCVarNodeName;
}

FText UMovieGraphSetCVarValueNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "SetCVarGraphNode_Category", "Utility");
}

FLinearColor UMovieGraphSetCVarValueNode::GetNodeTitleColor() const
{
	static const FLinearColor SetCVarNodeColor = FLinearColor(0.04f, 0.22f, 0.36f);
	return SetCVarNodeColor;
}

FSlateIcon UMovieGraphSetCVarValueNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon RenderLayerIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.BrowseCVars");

	OutColor = FLinearColor::White;
	return RenderLayerIcon;
}

void UMovieGraphSetCVarValueNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphSetCVarValueNode, Name))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}

#endif // WITH_EDITOR