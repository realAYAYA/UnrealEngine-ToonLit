// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphDebugObjectTree.h"

#include "PCGComponent.h"
#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorModule.h"
#include "PCGGraph.h"
#include "Elements/PCGLoopElement.h"

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

FString FPCGEditorGraphDebugObjectItem_Actor::GetLabel() const
{
	return Actor.IsValid() ? Actor->GetActorNameOrLabel() : FString();
}

FString FPCGEditorGraphDebugObjectItem_PCGComponent::GetLabel() const
{
	return PCGComponent.IsValid() ? PCGComponent->GetName() : FString();
}

FString FPCGEditorGraphDebugObjectItem_PCGGraph::GetLabel() const
{
	return PCGGraph.IsValid() ? PCGGraph->GetName() : FString();
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
	if (PCGNode.IsValid())
	{
		return PCGNode->GetName() + FString::Format(TEXT(" - {0}"), { LoopIndex });
	}

	return FString();
}

void SPCGEditorGraphDebugObjectItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FPCGEditorGraphDebugObjectItemPtr InItem)
{
	Item = InItem;
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SImage)
			.Visibility(EVisibility::HitTestInvisible)
			.Image_Lambda([this]()
			{
				const UObject* ItemObject = Item->GetObject();
				return FSlateIconFinder::FindIconBrushForClass(IsValid(ItemObject) ? ItemObject->GetClass() : nullptr); // TODO: IsValid check is done here to prevent crashing due to deleted pcg components, we need to refresh the tree items when world changes, then this might not be needed.
			})
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->GetLabel()))
		]
	];
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
				.OnSetExpansionRecursive(this, &SPCGEditorGraphDebugObjectTree::OnSetExpansionRecursive)
				.ItemHeight(18)
				.AllowOverscroll(EAllowOverscroll::No)
				.ExternalScrollbar(VerticalScrollBar)
				.ConsumeMouseWheel(EConsumeMouseWheel::Always);

	const TSharedRef<SWidget> SetButton = PropertyCustomizationHelpers::MakeUseSelectedButton(
		FSimpleDelegate::CreateSP(this, &SPCGEditorGraphDebugObjectTree::SetDebugObjectFromSelection_OnClicked),
		LOCTEXT("SetDebugObject", "Set debug object from Level Editor selection."),
		TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SPCGEditorGraphDebugObjectTree::IsSetDebugObjectFromSelectionButtonEnabled)));

	const TSharedRef<SWidget> BrowseButton = PropertyCustomizationHelpers::MakeBrowseButton(
		FSimpleDelegate::CreateSP(this, &SPCGEditorGraphDebugObjectTree::SelectedDebugObject_OnClicked),
		LOCTEXT("DebugSelectActor", "Select and frame the debug actor in the Level Editor."),
		TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SPCGEditorGraphDebugObjectTree::IsSelectDebugObjectButtonEnabled))
);
	
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
		bNeedsRefresh = false;
		RefreshTree();
	}
}

void SPCGEditorGraphDebugObjectTree::AddDynamicStack(const TWeakObjectPtr<UPCGComponent> InComponent, const FPCGStack& InvocationStack)
{
	TArray<FPCGStack>& Stacks = DynamicInvocationStacks.FindOrAdd(InComponent);

	if (!Stacks.Contains(InvocationStack))
	{
		Stacks.Add(InvocationStack);
		RequestRefresh();
	}
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

			FPCGStackContext StackContext = FPCGStackContext::CreateStackContextFromGraph(PCGComponent->GetGraph());

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
			
			FPCGStackContext StackContext = FPCGStackContext::CreateStackContextFromGraph(PCGComponent->GetGraph());

			const bool bGraphFound = Algo::AnyOf(StackContext.GetStacks(), [&PCGGraph](const FPCGStack& InStack)
			{
				return Cast<const UPCGGraph>(InStack.GetStackFrames().Top().Object) == PCGGraph;
			});
			
			if(bGraphFound)
			{
				return true;
			}
		}
	}

	return false;
}

void SPCGEditorGraphDebugObjectTree::RefreshTree()
{
	RootItems.Empty();
	AllGraphItems.Empty();
	DebugObjectTreeView->RequestTreeRefresh();	

	const UPCGGraph* PCGGraph = GetPCGGraph();
	if (!PCGGraph)
	{
		return;
	}

	TArray<UObject*> PCGComponents;
	GetObjectsOfClass(UPCGComponent::StaticClass(), PCGComponents, /*bIncludeDerivedClasses=*/ true);

	TMap<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>> ActorItems;
	TMap<UPCGComponent*, TSharedPtr<FPCGEditorGraphDebugObjectItem_PCGComponent>> ComponentItems;
	
	for (UObject* PCGComponentObject : PCGComponents)
	{
		if (!IsValid(PCGComponentObject) || PCGComponentObject->HasAnyFlags(RF_Transient))
		{
			continue;
		}

		UPCGComponent* PCGComponent = Cast<UPCGComponent>(PCGComponentObject);
		if (!PCGComponent)
		{
			continue;
		}

		AActor* Actor = PCGComponent->GetOwner();
		if (!Actor)
		{
			continue;
		}

		const UPCGGraph* PCGComponentGraph = PCGComponent->GetGraph();
		if (!PCGComponentGraph)
		{
			continue;
		}

		FPCGStackContext StackContext = FPCGStackContext::CreateStackContextFromGraph(PCGComponentGraph);

		TMap<const UPCGGraph*, FPCGEditorGraphDebugObjectItemPtr> GraphItems;
		TArray<FPCGEditorGraphDebugObjectItemPtr> NodeItems;

		// Process statically generated stacks
		for (const FPCGStack& Stack : StackContext.GetStacks())
		{
			const UPCGGraph* TopStackGraph = Cast<const UPCGGraph>(Stack.GetStackFrames().Top().Object);
			if (TopStackGraph == PCGGraph)
			{
				// Find or Create ActorItem
				TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor> ActorItem;
				if (TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>* FoundActorItem = ActorItems.Find(Actor))
				{
					ActorItem = *FoundActorItem;
				}
				else
				{
					ActorItem = ActorItems.Emplace(Actor, MakeShared<FPCGEditorGraphDebugObjectItem_Actor>(Actor));
				}

				// Find or Create ComponentItem
				TSharedPtr<FPCGEditorGraphDebugObjectItem_PCGComponent> ComponentItem;
				if (TSharedPtr<FPCGEditorGraphDebugObjectItem_PCGComponent>* FoundComponentItem = ComponentItems.Find(PCGComponent))
				{
					ComponentItem = *FoundComponentItem;
				}
				else
				{
					ComponentItem = ComponentItems.Emplace(PCGComponent, MakeShared<FPCGEditorGraphDebugObjectItem_PCGComponent>(PCGComponent));
				}

				if (!ComponentItem->GetParent())
				{
					ActorItem->AddChild(ComponentItem.ToSharedRef());
				}

				const TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFrames();

				FString StackPath;
				Stack.CreateStackFramePath(StackPath);

				FPCGEditorGraphDebugObjectItemPtr PreviousItem = ComponentItem;
				for (int32 StackIndex = 0; StackIndex < StackFrames.Num(); StackIndex++)
				{
					const FPCGStackFrame& StackFrame = StackFrames[StackIndex];
					FPCGEditorGraphDebugObjectItemPtr CurrentItem;
					if (const UPCGGraph* StackGraph = Cast<const UPCGGraph>(StackFrame.Object))
					{
						if (FPCGEditorGraphDebugObjectItemPtr* GraphItem = GraphItems.Find(StackGraph))
						{
							CurrentItem = *GraphItem;
						}
						else
						{
							CurrentItem = GraphItems.Emplace(StackGraph, MakeShared<FPCGEditorGraphDebugObjectItem_PCGGraph>(StackGraph, StackGraph == PCGGraph ? Stack : FPCGStack()));
						}
					}
					else if (const UPCGNode* StackNode = Cast<const UPCGNode>(StackFrame.Object))
					{
						// We cannot handle dynamically executed graphs in this path. They must be provided through DynamicInvocationStacks
						if (const UPCGBaseSubgraphSettings* SubgraphSettings = Cast<const UPCGBaseSubgraphSettings>(StackNode->GetSettings()))
						{
							if (SubgraphSettings->IsDynamicGraph())
							{
								break;
							}
						}

						const int32 NextStackIndex = StackIndex + 1;
						if (StackFrames.IsValidIndex(NextStackIndex))
						{
							const FPCGStackFrame& NextStackFrame = StackFrames[NextStackIndex];
							if (const UPCGGraph* NextStackGraph = Cast<const UPCGGraph>(NextStackFrame.Object))
							{
								const FPCGEditorGraphDebugObjectItemPtr* NodeItem = NodeItems.FindByPredicate([&StackNode,&Stack](const FPCGEditorGraphDebugObjectItemPtr& InItem)
								{
									return (InItem->GetObject() == StackNode) && (*InItem->GetPCGStack() == Stack);
								});

								if (NodeItem)
								{
									CurrentItem = *NodeItem;
								}
								else
								{
									CurrentItem = NodeItems.Add_GetRef(MakeShared<FPCGEditorGraphDebugObjectItem_PCGSubgraph>(StackNode, NextStackGraph, NextStackGraph == TopStackGraph ? Stack : FPCGStack()));
								}
								
								StackIndex++;
							}
						}
					}

					if (!CurrentItem)
					{
						break;
					}

					if (!CurrentItem->GetParent())
					{
						PreviousItem->AddChild(CurrentItem.ToSharedRef());
					}

					AllGraphItems.Add(CurrentItem);
					PreviousItem = CurrentItem;
				}
			}
		}

		if (const TArray<FPCGStack>* DynamicStacks = DynamicInvocationStacks.Find(PCGComponent))
		{
			for (const FPCGStack& Stack : *DynamicStacks)
			{
				TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor> ActorItem;
				if (TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>* FoundActorItem = ActorItems.Find(Actor))
				{
					ActorItem = *FoundActorItem;
				}
				else
				{
					ActorItem = ActorItems.Emplace(Actor, MakeShared<FPCGEditorGraphDebugObjectItem_Actor>(Actor));
				}

				TSharedPtr<FPCGEditorGraphDebugObjectItem_PCGComponent> ComponentItem;
				if (TSharedPtr<FPCGEditorGraphDebugObjectItem_PCGComponent>* FoundComponentItem = ComponentItems.Find(PCGComponent))
				{
					ComponentItem = *FoundComponentItem;
				}
				else
				{
					ComponentItem = ComponentItems.Emplace(PCGComponent, MakeShared<FPCGEditorGraphDebugObjectItem_PCGComponent>(PCGComponent));
				}

				if (!ComponentItem->GetParent())
				{
					ActorItem->AddChild(ComponentItem.ToSharedRef());
				}

				const TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFrames();

				FString StackPath;
				Stack.CreateStackFramePath(StackPath);

				FPCGEditorGraphDebugObjectItemPtr PreviousItem = ComponentItem;
				for (int32 StackIndex = 0; StackIndex < StackFrames.Num(); StackIndex++)
				{
					const FPCGStackFrame& StackFrame = StackFrames[StackIndex];
					FPCGEditorGraphDebugObjectItemPtr CurrentItem;
					if (const UPCGGraph* StackGraph = Cast<const UPCGGraph>(StackFrame.Object))
					{
						if (FPCGEditorGraphDebugObjectItemPtr* GraphItem = GraphItems.Find(StackGraph))
						{
							CurrentItem = *GraphItem;
						}
						else
						{
							CurrentItem = GraphItems.Emplace(StackGraph, MakeShared<FPCGEditorGraphDebugObjectItem_PCGGraph>(StackGraph, StackGraph == PCGGraph ? Stack : FPCGStack()));
						}
					}
					else if (const UPCGNode* StackNode = Cast<const UPCGNode>(StackFrame.Object))
					{
						const int32 NextStackIndex = StackIndex + 1;
						if (StackFrames.IsValidIndex(NextStackIndex))
						{
							const FPCGEditorGraphDebugObjectItemPtr* NodeItem = NodeItems.FindByPredicate([&StackNode, &Stack](const FPCGEditorGraphDebugObjectItemPtr& InItem)
								{
									return (InItem->GetObject() == StackNode) && (*InItem->GetPCGStack() == Stack);
								});

							if (NodeItem)
							{
								CurrentItem = *NodeItem;
							}
							else
							{
								const FPCGStackFrame& NextStackFrame = StackFrames[NextStackIndex];

								if (const UPCGGraph* NextStackGraph = Cast<const UPCGGraph>(NextStackFrame.Object))
								{
									CurrentItem = NodeItems.Add_GetRef(MakeShared<FPCGEditorGraphDebugObjectItem_PCGSubgraph>(StackNode, NextStackGraph, Stack));
								}
								else if (NextStackFrame.LoopIndex != INDEX_NONE)
								{
									CurrentItem = NodeItems.Add_GetRef(MakeShared<FPCGEditorGraphDebugObjectItem_PCGLoopIndex>(StackNode, NextStackFrame.LoopIndex, Stack));
								}
							}

							StackIndex++;
						}
					}

					if (!CurrentItem)
					{
						break;
					}

					if (!CurrentItem->GetParent())
					{
						PreviousItem->AddChild(CurrentItem.ToSharedRef());
					}

					AllGraphItems.Add(CurrentItem);
					PreviousItem = CurrentItem;
				}
			}
		}
	}
	
	for (TPair<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>>& ActorItem : ActorItems)
	{	
		RootItems.Add(MoveTemp(ActorItem.Value));
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

TSharedRef<ITableRow> SPCGEditorGraphDebugObjectTree::MakeTreeRowWidget(FPCGEditorGraphDebugObjectItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew( STableRow<TSharedPtr<SPCGEditorGraphDebugObjectItemRow>>, InOwnerTable)
		[
			SNew(SPCGEditorGraphDebugObjectItemRow, InOwnerTable, InItem)
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

void SPCGEditorGraphDebugObjectTree::OnSelectionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, ESelectInfo::Type InSelectInfo) const
{
	if (!InItem)
	{
		PCGEditor.Pin()->SetComponentAndStackBeingInspected(nullptr, FPCGStack());
		return;
	}
	
	if (UPCGComponent* PCGComponent = InItem->GetPCGComponent())
	{
		if (const FPCGStack* PCGStack = InItem->GetPCGStack())
		{
			PCGEditor.Pin()->SetComponentAndStackBeingInspected(PCGComponent, *PCGStack);
		}
	}
}

void SPCGEditorGraphDebugObjectTree::OnSetExpansionRecursive(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInExpand)
{
	if (!InItem.IsValid() || !DebugObjectTreeView.IsValid())
	{
		return;
	}

	DebugObjectTreeView->SetItemExpansion(InItem, bInExpand);
	for (const FPCGEditorGraphDebugObjectItemPtr& ChildItem : InItem->GetChildren())
	{
		if (ChildItem.IsValid())
		{
			OnSetExpansionRecursive(ChildItem, bInExpand);
		}
	}
}

#undef LOCTEXT_NAMESPACE
