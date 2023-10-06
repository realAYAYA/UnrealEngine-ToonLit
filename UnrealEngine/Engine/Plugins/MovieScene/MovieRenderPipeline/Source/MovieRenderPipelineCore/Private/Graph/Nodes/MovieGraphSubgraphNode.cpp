// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphSubgraphNode.h"

#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MovieGraphNode"

namespace UE::MovieGraph::Private
{
	/** Converts graph input/output members to pin properties. */
	TArray<FMovieGraphPinProperties> MembersToPinProperties(const TArray<UMovieGraphInterfaceBase*>& InMembers)
	{
		TArray<FMovieGraphPinProperties> PinProperties;

		Algo::TransformIf(InMembers, PinProperties,
			[](const UMovieGraphInterfaceBase* Member) -> bool
			{
				return Member != nullptr;
			},
			[](const UMovieGraphInterfaceBase* Member) -> FMovieGraphPinProperties
			{
				constexpr bool bAllowMultipleConnections = false;
				FMovieGraphPinProperties Properties(FName(Member->GetMemberName()), Member->GetValueType(), bAllowMultipleConnections);
				Properties.bIsBranch = Member->bIsBranch;
				return Properties;
			});

		return PinProperties;
	}
}

UMovieGraphSubgraphNode::UMovieGraphSubgraphNode()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RefreshOnSubgraphAssetSaved();
	}
}

TArray<FMovieGraphPinProperties> UMovieGraphSubgraphNode::GetInputPinProperties() const
{
	if (const UMovieGraphConfig* Config = GetSubgraphAsset())
	{
		TArray<UMovieGraphInterfaceBase*> InputMembers;
		Algo::Transform(Config->GetInputs(), InputMembers, [](UMovieGraphInput* Input) { return Input; });
		
		return UE::MovieGraph::Private::MembersToPinProperties(InputMembers);
	}

	return {};
}

TArray<FMovieGraphPinProperties> UMovieGraphSubgraphNode::GetOutputPinProperties() const
{
	if (const UMovieGraphConfig* Config = GetSubgraphAsset())
	{
		TArray<UMovieGraphInterfaceBase*> OutputMembers;
		Algo::Transform(Config->GetOutputs(), OutputMembers, [](UMovieGraphOutput* Output) { return Output; });
		
		return UE::MovieGraph::Private::MembersToPinProperties(OutputMembers);
	}

	return {};
}

TArray<UMovieGraphPin*> UMovieGraphSubgraphNode::EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const
{
	TArray<UMovieGraphPin*> PinsToFollow;
	
	if (!ensure(InContext.PinBeingFollowed))
	{
		return PinsToFollow;
	}

	const UMovieGraphConfig* SubgraphPtr = SubgraphAsset.LoadSynchronous();
	if (!SubgraphPtr)
	{
		return PinsToFollow;
	}

	// Find the input pin on the subgraph asset's outputs node that corresponds to the pin being followed
	//
	// Subgraph
	// -----------------+
	//     Outputs node |
	//     +----------+ |
	// --->| Out1     | |Out1 <--- Pin being followed ---
	//     | Out2     | |Out2
	//     | Out3     | |Out3
	//     +----------+ |
	// -----------------+
	
	const UMovieGraphNode* OutputNode = SubgraphPtr->GetOutputNode();
	if (!ensure(OutputNode))
	{
		return PinsToFollow;
	}

	if (UMovieGraphPin* OutputNodePin = OutputNode->GetInputPin(InContext.PinBeingFollowed->Properties.Label))
	{
		PinsToFollow.Add(OutputNodePin);
	}

	// Update the subgraph traversal stack so traversal can resume in parent graphs when traversal reaches the input
	// node of this subgraph
	InContext.SubgraphStack.Add(this);
	
	return PinsToFollow;
}

#if WITH_EDITOR
void UMovieGraphSubgraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update pins when the subgraph asset is changed
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphSubgraphNode, SubgraphAsset))
	{
		UpdatePins();
	}
}
#endif

#if WITH_EDITOR
FText UMovieGraphSubgraphNode::GetNodeTitle(const bool bGetDescriptive) const
{
	// Note: Only the descriptive title indicates invalidity because we don't want the node creation menu to say the
	// subgraph node is invalid
	static const FText SubgraphTitle = LOCTEXT("SubgraphTitle", "Subgraph");
	static const FText DescriptiveSubgraphTitle = LOCTEXT("DescriptiveSubgraphTitle", "Invalid Subgraph\nNo subgraph asset specified");

	if (const UMovieGraphConfig* Subgraph = GetSubgraphAsset())
	{
		return FText::Format(LOCTEXT("SubgraphTitle_IncludesAsset", "Subgraph: {0}"), FText::FromString(Subgraph->GetName()));
	}

	return bGetDescriptive ? DescriptiveSubgraphTitle : SubgraphTitle;
}

FText UMovieGraphSubgraphNode::GetMenuCategory() const
{
	static const FText NodeCategory = LOCTEXT("NodeCategory_Utility", "Utility");

	return NodeCategory;
}
#endif

void UMovieGraphSubgraphNode::SetSubGraphAsset(const TSoftObjectPtr<UMovieGraphConfig>& InSubgraphAsset)
{
	SubgraphAsset = InSubgraphAsset;
}

UMovieGraphConfig* UMovieGraphSubgraphNode::GetSubgraphAsset() const
{
	return SubgraphAsset.LoadSynchronous();
}

void UMovieGraphSubgraphNode::RefreshOnSubgraphAssetSaved()
{
	UPackage::PackageSavedWithContextEvent.AddWeakLambda(this, [this](const FString& PackageFilename, const UPackage* Package, FObjectPostSaveContext Context)
	{
		// Ignore procedural saves (ie, saves not triggered by a user)
		if (Context.IsProceduralSave())
		{
			return;
		}

		// Refresh the node if the subgraph asset was saved
		if (const UMovieGraphConfig* Subgraph = GetSubgraphAsset())
		{
			if (Subgraph->GetPackage() == Package)
			{
				UpdatePins();
			}
		}
	});
}

#undef LOCTEXT_NAMESPACE
