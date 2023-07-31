// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/PinViewer/SPinViewer.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/PinViewer/SPinViewerListRow.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "Types/SlateEnums.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

class ITableRow;
class STableViewBase;
class UObject;
struct EVisibility;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


const FName SPinViewer::COLUMN_NAME(TEXT("Name"));

const FName SPinViewer::COLUMN_TYPE(TEXT("Type"));

const FName SPinViewer::COLUMN_VISIBILITY(TEXT("Visible"));


FText SPinViewer::GetPinName(const UEdGraphPin& Pin)
{
	if (!Pin.PinFriendlyName.IsEmpty())
	{
		return Pin.PinFriendlyName;
	}
	else
	{
		return FText::FromName(Pin.PinName);
	}
}


/** Sort pin references by name. The pin references to be valid. */
bool SortPinsByName(const TSharedPtr<FEdGraphPinReference>& PinReferenceA, const TSharedPtr<FEdGraphPinReference>& PinReferenceB)
{
	const FText PinNameA = SPinViewer::GetPinName(*PinReferenceA->Get());
	const FText PinNameB = SPinViewer::GetPinName(*PinReferenceB->Get());

	return PinNameA.CompareTo(PinNameB, ETextComparisonLevel::Default) < 0;
}


/** Sort pin references by the provided method. The pin references have to be valid. */
void Sort(const FName& Method, TArray<TSharedPtr<FEdGraphPinReference>>& Pins)
{
	if (Method == SPinViewer::COLUMN_NAME)
	{
		Pins.Sort(&SortPinsByName);
	}
	else if (Method == SPinViewer::COLUMN_TYPE)
	{
		Pins.Sort([](const TSharedPtr<FEdGraphPinReference>& PinReferenceA, const TSharedPtr<FEdGraphPinReference>& PinReferenceB)
			{
				if (PinReferenceA->Get()->PinType.PinCategory == PinReferenceB->Get()->PinType.PinCategory)
				{
					return SortPinsByName(PinReferenceA, PinReferenceB);
				}
				else if (PinReferenceA->Get()->PinType.PinCategory.FastLess(PinReferenceB->Get()->PinType.PinCategory))
				{
					return true;
				}
				else
				{
					return false;
				}
			});
	}
	else if (Method == SPinViewer::COLUMN_VISIBILITY)
	{
		Pins.Sort([](const TSharedPtr<FEdGraphPinReference>& PinReferenceA, const TSharedPtr<FEdGraphPinReference>& PinReferenceB)
			{
				if (PinReferenceA->Get()->bHidden == PinReferenceB->Get()->bHidden)
				{
					return SortPinsByName(PinReferenceA, PinReferenceB);
				}
				else if (PinReferenceA->Get()->bHidden < PinReferenceB->Get()->bHidden)
				{
					return false;
				}
				else
				{
					return true;
				}
			});
	}
	else
	{
		check(false); // Unknown method.
	}
}


void SPinViewer::GeneratePinInfoList()
{
	PinReferences.Reset();
	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		if (CurrentFilter.IsEmpty() || GetPinName(*Pin).ToString().Contains(CurrentFilter))
		{
			PinReferences.Add(MakeShareable(new FEdGraphPinReference(Pin)));
		}
	}

	Sort(CurrentSortColumn, PinReferences);
}


void SPinViewer::PinsRemapped(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap)
{
	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		if (const EVisibility* Result = AdditionalWidgetVisibility.Find(Pair.Key->PinId))
		{
			AdditionalWidgetVisibility.Add(Pair.Value->PinId, *Result);
		}
	}
}


void SPinViewer::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;
	Node->PostReconstructNodeDelegate.AddSP(this, &SPinViewer::UpdateWidget);
	Node->NodeConnectionListChangedDelegate.AddSP(this, &SPinViewer::UpdateWidget);
	Node->RemapPinsDelegate.AddSP(this, &SPinViewer::PinsRemapped);

	GeneratePinInfoList();
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f,5.0f,0.0f,0.0f)
		.AutoHeight()
		[
			SNew(SSearchBox)
			.SelectAllTextWhenFocused(true)
			.OnTextChanged(this, &SPinViewer::OnFilterTextChanged)
		]

		+ SVerticalBox::Slot()
		.Padding(0.0f,5.0f,0.0f,5.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("Text","Show All"))
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("ShowAllTooltip","Show all pins"))
				.OnClicked(this, &SPinViewer::OnShowAllPressed)
			]
			
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("HideAllText", "Hide All"))
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("ShowLinkedTooltip", "Hide all non connnected pins"))
				.OnClicked(this, &SPinViewer::OnHideAllPressed)
			]
		]

		+SVerticalBox::Slot()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(ListView, SListView<TSharedPtr<FEdGraphPinReference>>)
				.ListItemsSource(&PinReferences)
				.OnGenerateRow(this, &SPinViewer::GenerateNodePinRow)
				.ItemHeight(22.0f)
				.SelectionMode(ESelectionMode::Single)
				.IsFocusable(true)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(COLUMN_NAME)
					.DefaultLabel(LOCTEXT("PinNameLabel", "Name"))
					.OnSort(this, &SPinViewer::SortListView)
					.FillWidth(0.5)

					+ SHeaderRow::Column(COLUMN_TYPE)
					.DefaultLabel(LOCTEXT("TypeLabel", "Type"))
					.OnSort(this, &SPinViewer::SortListView)
					.FillWidth(0.25)

					+ SHeaderRow::Column(COLUMN_VISIBILITY)
					.DefaultLabel(LOCTEXT("VisibilityLabel", "Visibility"))
					.OnSort(this, &SPinViewer::SortListView)
					.FillWidth(0.25)
				)
			]
		]
	];
}


void SPinViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Node);
}


void SPinViewer::UpdateWidget()
{
	GeneratePinInfoList();
	ListView->RequestListRefresh();
}


TSharedRef<ITableRow> SPinViewer::GenerateNodePinRow(TSharedPtr<FEdGraphPinReference> PinReference, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPinViewerListRow, OwnerTable)
		.PinViewer(this)
		.PinReference(*PinReference);
}


void SPinViewer::OnFilterTextChanged(const FText& SearchText)
{
	CurrentFilter = SearchText.ToString();
	UpdateWidget();
}


FReply SPinViewer::OnShowAllPressed() const
{
	FScopedTransaction Transaction(LOCTEXT("ShowAllPinsTransaction", "Show All Pins"));
	Node->Modify();
	Node->SetPinHidden(Node->GetAllNonOrphanPins(), false);

	return FReply::Handled();
}


FReply SPinViewer::OnHideAllPressed() const
{
	FScopedTransaction Transaction(LOCTEXT("HideAllPinsTransaction", "Hide All Pins"));
	Node->Modify();
	Node->SetPinHidden(Node->GetAllNonOrphanPins(), true);

	return FReply::Handled();
}


void SPinViewer::SortListView(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	CurrentSortColumn = ColumnId;
	UpdateWidget();
}


void PinViewerAttachToDetailCustomization(IDetailLayoutBuilder& DetailBuilder)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetDetailsView()->GetSelectedObjects();
	if (SelectedObjects.Num())
	{
		UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(SelectedObjects[0].Get());

		IDetailCategoryBuilder& PinViewerCategoryBuilder = DetailBuilder.EditCategory("PinViewer", FText::GetEmpty(), ECategoryPriority::Uncommon);
		PinViewerCategoryBuilder.AddCustomRow(LOCTEXT("PinViewerDetailsCategory", "PinViwer"))
		[
			SNew(SPinViewer).Node(Node)
		];
	}
}


#undef LOCTEXT_NAMESPACE
