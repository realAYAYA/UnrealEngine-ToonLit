// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGeometryCollectionOutliner.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "PropertyInfoViewStyle.h"

#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "FractureEditorMode.h"
#include "ScopedTransaction.h"
#include "FractureSettings.h"
#include "FractureToolProperties.h"
#include "GeometryCollectionOutlinerDragDrop.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SGeometryCollectionOutliner)

#define LOCTEXT_NAMESPACE "ChaosEditor"

static FText GetTextFromInitialDynamicState(int32 InitialDynamicState)
{
	switch (InitialDynamicState)
	{
	case 0: return LOCTEXT("GCOutliner_InitialState_NoOverride_Text", "-");
	case 1: return LOCTEXT("GCOutliner_InitialState_Sleeping_Text", "Sleeping");
	case 2: return LOCTEXT("GCOutliner_InitialState_Kinematic_Text", "Kinematic");
	case 3: return LOCTEXT("GCOutliner_InitialState_Static_Text", "Static");
	default:
		return FText();
	}
}

static FText GetTextFromAnchored(bool bAnchored)
{
	if (bAnchored)
	{
		return LOCTEXT("GCOutliner_Anchored_Yes_Text", "Yes");
	}
	return LOCTEXT("GCOutliner_Anchored_No_Text", "-");
}

void FGeometryCollectionTreeItem::OnDragLeave(const FDragDropEvent& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = InDragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		if (Operation->IsOfType<FGeometryCollectionBoneDragDrop>())
		{
			TSharedPtr<FGeometryCollectionBoneDragDrop> GeometryCollectionBoneOp = InDragDropEvent.GetOperationAs<FGeometryCollectionBoneDragDrop>();
			const FSlateBrush* Icon = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			GeometryCollectionBoneOp->SetToolTip(FText(), Icon);
		}
	}
}

void FGeometryCollectionTreeItem::GenerateContextMenu(UToolMenu* Menu, SGeometryCollectionOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SGeometryCollectionOutliner>(Outliner.AsShared());

	FToolMenuSection& StateSection = Menu->AddSection("State");
	StateSection.AddSubMenu("FractureToolSetInitialDynamicStateMenu", NSLOCTEXT("Fracture", "FractureToolSetInitialDynamicStateMenu", "Initial Dynamic State"), FText(),
		FNewToolMenuDelegate::CreateLambda([&Outliner](UToolMenu* Menu)
		{
			const FName MenuEntryNames[] =
			{
				"NoOverride"
				"Sleeping",
				"Kinematic",
				"Static"
			};
			const int32 MenuEntryNamesCount = sizeof(MenuEntryNames) / sizeof(FName);

			FToolMenuSection& StateSection = Menu->AddSection("State");
			for (int32 Index = 0; Index < MenuEntryNamesCount; ++Index)
			{
				StateSection.AddMenuEntry(MenuEntryNames[Index], GetTextFromInitialDynamicState(Index), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SGeometryCollectionOutliner::SetInitialDynamicState, Index)));
			}
		}));

	FToolMenuSection& AnchoredSection = Menu->AddSection("Anchored");
	StateSection.AddSubMenu("FractureToolSetAnchoredMenu", NSLOCTEXT("Fracture", "FractureToolSetAnchoredMenu", "Anchored"), FText(),
		FNewToolMenuDelegate::CreateLambda([&Outliner](UToolMenu* Menu)
		{
			FToolMenuSection& AnchoredSection = Menu->AddSection("Anchored");
			{
				AnchoredSection.AddMenuEntry("Yes", LOCTEXT("GCOutliner_Anchored_Yes_ContextMenuText", "Yes"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SGeometryCollectionOutliner::SetAnchored, true)));
				AnchoredSection.AddMenuEntry("No", LOCTEXT("GCOutliner_Anchored_No_ContextMenuText", "No"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SGeometryCollectionOutliner::SetAnchored, false)));
			}
		}));
}

FColor FGeometryCollectionTreeItem::GetColorPerDepth(uint32 Depth)
{
	const FColor ColorsPerDepth[] =
	{
		FColor::Cyan,
		FColor::Orange,
		FColor::Emerald,
		FColor::Yellow,
	};
	const uint32 ColorPerDepthCount = sizeof(ColorsPerDepth) / sizeof(FColor);
	return ColorsPerDepth[Depth % ColorPerDepthCount];
}

void FGeometryCollectionTreeItemBone::OnDragEnter(FDragDropEvent const& InDragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = InDragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		if (Operation->IsOfType<FGeometryCollectionBoneDragDrop>())
		{
			TSharedPtr<FGeometryCollectionBoneDragDrop> GeometryCollectionBoneOp = InDragDropEvent.GetOperationAs<FGeometryCollectionBoneDragDrop>();
			
			UGeometryCollectionComponent* SourceComponent = GetComponent();
			FGeometryCollectionEdit GeometryCollectionEdit = SourceComponent->EditRestCollection();
			if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
			{
				FGeometryCollection* GeometryCollection = GeometryCollectionObject->GetGeometryCollection().Get();
				FText HoverText;
				bool bValid = GeometryCollectionBoneOp->ValidateDrop(GeometryCollection, BoneIndex, HoverText);
				const FSlateBrush* Icon = bValid ? FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				GeometryCollectionBoneOp->SetToolTip(HoverText, Icon);
			}
		}
	}
}

FReply FGeometryCollectionTreeItemBone::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		UGeometryCollectionComponent* SourceComponent = GetComponent();

		FGeometryCollectionEdit GeometryCollectionEdit = SourceComponent->EditRestCollection();
		if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
		{
			TSharedPtr<FGeometryCollection,ESPMode::ThreadSafe> GeometryCollection = GeometryCollectionObject->GetGeometryCollection();
			TArray<int32> SelectedBones = SourceComponent->GetSelectedBones();

			return FReply::Handled().BeginDragDrop(FGeometryCollectionBoneDragDrop::New(GeometryCollection, SelectedBones));
		}		
	}

	return FReply::Unhandled();
}

FReply FGeometryCollectionTreeItemBone::OnDrop(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid()) 
	{
		return FReply::Unhandled();
	}
	
	if (Operation->IsOfType<FGeometryCollectionBoneDragDrop>())
	{
		TSharedPtr<FGeometryCollectionBoneDragDrop> GeometryCollectionBoneDragDrop = StaticCastSharedPtr<FGeometryCollectionBoneDragDrop>(Operation);

		UGeometryCollectionComponent* SourceComponent = GetComponent();

		FGeometryCollectionEdit GeometryCollectionEdit = SourceComponent->EditRestCollection();
		if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
		{
			FGeometryCollection* GeometryCollection = GeometryCollectionObject->GetGeometryCollection().Get();
			if (GeometryCollectionBoneDragDrop->ReparentBones(GeometryCollection, BoneIndex))
			{
				ParentComponentItem->RegenerateChildren();
				ParentComponentItem->RequestTreeRefresh();
				ParentComponentItem->ExpandAll();
			}
		}		
	}
	
	return FReply::Unhandled();
}

UOutlinerSettings::UOutlinerSettings(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, ItemText(EOutlinerItemNameEnum::BoneIndex)
	, ColorByLevel(false)
	, ColumnMode(EOutlinerColumnMode::StateAndSize)
{}

TSharedRef<SWidget> SGeometryCollectionOutlinerRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == SGeometryCollectionOutlinerColumnID::Bone)
	{
		const TSharedPtr<SWidget> NameWidget = Item->MakeNameColumnWidget();
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.ShouldDrawWires(true)
				]
			+ SHorizontalBox::Slot()
				[
					NameWidget.ToSharedRef()
				];
	}
	if (ColumnName == SGeometryCollectionOutlinerColumnID::RelativeSize)
		return Item->MakeRelativeSizeColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::InitialState)
		return Item->MakeInitialStateColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::Anchored)
		return Item->MakeAnchoredColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::Damage)
		return Item->MakeDamagesColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::DamageThreshold)
		return Item->MakeDamageThresholdColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::Broken)
		return Item->MakeBrokenColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::PostBreakTime)
		return Item->MakePostBreakTimeColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::RemovalTime)
		return Item->MakeRemovalTimeColumnWidget();
	if (ColumnName == SGeometryCollectionOutlinerColumnID::ImportedCollisions)
		return Item->MakeImportedCollisionsColumnWidget();
	return Item->MakeEmptyColumnWidget();
}

void SGeometryCollectionOutliner::Construct(const FArguments& InArgs)
{
	BoneSelectionChangedDelegate = InArgs._OnBoneSelectionChanged;
	bPerformingSelection = false;

	HeaderRowWidget =
		SNew( SHeaderRow )
			.Visibility( EVisibility::Visible );
	RegenerateHeader();
	
	ChildSlot
	[
		SAssignNew(TreeView, STreeView<FGeometryCollectionTreeItemPtr>)
		.TreeItemsSource(reinterpret_cast<FGeometryCollectionTreeItemList*>(&RootNodes))
		.OnSelectionChanged(this, &SGeometryCollectionOutliner::OnSelectionChanged)
		.OnGenerateRow(this, &SGeometryCollectionOutliner::MakeTreeRowWidget)
		.OnGetChildren(this, &SGeometryCollectionOutliner::OnGetChildren)
		.OnContextMenuOpening(this, &SGeometryCollectionOutliner::OnOpenContextMenu)
		.AllowInvisibleItemSelection(true)
		.ShouldStackHierarchyHeaders(true)
		.OnGeneratePinnedRow(this, &SGeometryCollectionOutliner::OnGeneratePinnedRowWidget, true)
		.HighlightParentNodesForSelection(true)
		.OnSetExpansionRecursive(this, &SGeometryCollectionOutliner::ExpandRecursive)
		.HeaderRow(HeaderRowWidget)
	];
}

void SGeometryCollectionOutliner::RegenerateHeader()
{
	constexpr int32 CustomFillWidth = 2.0f;

	HeaderRowWidget->ClearColumns();

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::Bone)
		.DefaultLabel(LOCTEXT("GCOutliner_Column_Bone", "Bone"))
		.FillWidth(2.0f)
	);

	// then add the right customn one based on the selection
	UOutlinerSettings* OutlinerSettings = GetMutableDefault<UOutlinerSettings>();
	switch (OutlinerSettings->ColumnMode)
	{
	case EOutlinerColumnMode::StateAndSize:
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::RelativeSize)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_RelativeSize", "Relative Size"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_RelativeSize_ToolTip", "Relative size ( Used for size specific data )"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::InitialState)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_InitialState", "Initial State"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_InitialState_ToolTip", "Initial state override"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::Anchored)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_Anchored", "Anchored"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_Anchored_ToolTip", "Anchored"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		break;

	case EOutlinerColumnMode::Damage:
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::Damage)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_MaxDamage", "Max damage"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_MaxDamage_ToolTip", "Maximum amount of damage recorded ( through collision )"))
	 			.HAlignHeader(EHorizontalAlignment::HAlign_Center)
	 			.FillWidth(CustomFillWidth)
		);
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::DamageThreshold)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_DamageThreshold", "Damage Threshold"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_DamageThreshold_ToolTip", "Current damage threshold ( equivalent to internal strain )"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::Broken)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_Broken", "Broken"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_Broken_ToolTip", "Whether the piece has broken off"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(1.0)
		);
		break;
	case EOutlinerColumnMode::Removal:
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::PostBreakTime)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_PostBreakTime", "Post Break Time"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_PostBreakTime_ToolTip", "Min/Max time after break until removal starts"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::RemovalTime)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_RemovalTime", "Removal Time"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_RemovalTime_ToolTip", "Min/Max time for removal"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		break;
		
	case EOutlinerColumnMode::Collision:
		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(SGeometryCollectionOutlinerColumnID::ImportedCollisions)
				.DefaultLabel(LOCTEXT("GCOutliner_Column_ImportedCollisions", "Imported Collisions"))
				.DefaultTooltip(LOCTEXT("GCOutliner_Column_ImportedCollisions_ToolTip", "Status of imported Collision [available, used, nothing])"))
				.HAlignHeader(EHorizontalAlignment::HAlign_Center)
				.FillWidth(CustomFillWidth)
		);
		break;
	}
}

void SGeometryCollectionOutliner::RegenerateItems()
{
	TreeView->RebuildList();
}

TSharedRef<ITableRow> SGeometryCollectionOutliner::MakeTreeRowWidget(FGeometryCollectionTreeItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return InItem->MakeTreeRowWidget(InOwnerTable);
}

TSharedRef<ITableRow> SGeometryCollectionOutliner::OnGeneratePinnedRowWidget(FGeometryCollectionTreeItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable, bool bPinned)
{
	return InItem->MakeTreeRowWidget(InOwnerTable, true);
}

void SGeometryCollectionOutliner::OnGetChildren(FGeometryCollectionTreeItemPtr InItem, TArray<FGeometryCollectionTreeItemPtr>& OutChildren)
{
	InItem->GetChildren(OutChildren);
}

TSharedPtr<SWidget> SGeometryCollectionOutliner::OnOpenContextMenu()
{
	FGeometryCollectionTreeItemList SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);

	if (SelectedItems.Num())
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		static const FName MenuName = "SGeometryCollectionOutliner.GeometryCollectionOutlinerContextMenu";
		if (!ToolMenus->IsMenuRegistered(MenuName))
		{
			ToolMenus->RegisterMenu(MenuName);
		}

		// Build up the menu for a selection
		FToolMenuContext Context;
		UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, Context);
		SelectedItems[0]->GenerateContextMenu(Menu, *this);
		return ToolMenus->GenerateWidget(Menu);
	}

	return TSharedPtr<SWidget>();
}

void SGeometryCollectionOutliner::UpdateGeometryCollection()
{
	TreeView->RequestTreeRefresh();
	ExpandAll();
}

void SGeometryCollectionOutliner::SetComponents(const TArray<UGeometryCollectionComponent*>& InNewComponents)
{
	// Clear the cached Tree ItemSelection without affecting the SelectedBones as 
	// we want to refresh the tree selection using selected bones
	TGuardValue<bool> ExternalSelectionGuard(bPerformingSelection, true);
	TreeView->ClearSelection();

	RootNodes.Empty();

	for (UGeometryCollectionComponent* Component : InNewComponents)
	{
		if (Component->GetRestCollection() && IsValidChecked(Component->GetRestCollection()))
		{
			RootNodes.Add(MakeShared<FGeometryCollectionTreeItemComponent>(Component, TreeView));
			TArray<int32> SelectedBones = Component->GetSelectedBones();
			SetBoneSelection(Component, SelectedBones, false);
		}
	}

	TreeView->RequestTreeRefresh();
	ExpandAll();
}

void SGeometryCollectionOutliner::ExpandAll()
{
	for (TSharedPtr<FGeometryCollectionTreeItemComponent> ItemPtr : RootNodes)
	{
		ItemPtr->ExpandAll();
	}
}

void SGeometryCollectionOutliner::ExpandRecursive(TSharedPtr<FGeometryCollectionTreeItem> ItemPtr, bool bInExpansionState) const
{
	TreeView->SetItemExpansion(ItemPtr, bInExpansionState);

	FGeometryCollectionTreeItemList ItemChildren;
	ItemPtr->GetChildren(ItemChildren);
	for (auto& Child : ItemChildren)
	{
		ExpandRecursive(Child, bInExpansionState);
	}
}

void SGeometryCollectionOutliner::SetHistogramSelection(UGeometryCollectionComponent* RootComponent, TArray<int32>& SelectedBones)
{
	// Find the matching component
	for (TSharedPtr<FGeometryCollectionTreeItemComponent> RootNode : RootNodes)
	{
		if (RootNode->GetComponent() == RootComponent)
		{
			// Copy the histogram selection.
			RootNode->SetHistogramSelection(SelectedBones);
			RootNode->RegenerateChildren();
			TreeView->RequestTreeRefresh();
			ExpandAll();
			return;
		}
	}
}

int32 SGeometryCollectionOutliner::GetBoneSelectionCount() const
{
	return TreeView->GetSelectedItems().Num();
}

void SGeometryCollectionOutliner::SetBoneSelection(UGeometryCollectionComponent* RootComponent, const TArray<int32>& InSelection, bool bClearCurrentSelection, int32 FocusBoneIdx)
{
	TGuardValue<bool> ExternalSelectionGuard(bPerformingSelection, true);

	if (bClearCurrentSelection)
	{
		TreeView->ClearSelection();
	}

	bool bFirstSelection = true;

	FGeometryCollectionTreeItemList NewSelection;
	for(auto& RootNode : RootNodes)
	{
		if (RootNode->GetComponent() == RootComponent)
		{
			for(int32 BoneIndex : InSelection)
			{
				FGeometryCollectionTreeItemPtr Item = RootNode->GetItemFromBoneIndex(BoneIndex);
				if (ensure(Item.IsValid()))
				{
					if (bFirstSelection && FocusBoneIdx == BoneIndex)
					{
						TreeView->RequestScrollIntoView(Item);
						bFirstSelection = false;
					}
					NewSelection.Add(Item);
				}
			}
			break;
		}
	}
	TreeView->SetItemSelection(NewSelection, true);
}

void SGeometryCollectionOutliner::OnSelectionChanged(FGeometryCollectionTreeItemPtr Item, ESelectInfo::Type SelectInfo)
{
	if(!bPerformingSelection && BoneSelectionChangedDelegate.IsBound())
	{
		TMap<UGeometryCollectionComponent*, TArray<int32>> ComponentToBoneSelectionMap;

		ComponentToBoneSelectionMap.Reserve(RootNodes.Num());

		// Create an entry for each component in the tree.  If the component has no selected bones then we return an empty array to signal that the selection should be cleared
		for (auto& Root : RootNodes)
		{
			ComponentToBoneSelectionMap.Add(Root->GetComponent(), TArray<int32>());
		}

		if (Item == nullptr)
		{
			TreeView->ClearSelection();
		}

		FGeometryCollectionTreeItemList SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		FScopedTransaction Transaction(FractureTransactionContexts::SelectBoneContext, LOCTEXT("SelectGeometryCollectionBoneTransaction", "Select Bone"), Item != nullptr ? Item->GetComponent() : nullptr);
		for(auto& SelectedItem : SelectedItems)
		{
			if (SelectedItem->GetBoneIndex() != INDEX_NONE)
			{
				TArray<int32>& SelectedBones = ComponentToBoneSelectionMap.FindChecked(SelectedItem->GetComponent());
				SelectedBones.Add(SelectedItem->GetBoneIndex());
				SelectedItem->GetComponent()->Modify();
			}
		}
		// Fire off the delegate for each component
		for (auto& SelectionPair : ComponentToBoneSelectionMap)
		{
			BoneSelectionChangedDelegate.Execute(SelectionPair.Key, SelectionPair.Value);
		}
	}
}

void SGeometryCollectionOutliner::SetInitialDynamicState(int32 InDynamicState)
{
	UFractureToolSetInitialDynamicState::SetSelectedInitialDynamicState(InDynamicState);
	RegenerateItems();
}

void SGeometryCollectionOutliner::SetAnchored(bool bAnchored)
{
	// todo : eventually move this to a more central place ( tools ?)  
	TSet<UGeometryCollectionComponent*> GeomCompSelection;
	UFractureToolSetInitialDynamicState::GetSelectedGeometryCollectionComponents(GeomCompSelection);
	for (UGeometryCollectionComponent* GeometryCollectionComponent : GeomCompSelection)
	{
		FGeometryCollectionEdit GCEdit = GeometryCollectionComponent->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysics, true /*bShapeIsUnchanged*/);
		if (UGeometryCollection* GCObject = GCEdit.GetRestCollection())
		{
			TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GCObject->GetGeometryCollection();
			if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
			{
				Chaos::Facades::FCollectionAnchoringFacade AnchoringFacade(*GeometryCollection);
				if (!AnchoringFacade.HasAnchoredAttribute())
				{
					AnchoringFacade.AddAnchoredAttribute();
				}
				AnchoringFacade.SetAnchored(GeometryCollectionComponent->GetSelectedBones(), bAnchored);
			}
		}
	}
	RegenerateItems();
}

TSharedRef<ITableRow> FGeometryCollectionTreeItemComponent::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, bool bNoExtraColumn)
{
	FString ActorName = Component.IsValid()? Component->GetOwner()->GetActorLabel(): FString();
	FString ComponentName = Component.IsValid()? Component->GetClass()->GetFName().ToString() : FString();

	return SNew(STableRow<FGeometryCollectionTreeItemPtr>, InOwnerTable)
		.Content()
		[
			SNew(STextBlock).Text(FText::FromString(ActorName + '.' + ComponentName))
		];
}

void FGeometryCollectionTreeItemComponent::GetChildren(FGeometryCollectionTreeItemList& OutChildren)
{
	OutChildren = MyChildren;
}

FGeometryCollectionTreeItemPtr FGeometryCollectionTreeItemComponent::GetItemFromBoneIndex(int32 BoneIndex) const
{
	for (auto& Pair : NodesMap)
	{
		if (Pair.Value->GetBoneIndex() == BoneIndex)
		{
			return Pair.Value;
		}
	}

	return FGeometryCollectionTreeItemPtr();
}

void FGeometryCollectionTreeItemComponent::GetChildrenForBone(FGeometryCollectionTreeItemBone& BoneItem, FGeometryCollectionTreeItemList& OutChildren) const
{
	if (!Component.IsValid())
	{
		return;
	}
	if(const UGeometryCollection* RestCollection = Component->GetRestCollection())
	{
		if (FGeometryCollection* Collection = RestCollection->GetGeometryCollection().Get())
		{
			if (!Collection->HasAttribute("GUID", "Transform"))
			{
				GeometryCollection::GenerateTemporaryGuids(Collection);
			}

			if (const int32* BoneIndex = GuidIndexMap.Find(BoneItem.GetGuid()))
			{
				if (!ensure(*BoneIndex >= 0 && *BoneIndex < Collection->NumElements(FGeometryCollection::TransformGroup)))
				{
					return;
				}
				const TManagedArray<TSet<int32>>& Children = Collection->Children;
				const TManagedArray<FGuid>& Guids = Collection->GetAttribute<FGuid>("GUID", "Transform");
				for (auto ChildIndex : Children[*BoneIndex])
				{
					FGuid ChildGuid = Guids[ChildIndex];
					FGeometryCollectionTreeItemPtr ChildPtr = NodesMap.FindRef(ChildGuid);
					if (ChildPtr.IsValid())
					{
						OutChildren.Add(ChildPtr);
					}
				}
			}
		}
	}
}

bool FGeometryCollectionTreeItemComponent::HasChildrenForBone(const FGeometryCollectionTreeItemBone& BoneItem) const
{
	if (!Component.IsValid())
	{
		return false;
	}
	if(const UGeometryCollection* RestCollection = Component->GetRestCollection())
	{
		if (const FGeometryCollection* Collection = RestCollection->GetGeometryCollection().Get())
		{
			if (const int32* BoneIndex = GuidIndexMap.Find(BoneItem.GetGuid()))
			{
				return Collection->Children[(*BoneIndex)].Num() > 0;
			}
		}
	}
	return false;
}

FText FGeometryCollectionTreeItemComponent::GetDisplayNameForBone(const FGuid& Guid) const
{
	if (!Component.IsValid())
	{
		return LOCTEXT("BoneNotFound", "Bone Not Found, Invalid Geometry Collection");
	}
	if (const UGeometryCollection* RestCollection = Component->GetRestCollection())
	{
		if (FGeometryCollection* Collection = RestCollection->GetGeometryCollection().Get())
		{
			const TManagedArray<FString>& BoneNames = Collection->BoneName;

			if (const int32* BoneIndex = GuidIndexMap.Find(Guid))
			{
				if (*BoneIndex < BoneNames.Num())
				{
					return FText::FromString(BoneNames[*BoneIndex]);
				}
				else
				{
					return FText::Format(LOCTEXT("BoneNameNotFound", "Bone Name Not Found: Index {0}"), (*BoneIndex));
				}
			}
		}
	}
	return LOCTEXT("BoneNotFound", "Bone Not Found, Invalid Geometry Collection");
}

void FGeometryCollectionTreeItemComponent::ExpandAll()
{
	TreeView->SetItemExpansion(AsShared(), true);

	for (auto& Elem : NodesMap)
	{
	    TreeView->SetItemExpansion(Elem.Value, true);
	}
}

void FGeometryCollectionTreeItemComponent::RegenerateChildren()
{
	if(Component.IsValid() && Component->GetRestCollection())
	{
		//@todo Fracture:  This is potentially very expensive to refresh with giant trees
		FGeometryCollection* Collection = Component->GetRestCollection()->GetGeometryCollection().Get();

		NodesMap.Empty();
		GuidIndexMap.Empty();
		MyChildren.Empty();

		if (Collection)
		{
			int32 NumElements = Collection->NumElements(FGeometryCollection::TransformGroup);

			GeometryCollection::GenerateTemporaryGuids(Collection, 0, true);
			const TManagedArray<FGuid>& Guids = Collection->GetAttribute<FGuid>("GUID", "Transform");
			const TManagedArray<int32>& Parents = Collection->Parent;
			// const TManagedArray<FString>& BoneNames = CollectionPtr->BoneName;

			RootIndex = FGeometryCollection::Invalid;

			// Add a sub item to the outliner tree for each of the bones/chunks in this GeometryCollection
			for (int32 Index = 0; Index < NumElements; Index++)
			{
				if (FilterBoneIndex(Index))
				{
					TSharedRef<FGeometryCollectionTreeItemBone> NewItem = MakeShared<FGeometryCollectionTreeItemBone>(Guids[Index], Index, this);
					if (Parents[Index] == RootIndex)
					{
						// The actual children directly beneath this node are the ones without a parent.  The rest are children of children
						MyChildren.Add(NewItem);
					}

					NodesMap.Add(Guids[Index], NewItem);
					GuidIndexMap.Add(Guids[Index], Index);
				}			
			}

		}
	}
}

void FGeometryCollectionTreeItemComponent::RequestTreeRefresh()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void FGeometryCollectionTreeItemComponent::SetHistogramSelection(TArray<int32>& SelectedBones)
{
	HistogramSelection = SelectedBones;
}

bool FGeometryCollectionTreeItemComponent::FilterBoneIndex(int32 BoneIndex) const
{
	if (Component.IsValid())
	{
		const FGeometryCollection* Collection = Component->GetRestCollection()->GetGeometryCollection().Get();
		const TManagedArray<int32>& SimTypes = Collection->SimulationType;
		bool bHasChildren = Collection->Children[BoneIndex].Num() > 0;

		if (SimTypes[BoneIndex] != FGeometryCollection::ESimulationTypes::FST_Clustered)
		{
			// We only display cluster nodes deeper than the view level.
			UFractureSettings* FractureSettings = GetMutableDefault<UFractureSettings>();

			if (FractureSettings->FractureLevel >= 0 && Collection->HasAttribute("Level", FTransformCollection::TransformGroup))
			{
				const TManagedArray<int32>& Level = Collection->GetAttribute<int32>("Level", FTransformCollection::TransformGroup);
				int32 BoneLevel = Level[BoneIndex];
				// bone is not at the right level itself and doesn't have child(ren) at the right level
				if (BoneLevel != FractureSettings->FractureLevel && (!bHasChildren || BoneLevel + 1 != FractureSettings->FractureLevel))
				{
					return false;
				}
			}

			// If anything is selected int the Histogram, we filter by that selection.
			if (HistogramSelection.Num() > 0)
			{
				if (!HistogramSelection.Contains(BoneIndex))
				{
					return false;
				}
			}		
		}
	}
	return true;	
}

void FGeometryCollectionTreeItemBone::UpdateItemFromCollection()
{
	const int32 ItemBoneIndex = GetBoneIndex();

	constexpr FLinearColor InvalidColor(0.1f, 0.1f, 0.1f);
	
	ItemColor = InvalidColor;
	RelativeSize = 0;
	Damage = 0;
	DamageThreshold = 0;
	Broken = false;
	InitialState = INDEX_NONE;
	Anchored = false;
	RemoveOnBreakAvailable = false;
	RemoveOnBreak = FVector4f{-1};
	ImportedCollisionsAvailable = false;
	ImportedCollisionsUsed = false;
	
	// Name / ItemText
	UOutlinerSettings* OutlinerSettings = GetMutableDefault<UOutlinerSettings>();
	ItemText = ParentComponentItem->GetDisplayNameForBone(Guid);
	if (OutlinerSettings->ItemText == EOutlinerItemNameEnum::BoneIndex)
	{
		ItemText = FText::FromString(FString::FromInt(ItemBoneIndex));
	}

	// Set color according to simulation type
	const UGeometryCollectionComponent* Component = ParentComponentItem->GetComponent();
	if (Component)
	{
		const FDamageCollector* Collector = Component->GetRunTimeDataCollector();
		const UGeometryCollection* RestCollection = Component->GetRestCollection();
		if (RestCollection && IsValidChecked(RestCollection))
		{
			TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = RestCollection->GetGeometryCollection();
			const TManagedArray<int32>& SimulationType = GeometryCollectionPtr->SimulationType;
			const TManagedArray<float>* RelativeSizes = GeometryCollectionPtr->FindAttribute<float>("Size", FTransformCollection::TransformGroup);
			const TManagedArray<FVector4f>* RemoveOnBreakArray = GeometryCollectionPtr->FindAttribute<FVector4f>("RemoveOnBreak", FTransformCollection::TransformGroup);

			using FImplicitGeom = FGeometryDynamicCollection::FSharedImplicit;
			const TManagedArray<FImplicitGeom>* ExternalCollisions = GeometryCollectionPtr->FindAttribute<FImplicitGeom>("ExternalCollisions", FTransformCollection::TransformGroup);

			if (ensure(ItemBoneIndex >= 0 && ItemBoneIndex < SimulationType.Num()))
			{
				const bool bHasLevelAttribute = GeometryCollectionPtr->HasAttribute("Level", FGeometryCollection::TransformGroup);
				if (bHasLevelAttribute && OutlinerSettings->ColorByLevel)
				{
					const TManagedArray<int32>& Level = GeometryCollectionPtr->GetAttribute<int32>("Level", FTransformCollection::TransformGroup);
					ItemColor = FSlateColor(FGeometryCollectionTreeItem::GetColorPerDepth((uint32)Level[ItemBoneIndex]));
				}
				else
				{
					switch (SimulationType[ItemBoneIndex])
					{
					case FGeometryCollection::ESimulationTypes::FST_None:
						ItemColor = FLinearColor::Green;
						break;

					case FGeometryCollection::ESimulationTypes::FST_Rigid:
						if (GeometryCollectionPtr->IsVisible(ItemBoneIndex))
						{
							ItemColor = FSlateColor::UseForeground();
						}
						else
						{
							ItemColor = InvalidColor;
						}
						break;

					case FGeometryCollection::ESimulationTypes::FST_Clustered:
						ItemColor = FSlateColor(FColor::Cyan);
						break;

					default:
						ensureMsgf(false, TEXT("Invalid Geometry Collection simulation type encountered."));
						break;
					}
				}

				Chaos::Facades::FCollectionAnchoringFacade AnchoringFacade(*GeometryCollectionPtr);
				InitialState = static_cast<int32>(AnchoringFacade.GetInitialDynamicState(ItemBoneIndex));
				if (AnchoringFacade.HasAnchoredAttribute())
				{
					Anchored = AnchoringFacade.IsAnchored(ItemBoneIndex);
				}

				if (RelativeSizes)
				{
					RelativeSize = (*RelativeSizes)[ItemBoneIndex];
				}

				if (Collector)
				{
					const FDamageCollector::FDamageData& DamageData = (*Collector)[ItemBoneIndex];
					Damage = DamageData.MaxDamages;
					DamageThreshold = DamageData.DamageThreshold;
					Broken = DamageData.bIsBroken;
				}
				RemoveOnBreakAvailable = (RemoveOnBreakArray != nullptr); 
				if (RemoveOnBreakAvailable)
				{
					RemoveOnBreak = (*RemoveOnBreakArray)[ItemBoneIndex];
				}

				ImportedCollisionsUsed = RestCollection->bImportCollisionFromSource;
				if (ExternalCollisions)
				{
					ImportedCollisionsAvailable = (*ExternalCollisions)[ItemBoneIndex] != nullptr;
				}

				IsCluster = (GeometryCollectionPtr->Children[ItemBoneIndex].Num() > 0);
			}
		}
	}
}

TSharedRef<ITableRow> FGeometryCollectionTreeItemBone::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, bool bIsPinned)
{
	UpdateItemFromCollection();
	if (bIsPinned)
	{
		return SNew(STableRow<FGeometryCollectionTreeItemPtr>, InOwnerTable)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.Padding(2.0f, 4.0f)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ItemText)
						.ColorAndOpacity(ItemColor)
					]
			];
	}
	return SNew(SGeometryCollectionOutlinerRow, InOwnerTable, SharedThis(this));
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeNameColumnWidget() const
{
	return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(ItemText)
					.ColorAndOpacity(ItemColor)
				];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeRelativeSizeColumnWidget() const
{
	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(2)
				.SetMaximumFractionalDigits(2);
	
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(RelativeSize, &FormatOptions))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeDamagesColumnWidget() const
{
	static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(1)
			.SetMaximumFractionalDigits(1);
	
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Damage, &FormatOptions))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeDamageThresholdColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(DamageThreshold))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeBrokenColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Broken? TEXT("Broken"): TEXT("")))
				.ColorAndOpacity(ItemColor)
			];
}

static FText FormatRemoveOnBreakTimeData(bool bDataAvailable, bool bEnabled, const FVector2f& Timer)
{
	if (bDataAvailable)
	{
		if (bEnabled)
		{
			return FText::Format(LOCTEXT("GCOutliner_RemoveOnBreakTimer_Format", "[{0}s, {1}s]"),Timer.X, Timer.Y);
		}
		return FText(LOCTEXT("GCOutliner_RemoveOnBreakTimer_Empty", "[-, -]"));
	}
	return FText();
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakePostBreakTimeColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FormatRemoveOnBreakTimeData(RemoveOnBreakAvailable, RemoveOnBreak.IsEnabled(), RemoveOnBreak.GetBreakTimer()))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeRemovalTimeColumnWidget() const
{
	const bool EnableRemovalTimer = RemoveOnBreak.IsEnabled() && !(IsCluster && RemoveOnBreak.GetClusterCrumbling());
	const bool ClusterCrumbling = RemoveOnBreak.IsEnabled() && (IsCluster && RemoveOnBreak.GetClusterCrumbling());
	FText RemovalTimeText; 
	if (ClusterCrumbling)
	{
		RemovalTimeText = FText(LOCTEXT("GCOutliner_ClusterCrumbling_Text", "Cluster Crumbling"));
	}
	else
	{
		RemovalTimeText = FormatRemoveOnBreakTimeData(RemoveOnBreakAvailable, EnableRemovalTimer,  RemoveOnBreak.GetRemovalTimer());
	}
	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
		.Padding(12.f, 0.f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(RemovalTimeText)
			.ColorAndOpacity(ItemColor)
		];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeImportedCollisionsColumnWidget() const
{
	FText ImportedCollisionText = LOCTEXT("GCOutliner_ImportedCollision_NotAvailable", "-");
	if (ImportedCollisionsAvailable)
	{
		if (ImportedCollisionsUsed)
		{
			ImportedCollisionText = LOCTEXT("GCOutliner_ImportedCollision_Used", "Used");
		}
		else
		{
			ImportedCollisionText = LOCTEXT("GCOutliner_ImportedCollision_Available", "Available");
		}
	}
	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
		.Padding(12.f, 0.f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(ImportedCollisionText)
			.ColorAndOpacity(ItemColor)
		];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeInitialStateColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			[
				SNew(STextBlock)
				.Text(GetTextFromInitialDynamicState(InitialState))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeAnchoredColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
	[
				SNew(STextBlock)
				.Text(GetTextFromAnchored(Anchored))
				.ColorAndOpacity(ItemColor)
			];
}

TSharedRef<SWidget> FGeometryCollectionTreeItemBone::MakeEmptyColumnWidget() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.Padding(12.f, 0.f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(ItemColor)
			];
}

void FGeometryCollectionTreeItemBone::GetChildren(FGeometryCollectionTreeItemList& OutChildren)
{
	ParentComponentItem->GetChildrenForBone(*this, OutChildren);
}

bool FGeometryCollectionTreeItemBone::HasChildren() const 
{
	return ParentComponentItem->HasChildrenForBone(*this);
}


#undef LOCTEXT_NAMESPACE // "ChaosEditor"
