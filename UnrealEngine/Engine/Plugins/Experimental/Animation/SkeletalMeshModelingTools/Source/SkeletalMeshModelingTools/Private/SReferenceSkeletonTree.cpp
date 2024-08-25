// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReferenceSkeletonTree.h"

#include "SkeletonModifier.h"
#include "SkeletalMeshModelingToolsCommands.h"
#include "SkeletonClipboard.h"

#include "SPositiveActionButton.h"

#include "Misc/TextFilterExpressionEvaluator.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"

#include "Misc/ITransaction.h"

#define LOCTEXT_NAMESPACE "SReferenceSkeletonTree"

namespace ReferenceSkeletonTreeLocals
{

FSkeletonModifierChange::FSkeletonModifierChange(const USkeletonModifier* InModifier)
	: FCommandChange()
	, PreChangeSkeleton(InModifier->GetReferenceSkeleton())
	, PreBoneTracker(InModifier->GetBoneIndexTracker())
	, PostChangeSkeleton(InModifier->GetReferenceSkeleton())
	, PostBoneTracker(InModifier->GetBoneIndexTracker())
{}

void FSkeletonModifierChange::StoreSkeleton(const USkeletonModifier* InModifier)
{
	PostChangeSkeleton = InModifier->GetReferenceSkeleton();
	PostBoneTracker = InModifier->GetBoneIndexTracker();
}

void FSkeletonModifierChange::Apply(UObject* Object)
{ // redo
	USkeletonModifier* Modifier = CastChecked<USkeletonModifier>(Object);
	Modifier->ExternalUpdate(PostChangeSkeleton, PostBoneTracker);
}

void FSkeletonModifierChange::Revert(UObject* Object)
{ // undo
	USkeletonModifier* Modifier = CastChecked<USkeletonModifier>(Object);
	Modifier->ExternalUpdate(PreChangeSkeleton, PreBoneTracker);
}

}

FBoneElement::FBoneElement(const FName& InBoneName, TWeakObjectPtr<USkeletonModifier> InModifier)
	: BoneName(InBoneName)
	, WeakModifier(InModifier)
{}

void FBoneElement::RequestRename() const
{
	OnRenameRequested.ExecuteIfBound();
}

void SBoneItem::Construct(const FArguments& InArgs)
{
	// cf. FSkeletonTreeBoneItem::GenerateWidgetForNameColumn
	
	WeakTreeElement = InArgs._TreeElement;
	WeakModifier = InArgs._WeakModifier;

	const TSharedPtr<FBoneElement> TreeElement = WeakTreeElement.Pin();

	const FSlateBrush* Brush = FAppStyle::Get().GetBrush("SkeletonTree.Bone");

	TSharedPtr<SHorizontalBox> RowBox;

	ChildSlot
	[
		SAssignNew(RowBox, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 2.0f))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SImage)
			.Image(Brush)
			.ColorAndOpacity_Static(&SBoneItem::GetTextColor, InArgs._IsSelected)
		]
	];

	TSharedPtr< SInlineEditableTextBlock > InlineWidget;
	
	RowBox->AddSlot()
    .AutoWidth()
	.Padding(4, 0, 0, 0)
    .VAlign(VAlign_Center)
    [
	    SAssignNew(InlineWidget, SInlineEditableTextBlock)
		.Text(this, &SBoneItem::GetName)
		// .Font(TextFont)
		.ColorAndOpacity_Static(&SBoneItem::GetTextColor, InArgs._IsSelected)
		.OnVerifyTextChanged(this, &SBoneItem::OnVerifyNameChanged)
		.OnTextCommitted(InArgs._Delegates.OnBoneNameCommitted)
		.MultiLine(false)
    ];

	TreeElement->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

FSlateColor SBoneItem::GetTextColor(FIsSelected InIsSelected)
{
	const bool bIsSelected = InIsSelected.IsBound() ? InIsSelected.Execute() : false;
	if (bIsSelected)
	{
		return FAppStyle::Get().GetSlateColor("Colors.ForegroundInverted");
	}
	
	return FSlateColor::UseForeground();
}

bool SBoneItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage) const
{
	if (!WeakModifier.IsValid())
	{
		return false;
	}

	const TSharedPtr<FBoneElement> BoneElementPtr = WeakTreeElement.Pin();
	const FReferenceSkeleton& ReferenceSkeleton = WeakModifier->GetReferenceSkeleton();

	const FName NewName = FName(InText.ToString());
	const int32 BoneIndex = ReferenceSkeleton.FindRawBoneIndex(NewName);
	return BoneIndex == INDEX_NONE;
}

FText SBoneItem::GetName() const
{
	return FText::FromName(WeakTreeElement.Pin()->BoneName);
}

void SReferenceSkeletonRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
	WeakTreeElement = InArgs._TreeElement;
	WeakModifier = InArgs._WeakModifier;
	Delegates = InArgs._Delegates;

	const FSuperRowType::FArguments Args = FSuperRowType::FArguments()
		.OnDragDetected(InArgs._Delegates.OnDragDetected)
		.OnCanAcceptDrop(InArgs._Delegates.OnCanAcceptDrop)
		.OnAcceptDrop(InArgs._Delegates.OnAcceptDrop)
		.Style( FAppStyle::Get(), "TableView.AlternatingRow" );
	
	SMultiColumnTableRow::Construct(Args , InOwnerTable);
}

TSharedRef<SWidget> SReferenceSkeletonRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew( SExpanderArrow, SharedThis(this) )
			.ShouldDrawWires(true)
	]

	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SBoneItem)
			.WeakModifier(WeakModifier)
			.TreeElement(WeakTreeElement)
			.Delegates(Delegates)
			.IsSelected(this, &SReferenceSkeletonRow::IsSelected)
	];
}

TSharedRef<FBoneItemDragDropOp> FBoneItemDragDropOp::New(const TWeakPtr<FBoneElement>& InElement)
{
	TSharedRef<FBoneItemDragDropOp> Operation = MakeShared<FBoneItemDragDropOp>();
		Operation->Element = InElement;
		Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FBoneItemDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
        .Visibility(EVisibility::Visible)
        .BorderImage(FAppStyle::GetBrush("Menu.Background"))
        [
            SNew(STextBlock)
            .Text(FText::FromName(Element.Pin()->BoneName))
        ];
}

SReferenceSkeletonTree::SReferenceSkeletonTree()
{}

SReferenceSkeletonTree::~SReferenceSkeletonTree()
{
	// GEditor->UnregisterForUndo(this);
}

void SReferenceSkeletonTree::Construct(const FArguments& InArgs)
{
	Modifier = InArgs._Modifier;
	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString));
	CommandList = MakeShared<FUICommandList>();
	BindCommands();
	
	FRefSkeletonTreeDelegates Delegates;
	Delegates.OnCanAcceptDrop = FOnRefSkeletonTreeCanAcceptDrop::CreateSP(this, &SReferenceSkeletonTree::OnCanAcceptDrop);
	Delegates.OnAcceptDrop = FOnRefSkeletonTreeAcceptDrop::CreateSP(this, &SReferenceSkeletonTree::OnAcceptDrop);
	Delegates.OnDragDetected = FOnDragDetected::CreateSP(this, &SReferenceSkeletonTree::OnDragDetected);
	Delegates.OnBoneRenamed = FOnBoneRenamed::CreateSP(this, &SReferenceSkeletonTree::OnBoneRenamed);
	Delegates.OnBoneNameCommitted = FOnTextCommitted::CreateSP(this, &SReferenceSkeletonTree::OnNewBoneNameCommitted);
	
	ChildSlot
	[
		SNew( SOverlay )
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(6.f, 0.0))
				[
					SNew(SPositiveActionButton)
					.OnGetMenuContent(this, &SReferenceSkeletonTree::CreateAddNewMenu)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSearchBox)
					.SelectAllTextWhenFocused(true)
					.OnTextChanged( this, &SReferenceSkeletonTree::OnFilterTextChanged )
					.HintText(LOCTEXT( "SearchBoxHint", "Search Reference Skeleton Tree..."))
				]
			]
			
			+SVerticalBox::Slot()
			.Padding(0.0f, 0.0f)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SRefSkeletonTreeView)
					.TreeItemsSource(&RootElements)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow_Lambda( [this, Delegates](
						TSharedPtr<FBoneElement> InItem,
						const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
					{
						return SNew(SReferenceSkeletonRow, OwnerTable)
							.WeakModifier(Modifier)
							.TreeElement(InItem)
							.Delegates(Delegates);
					})
					.OnGetChildren(this, &SReferenceSkeletonTree::HandleGetChildrenForTree)
					.OnSelectionChanged(this, &SReferenceSkeletonTree::OnSelectionChanged)
					.OnContextMenuOpening(this, &SReferenceSkeletonTree::CreateContextMenu)
					.OnMouseButtonDoubleClick(this, &SReferenceSkeletonTree::OnItemDoubleClicked)
					.OnSetExpansionRecursive(this, &SReferenceSkeletonTree::OnSetExpansionRecursive)
					.ItemHeight(24)
					.HighlightParentNodesForSelection(true)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("Name")
						.ShouldGenerateWidget(true)
						.DefaultLabel(LOCTEXT("BoneNameLabel", "Name"))
						.FillWidth(0.5f)
					)
				]
			]
		]
    ];

	static constexpr bool IsInitialSetup = true;
	RefreshTreeView(IsInitialSetup);

	GEditor->RegisterForUndo(this);
}

void SReferenceSkeletonTree::PostUndo(bool bSuccess)
{
	RefreshTreeView(true);
}

void SReferenceSkeletonTree::PostRedo(bool bSuccess)
{
	RefreshTreeView(true);
}

void SReferenceSkeletonTree::AddItemToSelection(const TSharedPtr<FBoneElement>& InItem)
{
	TreeView->SetItemSelection(InItem, true, ESelectInfo::Direct);
	TreeView->RequestScrollIntoView(InItem);
}

void SReferenceSkeletonTree::RemoveItemFromSelection(const TSharedPtr<FBoneElement>& InItem)
{
	TreeView->SetItemSelection(InItem, false, ESelectInfo::Direct);
}

void SReferenceSkeletonTree::ReplaceItemInSelection(const FText& InOldName, const FText& InNewName)
{
	const FName OldName(*InOldName.ToString());
	const FName NewName(*InNewName.ToString());
	
	for (const TSharedPtr<FBoneElement>& Item : AllElements)
	{
		// remove old selection
		if (Item->BoneName.IsEqual(OldName))
		{
			TreeView->SetItemSelection(Item, false, ESelectInfo::Direct);
		}
		// add new selection
		if (Item->BoneName.IsEqual(NewName))
		{
			TreeView->SetItemSelection(Item, true, ESelectInfo::Direct);
		}
	}
}

TArray<TSharedPtr<FBoneElement>> SReferenceSkeletonTree::GetSelectedItems() const
{
	return TreeView->GetSelectedItems();
}

bool SReferenceSkeletonTree::HasSelectedItems() const
{
	return TreeView->GetNumItemsSelected() > 0;
}

void SReferenceSkeletonTree::BindCommands()
{
	const FSkeletalMeshModelingToolsCommands& Commands = FSkeletalMeshModelingToolsCommands::Get();
 
	CommandList->MapAction(Commands.NewBone,
        FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleNewBone),
        FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanAddNewBone));
	
	CommandList->MapAction(Commands.RemoveBone,
        FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleDeleteBone),
        FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanDeleteBone));
 
	CommandList->MapAction(Commands.RenameBone,
	FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleRenameBone),
		FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanRenameBone));
	
	CommandList->MapAction(Commands.UnParentBone,
		FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleUnParentBone),
		FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanUnParentBone));

	CommandList->MapAction(Commands.CopyBones,
		FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleCopyBones),
		FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanCopyBones));
	
	CommandList->MapAction(Commands.PasteBones,
		FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandlePasteBones),
		FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanPasteBones));
	
	CommandList->MapAction(Commands.DuplicateBones,
		FExecuteAction::CreateSP(this, &SReferenceSkeletonTree::HandleDuplicateBones),
		FCanExecuteAction::CreateSP(this, &SReferenceSkeletonTree::CanDuplicateBones));
}

void SReferenceSkeletonTree::HandleNewBone()
{
	if (!Modifier.IsValid())
	{
		return;
	}

	BeginChange();
	
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);
	
	const FName ParentName = BoneNames.IsEmpty() ? NAME_None : BoneNames[0];

	static const FName DefaultName("joint");
	const FName BoneName = Modifier->GetUniqueName(DefaultName);
	const bool bAdded = Modifier->AddBone(BoneName, ParentName, FTransform::Identity);
	if (bAdded)
	{
		RefreshTreeView(true);
		if (Notifier.IsValid())
		{
			Notifier->Notify( {BoneName}, ESkeletalMeshNotifyType::BonesAdded);
		}
		EndChange();
	}
	else
	{
		CancelChange();
	}
}

bool SReferenceSkeletonTree::CanAddNewBone() const
{
	return true;
}

void SReferenceSkeletonTree::HandleDeleteBone()
{
	if (!Modifier.IsValid())
	{
		return;
	}

	BeginChange();
	
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);
	
	const bool bRemoved = Modifier->RemoveBones(BoneNames, true);
	if (bRemoved)
	{
		RefreshTreeView(true);
		if (Notifier.IsValid())
		{
			Notifier->Notify(BoneNames, ESkeletalMeshNotifyType::BonesRemoved);
		}
		EndChange();
	}
	else
	{
		CancelChange();
	}
}

bool SReferenceSkeletonTree::CanDeleteBone() const
{
	return HasSelectedItems();
}

void SReferenceSkeletonTree::HandleUnParentBone()
{
	if (!Modifier.IsValid())
	{
		return;
	}

	BeginChange();
	
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);

	static const TArray<FName> Dummy;
	const bool bUnParented = Modifier->ParentBones(BoneNames, Dummy);
	if (bUnParented)
	{
		RefreshTreeView(true);
		SelectItemFromNames(BoneNames);
		if (Notifier.IsValid())
		{
			Notifier->Notify(BoneNames, ESkeletalMeshNotifyType::HierarchyChanged);
		}
		EndChange();
	}
	else
	{
		CancelChange();
	}
}

bool SReferenceSkeletonTree::CanUnParentBone() const
{
	return HasSelectedItems();
}

void SReferenceSkeletonTree::HandleCopyBones() const
{
	if (!ensure(Modifier.IsValid()))
	{
		return;
	}

	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);

	if (BoneNames.IsEmpty())
	{
		return;
	}

	SkeletonClipboard::CopyToClipboard(*Modifier.Get(), BoneNames);
}

bool SReferenceSkeletonTree::CanCopyBones() const
{
	return Modifier.IsValid() && HasSelectedItems();
}

void SReferenceSkeletonTree::HandlePasteBones()
{
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);

	const FName DefaultParent = BoneNames.IsEmpty() ? NAME_None : BoneNames[0];  

	BeginChange();
	
	const TArray<FName> NewBones = SkeletonClipboard::PasteFromClipboard(*Modifier.Get(), DefaultParent);
	if (NewBones.IsEmpty())
	{
		CancelChange();
		return;
	}

	static constexpr bool bRebuildAll = true;
	RefreshTreeView(bRebuildAll);
	static constexpr bool bFrameSelection = true;
	SelectItemFromNames(NewBones, bFrameSelection);

	if (Notifier.IsValid())
	{
		Notifier->Notify(NewBones, ESkeletalMeshNotifyType::HierarchyChanged);
		Notifier->Notify(NewBones, ESkeletalMeshNotifyType::BonesSelected);
	}

	EndChange();
}

bool SReferenceSkeletonTree::CanPasteBones() const
{
	return Modifier.IsValid() && SkeletonClipboard::IsClipboardValid();
}

void SReferenceSkeletonTree::HandleDuplicateBones()
{
	HandleCopyBones();

	if (!SkeletonClipboard::IsClipboardValid())
	{
		return;
	}  

	BeginChange();
	
	const TArray<FName> NewBones = SkeletonClipboard::PasteFromClipboard(*Modifier.Get(), NAME_None);
	if (NewBones.IsEmpty())
	{
		CancelChange();
		return;
	}

	static constexpr bool bRebuildAll = true;
	RefreshTreeView(bRebuildAll);
	static constexpr bool bFrameSelection = true;
	SelectItemFromNames(NewBones, bFrameSelection);

	if (Notifier.IsValid())
	{
		Notifier->Notify(NewBones, ESkeletalMeshNotifyType::HierarchyChanged);
		Notifier->Notify(NewBones, ESkeletalMeshNotifyType::BonesSelected);
	}

	EndChange();
}

bool SReferenceSkeletonTree::CanDuplicateBones() const
{
	return CanCopyBones();
}

void SReferenceSkeletonTree::GetSelectedBoneNames(TArray<FName>& OutSelectedBoneNames) const
{
	OutSelectedBoneNames.Reset();
	const TArray<TSharedPtr<FBoneElement>> BoneElements = TreeView->GetSelectedItems();
	OutSelectedBoneNames.Reserve(BoneElements.Num());

	Algo::Transform(BoneElements, OutSelectedBoneNames, [](const TSharedPtr<FBoneElement>& BoneElement)
	{
		return BoneElement->BoneName;
	});
}

void SReferenceSkeletonTree::SelectItemFromNames(const TArray<FName>& InBoneNames, bool bFrameSelection)
{
	for (const TSharedPtr<FBoneElement>& Item : AllElements)
	{
		const bool bSelect = InBoneNames.Contains(Item->BoneName);
		TreeView->SetItemSelection(Item, bSelect, ESelectInfo::Direct);

		if (bFrameSelection && bSelect)
		{
			TreeView->RequestScrollIntoView(Item);
			bFrameSelection = false;
		}
	}
}

ISkeletalMeshNotifier& SReferenceSkeletonTree::GetNotifier()
{
	if (!Notifier)
	{
		Notifier.Reset(new FReferenceSkeletonWidgetNotifier(SharedThis(this)));
	}
	return *Notifier;
}

void SReferenceSkeletonTree::HandleRenameBone() const
{
	TArray<TSharedPtr<FBoneElement>> BoneElements = TreeView->GetSelectedItems();
	if (BoneElements.IsEmpty())
	{
		return;
	}
	BoneElements[0]->RequestRename();
}

bool SReferenceSkeletonTree::CanRenameBone() const
{
	return HasSelectedItems();
}

void SReferenceSkeletonTree::OnBoneRenamed(const FName InOldName, const FName InNewName) const
{
	if (!Modifier.IsValid())
	{
		return;
	}

	if (Notifier.IsValid())
	{
		Notifier->Notify( {InNewName}, ESkeletalMeshNotifyType::BonesRenamed);
	}
}

void SReferenceSkeletonTree::OnNewBoneNameCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);
	
	const FName NewName = FName(InText.ToString());
	
	if (BoneNames.IsEmpty() || NewName == NAME_None)
	{
		return;
	}
	
	if (BoneNames.Num() == 1 && BoneNames[0] == NewName)
	{
		return;
	}

	TArray<int32> BoneIndices;
	const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
	Algo::Transform(BoneNames, BoneIndices, [&](const FName& BoneName)
	{
		return ReferenceSkeleton.FindRawBoneIndex(BoneName);
	});

	TArray<FName> NewNames;
	NewNames.Init(NewName, BoneNames.Num());
	
	BeginChange();

	const bool bBoneRenamed = Modifier->RenameBones(BoneNames, NewNames);
	if (bBoneRenamed)
	{
		const TArray<FMeshBoneInfo>& BoneInfos = ReferenceSkeleton.GetRawRefBoneInfo();
		for (int32 Index = 0; Index < BoneIndices.Num(); Index++)
		{
			if (BoneIndices[Index] != INDEX_NONE)
			{
				const FName OldName = BoneNames[Index];
				BoneNames[Index] = BoneInfos[BoneIndices[Index]].Name;
				const int32 ItemIndex = AllElements.IndexOfByPredicate([OldName](const TSharedPtr<FBoneElement>& Item)
				{
					return Item->BoneName == OldName;
				});

				if (ItemIndex != INDEX_NONE)
				{
					AllElements[ItemIndex]->BoneName = BoneNames[Index];
				}
			}
		}
		
		if (Notifier.IsValid())
		{
			Notifier->Notify(BoneNames, ESkeletalMeshNotifyType::BonesRenamed);
		}
		
		EndChange();
	}
	else
	{
		CancelChange();	
	}
}

void SReferenceSkeletonTree::OnFilterTextChanged(const FText& SearchText)
{
	TextFilter->SetFilterText(SearchText);
	RefreshTreeView();
}

void SReferenceSkeletonTree::RefreshTreeView(bool IsInitialSetup /*=false*/)
{
	if (!Modifier.IsValid())
	{
		return;
	}
	
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
	const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.GetRawRefBoneInfo();
	
	// reset all tree items
	RootElements.Reset();
	AllElements.Reset();

	// validate we have a skeleton to load
	if (BoneInfos.IsEmpty())
	{
		TreeView->RequestTreeRefresh();
		return;
	}
	
	// record bone element indices
	TMap<FName, int32> BoneTreeElementIndices;

	auto FilterString = [this](const FString& StringToTest) ->bool
	{
		return TextFilter->TestTextFilter(FBasicStringFilterExpressionContext(StringToTest));
	};

	TMap<int32, int32> PostponedParent;
	
	// create all bone elements
	for (int32 BoneIndex=0; BoneIndex < BoneInfos.Num(); ++BoneIndex)
	{
		const FName BoneName = BoneInfos[BoneIndex].Name;
		TSharedPtr<FBoneElement> BoneElement = MakeShared<FBoneElement>(BoneName, Modifier);
		const int32 BoneElementIndex = AllElements.Add(BoneElement);
		BoneTreeElementIndices.Add(BoneName, BoneElementIndex);

		// store pointer to parent (if there is one)
		const int32 ParentIndex = BoneInfos[BoneIndex].ParentIndex;
		if (ParentIndex != INDEX_NONE)
		{
			// get parent tree element
			const FName ParentBoneName = BoneInfos[ParentIndex].Name;

			const int32* FoundParent = BoneTreeElementIndices.Find(ParentBoneName);
			if (FoundParent)
			{ // set parent info directly
				const TSharedPtr<FBoneElement> ParentBoneTreeElement = AllElements[*FoundParent];
				BoneElement->UnFilteredParent = ParentBoneTreeElement;
			}
			else
			{ // postpone as the parent might not have been added to AllElements / BoneTreeElementIndices yet
				PostponedParent.Emplace(BoneElementIndex, ParentIndex);
			}
		}
		
		// apply text filter to bones
		if (!(TextFilter->GetFilterText().IsEmpty() || FilterString(BoneName.ToString())))
		{
			BoneElement->bIsHidden = true;
		}
	}

	// store pointer to parents that were postponed 
	for (const auto & [BoneElementIndex, ParentIndex] : PostponedParent)
	{
		const FName ParentBoneName = BoneInfos[ParentIndex].Name;
		const int32* FoundParent = BoneTreeElementIndices.Find(ParentBoneName);
		if (ensure(FoundParent))
		{
			const TSharedPtr<FBoneElement> BoneElement = AllElements[BoneElementIndex];
			const TSharedPtr<FBoneElement> ParentBoneTreeElement = AllElements[*FoundParent];
			BoneElement->UnFilteredParent = ParentBoneTreeElement;
		}
	}
	
	// resolve parent/children pointers on all tree elements, taking into consideration the filter options
	// (elements are parented to their nearest non-hidden/filtered parent element)
	for (TSharedPtr<FBoneElement>& Element : AllElements)
	{
		if (Element->bIsHidden)
		{
			continue;
		}
		
		// find first parent that is not filtered
		TSharedPtr<FBoneElement> ParentElement = Element->UnFilteredParent;
		while (true)
		{
			if (!ParentElement.IsValid())
			{
				break;
			}

			if (!ParentElement->bIsHidden)
			{
				break;
			}

			ParentElement = ParentElement->UnFilteredParent;
		}

		if (ParentElement.IsValid())
		{
			// store pointer to child on parent
			ParentElement->Children.Add(Element);
			// store pointer to parent on child
			Element->Parent = ParentElement;
		}
		else
		{
			// has no parent, store a root element
			RootElements.Add(Element);
		}
	}

	// expand all elements upon the initial construction of the tree
	for (const TSharedPtr<FBoneElement>& RootElement : RootElements)
	{
		SetExpansionRecursive(RootElement, false, true);
	}
	
	TreeView->RequestTreeRefresh();
}

void SReferenceSkeletonTree::HandleGetChildrenForTree(
	TSharedPtr<FBoneElement> InItem,
	TArray<TSharedPtr<FBoneElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

void SReferenceSkeletonTree::OnSelectionChanged(TSharedPtr<FBoneElement> InItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	
	if (!InItem)
	{
		if (Notifier.IsValid())
		{
			Notifier->Notify({}, ESkeletalMeshNotifyType::BonesSelected);
		}
		return;
	}
	
	TArray<FName> BoneNames;
	GetSelectedBoneNames(BoneNames);
	
	if (Notifier.IsValid())
	{
		Notifier->Notify(BoneNames, ESkeletalMeshNotifyType::BonesSelected);
	}
}

TSharedRef<SWidget> SReferenceSkeletonTree::CreateAddNewMenu()
{
	const FSkeletalMeshModelingToolsCommands& Commands = FSkeletalMeshModelingToolsCommands::Get();

	static constexpr bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, CommandList);
	
	MenuBuilder.BeginSection("NewBone", LOCTEXT("AddNewBoneOperations", "Bones"));
	MenuBuilder.AddMenuEntry(Commands.NewBone);
	MenuBuilder.EndSection();
		
	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SReferenceSkeletonTree::CreateContextMenu()
{
	static constexpr bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, CommandList);

	const FSkeletalMeshModelingToolsCommands& Commands = FSkeletalMeshModelingToolsCommands::Get();
	
	const TArray<TSharedPtr<FBoneElement>> SelectedItems = GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		MenuBuilder.BeginSection("NewBone", LOCTEXT("AddNewBoneOperations", "Bones"));
	
		MenuBuilder.AddMenuEntry(Commands.NewBone);

		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("CopyPasteBones", LOCTEXT("CopyPasteBonesOperations", "Copy & Paste"));

		MenuBuilder.AddMenuEntry(Commands.PasteBones);

		MenuBuilder.EndSection();
	}
	else
	{
		MenuBuilder.BeginSection("EditBones", LOCTEXT("EditBonesOperations", "Bones"));

		MenuBuilder.AddMenuEntry(Commands.NewBone);
		MenuBuilder.AddMenuEntry(Commands.RemoveBone);
		MenuBuilder.AddMenuEntry(Commands.UnParentBone);
		MenuBuilder.AddMenuEntry(Commands.RenameBone);

		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("CopyPasteBones", LOCTEXT("CopyPasteBonesOperations", "Copy & Paste"));

		MenuBuilder.AddMenuEntry(Commands.CopyBones);
		MenuBuilder.AddMenuEntry(Commands.PasteBones);
		MenuBuilder.AddMenuEntry(Commands.DuplicateBones);

		MenuBuilder.EndSection();
	}
	
	return MenuBuilder.MakeWidget();
}

void SReferenceSkeletonTree::OnItemDoubleClicked(TSharedPtr<FBoneElement> InItem)
{
	InItem->RequestRename();
}

void SReferenceSkeletonTree::OnSetExpansionRecursive(TSharedPtr<FBoneElement> InItem, bool bShouldBeExpanded)
{
	SetExpansionRecursive(InItem, false, bShouldBeExpanded);
}

void SReferenceSkeletonTree::SetExpansionRecursive(
	TSharedPtr<FBoneElement> InElement,
	bool bTowardsParent,
    bool bShouldBeExpanded)
{
	TreeView->SetItemExpansion(InElement, bShouldBeExpanded);
    
    if (bTowardsParent)
    {
    	if (InElement->Parent.Get())
    	{
    		SetExpansionRecursive(InElement->Parent, bTowardsParent, bShouldBeExpanded);
    	}
    }
    else
    {
    	for (int32 ChildIndex = 0; ChildIndex < InElement->Children.Num(); ++ChildIndex)
    	{
    		SetExpansionRecursive(InElement->Children[ChildIndex], bTowardsParent, bShouldBeExpanded);
    	}
    }
}

FReply SReferenceSkeletonTree::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!Modifier.IsValid())
	{
		return FReply::Handled();
	}
	
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SReferenceSkeletonTree::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const TArray<TSharedPtr<FBoneElement>> BoneElements = GetSelectedItems();
	if (BoneElements.Num() != 1)
	{
		return FReply::Unhandled();
	}
	
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const TSharedRef<FBoneItemDragDropOp> DragDropOp = FBoneItemDragDropOp::New(BoneElements[0]);
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}
	
	return FReply::Unhandled();
}

TOptional<EItemDropZone> SReferenceSkeletonTree::OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FBoneElement> TargetItem)
{
	TOptional<EItemDropZone> ReturnedDropZone;

	const TSharedPtr<FBoneItemDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FBoneItemDragDropOp>();
	if (DragDropOp.IsValid())
	{
		ReturnedDropZone = EItemDropZone::OntoItem;
	}

	return ReturnedDropZone;
}

FReply SReferenceSkeletonTree::OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FBoneElement> TargetItem)
{
	const TSharedPtr<FBoneItemDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FBoneItemDragDropOp>();
	if (!DragDropOp.IsValid())
	{
		return FReply::Unhandled();
	}

	if (!Modifier.IsValid())
	{
		return FReply::Handled();
	}
	
	const FName BoneName = DragDropOp->Element.Pin()->BoneName;
	const FName ParentName = TargetItem->BoneName;

	BeginChange();
	
	const bool bParented = Modifier->ParentBone(BoneName, ParentName);
	if (bParented)
	{
		RefreshTreeView(true);
		SelectItemFromNames({BoneName});
		if (Notifier.IsValid())
		{
			Notifier->Notify( {BoneName}, ESkeletalMeshNotifyType::HierarchyChanged);
			Notifier->Notify( {BoneName}, ESkeletalMeshNotifyType::BonesSelected);
		}
		EndChange();
	}
	else
	{
		CancelChange();
	}
	
	return FReply::Handled();
}

FReferenceSkeletonWidgetNotifier::FReferenceSkeletonWidgetNotifier(TSharedRef<SReferenceSkeletonTree> InWidget)
	: ISkeletalMeshNotifier()
	, Tree(InWidget)
{}

void FReferenceSkeletonWidgetNotifier::HandleNotification(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType)
{
	if (Notifying() || !Tree.IsValid())
	{
		return;
	}

	TSharedPtr<SReferenceSkeletonTree> TreePtr = Tree.Pin();
	switch (InNotifyType)
	{
		case ESkeletalMeshNotifyType::BonesAdded:
			TreePtr->RefreshTreeView();
			break;
		case ESkeletalMeshNotifyType::BonesRemoved:
			TreePtr->RefreshTreeView();
			break;
		case ESkeletalMeshNotifyType::BonesMoved:
			break;
		case ESkeletalMeshNotifyType::BonesSelected:
			{
				static constexpr bool bFrameSelection = true;
				TreePtr->SelectItemFromNames(InBoneNames, bFrameSelection);
				break;
			}
		case ESkeletalMeshNotifyType::BonesRenamed:
			TreePtr->RefreshTreeView();
			TreePtr->SelectItemFromNames(InBoneNames);
			break;
		case ESkeletalMeshNotifyType::HierarchyChanged:
			{
				TArray<FName> BoneNames;
				TreePtr->GetSelectedBoneNames(BoneNames);
				TreePtr->RefreshTreeView();
				TreePtr->SelectItemFromNames(BoneNames);
			}
			break;
	}
}

void SReferenceSkeletonTree::BeginChange()
{
	ensure( ActiveChange == nullptr );
	ActiveChange = MakeUnique<ReferenceSkeletonTreeLocals::FSkeletonModifierChange>(Modifier.Get()); 
}

void SReferenceSkeletonTree::EndChange()
{
	if (!ActiveChange.IsValid())
	{
		return;
	}

	ActiveChange->StoreSkeleton(Modifier.Get());
	
	GEditor->BeginTransaction(LOCTEXT("ModifyRefSkeleton", "Modify Reference Skeleton"));
	GUndo->StoreUndo(Modifier.Get(), MoveTemp(ActiveChange));
	GEditor->EndTransaction();
}

void SReferenceSkeletonTree::CancelChange()
{
	ActiveChange.Reset();
}

#undef LOCTEXT_NAMESPACE