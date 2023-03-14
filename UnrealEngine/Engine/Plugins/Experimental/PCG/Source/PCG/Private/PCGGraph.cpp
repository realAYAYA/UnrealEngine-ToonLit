// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraph.h"
#include "PCGEdge.h"
#include "PCGInputOutputSettings.h"
#include "PCGSubgraph.h"
#include "PCGSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UPCGGraph::UPCGGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputNode = ObjectInitializer.CreateDefaultSubobject<UPCGNode>(this, TEXT("DefaultInputNode"));
	InputNode->SetFlags(RF_Transactional);

	// Since pins would be allocated after initializing the input/output nodes, we must make sure to allocate them using the object initializer
	int NumAllocatedPins = 1;
	auto PinAllocator = [&ObjectInitializer, &NumAllocatedPins](UPCGNode* Node)
	{ 
		FName DefaultPinName = TEXT("DefaultPin");
		DefaultPinName.SetNumber(NumAllocatedPins++);
		return ObjectInitializer.CreateDefaultSubobject<UPCGPin>(Node, DefaultPinName);
	};

	UPCGGraphInputOutputSettings* InputSettings = ObjectInitializer.CreateDefaultSubobject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultInputNodeSettings"));
	InputSettings->SetInput(true);
	InputNode->SetDefaultSettings(InputSettings, /*bUpdatePins=*/false);
	InputNode->UpdatePins(PinAllocator);
	
	OutputNode = ObjectInitializer.CreateDefaultSubobject<UPCGNode>(this, TEXT("DefaultOutputNode"));
	OutputNode->SetFlags(RF_Transactional);

	UPCGGraphInputOutputSettings* OutputSettings = ObjectInitializer.CreateDefaultSubobject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultOutputNodeSettings"));
	OutputSettings->SetInput(false);
	OutputNode->SetDefaultSettings(OutputSettings, /*bUpdatePins=*/false);
	OutputNode->UpdatePins(PinAllocator);
	
#if WITH_EDITORONLY_DATA
	OutputNode->PositionX = 200;
#endif

#if WITH_EDITOR
	InputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
	OutputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
#endif 

	// Note: default connection from input to output
	// should be added when creating from scratch,
	// but not when using a blueprint construct script.
	//InputNode->ConnectTo(OutputNode);
	//OutputNode->ConnectFrom(InputNode);
}

void UPCGGraph::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// TODO: review this once the data has been updated, it's unwanted weight going forward
	// Deprecation
	InputNode->ConditionalPostLoad();

	if (!Cast<UPCGGraphInputOutputSettings>(InputNode->DefaultSettings))
	{
		InputNode->DefaultSettings = NewObject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultInputNodeSettings"));
	}

	Cast<UPCGGraphInputOutputSettings>(InputNode->DefaultSettings)->SetInput(true);

	OutputNode->ConditionalPostLoad();

	if (!Cast<UPCGGraphInputOutputSettings>(OutputNode->DefaultSettings))
	{
		OutputNode->DefaultSettings = NewObject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultOutputNodeSettings"));
	}

	Cast<UPCGGraphInputOutputSettings>(OutputNode->DefaultSettings)->SetInput(false);

	// Ensure that all nodes are loaded (& updated their deprecated data)
	for (UPCGNode* Node : Nodes)
	{
		Node->ConditionalPostLoad();
	}
	
	// Also do this for ExtraNodes
	for (UObject* ExtraNode : ExtraEditorNodes)
	{
		ExtraNode->ConditionalPostLoad();
	}

	// Update pins on all nodes
	InputNode->UpdatePins();
	OutputNode->UpdatePins();

	for (UPCGNode* Node : Nodes)
	{
		Node->UpdatePins();
	}

	// Finally, apply deprecation that changes edges/rebinds
	InputNode->ApplyDeprecation();
	OutputNode->ApplyDeprecation();
	
	for (UPCGNode* Node : Nodes)
	{
		Node->ApplyDeprecation();
	}

	InputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
	OutputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);

	for (UPCGNode* Node : Nodes)
	{
		OnNodeAdded(Node);
	}
#endif
}

void UPCGGraph::BeginDestroy()
{
#if WITH_EDITOR
	for (UPCGNode* Node : Nodes)
	{
		OnNodeRemoved(Node);
	}

	OutputNode->OnNodeChangedDelegate.RemoveAll(this);
	InputNode->OnNodeChangedDelegate.RemoveAll(this);

	// Notify the compiler to remove this graph from its cache
	if (GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		UPCGSubsystem* PCGSubsystem = World ? World->GetSubsystem<UPCGSubsystem>() : nullptr;
		if (PCGSubsystem)
		{
			PCGSubsystem->NotifyGraphChanged(this);
		}
	}

#endif

	Super::BeginDestroy();
}

UPCGNode* UPCGGraph::AddNodeOfType(TSubclassOf<class UPCGSettings> InSettingsClass, UPCGSettings*& OutDefaultNodeSettings)
{
	UPCGSettings* Settings = NewObject<UPCGSettings>(GetTransientPackage(), InSettingsClass);

	if (!Settings)
	{
		return nullptr;
	}

	UPCGNode* Node = Settings->CreateNode();

	if (Node)
	{
		Node->SetFlags(RF_Transactional);

		Modify();

		// Assign settings to node
		Node->SetDefaultSettings(Settings);
		Settings->Rename(nullptr, Node);
		Settings->SetFlags(RF_Transactional);

		// Reparent node to this graph
		Node->Rename(nullptr, this);

#if WITH_EDITOR
		const FName DefaultNodeName = Settings->GetDefaultNodeName();
		if (DefaultNodeName != NAME_None)
		{
			FName NodeName = MakeUniqueObjectName(this, UPCGNode::StaticClass(), DefaultNodeName);
			Node->Rename(*NodeName.ToString());
		}
#endif

		Nodes.Add(Node);
		OnNodeAdded(Node);
	}

	OutDefaultNodeSettings = Settings;
	return Node;
}

UPCGNode* UPCGGraph::AddNode(UPCGSettings* InSettings)
{
	if (!InSettings)
	{
		return nullptr;
	}

	UPCGNode* Node = InSettings->CreateNode();

	if (Node)
	{
		Node->SetFlags(RF_Transactional);

		Modify();

		// Assign settings to node & reparent
		Node->SetDefaultSettings(InSettings);

		// Reparent node to this graph
		Node->Rename(nullptr, this);

#if WITH_EDITOR
		const FName DefaultNodeName = InSettings->GetDefaultNodeName();
		if (DefaultNodeName != NAME_None)
		{
			FName NodeName = MakeUniqueObjectName(this, UPCGNode::StaticClass(), DefaultNodeName);
			Node->Rename(*NodeName.ToString());
		}
#endif

		Nodes.Add(Node);
		OnNodeAdded(Node);
	}

	return Node;
}

void UPCGGraph::OnNodeAdded(UPCGNode* InNode)
{
#if WITH_EDITOR
	InNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
	NotifyGraphChanged(EPCGChangeType::Structural);
#endif
}

void UPCGGraph::OnNodeRemoved(UPCGNode* InNode)
{
#if WITH_EDITOR
	if (InNode)
	{
		InNode->OnNodeChangedDelegate.RemoveAll(this);
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif
}

UPCGNode* UPCGGraph::AddEdge(UPCGNode* From, UPCGNode* To)
{
	return AddLabeledEdge(From, NAME_None, To, NAME_None);
}

UPCGNode* UPCGGraph::AddLabeledEdge(UPCGNode* From, const FName& InboundLabel, UPCGNode* To, const FName& OutboundLabel)
{
	if (!From || !To)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid edge nodes"));
		return To;
	}

	UPCGPin* FromPin = From->GetOutputPin(InboundLabel);

	if(!FromPin)
	{
		UE_LOG(LogPCG, Error, TEXT("From node %s does not have the %s label"), *From->GetName(), *InboundLabel.ToString());
		return To;
	}

	UPCGPin* ToPin = To->GetInputPin(OutboundLabel);

	if(!ToPin)
	{
		UE_LOG(LogPCG, Error, TEXT("To node %s does not have the %s label"), *To->GetName(), *OutboundLabel.ToString());
		return To;
	}

	// Create edge
	FromPin->AddEdgeTo(ToPin);
	
#if WITH_EDITOR
	NotifyGraphChanged(EPCGChangeType::Structural);
#endif

	return To;
}

TObjectPtr<UPCGNode> UPCGGraph::ReconstructNewNode(const UPCGNode* InNode)
{
	TObjectPtr<UPCGSettings> NewSettings = DuplicateObject(InNode->DefaultSettings, nullptr);
	TObjectPtr<UPCGNode> NewNode = AddNode(NewSettings);
	NewSettings->Rename(nullptr, NewNode);

#if WITH_EDITOR
	InNode->TransferEditorProperties(NewNode);
#endif // WITH_EDITOR

	return NewNode;
}

bool UPCGGraph::Contains(UPCGNode* Node) const
{
	return Node == InputNode || Node == OutputNode || Nodes.Contains(Node);
}

void UPCGGraph::AddNode(UPCGNode* InNode)
{
	check(InNode);

	Modify();

	InNode->Rename(nullptr, this);

#if WITH_EDITOR
	const FName DefaultNodeName = InNode->DefaultSettings->GetDefaultNodeName();
	if (DefaultNodeName != NAME_None)
	{
		FName NodeName = MakeUniqueObjectName(this, UPCGNode::StaticClass(), DefaultNodeName);
		InNode->Rename(*NodeName.ToString());
	}
#endif

	Nodes.Add(InNode);
	OnNodeAdded(InNode);
}

void UPCGGraph::RemoveNode(UPCGNode* InNode)
{
	check(InNode);

	Modify();

	for (UPCGPin* InputPin : InNode->InputPins)
	{
		InputPin->BreakAllEdges();
	}

	for (UPCGPin* OutputPin : InNode->OutputPins)
	{
		OutputPin->BreakAllEdges();
	}

	Nodes.Remove(InNode);
	OnNodeRemoved(InNode);
}

bool UPCGGraph::RemoveEdge(UPCGNode* From, const FName& FromLabel, UPCGNode* To, const FName& ToLabel)
{
	if (!From || !To)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid from/to node in RemoveEdge"));
		return false;
	}

	UPCGPin* OutPin = From->GetOutputPin(FromLabel);
	UPCGPin* InPin = To->GetInputPin(ToLabel);

	const bool bChanged = OutPin && OutPin->BreakEdgeTo(InPin);

#if WITH_EDITOR
	if (bChanged)
	{
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif

	return bChanged;
}

bool UPCGGraph::RemoveAllInboundEdges(UPCGNode* InNode)
{
	check(InNode);
	bool bChanged = false;

	for (UPCGPin* InputPin : InNode->InputPins)
	{
		bChanged |= InputPin->BreakAllEdges();
	}

#if WITH_EDITOR
	if (bChanged)
	{
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif

	return bChanged;
}

bool UPCGGraph::RemoveAllOutboundEdges(UPCGNode* InNode)
{
	check(InNode);
	bool bChanged = false;
	for (UPCGPin* OutputPin : InNode->OutputPins)
	{
		bChanged |= OutputPin->BreakAllEdges();
	}

#if WITH_EDITOR
	if (bChanged)
	{
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif

	return bChanged;
}

bool UPCGGraph::RemoveInboundEdges(UPCGNode* InNode, const FName& InboundLabel)
{
	check(InNode);
	bool bChanged = false;

	if (UPCGPin* InputPin = InNode->GetInputPin(InboundLabel))
	{
		bChanged = InputPin->BreakAllEdges();
	}

#if WITH_EDITOR
	if (bChanged)
	{
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif

	return bChanged;
}

bool UPCGGraph::RemoveOutboundEdges(UPCGNode* InNode, const FName& OutboundLabel)
{
	check(InNode);
	bool bChanged = false;

	if (UPCGPin* OutputPin = InNode->GetOutputPin(OutboundLabel))
	{
		bChanged = OutputPin->BreakAllEdges();
	}

#if WITH_EDITOR
	if (bChanged)
	{
		NotifyGraphChanged(EPCGChangeType::Structural);
	}
#endif

	return bChanged;
}

#if WITH_EDITOR
void UPCGGraph::ForceNotificationForEditor()
{
	// Queue up the delayed change
	NotifyGraphChanged(EPCGChangeType::Structural);
	if (bUserPausedNotificationsInGraphEditor)
	{
		EnableNotificationsForEditor();
		DisableNotificationsForEditor();
	}
}

void UPCGGraph::PreNodeUndo(UPCGNode* InPCGNode)
{
	if (InPCGNode)
	{
		InPCGNode->OnNodeChangedDelegate.RemoveAll(this);
	}
}

void UPCGGraph::PostNodeUndo(UPCGNode* InPCGNode)
{
	if (InPCGNode)
	{
		InPCGNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
	}
}
#endif

#if WITH_EDITOR
void UPCGGraph::DisableNotificationsForEditor()
{
	check(GraphChangeNotificationsDisableCounter >= 0);
	++GraphChangeNotificationsDisableCounter;
}

void UPCGGraph::EnableNotificationsForEditor()
{
	check(GraphChangeNotificationsDisableCounter > 0);
	--GraphChangeNotificationsDisableCounter;

	if (GraphChangeNotificationsDisableCounter == 0 && bDelayedChangeNotification)
	{
		NotifyGraphChanged(DelayedChangeType);
		bDelayedChangeNotification = false;
		DelayedChangeType = EPCGChangeType::None;
	}
}

void UPCGGraph::ToggleUserPausedNotificationsForEditor()
{
	if (bUserPausedNotificationsInGraphEditor)
	{
		EnableNotificationsForEditor();
	}
	else
	{
		DisableNotificationsForEditor();
	}

	bUserPausedNotificationsInGraphEditor = !bUserPausedNotificationsInGraphEditor;
}

void UPCGGraph::SetExtraEditorNodes(const TArray<TObjectPtr<const UObject>>& InNodes)
{
	ExtraEditorNodes.Empty();

	for (const UObject* Node : InNodes)
	{
		ExtraEditorNodes.Add(DuplicateObject(Node, this));
	}
}

FPCGTagToSettingsMap UPCGGraph::GetTrackedTagsToSettings() const
{
	FPCGTagToSettingsMap TagsToSettings;
	TArray<TObjectPtr<const UPCGGraph>> VisitedGraphs;

	GetTrackedTagsToSettings(TagsToSettings, VisitedGraphs);
	return TagsToSettings;
}

void UPCGGraph::GetTrackedTagsToSettings(FPCGTagToSettingsMap& OutTagsToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (OutVisitedGraphs.Contains(this))
	{
		return;
	}

	OutVisitedGraphs.Emplace(this);

	for (UPCGNode* Node : Nodes)
	{
		if (Node && Node->DefaultSettings)
		{
			Node->DefaultSettings->GetTrackedActorTags(OutTagsToSettings, OutVisitedGraphs);
		}
	}
}

void UPCGGraph::NotifyGraphChanged(EPCGChangeType ChangeType)
{
	if (GraphChangeNotificationsDisableCounter > 0)
	{
		bDelayedChangeNotification = true;
		DelayedChangeType |= ChangeType;
		return;
	}

	// Skip recursive cases which can happen either through direct recursivity (A -> A) or indirectly (A -> B -> A)
	if (bIsNotifying)
	{
		return;
	}

	bIsNotifying = true;

	// Notify the subsystem/compiler cache before so it gets recompiled properly
	const bool bNotifySubsystem = ((ChangeType & (EPCGChangeType::Structural | EPCGChangeType::Edge)) != EPCGChangeType::None);
	if (bNotifySubsystem && GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		UPCGSubsystem* PCGSubsystem = World ? World->GetSubsystem<UPCGSubsystem>() : nullptr;
		if (PCGSubsystem)
		{
			PCGSubsystem->NotifyGraphChanged(this);
		}
	}

	OnGraphChangedDelegate.Broadcast(this, ChangeType);

	bIsNotifying = false;
}

void UPCGGraph::OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType)
{
	if((ChangeType & ~EPCGChangeType::Cosmetic) != EPCGChangeType::None)
	{
		NotifyGraphChanged(ChangeType);
	}
}

void UPCGGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGGraph, bLandscapeUsesMetadata))
	{
		NotifyGraphChanged(EPCGChangeType::Input);
	}
}
#endif // WITH_EDITOR
