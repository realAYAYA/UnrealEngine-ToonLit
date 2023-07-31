// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPatchedUniverse.h"

#include "DMXEditor.h"
#include "DMXEditorUtils.h"
#include "DMXFixturePatchEditorDefinitions.h"
#include "DMXFixturePatchNode.h"
#include "DMXFixturePatchSharedData.h"
#include "SDMXChannelConnector.h"
#include "SDMXFixturePatchFragment.h"
#include "DragDrop/DMXEntityFixturePatchDragDropOp.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SGridPanel.h"


#define LOCTEXT_NAMESPACE "SDMXPatchedUniverse"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDMXPatchedUniverse::Construct(const FArguments& InArgs)
{
	check(InArgs._UniverseID != INDEX_NONE);

	UniverseID = InArgs._UniverseID;

	DMXEditorPtr = InArgs._DMXEditor;
	OnDragEnterChannel = InArgs._OnDragEnterChannel;
	OnDragLeaveChannel = InArgs._OnDragLeaveChannel;
	OnDropOntoChannel = InArgs._OnDropOntoChannel;

	const TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin();
	if (!DMXEditor.IsValid())
	{
		return;
	}
	SharedData = DMXEditor->GetFixturePatchSharedData();
	check(SharedData.IsValid());

	ChildSlot
	[
		SNew (SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(UniverseName, SBorder)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 0.3f))
				.ToolTipText(FText::Format(LOCTEXT("UniverseListCategoryTooltip", "Patches assigned to Universe {0}"), UniverseID))
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Text(this, &SDMXPatchedUniverse::GetHeaderText)
				]
			]

			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.Node.DevelopmentBanner"))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Visibility(this, &SDMXPatchedUniverse::GetPatchedUniverseReachabilityBannerVisibility)
			]
		]
			
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)		
		[					
			SNew(SBox)
			.HAlign(HAlign_Center)
			.MinDesiredWidth(920.0f) // Avoids a slate issue, where SScaleBox ignores border size hence scales			
			.Padding(FMargin(4.0f, 4.0f, 4.0f, 8.0f))
			[
				SAssignNew(Grid, SGridPanel)												
			]
		]
	];

	CreateChannelConnectors();

	SetUniverseIDInternal(UniverseID);

	UpdateZOrderOfNodes();
	UpdatePatchedUniverseReachability();

	RegisterCommands();

	// Handle external changes
	UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddSP(this, &SDMXPatchedUniverse::OnFixturePatchChanged);
	SharedData->OnFixturePatchSelectionChanged.AddSP(this, &SDMXPatchedUniverse::OnSelectionChanged);
	FDMXPortManager::Get().OnPortsChanged.AddSP(this, &SDMXPatchedUniverse::UpdatePatchedUniverseReachability);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMXPatchedUniverse::RequestRefresh()
{
	if (!RequestRefreshTimerHandle.IsValid())
	{
		RequestRefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXPatchedUniverse::RefreshInternal));
	}
}

void SDMXPatchedUniverse::CreateChannelConnectors()
{
	check(Grid.IsValid());

	for (int32 ChannelIndex = 0; ChannelIndex < DMX_UNIVERSE_SIZE; ++ChannelIndex)
	{
		int32 Column = ChannelIndex % FDMXChannelGridSpecs::NumColumns;
		int32 Row = ChannelIndex / FDMXChannelGridSpecs::NumColumns;
		int32 Channel = ChannelIndex + FDMXChannelGridSpecs::ChannelIDOffset;

		TSharedRef<SDMXChannelConnector> ChannelPatchWidget =
			SNew(SDMXChannelConnector)
			.ChannelID(Channel)
			.Value(0)
			.OnMouseButtonDownOnChannel(this, &SDMXPatchedUniverse::HandleOnMouseButtonDownOnChannel)
			.OnMouseButtonUpOnChannel(this, &SDMXPatchedUniverse::HandleOnMouseButtonUpOnChannel)
			.OnDragDetectedOnChannel(this, &SDMXPatchedUniverse::HandleOnDragDetectedOnChannel)
			.OnDragEnterChannel(this, &SDMXPatchedUniverse::HandleDragEnterChannel)
			.OnDragLeaveChannel(this, &SDMXPatchedUniverse::HandleDragLeaveChannel)
			.OnDropOntoChannel(this, &SDMXPatchedUniverse::HandleDropOntoChannel)
			.DMXEditor(DMXEditorPtr);

		ChannelConnectors.Add(ChannelPatchWidget);

		Grid->AddSlot(Column, Row)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[			
				ChannelPatchWidget
			];
	}
}

bool SDMXPatchedUniverse::Patch(const TSharedPtr<FDMXFixturePatchNode>& Node, int32 NewStartingChannel, bool bCreateTransaction)
{
	check(Node.IsValid());

	UDMXEntityFixturePatch* FixturePatch = Node->GetFixturePatch().Get();
	if (!FixturePatch)
	{
		return false;
	}

	PatchedNodes.AddUnique(Node);

	const int32 NewChannelSpan = FixturePatch->GetChannelSpan();
	Node->SetAddresses(UniverseID, NewStartingChannel, NewChannelSpan, bCreateTransaction);
	RequestRefresh();

	return true;
}

bool SDMXPatchedUniverse::CanAssignFixturePatch(TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch) const
{
	if (FixturePatch.IsValid())
	{
		return CanAssignFixturePatch(FixturePatch, FixturePatch->GetStartingChannel());
	}

	return false;
}

bool SDMXPatchedUniverse::CanAssignFixturePatch(TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch, int32 StartingChannel) const
{
	// Test for a valid and patch that has an active mode
	FText InvalidReason;
	if (!FixturePatch.IsValid() ||
		!FixturePatch->IsValidEntity(InvalidReason) ||
		!FixturePatch->GetActiveMode())
	{
		return false;
	}

	int32 ChannelSpan = FixturePatch->GetChannelSpan();
	if (ChannelSpan == 0 || ChannelSpan > DMX_MAX_ADDRESS)
	{
		// Cannot patch a patch with 0 channel span or exceeding DMX_MAX_ADDRESS
		return false;
	}

	// Only fully valid channels are supported
	check(StartingChannel > 0);
	check(StartingChannel + ChannelSpan - 1 <= DMX_UNIVERSE_SIZE);

	// Test for overlapping nodes in this universe
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : PatchedNodes)
	{
		if (Node->GetFixturePatch() == FixturePatch)
		{
			continue;
		}

		if (Node->OccupiesChannels(StartingChannel, ChannelSpan))
		{
			return false;
		}
	}

	return true;
}

bool SDMXPatchedUniverse::CanAssignNode(const TSharedPtr<FDMXFixturePatchNode>& TestedNode, int32 StartingChannel) const
{
	check(TestedNode.IsValid());

	UDMXEntityFixturePatch* FixturePatch = TestedNode->GetFixturePatch().Get();

	return CanAssignFixturePatch(FixturePatch, StartingChannel);
}

TSharedPtr<FDMXFixturePatchNode> SDMXPatchedUniverse::FindPatchNode(TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch) const
{
	const TSharedPtr<FDMXFixturePatchNode>* NodePtr = PatchedNodes.FindByPredicate([&FixturePatch](const TSharedPtr<FDMXFixturePatchNode>& Node)
		{
			if (Node->GetFixturePatch() == FixturePatch)
			{
				return true;
			}
			return false;
		});

	if (NodePtr)
	{
		return *NodePtr;
	}

	return nullptr;
}

TSharedPtr<FDMXFixturePatchNode> SDMXPatchedUniverse::FindPatchNodeOfType(UDMXEntityFixtureType* Type, const TSharedPtr<FDMXFixturePatchNode>& IgoredNode) const
{	
	if (!Type)
	{
		return nullptr;
	}

	for (const TSharedPtr<FDMXFixturePatchNode>& PatchNode : PatchedNodes)
	{
		if (PatchNode == IgoredNode)
		{
			continue;
		}

		if (PatchNode->GetFixturePatch().IsValid() &&
			PatchNode->GetFixturePatch()->GetFixtureType() == Type)
		{
			return PatchNode;
		}
	}

	return nullptr;
}

void SDMXPatchedUniverse::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) 
{
	TArray<TSharedPtr<FDMXFixturePatchNode>> RemovedNodes;
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : PatchedNodes)
	{
		if (Node->GetUniverseID() != UniverseID)
		{
			RemovedNodes.Add(Node);
		}
		else if (Node->NeedsUpdateGrid())
		{
			RequestRefresh();
			break;
		}
	}

	for (const TSharedPtr<FDMXFixturePatchNode>& RemovedNode : RemovedNodes)
	{
		Unpatch(RemovedNode);
	}
}

void SDMXPatchedUniverse::Unpatch(const TSharedPtr<FDMXFixturePatchNode>& Node)
{
	check(Node.IsValid());
	check(Grid.IsValid());

	if (PatchedNodes.Contains(Node))
	{
		PatchedNodes.RemoveSingle(Node);

		for (const TSharedPtr<SDMXFixturePatchFragment>& Widget : Node->GetGeneratedWidgets())
		{
			if (Widget.IsValid())
			{
				Grid->RemoveSlot(Widget.ToSharedRef());
			}
		}
		RequestRefresh();
	}
}

void SDMXPatchedUniverse::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	if (!FixturePatch || FixturePatch->GetParentLibrary() != GetDMXLibrary())
	{
		return;
	}

	const int32 UniverseIDOfFixturePatch = FixturePatch->GetUniverseID();
	if (UniverseIDOfFixturePatch == UniverseID)
	{
		const TSharedPtr<FDMXFixturePatchNode>* NodePtr = PatchedNodes.FindByPredicate([FixturePatch](const TSharedPtr<FDMXFixturePatchNode>& Node)
			{
				return Node->GetFixturePatch() == FixturePatch;
			});

		// Redraw all if the patch cannot be found.
		if (!NodePtr)
		{
			SetUniverseIDInternal(UniverseID);
		}
	}
}

void SDMXPatchedUniverse::SetUniverseIDInternal(int32 NewUniverseID)
{
	check(NewUniverseID > -1);

	// Find patches in new universe
	UDMXLibrary* Library = GetDMXLibrary();
	if (Library)
	{
		// Unpatch all nodes
		TArray<TSharedPtr<FDMXFixturePatchNode>> CachedPatchedNodes = PatchedNodes;
		for (const TSharedPtr<FDMXFixturePatchNode>& Node : CachedPatchedNodes)
		{
			Unpatch(Node);
		}
		check(PatchedNodes.Num() == 0);

		// Update what to draw
		UniverseID = NewUniverseID;

		UpdatePatchedUniverseReachability();

		TArray<UDMXEntityFixturePatch*> PatchesInUniverse;
		Library->ForEachEntityOfType<UDMXEntityFixturePatch>([&](UDMXEntityFixturePatch* Patch)
			{
				const bool bResidesInThisUniverse = Patch->GetUniverseID() == UniverseID;
				const bool bHasValidChannelSpan = Patch->GetChannelSpan() > 0 && Patch->GetChannelSpan() < DMX_MAX_ADDRESS;
				if (bResidesInThisUniverse && bHasValidChannelSpan)
				{
					PatchesInUniverse.Add(Patch);
				}
			});

		PatchesInUniverse.Sort([](const UDMXEntityFixturePatch& FixturePatchA, const UDMXEntityFixturePatch& FixturePatchB)
			{
				return FixturePatchA.GetEndingChannel() <= FixturePatchB.GetEndingChannel();
			});

		// Add fixture patches to the grid
		for (UDMXEntityFixturePatch* FixturePatch : PatchesInUniverse)
		{
			TSharedPtr<FDMXFixturePatchNode> Node = FindPatchNode(FixturePatch);
			if (!Node.IsValid())
			{
				Node = FDMXFixturePatchNode::Create(DMXEditorPtr, FixturePatch);
			}
			check(Node.IsValid());

			Patch(Node, FixturePatch->GetStartingChannel(), false);
		}

		// Update the channel connectors' Universe ID
		for (const TSharedPtr<SDMXChannelConnector>& Connector : ChannelConnectors)
		{
			Connector->SetUniverseID(NewUniverseID);
		}

		RequestRefresh();
	}
}

void SDMXPatchedUniverse::RefreshInternal()
{
	RequestRefreshTimerHandle.Invalidate();

	UpdateZOrderOfNodes();

	// Remove old widgets from the grid
	for (const TSharedPtr<SDMXFixturePatchFragment>& OldWidget : FixturePatchWidgets)
	{
		Grid->RemoveSlot(OldWidget.ToSharedRef());
	}
	FixturePatchWidgets.Reset();

	// Create Groups of Nodes with same Universe/Address
	TMap<int32, TArray<TSharedPtr<FDMXFixturePatchNode>>> AddressToNodeGroupMap;
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : PatchedNodes)
	{
		if (Node->GetChannelSpan() == 0 || Node->GetChannelSpan() > DMX_MAX_ADDRESS)
		{
			// Ignore nodes without or with excess channelspan, they're not displayed in UI either
			continue;
		}

		const int32 Address = Node->GetStartingChannel();
		AddressToNodeGroupMap.FindOrAdd(Address).Add(Node);
	}

	// Generate new widgets
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : PatchedNodes)
	{
		if (const TArray<TSharedPtr<FDMXFixturePatchNode>>* const NodeGroup = AddressToNodeGroupMap.Find(Node->GetStartingChannel()))
		{
			FixturePatchWidgets.Append(Node->GenerateWidgets(*NodeGroup));
		}
	}

	// Add widgets to the grid
	for (const TSharedPtr<SDMXFixturePatchFragment>& NewWidget : FixturePatchWidgets)
	{
		Grid->AddSlot(NewWidget->GetColumn(), NewWidget->GetRow())
			.ColumnSpan(NewWidget->GetColumnSpan())
			.Layer(1)
			[
				NewWidget.ToSharedRef()
			];
	}
}

FReply SDMXPatchedUniverse::HandleOnMouseButtonDownOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	if (PointerEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> OldSelection = SharedData->GetSelectedFixturePatches();
		const TArray<UDMXEntityFixturePatch*> FixturePatchesOnChannel = GetFixturePatchesOnChannel(Channel);
		const bool bOldSelectionContainsPatchesOnChannel = FixturePatchesOnChannel.ContainsByPredicate([&OldSelection](const UDMXEntityFixturePatch* FixturePatch)
			{
				return OldSelection.Contains(FixturePatch);
			});
		if (!bOldSelectionContainsPatchesOnChannel)
		{
			if (UDMXEntityFixturePatch* Patch = GetTopmostFixturePatchOnChannel(Channel))
			{
				if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
				{
					SharedData->AddFixturePatchToSelection(Patch);
				}
				else
				{
					SharedData->SelectFixturePatch(Patch);
				}
			}
		}
	}

	if (PointerEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UDMXEntityFixturePatch* Patch = GetTopmostFixturePatchOnChannel(Channel))
		{
			if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
			{
				SharedData->AddFixturePatchToSelection(Patch);
			}
			else
			{
				SharedData->SelectFixturePatch(Patch);
			}

			if (ensureMsgf(ChannelConnectors.IsValidIndex(Channel - 1), TEXT("Trying to drag Fixture Patch, but the dragged channel is not valid.")))
			{
				return FReply::Handled().DetectDrag(ChannelConnectors[Channel - 1].ToSharedRef(), EKeys::LeftMouseButton);
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPatchedUniverse::HandleOnMouseButtonUpOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	if (PointerEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FVector2D& SummonLocation = PointerEvent.GetScreenSpacePosition();

		TSharedRef<SWidget> MenuContent = CreateContextMenu(Channel);
		FWidgetPath WidgetPath = PointerEvent.GetEventPath() ? *PointerEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent, SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXPatchedUniverse::HandleOnDragDetectedOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	if (UDMXEntityFixturePatch* Patch = GetTopmostFixturePatchOnChannel(Channel))
	{
		int32 ChannelOffset = Channel - Patch->GetStartingChannel();

		TSharedRef<FDMXEntityFixturePatchDragDropOperation> DragDropOp = MakeShared<FDMXEntityFixturePatchDragDropOperation>(Patch->GetParentLibrary(), TArray<TWeakObjectPtr<UDMXEntity>>{ Patch }, ChannelOffset);

		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

	return FReply::Unhandled();
}

void SDMXPatchedUniverse::HandleDragEnterChannel(uint32 Channel, const FDragDropEvent& DragDropEvent)
{
	OnDragEnterChannel.ExecuteIfBound(UniverseID, Channel, DragDropEvent);
}

void SDMXPatchedUniverse::HandleDragLeaveChannel(uint32 Channel, const FDragDropEvent& DragDropEvent)
{
	OnDragLeaveChannel.ExecuteIfBound(UniverseID, Channel, DragDropEvent);
}

FReply SDMXPatchedUniverse::HandleDropOntoChannel(uint32 Channel, const FDragDropEvent& DragDropEvent)
{
	return OnDropOntoChannel.Execute(UniverseID, Channel, DragDropEvent);
}

FText SDMXPatchedUniverse::GetHeaderText() const
{
	switch (PatchedUniverseReachability)
	{
		case EDMXPatchedUniverseReachability::Reachable:
			return FText::Format(LOCTEXT("UniverseIDLabel", "Universe {0}"), UniverseID);

		case EDMXPatchedUniverseReachability::UnreachableForInputPorts:
			return FText::Format(LOCTEXT("UnreachableForInputPorts", "Universe {0} - Unreachable by Input Ports"), UniverseID);

		case EDMXPatchedUniverseReachability::UnreachableForOutputPorts:
			return FText::Format(LOCTEXT("UnreachableForOutputPorts", "Universe {0} - Unreachable by Output Ports"), UniverseID);

		case EDMXPatchedUniverseReachability::UnreachableForInputAndOutputPorts:
			return FText::Format(LOCTEXT("UnreachableForInputAndOutputPorts", "Universe {0} - Unreachable by Input and Output Ports"), UniverseID);

		default:
			// Unhandled enum value
			checkNoEntry();
	}

	return FText::GetEmpty();
}

void SDMXPatchedUniverse::UpdateZOrderOfNodes()
{
	if (PatchedNodes.Num() == 0)
	{
		return;
	}

	TArray<TSharedPtr<FDMXFixturePatchNode>> Nodes = PatchedNodes;
	// Sort by ZOrder descending
	Nodes.StableSort([](const TSharedPtr<FDMXFixturePatchNode>& NodeA, const TSharedPtr<FDMXFixturePatchNode>& NodeB)
		{
			if (NodeA->GetStartingChannel() > NodeB->GetStartingChannel())
			{
				return true;
			}
			else if (NodeA->GetStartingChannel() == NodeB->GetStartingChannel())
			{
				const bool bNodeASelected = NodeA->IsSelected();
				if (bNodeASelected)
				{
					return
						!NodeB->IsSelected() ||
						NodeA->GetZOrder() > NodeB->GetZOrder();
				}
				else
				{
					return
						!NodeB->IsSelected() &&
						NodeA->GetZOrder() > NodeB->GetZOrder();
				}
			}
			return false;
		});

	int32 ZOrder = Nodes.Num();
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : Nodes)
	{
		if (Node.IsValid())
		{
			Node->SetZOrder(ZOrder);
			--ZOrder;
		}
	}
}

EVisibility SDMXPatchedUniverse::GetPatchedUniverseReachabilityBannerVisibility() const
{
	if (PatchedUniverseReachability == EDMXPatchedUniverseReachability::Reachable)
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}

void SDMXPatchedUniverse::UpdatePatchedUniverseReachability()
{
	PatchedUniverseReachability = EDMXPatchedUniverseReachability::Reachable;

	UDMXLibrary* Library = GetDMXLibrary();
	if (Library)
	{
		bool bReachableForAnyInput = false;
		for (const FDMXInputPortSharedRef& InputPort : Library->GetInputPorts())
		{
			if (InputPort->IsLocalUniverseInPortRange(UniverseID))
			{
				bReachableForAnyInput = true;
				break;
			}
		}

		bool bReachableForAnyOutput = false;
		for (const FDMXOutputPortSharedRef& OutputPort : Library->GetOutputPorts())
		{
			if (OutputPort->IsLocalUniverseInPortRange(UniverseID))
			{
				bReachableForAnyOutput = true;
				break;
			}
		}

		if (!bReachableForAnyInput && !bReachableForAnyOutput)
		{
			PatchedUniverseReachability = EDMXPatchedUniverseReachability::UnreachableForInputAndOutputPorts;
		}
		else if (!bReachableForAnyOutput)
		{
			PatchedUniverseReachability = EDMXPatchedUniverseReachability::UnreachableForOutputPorts;
		}
		else if (!bReachableForAnyInput)
		{
			PatchedUniverseReachability = EDMXPatchedUniverseReachability::UnreachableForInputPorts;
		}
	}
}

void SDMXPatchedUniverse::OnSelectionChanged()
{
	const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatches = SharedData->GetSelectedFixturePatches();
	const bool bSelectionChanged = [&FixturePatches, this]()
		{
			for (const TSharedPtr<FDMXFixturePatchNode>& Node : PatchedNodes)
			{
				if (Node->IsSelected() ^ FixturePatches.Contains(Node->GetFixturePatch()))
				{
					return true;
					break;
				}
			}
			return false;
		}();
	if (bSelectionChanged)
	{
		RequestRefresh();
	}
}

void SDMXPatchedUniverse::AutoAssignFixturePatches()
{
	if (SharedData.IsValid())
	{
		TArray<UDMXEntityFixturePatch*> FixturePatchesToAutoAssign;
		const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = SharedData->GetSelectedFixturePatches();
		for (TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch : SelectedFixturePatches)
		{
			if (FixturePatch.IsValid())
			{
				FixturePatchesToAutoAssign.Add(FixturePatch.Get());
			}
		}

		if (FixturePatchesToAutoAssign.IsEmpty())
		{
			return;
		}

		constexpr bool bAllowDecrementUniverse = false;
		constexpr bool bAllowDecrementChannels = true;
		FDMXEditorUtils::AutoAssignedChannels(bAllowDecrementUniverse, bAllowDecrementChannels, FixturePatchesToAutoAssign);
		SharedData->SelectUniverse(FixturePatchesToAutoAssign[0]->GetUniverseID());

		RequestRefresh();
	}
}

UDMXEntityFixturePatch* SDMXPatchedUniverse::GetTopmostFixturePatchOnChannel(uint32 Channel) const
{
	TArray<TSharedPtr<FDMXFixturePatchNode>> Nodes(PatchedNodes);
	Nodes.RemoveAll([Channel](const TSharedPtr<FDMXFixturePatchNode>& Node)
			{
				if (UDMXEntityFixturePatch* FixturePatch = Node->GetFixturePatch().Get())
				{
					const uint32 PatchStartingChannel = FixturePatch->GetStartingChannel();
					const TRange<uint32> FixturePatchRange(PatchStartingChannel, PatchStartingChannel + FixturePatch->GetChannelSpan());

					return !FixturePatchRange.Contains(Channel);
				}
				return true;
			});

	Nodes.Sort([](const TSharedPtr<FDMXFixturePatchNode>& NodeA, const TSharedPtr<FDMXFixturePatchNode>& NodeB)
			{
				return NodeA->GetZOrder() > NodeB->GetZOrder();
			});

	if (Nodes.IsEmpty())
	{
		return nullptr;
	}
	else
	{
		return Nodes[0]->GetFixturePatch().Get();
	}
}

TArray<UDMXEntityFixturePatch*> SDMXPatchedUniverse::GetFixturePatchesOnChannel(uint32 Channel) const
{
	TArray<TSharedPtr<FDMXFixturePatchNode>> Nodes(PatchedNodes);
	Nodes.RemoveAll([Channel](const TSharedPtr<FDMXFixturePatchNode>& Node)
		{
			if (UDMXEntityFixturePatch* FixturePatch = Node->GetFixturePatch().Get())
			{
				const uint32 PatchStartingChannel = FixturePatch->GetStartingChannel();
				const TRange<uint32> FixturePatchRange(PatchStartingChannel, PatchStartingChannel + FixturePatch->GetChannelSpan());

				return !FixturePatchRange.Contains(Channel);
			}
			return true;
		});

	TArray<UDMXEntityFixturePatch*> Result;
	for (const TSharedPtr<FDMXFixturePatchNode>& Node : Nodes)
	{
		if (UDMXEntityFixturePatch* FixturePatch = Node->GetFixturePatch().Get())
		{
			Result.Add(FixturePatch);
		}
	}

	return Result;
}

UDMXLibrary* SDMXPatchedUniverse::GetDMXLibrary() const
{
	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		return DMXEditor->GetDMXLibrary();
	}
	return nullptr;
}

void SDMXPatchedUniverse::RegisterCommands()
{
	if (CommandList.IsValid())
	{
		return;
	}

	// listen to common editor shortcuts for copy/paste etc
	CommandList = MakeShared<FUICommandList>();
}

TSharedRef<SWidget> SDMXPatchedUniverse::CreateContextMenu(int32 Channel)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	if (!DMXLibrary)
	{
		return SNullWidget::NullWidget;
	}

	TArray<UDMXEntityFixturePatch*> FixturePatchesOnChannel = GetFixturePatchesOnChannel(Channel);
	if (FixturePatchesOnChannel.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	constexpr bool bCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bCloseWindowAfterMenuSelection, CommandList);

	// Auto Assign Section
	MenuBuilder.BeginSection("AutoAssignSection", LOCTEXT("AutoAssignSection", "Auto-Assign"));
	{
		const FUIAction Action(FExecuteAction::CreateSP(this, &SDMXPatchedUniverse::AutoAssignFixturePatches));

		const FText AutoAssignText = LOCTEXT("AutoAssignContextMenuEntry", "Auto-Assign Selection");
		const TSharedRef<SWidget> Widget =
			SNew(STextBlock)
			.Text(AutoAssignText);

		MenuBuilder.AddMenuEntry(Action, Widget);
		MenuBuilder.EndSection();
	}

	if (FixturePatchesOnChannel.Num() > 1)
	{
		// Select section
		MenuBuilder.BeginSection("SelectSection", LOCTEXT("SelectSection", "Select"));
		{
			// Select all
			{
				const FUIAction Action(
					FExecuteAction::CreateLambda([FixturePatchesOnChannel, this]()
						{
							if (SharedData.IsValid())
							{
								TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatchesOnChannel;
								for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesOnChannel)
								{
									WeakFixturePatchesOnChannel.Add(FixturePatch);
								}
								SharedData->SelectFixturePatches(WeakFixturePatchesOnChannel);
							}
						}));

				const FText SelectAllText = LOCTEXT("SelectAllFixturePatchesContextMenuEntry", "Select All");
				const TSharedRef<SWidget> Widget =
					SNew(STextBlock)
					.Text(SelectAllText);

				MenuBuilder.AddMenuEntry(Action, Widget);
			}

			MenuBuilder.AddSeparator();

			// Select specific patch
			{
				const TArray<UDMXEntityFixturePatch*> DMXFixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
				FixturePatchesOnChannel.Sort([&DMXFixturePatchesInLibrary](const UDMXEntityFixturePatch& FixturePatchA, const UDMXEntityFixturePatch& FixturePatchB)
					{
						return DMXFixturePatchesInLibrary.IndexOfByKey(&FixturePatchA) < DMXFixturePatchesInLibrary.IndexOfByKey(&FixturePatchB);
					});

				for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatchesOnChannel)
				{
					const FUIAction Action(
						FExecuteAction::CreateLambda([this, WeakFixturePatch]()
							{
								if (SharedData.IsValid() && WeakFixturePatch.IsValid())
								{
									SharedData->SelectFixturePatch(WeakFixturePatch.Get());
								}
							})
					);

					const FText FixturePatchName = FText::FromString(WeakFixturePatch->Name);
					const TSharedRef<SWidget> Widget =
						SNew(STextBlock)
						.Text(FixturePatchName);

					MenuBuilder.AddMenuEntry(Action, Widget);
				}
			}
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
