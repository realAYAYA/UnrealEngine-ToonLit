// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGNode.h"

#include "PCGCustomVersion.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGPin.h"

#include "Algo/Find.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGNode)

UPCGNode::UPCGNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SettingsInterface = ObjectInitializer.CreateDefaultSubobject<UPCGTrivialSettings>(this, TEXT("DefaultNodeSettings"));
}

void UPCGNode::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (DefaultSettings_DEPRECATED)
	{
		SettingsInterface = DefaultSettings_DEPRECATED;
		DefaultSettings_DEPRECATED = nullptr;
	}

	if (SettingsInterface)
	{
		SettingsInterface->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
		SettingsInterface->ConditionalPostLoad();
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
void UPCGNode::ApplyDeprecationBeforeUpdatePins()
{
	if (UPCGSettings* Settings = GetSettings())
	{
		const int VersionBefore = Settings->DataVersion;

		Settings->ApplyDeprecationBeforeUpdatePins(this, InputPins, OutputPins);

		// Version number should not be bumped in this before-update-pins migration, if it does
		// we risk deprecation code not running in post-update-pins because version number is latest.
		ensure(VersionBefore == Settings->DataVersion);
	}
}

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

	if (UPCGSettings* Settings = GetSettings())
	{
		Settings->ApplyDeprecation(this);

		// Once deprecation has run, version should be up to date.
		ensure(Settings->DataVersion == FPCGCustomVersion::LatestVersion);
	}
}
#endif

#if WITH_EDITOR
void UPCGNode::PostEditImport()
{
	Super::PostEditImport();
	if (SettingsInterface)
	{
		SettingsInterface->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
	}
}

void UPCGNode::PreEditUndo()
{
	if (SettingsInterface)
	{
		SettingsInterface->OnSettingsChangedDelegate.RemoveAll(this);
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

	if (SettingsInterface)
	{
		SettingsInterface->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
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
	if (SettingsInterface)
	{
		SettingsInterface->OnSettingsChangedDelegate.RemoveAll(this);
	}
#endif

	Super::BeginDestroy();
}

UPCGGraph* UPCGNode::GetGraph() const
{
	return Cast<UPCGGraph>(GetOuter());
}

UPCGNode* UPCGNode::AddEdgeTo(FName FromPinLabel, UPCGNode* To, FName ToPinLabel)
{
	if (UPCGGraph* Graph = GetGraph())
	{
		return Graph->AddEdge(this, FromPinLabel, To, ToPinLabel);
	}
	else
	{
		return nullptr;
	}
}

bool UPCGNode::RemoveEdgeTo(FName FromPinLabel, UPCGNode* To, FName ToPinLabel)
{
	if (UPCGGraph* Graph = GetGraph())
	{
		return Graph->RemoveEdge(this, FromPinLabel, To, ToPinLabel);
	}
	else
	{
		return false;
	}
}

FText UPCGNode::GetNodeTitle() const
{
	if (NodeTitle != NAME_None)
	{
		return FText::FromName(NodeTitle);
	}
	else if (UPCGSettings* Settings = GetSettings())
	{
		if (Settings->AdditionalTaskName() != NAME_None)
		{
			return FText::FromName(Settings->AdditionalTaskName());
		}
#if WITH_EDITOR
		else
		{
			return Settings->GetDefaultNodeTitle();
		}
#endif
	}

	return NSLOCTEXT("PCGNode", "NodeTitle", "Unnamed node");
}

#if WITH_EDITOR
FText UPCGNode::GetNodeTooltipText() const
{
	if (UPCGSettings* Settings = GetSettings())
	{
		return Settings->GetNodeTooltipText();
	}
	else
	{
		return FText::GetEmpty();
	}
}
#endif

bool UPCGNode::IsInstance() const
{
	return SettingsInterface && SettingsInterface->IsInstance();
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

void UPCGNode::RenameInputPin(const FName& OldLabel, const FName& NewLabel)
{
	if (UPCGPin* Pin = GetInputPin(OldLabel))
	{
		Pin->Modify();
		Pin->Properties.Label = NewLabel;

#if WITH_EDITOR
		OnNodeChangedDelegate.Broadcast(this, EPCGChangeType::Node);
#endif // WITH_EDITOR
	}
}

void UPCGNode::RenameOutputPin(const FName& OldLabel, const FName& NewLabel)
{
	if (UPCGPin* Pin = GetOutputPin(OldLabel))
	{
		Pin->Modify();
		Pin->Properties.Label = NewLabel;

#if WITH_EDITOR
		OnNodeChangedDelegate.Broadcast(this, EPCGChangeType::Node);
#endif // WITH_EDITOR
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

const UPCGPin* UPCGNode::GetPassThroughInputPin() const
{
	// No outputs means nothing will be passed through
	if (GetOutputPins().Num() == 0)
	{
		return nullptr;
	}

	const EPCGDataType OutputType = GetOutputPins()[0]->Properties.AllowedTypes;

	// We assume a node whose primary output is params is a param processing node.
	const bool bNodeOutputsParams = (OutputType == EPCGDataType::Param);

	if (bNodeOutputsParams)
	{
		// If this node primarily processes params, then look for a params pin, if we find it then we will pass it through
		auto FindParams = [](const TObjectPtr<UPCGPin>& InPin) { return InPin->Properties.AllowedTypes == EPCGDataType::Param; };
		const TObjectPtr<UPCGPin>* FirstParamsPinPtr = Algo::FindByPredicate(GetInputPins(), FindParams);
		return FirstParamsPinPtr ? *FirstParamsPinPtr : nullptr;
	}
	else
	{
		// 'Normal' node. Look for the first pin that is not of type Params to pass through. If the node is disabled, the params are unused.
		// Params-only pins will be rejected/ignored.
		auto FindNonParams = [](const TObjectPtr<UPCGPin>& InPin) { return InPin->Properties.AllowedTypes != EPCGDataType::Param; };
		const TObjectPtr<UPCGPin>* FirstNonParamsPinPtr = Algo::FindByPredicate(GetInputPins(), FindNonParams);

		if (FirstNonParamsPinPtr)
		{
			// Finally in order to be a candidate for passing through, data must be compatible.
			const UPCGPin* InputPin = *FirstNonParamsPinPtr;
			const EPCGDataType InputType = InputPin->Properties.AllowedTypes;
			const bool bTypesOverlap = !!(InputType & OutputType);
			
			// Misc note - it would be nice if we could be stricter, like line below this comment. However it would mean it will stop an Any
			// input being passed through to a Point output, even if the incoming edge will be receiving points dynamically/during execution. If a
			// user creates a BP node with an Any input, which they may do lazily or unknowingly, this blocks passthrough. So instead we'll indicate
			// that this pin *may* be used as a passthrough, and during execution in DisabledPassThroughData() we check dynamic types.
			//const bool bInputTypeNotWiderThanOutputType = !(InputType & ~OutputType);
			
			if (bTypesOverlap)
			{
				return *FirstNonParamsPinPtr;
			}
		}

		return nullptr;
	}
}

bool UPCGNode::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	check(InPin);

	UPCGSettings* Settings = GetSettings();
	if (!Settings)
	{
		// Safe default - assume used
		return true;
	}

	// Disabled nodes only use the 'pass through pin'
	if (!Settings->bEnabled)
	{
		return InPin == GetPassThroughInputPin();
	}

	// Let Settings signal whether it uses a pin or not
	return Settings->IsPinUsedByNodeExecution(InPin);
}

bool UPCGNode::IsEdgeUsedByNodeExecution(const UPCGEdge* InEdge) const
{
	check(InEdge);

	// Locate the pin on this node that the edge is connected to
	const UPCGPin* Pin = nullptr;
	if (InEdge->InputPin->Node == this)
	{
		Pin = InEdge->InputPin;
	}
	else if (InEdge->OutputPin->Node == this)
	{
		Pin = InEdge->OutputPin;
	}

	if (!ensure(Pin))
	{
		// Safe default - assume used
		return true;
	}

	UPCGSettings* Settings = GetSettings();
	if (!Settings)
	{
		// Safe default - assume used
		return true;
	}

	// Disabled nodes only use the 'pass through pin'
	if (!Settings->bEnabled)
	{
		// Only accept first edge if node is disabled
		const bool bConnectedToPassThrough = (Pin == GetPassThroughInputPin());
		const bool bMustBeFirstEdge = Settings->OnlyPassThroughOneEdgeWhenDisabled();
		const bool bIsFirstEdge = Pin->Edges.Num() > 0 && Pin->Edges[0] == InEdge;
		if (!bConnectedToPassThrough || (bMustBeFirstEdge && !bIsFirstEdge))
		{
			return false;
		}
	}

	// Ask Settings if the pin is in use, good opportunity to gray out pins based on parameters
	return Settings->IsPinUsedByNodeExecution(Pin);
}

void UPCGNode::SetSettingsInterface(UPCGSettingsInterface* InSettingsInterface, bool bUpdatePins)
{
	const bool bDifferentInterface = (SettingsInterface.Get() != InSettingsInterface);
	if (bDifferentInterface && SettingsInterface)
	{
#if WITH_EDITOR
		SettingsInterface->OnSettingsChangedDelegate.RemoveAll(this);
#endif

		// Un-outer the current settings to disassociate old settings from node. Without this one can copy paste
		// a node and get both settings objects in the clipboard text, and the wrong settings can be used upon paste.
		if (ensure(SettingsInterface->GetOuter() == this))
		{
			SettingsInterface->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			SettingsInterface->MarkAsGarbage();
		}
	}

	SettingsInterface = InSettingsInterface;

#if WITH_EDITOR
	if (bDifferentInterface && SettingsInterface)
	{
		check(SettingsInterface->GetSettings());
		SettingsInterface->OnSettingsChangedDelegate.AddUObject(this, &UPCGNode::OnSettingsChanged);
	}
#endif

	if (bUpdatePins)
	{
		UpdatePins();
	}
}

UPCGSettings* UPCGNode::GetSettings() const
{
	if (SettingsInterface)
	{
		return SettingsInterface->GetSettings();
	}
	else
	{
		return nullptr;
	}
}

#if WITH_EDITOR
void UPCGNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGNode, NodeTitle))
	{
		OnNodeChangedDelegate.Broadcast(this, EPCGChangeType::Cosmetic);
	}
}

void UPCGNode::OnSettingsChanged(UPCGSettings* InSettings, EPCGChangeType ChangeType)
{
	if (InSettings == GetSettings())
	{
		const EPCGChangeType PinChangeType = UpdatePins();
		OnNodeChangedDelegate.Broadcast(this, ChangeType | PinChangeType);
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

EPCGChangeType UPCGNode::UpdatePins()
{
	return UpdatePins([](UPCGNode* Node){ return NewObject<UPCGPin>(Node); });
}

void UPCGNode::CreateDefaultPins(TFunctionRef<UPCGPin* (UPCGNode*)> PinAllocator)
{
	auto CreatePins = [this, &PinAllocator](TArray<UPCGPin*>& Pins, const TArray<FPCGPinProperties>& PinProperties)
	{
		for (const FPCGPinProperties& Properties : PinProperties)
		{
			UPCGPin* NewPin = PinAllocator(this);
			NewPin->Node = this;
			NewPin->Properties = Properties;
			Pins.Add(NewPin);
		};
	};

	UPCGSettings* Settings = GetSettings();
	check(Settings);
	CreatePins(InputPins, Settings->DefaultInputPinProperties());
	CreatePins(OutputPins, Settings->DefaultOutputPinProperties());
}

EPCGChangeType UPCGNode::UpdatePins(TFunctionRef<UPCGPin*(UPCGNode*)> PinAllocator, const UPCGNode* FromNode /*= nullptr*/)
{
	const UPCGSettings* Settings = GetSettings();
	
	if (!Settings)
	{
		bool bChanged = !InputPins.IsEmpty() || !OutputPins.IsEmpty();

		if (bChanged)
		{
			Modify();
		}

		// Clean up edges
		for (UPCGPin* Pin : InputPins)
		{
			if (Pin)
			{
				Pin->BreakAllEdges();
			}
		}

		for (UPCGPin* Pin : OutputPins)
		{
			if (Pin)
			{
				Pin->BreakAllEdges();
			}
		}

		InputPins.Reset();
		OutputPins.Reset();
		return EPCGChangeType::Edge | EPCGChangeType::Node;
	}
	
	const TArray<FPCGPinProperties> InboundPinProperties = Settings->AllInputPinProperties();
	const TArray<FPCGPinProperties> OutboundPinProperties = Settings->AllOutputPinProperties();

	auto UpdatePins = [this, &PinAllocator](TArray<UPCGPin*>& Pins, const TArray<FPCGPinProperties>& PinProperties)
	{
		bool bAppliedEdgeChanges = false;
		bool bChangedPins = false;

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
					bChangedPins = true;
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
			bChangedPins = true;
		}
		else
		{
			// Verification that we don't have 2 pins with the same name
			// If so, mark them to be removed.
			TSet<FName> AllPinNames;
			TArray<UPCGPin*> DuplicatedNamePins;

			for (UPCGPin* Pin : Pins)
			{
				if (AllPinNames.Contains(Pin->Properties.Label))
				{
					DuplicatedNamePins.Add(Pin);
				}

				AllPinNames.Add(Pin->Properties.Label);
			}

			if(!UnmatchedPins.IsEmpty() || !UnmatchedProperties.IsEmpty() || !DuplicatedNamePins.IsEmpty())
			{
				Modify();
				bChangedPins = true;
			}

			auto RemovePins = [&Pins, &AllPinNames, &bAppliedEdgeChanges](TArray<UPCGPin*>& PinsToRemove, bool bRemoveFromAllNames)
			{
				for (int32 RemovedPinIndex = PinsToRemove.Num() - 1; RemovedPinIndex >= 0; --RemovedPinIndex)
				{
					const int32 PinIndex = Pins.IndexOfByKey(PinsToRemove[RemovedPinIndex]);
					if (PinIndex >= 0)
					{
						if (bRemoveFromAllNames)
						{
							AllPinNames.Remove(Pins[PinIndex]->Properties.Label);
						}

						bAppliedEdgeChanges |= Pins[PinIndex]->BreakAllEdges();
						Pins.RemoveAt(PinIndex);
					}
				}
			};

			RemovePins(UnmatchedPins, /*bRemoveFromAllNames=*/ true);
			RemovePins(DuplicatedNamePins, /*bRemoveFromAllNames=*/ false);

			// Add new pins
			for (const FPCGPinProperties& UnmatchedProperty : UnmatchedProperties)
			{
				if (ensure(!AllPinNames.Contains(UnmatchedProperty.Label)))
				{
					AllPinNames.Add(UnmatchedProperty.Label);

					const int32 InsertIndex = FMath::Min(PinProperties.IndexOfByKey(UnmatchedProperty), Pins.Num());
					UPCGPin* NewPin = PinAllocator(this);
					NewPin->Modify();
					NewPin->Node = this;
					NewPin->Properties = UnmatchedProperty;
					Pins.Insert(NewPin, InsertIndex);
				}
			}
		}

		return (bAppliedEdgeChanges ? EPCGChangeType::Edge : EPCGChangeType::None) | (bChangedPins ? EPCGChangeType::Node : EPCGChangeType::None);
	};

	EPCGChangeType ChangeType = EPCGChangeType::None;
	ChangeType |= UpdatePins(InputPins, InboundPinProperties);
	ChangeType |= UpdatePins(OutputPins, OutboundPinProperties);

#if WITH_EDITOR
	OnNodeChangedDelegate.Broadcast(this, ChangeType);
#endif // WITH_EDITOR
		
	return ChangeType;
}

EPCGChangeType UPCGNode::UpdateDynamicPins(const UPCGNode* FromNode /*= nullptr*/)
{
	EPCGChangeType ChangeType = EPCGChangeType::None;
	const UPCGSettings* Settings = GetSettings();
	if (!Settings || !Settings->HasDynamicPins())
	{
		return ChangeType;
	}

	ChangeType |= UpdatePins();
	
	for (UPCGPin* OutputPin : OutputPins)
	{
		for (UPCGEdge* Edge : OutputPin->Edges)
		{
			const UPCGPin* OtherPin = Edge->GetOtherPin(OutputPin);
			if (!OtherPin)
			{
				continue;
			}
			
			UPCGNode* OtherNode = OtherPin->Node;
			if (!OtherNode || OtherNode == FromNode)
			{
				continue;
			}
			
			const UPCGSettings* OtherSettings = OtherNode->GetSettings();
			if (Settings && OtherSettings->HasDynamicPins())
			{
				ChangeType |= OtherNode->UpdateDynamicPins(this);
			}
		}
	}
	
	return ChangeType;
}

#if WITH_EDITOR

void UPCGNode::GetNodePosition(int32& OutPositionX, int32& OutPositionY) const
{
	OutPositionX = PositionX;
	OutPositionY = PositionY;
}

void UPCGNode::SetNodePosition(int32 InPositionX, int32 InPositionY)
{
	PositionX = InPositionX;
	PositionY = InPositionY;
}

#endif
