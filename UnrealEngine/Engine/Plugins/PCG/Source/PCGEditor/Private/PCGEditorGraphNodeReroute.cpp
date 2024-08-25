// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeReroute.h"

#include "PCGEditorGraph.h"

#include "PCGNode.h"
#include "Elements/PCGReroute.h"

#include "EdGraph/EdGraphPin.h"

FText UPCGEditorGraphNodeReroute::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PCGEditorGraphNodeReroute", "NodeTitle", "Reroute");
}

bool UPCGEditorGraphNodeReroute::ShouldOverridePinNames() const
{
	return true;
}

FText UPCGEditorGraphNodeReroute::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	return FText::GetEmpty();
}

bool UPCGEditorGraphNodeReroute::CanSplitPin(const UEdGraphPin* Pin) const
{
	return false;
}

bool UPCGEditorGraphNodeReroute::ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const
{
	OutInputPinIndex = 0;
	OutOutputPinIndex = 1;
	return true;
}

FText UPCGEditorGraphNodeReroute::GetTooltipText() const
{
	return FText::GetEmpty();
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if (FromPin == GetInputPin())
	{
		return GetOutputPin();
	}
	else
	{
		return GetInputPin();
	}
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Input)
		{
			return Pin;
		}
	}

	return nullptr;
}

UEdGraphPin* UPCGEditorGraphNodeReroute::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}

	return nullptr;
}

FText UPCGEditorGraphNodeNamedRerouteBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// Never show full title, only list view
	return Super::GetNodeTitle(ENodeTitleType::ListView);
}

FText UPCGEditorGraphNodeNamedRerouteUsage::GetPinFriendlyName(const UPCGPin* InPin) const
{
	return FText::FromString(" ");
}

void UPCGEditorGraphNodeNamedRerouteUsage::RebuildEdgesFromPins_Internal()
{
	Super::RebuildEdgesFromPins_Internal();

	check(PCGNode);

	if (PCGNode->HasInboundEdges())
	{
		return;
	}

	UPCGGraph* Graph = PCGNode->GetGraph();

	if (!Graph)
	{
		return;
	}

	UPCGNamedRerouteUsageSettings* Usage = Cast<UPCGNamedRerouteUsageSettings>(PCGNode->GetSettings());

	if (!Usage)
	{
		return;
	}

	// Make sure we're hooked to the declaration if it's not already the case
	if (UPCGNode* DeclarationNode = Graph->FindNodeWithSettings(Usage->Declaration))
	{
		DeclarationNode->AddEdgeTo(PCGNamedRerouteConstants::InvisiblePinLabel, PCGNode, PCGPinConstants::DefaultInputLabel);
	}
}

FText UPCGEditorGraphNodeNamedRerouteDeclaration::GetPinFriendlyName(const UPCGPin* InPin) const
{
	return FText::FromString(" ");
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::ApplyToUsageNodes(TFunctionRef<void(UPCGEditorGraphNodeNamedRerouteUsage*)> Action)
{
	if (!GetPCGNode())
	{
		return;
	}

	const UPCGNamedRerouteDeclarationSettings* Declaration = Cast<UPCGNamedRerouteDeclarationSettings>(GetPCGNode()->GetSettings());

	if (!Declaration)
	{
		return;
	}

	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(GetGraph());

	if (!EditorGraph)
	{
		return;
	}

	for (const TObjectPtr<UEdGraphNode>& EdGraphNode : EditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeNamedRerouteUsage* RerouteNode = Cast<UPCGEditorGraphNodeNamedRerouteUsage>(EdGraphNode))
		{
			if (RerouteNode->GetPCGNode() &&
				Cast<UPCGNamedRerouteUsageSettings>(RerouteNode->GetPCGNode()->GetSettings()) &&
				Cast<UPCGNamedRerouteUsageSettings>(RerouteNode->GetPCGNode()->GetSettings())->Declaration == Declaration)
			{
				Action(RerouteNode);
			}
		}
	}
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::ReconstructNodeOnChange()
{
	Super::ReconstructNodeOnChange();

	// We must make sure to trigger a notify node changed on all editor nodes that are usages of that declaration
	ApplyToUsageNodes([](UPCGEditorGraphNodeNamedRerouteUsage* RerouteNode)
	{
		RerouteNode->ReconstructNodeOnChange();
	});
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::SetNodeName(const UPCGNode* FromNode, FName FromPinName)
{
	FString NewName;

	if (FromNode)
	{
		NewName = FromNode->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() + " " + FromPinName.ToString();
	}
	else if (FromPinName != NAME_None)
	{
		NewName = FromPinName.ToString();
	}
	else
	{
		NewName = TEXT("Reroute");
	}

	bCanRenameNode = true;
	OnRenameNode(GetCollisionFreeNodeName(NewName));
	bCanRenameNode = false;
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::OnRenameNode(const FString& NewName)
{
	Super::OnRenameNode(NewName);

	// Propagate the name change to downstream usage nodes
	ApplyToUsageNodes([&NewName](UPCGEditorGraphNodeNamedRerouteUsage* RerouteNode)
	{
		RerouteNode->bCanRenameNode = true;
		RerouteNode->OnRenameNode(NewName);
		RerouteNode->bCanRenameNode = false;
	});

	ReconstructNodeOnChange();
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::OnColorPicked(FLinearColor NewColor)
{
	Super::OnColorPicked(NewColor);

	// Propagate change to downstream usage nodes
	ApplyToUsageNodes([&NewColor](UPCGEditorGraphNodeNamedRerouteUsage* RerouteNode)
	{
		RerouteNode->OnColorPicked(NewColor);
	});
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::PostPaste()
{
	Super::PostPaste();
	FixNodeNameCollision();
}

void UPCGEditorGraphNodeNamedRerouteDeclaration::FixNodeNameCollision()
{
	const FString BaseName = GetNodeTitle(ENodeTitleType::ListView).ToString();

	bCanRenameNode = true;
	OnRenameNode(GetCollisionFreeNodeName(BaseName));
	bCanRenameNode = false;
}

FString UPCGEditorGraphNodeNamedRerouteDeclaration::GetCollisionFreeNodeName(const FString& BaseName) const
{
	// If there is another declaration in the graph with the same name, append a _N to the name here.
	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(GetGraph());

	if (!EditorGraph)
	{
		return BaseName;
	}

	FString TentativeName = BaseName;
	bool bHasNameCollision = true;
	int32 NameSuffix = 1;

	while (bHasNameCollision)
	{
		bHasNameCollision = false;

		for (const TObjectPtr<UEdGraphNode>& EdGraphNode : EditorGraph->Nodes)
		{
			if (EdGraphNode && EdGraphNode != this && EdGraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString() == TentativeName)
			{
				bHasNameCollision = true;
				break;
			}
		}

		if (bHasNameCollision)
		{
			// Implementation note: since the node title is a display string, we need to craft the tentative name to be "stable" against it, otherwise the comparisons will fail.
			TentativeName = FString::Printf(TEXT("%s %d"), *BaseName, NameSuffix++);
		}
	}

	return TentativeName;
}