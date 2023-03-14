// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMassProcessorsView.h"
#include "MassDebugger.h"
#include "SMassDebugger.h"
#include "SMassProcessor.h"
#include "MassDebuggerModel.h"
#include "MassEntitySettings.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SMassDebugger"

namespace UE::Mass::Debug::UI::Private
{
	const FText PickProcessorLabel = FText::FromString(TEXT("Pick a processor from the list"));
	const FText MissingDebugData = FText::FromString(TEXT("Missing debug data"));
}

//----------------------------------------------------------------------//
// SMassProcessorListTableRow
//----------------------------------------------------------------------//
using FMassDebuggerProcessorDataPtr = TSharedPtr<FMassDebuggerProcessorData, ESPMode::ThreadSafe>;

class SMassProcessorListTableRow : public STableRow<FMassDebuggerProcessorDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SMassProcessorListTableRow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FMassDebuggerProcessorDataPtr InEntryItem)
	{
		if (!InEntryItem.IsValid())
		{
			return;
		}

		Item = InEntryItem;
		
		STableRow<FMassDebuggerProcessorDataPtr>::Construct(STableRow<FMassDebuggerProcessorDataPtr>::FArguments(), InOwnerTableView);
		
		ChildSlot
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->Label))
		];
	}

	FMassDebuggerProcessorDataPtr Item;
};

class SMassProcessorTableRow : public STableRow<FMassDebuggerProcessorDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SMassProcessorTableRow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FMassDebuggerProcessorDataPtr InEntryItem)
	{
		Item = InEntryItem;

		STableRow<FMassDebuggerProcessorDataPtr>::Construct(STableRow<FMassDebuggerProcessorDataPtr>::FArguments(), InOwnerTableView);

		ChildSlot
		[
			SNew(SMassProcessor, Item)
		];
	}

	FMassDebuggerProcessorDataPtr Item;
};

//----------------------------------------------------------------------//
// SMassProcessorsView
//----------------------------------------------------------------------//
void SMassProcessorsView::Construct(const FArguments& InArgs, TSharedRef<FMassDebuggerModel> InDebuggerModel)
{
	Initialize(InDebuggerModel);

	ProcessorsBox = SNew(SVerticalBox);
	for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : DebuggerModel->SelectedProcessors)
	{
		ProcessorsBox->AddSlot()
		[
			SNew(SMassProcessor, ProcessorData)
		];
	}

	ChildSlot
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			 
			+ SSplitter::Slot()
			.Value(.3f)
			.MinSize(250.0f)
			[
				SAssignNew(ProcessorsListWidget, SListView<TSharedPtr<FMassDebuggerProcessorData> >)
					.ItemHeight(20)
					.ListItemsSource(&DebuggerModel->CachedProcessors)
					.SelectionMode(ESelectionMode::Multi)
					.OnSelectionChanged(this, &SMassProcessorsView::ProcessorListSelectionChanged)
					.OnGenerateRow_Lambda([](TSharedPtr<FMassDebuggerProcessorData> Item, const TSharedRef<STableViewBase>& OwnerTable)
						{
							return SNew(SMassProcessorListTableRow, OwnerTable, Item);
						})
			]
			
			+ SSplitter::Slot()
			.Value(.7f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+SScrollBox::Slot()
				[
					ProcessorsBox.ToSharedRef()
				]
			]
		];

	PopulateProcessorList();
}


void SMassProcessorsView::ProcessorListSelectionChanged(TSharedPtr<FMassDebuggerProcessorData> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	
	check(DebuggerModel);

	TArray<TSharedPtr<FMassDebuggerProcessorData>> CurrentlySelectedProcessors;
	ProcessorsListWidget->GetSelectedItems(CurrentlySelectedProcessors);
	DebuggerModel->SelectProcessors(CurrentlySelectedProcessors, SelectInfo);
}

void SMassProcessorsView::OnProcessorsSelected(TConstArrayView<TSharedPtr<FMassDebuggerProcessorData>> SelectedProcessors, ESelectInfo::Type SelectInfo)
{
	using namespace UE::Mass::Debug::UI::Private;
	using namespace UE::Mass::Debug;
	
	if (!DebuggerModel)
	{
		return;
	}

	if (SelectInfo == ESelectInfo::Direct)
	{
		ProcessorsListWidget->ClearSelection();
	}
	ProcessorsBox->ClearChildren();

	if (SelectedProcessors.Num())
	{		
		if (SelectInfo == ESelectInfo::Direct)
		{
			ProcessorsListWidget->SetItemSelection(SelectedProcessors, /*bSelected=*/true, ESelectInfo::OnMouseClick);
			// scroll to the first item to make sure there's anything in view
			ProcessorsListWidget->RequestScrollIntoView(SelectedProcessors[0]);
		}

		for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : DebuggerModel->SelectedProcessors)
		{
			ProcessorsBox->AddSlot()
				.AutoHeight()
				[
					SNew(SMassProcessor, ProcessorData)
				];
		}
	}
	ProcessorsListWidget->RequestListRefresh();
}

void SMassProcessorsView::OnArchetypesSelected(TConstArrayView<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes, ESelectInfo::Type SelectInfo)
{
	OnProcessorsSelected(DebuggerModel->SelectedProcessors, ESelectInfo::Direct);
}

void SMassProcessorsView::OnRefresh()
{
	PopulateProcessorList();
}

void SMassProcessorsView::PopulateProcessorList()
{
	check(DebuggerModel.IsValid());
	ProcessorsListWidget->ClearSelection();
	DebuggerModel->ClearProcessorSelection();

	ProcessorsListWidget->RequestListRefresh();

	if (ProcessorsBox)
	{
		ProcessorsBox->ClearChildren();
		for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : DebuggerModel->SelectedProcessors)
		{
			ProcessorsBox->AddSlot()
				.AutoHeight()
				[
					SNew(SMassProcessor, ProcessorData)
				];
		}
	}
}

#undef LOCTEXT_NAMESPACE

