// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRewindDebuggerComponentTree.h"

#include "ISequencerWidgetsModule.h"
#include "ObjectTrace.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "RewindDebuggerStyle.h"

#include "RewindDebugger.h"
#include "Widgets/Images/SLayeredImage.h"

#define LOCTEXT_NAMESPACE "SAnimationInsights"

SRewindDebuggerComponentTree::SRewindDebuggerComponentTree() 
	: SCompoundWidget()
	, DebugComponents(nullptr)
{ 
}

SRewindDebuggerComponentTree::~SRewindDebuggerComponentTree() 
{

}

class SRewindDebuggerComponentTreeTableRow : public STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>
{
public:
	
	SLATE_BEGIN_ARGS(SRewindDebuggerComponentTreeTableRow) {}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::Construct( STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::FArguments(), InOwnerTableView);
		Style =  &FRewindDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>("RewindDebugger.TableRow");

		SetHover(TAttribute<bool>::CreateLambda([this]()
		{
			if (TSharedPtr<ITypedTableView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>> TableView = OwnerTablePtr.Pin())
			{
				if (const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>* Track = GetItemForThis(TableView.ToSharedRef()))
				{
					return (*Track)->GetIsHovered();
				}
			}

			return false;
		}));
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (TSharedPtr<ITypedTableView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>> TableView = OwnerTablePtr.Pin())
		{
			if (const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>* Track = GetItemForThis(TableView.ToSharedRef()))
			{
				(*Track)->SetIsTreeHovered(true);
			}
		}
		STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::OnMouseEnter(MyGeometry, MouseEvent);
	}
	
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::OnMouseLeave(MouseEvent);

		if (TSharedPtr<ITypedTableView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>> TableView = OwnerTablePtr.Pin())
		{
			if (const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>* Track = GetItemForThis(TableView.ToSharedRef()))
			{
				(*Track)->SetIsTreeHovered(false);
			}
		}
	}

};


TSharedRef<ITableRow> SRewindDebuggerComponentTree::ComponentTreeViewGenerateRow(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FSlateIcon ObjectIcon = InItem->GetIcon();

	const TSharedRef<SLayeredImage> LayeredIcons = SNew(SLayeredImage)
				.DesiredSizeOverride(FVector2D(16, 16))
				.Image(ObjectIcon.GetIcon());

	if (ObjectIcon.GetOverlayIcon())
	{
		LayeredIcons->AddLayer(ObjectIcon.GetOverlayIcon());
	}
	
	TSharedRef<SRewindDebuggerComponentTreeTableRow> Row = SNew(SRewindDebuggerComponentTreeTableRow, OwnerTable);


	Row->SetContent(
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				LayeredIcons
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(1.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InItem->GetDisplayName())
				.Font_Lambda([this,  InItem]() { return InItem->GetIsHovered() ? FCoreStyle::GetDefaultFontStyle("Bold", 10) : FCoreStyle::GetDefaultFontStyle("Regular", 10); })
			]
		);

	return Row;
}

void ComponentTreeViewGetChildren(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>& OutChildren)
{
	InItem->IterateSubTracks([&OutChildren](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Track)
	{
		if (Track->IsVisible())
		{
			OutChildren.Add(Track);
		}
	});
}

void SRewindDebuggerComponentTree::ComponentTreeViewExpansionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, bool bShouldBeExpanded)
{
	InItem->SetIsExpanded(bShouldBeExpanded);
	OnExpansionChanged.ExecuteIfBound();
}

void SRewindDebuggerComponentTree::SetSelection(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem)
{
	ComponentTreeView->SetSelection(SelectedItem);
}

void SRewindDebuggerComponentTree::ScrollTo(double ScrollOffset)
{
	ComponentTreeView->SetScrollOffset(ScrollOffset);
}

void SRewindDebuggerComponentTree::Construct(const FArguments& InArgs)
{
	DebugComponents = InArgs._DebugComponents;

	ComponentTreeView = SNew(STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>)
									.ItemHeight(20.0f)
									.TreeItemsSource(DebugComponents)
									.OnGenerateRow(this, &SRewindDebuggerComponentTree::ComponentTreeViewGenerateRow)
									.OnGetChildren_Static(&ComponentTreeViewGetChildren)
									.OnExpansionChanged(this, &SRewindDebuggerComponentTree::ComponentTreeViewExpansionChanged)
									.SelectionMode(ESelectionMode::Single)
									.OnSelectionChanged(InArgs._OnSelectionChanged)
									.OnMouseButtonDoubleClick(InArgs._OnMouseButtonDoubleClick)
									.ExternalScrollbar(InArgs._ExternalScrollBar)
									.AllowOverscroll(EAllowOverscroll::No)
									.OnTreeViewScrolled(InArgs._OnScrolled)
									.ScrollbarDragFocusCause(EFocusCause::SetDirectly)
									.OnContextMenuOpening(InArgs._OnContextMenuOpening);

	ChildSlot
	[
		ComponentTreeView.ToSharedRef()
	];

	OnExpansionChanged = InArgs._OnExpansionChanged;
}

static void RestoreExpansion(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Track, TSharedPtr<STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>>& TreeView)
{
	TreeView->SetItemExpansion(Track, Track->GetIsExpanded());
	Track->IterateSubTracks([&TreeView](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SubTrack)
	{
		RestoreExpansion(SubTrack, TreeView);
	});
}

void SRewindDebuggerComponentTree::RestoreExpansion()
{
	for (TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& Track : *DebugComponents)
	{
		::RestoreExpansion(Track, ComponentTreeView);
	}
}

void SRewindDebuggerComponentTree::Refresh()
{
	ComponentTreeView->RebuildList();

	if (DebugComponents)
	{
		// make sure any newly added TreeView nodes are created expanded
		RestoreExpansion();
	}
}

#undef LOCTEXT_NAMESPACE
