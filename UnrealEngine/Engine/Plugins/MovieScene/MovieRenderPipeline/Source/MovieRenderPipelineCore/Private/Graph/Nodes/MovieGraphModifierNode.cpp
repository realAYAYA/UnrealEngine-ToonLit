// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphModifierNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MovieGraph"

#if WITH_EDITOR
FText UMovieGraphModifierNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText ModifierNodeName = LOCTEXT("NodeName_Modifier", "Modifier");
	static const FText ModifierNodeDescription = LOCTEXT("NodeDescription_Modifier", "Modifier\n{0} - {1}");

	const FString ModifiedCollectionNameDisp = ModifiedCollectionName.IsEmpty() ? TEXT("MISSING") : ModifiedCollectionName;
	const FString ModifierNameDisp = ModifierName.IsEmpty() ? TEXT("MISSING") : ModifierName;

	if (bGetDescriptive)
	{
		return FText::Format(ModifierNodeDescription, FText::FromString(ModifierNameDisp), FText::FromString(ModifiedCollectionNameDisp));
	}

	return ModifierNodeName;
}

FText UMovieGraphModifierNode::GetMenuCategory() const
{
	return LOCTEXT("CollectionNode_Category", "Utility");
}

FLinearColor UMovieGraphModifierNode::GetNodeTitleColor() const
{
	static const FLinearColor ModifierNodeColor = FLinearColor(0.6f, 0.113f, 0.113f);
	return ModifierNodeColor;
}

FSlateIcon UMovieGraphModifierNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon ModifierIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.ReferenceViewer");

	OutColor = FLinearColor::White;
	return ModifierIcon;
}

void UMovieGraphModifierNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Broadcast a node-changed delegate so that the node title's UI gets updated.
	if ((PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphModifierNode, ModifierName)) ||
		(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphModifierNode, bOverride_ModifierName)) ||
		(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphModifierNode, ModifiedCollectionName)) || 
		(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphModifierNode, bOverride_ModifiedCollectionName)))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE // "MovieGraph"