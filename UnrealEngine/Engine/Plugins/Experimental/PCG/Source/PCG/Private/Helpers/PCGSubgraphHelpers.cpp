// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGSubgraphHelpers.h"

#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "Elements/PCGUserParameterGet.h"

#include "Algo/AnyOf.h"
#include "Algo/Transform.h"

#define LOCTEXT_NAMESPACE "PCGSubgraphHelpers"

namespace PCGSubgraphHelpersExtra
{
	// Function that will create a new pin in the input/output node of the subgraph, with a unique name that will
	// be the related to the pin passed as argument. It will be formatted like this:
	// "{NodeName} {PinName} {OptionalIndex}"
	FName CreateNewCustomPin(UPCGGraph* InGraph, UPCGPin* InPinToClone, bool bIsInput, TMap<FString, int>& InOutNameCollisionMapping)
	{
		UPCGNode* NewInputOutputNode = bIsInput ? InGraph->GetInputNode() : InGraph->GetOutputNode();
		UPCGGraphInputOutputSettings* NewInputOutputSettings = CastChecked<UPCGGraphInputOutputSettings>(NewInputOutputNode->GetSettings());
		FString NewName = InPinToClone->Node->GetNodeTitle().ToString() + " " + InPinToClone->Properties.Label.ToString();
		if (InOutNameCollisionMapping.Contains(NewName))
		{
			if (!bIsInput)
			{
				// Output pin can be re-used. If there is a clash, just return the new name, the pin was already added.
				return FName(NewName);
			}

			NewName += " " + FString::FormatAsNumber(++InOutNameCollisionMapping[NewName]);
		}
		else
		{
			InOutNameCollisionMapping.Emplace(NewName, 1);
		}

		FPCGPinProperties NewProperties = InPinToClone->Properties;

		// For the pin type, narrow it down to the input edges (if it is an input pin)
		if (bIsInput)
		{
			const EPCGDataType FirstPinTypeUnion = InPinToClone->Node->GetSettings()->GetTypeUnionOfIncidentEdges(InPinToClone->Properties.Label);
			if (FirstPinTypeUnion != EPCGDataType::None)
			{
				NewProperties.AllowedTypes = FirstPinTypeUnion;
			}
		}

		NewProperties.Label = FName(NewName);
		NewProperties = NewInputOutputSettings->AddCustomPin(NewProperties);
		NewInputOutputNode->UpdateAfterSettingsChangeDuringCreation();
		return NewProperties.Label;
	}

	struct FCollapsingInformation
	{
#if WITH_EDITOR
		// Keep track of all node positions to know where to spawn our subgraph node (editor only)
		FVector2D AveragePosition = FVector2D::ZeroVector;
		int32 MinX = std::numeric_limits<int32>::max();
		int32 MaxX = std::numeric_limits<int32>::min();
#endif // WITH_EDITOR

		// Those 3 sets are used to keep track of the subgraph pins
		// and will be used to extract pins that will be outside the subgraph
		// and will need special treatment.
		TSet<UPCGPin*> InputFromSubgraphPins;
		TSet<UPCGPin*> OutputToSubgraphPins;
		TSet<UPCGPin*> AllSubgraphPins;

		// Also keep track of all the edges in the subgraph. Use a set to be able to add the
		// same edge twice without duplicates.
		TSet<UPCGEdge*> PCGGraphEdges;

		// Keep track of all graph parameters used, they will need to be forwarded to the new graph.
		TArray<const FPropertyBagPropertyDesc*> GraphParametersUsed;

		TArray<UPCGNode*> ValidNodesToCollapse;

		// Keep track of all user parameters output pins
		TMap<FGuid, UPCGPin*> GetUserParametersOutputPins;
		// Also keep track of duplicated get user parameters, to only keep a single one for each user parameter.
		TArray<UPCGNode*> SuperfluousNodes;
	};

	FCollapsingInformation GatheringCollapseInformation(const TArray<UPCGNode*>& InNodesToCollapse, const FInstancedPropertyBag& InGraphParameters)
	{
		FCollapsingInformation CollapseInfo{};

		for (UPCGNode* PCGNode : InNodesToCollapse)
		{
			check(PCGNode);

			const UPCGSettings* NodeSettings = PCGNode->GetSettings();

			// Exclude input and output nodes from the subgraph.
			if (!NodeSettings || NodeSettings->IsA<UPCGGraphInputOutputSettings>())
			{
				continue;
			}

			if (const UPCGUserParameterGetSettings* GetSettings = Cast<UPCGUserParameterGetSettings>(NodeSettings))
			{
				if (const FPropertyBagPropertyDesc* PropertyDesc = InGraphParameters.FindPropertyDescByID(GetSettings->PropertyGuid))
				{
					CollapseInfo.GraphParametersUsed.AddUnique(PropertyDesc);

					if (CollapseInfo.GetUserParametersOutputPins.Contains(PropertyDesc->ID))
					{
						CollapseInfo.SuperfluousNodes.Add(PCGNode);
					}
					else
					{
						CollapseInfo.GetUserParametersOutputPins.Emplace(PropertyDesc->ID, PCGNode->GetOutputPins()[0]);
					}
				}
				else
				{
					// Invalid node
					continue;
				}
			}

			// For each pin of the subgraph, gather all its edges in a set and store the other pin of
			// the edge as an input from or output to the subgraph. It will be useful for tracking pins
			// that are outside the subgraph.
			for (UPCGPin* InputPin : PCGNode->GetInputPins())
			{
				check(InputPin);

				for (UPCGEdge* Edge : InputPin->Edges)
				{
					check(Edge);
					CollapseInfo.PCGGraphEdges.Add(Edge);
					CollapseInfo.OutputToSubgraphPins.Add(Edge->InputPin);
				}

				CollapseInfo.AllSubgraphPins.Add(InputPin);
			}

			for (UPCGPin* OutputPin : PCGNode->GetOutputPins())
			{
				check(OutputPin);

				for (UPCGEdge* Edge : OutputPin->Edges)
				{
					check(Edge);
					CollapseInfo.PCGGraphEdges.Add(Edge);
					CollapseInfo.InputFromSubgraphPins.Add(Edge->OutputPin);
				}

				CollapseInfo.AllSubgraphPins.Add(OutputPin);
			}

			CollapseInfo.ValidNodesToCollapse.Add(PCGNode);

#if WITH_EDITOR
			// And do all the computation to get the min, max and mean position.
			CollapseInfo.AveragePosition.X += PCGNode->PositionX;
			CollapseInfo.AveragePosition.Y += PCGNode->PositionY;
			CollapseInfo.MinX = FMath::Min(CollapseInfo.MinX, PCGNode->PositionX);
			CollapseInfo.MaxX = FMath::Max(CollapseInfo.MaxX, PCGNode->PositionY);
#endif // WITH_EDITOR
		}

#if WITH_EDITOR
		if (CollapseInfo.ValidNodesToCollapse.Num() > 1)
		{
			// Compute the average position
			CollapseInfo.AveragePosition /= CollapseInfo.ValidNodesToCollapse.Num();
		}
#endif // WITH_EDITOR

		// Gather the pins that are outside the subgraph
		CollapseInfo.InputFromSubgraphPins = CollapseInfo.InputFromSubgraphPins.Difference(CollapseInfo.AllSubgraphPins);
		CollapseInfo.OutputToSubgraphPins = CollapseInfo.OutputToSubgraphPins.Difference(CollapseInfo.AllSubgraphPins);

		return CollapseInfo;
	}
}

UPCGGraph* FPCGSubgraphHelpers::CollapseIntoSubgraph(UPCGGraph* InOriginalGraph, const TArray<UPCGNode*>& InNodesToCollapse, const TArray<UObject*>& InExtraNodesToCollapse, UPCGGraph* OptionalPreAllocatedGraph)
{
	// Don't collapse into a subgraph if you don't have at least 2 nodes
	if (!InOriginalGraph || InNodesToCollapse.Num() < 2)
	{
		return nullptr;
	}

	const FInstancedPropertyBag* GraphParameters = InOriginalGraph->GetUserParametersStruct();
	check(GraphParameters);

	PCGSubgraphHelpersExtra::FCollapsingInformation CollapseInfo = PCGSubgraphHelpersExtra::GatheringCollapseInformation(InNodesToCollapse, *GraphParameters);

	// If we have at most 1 valid node to collapse, just exit
	if (CollapseInfo.ValidNodesToCollapse.Num() <= 1)
	{
		UE_LOG(LogPCG, Warning, TEXT("There were less than 2 PCG nodes selected, abort"));
		return nullptr;
	}

	// 2. Create a new subgraph if necessary.
	UPCGGraph* NewPCGGraph = OptionalPreAllocatedGraph;
	if (!NewPCGGraph)
	{
		NewPCGGraph = NewObject<UPCGGraph>();
	}

#if WITH_EDITOR
	// Do some clean-up on input/output nodes
	constexpr int32 Padding = 200;
	NewPCGGraph->GetInputNode()->PositionX = CollapseInfo.MinX - Padding;
	NewPCGGraph->GetInputNode()->PositionY = CollapseInfo.AveragePosition.Y;
	NewPCGGraph->GetOutputNode()->PositionX = CollapseInfo.MaxX + Padding;
	NewPCGGraph->GetOutputNode()->PositionY = CollapseInfo.AveragePosition.Y;
#endif // WITH_EDITOR

	// 4. Duplicate all the nodes, and keep a mapping between the old pins and new pins
	TMap<UPCGPin*, UPCGPin*> PinMapping;
	for (const UPCGNode* PCGNode : CollapseInfo.ValidNodesToCollapse)
	{
		check(PCGNode);

		// Reconstruct a new node, same as PCGNode, but without any edges in the new graph 
		UPCGNode* NewNode = NewPCGGraph->ReconstructNewNode(PCGNode);

		// Safeguard: We should have a 1 for 1 matching between pins labels between the original node
		// and the copied node. If for some reason we don't (perhaps the node was not updated correctly after pins were added/removed)
		// we will log an error and try to connect as best as we can (probably breaking some edges on the process).

		auto Mapping = [&PinMapping, PCGNode](const TArray<UPCGPin*>& OriginalPins, const TArray<UPCGPin*>& NewPins)
		{
			TSet<FName> UnmatchedOriginal;
			TMap<FName, UPCGPin*> NewMapping;

			for (UPCGPin* NewPin : NewPins)
			{
				NewMapping.Emplace(NewPin->Properties.Label, NewPin);
			}

			for (UPCGPin* OriginalPin : OriginalPins)
			{
				FName PinLabel = OriginalPin->Properties.Label;
				if (UPCGPin** NewPinPtr = NewMapping.Find(PinLabel))
				{
					PinMapping.Emplace(OriginalPin, *NewPinPtr);
				}
				else if (OriginalPin->IsConnected())
				{
					// It is only problematic if the pin was connected
					UE_LOG(LogPCG, Error, TEXT("[CollapseInSubgraph - %s] %s pin %s does not exist anymore. Edges will be broken."),
						*PCGNode->GetNodeTitle().ToString(), (OriginalPin->IsOutputPin() ? TEXT("Output") : TEXT("Input")), *PinLabel.ToString());
				}
			}
		};

		Mapping(PCGNode->GetInputPins(), NewNode->GetInputPins());
		Mapping(PCGNode->GetOutputPins(), NewNode->GetOutputPins());
	}

#if WITH_EDITOR
	// Also duplicate the extra nodes and assign them to the new graph
	TArray<TObjectPtr<const UObject>> NewExtraGraphNodes;
	for (const UObject* ExtraNode : InExtraNodesToCollapse)
	{
		NewExtraGraphNodes.Add(DuplicateObject(ExtraNode, NewPCGGraph));
	}

	NewPCGGraph->SetExtraEditorNodes(NewExtraGraphNodes);
#endif ///WITH_EDITOR

	// 5. Iterate over all the edges and create edges "placeholders"
	// Most of them will already be complete, but for those that needs to be connected to the new
	// subgraph nodes, the pins don't exist yet. Therefore we identify them with their pin labels.
	//
	// The logic behind the new pins is this:
	// -> If the pin is connected to the simple pin of the input/output node, let it like this
	// -> If the pin is part of the original input/output advanced pins, we will trigger the advanced pins flag on the input/output node in the subgraph
	// -> If the pin is connected to a get user parameters that is outside of the subgraph, we'll create a new node and connect it to it.
	// -> Otherwise, we add a new custom pin, with the name of the node, the name of the pin and a number if there is name collision
	struct EdgePlaceholder
	{
		UPCGPin* InputPin = nullptr;
		UPCGPin* OutputPin = nullptr;
		FName InputPinLabel;
		FName OutputPinLabel;
	};

	TArray<EdgePlaceholder> EdgePlaceholders;
	TMap<FString, int> NameCollisionMapping;

	// For all internal parameters, create extra edges to connect those nodes to their override pin counterparts
	for (const TPair<FGuid, UPCGPin*>& It : CollapseInfo.GetUserParametersOutputPins)
	{
		check(It.Value);

		EdgePlaceholder OutsideSubgraphEdge;
		OutsideSubgraphEdge.InputPin = It.Value;
		// Gymnastics to make sure we have a 1 to 1 match between labels, because subgraph override uses the display text name.
		OutsideSubgraphEdge.OutputPinLabel = FName(FName::NameToDisplayString(OutsideSubgraphEdge.InputPin->Properties.Label.ToString(), /*bIsBool=*/ false));

		EdgePlaceholders.Add(std::move(OutsideSubgraphEdge));
	}

	for (UPCGEdge* Edge : CollapseInfo.PCGGraphEdges)
	{
		UPCGPin* const* InPin = PinMapping.Find(Edge->InputPin);
		UPCGPin* const* OutPin = PinMapping.Find(Edge->OutputPin);

		check(InPin || OutPin);

		if (InPin == nullptr)
		{
			// The edge comes from outside the graph.
			// If it is from the input node, we have a special behavior
			// Same if it is from a get user parameter
			EdgePlaceholder OutsideSubgraphEdge;
			EdgePlaceholder InsideSubgraphEdge;
			bool bProcessed = false;

			OutsideSubgraphEdge.InputPin = Edge->InputPin;
			InsideSubgraphEdge.OutputPin = *OutPin;

			if (Edge->InputPin->Node == InOriginalGraph->GetInputNode())
			{
				const UPCGGraphInputOutputSettings* Settings = Cast<const UPCGGraphInputOutputSettings>(Edge->InputPin->Node->GetSettings());

				if (Settings && !Settings->IsCustomPin(Edge->InputPin))
				{
					OutsideSubgraphEdge.OutputPinLabel = Edge->InputPin->Properties.Label;
					InsideSubgraphEdge.InputPin = NewPCGGraph->GetInputNode()->GetOutputPin(Edge->InputPin->Properties.Label);
					bProcessed = true;
				}
			}
			else if (Edge->InputPin->Node && Edge->InputPin->Node->GetSettings() && Edge->InputPin->Node->GetSettings()->IsA<UPCGUserParameterGetSettings>())
			{
				const UPCGUserParameterGetSettings* OldSettings = CastChecked<const UPCGUserParameterGetSettings>(Edge->InputPin->Node->GetSettings());
				if (CollapseInfo.GetUserParametersOutputPins.Contains(OldSettings->PropertyGuid))
				{
					if (CollapseInfo.GetUserParametersOutputPins[OldSettings->PropertyGuid] != Edge->InputPin->Node->GetOutputPins()[0])
					{
						CollapseInfo.SuperfluousNodes.Add(Edge->InputPin->Node);
					}
				}
				else
				{
					CollapseInfo.GetUserParametersOutputPins.Emplace(OldSettings->PropertyGuid, Edge->InputPin->Node->GetOutputPins()[0]);
				}

				check(CollapseInfo.GetUserParametersOutputPins.Contains(OldSettings->PropertyGuid));

				// The label will be the same as the input pin, since it will be an override pin.
				OutsideSubgraphEdge.InputPin = CollapseInfo.GetUserParametersOutputPins[OldSettings->PropertyGuid];
				// Gymnastics to make sure we have a 1 to 1 match between labels, because subgraph override uses the display text name.
				OutsideSubgraphEdge.OutputPinLabel = FName(FName::NameToDisplayString(OutsideSubgraphEdge.InputPin->Properties.Label.ToString(), /*bIsBool=*/ false));

				// Create a new node and connect it to it.
				UPCGSettings* NewSettings = nullptr;
				UPCGNode* NewNode = NewPCGGraph->AddNodeCopy(Edge->InputPin->Node->GetSettings(), NewSettings);
				check(NewNode && NewSettings && NewNode->GetOutputPins().Num() == 1);

				// Also need to add this parameter to the list.
				if (const FPropertyBagPropertyDesc* PropertyDesc = GraphParameters->FindPropertyDescByID(OldSettings->PropertyGuid))
				{
					CollapseInfo.GraphParametersUsed.AddUnique(PropertyDesc);
				}

				InsideSubgraphEdge.InputPin = NewNode->GetOutputPins()[0];

				bProcessed = true;
#if WITH_EDITOR
				// Place the newly created node slightly to the left of the output pin node.
				if (ensure(Edge->OutputPin->Node))
				{
					constexpr int32 OffsetX = -200;
					NewNode->PositionX = Edge->OutputPin->Node->PositionX + OffsetX;
					NewNode->PositionY = Edge->OutputPin->Node->PositionY;
				}
#endif // WITH_EDITOR
			}

			if (!bProcessed)
			{
				FName NewPinName = PCGSubgraphHelpersExtra::CreateNewCustomPin(NewPCGGraph, Edge->OutputPin, true, NameCollisionMapping);
				OutsideSubgraphEdge.OutputPinLabel = NewPinName;
				InsideSubgraphEdge.InputPin = NewPCGGraph->GetInputNode()->GetOutputPin(NewPinName);
			}

			EdgePlaceholders.Add(std::move(OutsideSubgraphEdge));
			EdgePlaceholders.Add(std::move(InsideSubgraphEdge));
		}
		else if (OutPin == nullptr)
		{
			// The edge comes from outside the graph.
			// If it is from the output node, we have a special behavior
			EdgePlaceholder OutsideSubgraphEdge;
			EdgePlaceholder InsideSubgraphEdge;
			bool bProcessed = false;

			OutsideSubgraphEdge.OutputPin = Edge->OutputPin;
			InsideSubgraphEdge.InputPin = *InPin;

			if (Edge->OutputPin->Node == InOriginalGraph->GetOutputNode())
			{
				const UPCGGraphInputOutputSettings* Settings = Cast<const UPCGGraphInputOutputSettings>(Edge->OutputPin->Node->GetSettings());

				if (Settings && !Settings->IsCustomPin(Edge->OutputPin))
				{
					OutsideSubgraphEdge.InputPinLabel = Edge->OutputPin->Properties.Label;
					InsideSubgraphEdge.OutputPin = NewPCGGraph->GetOutputNode()->GetInputPin(Edge->OutputPin->Properties.Label);
					bProcessed = true;
				}
			}

			if (!bProcessed)
			{
				FName NewPinName = PCGSubgraphHelpersExtra::CreateNewCustomPin(NewPCGGraph, Edge->InputPin, false, NameCollisionMapping);
				OutsideSubgraphEdge.InputPinLabel = NewPinName;
				InsideSubgraphEdge.OutputPin = NewPCGGraph->GetOutputNode()->GetInputPin(NewPinName);
			}

			EdgePlaceholders.Add(std::move(OutsideSubgraphEdge));
			EdgePlaceholders.Add(std::move(InsideSubgraphEdge));
		}
		else
		{
			// Both nodes are inside
			EdgePlaceholders.Add(EdgePlaceholder{ *InPin, *OutPin });
		}
	}

	// Transfer the values from the original graph parameters to the new graph
	TArray<FPropertyBagPropertyDesc> GraphParametersUsed;
	Algo::Transform(CollapseInfo.GraphParametersUsed, GraphParametersUsed, [](const FPropertyBagPropertyDesc* In) { check(In); return *In; });
	NewPCGGraph->AddUserParameters(GraphParametersUsed, InOriginalGraph);

	// 6. Create subgraph and delete old nodes and superfluous nodes
	UPCGNode* SubgraphNode = nullptr;
	{
		for (UPCGNode* PCGNode : CollapseInfo.ValidNodesToCollapse)
		{
			const UPCGSettings* NodeSettings = PCGNode->GetSettings();

			// Don't delete input, output and get user parameters nodes from the subgraph.
			if (!NodeSettings || NodeSettings->IsA<UPCGGraphInputOutputSettings>() || NodeSettings->IsA<UPCGUserParameterGetSettings>())
			{
				continue;
			}

			InOriginalGraph->RemoveNode(PCGNode);
		}

#if WITH_EDITOR
		for (const UObject* ExtraNode : InExtraNodesToCollapse)
		{
			InOriginalGraph->RemoveExtraEditorNode(ExtraNode);
		}
#endif // WITH_EDITOR

		for (UPCGNode* SuperfluousNode : CollapseInfo.SuperfluousNodes)
		{
			check(SuperfluousNode);
			// Only remove them if they have no edges anymore
			bool bHasAnyEdges = Algo::AnyOf(SuperfluousNode->GetInputPins(), [](const UPCGPin* InPin) { return InPin && InPin->EdgeCount() > 0; })
				|| Algo::AnyOf(SuperfluousNode->GetOutputPins(), [](const UPCGPin* InPin) { return InPin && InPin->EdgeCount() > 0; });

			if (!bHasAnyEdges)
			{
				InOriginalGraph->RemoveNode(SuperfluousNode);
			}
		}

		UPCGSettings* DefaultNodeSettings = nullptr;
		SubgraphNode = InOriginalGraph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), DefaultNodeSettings);
		UPCGSubgraphSettings* DefaultSubgraphSettings = CastChecked<UPCGSubgraphSettings>(DefaultNodeSettings);
		DefaultSubgraphSettings->SetSubgraph(NewPCGGraph);

#if WITH_EDITOR
		SubgraphNode->PositionX = CollapseInfo.AveragePosition.X;
		SubgraphNode->PositionY = CollapseInfo.AveragePosition.Y;
#endif // WITH_EDITOR

		SubgraphNode->UpdateAfterSettingsChangeDuringCreation();
	}

	// 8. Connect all the edges
	TSet<UPCGNode*> TouchedNodes;
	for (EdgePlaceholder& Edge : EdgePlaceholders)
	{
		if (Edge.InputPin == nullptr)
		{
			SubgraphNode->GetOutputPin(Edge.InputPinLabel)->AddEdgeTo(Edge.OutputPin, &TouchedNodes);
		}
		else if (Edge.OutputPin == nullptr)
		{
			Edge.InputPin->AddEdgeTo(SubgraphNode->GetInputPin(Edge.OutputPinLabel), &TouchedNodes);
		}
		else
		{
			Edge.InputPin->AddEdgeTo(Edge.OutputPin, &TouchedNodes);
		}
	}

#if WITH_EDITOR
	InOriginalGraph->NotifyGraphChanged(EPCGChangeType::Structural);
	NewPCGGraph->NotifyGraphChanged(EPCGChangeType::Structural);
	for (UPCGNode* TouchedNode : TouchedNodes)
	{
		TouchedNode->OnNodeChangedDelegate.Broadcast(TouchedNode, EPCGChangeType::Node);
	}
#endif // WITH_EDITOR

	return NewPCGGraph;
}

#undef LOCTEXT_NAMESPACE