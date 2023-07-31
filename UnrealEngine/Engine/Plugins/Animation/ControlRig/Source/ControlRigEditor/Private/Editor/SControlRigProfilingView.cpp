// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SControlRigProfilingView.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ControlRigEditorStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"
#include "HAL/ConsoleManager.h"

#include "Framework/Application/SlateApplication.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "SControlRigProfilingView"

class FStatsHierarchicalClient
{
public:

	static TSharedPtr<FJsonValue> ToJson(const FStatsTreeElement* Element)
	{
		if (!Element)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
		Data->SetStringField(TEXT("Name"), Element->GetName());
		Data->SetStringField(TEXT("Path"), Element->GetPath());
		Data->SetNumberField(TEXT("Cycles"), (double)Element->TotalCycles());
		Data->SetNumberField(TEXT("Invocations"), (double)Element->Num());

		if (Element->GetChildren().Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildArray;
			for (const TSharedPtr<FStatsTreeElement>& Child : Element->GetChildren())
			{
				TSharedPtr<FJsonValue> ChildData = ToJson(Child.Get());
				if (ChildData)
				{
					ChildArray.Add(ChildData);
				}
			}
			Data->SetArrayField(TEXT("Children"), ChildArray);
		}

		return MakeShareable(new FJsonValueObject(Data));
	}

	static TSharedPtr<FStatsTreeElement> FromJson(const FJsonObject* Data)
	{
		TSharedPtr<FStatsTreeElement> Element = MakeShareable(new FStatsTreeElement);

		const TArray<TSharedPtr<FJsonValue>>* ChildArray;
		if (Data->TryGetArrayField(TEXT("Markers"), ChildArray))
		{
			Element->Invocations = 1;

			for (const TSharedPtr<FJsonValue>& ChildData : *ChildArray)
			{
				const TSharedPtr<FJsonObject>* ChildObj = nullptr;
				if (ChildData->TryGetObject(ChildObj))
				{
					Element->Children.Add(FStatsHierarchicalClient::FromJson(ChildObj->Get()));
				}
			}

			Element->UpdatePostMeasurement();
		}
		else
		{
			Element->Name = *Data->GetStringField(TEXT("Name"));
			Element->Path = Data->GetStringField(TEXT("Path"));
			Element->Cycles = (uint32)Data->GetNumberField(TEXT("Cycles"));
			Element->Invocations = (uint32)Data->GetNumberField(TEXT("Invocations"));

			if (Data->TryGetArrayField(TEXT("Children"), ChildArray))
			{
				for (const TSharedPtr<FJsonValue>& ChildData : *ChildArray)
				{
					const TSharedPtr<FJsonObject>* ChildObj = nullptr;
					if (ChildData->TryGetObject(ChildObj))
					{
						Element->Children.Add(FStatsHierarchicalClient::FromJson(ChildObj->Get()));
					}
				}
			}
		}
		return Element;
	}

	static void GetAllElementsInline(const FStatsTreeElement* Element, TArray<TSharedPtr<FStatsTreeElement>>& OutElements)
	{
		for (const TSharedPtr<FStatsTreeElement>& Child : Element->Children)
		{
			OutElements.Add(Child);
			GetAllElementsInline(Child.Get(), OutElements);
		}
	}

	static void GetAllElementsCombined(const FStatsTreeElement* Element, TArray<TSharedPtr<FStatsTreeElement>>& OutElements, TMap<FName, TSharedPtr<FStatsTreeElement>>& InOutMap)
	{
		bool bIsRoot = InOutMap.Num() == 0;

		for (const TSharedPtr<FStatsTreeElement>& Child : Element->Children)
		{
			TSharedPtr<FStatsTreeElement>* ExistingElementPtr = InOutMap.Find(Child->GetFName());
			if (ExistingElementPtr)
			{
				TSharedPtr<FStatsTreeElement>& ExistingElement = *ExistingElementPtr;
				ExistingElement->Cycles += Child->Cycles;
				ExistingElement->CyclesOfChildren += Child->CyclesOfChildren;
				ExistingElement->Invocations += Child->Invocations;
			}
			else
			{
				TSharedPtr<FStatsTreeElement> NewElement = MakeShareable(new FStatsTreeElement);
				NewElement->Name = Child->GetFName();
				NewElement->Path = Child->GetName();
				NewElement->Cycles = Child->Cycles;
				NewElement->CyclesOfChildren = Child->CyclesOfChildren;
				NewElement->Invocations = Child->Invocations;
				OutElements.Add(NewElement);
				InOutMap.Add(NewElement->GetFName(), NewElement);
			}
			GetAllElementsCombined(Child.Get(), OutElements, InOutMap);
		}

		if (bIsRoot)
		{
			TSharedPtr<FStatsTreeElement> TempParentElement = MakeShareable(new FStatsTreeElement);
			TempParentElement->Children = OutElements;
			TempParentElement->UpdatePostMeasurement();
		}
	}

	static bool LeafChildPathContains(const FStatsTreeElement* Element, const FString& BasePath, const FString& SearchText)
	{
		if (Element == nullptr)
		{
			return false;
		}

		if (Element->Children.Num() == 0)
		{
			FString Path = Element->GetPath().RightChop(BasePath.Len());
			return Path.Contains(SearchText);
		}

		for (const TSharedPtr<FStatsTreeElement>& Child : Element->Children)
		{
			const FStatsTreeElement* ChildPtr = Child.Get();
			if (LeafChildPathContains(ChildPtr, BasePath, SearchText))
			{
				return true;
			}
		}

		return false;
	}
};

//////////////////////////////////////////////////////////////
/// SControlRigProfilingItem
///////////////////////////////////////////////////////////

FName SControlRigProfilingItem::NAME_MarkerName(TEXT("Marker"));
FName SControlRigProfilingItem::NAME_TotalTimeInclusive(TEXT("Total"));
FName SControlRigProfilingItem::NAME_TotalTimeExclusive(TEXT("Total (ex)"));
FName SControlRigProfilingItem::NAME_AverageTimeInclusive(TEXT("Average"));
FName SControlRigProfilingItem::NAME_AverageTimeExclusive(TEXT("Average (ex)"));
FName SControlRigProfilingItem::NAME_Invocations(TEXT("Calls"));

void SControlRigProfilingItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, SControlRigProfilingView* InProfilingView, TSharedPtr<FStatsTreeElement> InTreeElement)
{
	WeakTreeElement = InTreeElement;
	ProfilingView = InProfilingView;

	SMultiColumnTableRow<TSharedPtr<FStatsTreeElement>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FStatsTreeElement>>::FArguments()
			.Padding(0)	
		, InOwnerTable);

	UpdateVisibilityFromSearch(ProfilingView->SearchText);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SControlRigProfilingItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == NAME_MarkerName)
	{
		return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
			.IndentAmount(16)
			.ShouldDrawWires(true)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(this, &SControlRigProfilingItem::GetLabelText)
			.ToolTipText(this, &SControlRigProfilingItem::GetToolTipText)
			.ColorAndOpacity(this, &SControlRigProfilingItem::GetTextColor)
			.OnDoubleClicked(this, &SControlRigProfilingItem::OnDoubleClicked)
		];
	}
	else if (ColumnName == NAME_TotalTimeInclusive)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(this, &SControlRigProfilingItem::GetTotalTimeText, true)
			];
	}
	else if (ColumnName == NAME_TotalTimeExclusive)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(this, &SControlRigProfilingItem::GetTotalTimeText, false)
			];
	}
	else if (ColumnName == NAME_AverageTimeInclusive)
	{
		return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(this, &SControlRigProfilingItem::GetAverageTimeText, true)
		];
	}
	else if (ColumnName == NAME_AverageTimeExclusive)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(this, &SControlRigProfilingItem::GetAverageTimeText, false)
			];
	}
	else if (ColumnName == NAME_Invocations)
	{
		return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(this, &SControlRigProfilingItem::GetInvocationsText)
		];
	}

	return SNullWidget::NullWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SControlRigProfilingItem::GetLabelText() const
{
	const FName& Name = WeakTreeElement.Pin()->GetFName();
	if (Name == FStatsHierarchical::GetUntrackedTimeName())
	{
		return FText::FromString(TEXT("Unknown..."));
	}
	return (FText::FromName(Name));
}

FText SControlRigProfilingItem::GetToolTipText() const
{
	FString Path = WeakTreeElement.Pin()->GetPath();
	
	FString Tooltip;
	FString Indent;

	FString Left, Right;
	while (Path.Split(TEXT("."), &Left, &Right))
	{
		Tooltip += Indent + Left + TEXT("\n");
		Indent = Indent + TEXT("    ");
		Path = Right;
	}
	Tooltip += Indent + Path;

	return FText::FromString(Tooltip);
}

FSlateColor SControlRigProfilingItem::GetTextColor() const
{
	static FVector4f Green(FLinearColor::Green);
	static FVector4f Red(FLinearColor::Red);
	FVector4f Color = FMath::Lerp<FVector4f>(Green, Red, FMath::Clamp<float>((float)WeakTreeElement.Pin()->Contribution(true /* against max */), 0.f, 1.f));
	return FSlateColor(FLinearColor(Color));
}

FText SControlRigProfilingItem::GetTotalTimeText(bool bInclusive) const
{
	return FText::FromString(FString::Printf(TEXT("%.03f ms"), float(WeakTreeElement.Pin()->TotalSeconds(bInclusive) * 1000.0)));
}

FText SControlRigProfilingItem::GetAverageTimeText(bool bInclusive) const
{
	return FText::FromString(FString::Printf(TEXT("%.03f ms"), float(WeakTreeElement.Pin()->AverageSeconds(bInclusive) * 1000.0)));
}

FText SControlRigProfilingItem::GetInvocationsText() const
{
	return FText::FromString(FString::FormatAsNumber((int32)WeakTreeElement.Pin()->Num()));
}

FReply SControlRigProfilingItem::OnDoubleClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
{
	ProfilingView->ToggleItemExpansion(WeakTreeElement.Pin(), true /* recursive */);
	return FReply::Handled();
}

void SControlRigProfilingItem::UpdateVisibilityFromSearch(const FString& InSearchText)
{
	if (InSearchText.IsEmpty())
	{
		SetVisibility(EVisibility::Visible);
		return;
	}

	if (WeakTreeElement.Pin()->GetName().Contains(InSearchText))
	{
		SetVisibility(EVisibility::Visible);
		return;
	}

	if (ProfilingView->DisplayMode != TEXT("Hierarchical"))
	{
		SetVisibility(EVisibility::Hidden);
		return;
	}

	bool bFoundMatch = FStatsHierarchicalClient::LeafChildPathContains(WeakTreeElement.Pin().Get(), WeakTreeElement.Pin()->GetPath(), InSearchText);
	SetVisibility(bFoundMatch ? EVisibility::Visible : EVisibility::Hidden);
}

//////////////////////////////////////////////////////////////
/// SControlRigProfilingView
///////////////////////////////////////////////////////////

SControlRigProfilingView::~SControlRigProfilingView()
{
}

void SControlRigProfilingView::Construct( const FArguments& InArgs)
{
	SortColumn = SControlRigProfilingItem::NAME_MarkerName;
	bSortAscending = false;
	bShowUntracked = false;

	RecordTime = TEXT("1 s");
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("1/120 s"))));
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("1/60 s"))));
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("1/2 s"))));
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("1 s"))));
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("5 s"))));
	RecordTimeOptions.Add(MakeShareable(new FName(TEXT("10 s"))));

	DisplayMode = TEXT("Hierarchical");
	DisplayModeOptions.Add(MakeShareable(new FName(TEXT("Hierarchical"))));
	DisplayModeOptions.Add(MakeShareable(new FName(TEXT("Flat (separate)"))));
	DisplayModeOptions.Add(MakeShareable(new FName(TEXT("Flat (combined)"))));

	ParallelAnimEvaluationVarPrevValue = 0;

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(4.0f, 4.0f)
			.AutoHeight()
			.MaxHeight(40)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked(this, &SControlRigProfilingView::HandleProfilingButton)
					.Text(this, &SControlRigProfilingView::GetProfilingButtonText)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(10)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&RecordTimeOptions)
					.OnSelectionChanged(this, &SControlRigProfilingView::OnComboBoxChanged, &RecordTime)
					.OnGenerateWidget(this, &SControlRigProfilingView::OnGetComboBoxWidget)
					.Content()
					[
						SNew(STextBlock).Text(this, &SControlRigProfilingView::GetComboBoxValueAsText, &RecordTime)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(30)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowUntrackedMarkers", "Record unmarked time:"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SControlRigProfilingView::UntrackedCheckBoxCheckStateChanged)
					.IsChecked(this, &SControlRigProfilingView::GetUntrackedCheckBoxCheckState)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(60)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked(this, &SControlRigProfilingView::HandleLoadFromFile)
					.Text(LOCTEXT("ControlRigProfilingViewLoadFromFile", "Load"))
					.IsEnabled(this, &SControlRigProfilingView::IsLoadingEnabled)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.OnClicked(this, &SControlRigProfilingView::HandleSaveToFile)
					.Text(LOCTEXT("ControlRigProfilingViewSaveToFile", "Save"))
					.IsEnabled(this, &SControlRigProfilingView::IsSavingEnabled)
				]
			]

			+ SVerticalBox::Slot()
			.Padding(4.0f, 4.0f)
			.AutoHeight()
			.MaxHeight(40)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.MaxWidth(250)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(250)
					.Font(FAppStyle::GetFontStyle("ContentBrowser.AssetTileViewNameFont"))
					.Text(GetSearchText())
					.HintText(LOCTEXT("Search", "Search"))
					.OnTextCommitted(this, &SControlRigProfilingView::OnSearchTextCommitted)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(10)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DisplayMode", "DisplayMode:"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(3)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&DisplayModeOptions)
					.OnSelectionChanged(this, &SControlRigProfilingView::OnComboBoxChanged, &DisplayMode)
					.OnGenerateWidget(this, &SControlRigProfilingView::OnGetComboBoxWidget)
					.Content()
					[
						SNew(STextBlock).Text(this, &SControlRigProfilingView::GetComboBoxValueAsText, &DisplayMode)
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f)
			.FillHeight(1)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderBackgroundColor(this, &SControlRigProfilingView::GetTreeBackgroundColor)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FStatsTreeElement>>)
					.ItemHeight(28)
					.Visibility(this, &SControlRigProfilingView::GetTreeVisibility)
					.TreeItemsSource(&RootChildren)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &SControlRigProfilingView::MakeTableRowWidget)
					.OnGetChildren(this, &SControlRigProfilingView::HandleGetChildrenForTree)
					.HighlightParentNodesForSelection(true)
					.HeaderRow
					(
						SNew(SHeaderRow)

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_MarkerName)
						.DefaultLabel(LOCTEXT("MarkerName", "Marker Name"))
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FillWidth(0.8f)

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_TotalTimeInclusive)
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FixedWidth(90.0f)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TotalMilisecondsInc", "Total"))
							.ToolTipText(LOCTEXT("TotalMilisecondsIncTooltip", "Total time in Miliseconds (including children)"))
						]

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_TotalTimeExclusive)
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FixedWidth(90)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TotalMilisecondsExc", "Total (ex)"))
							.ToolTipText(LOCTEXT("TotalMilisecondsExcTooltip", "Total time in Miliseconds (excluding children)"))
						]

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_AverageTimeInclusive)
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FixedWidth(90)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AverageMilisecondsInc", "Average"))
							.ToolTipText(LOCTEXT("AverageMilisecondsIncTooltip", "Average time in Miliseconds (including children)"))
						]

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_AverageTimeExclusive)
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FixedWidth(90)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AverageMilisecondsExc", "Average (ex)"))
							.ToolTipText(LOCTEXT("AverageMilisecondsExcTooltip", "Average time in Miliseconds (excluding children)"))
						]

						+ SHeaderRow::Column(SControlRigProfilingItem::NAME_Invocations)
						.OnSort(FOnSortModeChanged::CreateSP(this, &SControlRigProfilingView::OnSortColumnHeader))
						.FixedWidth(70)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NumberOfCalls", "Calls"))
							.ToolTipText(LOCTEXT("NumberOfCallsTooltip", "Total number of invocations"))
						]
					)
				]
			]
		];

	RefreshTreeView();
}

TSharedRef<ITableRow> SControlRigProfilingView::MakeTableRowWidget(TSharedPtr<FStatsTreeElement> InTreeElement, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SControlRigProfilingItem, OwnerTable, this, InTreeElement);
}

void SControlRigProfilingView::OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	if (FStatsHierarchical::IsEnabled())
	{
		return;
	}

	if (SortColumn == ColumnId)
	{
		bSortAscending = !bSortAscending;
	}

	SortColumn = ColumnId;

	RefreshTreeView();
}

void SControlRigProfilingView::HandleGetChildrenForTree(TSharedPtr<FStatsTreeElement> InItem, TArray<TSharedPtr<FStatsTreeElement>>& OutChildren)
{
	if (DisplayMode == TEXT("Hierarchical"))
	{
		OutChildren.Append(InItem->GetChildren());
		SortChildren(OutChildren);
	}
}

void SControlRigProfilingView::SortChildren(TArray<TSharedPtr<FStatsTreeElement>>& InOutChildren) const
{
	bool bRequiresSort = SortColumn != SControlRigProfilingItem::NAME_MarkerName;
	if (!bRequiresSort)
	{
		bRequiresSort = DisplayMode != TEXT("Hierarchical");
	}

	if (bRequiresSort)
	{
		Algo::Sort(InOutChildren, [&](const TSharedPtr<FStatsTreeElement>& A, const TSharedPtr<FStatsTreeElement>& B)
			{
				if (SortColumn == SControlRigProfilingItem::NAME_MarkerName)
				{
					if (bSortAscending)
					{
						return A->GetName().Compare(B->GetName()) < 0;
					}
					return A->GetName().Compare(B->GetName()) > 0;
				}
				if (SortColumn == SControlRigProfilingItem::NAME_TotalTimeInclusive)
				{
					if (bSortAscending)
					{
						return A->TotalCycles(true) < B->TotalCycles(true);
					}
					return A->TotalCycles(true) > B->TotalCycles(true);
				}
				if (SortColumn == SControlRigProfilingItem::NAME_TotalTimeExclusive)
				{
					if (bSortAscending)
					{
						return A->TotalCycles(false) < B->TotalCycles(false);
					}
					return A->TotalCycles(false) > B->TotalCycles(false);
				}
				if (SortColumn == SControlRigProfilingItem::NAME_AverageTimeInclusive)
				{
					if (bSortAscending)
					{
						return A->AverageSeconds(true) < B->AverageSeconds(true);
					}
					return A->AverageSeconds(true) > B->AverageSeconds(true);
				}
				if (SortColumn == SControlRigProfilingItem::NAME_AverageTimeExclusive)
				{
					if (bSortAscending)
					{
						return A->AverageSeconds(false) < B->AverageSeconds(false);
					}
					return A->AverageSeconds(false) > B->AverageSeconds(false);
				}

				// fall through on invocations
				if (bSortAscending)
				{
					return A->Num() < B->Num();
				}
				return A->Num() > B->Num();
			}
		);
	}
}

EVisibility SControlRigProfilingView::GetTreeVisibility() const
{
	return FStatsHierarchical::IsEnabled() ? EVisibility::Hidden : EVisibility::Visible;
}

FSlateColor SControlRigProfilingView::GetTreeBackgroundColor() const
{
	return FSlateColor(FStatsHierarchical::IsEnabled() ? FLinearColor(3, 0, 0, 1) : FLinearColor::White);
}

FText SControlRigProfilingView::GetProfilingButtonText() const
{
	return FText::FromString(FStatsHierarchical::IsEnabled() ? TEXT("Stop") : TEXT("Record"));
}

FReply SControlRigProfilingView::HandleProfilingButton()
{
	if (FStatsHierarchical::IsEnabled())
	{
		// todo: aggregate if need be
		RootElement = FStatsHierarchical::EndMeasurements(FStatsTreeElement(), bShowUntracked);

		// optionally re-enable parallel anim eval
		FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();
		IConsoleVariable* ParallelAnimEvaluationVar = ConsoleManager.FindConsoleVariable(TEXT("a.ParallelAnimEvaluation"));
		if (ParallelAnimEvaluationVar)
		{
			if (ParallelAnimEvaluationVarPrevValue != ParallelAnimEvaluationVar->AsVariableInt()->GetValueOnAnyThread())
			{
				*ParallelAnimEvaluationVar->AsVariableInt() = ParallelAnimEvaluationVarPrevValue;
			}
		}

		RefreshTreeView();
	}
	else
	{
		// clear the Tree
		RootChildren.Reset();
		TreeView->RequestTreeRefresh();

		// disable parallel evaluation of the animBP
		FConsoleManager& ConsoleManager = (FConsoleManager&)IConsoleManager::Get();
		IConsoleVariable* ParallelAnimEvaluationVar = ConsoleManager.FindConsoleVariable(TEXT("a.ParallelAnimEvaluation"));
		if (ParallelAnimEvaluationVar)
		{
			ParallelAnimEvaluationVarPrevValue = ParallelAnimEvaluationVar->AsVariableInt()->GetValueOnAnyThread();
			*ParallelAnimEvaluationVar->AsVariableInt() = 0;
		}

		FStatsHierarchical::BeginMeasurements();

		double TimerSeconds = 0.0;
		if(RecordTime.IsEqual(TEXT("1/120 s")))
		{
			TimerSeconds = 1.0 / 120.0;
		}
		if(RecordTime.IsEqual(TEXT("1/60 s")))
		{
			TimerSeconds = 1.0 / 60.0;
		}
		if(RecordTime.IsEqual(TEXT("1/2 s")))
		{
			TimerSeconds = 0.5;
		}
		if(RecordTime.IsEqual(TEXT("1 s")))
		{
			TimerSeconds = 1.0;
		}
		if(RecordTime.IsEqual(TEXT("5 s")))
		{
			TimerSeconds = 5.0;
		}
		if(RecordTime.IsEqual(TEXT("10 s")))
		{
			TimerSeconds = 10.0;
		}

		RegisterActiveTimer(TimerSeconds, FWidgetActiveTimerDelegate::CreateSP(this, &SControlRigProfilingView::OnRecordingTimerFinished));
	}

	return FReply::Handled();
}

FText SControlRigProfilingView::GetSearchText() const
{
	return FText::FromString(SearchText);
}

TSharedRef<SWidget> SControlRigProfilingView::OnGetComboBoxWidget(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock).Text(FText::FromName(InItem.IsValid() ? *InItem : NAME_None));
}

void SControlRigProfilingView::OnComboBoxChanged(TSharedPtr<FName> InItem, ESelectInfo::Type InSeletionInfo, FName* OutValue)
{
	*OutValue = *InItem;

	if (OutValue == &DisplayMode)
	{
		RefreshTreeView();
	}
}

FText SControlRigProfilingView::GetComboBoxValueAsText(FName* InValue) const
{
	return FText::FromName(*InValue);
}

EActiveTimerReturnType SControlRigProfilingView::OnRecordingTimerFinished(double InCurrentTime, float InDeltaTime)
{
	if (FStatsHierarchical::IsEnabled())
	{
		HandleProfilingButton();
	}
	return EActiveTimerReturnType::Stop;
}

void SControlRigProfilingView::UntrackedCheckBoxCheckStateChanged(ECheckBoxState CheckState)
{
	bShowUntracked = CheckState == ECheckBoxState::Checked;
}

ECheckBoxState SControlRigProfilingView::GetUntrackedCheckBoxCheckState() const
{
	return bShowUntracked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SControlRigProfilingView::IsLoadingEnabled() const
{
	if (FStatsHierarchical::IsEnabled())
	{
		return false;
	}
	return true;
}

bool SControlRigProfilingView::IsSavingEnabled() const
{
	if (FStatsHierarchical::IsEnabled())
	{
		return false;
	}
	return RootElement.GetChildren().Num() > 0;
}

FReply SControlRigProfilingView::HandleLoadFromFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	const FText Title = LOCTEXT("ImportFromJson", "Import markers from JSON...");
	const FString FileTypes = TEXT("Profiling JSON (*.json)|*.json");

	TArray<FString> OutFilenames;
	DesktopPlatform->OpenFileDialog(
		ParentWindowWindowHandle,
		Title.ToString(),
		TEXT(""),
		TEXT("Profiling.json"),
		FileTypes,
		EFileDialogFlags::None,
		OutFilenames
	);

	if (OutFilenames.Num() == 0)
	{
		return FReply::Unhandled();
	}

	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *OutFilenames[0]))
	{

		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonText);
		TSharedPtr<FJsonObject> RootData;
		if (FJsonSerializer::Deserialize(JsonReader, RootData))
		{
			TSharedPtr<FStatsTreeElement> ParsedElement = FStatsHierarchicalClient::FromJson(RootData.Get());
			if (ParsedElement)
			{
				RootElement = *ParsedElement.Get();
				RefreshTreeView();
			}
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SControlRigProfilingView::HandleSaveToFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	const FText Title = LOCTEXT("ExportToJson", "Export markers as JSON...");
	const FString FileTypes = TEXT("Profiling JSON (*.json)|*.json");

	TArray<FString> OutFilenames;
	DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		Title.ToString(),
		TEXT(""),
		TEXT("Profiling.json"),
		FileTypes,
		EFileDialogFlags::None,
		OutFilenames
	);

	if (OutFilenames.Num() == 0)
	{
		return FReply::Unhandled();
	}

	TSharedPtr<FJsonObject> RootData = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> RootArray;

	for (const TSharedPtr<FStatsTreeElement>& Child : RootElement.GetChildren())
	{
		TSharedPtr<FJsonValue> ChildData = FStatsHierarchicalClient::ToJson(Child.Get());
		if (ChildData)
		{
			RootArray.Add(ChildData);
		}
	}

	RootData->SetArrayField(TEXT("Markers"), RootArray);

	FString JsonText;
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonText);
	if (FJsonSerializer::Serialize(RootData.ToSharedRef(), JsonWriter))
	{
		FFileHelper::SaveStringToFile(JsonText, *OutFilenames[0]);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SControlRigProfilingView::RefreshTreeView()
{
	if (DisplayMode == TEXT("Hierarchical"))
	{
		RootChildren = RootElement.GetChildren();
	}
	else
	{
		RootChildren.Reset();

		if (DisplayMode == TEXT("Flat (separate)"))
		{
			FStatsHierarchicalClient::GetAllElementsInline(&RootElement, RootChildren);
		}
		else if (DisplayMode == TEXT("Flat (combined)"))
		{
			TMap<FName, TSharedPtr<FStatsTreeElement>> Map;
			FStatsHierarchicalClient::GetAllElementsCombined(&RootElement, RootChildren, Map);
		}
	}

	SortChildren(RootChildren);

	TreeView->RequestTreeRefresh();
}

void SControlRigProfilingView::SetItemExpansion(const TSharedPtr<FStatsTreeElement>& InItem, bool bExpand, bool bRecursive)
{
	TreeView->SetItemExpansion(InItem, bExpand);

	if (bExpand && bRecursive)
	{
		for (const TSharedPtr<FStatsTreeElement>& Child : InItem->GetChildren())
		{
			SetItemExpansion(Child, bExpand, bRecursive);
		}
	}
}

void SControlRigProfilingView::ToggleItemExpansion(const TSharedPtr<FStatsTreeElement>& InItem, bool bRecursive)
{
	bool bExpand = !TreeView->IsItemExpanded(InItem);
	SetItemExpansion(InItem, bExpand, bRecursive);
}

void SControlRigProfilingView::OnSearchTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	SearchText = InNewText.ToString();
	TreeView->RebuildList();
	//TreeView->RequestTreeRefresh();
}

#undef LOCTEXT_NAMESPACE
