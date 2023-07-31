// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectNodeObjectStatesView.h"

#include "Framework/Views/ITypedTableView.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Misc/Attribute.h"
#include "Misc/CString.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableRow.h"

class STableViewBase;
struct FSlateBrush;


#define LOCTEXT_NAMESPACE "CustomizableObjectNodeObjectStatesView"


void SCustomizableObjectNodeObjectSatesView::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;

	if (Node)
	{
		SAssignNew(VerticalSlots, SVerticalBox);

		for (int32 i = 0; i < Node->States.Num(); i++)
		{
			// Images and Runtime parameters widgets are stored in arrays to control the visibility
			RuntimeParametersListWidgets.Add(SNew(SCustomizableObjectRuntimeParameterList).Node(Node).StateIndex(i));
			RuntimeParametersListWidgets[i]->SetCollapsed(true);
			RuntimeParametersListWidgets[i]->SetVisibility(GetCollapsed(i));

			CollapsedArrows.Add(SNew(SImage).Image(GetExpressionPreviewArrow(i)));

			VerticalSlots->AddSlot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 0.0f, 3.0f, 5.0f)
				[
					SNew(SHorizontalBox)

					// State variable label
					+ SHorizontalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.AutoWidth()
					.Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString("State:"))
					]
					
					// State name
					+ SHorizontalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.AutoWidth()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Node->States[i].Name))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.Padding(10.0f, 0.0f, 3.0f, 3.0f)
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						// Collapsed Arrow checkbox
						+SHorizontalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Left)
						.AutoWidth()
						.Padding(0.0f, 0.0f, 3.0f, 0.0f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SCustomizableObjectNodeObjectSatesView::OnCollapseChanged, i)
							.IsChecked(ECheckBoxState::Unchecked)
							.Cursor(EMouseCursor::Default)
							.Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									CollapsedArrows[i].ToSharedRef()
								]
							]
						]
						
						// Number of runtime parameters
						+ SHorizontalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Left)
						.AutoWidth()
						.Padding(0.0f, 0.0f, 3.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("Runtime Parameters:  %d"), Node->States[i].RuntimeParameters.Num())))
						]

						+ SHorizontalBox::Slot()
						.HAlign(EHorizontalAlignment::HAlign_Left)
						.AutoWidth()
						.Padding(0.0f, 0.0f, 3.0f, 0.0f)
						[
							SNew(SButton)
							.OnClicked(this, &SCustomizableObjectNodeObjectSatesView::OnAddRuntimeParameterPressed,i)
							.ToolTipText(LOCTEXT("AddRuntimeParameter", "Add Runtime Parameter"))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush(TEXT("Plus")))
								]
							]
						]
					]
					
					// Runtime parameters widget
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.Padding(20.0f, 0.0f, 0.0f, 0.0f)
					.AutoHeight()
					[
						RuntimeParametersListWidgets[i].ToSharedRef()
					]
				]
			];
		}
	}

	// Add the widget to the child slot
	ChildSlot
	[
		VerticalSlots.ToSharedRef()
	];
}


void SCustomizableObjectNodeObjectSatesView::OnCollapseChanged(const ECheckBoxState NewCheckedState, int32 StateIndex)
{
	bool bCollapse = (NewCheckedState != ECheckBoxState::Checked);

	RuntimeParametersListWidgets[StateIndex]->SetCollapsed(bCollapse);
	RuntimeParametersListWidgets[StateIndex]->SetVisibility(GetCollapsed(StateIndex));
	CollapsedArrows[StateIndex]->SetImage(GetExpressionPreviewArrow(StateIndex));
}


EVisibility SCustomizableObjectNodeObjectSatesView::GetCollapsed(int32 StateIndex)
{
	return RuntimeParametersListWidgets[StateIndex]->IsCollapsed() ? EVisibility::Collapsed : EVisibility::Visible;
}


const FSlateBrush* SCustomizableObjectNodeObjectSatesView::GetExpressionPreviewArrow(int32 StateIndex) const
{
	return FAppStyle::GetBrush(RuntimeParametersListWidgets[StateIndex]->IsCollapsed() ? TEXT("SurfaceDetails.PanUPositive") : TEXT("SurfaceDetails.PanVPositive"));
}


FReply SCustomizableObjectNodeObjectSatesView::OnAddRuntimeParameterPressed(int32 StateIndex)
{
	Node->States[StateIndex].RuntimeParameters.Add("NONE");
	RuntimeParametersListWidgets[StateIndex]->BuildList();

	return FReply::Handled();
}


// Widget for a list ofruntime parameters --------------------------------------------------------------------------


void SCustomizableObjectRuntimeParameterList::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;
	StateIndex = InArgs._StateIndex;

	if (Node)
	{
		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Node->GetCustomizableObjectGraph()->GetOuter());

		if (CustomizableObject)
		{
			SAssignNew(VerticalSlots, SVerticalBox);

			if (VerticalSlots.IsValid())
			{
				BuildList();

				ChildSlot
				[
					VerticalSlots.ToSharedRef()
				];
			}
		}
	}

}


void SCustomizableObjectRuntimeParameterList::BuildList()
{
	if (VerticalSlots.IsValid())
	{
		VerticalSlots->ClearChildren();

		for (int32 i = 0; i < Node->States[StateIndex].RuntimeParameters.Num(); ++i)
		{
			VerticalSlots->AddSlot()
				.Padding(0.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCustomizableObjectRuntimeParameter)
					.Node(Node)
				.StateIndex(StateIndex)
				.RuntimeParameterIndex(i)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SCustomizableObjectRuntimeParameterList::OnDeleteRuntimeParameter, i)
				.ToolTipText(LOCTEXT("RemoveRuntimeParameter", "Remove Runtime Parameter"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Cross")))
				]
				]
				]
				];
		}
	}
}


FReply SCustomizableObjectRuntimeParameterList::OnDeleteRuntimeParameter(int32 ParameterIndex)
{
	Node->States[StateIndex].RuntimeParameters.RemoveAt(ParameterIndex);
	BuildList();

	return FReply::Handled();
}


// Widget for each Runtime parameter -------------------------------------------------------------------------------


void SCustomizableObjectRuntimeParameter::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;
	StateIndex = InArgs._StateIndex;
	RuntimeParameterIndex = InArgs._RuntimeParameterIndex;

	if (Node)
	{
		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Node->GetCustomizableObjectGraph()->GetOuter());

		if (CustomizableObject)
		{
			ListViewOptions.Empty();
			int32 ParameterCount = CustomizableObject->GetParameterCount();

			for (int32 i = 0; i < ParameterCount; ++i)
			{
				ListViewOptions.Add(MakeShareable(new FString(CustomizableObject->GetParameterName(i))));
			}

			for (int32 i = 0; i < Node->States[StateIndex].RuntimeParameters.Num(); ++i)
			{
				bool bFound = false;

				for (int32 j = 0; j < ListViewOptions.Num(); ++j)
				{
					if (*ListViewOptions[j].Get() == Node->States[StateIndex].RuntimeParameters[i])
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					ListViewOptions.Add(MakeShareable(new FString(Node->States[StateIndex].RuntimeParameters[i])));
				}
			}


			// Alphabetical Order
			ListViewOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
			{
				FString StringA = *A.Get();
				FString StringB = *B.Get();

				int32 length = StringA.Len() > StringB.Len() ? StringB.Len() : StringA.Len();

				for (int32 i = 0; i <length; ++i)
				{
					if (StringA[i] != StringB[i])
					{
						return StringA[i] < StringB[i];
					}
				}

				return true;
			}
			);

			ChildSlot
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 4.0f, 2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Parameter %d:"), RuntimeParameterIndex)))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(ComboButton,SComboButton)
					.OnGetMenuContent(this, &SCustomizableObjectRuntimeParameter::GetComboButtonContent)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SCustomizableObjectRuntimeParameter::GetCurrentItemLabel)
					]
				]
			];
		}
	}
}


TSharedRef<SWidget> SCustomizableObjectRuntimeParameter::GetComboButtonContent()
{
	SearchItem.Reset();

	// Listview Init
	SAssignNew(RowNameComboListView, SListView<TSharedPtr<FString> >)
		.ListItemsSource(&ListViewOptions)
		.OnSelectionChanged(this, &SCustomizableObjectRuntimeParameter::OnComboButtonSelectionChanged)
		.OnGenerateRow(this, &SCustomizableObjectRuntimeParameter::RowNameComboButtonGenarateWidget)
		.SelectionMode(ESelectionMode::Single);

	// Serachbox Init
	SearchBoxWidget = SNew(SSearchBox)
		.OnTextChanged(this, &SCustomizableObjectRuntimeParameter::OnSearchBoxFilterTextChanged)
		.OnTextCommitted(this, &SCustomizableObjectRuntimeParameter::OnSearchBoxFilterTextCommitted);
	
	//Setting the focus to the Searchbox when the Comob button is oppen
	ComboButton->SetMenuContentWidgetToFocus(SearchBoxWidget);

	// Creating a widget that gives navigation to the searchbox and the listview
	return  SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SearchBoxWidget.ToSharedRef()
			]
			+SVerticalBox::Slot()
			.MaxHeight(100.0f)
			[
				RowNameComboListView.ToSharedRef()
			];
}


void SCustomizableObjectRuntimeParameter::OnComboButtonSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::OnNavigation)
	{
		if (SelectedItem.IsValid())
		{
			// Sets the value of the displayed name of the combo button
			Node->States[StateIndex].RuntimeParameters[RuntimeParameterIndex] = *(SelectedItem.Get());

			//Close the combobox when a selection is made
			ComboButton->SetIsOpen(false);
		}
	}
}


TSharedRef<ITableRow> SCustomizableObjectRuntimeParameter::RowNameComboButtonGenarateWidget(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	//This is needed because the filter made in the function OnSearchBocFilterTextChanged Only works for the rendered items
	const EVisibility WidgetVisibility = IsItemVisible(InItem) ? EVisibility::Visible : EVisibility::Collapsed;

	return
		SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Visibility(WidgetVisibility)
		[
			SNew(STextBlock).Text(FText::FromString(*InItem))
		];
}


FText SCustomizableObjectRuntimeParameter::GetRowNameComboButtonContentText() const
{
	TSharedPtr<FString> SelectedRowName = ComboButtonSelection;

	if (SelectedRowName.IsValid())
	{
		return FText::FromString(*SelectedRowName);
	}
	else
	{
		return LOCTEXT("None", "None");
	}
}


FText SCustomizableObjectRuntimeParameter::GetCurrentItemLabel() const
{
	return FText::FromString(Node->States[StateIndex].RuntimeParameters[RuntimeParameterIndex]);
}


void SCustomizableObjectRuntimeParameter::OnSearchBoxFilterTextChanged(const FText& InText)
{
	SearchItem = InText.ToString();

	//This filter is just aplied to the items that are rendered of the ListView
	for (int32 i = 0; i < ListViewOptions.Num(); i++)
	{
		TSharedPtr<ITableRow> Row = RowNameComboListView->WidgetFromItem(ListViewOptions[i]);
		if (Row)
		{
			Row->AsWidget()->SetVisibility(IsItemVisible(ListViewOptions[i]) ? EVisibility::Visible : EVisibility::Collapsed);
		}
	}

	RowNameComboListView->RequestListRefresh();
}


void SCustomizableObjectRuntimeParameter::OnSearchBoxFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		bool bExists = false;

		for (int32 i = 0; i < ListViewOptions.Num(); i++)
		{
			TSharedPtr<ITableRow> Row = RowNameComboListView->WidgetFromItem(ListViewOptions[i]);
			if (Row && ListViewOptions[i].Get()->Equals(InText.ToString(), ESearchCase::IgnoreCase))
			{
				bExists = true;
				Node->States[StateIndex].RuntimeParameters[RuntimeParameterIndex] = *ListViewOptions[i].Get();
				break;
			}
		}

		if (!bExists)
		{			
			UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Node->GetCustomizableObjectGraph()->GetOuter());

			if (CustomizableObject)
			{
				Node->States[StateIndex].RuntimeParameters[RuntimeParameterIndex] = InText.ToString();
				ListViewOptions.Add(MakeShareable(new FString(InText.ToString())));
			}
		}

		ComboButton->SetIsOpen(false);
	}
}


bool SCustomizableObjectRuntimeParameter::IsItemVisible(TSharedPtr<FString> Item)
{
	bool bVisible = false;

	if (SearchItem == "" || Item.Get()->Contains(SearchItem, ESearchCase::IgnoreCase))
	{
		bVisible = true;
	}
	
	return bVisible;
}

#undef LOCTEXT_NAMESPACE
