// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPatchedUniverse.h"

#include "DMXEditor.h"
#include "DMXEditorUtils.h"
#include "DMXFixturePatchEditorDefinitions.h"
#include "DMXFixturePatchNode.h"
#include "DMXFixturePatchSharedData.h"
#include "Framework/Application/SlateApplication.h"
#include "SDMXChannelConnector.h"
#include "SDMXFixturePatchFragment.h"
#include "DragDrop/DMXEntityFixturePatchDragDropOp.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXPortManager.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"

#include "Algo/RemoveIf.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"


#define LOCTEXT_NAMESPACE "SDMXPatchedUniverse"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDMXPatchedUniverse::Construct(const FArguments& InArgs)
{
	check(InArgs._UniverseID != INDEX_NONE);

	SetCanTick(true);

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

	UDMXProtocolSettings* ProtocolSettings = GetMutableDefault<UDMXProtocolSettings>();
	ProtocolSettings->GetOnSetReceiveDMXEnabled().AddSP(this, &SDMXPatchedUniverse::OnReceiveDMXEnabledChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMXPatchedUniverse::RequestRefresh()
{
	if (!RequestRefreshTimerHandle.IsValid())
	{
		RequestRefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXPatchedUniverse::RefreshInternal));
	}
}

void SDMXPatchedUniverse::SetMonitorInputsEnabled(bool bEnabled)
{
	bMonitorInputs = bEnabled;

	if (!bEnabled)
	{
		ResetMonitor();
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

bool SDMXPatchedUniverse::FindOrAdd(const TSharedPtr<FDMXFixturePatchNode>& Node)
{
	if (!Node.IsValid())
	{
		return false;
	}

	UDMXEntityFixturePatch* FixturePatch = Node->GetFixturePatch().Get();
	if (!FixturePatch)
	{
		return false;
	}

	PatchedNodes.AddUnique(Node);

	RequestRefresh();

	return true;
}

void SDMXPatchedUniverse::Remove(const TSharedPtr<FDMXFixturePatchNode>& Node)
{
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
	if (bMonitorInputs)
	{
		UpdateMonitor();
	}
}

void SDMXPatchedUniverse::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	if (!FixturePatch || FixturePatch->GetParentLibrary() != GetDMXLibrary())
	{
		return;
	}

	const TSharedPtr<FDMXFixturePatchNode>* NodePtr = PatchedNodes.FindByPredicate([FixturePatch](const TSharedPtr<FDMXFixturePatchNode>& Node)
		{
			return Node->GetFixturePatch() == FixturePatch;
		});

	if (NodePtr && (*NodePtr)->GetUniverseID() != UniverseID)
	{
		Remove(*NodePtr);
	}
	else if (FixturePatch->GetUniverseID() == UniverseID)
	{
		// Redraw all if the patch is in this Universe but not found
		SetUniverseIDInternal(UniverseID);
	}
}

void SDMXPatchedUniverse::OnReceiveDMXEnabledChanged(bool bNewEnabled)
{
	if (!bNewEnabled)
	{
		ResetMonitor();
	}
}

void SDMXPatchedUniverse::SetUniverseIDInternal(int32 NewUniverseID)
{
	if (NewUniverseID < 0)
	{
		return;
	}

	// Find patches in new universe
	UDMXLibrary* Library = GetDMXLibrary();
	if (Library)
	{
		// Unpatch all nodes
		TArray<TSharedPtr<FDMXFixturePatchNode>> CachedPatchedNodes = PatchedNodes;
		for (const TSharedPtr<FDMXFixturePatchNode>& Node : CachedPatchedNodes)
		{
			Remove(Node);
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

			FindOrAdd(Node);
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

void SDMXPatchedUniverse::UpdateMonitor()
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	const UDMXProtocolSettings* ProtocolSettings = GetDefault<UDMXProtocolSettings>();
	if (!DMXLibrary || !ProtocolSettings || !ProtocolSettings->IsReceiveDMXEnabled())
	{
		return;
	}


	const TSet<FDMXInputPortSharedRef>& InputPorts = DMXLibrary->GetInputPorts();
	for (const FDMXInputPortSharedRef& InputPort : InputPorts)
	{
		FDMXSignalSharedPtr Signal;
		InputPort->GameThreadGetDMXSignal(UniverseID, Signal);
		if (Signal.IsValid())
		{
			for (int32 DataIndex = 0; DataIndex < Signal->ChannelData.Num(); DataIndex++)
			{
				if (ensureMsgf(ChannelConnectors.IsValidIndex(DataIndex), TEXT("Missing Channel to display value of Channel %i"), DataIndex))
				{
					ChannelConnectors[DataIndex]->SetValue(Signal->ChannelData[DataIndex]);
				}
			}
		}
	}
}

void SDMXPatchedUniverse::ResetMonitor()
{
	for (const TSharedPtr<SDMXChannelConnector>& Channel : ChannelConnectors)
	{
		Channel->SetValue(0);
	}
}

FReply SDMXPatchedUniverse::HandleOnMouseButtonDownOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	if (PointerEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Get valid selection, sorted by Universe/Address
		TArray<UDMXEntityFixturePatch*> Selection;
		Algo::TransformIf(SharedData->GetSelectedFixturePatches(), Selection,
			[](TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch) { return WeakFixturePatch.IsValid(); },
			[](TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch) { return WeakFixturePatch.Get(); }
		);
		Algo::SortBy(Selection, [](UDMXEntityFixturePatch* FixturePatch)
			{
				return FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetStartingChannel();
			});

		const TArray<UDMXEntityFixturePatch*> FixturePatchesOnChannel = GetFixturePatchesOnChannel(Channel);
		UDMXEntityFixturePatch* ClickedPatch = GetTopmostFixturePatchOnChannel(Channel);

		// Shift Select
		if (FSlateApplication::Get().GetModifierKeys().IsShiftDown() &&
			!Selection.IsEmpty())
		{
			ShiftSelectAnchorPatch = ShiftSelectAnchorPatch.IsValid() ? ShiftSelectAnchorPatch : Selection[0];
			
			UDMXLibrary* DMXLibrary = ShiftSelectAnchorPatch->GetParentLibrary();
			if (!DMXLibrary)
			{
				return FReply::Handled();
			}
			TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
			const int64 AbsoluteClickedChannel = UniverseID * DMX_MAX_ADDRESS + Channel;
			const int64 ShiftSelectAbsoluteStartingChannel = ShiftSelectAnchorPatch->GetUniverseID() * DMX_MAX_ADDRESS + ShiftSelectAnchorPatch->GetStartingChannel();
			const int64 ShiftSelectAbsoluteEndingChannel = ShiftSelectAnchorPatch->GetUniverseID() * DMX_MAX_ADDRESS + ShiftSelectAnchorPatch->GetEndingChannel();

			const TRange<int64> SelectedRange(FMath::Min(ShiftSelectAbsoluteStartingChannel, AbsoluteClickedChannel), FMath::Max(ShiftSelectAbsoluteStartingChannel, AbsoluteClickedChannel + 1));

			TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> ShiftSelection;
			Algo::TransformIf(FixturePatches, ShiftSelection,
				[&SelectedRange, Channel, this](UDMXEntityFixturePatch* FixturePatch)
				{			
					const int64 PatchAbsoluteStartingChannel = FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetStartingChannel();
					const int64 PatchAbsoluteEndingChannel = FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetEndingChannel();
					const TRange<int64> PatchRange(PatchAbsoluteStartingChannel, PatchAbsoluteEndingChannel + 1);
					return 
						SelectedRange.Overlaps(PatchRange);
				},
				[](UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch;
				}
			);
			SharedData->SelectFixturePatches(ShiftSelection);

			return FReply::Handled();
		}
		ShiftSelectAnchorPatch.Reset();

		// Ctrl-Select
		if (FSlateApplication::Get().GetModifierKeys().IsControlDown() && 
			ClickedPatch)
		{		
			if (const bool bUnselect = Selection.Num() > 1 && Selection.Contains(ClickedPatch))
			{
				TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakSelection = SharedData->GetSelectedFixturePatches();
				WeakSelection.Remove(ClickedPatch);
				SharedData->SelectFixturePatches(WeakSelection);
			}
			else
			{
				SharedData->AddFixturePatchToSelection(ClickedPatch);
			}

			return FReply::Handled();
		}

		// Normal Select and Detect Drag
		if (ClickedPatch &&
			ensureMsgf(ChannelConnectors.IsValidIndex(Channel - 1), TEXT("Trying to drag Fixture Patch, but the dragged channel is not valid.")))
		{
			ShiftSelectAnchorPatch.Reset();

			// Replace selection with clicked patch if it wasn't selected
			if (!Selection.Contains(ClickedPatch))
			{
				SharedData->SelectFixturePatch(ClickedPatch);
			}

			return FReply::Handled().DetectDrag(ChannelConnectors[Channel - 1].ToSharedRef(), EKeys::LeftMouseButton);
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPatchedUniverse::HandleOnMouseButtonUpOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	if (PointerEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UDMXEntityFixturePatch* Patch = GetTopmostFixturePatchOnChannel(Channel);
		if (!Patch)
		{
			return FReply::Unhandled();
		}

		// Normal Select
		if (!FSlateApplication::Get().GetModifierKeys().IsControlDown() && !FSlateApplication::Get().GetModifierKeys().IsShiftDown())
		{
			UDMXEntityFixturePatch* ClickedPatch = GetTopmostFixturePatchOnChannel(Channel);
			if (ClickedPatch)
			{
				SharedData->SelectFixturePatch(ClickedPatch);
			}
		}
	}

	if (PointerEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FVector2D& ContextMenuSummonLocation = PointerEvent.GetScreenSpacePosition();

		TSharedRef<SWidget> MenuContent = CreateContextMenu(Channel);
		FWidgetPath WidgetPath = PointerEvent.GetEventPath() ? *PointerEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent, ContextMenuSummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}

	return FReply::Unhandled();
}

FReply SDMXPatchedUniverse::HandleOnDragDetectedOnChannel(uint32 Channel, const FPointerEvent& PointerEvent)
{
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = SharedData->GetSelectedFixturePatches();

	// Only valid patches, sorted by Universe/Address
	const int32 NumValidPatches = Algo::RemoveIf(SelectedFixturePatches, [this](TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch) 
		{ 
			return !FixturePatch.IsValid();
		});
	SelectedFixturePatches.SetNum(NumValidPatches);
	Algo::SortBy(SelectedFixturePatches, [](TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch)
		{
			return (int64)FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetStartingChannel();
		}
	);

	if (SelectedFixturePatches.IsEmpty())
	{
		return FReply::Unhandled();
	}

	TMap<TWeakObjectPtr<UDMXEntityFixturePatch>, int64> FixturePatchToAbsoluteChannelOffsetMap;
	for (TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch : SelectedFixturePatches)
	{
		if (!FixturePatch.IsValid())
		{
			continue;
		}

		FixturePatch->Modify();

		const int64 AbsoluteOffset = UniverseID * DMX_MAX_ADDRESS + Channel - (FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetStartingChannel());
		FixturePatchToAbsoluteChannelOffsetMap.Add(FixturePatch, AbsoluteOffset);
	}

	const TSharedRef<FDMXEntityFixturePatchDragDropOperation> DragDropOp = MakeShared<FDMXEntityFixturePatchDragDropOperation>(FixturePatchToAbsoluteChannelOffsetMap);
	return FReply::Handled().BeginDragDrop(DragDropOp);
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
