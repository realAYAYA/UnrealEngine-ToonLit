// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMManageViewModelsWidget.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SMVVMViewModelContextListWidget.h"
#include "SMVVMViewModelBindingListWidget.h"
#include "SPrimaryButton.h"
#include "SSimpleButton.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Textures/SlateIcon.h"
#include "Types/MVVMFieldVariant.h"
#include "UMGStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "ManageViewModelsWidget"

namespace UE::MVVM::Private
{
	class FMVVMViewModelTreeNode : public TSharedFromThis<FMVVMViewModelTreeNode>
	{
	public:
		struct FClassData
		{
			TSubclassOf<UMVVMViewModelBase> Class;
			bool bPassesClassFlagsFilter;
		};

		struct FTagData
		{
			FText Name;
		};

		FMVVMViewModelTreeNode(FMVVMViewModelTreeNode& OtherNode)
		{
			NodeData = OtherNode.NodeData;
		}

		FMVVMViewModelTreeNode(FClassData& Class)
		{
			NodeData.Set<FMVVMViewModelTreeNode::FClassData>(Class);
		}

		FMVVMViewModelTreeNode(FTagData& Tag)
		{
			NodeData.Set<FMVVMViewModelTreeNode::FTagData>(Tag);
		}

		void AddChild(TSharedRef<FMVVMViewModelTreeNode> Child)
		{
			ChildrenList.Add(Child);
		}

		TArray<TSharedRef<FMVVMViewModelTreeNode>>& GetChildrenList()
		{
			return ChildrenList;
		}

		void UpdateNodeSearchFilterFlag(const FString& SearchString)
		{
			bPassesSearchTextFilter = SearchString.IsEmpty() ? true : GetDisplayName().Contains(SearchString);

			for (TSharedRef<FMVVMViewModelTreeNode>& Child : GetChildrenList())
			{
				Child->UpdateNodeSearchFilterFlag(SearchString);
			}
		}

		TSet<TSharedRef<FMVVMViewModelTreeNode>> GetNodeBranchesToDiscard()
		{
			TSet<TSharedRef<FMVVMViewModelTreeNode>> NodeBranchesToDiscard;

			TSharedRef<FMVVMViewModelTreeNode> SharedNode = AsShared();
			GetNodeBranchesToDiscard_Helper(SharedNode, NodeBranchesToDiscard);

			return NodeBranchesToDiscard;
		}

		bool IsAddable()
		{
			if (FClassData* ClassInfo = GetClassData())
			{
				return ClassInfo->bPassesClassFlagsFilter;
			}

			return false;
		}

		bool IsPassingSearchFilter()
		{
			return bPassesSearchTextFilter;
		}

		FText GetDisplayNameText()
		{
			if (FMVVMViewModelTreeNode::FClassData* ClassInfo = GetClassData())
			{
				return ClassInfo->Class->GetDisplayNameText();
			}
			else
			{
				FMVVMViewModelTreeNode::FTagData* TagInfo = GetTagData();
				check(TagInfo);
				return TagInfo->Name;
			}
		}

		FString GetDisplayName()
		{
			if (FMVVMViewModelTreeNode::FClassData* ClassInfo = GetClassData())
			{
				return ClassInfo->Class->GetDisplayNameText().ToString();
			}
			else
			{
				FMVVMViewModelTreeNode::FTagData* TagInfo = GetTagData();
				check(TagInfo);
				return TagInfo->Name.ToString();
			}
		}

		TSharedRef<SWidget> GetIcon()
		{
			if (FMVVMViewModelTreeNode::FClassData* ClassInfo = GetClassData())
			{
				return SNew(SImage).Image(FSlateIconFinder::FindIconBrushForClass(ClassInfo->Class));
			}
			else
			{
				return SNew(SImage)
					.Image(FSlateIcon(FMVVMEditorStyle::Get().GetStyleSetName(), "ViewModelSelection.TagIcon").GetIcon());
			}
		}

		FClassData* GetClassData()
		{
			return NodeData.TryGet<FClassData>();
		}

		FTagData* GetTagData()
		{
			return NodeData.TryGet<FTagData>();
		}

		void SortChildren()
		{
			GetChildrenList().Sort([](const TSharedPtr<FMVVMViewModelTreeNode>& A, const TSharedPtr<FMVVMViewModelTreeNode>& B)
				{
					return A->GetDisplayName() <= B->GetDisplayName();
				});
		}

	private:
		static bool GetNodeBranchesToDiscard_Helper(TSharedRef<FMVVMViewModelTreeNode>& Node, TSet<TSharedRef<FMVVMViewModelTreeNode>>& OutNodeBranchesToDiscard)
		{
			bool bShouldKeep = false;

			for (TSharedRef<FMVVMViewModelTreeNode>& Child : Node->GetChildrenList())
			{
				bool bShouldKeepChild = GetNodeBranchesToDiscard_Helper(Child, OutNodeBranchesToDiscard);
				bShouldKeep = bShouldKeep || bShouldKeepChild;
			}

			if (!bShouldKeep)
			{
				if (FClassData* Class = Node->GetClassData())
				{
					bShouldKeep = bShouldKeep || (Class->bPassesClassFlagsFilter && Node->IsPassingSearchFilter());
				}
			}

			if (!bShouldKeep)
			{
				OutNodeBranchesToDiscard.Add(Node);
			}

			return bShouldKeep;
		}

		TVariant<FClassData, FTagData> NodeData;
		bool bPassesSearchTextFilter = true;
		TArray<TSharedRef<FMVVMViewModelTreeNode>> ChildrenList;
	};
} // namespace UE::MVVM::Private

void SMVVMManageViewModelsWidget::Construct(const FArguments& InArgs)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;

	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	UMVVMBlueprintView* BlueprintView = EditorSubsystem->RequestView(InArgs._WidgetBlueprint);

	WeakParentWindow = InArgs._ParentWindow;
	OnViewModelContextsPicked = InArgs._OnViewModelContextsPickedDelegate;
	ViewModelContextListWidget = SNew(SMVVMViewModelContextListWidget)
		.ExistingViewModelContexts(TArray<FMVVMBlueprintViewModelContext>(BlueprintView->GetViewModels()))
		.WidgetBlueprint(InArgs._WidgetBlueprint);

	PopulateViewModelsTree();

	TSharedRef<SWidget> ButtonsPanelContent = InArgs._ButtonsPanel.Widget;
	if (InArgs._ButtonsPanel.Widget == SNullWidget::NullWidget)
	{
		SAssignNew(ButtonsPanelContent, SUniformGridPanel)
			.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("ViewModelFinishButtonText", "Finish"))
				.OnClicked(this, &SMVVMManageViewModelsWidget::HandleClicked_Finish)
				.IsEnabled(this, &SMVVMManageViewModelsWidget::IsFinishEnabled)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ViewModelCancelButtonText", "Cancel"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SMVVMManageViewModelsWidget::HandleClicked_Cancel)
			];
	}


	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6)
			[
				GenerateSearchBar()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				.PhysicalSplitterHandleSize(2.0f)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SNew(SSplitter)
					.PhysicalSplitterHandleSize(2.0f)
					+ SSplitter::Slot()
					.Value(0.6f)
					[
						SNew(SBorder)
						.BorderImage(FStyleDefaults::GetNoBrush())
						[
							SAssignNew(TreeViewWidget, STreeView<TSharedPtr<FMVVMViewModelTreeNode>>)
							.SelectionMode(ESelectionMode::Single)
							.TreeItemsSource(&NodeTreeViewSource)
							.OnGetChildren(this, &SMVVMManageViewModelsWidget::HandleGetChildrenForTreeView)
							.OnSetExpansionRecursive(this, &SMVVMManageViewModelsWidget::SetAllExpansionStates_Helper)
							.OnGenerateRow(this, &SMVVMManageViewModelsWidget::HandleGenerateRowForTreeView)
							.OnSelectionChanged(this, &SMVVMManageViewModelsWidget::HandleTreeViewSelectionChanged)
							.OnExpansionChanged(this, &SMVVMManageViewModelsWidget::HandleTreeViewExpansionChanged)
							.OnMouseButtonDoubleClick(this, &SMVVMManageViewModelsWidget::HandleTreeRowDoubleClicked)
							.ItemHeight(20.0f)
						]
					]
					+ SSplitter::Slot()
					.Value(0.4f)
					[
						SNew(SBorder)
						.BorderImage(FStyleDefaults::GetNoBrush())
						[
							SAssignNew(BindingListWidget, UE::MVVM::SSourceBindingList, InArgs._WidgetBlueprint)
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMargin(6.0f, 3.0f))
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedViewModelsHeading", "Selected ViewModels"))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						ViewModelContextListWidget.ToSharedRef()
					]
				]
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.AutoHeight()
			.Padding(8)
			[
				ButtonsPanelContent
			]
		]
	];

	RefreshTreeView(true);
}

void SMVVMManageViewModelsWidget::PopulateViewModelsTree()
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	TMap<FName, TSharedRef<FMVVMViewModelTreeNode>> ClassNodeMap;

	TMap<FString, TArray<FMVVMViewModelTreeNode::FClassData>> TagClassMap;
	FString UntaggedTagName = "<untagged>";
	TagClassMap.Add(UntaggedTagName);

	{
		FMVVMViewModelTreeNode::FClassData NullClassData;
		NullClassData.Class = UMVVMViewModelBase::StaticClass();
		NullClassData.bPassesClassFlagsFilter = false;
		ClassListRootNode = MakeShared<FMVVMViewModelTreeNode>(NullClassData);
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		bool bPassesClassFlagsFilter = !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated |
										CLASS_NewerVersionExists | CLASS_HideDropDown | CLASS_Hidden);
		bool bIsNotSkeleton = !FKismetEditorUtilities::IsClassABlueprintSkeleton(*It);

		if (It->IsChildOf(UMVVMViewModelBase::StaticClass()) && bIsNotSkeleton)
		{
			UClass* Class = *It;
			FMVVMViewModelTreeNode::FClassData Data;
			Data.Class = Class;
			Data.bPassesClassFlagsFilter = bPassesClassFlagsFilter;
			if (bPassesClassFlagsFilter)
			{
				TArray<FString> Tags;
				Class->GetMetaData("MVVMTags").ParseIntoArray(Tags, TEXT(","));

				if (Tags.IsEmpty())
				{
					TagClassMap[UntaggedTagName].Add(Data);
				} 
				else
				{
					for (const FString& TagName : Tags)
					{
						TArray<FMVVMViewModelTreeNode::FClassData>& ClassDataList = TagClassMap.FindOrAdd(TagName);
						ClassDataList.Add(Data);
					}
				}

				ClassListRootNode->AddChild(MakeShared<FMVVMViewModelTreeNode>(Data));
			}

			ClassNodeMap.Add(Class->GetFName(), MakeShared<FMVVMViewModelTreeNode>(Data));
		}
	}

	// Build Tag Hierarchy
	{
		FText AllTagsStr = LOCTEXT("MVVMViewModelAllTagsTreeNode", "<all_tags>");
		FMVVMViewModelTreeNode::FTagData AllTagsNodeData{ AllTagsStr };

		TagTreeRootNode = MakeShared<FMVVMViewModelTreeNode>(AllTagsNodeData);
		TagClassMap.KeySort(TLess<FString>());


		for (TPair<FString, TArray<FMVVMViewModelTreeNode::FClassData>>& Pair : TagClassMap)
		{
			FText LocalizedTagText;
			if (Pair.Key == UntaggedTagName)
			{
				LocalizedTagText = LOCTEXT("MVVMViewModelUntaggedTreeNode", "<untagged>");
			}
			else
			{
				LocalizedTagText = FText::FromString(Pair.Key);
			}
			FMVVMViewModelTreeNode::FTagData NewTagData{ LocalizedTagText };
			TSharedRef<FMVVMViewModelTreeNode> NewTagNode = MakeShared<FMVVMViewModelTreeNode>(NewTagData);
			TagTreeRootNode->AddChild(NewTagNode);
			for (FMVVMViewModelTreeNode::FClassData& NewClass : Pair.Value)
			{
				NewTagNode->AddChild(MakeShared<FMVVMViewModelTreeNode>(NewClass));
			}
		}
	}

	// Build Class Hierarchy
	for (const TPair<FName, TSharedRef<FMVVMViewModelTreeNode>>& Pair : ClassNodeMap)
	{
		TSubclassOf<UMVVMViewModelBase> Class = GetClassFromNode(Pair.Value);

		if (Class == UMVVMViewModelBase::StaticClass())
		{
			ClassTreeRootNode = Pair.Value;
			continue;
		}

		{
			FName SuperClassName = Class->GetSuperClass()->GetFName();

			if (TSharedRef<FMVVMViewModelTreeNode>* SuperClassNodePtr = ClassNodeMap.Find(SuperClassName))
			{
				(*SuperClassNodePtr)->AddChild(Pair.Value);
			}
		}

	}

	CurrentViewTreeRootNode = ClassListRootNode;
}

void SMVVMManageViewModelsWidget::SetViewMode(EViewMode Mode)
{
	CurrentViewMode = Mode;

	switch (Mode)
	{
	case EViewMode::List:
		CurrentViewTreeRootNode = ClassListRootNode;
		break;
	case EViewMode::Hierarchy:
		CurrentViewTreeRootNode = ClassTreeRootNode;
		break;
	case EViewMode::Tag:
		CurrentViewTreeRootNode = TagTreeRootNode;
		break;
	}

	RefreshTreeView();
}

TSharedRef<SWidget> SMVVMManageViewModelsWidget::HandleGetMenuContent_ViewSettings()
{
	FMenuBuilder MenuBuilder(true, NULL);

	SMVVMManageViewModelsWidget* Self = this;

	MenuBuilder.BeginSection("ViewSettings", LOCTEXT("ModeHeading", "View Mode"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ListView", "List View"),
			LOCTEXT("ListViewToolTip", "Show available ViewModels as a flat list"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Self]() { Self->SetViewMode(EViewMode::List); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Self]() { return Self->CurrentViewMode == EViewMode::List; })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("HierarchyView", "Hierarchy View"),
			LOCTEXT("HierarchyViewToolTip", "Show available ViewModels as a class hierarchy"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Self]() { Self->SetViewMode(EViewMode::Hierarchy); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Self]() { return Self->CurrentViewMode == EViewMode::Hierarchy; })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TagView", "Tag View"),
			LOCTEXT("TagViewToolTip", "Show available ViewModels grouped by MVVMTags"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Self]() { Self->SetViewMode(EViewMode::Tag); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Self]() { return Self->CurrentViewMode == EViewMode::Tag; })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();

	if (CurrentViewMode == EViewMode::Hierarchy || CurrentViewMode == EViewMode::Tag)
	{
		MenuBuilder.BeginSection("HierarchySettings", LOCTEXT("HierarchyHeading", "Hierarchy"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ExpandAll", "Expand All"),
				LOCTEXT("ExpandAllToolTip", "Expand All Items in the Hierarchy"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([Self]() {
						if (!Self->NodeTreeViewSource.IsEmpty())
						{
							for (TSharedPtr<FMVVMViewModelTreeNode>& Child : Self->NodeTreeViewSource)
							{
								Self->SetAllExpansionStates_Helper(Child, true);
							}
						}
					})
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CollapseAll", "Collapse All"),
				LOCTEXT("CollapseAllToolTip", "Collapse All Items in the Hierarchy"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([Self]() {
						if (!Self->NodeTreeViewSource.IsEmpty())
						{
							for (TSharedPtr<FMVVMViewModelTreeNode>& Child : Self->NodeTreeViewSource)
							{
								Self->SetAllExpansionStates_Helper(Child, false);
							}
						}					
					})
				)
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SMVVMManageViewModelsWidget::GenerateSearchBar()
{
	SMVVMManageViewModelsWidget* Self = this;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(4.0f)
		.FillWidth(1.0f)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("FilterSearch", "Search..."))
			.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search"))
			.OnTextChanged_Lambda([Self](const FText& NewText) { Self->UpdateSearchString(NewText.ToString()); })
		]
		+ SHorizontalBox::Slot()
		.Padding(4.0f)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButtonWithIcon")
			.OnGetMenuContent(this, &SMVVMManageViewModelsWidget::HandleGetMenuContent_ViewSettings)
			.ButtonContent()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
			]
		];

}

void SMVVMManageViewModelsWidget::UpdateSearchString(const FString& SearchString)
{
	bSearchStringIsEmpty = SearchString.IsEmpty();

	ClassListRootNode->UpdateNodeSearchFilterFlag(SearchString);
	ClassTreeRootNode->UpdateNodeSearchFilterFlag(SearchString);
	TagTreeRootNode->UpdateNodeSearchFilterFlag(SearchString);

	RefreshTreeView();
}

void SMVVMManageViewModelsWidget::RefreshTreeView(bool bForceExpandAll /* = false */)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	RebuildTreeViewSource(NodeTreeViewSource, CurrentViewTreeRootNode.ToSharedRef());
	TreeViewWidget->RequestListRefresh();
	if ((CurrentViewTreeRootNode == ClassTreeRootNode && ClassTreeExpansionStateMap.IsEmpty())
		|| (CurrentViewTreeRootNode == TagTreeRootNode && TagTreeExpansionStateMap.IsEmpty())
		|| CurrentViewTreeRootNode == ClassListRootNode)
	{
		bForceExpandAll = true;
	}
	if (bForceExpandAll)
	{
		if (!NodeTreeViewSource.IsEmpty())
		{
			for (TSharedPtr<FMVVMViewModelTreeNode>& Child : NodeTreeViewSource)
			{
				SetAllExpansionStates_Helper(Child, true);
			}
		}
	}
	else
	{
		RestoreExpansionStatesInTree();
	}
}

void SMVVMManageViewModelsWidget::RebuildTreeViewSource(TArray<TSharedPtr<FMVVMViewModelTreeNode>>& ViewSource, TSharedRef<FMVVMViewModelTreeNode> RootNode)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	ViewSource.Empty();

	TSet<TSharedRef<FMVVMViewModelTreeNode>> NodesToDiscard = RootNode->GetNodeBranchesToDiscard();

	if (!NodesToDiscard.Contains(RootNode))
	{
		for (TSharedRef<FMVVMViewModelTreeNode>& ChildNode : RootNode->GetChildrenList())
		{
			if (!NodesToDiscard.Contains(ChildNode))
			{
				TSharedRef<FMVVMViewModelTreeNode> ChildClone = MakeShared<FMVVMViewModelTreeNode>(*ChildNode);
				ViewSource.Add(ChildClone);
				RebuildTreeViewSource_Helper(ChildNode, ChildClone, NodesToDiscard);
			}

		}

		ViewSource.Sort([](const TSharedPtr<FMVVMViewModelTreeNode>& A, const TSharedPtr<FMVVMViewModelTreeNode>& B)
			{
				return A->GetDisplayName() < B->GetDisplayName();
			});
	}
}

void SMVVMManageViewModelsWidget::RebuildTreeViewSource_Helper(TSharedRef<FMVVMViewModelTreeNode>& Parent, TSharedRef<FMVVMViewModelTreeNode>& CloneParent, const TSet<TSharedRef<FMVVMViewModelTreeNode>>& NodesToDiscard)
{
	for (TSharedRef<FMVVMViewModelTreeNode>& Child : Parent->GetChildrenList())
	{
		if (NodesToDiscard.Contains(Child))
		{
			continue;
		}

		TSharedRef<FMVVMViewModelTreeNode> CloneChild = MakeShared<FMVVMViewModelTreeNode>(*Child);
		CloneParent->AddChild(CloneChild);

		RebuildTreeViewSource_Helper(Child, CloneChild, NodesToDiscard);
	}

	CloneParent->SortChildren();
}

void SMVVMManageViewModelsWidget::HandleGetChildrenForTreeView(TSharedPtr<FMVVMViewModelTreeNode> InParent, TArray<TSharedPtr<FMVVMViewModelTreeNode>>& OutChildren)
{
	OutChildren.Append(InParent->GetChildrenList());
}

void SMVVMManageViewModelsWidget::SetAllExpansionStates_Helper(TSharedPtr<FMVVMViewModelTreeNode> InNode, bool bInExpansionState)
{
	check(InNode.IsValid());
	TreeViewWidget->SetItemExpansion(InNode, bInExpansionState);

	if (CurrentViewTreeRootNode == ClassTreeRootNode)
	{
		ClassTreeExpansionStateMap.Add(InNode->GetDisplayName(), true);
	}
	else if (CurrentViewTreeRootNode == TagTreeRootNode)
	{
		TagTreeExpansionStateMap.Add(InNode->GetDisplayName(), true);
	}

	// Recursively go through the children.
	for (int32 ChildIndex = 0; ChildIndex < InNode->GetChildrenList().Num(); ChildIndex++)
	{
		SetAllExpansionStates_Helper(InNode->GetChildrenList()[ChildIndex], bInExpansionState);
	}
}

void SMVVMManageViewModelsWidget::RestoreExpansionStatesInTree()
{
	if (CurrentViewTreeRootNode == ClassTreeRootNode)
	{
		RestoreExpansionStatesInTree_Helper(NodeTreeViewSource[0].ToSharedRef(), ClassTreeExpansionStateMap, this);
	}
	else if (CurrentViewTreeRootNode == TagTreeRootNode)
	{
		RestoreExpansionStatesInTree_Helper(NodeTreeViewSource[0].ToSharedRef(), TagTreeExpansionStateMap, this);
	}
}

bool SMVVMManageViewModelsWidget::RestoreExpansionStatesInTree_Helper(TSharedRef<FMVVMViewModelTreeNode> InNode, TMap<FString, bool>& ExpansionStateMap, SMVVMManageViewModelsWidget* Self)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	bool bForceParentOpen = false;
	for (TSharedRef<FMVVMViewModelTreeNode>& ChildNode : InNode->GetChildrenList())
	{
		bForceParentOpen |= RestoreExpansionStatesInTree_Helper(ChildNode, ExpansionStateMap, Self);
	}

	if (InNode->IsPassingSearchFilter() && !Self->bSearchStringIsEmpty)
	{
		bForceParentOpen |= true;
	}

	if (bForceParentOpen)
	{
		ExpansionStateMap[InNode->GetDisplayName()] = true;
	}

	Self->TreeViewWidget->SetItemExpansion(InNode, ExpansionStateMap.FindOrAdd(InNode->GetDisplayName(), false));

	return bForceParentOpen;
}

TSharedRef<ITableRow> SMVVMManageViewModelsWidget::HandleGenerateRowForTreeView(TSharedPtr<FMVVMViewModelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	typedef STableRow<TSharedPtr<FMVVMViewModelTreeNode>> RowType;

	bool bShowAddIcon = Item->IsAddable();

	TSharedRef<RowType> NewRow = SNew(RowType, OwnerTable);

	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	if (bShowAddIcon)
	{
		HBox->AddSlot()
		.AutoWidth()
		[
			SNew(SSimpleButton)
			.ToolTipText(LOCTEXT("AddViewModelButtonToolTip", "Add ViewModel"))
			.OnClicked(this, &SMVVMManageViewModelsWidget::HandleClicked_AddViewModel, Item)
			.Icon(FSlateIcon(FMVVMEditorStyle::Get().GetStyleSetName(), "ViewModelSelection.AddIcon").GetIcon())
		];
	}

	HBox->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		Item->GetIcon()
	];

	HBox->AddSlot()
	.Padding(4.0f)
	[
		SNew(STextBlock).Text(Item->GetDisplayNameText())
	];

	NewRow->SetContent(HBox);

	return NewRow;
}

void SMVVMManageViewModelsWidget::HandleTreeViewSelectionChanged(TSharedPtr<FMVVMViewModelTreeNode> Item, ESelectInfo::Type SelectInfo)
{
	TSubclassOf<UMVVMViewModelBase> InClass = nullptr;
	if (Item)
	{
		InClass = GetClassFromNode(Item.ToSharedRef());
	}
	BindingListWidget->AddSource(InClass, FName(), FGuid());
}

void SMVVMManageViewModelsWidget::HandleTreeViewExpansionChanged(TSharedPtr<FMVVMViewModelTreeNode> Item, bool bExpanded)
{
	if (CurrentViewTreeRootNode == ClassTreeRootNode)
	{
		ClassTreeExpansionStateMap.Add(Item->GetDisplayName(), bExpanded);
	}
	else if (CurrentViewTreeRootNode == TagTreeRootNode)
	{
		TagTreeExpansionStateMap.Add(Item->GetDisplayName(), bExpanded);
	}
}

void SMVVMManageViewModelsWidget::HandleTreeRowDoubleClicked(TSharedPtr<FMVVMViewModelTreeNode> Item)
{
	HandleClicked_AddViewModel(Item);
}

FReply SMVVMManageViewModelsWidget::HandleClicked_AddViewModel(TSharedPtr<FMVVMViewModelTreeNode> InNode)
{
	ViewModelContextListWidget->AddViewModelContext(InNode->GetClassData()->Class);

	return FReply::Handled();
}

FReply SMVVMManageViewModelsWidget::HandleClicked_Finish()
{
	TArray<FMVVMBlueprintViewModelContext> AddedContexts = ViewModelContextListWidget->GetViewModelContexts();
	OnViewModelContextsPicked.ExecuteIfBound(AddedContexts);

	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SMVVMManageViewModelsWidget::HandleClicked_Cancel()
{
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

TSubclassOf<UMVVMViewModelBase> SMVVMManageViewModelsWidget::GetClassFromNode(TSharedRef<FMVVMViewModelTreeNode> ClassNode)
{
	using UE::MVVM::Private::FMVVMViewModelTreeNode;
	if (FMVVMViewModelTreeNode::FClassData* ClassInfo = ClassNode->GetClassData())
	{
		return ClassInfo->Class;
	}

	return nullptr;
}

bool SMVVMManageViewModelsWidget::IsFinishEnabled() const
{
	return ViewModelContextListWidget->GetViewModelContexts().Num() > 0;
}

#undef LOCTEXT_NAMESPACE
