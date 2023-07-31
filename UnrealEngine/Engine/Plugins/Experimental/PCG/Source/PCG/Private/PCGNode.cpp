// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGNode.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"

#include "Algo/Transform.h"

UPCGNode::UPCGNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultSettings = ObjectInitializer.CreateDefaultSubobject<UPCGTrivialSettings>(this, TEXT("DefaultNodeSettings"));
}

void UPCGNode::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
		DefaultSettings->ConditionalPostLoad();
	}

	// Make sure legacy nodes support transactions.
	if (HasAllFlags(RF_Transactional) == false)
	{
		SetFlags(RF_Transactional);
	}

	for (UPCGPin* InputPin : InputPins)
	{
		check(InputPin);
		InputPin->ConditionalPostLoad();
	}

	for (UPCGPin* OutputPin : OutputPins)
	{
		check(OutputPin);
		OutputPin->ConditionalPostLoad();
	}
#endif
}

#if WITH_EDITOR
void UPCGNode::ApplyDeprecation()
{
	UPCGPin* DefaultOutputPin = OutputPins.IsEmpty() ? nullptr : OutputPins[0];
	for (TObjectPtr<UPCGNode> OutboundNode : OutboundNodes_DEPRECATED)
	{
		UPCGPin* OtherNodeInputPin = OutboundNode->InputPins.IsEmpty() ? nullptr : OutboundNode->InputPins[0];

		if (DefaultOutputPin && OtherNodeInputPin)
		{
			DefaultOutputPin->AddEdgeTo(OtherNodeInputPin);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Unable to apply deprecation on outbound nodes"));
		}
	}
	OutboundNodes_DEPRECATED.Reset();

	// Deprecated edges -> pins & edges
	// Inbound edges will be taken care of by other nodes outbounds
	InboundEdges_DEPRECATED.Reset();

	for (UPCGEdge* OutboundEdge : OutboundEdges_DEPRECATED)
	{
		check(OutboundEdge->InboundNode_DEPRECATED == this);
		check(OutboundEdge->OutboundNode_DEPRECATED);

		UPCGPin* OutputPin = nullptr;
		if (OutboundEdge->InboundLabel_DEPRECATED == NAME_None)
		{
			OutputPin = OutputPins.IsEmpty() ? nullptr : OutputPins[0];
		}
		else
		{
			OutputPin = GetOutputPin(OutboundEdge->InboundLabel_DEPRECATED);
		}

		if (!OutputPin)
		{
			UE_LOG(LogPCG, Error, TEXT("Unable to apply deprecation on outbound edge on node %s - can't find output pin %s"), *GetFName().ToString(), *OutboundEdge->InboundLabel_DEPRECATED.ToString());
			continue;
		}

		UPCGNode* OtherNode = OutboundEdge->OutboundNode_DEPRECATED;
		if (!OtherNode)
		{
			UE_LOG(LogPCG, Error, TEXT("Unable to apply deprecation on outbound edge on node %s - can't find other node"), *GetFName().ToString());
			continue;
		}

		UPCGPin* OtherNodeInputPin = nullptr;
		if (OutboundEdge->OutboundLabel_DEPRECATED == NAME_None)
		{
			OtherNodeInputPin = OtherNode->InputPins.IsEmpty() ? nullptr : OtherNode->InputPins[0];
		}
		else
		{
			OtherNodeInputPin = OtherNode->GetInputPin(OutboundEdge->OutboundLabel_DEPRECATED);
		}

		if (OtherNodeInputPin)
		{
			OutputPin->AddEdgeTo(OtherNodeInputPin);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Unable to apply deprecation on outbound edge on node %s output pin %s - can't find node %s input pin %s"), *GetFName().ToString(), *OutboundEdge->InboundLabel_DEPRECATED.ToString(), *OtherNode->GetFName().ToString(), *OutboundEdge->OutboundLabel_DEPRECATED.ToString());
		}
	}
	OutboundEdges_DEPRECATED.Reset();
}
#endif

#if WITH_EDITOR
void UPCGNode::PostEditImport()
{
	Super::PostEditImport();
	if (DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
	}
}

void UPCGNode::PreEditUndo()
{
	if (DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.RemoveAll(this);
	}

	UObject* Outer = GetOuter();
	if (UPCGGraph* PCGGraph = Cast<UPCGGraph>(Outer))
	{
		PCGGraph->PreNodeUndo(this);
	}

	Super::PreEditUndo();
}

void UPCGNode::PostEditUndo()
{
	Super::PostEditUndo();

	if (DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
	}

 	UObject* Outer = GetOuter();
	if (UPCGGraph* PCGGraph = Cast<UPCGGraph>(Outer))
	{
		PCGGraph->PostNodeUndo(this);
	}
}
#endif

void UPCGNode::BeginDestroy()
{
#if WITH_EDITOR
	if (DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.RemoveAll(this);
	}
#endif

	Super::BeginDestroy();
}

UPCGGraph* UPCGNode::GetGraph() const
{
	return Cast<UPCGGraph>(GetOuter());
}

UPCGNode* UPCGNode::AddEdgeTo(FName InboundName, UPCGNode* To, FName OutboundName)
{
	check(GetGraph());
	if (UPCGGraph* Graph = GetGraph())
	{
		return Graph->AddLabeledEdge(this, InboundName, To, OutboundName);
	}
	else
	{
		return nullptr;
	}
}

FName UPCGNode::GetNodeTitle() const
{
	if (NodeTitle != NAME_None)
	{
		return NodeTitle;
	}
	else if (DefaultSettings)
	{
		if (DefaultSettings->AdditionalTaskName() != NAME_None)
		{
			return DefaultSettings->AdditionalTaskName();
		}
#if WITH_EDITOR
		else
		{
			return DefaultSettings->GetDefaultNodeName();
		}
#endif
	}

	return TEXT("Unnamed node");
}

TArray<FPCGPinProperties> UPCGNode::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	Algo::Transform(InputPins, PinProperties, [](const UPCGPin* InputPin) { return InputPin->Properties; });
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGNode::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	Algo::Transform(OutputPins, PinProperties, [](const UPCGPin* OutputPin) { return OutputPin->Properties; });
	return PinProperties;
}

UPCGPin* UPCGNode::GetInputPin(const FName& Label)
{
	for (UPCGPin* InputPin : InputPins)
	{
		if (InputPin->Properties.Label == Label)
		{
			return InputPin;
		}
	}

	return nullptr;
}

const UPCGPin* UPCGNode::GetInputPin(const FName& Label) const
{
	for (const UPCGPin* InputPin : InputPins)
	{
		if (InputPin->Properties.Label == Label)
		{
			return InputPin;
		}
	}

	return nullptr;
}

UPCGPin* UPCGNode::GetOutputPin(const FName& Label)
{
	for (UPCGPin* OutputPin : OutputPins)
	{
		if (OutputPin->Properties.Label == Label)
		{
			return OutputPin;
		}
	}

	return nullptr;
}

const UPCGPin* UPCGNode::GetOutputPin(const FName& Label) const
{
	for (const UPCGPin* OutputPin : OutputPins)
	{
		if (OutputPin->Properties.Label == Label)
		{
			return OutputPin;
		}
	}

	return nullptr;
}

bool UPCGNode::IsInputPinConnected(const FName& Label) const
{
	if (const UPCGPin* InputPin = GetInputPin(Label))
	{
		return InputPin->IsConnected();
	}
	else
	{
		return false;
	}
}

bool UPCGNode::IsOutputPinConnected(const FName& Label) const
{
	if (const UPCGPin* OutputPin = GetOutputPin(Label))
	{
		return OutputPin->IsConnected();
	}
	else
	{
		return false;
	}
}

bool UPCGNode::HasInboundEdges() const
{
	for (const UPCGPin* InputPin : InputPins)
	{
		for (const UPCGEdge* InboundEdge : InputPin->Edges)
		{
			if (InboundEdge->IsValid())
			{
				return true;
			}
		}
	}

	return false;
}

void UPCGNode::SetDefaultSettings(TObjectPtr<UPCGSettings> InSettings, bool bUpdatePins)
{
#if WITH_EDITOR
	const bool bDifferentSettings = (DefaultSettings != InSettings);
	if (bDifferentSettings && DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.RemoveAll(this);
	}
#endif

	DefaultSettings = InSettings;

#if WITH_EDITOR
	if (bDifferentSettings && DefaultSettings)
	{
		DefaultSettings->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
	}
#endif

	if (bUpdatePins)
	{
		UpdatePins();
	}
}

#if WITH_EDITOR

void UPCGNode::PreEditChange(FProperty* PropertyAboutToChange)
{
	// To properly clean up old callbacks during paste we clear during null call since the Settings property doesn't get a specific call.
	if (!PropertyAboutToChange || (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGNode, DefaultSettings)))
	{
		if (DefaultSettings)
		{
			DefaultSettings->OnSettingsChangedDelegate.RemoveAll(this);
		}
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UPCGNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGNode, DefaultSettings))
	{
		if (DefaultSettings)
		{
			DefaultSettings->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
			UpdatePins();
			OnNodeChangedDelegate.Broadcast(this, ((Cast<UPCGBaseSubgraphSettings>(DefaultSettings) ? EPCGChangeType::Structural : EPCGChangeType::None) | EPCGChangeType::Settings));
		}
	}
	else if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGNode, NodeTitle))
	{
		OnNodeChangedDelegate.Broadcast(this, EPCGChangeType::Cosmetic);
	}
}

void UPCGNode::OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType)
{
	if (InSettings == DefaultSettings)
	{
		const bool bUpdatedPins = UpdatePins();
		OnNodeChangedDelegate.Broadcast(this, ((bUpdatedPins ? EPCGChangeType::Edge : EPCGChangeType::None) | ChangeType));
	}
}

void UPCGNode::TransferEditorProperties(UPCGNode* OtherNode) const
{
	OtherNode->PositionX = PositionX;
	OtherNode->PositionY = PositionY;
	OtherNode->bCommentBubblePinned = bCommentBubblePinned;
	OtherNode->bCommentBubbleVisible = bCommentBubbleVisible;
	OtherNode->NodeComment = NodeComment;
}

#endif // WITH_EDITOR

void UPCGNode::UpdateAfterSettingsChangeDuringCreation()
{
	UpdatePins();
}

bool UPCGNode::UpdatePins()
{
	return UpdatePins([](UPCGNode* Node){ return NewObject<UPCGPin>(Node); });
}

bool UPCGNode::UpdatePins(TFunctionRef<UPCGPin*(UPCGNode*)> PinAllocator)
{
	if (!DefaultSettings)
	{
		bool bChanged = !InputPins.IsEmpty() || !OutputPins.IsEmpty();

		if (bChanged)
		{
			Modify();
		}

		InputPins.Reset();
		OutputPins.Reset();
		return bChanged;
	}

	TArray<FPCGPinProperties> InboundPinProperties = DefaultSettings->InputPinProperties();
	TArray<FPCGPinProperties> OutboundPinProperties = DefaultSettings->OutputPinProperties();

	auto UpdatePins = [this, &PinAllocator](TArray<UPCGPin*>& Pins, const TArray<FPCGPinProperties>& PinProperties)
	{
		bool bAppliedEdgeChanges = false;

		// Find unmatched pins vs. properties on a name basis
		TArray<UPCGPin*> UnmatchedPins;
		for (UPCGPin* Pin : Pins)
		{
			if (const FPCGPinProperties* MatchingProperties = PinProperties.FindByPredicate([Pin](const FPCGPinProperties& Prop) { return Prop.Label == Pin->Properties.Label; }))
			{
				if (!(Pin->Properties == *MatchingProperties))
				{
					Pin->Modify();
					Pin->Properties = *MatchingProperties;
					bAppliedEdgeChanges |= Pin->BreakAllIncompatibleEdges();
				}
			}
			else
			{
				UnmatchedPins.Add(Pin);
			}
		}

		// Find unmatched properties vs pins on a name basis
		TArray<FPCGPinProperties> UnmatchedProperties;
		for (const FPCGPinProperties& Properties : PinProperties)
		{
			if (!Pins.FindByPredicate([&Properties](const UPCGPin* Pin) { return Pin->Properties.Label == Properties.Label; }))
			{
				UnmatchedProperties.Add(Properties);
			}
		}

		const bool bUpdateUnmatchedPin = UnmatchedPins.Num() == 1 && UnmatchedProperties.Num() == 1;
		if (bUpdateUnmatchedPin)
		{
			UnmatchedPins[0]->Modify();
			UnmatchedPins[0]->Properties = UnmatchedProperties[0];
			bAppliedEdgeChanges |= UnmatchedPins[0]->BreakAllIncompatibleEdges();
		}
		else
		{
			if(!UnmatchedPins.IsEmpty() || !UnmatchedProperties.IsEmpty())
			{
				Modify();
			}

			// Remove old pins
			for (int32 UnmatchedPinIndex = UnmatchedPins.Num() - 1; UnmatchedPinIndex >= 0; --UnmatchedPinIndex)
			{
				const int32 PinIndex = Pins.IndexOfByKey(UnmatchedPins[UnmatchedPinIndex]);
				check(PinIndex >= 0);

				bAppliedEdgeChanges |= Pins[PinIndex]->BreakAllEdges();
				Pins.RemoveAt(PinIndex);
			}

			// Add new pins
			for (const FPCGPinProperties& UnmatchedProperty : UnmatchedProperties)
			{
				const int32 InsertIndex = PinProperties.IndexOfByKey(UnmatchedProperty);
				UPCGPin* NewPin = PinAllocator(this);
				NewPin->Node = this;
				NewPin->Properties = UnmatchedProperty;
				Pins.Insert(NewPin, InsertIndex);
			}
		}

		return bAppliedEdgeChanges;
	};

	bool bChanged = false;
	bChanged |= UpdatePins(InputPins, InboundPinProperties);
	bChanged |= UpdatePins(OutputPins, OutboundPinProperties);

	return bChanged;
}
