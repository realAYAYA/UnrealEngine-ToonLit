// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphGetCVarValueNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

TArray<FMovieGraphPinProperties> UMovieGraphGetCVarValueNode::GetOutputPinProperties() const
{
	TArray<FMovieGraphPinProperties> OutputPinProperties = Super::GetOutputPinProperties();

	FMovieGraphPinProperties Properties(FName("Value"), EMovieGraphValueType::String, false);
	OutputPinProperties.Add(MoveTemp(Properties));

	return OutputPinProperties;
}

#if WITH_EDITOR
FText UMovieGraphGetCVarValueNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText GetCVarNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_GetCVar", "Get CVar Value");
	static const FText GetCVarNodeDescription = NSLOCTEXT("MovieGraphNodes", "NodeDescription_GetCVar", "Get CVar Value\n{0}");

	if (bGetDescriptive && !Name.IsEmpty())
	{
		return FText::Format(GetCVarNodeDescription, FText::FromString(Name));
	}
	
	return GetCVarNodeName;
}

FText UMovieGraphGetCVarValueNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "GetCVarGraphNode_Category", "Utility");
}

FLinearColor UMovieGraphGetCVarValueNode::GetNodeTitleColor() const
{
	static const FLinearColor GetCVarNodeColor = FLinearColor(0.04f, 0.22f, 0.36f);
	return GetCVarNodeColor;
}

FSlateIcon UMovieGraphGetCVarValueNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon RenderLayerIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.BrowseCVars");

	OutColor = FLinearColor::White;
	return RenderLayerIcon;
}

void UMovieGraphGetCVarValueNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphGetCVarValueNode, Name))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}
#endif // WITH_EDITOR