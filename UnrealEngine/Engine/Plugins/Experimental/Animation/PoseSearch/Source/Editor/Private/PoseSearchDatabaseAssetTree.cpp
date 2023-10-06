// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseAssetTree.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AssetSelection.h"
#include "ClassIconFinder.h"
#include "DetailColumnSizeData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/FeedbackContext.h"
#include "Misc/TransactionObjectEvent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearchDatabaseViewModel.h"
#include "SPositiveActionButton.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SDatabaseAssetTree"

namespace UE::PoseSearch
{
	SDatabaseAssetTree::~SDatabaseAssetTree()
	{
	}

	void SDatabaseAssetTree::Construct(
		const FArguments& InArgs, 
		TSharedRef<FDatabaseViewModel> InEditorViewModel)
	{
		EditorViewModel = InEditorViewModel;
		
		ColumnSizeData.SetValueColumnWidth(0.6f);

		CreateCommandList();

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(SPositiveActionButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.Text(LOCTEXT("AddNew", "Add"))
					.ToolTipText(LOCTEXT("AddNewToolTip", "Add a new Sequence, Blend Space, Anim Composite, or Anim Montage"))
					.OnGetMenuContent(this, &SDatabaseAssetTree::CreateAddNewMenuWidget)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(2, 0, 0, 0)
				[
					GenerateFilterBoxWidget()
				]
			]
			+SVerticalBox::Slot()
			.Padding(0.0f, 0.0f)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SAssignNew(TreeView, STreeView<TSharedPtr<FDatabaseAssetTreeNode>>)
						.TreeItemsSource(&RootNodes)
						.SelectionMode(ESelectionMode::Multi)
						.OnGenerateRow(this, &SDatabaseAssetTree::MakeTableRowWidget)
						.OnGetChildren(this, &SDatabaseAssetTree::HandleGetChildrenForTree)
						.OnContextMenuOpening(this, &SDatabaseAssetTree::CreateContextMenu)
						.HighlightParentNodesForSelection(false)
						.OnSelectionChanged_Lambda([this](TSharedPtr<FDatabaseAssetTreeNode> Item, ESelectInfo::Type Type)
							{
								TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedItems = TreeView->GetSelectedItems();
								OnSelectionChanged.Broadcast(SelectedItems, Type);
							})
						.ItemHeight(24)
					]
					+SOverlay::Slot()
					[
						SAssignNew(TreeViewDragAndDropSuggestion, SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Drag and drop Animation Sequences, Anim Composites, Blendspaces, or Anim Montages")))
							.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						]
					]
				]
			]
		];

		RefreshTreeView(true);
	}

	FReply SDatabaseAssetTree::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		FReply Reply = FReply::Unhandled();

		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

		const bool bValidOperation =
			Operation.IsValid() &&
			(Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>());
		if (bValidOperation)
		{
			Reply = AssetUtil::CanHandleAssetDrag(DragDropEvent);

			if (!Reply.IsEventHandled())
			{
				if (Operation->IsOfType<FAssetDragDropOp>())
				{
					const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

					for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
					{
						if (UClass* AssetClass = AssetData.GetClass())
						{
							if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) ||
								AssetClass->IsChildOf(UAnimComposite::StaticClass()) ||
								AssetClass->IsChildOf(UBlendSpace::StaticClass()) ||
								AssetClass->IsChildOf(UAnimMontage::StaticClass()))
							{
								Reply = FReply::Handled();
								break;
							}
						}
					}
				}
			}
		}

		return Reply;
	}

	FReply SDatabaseAssetTree::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		return OnAcceptDrop(DragDropEvent, EItemDropZone::OntoItem, nullptr);
	}

	FReply SDatabaseAssetTree::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (CommandList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	bool SDatabaseAssetTree::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		// Ensure that we only react to modifications to the UPosesSearchDatabase.
		if (const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin())
		{
			if (const UPoseSearchDatabase * Database = ViewModel->GetPoseSearchDatabase())
			{
				for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
				{
					const UObject* Object = TransactionObjectPair.Key;
					while (Object != nullptr)
					{
						if (Object == Database)
						{
							return true;
						}

						Object = Object->GetOuter();
					}
				}
			}
		}
		
		return false;
	}

	void SDatabaseAssetTree::PostUndo(bool bSuccess)
	{
		if (bSuccess)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::PostRedo(bool bSuccess)
	{
		if (bSuccess)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::RefreshTreeView(bool bIsInitialSetup, bool bRecoverSelection)
	{
		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		if (!ViewModel.IsValid())
		{
			return;
		}

		const TSharedRef<FDatabaseViewModel> ViewModelRef = ViewModel.ToSharedRef();

		// Empty node data.
		RootNodes.Reset();
		AllNodes.Reset();

		const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
		if (!IsValid(Database))
		{
			TreeView->RequestTreeRefresh();
			return;
		}

		// Store selection so we can recover it afterwards (if possible)
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> PreviouslySelectedNodes = TreeView->GetSelectedItems();

		// Rebuild node hierarchy
		{
			// Setup default group node
			{
				const TSharedPtr<FDatabaseAssetTreeNode> DefaultGroupNode = MakeShared<FDatabaseAssetTreeNode>(INDEX_NONE, ViewModelRef);
				AllNodes.Add(DefaultGroupNode);
				RootNodes.Add(DefaultGroupNode);
			}
			
			const int32 DefaultGroupIdx = RootNodes.Num() - 1;
						
			// Build an index based off of alphabetical order than iterate the index instead
			TArray<uint32> IndexArray;
			IndexArray.SetNumUninitialized(Database->AnimationAssets.Num());
			for (int32 AnimationAssetIdx = 0; AnimationAssetIdx < Database->AnimationAssets.Num(); ++AnimationAssetIdx)
			{
				IndexArray[AnimationAssetIdx] = AnimationAssetIdx;
			}

			IndexArray.Sort([Database](int32 SequenceIdxA, int32 SequenceIdxB)
			{
				const FPoseSearchDatabaseAnimationAssetBase* A = Database->GetAnimationAssetBase(SequenceIdxA);
				const FPoseSearchDatabaseAnimationAssetBase* B = Database->GetAnimationAssetBase(SequenceIdxB);

				//If its null add it to the end of the list 
				if (!B->GetAnimationAsset())
				{
					return true;
				}

				if (!A->GetAnimationAsset())
				{
					return false;
				}

				const int32 Comparison = A->GetName().Compare(B->GetName());
				return Comparison < 0;
			});

			// create all nodes
			for (int32 AnimationAssetIdx = 0; AnimationAssetIdx < Database->AnimationAssets.Num(); ++AnimationAssetIdx)
			{
				const int32 MappedId = IndexArray[AnimationAssetIdx];

				if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(MappedId))
				{
					const bool bFiltered = (DatabaseAnimationAsset->GetAnimationAsset() == nullptr || GetAssetFilterString().IsEmpty()) ? false : !DatabaseAnimationAsset->GetName().Contains(GetAssetFilterString());

					if (!bFiltered)
					{
						// Create sequence node
						const TSharedPtr<FDatabaseAssetTreeNode> SequenceGroupNode = MakeShared<FDatabaseAssetTreeNode>(MappedId, ViewModelRef);
						const TSharedPtr<FDatabaseAssetTreeNode>& ParentGroupNode = RootNodes[DefaultGroupIdx];

						// Setup hierarchy
						SequenceGroupNode->Parent = ParentGroupNode;
						ParentGroupNode->Children.Add(SequenceGroupNode);

						// Keep track of node
						AllNodes.Add(SequenceGroupNode);
					}
				}
			}

			// Show drag and drop suggestion if tree is empty
			TreeViewDragAndDropSuggestion->SetVisibility(IndexArray.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
		}

		// Update tree view
		TreeView->RequestTreeRefresh();

		for (TSharedPtr<FDatabaseAssetTreeNode>& RootNode : RootNodes)
		{
			TreeView->SetItemExpansion(RootNode, true);
		}

		// Handle selection
		if (bRecoverSelection)
		{
			RecoverSelection(PreviouslySelectedNodes);
		}
		else
		{
			TreeView->SetItemSelection(PreviouslySelectedNodes, false, ESelectInfo::Direct);
		}
	}

	TSharedRef<ITableRow> SDatabaseAssetTree::MakeTableRowWidget(
		TSharedPtr<FDatabaseAssetTreeNode> InItem,
		const TSharedRef<STableViewBase>& OwnerTable)
	{
		return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), CommandList.ToSharedRef(), SharedThis(this));
	}

	void SDatabaseAssetTree::HandleGetChildrenForTree(
		TSharedPtr<FDatabaseAssetTreeNode> InNode,
		TArray<TSharedPtr<FDatabaseAssetTreeNode>>& OutChildren)
	{
		OutChildren = InNode.Get()->Children;
	}

	TOptional<EItemDropZone> SDatabaseAssetTree::OnCanAcceptDrop(
		const FDragDropEvent& DragDropEvent,
		EItemDropZone DropZone,
		TSharedPtr<FDatabaseAssetTreeNode> TargetItem)
	{
		TOptional<EItemDropZone> ReturnedDropZone;

		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

		const bool bValidOperation = Operation.IsValid() && Operation->IsOfType<FAssetDragDropOp>();
		if (bValidOperation)
		{
			const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

			for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
			{
				if (UClass* AssetClass = AssetData.GetClass())
				{
					if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) ||
						AssetClass->IsChildOf(UAnimComposite::StaticClass()) ||
						AssetClass->IsChildOf(UBlendSpace::StaticClass()) ||
						AssetClass->IsChildOf(UAnimMontage::StaticClass()))
					{
						ReturnedDropZone = EItemDropZone::OntoItem;
						break;
					}
				}
			}
		}

		return ReturnedDropZone;
	}

	FReply SDatabaseAssetTree::OnAcceptDrop(
		const FDragDropEvent& DragDropEvent,
		EItemDropZone DropZone,
		TSharedPtr<FDatabaseAssetTreeNode> TargetItem)
	{
		const TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

		const bool bValidOperation = Operation.IsValid() && Operation->IsOfType<FAssetDragDropOp>();
		if (!bValidOperation)
		{
			return FReply::Unhandled();
		}

		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		if (!ViewModel.IsValid())
		{
			return FReply::Unhandled();
		}

		TArray<FAssetData> DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag(Operation);
		const int32 NumAssets = DroppedAssetData.Num();

		int32 AddedAssets = 0;
		if (NumAssets > 0)
		{
			GWarn->BeginSlowTask(LOCTEXT("LoadingAssets", "Loading Asset(s)"), true);

			{
				const FScopedTransaction Transaction(LOCTEXT("AddSequencesOrBlendspaces", "Add Sequence(s) and/or Blendspace(s) to Pose Search Database"));
				ViewModel->GetPoseSearchDatabase()->Modify();
				
				for (int32 DroppedAssetIdx = 0; DroppedAssetIdx < NumAssets; ++DroppedAssetIdx)
				{
					const FAssetData& AssetData = DroppedAssetData[DroppedAssetIdx];

					if (!AssetData.IsAssetLoaded())
					{
						GWarn->StatusUpdate(
							DroppedAssetIdx,
							NumAssets,
							FText::Format(
								LOCTEXT("LoadingAsset", "Loading Asset {0}"),
								FText::FromName(AssetData.AssetName)));
					}

					UClass* AssetClass = AssetData.GetClass();
					UObject* Asset = AssetData.GetAsset();
					
					if (AssetClass->IsChildOf(UAnimSequence::StaticClass()))
					{
						ViewModel->AddSequenceToDatabase(Cast<UAnimSequence>(Asset));
						++AddedAssets;
					}
					if (AssetClass->IsChildOf(UAnimComposite::StaticClass()))
					{
						ViewModel->AddAnimCompositeToDatabase(Cast<UAnimComposite>(Asset));
						++AddedAssets;
					}
					else if (AssetClass->IsChildOf(UBlendSpace::StaticClass()))
					{
						ViewModel->AddBlendSpaceToDatabase(Cast<UBlendSpace>(Asset));
						++AddedAssets;
					}
					else if (AssetClass->IsChildOf(UAnimMontage::StaticClass()))
					{
						ViewModel->AddAnimMontageToDatabase(Cast<UAnimMontage>(Asset));
						++AddedAssets;
					}
				}
			}
			
			GWarn->EndSlowTask();
		}

		if (AddedAssets == 0)
		{
			return FReply::Unhandled();
		}

		FinalizeTreeChanges(false);
		return FReply::Handled();
	}

	TSharedRef<SWidget> SDatabaseAssetTree::CreateAddNewMenuWidget()
	{
		FMenuBuilder AddOptions(true, nullptr);

		AddOptions.BeginSection("AddOptions", LOCTEXT("AssetAddOptions", "Assets"));
		{
			AddOptions.AddMenuEntry(
				LOCTEXT("AddSequenceOption", "Sequence"),
				LOCTEXT("AddSequenceToDatabaseTooltip", "Add new sequence to the database"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnAddSequence, true)),
				NAME_None,
				EUserInterfaceActionType::Button);

			AddOptions.AddMenuEntry(
				LOCTEXT("AddBlendSpaceOption", "Blend Space"),
				LOCTEXT("AddBlendSpaceToDatabaseTooltip", "Add new blend space to the database"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnAddBlendSpace, true)),
				NAME_None,
				EUserInterfaceActionType::Button);

			AddOptions.AddMenuEntry(
				LOCTEXT("AddAnimCompositeOption", "Anim Composite"),
				LOCTEXT("AddAnimCompositeToDatabaseTooltip", "Add new composite to the database"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnAddAnimComposite, true)),
				NAME_None,
				EUserInterfaceActionType::Button);

			AddOptions.AddMenuEntry(
				LOCTEXT("AddAnimMontageOption", "Anim Montage"),
				LOCTEXT("AddAnimMontageToDatabaseTooltip", "Add new montage to the database"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnAddAnimMontage, true)),
				NAME_None,
				EUserInterfaceActionType::Button);
		}
		AddOptions.EndSection();

		return AddOptions.MakeWidget();
	}

	TSharedPtr<SWidget> SDatabaseAssetTree::CreateContextMenu()
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder(CloseAfterSelection, CommandList);

		const TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedNodes = TreeView->GetSelectedItems();
		if (!SelectedNodes.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteUngroup", "Delete / Remove"),
				LOCTEXT("DeleteUngroupTooltip", "Deletes groups and ungrouped assets; removes grouped assets from group."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnDeleteNodes)),
				NAME_None,
				EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("Enable", "Enable"),
				LOCTEXT(
					"EnableTooltip",
					"Sets Assets Enabled."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnEnableNodes)),
				NAME_None,
				EUserInterfaceActionType::Button);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("Disable", "Disable"),
				LOCTEXT(
					"DisableToolTip",
					"Sets Assets Disabled."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnDisableNodes)),
				NAME_None,
				EUserInterfaceActionType::Button);
		}

		return MenuBuilder.MakeWidget();
	}

	TSharedRef<SWidget> SDatabaseAssetTree::GenerateFilterBoxWidget()
	{
		TSharedPtr<SSearchBox> SearchBox;
		SAssignNew(SearchBox, SSearchBox)
			.MinDesiredWidth(300.0f)
			.InitialText(this, &SDatabaseAssetTree::GetFilterText)
			.ToolTipText(FText::FromString(TEXT("Enter Asset Filter...")))
			.OnTextChanged(this, &SDatabaseAssetTree::OnAssetFilterTextCommitted, ETextCommit::Default)
			.OnTextCommitted(this, &SDatabaseAssetTree::OnAssetFilterTextCommitted);

		return SearchBox.ToSharedRef();
	}


	FText SDatabaseAssetTree::GetFilterText() const
	{
		return FText::FromString(GetAssetFilterString());
	}

	void SDatabaseAssetTree::OnAssetFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
	{
		SetAssetFilterString(InText.ToString());
		RefreshTreeView(false);
	}


	void SDatabaseAssetTree::OnAddSequence(bool bFinalizeChanges)
	{
		FScopedTransaction Transaction(LOCTEXT("AddSequence", "Add Sequence"));
		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		
		ViewModel->GetPoseSearchDatabase()->Modify();
		
		ViewModel->AddSequenceToDatabase(nullptr);

		if (bFinalizeChanges)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::OnAddBlendSpace(bool bFinalizeChanges)
	{
		FScopedTransaction Transaction(LOCTEXT("AddBlendSpaceTransaction", "Add Blend Space"));
		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		
		ViewModel->GetPoseSearchDatabase()->Modify();
		
		ViewModel->AddBlendSpaceToDatabase(nullptr);

		if (bFinalizeChanges)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::OnAddAnimComposite(bool bFinalizeChanges)
	{
		FScopedTransaction Transaction(LOCTEXT("AddAnimCompositeTransaction", "Add Anim Composite"));

		EditorViewModel.Pin()->AddAnimCompositeToDatabase(nullptr);

		if (bFinalizeChanges)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::OnAddAnimMontage(bool bFinalizeChanges)
	{
		FScopedTransaction Transaction(LOCTEXT("AddAnimMontageTransaction", "Add Anim Montage"));

		EditorViewModel.Pin()->AddAnimMontageToDatabase(nullptr);

		if (bFinalizeChanges)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::OnDeleteAsset(TSharedPtr<FDatabaseAssetTreeNode> Node, bool bFinalizeChanges)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteAsset", "Delete Asset"));
		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		
		ViewModel->GetPoseSearchDatabase()->Modify();
		ViewModel->DeleteFromDatabase(Node->SourceAssetIdx);

		if (bFinalizeChanges)
		{
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::RegisterOnSelectionChanged(const FOnSelectionChanged& Delegate)
	{
		OnSelectionChanged.Add(Delegate);
	}

	void SDatabaseAssetTree::UnregisterOnSelectionChanged(void* Unregister)
	{
		OnSelectionChanged.RemoveAll(Unregister);
	}

	void SDatabaseAssetTree::RecoverSelection(const TArray<TSharedPtr<FDatabaseAssetTreeNode>>& PreviouslySelectedNodes)
	{
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> NewSelectedNodes;

		for (const TSharedPtr<FDatabaseAssetTreeNode>& Node : AllNodes)
		{
			const bool bFoundNode = PreviouslySelectedNodes.ContainsByPredicate([Node](const TSharedPtr<FDatabaseAssetTreeNode>& PrevSelectedNode) { return PrevSelectedNode->SourceAssetIdx == Node->SourceAssetIdx; });
			if (bFoundNode)
			{
				NewSelectedNodes.Add(Node);
			}
		}

		// @todo: investigate if we should call a TreeView->ClearSelection() before TreeView->SetItemSelection
		TreeView->SetItemSelection(NewSelectedNodes, true, ESelectInfo::Direct);
	}

	void SDatabaseAssetTree::CreateCommandList()
	{
		CommandList = MakeShared<FUICommandList>();

		CommandList->MapAction(
			FGenericCommands::Get().Delete,
			FUIAction(
				FExecuteAction::CreateSP(this, &SDatabaseAssetTree::OnDeleteNodes),
				FCanExecuteAction::CreateSP(this, &SDatabaseAssetTree::CanDeleteNodes)));
	}

	bool SDatabaseAssetTree::CanDeleteNodes() const
	{
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedNodes = TreeView->GetSelectedItems();
		for (TSharedPtr<FDatabaseAssetTreeNode> SelectedNode : SelectedNodes)
		{
			if (SelectedNode->SourceAssetIdx != INDEX_NONE)
			{
				return true;
			}
		}

		return false;
	}

	void SDatabaseAssetTree::OnDeleteNodes()
	{
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedNodes = TreeView->GetSelectedItems();
		if (!SelectedNodes.IsEmpty())
		{
			const FScopedTransaction Transaction(LOCTEXT("DeletePoseSearchDatabaseNodes", "Delete selected items from Pose Search Database"));
			const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();

			ViewModel->GetPoseSearchDatabase()->Modify();
			
			SelectedNodes.Sort([](const TSharedPtr<FDatabaseAssetTreeNode>& A, const TSharedPtr<FDatabaseAssetTreeNode>& B)
				{
					return B->SourceAssetIdx < A->SourceAssetIdx;
				});

			for (TSharedPtr<FDatabaseAssetTreeNode> SelectedNode : SelectedNodes)
			{
				if (SelectedNode->SourceAssetIdx != INDEX_NONE)
				{
					OnDeleteAsset(SelectedNode, false);
				}
			}
			
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::EnableSelectedNodes(bool bIsEnabled)
	{
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedNodes = TreeView->GetSelectedItems();
		if (!SelectedNodes.IsEmpty())
		{
			const FText TransactionName = bIsEnabled ? LOCTEXT("EnablePoseSearchDatabaseNodes", "Enable selected items from Pose Search Database") : LOCTEXT("DisablePoseSearchDatabaseNodes", "Disable selected items from Pose Search Database");
			const FScopedTransaction Transaction(TransactionName);
			const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();

			ViewModel->GetPoseSearchDatabase()->Modify();

			for (TSharedPtr<FDatabaseAssetTreeNode> SelectedNode : SelectedNodes)
			{
				ViewModel->SetIsEnabled(SelectedNode->SourceAssetIdx, bIsEnabled);
			}
		
			FinalizeTreeChanges();
		}
	}

	void SDatabaseAssetTree::FinalizeTreeChanges(bool bRecoverSelection)
	{
		RefreshTreeView(false, bRecoverSelection);
		EditorViewModel.Pin()->BuildSearchIndex();
	}

	void SDatabaseAssetTree::SetSelectedItem(int32 SourceAssetIdx, bool bClearSelection)
	{
		if (bClearSelection)
		{
			TreeView->ClearSelection();
		}

		if (SourceAssetIdx >= 0)
		{
			for (TSharedPtr<FDatabaseAssetTreeNode>& Node : AllNodes)
			{
				if (Node->SourceAssetIdx == SourceAssetIdx)
				{
					TreeView->SetItemSelection(Node, true);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
