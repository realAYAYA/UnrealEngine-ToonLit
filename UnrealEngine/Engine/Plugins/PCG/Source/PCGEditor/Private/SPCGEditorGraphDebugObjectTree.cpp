// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphDebugObjectTree.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGHelpers.h"

#include "PCGEditor.h"
#include "PCGEditorGraph.h"

#include "PropertyCustomizationHelpers.h"
#include "Selection.h"
#include "UnrealEdGlobals.h"
#include "Algo/AnyOf.h"
#include "Editor/UnrealEdEngine.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphDebugObjectTree"

namespace PCGEditorGraphDebugObjectTree
{
	void GetRowIconState(FPCGEditorGraphDebugObjectItemPtr Item, const FSlateBrush*& OutBrush, FLinearColor& OutColorAndOpacity)
	{
		const UObject* ItemObject = Item->GetObject();

		if (Item->IsDebuggable())
		{
			OutBrush = FAppStyle::Get().GetBrush("LevelEditor.Tabs.Debug");
		}
		else if (IsValid(ItemObject) && ItemObject->IsA<AActor>())
		{
			OutBrush = FSlateIconFinder::FindIconBrushForClass(ItemObject->GetClass());
		}
		else
		{
			OutBrush = Item->IsExpanded() ? FAppStyle::Get().GetBrush("Icons.FolderOpen") : FAppStyle::Get().GetBrush("Icons.FolderClosed");
		}

		OutColorAndOpacity = Item->IsDebuggable() ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
	}

	void GetErrorIconState(FPCGEditorGraphDebugObjectItemPtr Item, EVisibility& OutIconVisibility, FText& OutIconTooltipText, FLinearColor& OutColorAndOpacity)
	{
		OutIconTooltipText = FText();
		OutColorAndOpacity = FLinearColor::White;

		ELogVerbosity::Type MinVerbosity = ELogVerbosity::All;

		if (const UPCGSubsystem* Subsystem = UPCGSubsystem::GetActiveEditorInstance())
		{
			if (Item->GetPCGStack())
			{
				OutIconTooltipText = Subsystem->GetNodeVisualLogs().GetLogsSummaryText(*Item->GetPCGStack(), &MinVerbosity);
			}
			else
			{
				for (const FPCGEditorGraphDebugObjectItemPtr& Child : Item->GetChildren())
				{
					if (Child->GetPCGStack())
					{
						OutIconTooltipText = Subsystem->GetNodeVisualLogs().GetLogsSummaryText(*Child->GetPCGStack(), &MinVerbosity);
						break;
					}
				}
			}
		}

		OutIconVisibility = OutIconTooltipText.IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;

		if (MinVerbosity < ELogVerbosity::All)
		{
			constexpr FLinearColor WarningColor(1.0f, 0.75f, 0.0f, 0.9f);
			constexpr FLinearColor ErrorColor(1.0f, 0.0f, 0.0f, 0.9f);

			OutColorAndOpacity = MinVerbosity <= ELogVerbosity::Error ? ErrorColor : WarningColor;
		}
	}
}

void FPCGEditorGraphDebugObjectItem::AddChild(TSharedRef<FPCGEditorGraphDebugObjectItem> InChild)
{
	check(!Children.Contains(InChild));
	InChild->Parent = AsShared();
	Children.Add(MoveTemp(InChild));
}

const TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>>& FPCGEditorGraphDebugObjectItem::GetChildren() const
{
	return Children;
}

FPCGEditorGraphDebugObjectItemPtr FPCGEditorGraphDebugObjectItem::GetParent() const
{
	return Parent.Pin();
}

void FPCGEditorGraphDebugObjectItem::SortChildren(bool bIsAscending, bool bIsRecursive)
{
	Children.Sort([bIsAscending](const FPCGEditorGraphDebugObjectItemPtr& InLHS, const FPCGEditorGraphDebugObjectItemPtr& InRHS)
	{
		// If both items have an explicit sort priority like a loop index, this is the primary sort key.
		const int32 IndexLHS = InLHS->GetSortPriority();
		const int32 IndexRHS = InRHS->GetSortPriority();
		if (IndexLHS != INDEX_NONE && IndexRHS != INDEX_NONE)
		{
			return (IndexLHS < IndexRHS) == bIsAscending;
		}

		// Next sort priority is presence or not of children. Items without children are shown first to reduce the possibility that
		// a child item ends up displayed far away from its parent item when the tree is expanded.
		const int HasChildrenLHS = InLHS->Children.Num() ? 1 : 0;
		const int HasChildrenRHS = InRHS->Children.Num() ? 1 : 0;
		if (HasChildrenLHS != HasChildrenRHS)
		{
			return (HasChildrenLHS < HasChildrenRHS) == bIsAscending;
		}

		// Otherwise fall back to alphanumeric order.
		return (InLHS->GetLabel() < InRHS->GetLabel()) == bIsAscending;
	});

	if (bIsRecursive)
	{
		for (const FPCGEditorGraphDebugObjectItemPtr& Child : Children)
		{
			Child->SortChildren(bIsAscending, bIsRecursive);
		}
	}
}

FString FPCGEditorGraphDebugObjectItem_Actor::GetLabel() const
{
	return Actor.IsValid() ? Actor->GetActorNameOrLabel() : FString();
}

FString FPCGEditorGraphDebugObjectItem_PCGComponent::GetLabel() const
{
	if (PCGComponent.IsValid() && PCGGraph.IsValid())
	{
		return PCGComponent->GetName() + FString(TEXT(" - ") + PCGGraph->GetName());
	}

	return FString();
}

FString FPCGEditorGraphDebugObjectItem_PCGSubgraph::GetLabel() const
{
	if (PCGNode.IsValid() && PCGGraph.IsValid())
	{
		return PCGNode->GetName() + FString(TEXT(" - ") + PCGGraph->GetName());
	}

	return FString();
}

FString FPCGEditorGraphDebugObjectItem_PCGLoopIndex::GetLabel() const
{
	return FString::Format(TEXT("{0}"), { LoopIndex });
}

void SPCGEditorGraphDebugObjectItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FPCGEditorGraphDebugObjectItemPtr InItem)
{
	Item = InItem;

	// This function should auto-expand the row and select the deepest entry as the debug object if it is unambiguous (the only entry at its level in the tree).
	DoubleClickFunc = InArgs._OnDoubleClickFunc;

	// Computed once during construction as the tree is refreshed on relevant events.
	const FSlateBrush* RowIcon = nullptr;
	FLinearColor RowIconColorAndOpacity;
	PCGEditorGraphDebugObjectTree::GetRowIconState(Item, RowIcon, RowIconColorAndOpacity);

	// Icon indicating warnings and errors. Tree refreshes after execution so computing once here is sufficient.
	FText ErrorIconTooltipText;
	EVisibility ErrorIconVisibility;
	FLinearColor ErrorIconColorAndOpacity;
	PCGEditorGraphDebugObjectTree::GetErrorIconState(InItem, ErrorIconVisibility, ErrorIconTooltipText, ErrorIconColorAndOpacity);
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(6.0f, 0.0f)
		[
			SNew(SImage)
			.Visibility(EVisibility::HitTestInvisible)
			.ColorAndOpacity(RowIconColorAndOpacity)
			.Image(RowIcon)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->GetLabel()))
			// Highlight based on data available for currently inspected node. Computed dynamically in lambda to respond to inspection changes.
			.ColorAndOpacity_Lambda([this] { return Item->IsGrayedOut() ? FColor(75, 75, 75) : FColor::White; })
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(20.0f, 0.0f)
		[
			SNew(SImage)
			.Visibility(ErrorIconVisibility)
			.ToolTipText(ErrorIconTooltipText)
			.Image(FAppStyle::Get().GetBrush("Icons.Error"))
			.ColorAndOpacity(ErrorIconColorAndOpacity)
		]
	];
}

FReply SPCGEditorGraphDebugObjectItemRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (Item.IsValid() && DoubleClickFunc)
	{
		DoubleClickFunc(Item);
	}

	return FReply::Handled();
}

SPCGEditorGraphDebugObjectTree::~SPCGEditorGraphDebugObjectTree()
{
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectConstructed.RemoveAll(this);
}

void SPCGEditorGraphDebugObjectTree::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	check(InPCGEditor);
	PCGEditor = InPCGEditor;

	UPCGGraph* PCGGraph = GetPCGGraph();
	check(PCGGraph);

	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddSP(this, &SPCGEditorGraphDebugObjectTree::OnPreObjectPropertyChanged);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SPCGEditorGraphDebugObjectTree::OnObjectPropertyChanged);
	FCoreUObjectDelegates::OnObjectConstructed.AddSP(this, &SPCGEditorGraphDebugObjectTree::OnObjectConstructed);

	const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	DebugObjectTreeView = SNew(STreeView<FPCGEditorGraphDebugObjectItemPtr>)
		.TreeItemsSource(&RootItems)
		.OnGenerateRow(this, &SPCGEditorGraphDebugObjectTree::MakeTreeRowWidget)
		.OnGetChildren(this, &SPCGEditorGraphDebugObjectTree::OnGetChildren)
		.OnSelectionChanged(this, &SPCGEditorGraphDebugObjectTree::OnSelectionChanged)
		.OnExpansionChanged(this, &SPCGEditorGraphDebugObjectTree::OnExpansionChanged)
		.OnSetExpansionRecursive(this, &SPCGEditorGraphDebugObjectTree::OnSetExpansionRecursive)
		.ItemHeight(18)
		.SelectionMode(ESelectionMode::SingleToggle)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &SPCGEditorGraphDebugObjectTree::OpenContextMenu));

	const TSharedRef<SWidget> SetButton = PropertyCustomizationHelpers::MakeUseSelectedButton(
		FSimpleDelegate::CreateSP(this, &SPCGEditorGraphDebugObjectTree::SetDebugObjectFromSelection_OnClicked),
		LOCTEXT("SetDebugObject", "Set debug object from Level Editor selection."),
		TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SPCGEditorGraphDebugObjectTree::IsSetDebugObjectFromSelectionButtonEnabled)));

	const TSharedRef<SWidget> BrowseButton = PropertyCustomizationHelpers::MakeBrowseButton(
		FSimpleDelegate::CreateSP(this, &SPCGEditorGraphDebugObjectTree::SelectedDebugObject_OnClicked),
		LOCTEXT("DebugSelectActor", "Select and frame the debug actor in the Level Editor."),
		TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SPCGEditorGraphDebugObjectTree::IsSelectDebugObjectButtonEnabled)));

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f)
			[
				SetButton
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f)
			[
				BrowseButton
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				.FillSize(1.0f)
				[
					DebugObjectTreeView.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				HorizontalScrollBar
			]
		]
	];

	RequestRefresh();
}

void SPCGEditorGraphDebugObjectTree::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bNeedsRefresh)
	{
		// Updating the tree while the inspected component is generating can be bad as the selection can
		// be lost. Don't change the tree if we're inspecting something that is generating.
		const UPCGComponent* InspectedComponent = SelectedStack.GetRootComponent();
		if(InspectedComponent == nullptr || !InspectedComponent->IsGenerating())
		{
			bNeedsRefresh = false;
			RefreshTree();
		}
	}
}

void SPCGEditorGraphDebugObjectTree::SetDebugObjectSelection(const FPCGStack& FullStack)
{
	bDisableDebugObjectChangeNotification = true;

	for (FPCGEditorGraphDebugObjectItemPtr& Item : AllGraphItems)
	{
		const FPCGStack* ItemStack = Item->GetPCGStack();
		const bool bSelected = ItemStack && *ItemStack == FullStack;

		DebugObjectTreeView->SetItemSelection(Item, bSelected);

		if (bSelected)
		{
			FPCGEditorGraphDebugObjectItemPtr Parent = Item->GetParent();
			while (Parent)
			{
				DebugObjectTreeView->SetItemExpansion(Parent, true);
				Parent = Parent->GetParent();
			}
		}
	}

	bDisableDebugObjectChangeNotification = false;
}

void SPCGEditorGraphDebugObjectTree::SetNodeBeingInspected(const UPCGNode* InPCGNode)
{
	PCGNodeBeingInspected = InPCGNode;

	RequestRefresh();
}

void SPCGEditorGraphDebugObjectTree::SelectedDebugObject_OnClicked() const
{
	if (UPCGComponent* PCGComponent = PCGEditor.Pin()->GetPCGComponentBeingInspected())
	{
		AActor* Actor = PCGComponent->GetOwner();
		if (Actor && GEditor && GUnrealEd && GEditor->CanSelectActor(Actor, /*bInSelected=*/true))
		{
			GEditor->SelectNone(/*bNoteSelectionChange=*/false, /*bDeselectBSPSurfs=*/true, /*WarnAboutManyActors=*/false);
			GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/true, /*bSelectEvenIfHidden=*/true);
			GUnrealEd->Exec(Actor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
			GEditor->SelectComponent(PCGComponent, /*bInSelected=*/true, /*bNotify=*/true, /*bSelectEvenIfHidden=*/true);
		}
	}
}

bool SPCGEditorGraphDebugObjectTree::IsSelectDebugObjectButtonEnabled() const
{
	return PCGEditor.IsValid() && (PCGEditor.Pin()->GetPCGComponentBeingInspected() != nullptr);
}

void SPCGEditorGraphDebugObjectTree::SetDebugObjectFromSelection_OnClicked()
{
	const UPCGGraph* PCGGraph = GetPCGGraph();
	if (!PCGGraph)
	{
		return;
	}

	check(GEditor);
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!IsValid(SelectedActors))
	{
		return;
	}

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		const AActor* SelectedActor = Cast<AActor>(*It);
		if (!IsValid(SelectedActor))
		{
			continue;
		}

		TArray<const UPCGComponent*> PCGComponents;
		SelectedActor->GetComponents<const UPCGComponent>(PCGComponents, /*bIncludeFromChildActors=*/true);

		for (const UPCGComponent* PCGComponent : PCGComponents)
		{
			if (!IsValid(PCGComponent))
			{
				continue;
			}

			FPCGStackContext StackContext;
			if (!PCGComponent->GetStackContext(StackContext))
			{
				continue;
			}

			for (const FPCGStack& Stack : StackContext.GetStacks())
			{
				FPCGEditorGraphDebugObjectItemPtr* DebugObjectItem = AllGraphItems.FindByPredicate([&PCGComponent, &Stack](const FPCGEditorGraphDebugObjectItemPtr Item)
				{
					return Item->GetPCGComponent() == PCGComponent && *Item->GetPCGStack() == Stack;
				});

				if (DebugObjectItem)
				{
					DebugObjectTreeView->SetSingleExpandedItem(*DebugObjectItem);

					FPCGEditorGraphDebugObjectItemPtr Parent = DebugObjectItem->Get()->GetParent();
					while (Parent)
					{
						DebugObjectTreeView->SetItemExpansion(Parent, true);
						Parent = Parent->GetParent();
					}

					DebugObjectTreeView->SetSelection(*DebugObjectItem);
					DebugObjectTreeView->RequestScrollIntoView(*DebugObjectItem);
					break;
				}
			}
		}
	}
}

bool SPCGEditorGraphDebugObjectTree::IsSetDebugObjectFromSelectionButtonEnabled() const
{
	const UPCGGraph* PCGGraph = GetPCGGraph();
	if (!PCGGraph)
	{
		return false;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (!IsValid(SelectedActors))
	{
		return false;
	}

	UPCGSubsystem* Subsystem = PCGEditor.Pin() ? PCGEditor.Pin()->GetSubsystem() : nullptr;

	for (FSelectionIterator It(*SelectedActors); It; ++It)
	{
		const AActor* SelectedActor = Cast<AActor>(*It);
		if (!IsValid(SelectedActor))
		{
			continue;
		}

		TArray<const UPCGComponent*> PCGComponents;
		SelectedActor->GetComponents<const UPCGComponent>(PCGComponents, /*bIncludeFromChildActors=*/true);

		for (const UPCGComponent* PCGComponent : PCGComponents)
		{
			if (!IsValid(PCGComponent))
			{
				continue;
			}

			// Look for graph in static stacks.
			FPCGStackContext StackContext;
			if (PCGComponent->GetStackContext(StackContext))
			{
				if (Algo::AnyOf(StackContext.GetStacks(), [&PCGGraph](const FPCGStack& InStack) { return InStack.GetGraphForCurrentFrame() == PCGGraph; }))
				{
					return true;
				}
			}

			// Look for graph in dynamic stacks.
			if (Subsystem)
			{
				if (Algo::AnyOf(Subsystem->GetExecutedStacks(PCGComponent, PCGGraph), [&PCGGraph](const FPCGStack& InStack) { return InStack.GetGraphForCurrentFrame() == PCGGraph; }))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void SPCGEditorGraphDebugObjectTree::AddStacksToTree(const TArray<FPCGStack>& Stacks,
	TMap<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>>& InOutActorItems,
	TMap<const FPCGStack, FPCGEditorGraphDebugObjectItemPtr>& InOutStackToItem)
{
	const UPCGGraph* GraphBeingEdited = GetPCGGraph();
	UPCGSubsystem* Subsystem = PCGEditor.Pin()->GetSubsystem();
	if (!GraphBeingEdited || !Subsystem)
	{
		return;
	}

	for (const FPCGStack& Stack : Stacks)
	{
		// If the current graph is not in the stack at all then skip.
		bool bRemainingStackContainsEditedGraph = Stack.HasObject(GraphBeingEdited);
		if (!bRemainingStackContainsEditedGraph)
		{
			continue;
		}

		UPCGComponent* PCGComponent = const_cast<UPCGComponent*>(Stack.GetRootComponent());
		if (!PCGComponent)
		{
			continue;
		}

		// Prevent duplicate entries from the editor world while in PIE.
		if (PCGHelpers::IsRuntimeOrPIE())
		{
			if (UWorld* World = PCGComponent->GetWorld())
			{
				if (!World->IsGameWorld())
				{
					continue;
				}
			}
		}

		int32 TopGraphIndex = INDEX_NONE;
		UPCGGraph* TopGraph = const_cast<UPCGGraph*>(Stack.GetRootGraph(&TopGraphIndex));
		if (!TopGraph)
		{
			continue;
		}

		// If we're inspecting a node which has not logged inspection data in a previous execution, display grayed out.
		const bool bDisplayGrayedOut = PCGNodeBeingInspected && !PCGComponent->HasNodeProducedData(PCGNodeBeingInspected, Stack);

		AActor* Actor = PCGComponent->GetOwner();
		if (!Actor)
		{
			continue;
		}

		// Add actor item if not already added.
		FPCGEditorGraphDebugObjectItemPtr ActorItem;
		if (TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>* FoundActorItem = InOutActorItems.Find(Actor))
		{
			ActorItem = *FoundActorItem;
		}
		else
		{
			ActorItem = InOutActorItems.Emplace(Actor, MakeShared<FPCGEditorGraphDebugObjectItem_Actor>(Actor, bDisplayGrayedOut));
			AllGraphItems.Add(ActorItem);
		}

		const TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFrames();

		// Example stack:
		//     Component/TopGraph/SubgraphNode/Subgraph/LoopSubgraphNode/LoopIndex/LoopSubgraph
		// 
		// The loop below adds tree items for component & top graph, and then whenever a graph is encountered
		// we look a previous frames to determine whether to add a subgraph item or loop subgraph item.
		for (int FrameIndex = 1; FrameIndex < StackFrames.Num() && bRemainingStackContainsEditedGraph; FrameIndex++)
		{
			const FPCGStackFrame& StackFrame = StackFrames[FrameIndex];
			const FPCGStackFrame& PreviousStackFrame = StackFrames[FrameIndex - 1];
			FPCGEditorGraphDebugObjectItemPtr CurrentItem;

			// When we encounter a graph, we look at the frame index and/or preceding frames to determine the graph type.
			if (const UPCGGraph* StackGraph = Cast<const UPCGGraph>(StackFrame.Object))
			{
				const bool bIsDebuggable = (GraphBeingEdited == StackGraph);

				// Top graph.
				if (FrameIndex == TopGraphIndex && StackGraph == TopGraph)
				{
					FPCGStack GraphStack = Stack;
					GraphStack.GetStackFramesMutable().SetNum(FrameIndex + 1);

					if (!InOutStackToItem.Contains(GraphStack))
					{
						FPCGEditorGraphDebugObjectItemPtr TopGraphItem = InOutStackToItem.Emplace(
							GraphStack,
							MakeShared<FPCGEditorGraphDebugObjectItem_PCGComponent>(PCGComponent, StackGraph, GraphStack, bIsDebuggable, bDisplayGrayedOut));

						AllGraphItems.Add(TopGraphItem);

						ActorItem->AddChild(TopGraphItem.ToSharedRef());
					}
				}
				// Previous stack was node, therefore subgraph.
				else if (const UPCGNode* SubgraphNode = Cast<const UPCGNode>(PreviousStackFrame.Object))
				{
					FPCGStack GraphStack = Stack;
					GraphStack.GetStackFramesMutable().SetNum(FrameIndex + 1);

					if (!InOutStackToItem.Contains(GraphStack))
					{
						FPCGEditorGraphDebugObjectItemPtr GraphItem = InOutStackToItem.Emplace(
							GraphStack,
							MakeShared<FPCGEditorGraphDebugObjectItem_PCGSubgraph>(SubgraphNode, StackGraph, GraphStack, bIsDebuggable, bDisplayGrayedOut));

						AllGraphItems.Add(GraphItem);

						TArray<FPCGStackFrame>& GraphStackFrames = GraphStack.GetStackFramesMutable();
						while (GraphStackFrames.Num() > 0)
						{
							GraphStackFrames.SetNum(GraphStackFrames.Num() - 1, EAllowShrinking::No);

							if (FPCGEditorGraphDebugObjectItemPtr* ParentItem = InOutStackToItem.Find(GraphStack))
							{
								(*ParentItem)->AddChild(GraphItem.ToSharedRef());
								break;
							}
						}
					}
				}
				// Previous stack was loop index, therefore loop subgraph.
				else if (FrameIndex >= 2 && PreviousStackFrame.LoopIndex != INDEX_NONE)
				{
					const UPCGNode* LoopSubgraphNode = Cast<const UPCGNode>(StackFrames[FrameIndex - 2].Object);
					if (ensure(LoopSubgraphNode))
					{
						// Take the stack up to the looped subgraph node, add a item for the node + graph.
						FPCGStack LoopGraphStack = Stack;
						LoopGraphStack.GetStackFramesMutable().SetNum(FrameIndex - 1);

						if (!InOutStackToItem.Contains(LoopGraphStack))
						{
							FPCGEditorGraphDebugObjectItemPtr LoopGraphItem = InOutStackToItem.Emplace(
								LoopGraphStack,
								MakeShared<FPCGEditorGraphDebugObjectItem_PCGSubgraph>(LoopSubgraphNode, StackGraph, LoopGraphStack, /*bIsDebuggable=*/false, false));

							AllGraphItems.Add(LoopGraphItem);

							TArray<FPCGStackFrame>& LoopGraphStackFrames = LoopGraphStack.GetStackFramesMutable();
							while (LoopGraphStackFrames.Num() > 0)
							{
								LoopGraphStackFrames.SetNum(LoopGraphStackFrames.Num() - 1, EAllowShrinking::No);

								if (FPCGEditorGraphDebugObjectItemPtr* ParentItem = InOutStackToItem.Find(LoopGraphStack))
								{
									(*ParentItem)->AddChild(LoopGraphItem.ToSharedRef());
									break;
								}
							}
						}

						// Take full stack up until this point which will be the unique stack for the loop iteration.
						FPCGStack LoopIterationStack = Stack;
						LoopIterationStack.GetStackFramesMutable().SetNum(FrameIndex + 1);

						if (!InOutStackToItem.Contains(LoopIterationStack))
						{
							FPCGEditorGraphDebugObjectItemPtr LoopIterationItem = InOutStackToItem.Emplace(
								LoopIterationStack,
								MakeShared<FPCGEditorGraphDebugObjectItem_PCGLoopIndex>(PreviousStackFrame.LoopIndex, StackGraph, LoopIterationStack, bIsDebuggable, bDisplayGrayedOut));

							AllGraphItems.Add(LoopIterationItem);

							TArray<FPCGStackFrame>& GraphStackFrames = LoopIterationStack.GetStackFramesMutable();
							while (GraphStackFrames.Num() > 0)
							{
								GraphStackFrames.SetNum(GraphStackFrames.Num() - 1, EAllowShrinking::No);

								if (FPCGEditorGraphDebugObjectItemPtr* ParentItem = InOutStackToItem.Find(LoopIterationStack))
								{
									(*ParentItem)->AddChild(LoopIterationItem.ToSharedRef());
									break;
								}
							}
						}
					}
				}
			}

			// Check if any of the remaining stack frames contain valid debug targets - invocations of the current edited graph.
			// If not then we will abandon this stack, pruning the tree.
			bRemainingStackContainsEditedGraph = false;
			for (int RemainingFrameIndex = FrameIndex + 1; RemainingFrameIndex < StackFrames.Num(); ++RemainingFrameIndex)
			{
				if (StackFrames[RemainingFrameIndex].Object == GraphBeingEdited)
				{
					bRemainingStackContainsEditedGraph = true;
					break;
				}
			}
		}
	}
}

void SPCGEditorGraphDebugObjectTree::RefreshTree()
{
	RootItems.Empty();
	AllGraphItems.Empty();
	DebugObjectTreeView->RequestTreeRefresh();

	UPCGGraph* PCGGraph = GetPCGGraph();
	if (!PCGGraph)
	{
		return;
	}

	UPCGSubsystem* Subsystem = PCGEditor.Pin()->GetSubsystem();
	if (!Subsystem)
	{
		return;
	}

	TArray<UObject*> PCGComponents;
	GetObjectsOfClass(UPCGComponent::StaticClass(), PCGComponents, /*bIncludeDerivedClasses=*/ true);

	TMap<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>> ActorItems;
	TMap<const FPCGStack, FPCGEditorGraphDebugObjectItemPtr> StackToItem;

	for (UObject* PCGComponentObject : PCGComponents)
	{
		if (!IsValid(PCGComponentObject))
		{
			continue;
		}

		UPCGComponent* PCGComponent = Cast<UPCGComponent>(PCGComponentObject);
		if (!PCGComponent || !PCGComponent->IsRegistered())
		{
			continue;
		}

		// Process static stacks that can be read from the compiled graph.
		FPCGStackContext StackContext;
		if (!PCGComponent->GetStackContext(StackContext))
		{
			continue;
		}
		AddStacksToTree(StackContext.GetStacks(), ActorItems, StackToItem);

		// Process stacks encountered during execution so far, which will include dynamic subgraphs & loop subgraphs.
		// There will be overlaps with the static stacks but only unique entries will be added to the tree.
		AddStacksToTree(Subsystem->GetExecutedStacks(PCGComponent, PCGGraph), ActorItems, StackToItem);
	}

	for (TPair<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>>& ActorItem : ActorItems)
	{
		RootItems.Add(MoveTemp(ActorItem.Value));
	}

	SortTreeItems();
	RestoreTreeState();
}

void SPCGEditorGraphDebugObjectTree::SortTreeItems(bool bIsAscending, bool bIsRecursive)
{
	RootItems.Sort([bIsAscending](const FPCGEditorGraphDebugObjectItemPtr& InLHS, const FPCGEditorGraphDebugObjectItemPtr& InRHS)
	{
		return (InLHS->GetLabel() < InRHS->GetLabel()) == bIsAscending;
	});

	if (bIsRecursive)
	{
		for (const FPCGEditorGraphDebugObjectItemPtr& Item : RootItems)
		{
			Item->SortChildren(bIsAscending, bIsRecursive);
		}
	}
}

void SPCGEditorGraphDebugObjectTree::RestoreTreeState()
{
	// Try to restore user item expansion.
	TSet<FPCGStack> ExpandedStacksBefore = ExpandedStacks;
	for (const FPCGStack& ExpandedStack : ExpandedStacksBefore)
	{
		for (FPCGEditorGraphDebugObjectItemPtr& Item : AllGraphItems)
		{
			const FPCGStack* ItemStack = Item->GetPCGStack();

			if (ItemStack && *ItemStack == ExpandedStack)
			{
				DebugObjectTreeView->SetItemExpansion(Item, true);
			}
		}
	}

	bool bFoundMatchingStack = false;

	// Try to restore user item selection by exact matching.
	for (FPCGEditorGraphDebugObjectItemPtr& Item : AllGraphItems)
	{
		const FPCGStack* ItemStack = Item->GetPCGStack();

		if (ItemStack && SelectedStack == *ItemStack)
		{
			DebugObjectTreeView->SetItemSelection(Item, true);
			bFoundMatchingStack = true;
			break;
		}
	}

	// Try to restore user item selection by fuzzy matching (e.g. share the same owner) if no exactly matching stack was found.
	if (!bFoundMatchingStack)
	{
		for (FPCGEditorGraphDebugObjectItemPtr& Item : AllGraphItems)
		{
			const FPCGStack* ItemStack = Item->GetPCGStack();

			bool bFuzzyMatch = false;

			if (ItemStack && SelectedGraph.Get() == ItemStack->GetRootGraph())
			{
				const UPCGComponent* RootComponent = ItemStack->GetRootComponent();
				const AActor* RootOwner = RootComponent ? RootComponent->GetOwner() : nullptr;
				const APCGPartitionActor* RootPartitionActor = Cast<APCGPartitionActor>(RootOwner);

				if (RootComponent && RootPartitionActor)
				{
					// For local components, we can fuzzy match as long as the GridSize, GridCoord, OriginalComponent, and ExecutionDomain are the same.
					// This is equivalent to saying they are on the same partition actor and come from the same original component.
					bFuzzyMatch = SelectedGridSize == RootComponent->GetGenerationGridSize()
						&& SelectedGridCoord == RootPartitionActor->GetGridCoord()
						&& SelectedOriginalComponent.Get() == RootPartitionActor->GetOriginalComponent(RootComponent)
						&& (SelectedOriginalComponent.IsValid() && SelectedOriginalComponent->IsManagedByRuntimeGenSystem() == RootComponent->IsManagedByRuntimeGenSystem());
				}
				else
				{
					// For original components, we can fuzzy match as long as the owning actor is the same.
					// Note: This fails for multiple original components with the same graph on the same actor, since there is no way to know which one to pick.
					if (SelectedOwner.Get() == RootOwner && Item->GetParent() && Item->GetParent()->GetChildren().Num() == 1)
					{
						int32 ItemRootGraphIndex = INDEX_NONE;
						int32 SelectedRootGraphIndex = INDEX_NONE;

						ItemStack->GetRootGraph(&ItemRootGraphIndex);
						SelectedStack.GetRootGraph(&SelectedRootGraphIndex);

						const TArray<FPCGStackFrame>& ItemStackFrames = ItemStack->GetStackFrames();
						const TArray<FPCGStackFrame>& SelectedStackFrames = SelectedStack.GetStackFrames();

						// If the stacks match from the RootGraph onwards, then our fuzzy match should succeed.
						if (ItemRootGraphIndex != INDEX_NONE && ItemRootGraphIndex == SelectedRootGraphIndex && ItemStackFrames.Num() == SelectedStackFrames.Num())
						{
							bool bAllStackFramesMatch = true;

							for (int I = ItemRootGraphIndex; I < ItemStackFrames.Num(); ++I)
							{
								if (!ItemStackFrames[I].IsValid() || ItemStackFrames[I] != SelectedStackFrames[I])
								{
									bAllStackFramesMatch = false;
									break;
								}
							}

							if (bAllStackFramesMatch)
							{
								bFuzzyMatch = true;
							}
						}
					}
				}
			}

			if (bFuzzyMatch)
			{
				// Force the selected object to re-expand.
				FPCGEditorGraphDebugObjectItemPtr Parent = Item->GetParent();
				while (Parent)
				{
					DebugObjectTreeView->SetItemExpansion(Parent, true);
					Parent = Parent->GetParent();
				}

				DebugObjectTreeView->SetItemSelection(Item, true);
				break;
			}
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& /*InPropertyChain*/)
{
	if (UPCGGraphInstance* PCGgraphInstance = Cast<UPCGGraphInstance>(InObject))
	{
		const UPCGGraph* PCGGraph = PCGgraphInstance->GetGraph();
		if (PCGGraph && PCGGraph == GetPCGGraph())
		{
			RequestRefresh();
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (const UPCGGraphInterface* PCGGraphInterface = Cast<UPCGGraphInterface>(InObject))
	{
		const UPCGGraph* PCGGraph = PCGGraphInterface->GetGraph();
		if (PCGGraph && PCGGraph == GetPCGGraph())
		{
			RequestRefresh();
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnObjectConstructed(UObject* InObject)
{
	if (const UPCGComponent* PCGComponent = Cast<UPCGComponent>(InObject))
	{
		RequestRefresh();
	}
}

UPCGGraph* SPCGEditorGraphDebugObjectTree::GetPCGGraph() const
{
	const TSharedPtr<FPCGEditor> PCGEditorPtr = PCGEditor.Pin();
	const UPCGEditorGraph* PCGEditorGraph = PCGEditorPtr.IsValid() ? PCGEditorPtr->GetPCGEditorGraph() : nullptr;

	return PCGEditorGraph ? PCGEditorGraph->GetPCGGraph() : nullptr;
}

TSharedRef<ITableRow> SPCGEditorGraphDebugObjectTree::MakeTreeRowWidget(FPCGEditorGraphDebugObjectItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew( STableRow<TSharedPtr<SPCGEditorGraphDebugObjectItemRow>>, InOwnerTable)
		[
			SNew(SPCGEditorGraphDebugObjectItemRow, InOwnerTable, InItem)
				.OnDoubleClickFunc([this](const FPCGEditorGraphDebugObjectItemPtr& Item) { ExpandAndSelectDebugObject(Item); } )
		];
}

void SPCGEditorGraphDebugObjectTree::OnGetChildren(FPCGEditorGraphDebugObjectItemPtr InItem, TArray<FPCGEditorGraphDebugObjectItemPtr>& OutChildren) const
{
	if (InItem.IsValid())
	{
		for (TWeakPtr<FPCGEditorGraphDebugObjectItem> ChildItem : InItem->GetChildren())
		{
			FPCGEditorGraphDebugObjectItemPtr ChildItemPtr = ChildItem.Pin();
			OutChildren.Add(ChildItemPtr);
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnSelectionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, ESelectInfo::Type InSelectInfo)
{
	// If the user selects a new item, record the previous stack. This is helpful to give previous selection information to 
	// context menu commands, because right clicking an item changes the selection.
	if (InSelectInfo != ESelectInfo::Direct)
	{
		PreviouslySelectedStack = SelectedStack;
	}

	// Reset selected item information.
	SelectedStack = FPCGStack();
	SelectedGraph = nullptr;
	SelectedOwner = nullptr;
	SelectedGridSize = PCGHiGenGrid::UnboundedGridSize();
	SelectedGridCoord = FIntVector::ZeroValue;
	SelectedOriginalComponent = nullptr;

	if (const FPCGStack* Stack = InItem ? InItem->GetPCGStack() : nullptr)
	{
		SelectedStack = *Stack;
		SelectedGraph = SelectedStack.GetRootGraph();

		if (const UPCGComponent* RootComponent = SelectedStack.GetRootComponent())
		{
			SelectedOwner = RootComponent->GetOwner();

			if (const APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(SelectedOwner.Get()))
			{
				SelectedGridSize = RootComponent->GetGenerationGridSize();
				SelectedGridCoord = PartitionActor->GetGridCoord();
				SelectedOriginalComponent = PartitionActor->GetOriginalComponent(RootComponent);
			}
			else
			{
				SelectedOriginalComponent = RootComponent;
			}
		}
	}

	if (bDisableDebugObjectChangeNotification)
	{
		return;
	}

	// Only attempt to inspect stacks that correspond to the edited graph. Other graphs need to be inspected in their own editor.
	if (InItem && SelectedStack.GetGraphForCurrentFrame() == PCGEditor.Pin()->GetPCGGraph())
	{
		PCGEditor.Pin()->SetStackBeingInspected(SelectedStack);
	}
	else
	{
		PCGEditor.Pin()->ClearStackBeingInspected();
	}
}

void SPCGEditorGraphDebugObjectTree::OnExpansionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInIsExpanded)
{
	if (!InItem.IsValid())
	{
		return;
	}

	InItem->SetExpanded(bInIsExpanded);

	if (const FPCGStack* Stack = InItem->GetPCGStack())
	{
		if (bInIsExpanded)
		{
			ExpandedStacks.Add(*Stack);
		}
		else
		{
			ExpandedStacks.Remove(*Stack);
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnSetExpansionRecursive(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInExpand) const
{
	if (!InItem.IsValid() || !DebugObjectTreeView.IsValid())
	{
		return;
	}

	DebugObjectTreeView->SetItemExpansion(InItem, bInExpand);
	InItem->SetExpanded(bInExpand);

	for (const FPCGEditorGraphDebugObjectItemPtr& ChildItem : InItem->GetChildren())
	{
		if (ChildItem.IsValid())
		{
			OnSetExpansionRecursive(ChildItem, bInExpand);
		}
	}
}

void SPCGEditorGraphDebugObjectTree::ExpandAndSelectDebugObject(FPCGEditorGraphDebugObjectItemPtr InItem)
{
	if (!InItem.IsValid())
	{
		return;
	}

	// Expand this item in the tree view.
	OnSetExpansionRecursive(InItem, /*bInExpand=*/true);

	FPCGEditorGraphDebugObjectItemPtr Item = InItem;
	int NumChildren = 1; // If we land on the deepest item already, we should just select it regardless of how many siblings it has.

	// Find the deepest entry in this branch of the tree view.
	while (true)
	{
		const TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>>& Children = Item->GetChildren();
		TSet<FPCGEditorGraphDebugObjectItemPtr>::TConstIterator It = Children.CreateConstIterator();

		if (!It)
		{
			break;
		}
		else
		{
			Item = *It;
			NumChildren = Children.Num();
		}
	}

	// Set the discovered item as the debug object if it is the only child.
	if (NumChildren == 1)
	{
		DebugObjectTreeView->SetSelection(Item);
	}
}

TSharedPtr<SWidget> SPCGEditorGraphDebugObjectTree::OpenContextMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, /*InCommandList=*/nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("JumpToSelectedGraph", "Jump To"),
		LOCTEXT("JumpToSelectedGraphTooltip", "Jumps to the selected graph."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
		FUIAction(FExecuteAction::CreateSP(this, &SPCGEditorGraphDebugObjectTree::ContextMenu_JumpToGraphInTree), FCanExecuteAction::CreateSP(this, &SPCGEditorGraphDebugObjectTree::ContextMenu_JumpToGraphInTree_CanExecute))
	);

	return MenuBuilder.MakeWidget();
}

void SPCGEditorGraphDebugObjectTree::ContextMenu_JumpToGraphInTree()
{
	// If we were previously debugging a target and we're jumping to a parent graph, jump to the subgraph/loop node
	// that corresponds to the previously selected stack.
	// 
	// Example:
	//		Jump target: Component/TopGraph
	//		Previous stack: Component/TopGraph/SubgraphNode/Subgraph
	// 
	// Here the "subject" of the jump here is SubgraphNode - obtain this from the previous stack, frame index after TopGraph.

	const TArray<FPCGEditorGraphDebugObjectItemPtr> SelectedItems = DebugObjectTreeView->GetSelectedItems();
	for (const FPCGEditorGraphDebugObjectItemPtr& SelectedItem : SelectedItems)
	{
		UPCGGraph* JumpToPCGGraph = nullptr;
		UPCGNode* JumpToPCGNode = nullptr;

		if (SelectedItem->IsDebuggable())
		{
			// Debuggable target graphs correspond to the currently edited graph. For this case open the parent graph and jump to the corresponding subgraph node.
			
			// Search the stack for the parent graph.
			for (int i = SelectedStack.GetStackFrames().Num() - 2; i > 0; --i) 
			{
				if (SelectedStack.GetStackFrames()[i].Object != nullptr && SelectedStack.GetStackFrames()[i].Object->IsA<UPCGGraph>())
				{
					JumpToPCGGraph = const_cast<UPCGGraph*>(Cast<const UPCGGraph>(SelectedStack.GetStackFrames()[i].Object));
					JumpToPCGNode = const_cast<UPCGNode*>(Cast<const UPCGNode>(SelectedStack.GetStackFrames()[i + 1].Object));
					break;
				}
			}
		}
		else 
		{
			// Non-debuggable target graphs are graphs that are not the current edited graph.
			JumpToPCGGraph = const_cast<UPCGGraph*>(SelectedItem->GetPCGGraph());

			// When the user right clicks, the selection moves. PreviouslySelectedStack will hold the previous selection, and
			// we retrieve our subject node from there, which can be found 1 frame after the jump to graph, hence the Num() below.
			const int SubjectNodeFrameIndex = SelectedStack.GetStackFrames().Num();
			if (PreviouslySelectedStack.GetStackFrames().IsValidIndex(SubjectNodeFrameIndex))
			{
				JumpToPCGNode = const_cast<UPCGNode*>(Cast<const UPCGNode>(PreviouslySelectedStack.GetStackFrames()[SubjectNodeFrameIndex].Object));
			}
		}

		if (JumpToPCGGraph && JumpToPCGNode)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(JumpToPCGGraph);

			if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(JumpToPCGGraph, /*bFocusIfOpen*/true))
			{
				static_cast<FPCGEditor*>(EditorInstance)->JumpToNode(JumpToPCGNode);
			}
		}
	}
}

bool SPCGEditorGraphDebugObjectTree::ContextMenu_JumpToGraphInTree_CanExecute() const
{
	const TArray<FPCGEditorGraphDebugObjectItemPtr> SelectedItems = DebugObjectTreeView->GetSelectedItems();
	for (const FPCGEditorGraphDebugObjectItemPtr& SelectedItem : SelectedItems)
	{
		const UPCGGraph* PCGGraphBeingEdited = PCGEditor.IsValid() ? PCGEditor.Pin()->GetPCGGraph() : nullptr;
		const UPCGGraph* SelectedPCGGraph = SelectedItem->GetPCGGraph();

		// Offer jump-to command if any selected item is a graph or a subgraph loop iteration.
		if (SelectedPCGGraph || SelectedItem->IsLoopIteration())
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
