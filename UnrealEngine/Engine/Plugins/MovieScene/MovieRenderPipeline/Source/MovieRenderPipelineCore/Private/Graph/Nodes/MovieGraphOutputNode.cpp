// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphOutputNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Styling/AppStyle.h"

UMovieGraphOutputNode::UMovieGraphOutputNode()
{
#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UMovieGraphConfig* Graph = GetGraph())
		{
			// Register delegates for new outputs when they're added to the graph
			Graph->OnGraphOutputAddedDelegate.AddUObject(this, &UMovieGraphOutputNode::RegisterDelegates);
		}
	}
#endif
}

TArray<FMovieGraphPinProperties> UMovieGraphOutputNode::GetInputPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;

	if (const UMovieGraphConfig* ParentGraph = GetGraph())
	{
		for (const UMovieGraphOutput* Output : ParentGraph->GetOutputs())
		{
			FMovieGraphPinProperties PinProperties(FName(Output->GetMemberName()), Output->GetValueType(), false);
			PinProperties.bIsBranch = Output->bIsBranch;
			Properties.Add(MoveTemp(PinProperties));
		}
	}
	
	return Properties;
}

#if WITH_EDITOR
FText UMovieGraphOutputNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "OutputNode_Description", "Output");
}

FText UMovieGraphOutputNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "OutputNode_Category", "Input/Output");
}

FLinearColor UMovieGraphOutputNode::GetNodeTitleColor() const
{
	return FLinearColor::Black;
}

FSlateIcon UMovieGraphOutputNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon OutputIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeBufferMode");

	OutColor = FLinearColor::White;
	return OutputIcon;
}
#endif // WITH_EDITOR

void UMovieGraphOutputNode::RegisterDelegates() const
{
	Super::RegisterDelegates();
	
	if (const UMovieGraphConfig* Graph = GetGraph())
	{
		for (UMovieGraphOutput* OutputMember : Graph->GetOutputs())
		{
			RegisterDelegates(OutputMember);
		}
	}
}

void UMovieGraphOutputNode::RegisterDelegates(UMovieGraphOutput* Output) const
{
#if WITH_EDITOR
	if (Output)
	{
		Output->OnMovieGraphOutputChangedDelegate.AddUObject(this, &UMovieGraphOutputNode::UpdateExistingPins);
	}
#endif
}

void UMovieGraphOutputNode::UpdateExistingPins(UMovieGraphMember* ChangedOutput) const
{
	if (const UMovieGraphConfig* Graph = GetGraph())
	{
		const TArray<UMovieGraphOutput*> OutputMembers = Graph->GetOutputs();
		if (OutputMembers.Num() == InputPins.Num())
		{
			for (int32 Index = 0; Index < OutputMembers.Num(); ++Index)
			{
				InputPins[Index]->Properties.Label = FName(OutputMembers[Index]->GetMemberName());
				InputPins[Index]->Properties.Type = OutputMembers[Index]->GetValueType();
				InputPins[Index]->Properties.bIsBranch = OutputMembers[Index]->bIsBranch;
			}
		}

		OnNodeChangedDelegate.Broadcast(this);
	}
}