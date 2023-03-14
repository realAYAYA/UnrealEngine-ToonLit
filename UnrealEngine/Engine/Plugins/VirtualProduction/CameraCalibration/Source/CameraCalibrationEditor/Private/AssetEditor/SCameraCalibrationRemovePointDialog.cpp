// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCameraCalibrationRemovePointDialog.h"

#include "Editor.h"
#include "LensFile.h"
#include "ScopedTransaction.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SCameraCalibrationRemovePointDialog"

class SRemoveLensDataItem;

/**
 * Checkbox row represents item to remove
 */
class SRemoveLensDataItem : public STableRow<TSharedPtr<FRemoveLensDataListItem>>
{
	SLATE_BEGIN_ARGS(SRemoveLensDataItem)
		:  _EntryLabel(FText::GetEmpty())
		,  _EntryValue(0.f)
	{}

		SLATE_ARGUMENT(FText, EntryLabel)
		SLATE_ARGUMENT(float, EntryValue)

		/** Called when the checked state has changed */
		SLATE_EVENT( FOnCheckStateChanged, OnCheckStateChanged )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FRemoveLensDataListItem> InItemData);

private:

	/** WeakPtr to source data item */
	TWeakPtr<FRemoveLensDataListItem> WeakItem;

	/** Checkbox widget */
	TSharedPtr<SCheckBox> CheckBox;
};

/**
 * Base Remove item data model
 */
class FRemoveLensDataListItem : public TSharedFromThis<FRemoveLensDataListItem>
{
public:
	FRemoveLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, bool bInChecked);
	virtual ~FRemoveLensDataListItem() = default;

	/** Remove Request handler */
	virtual void OnRemoveRequested(bool bIncludeChildren = false) const = 0;

	/** Generate the row widget */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) = 0;

protected:
	/** Check box state changes handler */
	virtual void OnCheckStateChanged(ECheckBoxState NewState) = 0;

public:
	/** Whether row selected */
	bool IsSelected() const;

	/** Get current checkbox state value */
	ECheckBoxState GetCheckBoxState() const { return CheckBoxState; }

	/** Check box setter */
	void SetCheckBoxState(const ECheckBoxState InState) { CheckBoxState = InState; }

	/** Get children of this item */
	const TArray<TSharedPtr<FRemoveLensDataListItem>>& GetChildren() const { return Children; }

	/** Add child to the children array */
	void AddChild(const TSharedPtr<FRemoveLensDataListItem> InItem) { Children.Add(InItem); }

	/** Get the check box state attribute */
	const TAttribute<ECheckBoxState>& GetCheckBoxStateAttribute() const { return CheckBoxStateAttribute; }

protected:
	/** LensFile we're editing */
	TWeakObjectPtr<ULensFile> WeakLensFile;

	/** Lens data category of that entry */
	ELensDataCategory Category;

	/** Children of this item */
	TArray<TSharedPtr<FRemoveLensDataListItem>> Children;

	/** Delegate to call when data is removed */
	FSimpleDelegate OnDataRemovedCallback;

	/** Check box selection state */
	ECheckBoxState CheckBoxState;

	/** Check box attribute */
	TAttribute<ECheckBoxState> CheckBoxStateAttribute;

	/** On check box state changes delegate */
	FOnCheckStateChanged OnCheckStateChangedDelegate;

	/** This point friendly name */
	FName PointName;
};

/**
 * Focus point remove item data model
 */
class FFocusRemoveLensDataListItem final : public FRemoveLensDataListItem
{
public:
	FFocusRemoveLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, bool bInChecked, float InFocus)
		: FRemoveLensDataListItem(InLensFile, InCategory, bInChecked)
		, Focus(InFocus)
	{}

public:
	//~ Begin FRemoveLensDataListItem interface
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) override;
	virtual void OnRemoveRequested(bool bIncludeChildren = false) const override;

	/** Get float value for this item */
	float GetFocus() const { return Focus; }

protected:
	virtual void OnCheckStateChanged(ECheckBoxState NewState) override;
	//~ End FRemoveLensDataListItem interface

private:
	/** Focus value of this item */
	float Focus;
};

/**
 * Focus point remove item data model
 */
class FZoomRemoveLensDataListItem final : public FRemoveLensDataListItem
{
public:
	FZoomRemoveLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, bool bInChecked, float InZoom, const TSharedRef<FFocusRemoveLensDataListItem> InParent)
		: FRemoveLensDataListItem(InLensFile, InCategory, bInChecked)
		, Zoom(InZoom)
		, WeakParent(InParent)
		, Focus(InParent->GetFocus())
	{}

	FZoomRemoveLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, bool bInChecked, float InZoom, float InFocus)
		: FRemoveLensDataListItem(InLensFile, InCategory, bInChecked)
		, Zoom(InZoom)
		, Focus(InFocus)
	{}

	//~ Begin FRemoveLensDataListItem interface
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) override;
	virtual void OnRemoveRequested(bool) const override;
	virtual void OnCheckStateChanged(ECheckBoxState NewState) override;
	//~ End FRemoveLensDataListItem interface

	/** Zoom value of this item */
	float Zoom;

	/** Optional pointer for parent item */
	TWeakPtr<FFocusRemoveLensDataListItem> WeakParent;

	/** Focus value of this item */
	float Focus;
};

FRemoveLensDataListItem::FRemoveLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, bool bInChecked)
	: WeakLensFile(InLensFile)
	, Category(InCategory)
{
	OnCheckStateChangedDelegate = FOnCheckStateChanged::CreateRaw(this, &FRemoveLensDataListItem::OnCheckStateChanged);
	if (bInChecked)
	{
		CheckBoxState = ECheckBoxState::Checked;
	}
	else
	{
		CheckBoxState = ECheckBoxState::Unchecked;
	}

	PointName = FBaseLensTable::GetFriendlyPointName(InCategory);
	CheckBoxStateAttribute = MakeAttributeLambda([this]() { return CheckBoxState; });
}

bool FRemoveLensDataListItem::IsSelected() const
{
	return CheckBoxState == ECheckBoxState::Checked;
}

TSharedRef<ITableRow> FFocusRemoveLensDataListItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	const FText EntryLabel = FText::Format(LOCTEXT("FocusLabel", "{0}. Focus:"), FText::FromName(PointName));
	return SNew(SRemoveLensDataItem, InOwnerTable, SharedThis(this))
		.EntryLabel(EntryLabel)
		.EntryValue(Focus)
		.OnCheckStateChanged(OnCheckStateChangedDelegate);
}
	
void FFocusRemoveLensDataListItem::OnRemoveRequested(bool bIncludeChildren) const
{
	if (bIncludeChildren)
	{
		for (TSharedPtr<FRemoveLensDataListItem> Child : Children)
		{
			Child->OnRemoveRequested(bIncludeChildren);
		}
	}

	if (IsSelected())
	{
		WeakLensFile.Get()->RemoveFocusPoint(Category, Focus);
	}
}

void FFocusRemoveLensDataListItem::OnCheckStateChanged(ECheckBoxState NewState)
{
	CheckBoxState = NewState;
			
	for (TSharedPtr<FRemoveLensDataListItem> Child : Children)
	{
		if (NewState != Child->GetCheckBoxState())
		{
			Child->SetCheckBoxState(NewState);
		}
	}
}

TSharedRef<ITableRow> FZoomRemoveLensDataListItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	FText EntryLabel = LOCTEXT("ZoomLabelIncomplete", "Zoom: ");

	if (!WeakParent.IsValid())
	{
		EntryLabel = FText::Format(LOCTEXT("ZoomLabelComplete", "{0}. Focus: {1}, Zoom: "), FText::FromName(PointName), Focus);
	}
		
	return SNew(SRemoveLensDataItem, InOwnerTable, SharedThis(this))
		.EntryLabel(EntryLabel)
		.EntryValue(Zoom)
		.OnCheckStateChanged(OnCheckStateChangedDelegate);
}
	
void FZoomRemoveLensDataListItem::OnRemoveRequested(bool) const
{
	if (IsSelected())
	{
		WeakLensFile.Get()->RemoveZoomPoint(Category, Focus, Zoom);
	}
}

void FZoomRemoveLensDataListItem::OnCheckStateChanged(const ECheckBoxState NewState)
{
	CheckBoxState = NewState;
			
	if (TSharedPtr<FFocusRemoveLensDataListItem> ParentPtr = WeakParent.Pin())
	{
		if (CheckBoxState == ECheckBoxState::Unchecked)
		{
			ParentPtr->SetCheckBoxState(CheckBoxState);
		}
		else if (CheckBoxState == ECheckBoxState::Checked)
		{
			bool bAllChildChecked = true;
			for (TSharedPtr<FRemoveLensDataListItem> Child : ParentPtr->GetChildren())
			{
				bAllChildChecked &= Child->GetCheckBoxState() == ECheckBoxState::Checked;
			}

			if (bAllChildChecked)
			{
				ParentPtr->SetCheckBoxState(CheckBoxState);
			}
		}
	}
}

void SRemoveLensDataItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FRemoveLensDataListItem> InItemData)
{
	WeakItem = InItemData;
	const float EntryValue = InArgs._EntryValue;

	STableRow<TSharedPtr<FRemoveLensDataListItem>>::Construct(
		STableRow<TSharedPtr<FRemoveLensDataListItem>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SAssignNew(CheckBox, SCheckBox)
					.IsChecked(InItemData->GetCheckBoxStateAttribute())
					.OnCheckStateChanged(InArgs._OnCheckStateChanged)
			]
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 5.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(InArgs._EntryLabel)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([EntryValue](){ return FText::AsNumber(EntryValue); })

			]
		], OwnerTable);
}

SCameraCalibrationRemovePointDialog::SCameraCalibrationRemovePointDialog()
	: Category(ELensDataCategory::Iris) // Initialize some default category
{
}

void SCameraCalibrationRemovePointDialog::Construct(const FArguments& InArgs, const TSharedRef<SWindow>& InWindow,
                                                    ULensFile* InLensFile, const ELensDataCategory InCategory,
                                                    const float InFocus, TOptional<float> InZoom)
{
	WindowWeakPtr = InWindow;
	WeakLensFile = InLensFile;
	Category = InCategory;
	Focus = InFocus;
	Zoom = InZoom;
	OnDataRemoved = InArgs._OnDataRemoved;

	const FBaseLensTable* const BaseDataTable = InLensFile->GetDataTable(Category);
	if (!ensure(BaseDataTable))
	{
		ChildSlot
		[
			SNew(STextBlock).Text(LOCTEXT("NoBaseTableErrorLabel", "No Base Table Error"))
		];
		
		return;
	}
	
	const FText DialogText = LOCTEXT("DialogTextLabel", "The calibration data you wish to delete may be inherently linked to additional data.\nChoose any and all linked data you wish to delete.");

	const TSharedPtr<SWidget> ButtonsWidget = [this]()
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SCameraCalibrationRemovePointDialog::OnRemoveButtonClicked)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("RemoveSelectedLabel", "Remove Selected"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SCameraCalibrationRemovePointDialog::OnCancelButtonClicked)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("CancelLabel", "Cancel"))
			];
	}();

	ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .Padding(5.0f, 5.0f)
        .AutoHeight()
        [
        	SNew(SBorder)
        	.HAlign(HAlign_Center)
        	.VAlign(VAlign_Center)
        	.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
        	[
        		SNew(STextBlock).Text(DialogText)
        	]
        ]
        + SVerticalBox::Slot()
        .Padding(5.0f, 5.0f)
        .FillHeight(1.f)
        [
        	SAssignNew(RemoveItemsTree, STreeView<TSharedPtr<FRemoveLensDataListItem>>)
			.TreeItemsSource(&RemoveItems)
			.ItemHeight(24.0f)
			.OnGenerateRow(this, &SCameraCalibrationRemovePointDialog::OnGenerateDataEntryRow)
			.OnGetChildren(this, &SCameraCalibrationRemovePointDialog::OnGetDataEntryChildren)
			.ClearSelectionOnClick(false)
        ]
        + SVerticalBox::Slot()
        .Padding(5.0f, 5.0f)
        .AutoHeight()
        [
        	SNew(SBorder)
        	.HAlign(HAlign_Fill)
        	.VAlign(VAlign_Fill)
        	.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
        	[
        		ButtonsWidget.ToSharedRef()
        	]
        ]
    ];

	Refresh();
}

void SCameraCalibrationRemovePointDialog::OpenWindow(ULensFile* InLensFile,
                                                     ELensDataCategory InCategory,
                                                     FSimpleDelegate OnDataRemoved,
                                                     float InFocus,
                                                     TOptional<float> InZoom)
{
	TSharedRef<SWindow> ModalWindow = SNew(SWindow)
		.Title( LOCTEXT("RemovePointsTitle", "Remove Points"))
		.ClientSize(FVector2D(570,420));

	const TSharedRef<SCameraCalibrationRemovePointDialog> DialogBox = SNew(
		SCameraCalibrationRemovePointDialog, ModalWindow, InLensFile, InCategory, InFocus, InZoom).OnDataRemoved(OnDataRemoved);

	ModalWindow->SetContent(DialogBox);

	GEditor->EditorAddModalWindow( ModalWindow );
}

void SCameraCalibrationRemovePointDialog::RemoveSelected()
{
	constexpr bool bIncludeChildren = true;

	for (const TSharedPtr<FRemoveLensDataListItem>& RemoveItem : RemoveItems)
	{
		RemoveItem->OnRemoveRequested(bIncludeChildren);	
	}

	Refresh();
}

void SCameraCalibrationRemovePointDialog::Refresh()
{
	RefreshRemoveItemsTree();
}

void SCameraCalibrationRemovePointDialog::RefreshRemoveItemsTree()
{
	// Empty the array of data model
	RemoveItems.Empty();
	
	const FBaseLensTable* const BaseDataTable = WeakLensFile.Get()->GetDataTable(Category);
	if (!ensure(BaseDataTable))
	{
		return;	
	}

	// If removing zoom point only
	if (Zoom.IsSet())
	{
		const float ZoomValue = Zoom.GetValue();

		// Add entry for zoom
		constexpr bool bChecked = true;
		const TSharedPtr<FZoomRemoveLensDataListItem> ZoomItem = MakeShared<FZoomRemoveLensDataListItem>(WeakLensFile.Get(), Category, bChecked, ZoomValue, Focus);
		RemoveItems.Add(ZoomItem);

		// Generate list for Linked Category
		BaseDataTable->ForEachLinkedFocusPoint([this, ZoomValue](const FBaseFocusPoint& InFocusPoint, ELensDataCategory InCategory, FLinkPointMetadata LinkPointMeta)
		{
			for(int32 Index = 0; Index < InFocusPoint.GetNumPoints(); ++Index)
			{
				// Check only points with given Zoom value
				const float FocusPointZoomValue = InFocusPoint.GetZoom(Index);
				if (!FMath::IsNearlyEqual(FocusPointZoomValue ,ZoomValue))
				{
					continue;
				}
		
				// Add zoom points for this focus
				const TSharedPtr<FZoomRemoveLensDataListItem> LinkedZoomItem = MakeShared<FZoomRemoveLensDataListItem>(WeakLensFile.Get(), InCategory, LinkPointMeta.bRemoveByDefault, ZoomValue, Focus);
				RemoveItems.Add(LinkedZoomItem);
			}
		}, Focus);
	}
	// If removing focus value with zoom values inside
	else
	{
		// Generate list of Focus and Zoom for this category
		BaseDataTable->ForEachFocusPoint([this](const FBaseFocusPoint& InFocusPoint)
		{
			//Add entry for focus
			constexpr bool bChecked = true;
			const TSharedPtr<FFocusRemoveLensDataListItem> RemoveFocus = MakeShared<FFocusRemoveLensDataListItem>(WeakLensFile.Get(), Category, bChecked, InFocusPoint.GetFocus());
			RemoveItems.Add(RemoveFocus);

			RemoveItemsTree->SetItemExpansion(RemoveFocus, true);

			for(int32 Index = 0; Index < InFocusPoint.GetNumPoints(); ++Index)
			{
				//Add zoom points for this focus
				RemoveFocus->AddChild(MakeShared<FZoomRemoveLensDataListItem>(WeakLensFile.Get(), Category, bChecked, InFocusPoint.GetZoom(Index), RemoveFocus.ToSharedRef()));
			}
		}, Focus);

		// Generate for Linked Category
		BaseDataTable->ForEachLinkedFocusPoint([this](const FBaseFocusPoint& InFocusPoint, ELensDataCategory InCategory, FLinkPointMetadata LinkPointMeta)
		{		
			//Add entry for focus
			const TSharedPtr<FFocusRemoveLensDataListItem> RemoveFocus = MakeShared<FFocusRemoveLensDataListItem>(WeakLensFile.Get(), InCategory, LinkPointMeta.bRemoveByDefault, InFocusPoint.GetFocus());
			RemoveItems.Add(RemoveFocus);
			RemoveItemsTree->SetItemExpansion(RemoveFocus, true);

			for(int32 Index = 0; Index < InFocusPoint.GetNumPoints(); ++Index)
			{
				//Add zoom points for this focus
				RemoveFocus->AddChild(MakeShared<FZoomRemoveLensDataListItem>(WeakLensFile.Get(), InCategory, LinkPointMeta.bRemoveByDefault, InFocusPoint.GetZoom(Index), RemoveFocus.ToSharedRef()));
			}
		}, Focus);
	}

	// When data entries have been repopulated, refresh the tree.
	RemoveItemsTree->RequestListRefresh();
}

TSharedRef<ITableRow> SCameraCalibrationRemovePointDialog::OnGenerateDataEntryRow(
	TSharedPtr<FRemoveLensDataListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return InItem->MakeTreeRowWidget(OwnerTable);
}

void SCameraCalibrationRemovePointDialog::OnGetDataEntryChildren(TSharedPtr<FRemoveLensDataListItem> InItem,
	TArray<TSharedPtr<FRemoveLensDataListItem>>& OutNodes)
{
	if (InItem.IsValid())
	{
		OutNodes = InItem->GetChildren();
	}
}

FReply SCameraCalibrationRemovePointDialog::OnRemoveButtonClicked()
{
	if (!ensure(WeakLensFile.IsValid()))
	{
		return FReply::Handled();
	}
					
	// Apply transaction
	FScopedTransaction Transaction(LOCTEXT("RemovePointTransaction", "Remove Point"));
	WeakLensFile->Modify();

	// Remove selected items
	RemoveSelected();

	// Execute callback
	OnDataRemoved.ExecuteIfBound();

	// Destroy modal window
	WindowWeakPtr.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

FReply SCameraCalibrationRemovePointDialog::OnCancelButtonClicked() const
{
	WindowWeakPtr.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE /* SCameraCalibrationRemovePointDialog */
