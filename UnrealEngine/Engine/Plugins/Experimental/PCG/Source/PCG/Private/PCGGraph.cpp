// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraph.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "PCGSubgraph.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Elements/PCGUserParameterGet.h"

#if WITH_EDITOR
#include "CoreGlobals.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "EdGraph/EdGraphPin.h"
#else
#include "UObject/Package.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGraph)

#define LOCTEXT_NAMESPACE "PCGGraph"

namespace PCGGraphUtils
{
	/** Returns true if the two descriptors are valid and compatible */
	bool ArePropertiesCompatible(const FPropertyBagPropertyDesc* InSourcePropertyDesc, const FPropertyBagPropertyDesc* InTargetPropertyDesc)
	{
		return InSourcePropertyDesc && InTargetPropertyDesc && InSourcePropertyDesc->CompatibleType(*InTargetPropertyDesc);
	}

	/** Checks if the value for a source property in a source struct has the same value that the target property in the target struct. */
	bool ArePropertiesIdentical(const FPropertyBagPropertyDesc* InSourcePropertyDesc, const FInstancedPropertyBag& InSourceInstance, const FPropertyBagPropertyDesc* InTargetPropertyDesc, const FInstancedPropertyBag& InTargetInstance)
	{
		if (!InSourceInstance.IsValid() || !InTargetInstance.IsValid() || !InSourcePropertyDesc || !InSourcePropertyDesc->CachedProperty || !InTargetPropertyDesc || !InTargetPropertyDesc->CachedProperty)
		{
			return false;
		}

		if (!InSourcePropertyDesc->CompatibleType(*InTargetPropertyDesc))
		{
			return false;
		}

		const uint8* SourceValueAddress = InSourceInstance.GetValue().GetMemory() + InSourcePropertyDesc->CachedProperty->GetOffset_ForInternal();
		const uint8* TargetValueAddress = InTargetInstance.GetValue().GetMemory() + InTargetPropertyDesc->CachedProperty->GetOffset_ForInternal();

		return InSourcePropertyDesc->CachedProperty->Identical(SourceValueAddress, TargetValueAddress);
	}

	/** Copy the value for a source property in a source struct to the target property in the target struct. */
	void CopyPropertyValue(const FPropertyBagPropertyDesc* InSourcePropertyDesc, const FInstancedPropertyBag& InSourceInstance, const FPropertyBagPropertyDesc* InTargetPropertyDesc, FInstancedPropertyBag& InTargetInstance)
	{
		if (!InSourceInstance.IsValid() || !InTargetInstance.IsValid() || !InSourcePropertyDesc || !InSourcePropertyDesc->CachedProperty || !InTargetPropertyDesc || !InTargetPropertyDesc->CachedProperty)
		{
			return;
		}

		// Can't copy if they are not compatible.
		if (!InSourcePropertyDesc->CompatibleType(*InTargetPropertyDesc))
		{
			return;
		}

		const uint8* SourceValueAddress = InSourceInstance.GetValue().GetMemory() + InSourcePropertyDesc->CachedProperty->GetOffset_ForInternal();
		uint8* TargetValueAddress = InTargetInstance.GetMutableValue().GetMemory() + InTargetPropertyDesc->CachedProperty->GetOffset_ForInternal();

		InSourcePropertyDesc->CachedProperty->CopyCompleteValue(TargetValueAddress, SourceValueAddress);
	}

	EPCGChangeType NotifyTouchedNodes(const TSet<UPCGNode*>& InTouchedNodes)
	{
		EPCGChangeType ChangeType = EPCGChangeType::None;

		for (UPCGNode* TouchedNode : InTouchedNodes)
		{
			if (TouchedNode)
			{
				ChangeType |= TouchedNode->PropagateDynamicPinTypes();

#if WITH_EDITOR
				TouchedNode->OnNodeChangedDelegate.Broadcast(TouchedNode, EPCGChangeType::Node);
#endif
			}
		}

		return ChangeType;
	}
}

/****************************
* UPCGGraphInterface
****************************/

bool UPCGGraphInterface::IsInstance() const
{
	return this != GetGraph();
}

bool UPCGGraphInterface::IsEquivalent(const UPCGGraphInterface* Other) const
{
	if (this == Other)
	{
		return true;
	}

	const UPCGGraph* OtherGraph = Other ? Other->GetGraph() : nullptr;
	const UPCGGraph* ThisGraph = GetGraph();

	if (ThisGraph != OtherGraph)
	{
		return false;
	}
	else if (!ThisGraph && !OtherGraph)
	{
		return true;
	}

	const FInstancedPropertyBag* OtherParameters = Other->GetUserParametersStruct();
	const FInstancedPropertyBag* ThisParameters = this->GetUserParametersStruct();
	check(OtherParameters && ThisParameters);

	if (ThisParameters->GetNumPropertiesInBag() != OtherParameters->GetNumPropertiesInBag())
	{
		return false;
	}

	const UPropertyBag* OtherPropertyBag = OtherParameters->GetPropertyBagStruct();
	const UPropertyBag* ThisPropertyBag = ThisParameters->GetPropertyBagStruct();

	if (!ThisPropertyBag || !OtherPropertyBag)
	{
		return ThisPropertyBag == OtherPropertyBag;
	}

	// TODO: Be more resitant to different layout.
	// For now we are only comparing structs that must have the same layout.
	TConstArrayView<FPropertyBagPropertyDesc> OtherParametersDescs = OtherPropertyBag->GetPropertyDescs();
	TConstArrayView<FPropertyBagPropertyDesc> ThisParametersDescs = ThisPropertyBag->GetPropertyDescs();
	check(OtherParametersDescs.Num() == ThisParametersDescs.Num());

	// TODO: Hashing might be more efficient.
	for (int32 i = 0; i < ThisParametersDescs.Num(); ++i)
	{
		const FPropertyBagPropertyDesc& ThisParametersDesc = ThisParametersDescs[i];
		const FPropertyBagPropertyDesc& OtherParametersDesc = OtherParametersDescs[i];

		if (!PCGGraphUtils::ArePropertiesCompatible(&ThisParametersDesc, &OtherParametersDesc))
		{
			return false;
		}

		if (!PCGGraphUtils::ArePropertiesIdentical(&ThisParametersDesc, *ThisParameters, &OtherParametersDesc, *OtherParameters))
		{
			return false;
		}
	}

	return true;
}

/****************************
* UPCGGraph
****************************/

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
	InputNode->SetSettingsInterface(InputSettings, /*bUpdatePins=*/false);

	// Only allocate default pins if this is the default object
	if (this->HasAnyFlags(RF_ClassDefaultObject))
	{
		InputNode->CreateDefaultPins(PinAllocator);
	}
	else
	{
		InputNode->UpdatePins();
	}

	OutputNode = ObjectInitializer.CreateDefaultSubobject<UPCGNode>(this, TEXT("DefaultOutputNode"));
	OutputNode->SetFlags(RF_Transactional);

	UPCGGraphInputOutputSettings* OutputSettings = ObjectInitializer.CreateDefaultSubobject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultOutputNodeSettings"));
	OutputSettings->SetInput(false);
	OutputNode->SetSettingsInterface(OutputSettings, /*bUpdatePins=*/false);

	// Only allocate default pins if this is the default object
	if (this->HasAnyFlags(RF_ClassDefaultObject))
	{
		OutputNode->CreateDefaultPins(PinAllocator);
	}
	else
	{
		OutputNode->UpdatePins();
	}

#if WITH_EDITOR
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

	// Force the user parameters to have an empty property bag. It is necessary to catch the first
	// add property into the undo/redo history.
	UserParameters.MigrateToNewBagStruct(UPropertyBag::GetOrCreateFromDescs({}));
}

void UPCGGraph::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// TODO: review this once the data has been updated, it's unwanted weight going forward
	// Deprecation
	InputNode->ConditionalPostLoad();

	if (!Cast<UPCGGraphInputOutputSettings>(InputNode->GetSettings()))
	{
		InputNode->SetSettingsInterface(NewObject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultInputNodeSettings")));
	}

	Cast<UPCGGraphInputOutputSettings>(InputNode->GetSettings())->SetInput(true);

	OutputNode->ConditionalPostLoad();

	if (!Cast<UPCGGraphInputOutputSettings>(OutputNode->GetSettings()))
	{
		OutputNode->SetSettingsInterface(NewObject<UPCGGraphInputOutputSettings>(this, TEXT("DefaultOutputNodeSettings")));
	}

	Cast<UPCGGraphInputOutputSettings>(OutputNode->GetSettings())->SetInput(false);

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

	// Create a copy to iterate through the nodes while more might be added
	TArray<UPCGNode*> NodesCopy(Nodes);
	for (UPCGNode* Node : NodesCopy)
	{
		Node->ApplyStructuralDeprecation();
	}

	// Finally, apply deprecation that changes edges/rebinds
	ForEachNode([](UPCGNode* InNode) { return InNode->ApplyDeprecationBeforeUpdatePins(); });

	// Update pins on all nodes
	ForEachNode([](UPCGNode* InNode) { return InNode->UpdatePins(); });

	// Finally, apply deprecation that changes edges/rebinds
	ForEachNode([](UPCGNode* InNode) { return InNode->ApplyDeprecation(); });

	InputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);
	OutputNode->OnNodeChangedDelegate.AddUObject(this, &UPCGGraph::OnNodeChanged);

	// Also, try to remove all nodes that are invalid (meaning that the settings are null)
	// We remove it at the end, to let the nodes that have null settings to clean up their pins and edges.
	for (int32 i = Nodes.Num() - 1; i >= 0; --i)
	{
		if (!Nodes[i]->GetSettings())
		{
			Nodes.RemoveAtSwap(i);
		}
		else
		{
			OnNodeAdded(Nodes[i]);
		}
	}
#endif
}

#if WITH_EDITOR
void UPCGGraph::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UPCGPin::StaticClass()));
}
#endif

void UPCGGraph::BeginDestroy()
{
#if WITH_EDITOR
	for (UPCGNode* Node : Nodes)
	{
		OnNodeRemoved(Node);
	}

	if (OutputNode)
	{
		OutputNode->OnNodeChangedDelegate.RemoveAll(this);
	}

	if (InputNode)
	{
		InputNode->OnNodeChangedDelegate.RemoveAll(this);
	}

	// Notify the compiler to remove this graph from its cache
	if (GEditor)
	{
		if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
		{
			PCGSubsystem->NotifyGraphChanged(this);
		}
	}

#endif

	Super::BeginDestroy();
}

uint32 UPCGGraph::GetDefaultGridSize() const
{
	ensure(IsHierarchicalGenerationEnabled());
	return PCGHiGenGrid::IsValidGrid(HiGenGridSize) ? PCGHiGenGrid::GridToGridSize(HiGenGridSize) : PCGHiGenGrid::UnboundedGridSize();
}

UPCGNode* UPCGGraph::AddNodeOfType(TSubclassOf<class UPCGSettings> InSettingsClass, UPCGSettings*& OutDefaultNodeSettings)
{
	UPCGSettings* Settings = NewObject<UPCGSettings>(GetTransientPackage(), InSettingsClass, NAME_None, RF_Transactional);

	if (!Settings)
	{
		return nullptr;
	}

	UPCGNode* Node = AddNode(Settings);

	if (Node)
	{
		Settings->Rename(nullptr, Node, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
	}

	OutDefaultNodeSettings = Settings;
	return Node;
}

UPCGNode* UPCGGraph::AddNode(UPCGSettingsInterface* InSettingsInterface)
{
	if (!InSettingsInterface || !InSettingsInterface->GetSettings())
	{
		return nullptr;
	}

	UPCGNode* Node = InSettingsInterface->GetSettings()->CreateNode();

	if (Node)
	{
		Node->SetFlags(RF_Transactional);

		Modify();

		// Assign settings to node & reparent
		Node->SetSettingsInterface(InSettingsInterface);

		// Reparent node to this graph
		Node->Rename(nullptr, this, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);

#if WITH_EDITOR
		const FName DefaultNodeName = InSettingsInterface->GetSettings()->GetDefaultNodeName();
		if (DefaultNodeName != NAME_None)
		{
			const FName NodeName = MakeUniqueObjectName(this, UPCGNode::StaticClass(), DefaultNodeName);
			// Flags added because default flags favor tick/interactive, not load-time renaming.
			Node->Rename(*NodeName.ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		}
#endif

		Nodes.Add(Node);
		OnNodeAdded(Node);
	}

	return Node;
}

UPCGNode* UPCGGraph::AddNodeInstance(UPCGSettings* InSettings)
{
	if (!InSettings)
	{
		return nullptr;
	}

	UPCGSettingsInstance* SettingsInstance = NewObject<UPCGSettingsInstance>();
	SettingsInstance->SetSettings(InSettings);

	UPCGNode* Node = AddNode(SettingsInstance);

	if (Node)
	{
		SettingsInstance->Rename(nullptr, Node);
		SettingsInstance->SetFlags(RF_Transactional);
	}

	return Node;
}

UPCGNode* UPCGGraph::AddNodeCopy(UPCGSettings* InSettings, UPCGSettings*& DefaultNodeSettings)
{
	if (!InSettings)
	{
		return nullptr;
	}

	UPCGSettings* SettingsCopy = DuplicateObject(InSettings, nullptr);
	UPCGNode* NewNode = AddNode(SettingsCopy);

	if (SettingsCopy)
	{
		SettingsCopy->Rename(nullptr, NewNode);
	}

	DefaultNodeSettings = SettingsCopy;
	return NewNode;
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

UPCGNode* UPCGGraph::AddEdge(UPCGNode* From, const FName& FromPinLabel, UPCGNode* To, const FName& ToPinLabel)
{
	AddLabeledEdge(From, FromPinLabel, To, ToPinLabel);
	return To;
}

bool UPCGGraph::AddLabeledEdge(UPCGNode* From, const FName& FromPinLabel, UPCGNode* To, const FName& ToPinLabel)
{
	if (!From || !To)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid edge nodes"));
		return false;
	}

	UPCGPin* FromPin = From->GetOutputPin(FromPinLabel);

	if(!FromPin)
	{
		UE_LOG(LogPCG, Error, TEXT("From node %s does not have the %s label"), *From->GetName(), *FromPinLabel.ToString());
		return false;
	}

	UPCGPin* ToPin = To->GetInputPin(ToPinLabel);

	if(!ToPin)
	{
		UE_LOG(LogPCG, Error, TEXT("To node %s does not have the %s label"), *To->GetName(), *ToPinLabel.ToString());
		return false;
	}

	TSet<UPCGNode*> TouchedNodes;

	// Create edge
	FromPin->AddEdgeTo(ToPin, &TouchedNodes);

	bool bToPinBrokeOtherEdges = false;
	
	// Add an edge to a pin that doesn't allow multiple connections requires to do some cleanup
	if (!ToPin->AllowMultipleConnections())
	{
		bToPinBrokeOtherEdges = ToPin->BreakAllIncompatibleEdges(&TouchedNodes);
	}
	
	const EPCGChangeType ChangeType = PCGGraphUtils::NotifyTouchedNodes(TouchedNodes);

#if WITH_EDITOR
	NotifyGraphChanged(ChangeType | EPCGChangeType::Structural);
#endif

	return bToPinBrokeOtherEdges;
}

TObjectPtr<UPCGNode> UPCGGraph::ReconstructNewNode(const UPCGNode* InNode)
{
	UPCGSettings* NewSettings = nullptr;
	TObjectPtr<UPCGNode> NewNode = AddNodeCopy(InNode->GetSettings(), NewSettings);

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
	const FName DefaultNodeName = InNode->GetSettings()->GetDefaultNodeName();
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
	
	TSet<UPCGNode*> TouchedNodes;

	for (UPCGPin* InputPin : InNode->InputPins)
	{
		InputPin->BreakAllEdges(&TouchedNodes);
	}

	for (UPCGPin* OutputPin : InNode->OutputPins)
	{
		OutputPin->BreakAllEdges(&TouchedNodes);
	}

	// We're about to remove InNode, so don't bother triggering updates
	TouchedNodes.Remove(InNode);

	PCGGraphUtils::NotifyTouchedNodes(TouchedNodes);

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

	TSet<UPCGNode*> TouchedNodes;
	if (OutPin)
	{
		OutPin->BreakEdgeTo(InPin, &TouchedNodes);
	}

	const EPCGChangeType ChangeType = PCGGraphUtils::NotifyTouchedNodes(TouchedNodes);

	if (TouchedNodes.Num() > 0)
	{
#if WITH_EDITOR
		NotifyGraphChanged(ChangeType | EPCGChangeType::Structural);
#endif
	}

	return TouchedNodes.Num() > 0;
}

void UPCGGraph::ForEachNode(const TFunction<void(UPCGNode*)>& Action)
{
	Action(InputNode);
	Action(OutputNode);

	for (UPCGNode* Node : Nodes)
	{
		Action(Node);
	}
}

bool UPCGGraph::RemoveInboundEdges(UPCGNode* InNode, const FName& InboundLabel)
{
	check(InNode);
	TSet<UPCGNode*> TouchedNodes;

	if (UPCGPin* InputPin = InNode->GetInputPin(InboundLabel))
	{
		InputPin->BreakAllEdges(&TouchedNodes);
	}

	const EPCGChangeType ChangeType = PCGGraphUtils::NotifyTouchedNodes(TouchedNodes);

#if WITH_EDITOR
	if (TouchedNodes.Num() > 0)
	{
		NotifyGraphChanged(ChangeType | EPCGChangeType::Structural);
	}
#endif

	return TouchedNodes.Num() > 0;
}

bool UPCGGraph::RemoveOutboundEdges(UPCGNode* InNode, const FName& OutboundLabel)
{
	check(InNode);
	// Make a list of downstream nodes which may need pin updates when the edges change
	TSet<UPCGNode*> TouchedNodes;

	if (UPCGPin* OutputPin = InNode->GetOutputPin(OutboundLabel))
	{
		OutputPin->BreakAllEdges(&TouchedNodes);
	}

	const EPCGChangeType ChangeType = PCGGraphUtils::NotifyTouchedNodes(TouchedNodes);

#if WITH_EDITOR
	if (TouchedNodes.Num() > 0)
	{
		NotifyGraphChanged(ChangeType | EPCGChangeType::Structural);
	}
#endif

	return TouchedNodes.Num() > 0;
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

void UPCGGraph::GetGridSizes(PCGHiGenGrid::FSizeArray& OutGridSizes, bool& bOutHasUnbounded) const
{
	bOutHasUnbounded = HiGenGridSize == EPCGHiGenGrid::Unbounded;

	const uint32 GraphDefaultGridSize = GetDefaultGridSize();
	if (!IsHierarchicalGenerationEnabled())
	{
		if (PCGHiGenGrid::IsValidGridSize(GetDefaultGridSize()))
		{
			OutGridSizes.Add(GraphDefaultGridSize);
		}
		return;
	}

	bool bHasUninitialized = false;
	for (const UPCGNode* Node : Nodes)
	{
		const uint32 GridSize = GetNodeGenerationGridSize(Node, GraphDefaultGridSize);
		if (PCGHiGenGrid::IsValidGridSize(GridSize))
		{
			if (!OutGridSizes.Contains(GridSize))
			{
				OutGridSizes.Add(GridSize);
			}
		}
		else if (GridSize == PCGHiGenGrid::UnboundedGridSize())
		{
			bOutHasUnbounded = true;
		}
		else if (GridSize == PCGHiGenGrid::UninitializedGridSize())
		{
			// Outside nodes will not have a concrete grid set
			bHasUninitialized = true;
		}
	}

	if (bHasUninitialized)
	{
		// Nodes outside grid ranges will execute at graph default
		OutGridSizes.Add(GraphDefaultGridSize);
	}

	// Descending
	OutGridSizes.Sort([](const uint32& A, const uint32& B) { return A > B; });

	return;
}

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

void UPCGGraph::RemoveExtraEditorNode(const UObject* InNode)
{
	ExtraEditorNodes.Remove(const_cast<UObject*>(InNode));
}

FPCGActorSelectionKeyToSettingsMap UPCGGraph::GetTrackedActorKeysToSettings() const
{
	FPCGActorSelectionKeyToSettingsMap TagsToSettings;
	TArray<TObjectPtr<const UPCGGraph>> VisitedGraphs;

	GetTrackedActorKeysToSettings(TagsToSettings, VisitedGraphs);
	return TagsToSettings;
}

void UPCGGraph::GetTrackedActorKeysToSettings(FPCGActorSelectionKeyToSettingsMap& OutTagsToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (OutVisitedGraphs.Contains(this))
	{
		return;
	}

	OutVisitedGraphs.Emplace(this);

	for (UPCGNode* Node : Nodes)
	{
		if (Node && Node->GetSettings())
		{
			Node->GetSettings()->GetTrackedActorKeys(OutTagsToSettings, OutVisitedGraphs);
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
		if (GEditor->PlayWorld)
		{
			if (UPCGSubsystem* PCGPIESubsystem = UPCGSubsystem::GetInstance(GEditor->PlayWorld.Get()))
			{
				PCGPIESubsystem->NotifyGraphChanged(this);
			}
		}

		if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World()))
		{
			PCGSubsystem->NotifyGraphChanged(this);
		}
	}

	// Graph settings, nodes, graph structure can all change the higen grid sizes.
	if (ChangeType != EPCGChangeType::Cosmetic)
	{
		FWriteScopeLock ScopedWriteLock(NodeToGridSizeLock);
		NodeToGridSize.Reset();
	}

	OnGraphChangedDelegate.Broadcast(this, ChangeType);

	bIsNotifying = false;
}

void UPCGGraph::NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	if (bIsNotifying)
	{
		return;
	}

	bIsNotifying = true;
	OnGraphParametersChangedDelegate.Broadcast(this, InChangeType, InChangedPropertyName);
	bIsNotifying = false;

	NotifyGraphChanged(EPCGChangeType::Settings);
}

void UPCGGraph::OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType)
{
	if (!!(ChangeType & EPCGChangeType::Structural) && Cast<UPCGHiGenGridSizeSettings>(InNode->GetSettings()))
	{
		// Update node to grid size map for grid size changes.
		{
			FWriteScopeLock ScopedWriteLock(NodeToGridSizeLock);
			NodeToGridSize.Reset();
		}

		// Broadcast so that grid size visualization can be updated editor-side.
		OnGraphGridSizesChangedDelegate.Broadcast(this);
	}

	if ((ChangeType & ~EPCGChangeType::Cosmetic) != EPCGChangeType::None)
	{
		NotifyGraphChanged(ChangeType);
	}
}

void UPCGGraph::PreEditChange(FProperty* InProperty)
{
	Super::PreEditChange(InProperty);

	if (!InProperty)
	{
		return;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGraph, UserParameters))
	{
		// We need to keep track of the number of properties, to detect if a property was added/removed/modified
		NumberOfUserParametersPreEdit = UserParameters.GetNumPropertiesInBag();
	}
}

void UPCGGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGGraph, bLandscapeUsesMetadata))
	{
		NotifyGraphChanged(EPCGChangeType::Input);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGGraph, UserParameters))
	{
		EPCGGraphParameterEvent ChangeType;
		int32 NumberOfUserParametersPostEdit = UserParameters.GetNumPropertiesInBag();

		if (NumberOfUserParametersPostEdit > NumberOfUserParametersPreEdit)
		{
			ChangeType = EPCGGraphParameterEvent::Added;
		}
		else if (NumberOfUserParametersPostEdit < NumberOfUserParametersPreEdit)
		{
			ChangeType = EPCGGraphParameterEvent::Removed;
		}
		else //NumberOfUserParametersPostEdit == NumberOfUserParametersPreEdit
		{
			ChangeType = EPCGGraphParameterEvent::PropertyModified;
		}

		OnGraphParametersChanged(ChangeType, NAME_None);
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerStruct() == UserParameters.GetPropertyBagStruct())
	{
		OnGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, MemberPropertyName);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGGraph, HiGenGridSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UPCGGraph, bUseHierarchicalGeneration))
	{
		// The higen settings change the structure of the graph (presence or absence of links between grid levels).
		NotifyGraphChanged(EPCGChangeType::Structural);
	}

	NumberOfUserParametersPreEdit = 0;
}

void UPCGGraph::OnGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	bool bWasModified = false;

	if (InChangeType == EPCGGraphParameterEvent::Removed || InChangeType == EPCGGraphParameterEvent::PropertyModified)
	{
		// Look for all the Get Parameter nodes and make sure to delete all nodes that doesn't exist anymore
		TArray<UPCGNode*> NodesToRemove;

		for (UPCGNode* Node : Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (UPCGUserParameterGetSettings* Settings = Cast<UPCGUserParameterGetSettings>(Node->GetSettings()))
			{
				const FPropertyBagPropertyDesc* PropertyDesc = UserParameters.FindPropertyDescByID(Settings->PropertyGuid);
				if (!PropertyDesc)
				{
					NodesToRemove.Add(Node);
				}
				else if (Settings->PropertyName != PropertyDesc->Name)
				{
					const FName OldName = Settings->PropertyName;
					Settings->UpdatePropertyName(PropertyDesc->Name);
					// We make sure to keep the edges connected, by renaming the pin label
					Node->RenameOutputPin(OldName, PropertyDesc->Name);
				}
			}
		}

		if (!NodesToRemove.IsEmpty())
		{
			bWasModified = true;
			Modify();

			for (UPCGNode* Node : NodesToRemove)
			{
				RemoveNode(Node);
			}
		}
	}

	// Also if we have no more properties, just reset
	if (UserParameters.GetNumPropertiesInBag() == 0)
	{
		if (!bWasModified)
		{
			bWasModified = true;
			Modify();
		}

		UserParameters.Reset();
	}

	NotifyGraphParametersChanged(InChangeType, InChangedPropertyName);
}

bool UPCGGraph::UserParametersIsPinTypeAccepted(FEdGraphPinType InPinType)
{
	// Text and interface not supported
	if (InPinType.PinCategory == TEXT("text") || InPinType.PinCategory == TEXT("interface"))
	{
		return false;
	}
	else if (InPinType.PinCategory == TEXT("struct"))
	{
		// Structs other than Vector/Transform/Rotator not supported
		return InPinType.PinSubCategoryObject == TBaseStructure<FVector>::Get() 
			|| InPinType.PinSubCategoryObject == TBaseStructure<FTransform>::Get()
			|| InPinType.PinSubCategoryObject == TBaseStructure<FRotator>::Get();
	}
	else
	{
		return true;
	}
}

bool UPCGGraph::UserParametersCanRemoveProperty(FGuid InPropertyID, FName InPropertyName)
{
	// Check if the property has some getters in the graph
	for (const UPCGNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (const UPCGUserParameterGetSettings* Settings = Cast<UPCGUserParameterGetSettings>(Node->GetSettings()))
		{
			if (Settings->PropertyGuid == InPropertyID)
			{
				// We found a getter. Ask the user if he is OK with that
				FText RemoveCheckMessage = FText::Format(LOCTEXT("UserParametersRemoveCheck", "Property {0} is in use in the graph. Are you sure you want to remove it?"), FText::FromName(InPropertyName));
				FSuppressableWarningDialog::FSetupInfo Info(RemoveCheckMessage, LOCTEXT("UserParametersRemoveCheck_Message", "Remove property"), "UserParametersRemove");
				Info.ConfirmText = FCoreTexts::Get().Yes;
				Info.CancelText = FCoreTexts::Get().No;
				FSuppressableWarningDialog AddLevelWarning(Info);
				if (AddLevelWarning.ShowModal() == FSuppressableWarningDialog::Cancel)
				{
					return false;
				}
			}
		}
	}

	return true;
}

#endif // WITH_EDITOR

uint32 UPCGGraph::GetNodeGenerationGridSize(const UPCGNode* InNode, uint32 InDefaultGridSize) const
{
	{
		FReadScopeLock ScopedReadLock(NodeToGridSizeLock);
		if (const uint32* CachedGridSize = NodeToGridSize.Find(InNode))
		{
			return *CachedGridSize;
		}
	}

	{
		FWriteScopeLock ScopedWriteLock(NodeToGridSizeLock);
		return CalculateNodeGridSizeRecursive_Unsafe(InNode, InDefaultGridSize);
	}
}

uint32 UPCGGraph::CalculateNodeGridSizeRecursive_Unsafe(const UPCGNode* InNode, uint32 InDefaultGridSize) const
{
	if (const uint32* CachedGridSize = NodeToGridSize.Find(InNode))
	{
		return *CachedGridSize;
	}

	uint32 GridSize = PCGHiGenGrid::UninitializedGridSize();

	const UPCGHiGenGridSizeSettings* GridSizeSettings = Cast<UPCGHiGenGridSizeSettings>(InNode->GetSettings());
	if (GridSizeSettings && GridSizeSettings->bEnabled)
	{
		GridSize = GridSizeSettings->GetGridSize();
	}
	else
	{
		// Grid size for a node is the minimum of the grid sizes of connected upstream nodes.
		uint32 MinGenerationSize = std::numeric_limits<uint32>::max();
		for (const UPCGPin* Pin : InNode->GetInputPins())
		{
			if (Pin)
			{
				for (const UPCGEdge* Edge : Pin->Edges)
				{
					const UPCGPin* OtherPin = Edge ? Edge->InputPin : nullptr;
					if (OtherPin && OtherPin->Node.Get())
					{
						const uint32 InputGridSize = CalculateNodeGridSizeRecursive_Unsafe(OtherPin->Node, InDefaultGridSize);
						if (PCGHiGenGrid::IsValidGridSize(InputGridSize))
						{
							MinGenerationSize = FMath::Min(MinGenerationSize, InputGridSize);
						}
					}
				}
			}
		}

		GridSize = (MinGenerationSize == std::numeric_limits<uint32>::max()) ? InDefaultGridSize : MinGenerationSize;
	}

	if (GridSize != PCGHiGenGrid::UninitializedGridSize())
	{
		NodeToGridSize.Add(InNode, GridSize);
	}

	return GridSize;
}

void UPCGGraph::AddUserParameters(const TArray<FPropertyBagPropertyDesc>& InDescs, const UPCGGraph* InOptionalOriginalGraph)
{
	UserParameters.AddProperties(InDescs);
	if (InOptionalOriginalGraph)
	{
		if (const FInstancedPropertyBag* OriginalPropertyBag = InOptionalOriginalGraph->GetUserParametersStruct())
		{
			UserParameters.CopyMatchingValuesByID(*OriginalPropertyBag);
		}
	}

#if WITH_EDITOR
	OnGraphParametersChanged(EPCGGraphParameterEvent::MultiplePropertiesAdded, NAME_None);
#endif // WITH_EDITOR
}

/****************************
* UPCGGraphInstance
****************************/

void UPCGGraphInstance::PostLoad()
{
	Super::PostLoad();

	if (Graph)
	{
		Graph->ConditionalPostLoad();
	}

	RefreshParameters(EPCGGraphParameterEvent::GraphPostLoad);

#if WITH_EDITOR
	SetupCallbacks();
#endif // WITH_EDITOR
}

void UPCGGraphInstance::BeginDestroy()
{
#if WITH_EDITOR
	TeardownCallbacks();
#endif // WITH_EDITOR

	Super::BeginDestroy();
}

void UPCGGraphInstance::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

#if WITH_EDITOR
	SetupCallbacks();
#endif // WITH_EDITOR
}

void UPCGGraphInstance::PostEditImport()
{
	Super::PostEditImport();

#if WITH_EDITOR
	SetupCallbacks();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UPCGGraphInstance::PreEditChange(FProperty* InProperty)
{
	Super::PreEditChange(InProperty);

	if (!InProperty)
	{
		return;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGraphInstance, Graph))
	{
		TeardownCallbacks();
	}
}

void UPCGGraphInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGGraphInstance, Graph))
	{
		SetupCallbacks();

		RefreshParameters(EPCGGraphParameterEvent::GraphChanged);
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerStruct() == ParametersOverrides.Parameters.GetPropertyBagStruct())
	{
		OnGraphParametersChanged(this, EPCGGraphParameterEvent::ValueModifiedLocally, PropertyChangedEvent.GetMemberPropertyName());
	}
}

void UPCGGraphInstance::PreEditUndo()
{
	Super::PreEditUndo();

	TeardownCallbacks();

	UndoRedoGraphCache = Graph;
}

void UPCGGraphInstance::PostEditUndo()
{
	Super::PostEditUndo();

	SetupCallbacks();

	// Since we don't know what happened, we need to notify any changes
	NotifyGraphParametersChanged(EPCGGraphParameterEvent::GraphChanged, NAME_None);
}

void UPCGGraphInstance::OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph == Graph)
	{
		OnGraphChangedDelegate.Broadcast(this, ChangeType);
	}
}

void UPCGGraphInstance::OnGraphGridSizesChanged(UPCGGraphInterface* InGraph)
{
	if (InGraph == Graph)
	{
		OnGraphGridSizesChangedDelegate.Broadcast(this);
	}
}

bool UPCGGraphInstance::CanEditChange(const FProperty* InProperty) const
{
	// Graph can only be changed if it is in a PCGComponent (not local) or a PCGSubgraphSettings
	if (InProperty && InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGraphInstance, Graph))
	{
		UObject* Outer = this->GetOuter();

		if (UPCGComponent* Component = Cast<UPCGComponent>(Outer))
		{
			return !Component->IsLocalComponent();
		}
		else
		{
			return Outer && Outer->IsA<UPCGSubgraphSettings>();
		}
	}

	return true;
}

void UPCGGraphInstance::TeardownCallbacks()
{
	if (Graph)
	{
		Graph->OnGraphChangedDelegate.RemoveAll(this);
		Graph->OnGraphGridSizesChangedDelegate.RemoveAll(this);
		Graph->OnGraphParametersChangedDelegate.RemoveAll(this);
	}
}

void UPCGGraphInstance::SetupCallbacks()
{
	if (Graph && !Graph->OnGraphChangedDelegate.IsBoundToObject(this))
	{
		Graph->OnGraphChangedDelegate.AddUObject(this, &UPCGGraphInstance::OnGraphChanged);
		Graph->OnGraphGridSizesChangedDelegate.AddUObject(this, &UPCGGraphInstance::OnGraphGridSizesChanged);
		Graph->OnGraphParametersChangedDelegate.AddUObject(this, &UPCGGraphInstance::OnGraphParametersChanged);
	}
}
#endif

void UPCGGraphInstance::SetGraph(UPCGGraphInterface* InGraph)
{
	if (InGraph == this)
	{
		UE_LOG(LogPCG, Error, TEXT("Try to set the graph of a graph instance to itself, would cause infinite recursion."));
		return;
	}

	if (InGraph == Graph)
	{
		// Nothing to do
		return;
	}

#if WITH_EDITOR
	TeardownCallbacks();
#endif // WITH_EDITOR

	Graph = InGraph;

#if WITH_EDITOR
	SetupCallbacks();
#endif // WITH_EDITOR

#if WITH_EDITOR
	OnGraphParametersChanged(Graph, EPCGGraphParameterEvent::GraphChanged, NAME_None);
#else
	// TODO: We need to revisit this, because it won't update any child graph that has this instance as their graph.
	// Making the hotswap of graph instances within graph instances not working as intended in non-editor builds.
	// Perhaps that should not be possible? At least it is mitigated in the GetUserParameter node, that will take the first valid layout.
	RefreshParameters(EPCGGraphParameterEvent::GraphChanged, NAME_None);
#endif // WITH_EDITOR
}

TObjectPtr<UPCGGraphInterface> UPCGGraphInstance::CreateInstance(UObject* InOwner, UPCGGraphInterface* InGraph)
{
	if (!InOwner || !InGraph)
	{
		return nullptr;
	}

	TObjectPtr<UPCGGraphInstance> GraphInstance = NewObject<UPCGGraphInstance>(InOwner, MakeUniqueObjectName(InOwner, UPCGGraphInstance::StaticClass(), InGraph->GetFName()), RF_Transactional | RF_Public);
	GraphInstance->SetGraph(InGraph);

	return GraphInstance;
}

#if WITH_EDITOR
void UPCGGraphInstance::NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	OnGraphParametersChangedDelegate.Broadcast(this, InChangeType, InChangedPropertyName);

	// Also propagates the changes
	OnGraphChanged(Graph, EPCGChangeType::Settings);
}

void UPCGGraphInstance::OnGraphParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	if (InGraph != Graph && InGraph != this)
	{
		return;
	}

	EPCGGraphParameterEvent ChangeType = InChangeType;
	if (InGraph == Graph && InChangeType == EPCGGraphParameterEvent::ValueModifiedLocally)
	{
		// If we receive a "ValueModifiedLocally" and it was on our Graph, we transform it to "ValueModifiedByParent"
		ChangeType = EPCGGraphParameterEvent::ValueModifiedByParent;
	}

	RefreshParameters(ChangeType, InChangedPropertyName);
	NotifyGraphParametersChanged(ChangeType, InChangedPropertyName);
}
#endif // WITH_EDITOR

void UPCGGraphInstance::RefreshParameters(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	if (!Graph)
	{
		if (ParametersOverrides.IsValid())
		{
			Modify();
		}

		ParametersOverrides.Reset();
	}
	else
	{		
		const FInstancedPropertyBag* ParentUserParameters = Graph->GetUserParametersStruct();

		// Refresh can modify nothing, but we still need to keep a snapshot of this object state, if it ever change.
		// Don't mark it dirty by default, only if something changed.
		Modify(/*bAlwaysMarkDirty=*/false);

		if (ParametersOverrides.RefreshParameters(ParentUserParameters, InChangeType, InChangedPropertyName))
		{
			MarkPackageDirty();
		}
	}
}

void UPCGGraphInstance::UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden)
{
	if (!Graph || !InProperty)
	{
		return;
	}

	Modify();

	const FInstancedPropertyBag* ParentUserParameters = Graph->GetUserParametersStruct();
	if (ParametersOverrides.UpdatePropertyOverride(InProperty, bMarkAsOverridden, ParentUserParameters))
	{
#if WITH_EDITOR
		// If it is true, it means that the value has changed, so propagate the changes, in Editor
		NotifyGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, InProperty->GetFName());
#endif // WITH_EDITOR
	}
}

void UPCGGraphInstance::CopyParameterOverrides(UPCGGraphInterface* InGraph)
{
	if (!InGraph)
	{
		return;
	}

	const UPCGGraph* ThisGraph = GetGraph();
	const UPCGGraph* OtherGraph = InGraph->GetGraph();

	// Can't copy if they have not the same base graph
	if (ThisGraph != OtherGraph)
	{
		return;
	}

	ParametersOverrides.Parameters.CopyMatchingValuesByID(*InGraph->GetUserParametersStruct());
}

void UPCGGraphInstance::ResetPropertyToDefault(const FProperty* InProperty)
{
	if (!IsPropertyOverridden(InProperty))
	{
		return;
	}

	Modify();

	ParametersOverrides.ResetPropertyToDefault(InProperty, Graph->GetUserParametersStruct());

#if WITH_EDITOR
	NotifyGraphParametersChanged(EPCGGraphParameterEvent::ValueModifiedLocally, InProperty->GetFName());
#endif // WITH_EDITOR
}

bool UPCGGraphInstance::IsPropertyOverriddenAndNotDefault(const FProperty* InProperty) const
{
	return Graph ? ParametersOverrides.IsPropertyOverriddenAndNotDefault(InProperty, Graph->GetUserParametersStruct()) : false;
}

bool FPCGOverrideInstancedPropertyBag::RefreshParameters(const FInstancedPropertyBag* ParentUserParameters, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName)
{
	check(ParentUserParameters);

	bool bWasModified = false;

	if (!ParentUserParameters->IsValid())
	{
		Reset();
		return true;
	}

	switch (InChangeType)
	{
	case EPCGGraphParameterEvent::GraphChanged:
	{
		// We should always copy the parents parameters and reset overrides when the graph changes. Even if it is the same struct, values might be different.
		bWasModified = true;
		// Copy the parent parameters and reset overriddes
		Parameters = *ParentUserParameters;
		PropertiesIDsOverridden.Reset();
#if WITH_EDITOR
		// Notify the UI to force refresh
		PCGDelegates::OnInstancedPropertyBagLayoutChanged.Broadcast(Parameters);
#endif // WITH_EDITOR
		break;
	}
	case EPCGGraphParameterEvent::Added:
	case EPCGGraphParameterEvent::Removed:
	case EPCGGraphParameterEvent::PropertyModified:
	{
		bWasModified = true;
		const FPropertyBagPropertyDesc* ThisPropertyDesc = Parameters.FindPropertyDescByName(InChangedPropertyName);

		if (ThisPropertyDesc)
		{
			UpdatePropertyOverride(ThisPropertyDesc->CachedProperty, false, ParentUserParameters);
		}

		MigrateToNewBagInstance(*ParentUserParameters);
#if WITH_EDITOR
		// Notify the UI to force refresh
		PCGDelegates::OnInstancedPropertyBagLayoutChanged.Broadcast(Parameters);
#endif // WITH_EDITOR
		break;
	}
	case EPCGGraphParameterEvent::ValueModifiedByParent:
	{
		const FPropertyBagPropertyDesc* OriginalPropertyDesc = ParentUserParameters->FindPropertyDescByName(InChangedPropertyName);
		const FPropertyBagPropertyDesc* ThisPropertyDesc = Parameters.FindPropertyDescByName(InChangedPropertyName);

		check(InChangedPropertyName != NAME_None);
		check(OriginalPropertyDesc);

		if (!PCGGraphUtils::ArePropertiesCompatible(OriginalPropertyDesc, ThisPropertyDesc))
		{
			bWasModified = true;
			MigrateToNewBagInstance(*ParentUserParameters);
		}
		else if (!IsPropertyOverridden(ThisPropertyDesc->CachedProperty))
		{
			// Only update the value if the property is not overriden.
			bWasModified = true;
			PCGGraphUtils::CopyPropertyValue(OriginalPropertyDesc, *ParentUserParameters, ThisPropertyDesc, Parameters);
		}
		break;
	}
	case EPCGGraphParameterEvent::ValueModifiedLocally:
	{
		const FPropertyBagPropertyDesc* OriginalPropertyDesc = ParentUserParameters->FindPropertyDescByName(InChangedPropertyName);
		const FPropertyBagPropertyDesc* ThisPropertyDesc = Parameters.FindPropertyDescByName(InChangedPropertyName);

		check(InChangedPropertyName != NAME_None);
		check(OriginalPropertyDesc);

		if (!PCGGraphUtils::ArePropertiesCompatible(OriginalPropertyDesc, ThisPropertyDesc))
		{
			bWasModified = true;
			MigrateToNewBagInstance(*ParentUserParameters);
		}
		else
		{
			// Force the value to be overridden, if it is not equal to the value and it was changed from the outside
			if (!PCGGraphUtils::ArePropertiesIdentical(OriginalPropertyDesc, *ParentUserParameters, ThisPropertyDesc, Parameters))
			{
				bWasModified = true;
				UpdatePropertyOverride(ThisPropertyDesc->CachedProperty, true, ParentUserParameters);
			}
		}
		break;
	}
	// Do the same thing in case of post load and multiple properties added.
	// We have 2 enums to avoid puzzling someone that wonders why we would call GraphPostLoad when we add multiple properties.
	case EPCGGraphParameterEvent::GraphPostLoad: // fall-through
	case EPCGGraphParameterEvent::MultiplePropertiesAdded:
	{
		// Check if the property struct mismatch. If so, do the migration
		if (Parameters.GetPropertyBagStruct() != ParentUserParameters->GetPropertyBagStruct())
		{
			bWasModified = true;
			MigrateToNewBagInstance(*ParentUserParameters);
		}

		if (Parameters.GetPropertyBagStruct() == nullptr)
		{
			return bWasModified;
		}

		// And then overwrite all non-overridden values
		for (const FPropertyBagPropertyDesc& ThisPropertyDesc : Parameters.GetPropertyBagStruct()->GetPropertyDescs())
		{
			if (!IsPropertyOverridden(ThisPropertyDesc.CachedProperty))
			{
				const FPropertyBagPropertyDesc* OriginalPropertyDesc = ParentUserParameters->FindPropertyDescByID(ThisPropertyDesc.ID);

				if (!PCGGraphUtils::ArePropertiesIdentical(OriginalPropertyDesc, *ParentUserParameters, &ThisPropertyDesc, Parameters))
				{
					bWasModified = true;
					PCGGraphUtils::CopyPropertyValue(OriginalPropertyDesc, *ParentUserParameters, &ThisPropertyDesc, Parameters);
				}
			}
		}
		break;
	}
	}

	return bWasModified;
}

bool FPCGOverrideInstancedPropertyBag::UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden, const FInstancedPropertyBag* ParentUserParameters)
{
	if (!InProperty)
	{
		return false;
	}

	if (const FPropertyBagPropertyDesc* PropertyDesc = Parameters.FindPropertyDescByName(InProperty->GetFName()))
	{
		if (bMarkAsOverridden)
		{
			PropertiesIDsOverridden.Add(PropertyDesc->ID);
		}
		else
		{
			PropertiesIDsOverridden.Remove(PropertyDesc->ID);
		}
	}

	// Reset the value if it is not marked overridden anymore.
	if (!bMarkAsOverridden)
	{
		ResetPropertyToDefault(InProperty, ParentUserParameters);
		return true;
	}

	return false;
}

void FPCGOverrideInstancedPropertyBag::ResetPropertyToDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters)
{
	check(ParentUserParameters);

	const FPropertyBagPropertyDesc* OriginalPropertyDesc = ParentUserParameters->FindPropertyDescByName(InProperty->GetFName());
	const FPropertyBagPropertyDesc* ThisPropertyDesc = Parameters.FindPropertyDescByName(InProperty->GetFName());

	if (OriginalPropertyDesc && ThisPropertyDesc)
	{
		PCGGraphUtils::CopyPropertyValue(OriginalPropertyDesc, *ParentUserParameters, ThisPropertyDesc, Parameters);
	}
}

bool FPCGOverrideInstancedPropertyBag::IsPropertyOverridden(const FProperty* InProperty) const
{
	if (!InProperty)
	{
		return false;
	}

	const FPropertyBagPropertyDesc* PropertyDesc = Parameters.FindPropertyDescByName(InProperty->GetFName());
	return PropertyDesc && PropertiesIDsOverridden.Contains(PropertyDesc->ID);
}

bool FPCGOverrideInstancedPropertyBag::IsPropertyOverriddenAndNotDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters) const
{
	check(ParentUserParameters);

	const FPropertyBagPropertyDesc* OriginalPropertyDesc = ParentUserParameters->FindPropertyDescByName(InProperty->GetFName());
	const FPropertyBagPropertyDesc* ThisPropertyDesc = Parameters.FindPropertyDescByName(InProperty->GetFName());

	if (OriginalPropertyDesc && ThisPropertyDesc && PropertiesIDsOverridden.Contains(ThisPropertyDesc->ID))
	{
		return !PCGGraphUtils::ArePropertiesIdentical(OriginalPropertyDesc, *ParentUserParameters, ThisPropertyDesc, Parameters);
	}
	else
	{
		return false;
	}
}

void FPCGOverrideInstancedPropertyBag::Reset()
{
	Parameters.Reset();
	PropertiesIDsOverridden.Reset();
}

void FPCGOverrideInstancedPropertyBag::MigrateToNewBagInstance(const FInstancedPropertyBag& NewBagInstance)
{
	// Keeping a map between id and types. We will remove override for property that changed types.
	TMap<FGuid, FPropertyBagPropertyDesc> IdToDescMap;
	if (Parameters.GetPropertyBagStruct())
	{
		for (const FPropertyBagPropertyDesc& PropertyDesc : Parameters.GetPropertyBagStruct()->GetPropertyDescs())
		{
			IdToDescMap.Emplace(PropertyDesc.ID, PropertyDesc);
		}
	}

	Parameters.MigrateToNewBagInstance(NewBagInstance);

	if (NewBagInstance.GetPropertyBagStruct() == nullptr)
	{
		return;
	}

	// Remove overridden parameters that are not in the bag anymore, or have changed type
	TArray<FGuid> OverriddenParametersCopy = PropertiesIDsOverridden.Array();
	for (const FGuid PropertyId: OverriddenParametersCopy)
	{
		const FPropertyBagPropertyDesc* NewPropertyDesc = NewBagInstance.FindPropertyDescByID(PropertyId);
		const FPropertyBagPropertyDesc* OldPropertyDesc = IdToDescMap.Find(PropertyId);

		const bool bTypeHasChanged = NewPropertyDesc && OldPropertyDesc && (NewPropertyDesc->ValueType != OldPropertyDesc->ValueType || NewPropertyDesc->ValueTypeObject != OldPropertyDesc->ValueTypeObject);

		if (!NewPropertyDesc || bTypeHasChanged)
		{
			PropertiesIDsOverridden.Remove(PropertyId);
			continue;
		}
	}
}

#undef LOCTEXT_NAMESPACE