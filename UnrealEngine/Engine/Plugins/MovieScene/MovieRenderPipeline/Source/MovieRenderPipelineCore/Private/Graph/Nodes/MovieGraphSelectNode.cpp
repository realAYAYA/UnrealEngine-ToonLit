// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphSelectNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

namespace UE::MovieGraph::SelectNode
{
	static const FName SelectedOption("Selected Option");
}

TArray<FMovieGraphPinProperties> UMovieGraphSelectNode::GetInputPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;

	// Generate branch pins for each option
	for (const FString& SelectOption : SelectOptions)
	{
		Properties.Add(FMovieGraphPinProperties::MakeBranchProperties(FName(SelectOption)));
	}

	Properties.Add(FMovieGraphPinProperties(UE::MovieGraph::SelectNode::SelectedOption, EMovieGraphValueType::String, false));
	
	return Properties;
}

TArray<FMovieGraphPinProperties> UMovieGraphSelectNode::GetOutputPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;
	Properties.Add(FMovieGraphPinProperties::MakeBranchProperties());
	return Properties;
}

TArray<UMovieGraphPin*> UMovieGraphSelectNode::EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const
{
	TArray<UMovieGraphPin*> PinsToFollow;

	// The resolved value of the "Selected Option" property. May come from a connection or a value specified on the node.
	FString ResolvedSelectValue;

	// Try getting the value from a connection first
	bool bGotValueFromConnection = false;
	if (const UMovieGraphPin* SelectPin = GetInputPin(UE::MovieGraph::SelectNode::SelectedOption))
	{
		if (const UMovieGraphPin* OtherPin = SelectPin->GetFirstConnectedPin())
		{
			if (const UMovieGraphNode* ConnectedNode = OtherPin->Node)
			{
				bGotValueFromConnection = true;
				ResolvedSelectValue = ConnectedNode->GetResolvedValueForOutputPin(OtherPin->Properties.Label, &InContext.UserContext);
			}
		}
	}

	if (!bGotValueFromConnection)
	{
		ResolvedSelectValue = SelectedOption;
	}

	for (const FString& SelectOption : SelectOptions)
	{
		if (SelectOption == ResolvedSelectValue)
		{
			PinsToFollow.Add(GetInputPin(FName(SelectOption)));
			break;
		}
	}

	return PinsToFollow;
}

#if WITH_EDITOR
FText UMovieGraphSelectNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText SelectNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_Select", "Select");
	static const FText SelectNodeDescription = NSLOCTEXT("MovieGraphNodes", "NodeDescription_Select", "Select\n{0}");

	if (bGetDescriptive && !Description.IsEmpty())
	{
		return FText::Format(SelectNodeDescription, FText::FromString(Description));
	}

	return SelectNodeName;
}

FText UMovieGraphSelectNode::GetMenuCategory() const
{
	static const FText NodeCategory_Conditionals = NSLOCTEXT("MovieGraphNodes", "NodeCategory_Conditionals", "Conditionals");
	return NodeCategory_Conditionals;
}

FLinearColor UMovieGraphSelectNode::GetNodeTitleColor() const
{
	static const FLinearColor SelectNodeColor = FLinearColor(0.266f, 0.266f, 0.266f);
	return SelectNodeColor;
}

FSlateIcon UMovieGraphSelectNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon SelectIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Merge");

	OutColor = FLinearColor::White;
	return SelectIcon;
}

void UMovieGraphSelectNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphSelectNode, SelectOptions))
	{
		UpdatePins();
	}
	
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphSelectNode, Description))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}
#endif // WITH_EDITOR