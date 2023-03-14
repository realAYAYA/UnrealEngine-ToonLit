// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDisplayClusterColorGradingDetailView.h"

#include "SDisplayClusterColorGradingDetailTreeRow.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "PropertyEditor/Private/DetailTreeNode.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

#include "PropertyEditor/Private/PropertyNode.h"

void FDisplayClusterColorGradingDetailTreeItem::Initialize(const FOnFilterDetailTreeNode& NodeFilter)
{
	if (DetailTreeNode.IsValid())
	{
		PropertyHandle = DetailTreeNode.Pin()->CreatePropertyHandle();

		TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
		DetailTreeNode.Pin()->GetChildren(ChildNodes);

		for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
		{
			bool bShouldDisplayNode = true;
			if (NodeFilter.IsBound())
			{
				bShouldDisplayNode = NodeFilter.Execute(ChildNode);
			}

			if (bShouldDisplayNode)
			{
				TSharedPtr<FDetailTreeNode> CastChildNode = StaticCastSharedRef<FDetailTreeNode>(ChildNode);
				TSharedRef<FDisplayClusterColorGradingDetailTreeItem> ChildItem = MakeShared<FDisplayClusterColorGradingDetailTreeItem>(CastChildNode);
				ChildItem->Parent = SharedThis(this);
				ChildItem->Initialize(NodeFilter);

				Children.Add(ChildItem);
			}
		}
	}    
}

void FDisplayClusterColorGradingDetailTreeItem::GetChildren(TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>>& OutChildren) const
{
	OutChildren.Reset(Children.Num());
	OutChildren.Append(Children);
}

TWeakPtr<IDetailTreeNode> FDisplayClusterColorGradingDetailTreeItem::GetDetailTreeNode() const
{
	return DetailTreeNode;
}

FName FDisplayClusterColorGradingDetailTreeItem::GetNodeName() const
{
	if (DetailTreeNode.IsValid())
	{
		return DetailTreeNode.Pin()->GetNodeName();
	}

	return NAME_None;
}

bool FDisplayClusterColorGradingDetailTreeItem::ShouldBeExpanded() const
{
	if (DetailTreeNode.IsValid())
	{
		return DetailTreeNode.Pin()->ShouldBeExpanded();
	}

	return false;
}

void FDisplayClusterColorGradingDetailTreeItem::OnItemExpansionChanged(bool bIsExpanded, bool bShouldSaveState)
{
	if (DetailTreeNode.IsValid())
	{
		DetailTreeNode.Pin()->OnItemExpansionChanged(bIsExpanded, bShouldSaveState);
	}
}

bool FDisplayClusterColorGradingDetailTreeItem::IsResetToDefaultVisible() const
{
	if (PropertyHandle.IsValid())
	{
		if (PropertyHandle->HasMetaData("NoResetToDefault") || PropertyHandle->GetInstanceMetaData("NoResetToDefault"))
		{
			return false;
		}

		return PropertyHandle->CanResetToDefault();
	}

	return false;
}

void FDisplayClusterColorGradingDetailTreeItem::ResetToDefault()
{
	if (PropertyHandle.IsValid())
	{
		PropertyHandle->ResetToDefault();
	}
}

TAttribute<bool> FDisplayClusterColorGradingDetailTreeItem::IsPropertyEditingEnabled() const
{
	if (DetailTreeNode.IsValid())
	{
		return DetailTreeNode.Pin()->IsPropertyEditingEnabled();
	}

	return false;
}

bool FDisplayClusterColorGradingDetailTreeItem::IsCategory() const
{
	if (DetailTreeNode.IsValid())
	{
		if (DetailTreeNode.Pin()->GetNodeType() == EDetailNodeType::Category)
		{
			return true;
		}
	}
	
	return false;
}

bool FDisplayClusterColorGradingDetailTreeItem::IsItem() const
{
	if (DetailTreeNode.IsValid())
	{
		return DetailTreeNode.Pin()->GetNodeType() == EDetailNodeType::Item;
	}

	return false;
}

bool FDisplayClusterColorGradingDetailTreeItem::IsReorderable() const
{
	if (PropertyHandle.IsValid())
	{
		if (TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle())
		{
			const bool bIsParentAnArray = ParentHandle->AsArray().IsValid();
			const bool bIsParentArrayReorderable = !ParentHandle->HasMetaData(TEXT("EditFixedOrder")) && !ParentHandle->HasMetaData(TEXT("ArraySizeEnum"));
			
			return bIsParentAnArray && bIsParentArrayReorderable && !PropertyHandle->IsEditConst() && !FApp::IsGame();
		}
	}

	return false;
}

bool FDisplayClusterColorGradingDetailTreeItem::IsCopyable() const
{
	if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
	{
		static const FName DisableCopyPasteMetaDataName("DisableCopyPaste");

		// Check to see if this property or any of its parents have the DisableCopyPaste metadata
		TSharedPtr<IPropertyHandle> CurrentPropertyHandle = PropertyHandle;
		while (CurrentPropertyHandle.IsValid())
		{
			if (CurrentPropertyHandle->HasMetaData(DisableCopyPasteMetaDataName))
			{
				return false;
			}

			CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle();
		}

		return true;
	}

	return false;
}

void FDisplayClusterColorGradingDetailTreeItem::GenerateDetailWidgetRow(FDetailWidgetRow& OutDetailWidgetRow) const
{
	if (DetailTreeNode.IsValid())
	{
		DetailTreeNode.Pin()->GenerateStandaloneWidget(OutDetailWidgetRow);
	}
}

void SDisplayClusterColorGradingDetailView::Construct(const FArguments& InArgs)
{
	PropertyRowGeneratorSource = InArgs._PropertyRowGeneratorSource;
	OnFilterDetailTreeNode = InArgs._OnFilterDetailTreeNode;

	ColumnSizeData.SetValueColumnWidth(0.5f);
	ColumnSizeData.SetRightColumnMinWidth(22);

	UpdateTreeNodes();

	TSharedRef<SScrollBar> ExternalScrollbar = SNew(SScrollBar);
	ExternalScrollbar->SetVisibility(TAttribute<EVisibility>(this, &SDisplayClusterColorGradingDetailView::GetScrollBarVisibility));

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	VerticalBox->AddSlot()
	.FillHeight(1)
	.Padding(0)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(DetailTree, SDetailTree)
			.TreeItemsSource(&RootTreeNodes)
			.OnGenerateRow(this, &SDisplayClusterColorGradingDetailView::GenerateNodeRow)
			.OnGetChildren(this, &SDisplayClusterColorGradingDetailView::GetChildrenForNode)
			.OnSetExpansionRecursive(this, &SDisplayClusterColorGradingDetailView::OnSetExpansionRecursive)
			.OnRowReleased(this, &SDisplayClusterColorGradingDetailView::OnRowReleased)
			.OnExpansionChanged(this, &SDisplayClusterColorGradingDetailView::OnExpansionChanged)
			.SelectionMode(ESelectionMode::None)
			.HandleDirectionalNavigation(false)
			.AllowOverscroll(EAllowOverscroll::Yes)
			.ExternalScrollbar(ExternalScrollbar)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			[
				ExternalScrollbar
			]
		]
	];

	ChildSlot
	[
		VerticalBox
	];
}

void SDisplayClusterColorGradingDetailView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (TreeItemsToSetExpansionState.Num() > 0)
	{
		for (const TPair<TWeakPtr<FDisplayClusterColorGradingDetailTreeItem>, bool>& Pair : TreeItemsToSetExpansionState)
		{
			if (TSharedPtr<FDisplayClusterColorGradingDetailTreeItem> DetailTreeItem = Pair.Key.Pin())
			{
				DetailTree->SetItemExpansion(DetailTreeItem.ToSharedRef(), Pair.Value);
			}
		}

		TreeItemsToSetExpansionState.Empty();
	}
}

void SDisplayClusterColorGradingDetailView::Refresh()
{
	UpdateTreeNodes();
	DetailTree->RebuildList();
}

void SDisplayClusterColorGradingDetailView::SaveExpandedItems()
{
	TSet<FString> ObjectTypes;
	if (PropertyRowGeneratorSource.IsValid())
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyRowGeneratorSource->GetSelectedObjects();

		for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
		{
			if (Object.IsValid())
			{
				ObjectTypes.Add(Object->GetClass()->GetName());
			}
		}
	}

	FString ExpandedDetailNodesString;
	for (const FString& DetailNode : ExpandedDetailNodes)
	{
		ExpandedDetailNodesString += DetailNode;
		ExpandedDetailNodesString += TEXT(",");
	}

	for (const FString& ObjectType : ObjectTypes)
	{
		if (!ExpandedDetailNodesString.IsEmpty())
		{
			GConfig->SetString(TEXT("DisplayClusterColorGradingDetailsExpansion"), *ObjectType, *ExpandedDetailNodesString, GEditorPerProjectIni);
		}
		else
		{
			// If the expanded nodes string is empty but the saved expanded state is not, we want to save the empty string
			FString SavedExpandedDetailNodesString;
			GConfig->GetString(TEXT("DisplayClusterColorGradingDetailsExpansion"), *ObjectType, SavedExpandedDetailNodesString, GEditorPerProjectIni);

			if (!SavedExpandedDetailNodesString.IsEmpty())
			{
				GConfig->SetString(TEXT("DisplayClusterColorGradingDetailsExpansion"), *ObjectType, *ExpandedDetailNodesString, GEditorPerProjectIni);
			}
		}
	}
}

void SDisplayClusterColorGradingDetailView::RestoreExpandedItems()
{
	TSet<FString> ObjectTypes;
	if (PropertyRowGeneratorSource.IsValid())
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyRowGeneratorSource->GetSelectedObjects();

		for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
		{
			if (Object.IsValid())
			{
				ObjectTypes.Add(Object->GetClass()->GetName());
			}
		}
	}

	for (const FString& ObjectType : ObjectTypes)
	{
		FString SavedExpandedDetailNodesString;
		GConfig->GetString(TEXT("DisplayClusterColorGradingDetailsExpansion"), *ObjectType, SavedExpandedDetailNodesString, GEditorPerProjectIni);
		TArray<FString> SavedExpandedDetailNodes;
		SavedExpandedDetailNodesString.ParseIntoArray(SavedExpandedDetailNodes, TEXT(","), true);

		ExpandedDetailNodes.Append(SavedExpandedDetailNodes);
	}
}

void SDisplayClusterColorGradingDetailView::UpdateTreeNodes()
{
	RootTreeNodes.Empty();

	RestoreExpandedItems();

	if (PropertyRowGeneratorSource.IsValid())
	{
		TArray<TSharedRef<IDetailTreeNode>> RawRootTreeNodes = PropertyRowGeneratorSource->GetRootTreeNodes();

		for (const TSharedRef<IDetailTreeNode>& RootTreeNode : RawRootTreeNodes)
		{
			bool bShouldDisplayNode = true;
			if (OnFilterDetailTreeNode.IsBound())
			{
				bShouldDisplayNode = OnFilterDetailTreeNode.Execute(RootTreeNode);
			}

			if (bShouldDisplayNode)
			{
				TSharedRef<FDetailTreeNode> CastRootTreeNode = StaticCastSharedRef<FDetailTreeNode>(RootTreeNode);
				TSharedRef<FDisplayClusterColorGradingDetailTreeItem> RootTreeItem = MakeShared<FDisplayClusterColorGradingDetailTreeItem>(CastRootTreeNode);
				RootTreeItem->Initialize(OnFilterDetailTreeNode);

				RootTreeNodes.Add(RootTreeItem);

				UpdateExpansionState(RootTreeItem);
			}
		}
	}
}

void SDisplayClusterColorGradingDetailView::UpdateExpansionState(const TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeItem)
{
	if (InTreeItem->IsCategory())
	{
		TreeItemsToSetExpansionState.Add(InTreeItem, InTreeItem->ShouldBeExpanded());
	}
	else if (InTreeItem->IsItem())
	{
		TSharedPtr<FDisplayClusterColorGradingDetailTreeItem> ParentCategory = InTreeItem->GetParent().Pin();
		while (ParentCategory.IsValid() && !ParentCategory->IsCategory())
		{
			ParentCategory = ParentCategory->GetParent().Pin();
		}

		FString Key;
		if (ParentCategory.IsValid())
		{
			Key = ParentCategory->GetNodeName().ToString() + TEXT(".") + InTreeItem->GetNodeName().ToString();
		}
		else
		{
			Key = InTreeItem->GetNodeName().ToString();
		}

		const bool bShouldItemBeExpanded = ExpandedDetailNodes.Contains(Key) && InTreeItem->HasChildren();
		TreeItemsToSetExpansionState.Add(InTreeItem, bShouldItemBeExpanded);
	}

	TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>> Children;
	InTreeItem->GetChildren(Children);

	for (const TSharedRef<FDisplayClusterColorGradingDetailTreeItem>& Child : Children)
	{
		UpdateExpansionState(Child);
	}
}

TSharedRef<ITableRow> SDisplayClusterColorGradingDetailView::GenerateNodeRow(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDisplayClusterColorGradingDetailTreeRow, InTreeItem, OwnerTable, ColumnSizeData);
}

void SDisplayClusterColorGradingDetailView::GetChildrenForNode(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeItem, TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>>& OutChildren)
{
	InTreeItem->GetChildren(OutChildren);
}

void SDisplayClusterColorGradingDetailView::SetNodeExpansionState(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeItem, bool bIsItemExpanded, bool bRecursive)
{
	TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>> Children;
	InTreeItem->GetChildren(Children);

	if (Children.Num())
	{
		const bool bShouldSaveState = true;
		InTreeItem->OnItemExpansionChanged(bIsItemExpanded, bShouldSaveState);

		// Category nodes will save themselves to the editor config, but the item nodes can't, so manually save their expansion state here
		if (InTreeItem->IsItem())
		{
			TSharedPtr<FDisplayClusterColorGradingDetailTreeItem> ParentCategory = InTreeItem->GetParent().Pin();
			while (ParentCategory.IsValid() && !ParentCategory->IsCategory())
			{
				ParentCategory = ParentCategory->GetParent().Pin();
			}

			FString Key;
			if (ParentCategory.IsValid())
			{
				Key = ParentCategory->GetNodeName().ToString() + TEXT(".") + InTreeItem->GetNodeName().ToString();
			}
			else
			{
				Key = InTreeItem->GetNodeName().ToString();
			}
			
			if (bIsItemExpanded)
			{
				ExpandedDetailNodes.Add(Key);
			}
			else
			{
				ExpandedDetailNodes.Remove(Key);
			}
		}

		if (bRecursive)
		{
			for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
			{
				SetNodeExpansionState(Children[ChildIndex], bIsItemExpanded, bRecursive);
			}
		}
	}
}

void SDisplayClusterColorGradingDetailView::OnSetExpansionRecursive(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, true);
	SaveExpandedItems();
}

void SDisplayClusterColorGradingDetailView::OnExpansionChanged(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, false);
	SaveExpandedItems();
}

void SDisplayClusterColorGradingDetailView::OnRowReleased(const TSharedRef<ITableRow>& TableRow)
{
	// search upwards from the current keyboard-focused widget to see if it's contained in our row
	TSharedPtr<SWidget> CurrentWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	while (CurrentWidget.IsValid())
	{
		if (CurrentWidget == TableRow->AsWidget())
		{
			// if so, clear focus so that any pending value changes are committed
			FSlateApplication::Get().ClearKeyboardFocus();
			return;
		}

		CurrentWidget = CurrentWidget->GetParentWidget();
	}
}

EVisibility SDisplayClusterColorGradingDetailView::GetScrollBarVisibility() const
{
	const bool bShowScrollBar = RootTreeNodes.Num() > 0;
	return bShowScrollBar ? EVisibility::Visible : EVisibility::Collapsed;
}