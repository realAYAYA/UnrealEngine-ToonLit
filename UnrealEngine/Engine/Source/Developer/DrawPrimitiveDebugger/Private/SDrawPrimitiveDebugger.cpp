// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDrawPrimitiveDebugger.h"

#include "DrawPrimitiveDebugger.h"
#include "DrawPrimitiveDebuggerConfig.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/PrimitiveComponent.h"
#include "Fonts/FontMeasure.h"
#include "Materials/Material.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#if !UE_BUILD_SHIPPING

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDrawPrimitiveDebugger::Construct(const FArguments& InArgs)
{
	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	ColumnHeader = SNew(SHeaderRow).ResizeMode(ESplitterResizeMode::Fill);
	const FName VisibilityColumn("Visible");
	const FName PinColumn("Pin");
	const FName NameColumn("Name");
	const FName NumDrawsColumn("NumDraws");
	const FName ActorClassColumn("ActorClass");
	const FName ActorColumn("Actor");
	const FName LocationColumn("Location");
	const FName MaterialsColumn("Materials");
	const FName LODColumn("LOD");
	const FName TrianglesColumn("Triangles");
	AddColumn(FText::FromString("Visible"), VisibilityColumn);
	AddColumn(FText::FromString("Pinned"), PinColumn);
	AddColumn(FText::FromString("Name"), NameColumn);
	AddColumn(FText::FromString("Draws"), NumDrawsColumn);
	AddColumn(FText::FromString("ActorClass"), ActorClassColumn);
	AddColumn(FText::FromString("Actor"), ActorColumn);
	AddColumn(FText::FromString("Location"), LocationColumn);
	AddColumn(FText::FromString("Materials"), MaterialsColumn);
	AddColumn(FText::FromString("LOD"), LODColumn);
	AddColumn(FText::FromString("Triangles"), TrianglesColumn);

	Refresh();

	Table = SNew(SListView<FPrimitiveRowDataPtr>)
		.ListItemsSource(&VisibleEntries)
		.HeaderRow(ColumnHeader)
		.OnGenerateRow(this, &SDrawPrimitiveDebugger::MakeRowWidget)
		.ExternalScrollbar(VerticalScrollBar)
		.Orientation(Orient_Vertical)
		.ConsumeMouseWheel(EConsumeMouseWheel::Never)
		.SelectionMode(ESelectionMode::Multi);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(6, 6)
			.HAlign(HAlign_Fill)
			.FillWidth(2)
			[
				SAssignNew(SearchBox, SSearchBox)
				.InitialText(this, &SDrawPrimitiveDebugger::GetFilterText)
				.OnTextChanged(this, &SDrawPrimitiveDebugger::OnFilterTextChanged)
				.OnTextCommitted(this, &SDrawPrimitiveDebugger::OnFilterTextCommitted)
			]
			+ SHorizontalBox::Slot()
			.Padding(6, 6)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Refresh"))
				.IsEnabled(this, &SDrawPrimitiveDebugger::CanCaptureSingleFrame)
				.OnClicked(this, &SDrawPrimitiveDebugger::OnRefreshClick)
			]
			+ SHorizontalBox::Slot()
			.Padding(6, 6)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Save to CSV"))
				.OnClicked(this, &SDrawPrimitiveDebugger::OnSaveClick)
			]
			/*+ SHorizontalBox::Slot()
			.Padding(6, 6)
			.AutoWidth()
			[
				SNew(SCheckBox)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Enable Live Capture"))
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), UDrawPrimitiveDebuggerUserSettings::GetFontSize()))
				]
				.IsChecked(this, &SDrawPrimitiveDebugger::IsLiveCaptureChecked)
				.OnCheckStateChanged(this, &SDrawPrimitiveDebugger::OnToggleLiveCapture)
			]*/ // TODO: Re-enable after the performance issues have been fixed
		]
		+SVerticalBox::Slot()
		.Padding(6, 6)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Vertical)
			.ConsumeMouseWheel(EConsumeMouseWheel::Always)
			+SScrollBox::Slot()
			[
				Table.ToSharedRef()
			]
		]
	];
}

FText SDrawPrimitiveDebugger::GetFilterText() const
{
	return FilterText;
}

void SDrawPrimitiveDebugger::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;
	UpdateVisibleRows();
	if (Table.IsValid())
	{
		Table->RequestListRefresh();
	}
}

void SDrawPrimitiveDebugger::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnCleared)
	{
		SearchBox->SetText(FText::GetEmpty());
		OnFilterTextChanged(FText::GetEmpty());
	}
}

TSharedRef<ITableRow> SDrawPrimitiveDebugger::MakeRowWidget(FPrimitiveRowDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SDrawPrimitiveDebuggerListViewRow, OwnerTable)
		.DrawPrimitiveDebugger(SharedThis(this))
		.RowDataPtr(InRowDataPtr);
}

void SDrawPrimitiveDebugger::UpdateVisibleRows()
{
	if (FilterText.IsEmptyOrWhitespace())
	{
		VisibleEntries = AvailableEntries;
	}
	else
	{
		VisibleEntries.Empty();

		const FString& ActiveFilterString = FilterText.ToString();
		for (const FPrimitiveRowDataPtr& RowData : AvailableEntries)
		{
			if (!RowData.IsValid()) continue;
			bool bPassesFilter = false;
			
			if (RowData->Name.Contains(ActiveFilterString))
			{
				bPassesFilter = true;
			}
			else if (IsValid(RowData->Owner) &&
				(RowData->Owner->GetClass()->GetName().Contains(ActiveFilterString) ||
				RowData->Owner->GetFullName().Contains(ActiveFilterString)))
			{
				bPassesFilter = true;
			}
			else
			{
				for (const UMaterialInterface* Material : RowData->Materials)
				{
					if (IsValid(Material) && IsValid(Material->GetMaterial()) &&
						Material->GetMaterial()->GetName().Contains(ActiveFilterString))
					{
						bPassesFilter = true;
						break;
					}
				}
			}

			if (bPassesFilter)
			{
				VisibleEntries.Add(RowData);
			}
		}
	}
	SortRows();
}

void SDrawPrimitiveDebugger::SortRows()
{
	VisibleEntries.Sort([this](FPrimitiveRowDataPtr A, FPrimitiveRowDataPtr B)
	{
		const bool bPinnedA = IsEntryPinned(A);
		const bool bPinnedB = IsEntryPinned(B);
		return (bPinnedA > bPinnedB) || ((bPinnedA == bPinnedB) && *A < *B); // Put pinned entries first
	});
}

void SDrawPrimitiveDebugger::Refresh()
{
	AvailableEntries.Empty();
	for (const TPair<FPrimitiveComponentId, FViewDebugInfo::FPrimitiveInfo>& Copy : LocalCopies)
	{
		AvailableEntries.Add(MakeShared<const FViewDebugInfo::FPrimitiveInfo>(Copy.Value)); // Add the local copies (pinned and hidden) at the top
	}
	FViewDebugInfo::Get().ForEachPrimitive([this](const FViewDebugInfo::FPrimitiveInfo& Primitive)
	{
		if (!LocalCopies.Contains(Primitive.ComponentId))
		{
			AvailableEntries.Add(MakeShared<const FViewDebugInfo::FPrimitiveInfo>(Primitive));
			UPrimitiveComponent* Component = Primitive.ComponentInterface ? Primitive.ComponentInterface->GetUObject<UPrimitiveComponent>() : nullptr;
			if (Component && IsValid(Component) && !Component->IsVisible())
			{
				LocalCopies.Add(Primitive.ComponentId, Primitive);
				HiddenEntries.Add(Primitive.ComponentId);
			}
		}
	});
	UpdateVisibleRows();
}

void SDrawPrimitiveDebugger::AddColumn(const FText& Name, const FName& ColumnId)
{
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo FontInfo = FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12);
	const FName VisibilityColumn("Visible");
	const FName PinColumn("Pin");
	const FName NumDrawsColumn("NumDraws");
	const FName LODColumn("LOD");
	if (ColumnId.IsEqual(VisibilityColumn) || ColumnId.IsEqual(PinColumn) ||
		ColumnId.IsEqual(NumDrawsColumn) || ColumnId.IsEqual(LODColumn))
	{ // Rows that can be narrow and fixed
		ColumnHeader->AddColumn(
			SHeaderRow::Column(ColumnId)
			.DefaultLabel(Name)
			.FixedWidth(FontMeasure->Measure(Name, FontInfo).X)
		);
	}
	else
	{
		ColumnHeader->AddColumn(
			SHeaderRow::Column(ColumnId)
			.DefaultLabel(Name)
		);
	}
}

void SDrawPrimitiveDebugger::OnChangeEntryVisibility(ECheckBoxState State, FPrimitiveRowDataPtr Data)
{
	UPrimitiveComponent* Component = Data->ComponentInterface->GetUObject<UPrimitiveComponent>();
	
	if (Component && Data.IsValid() && IsValid(Component) && State != ECheckBoxState::Undetermined)
	{
		Component->SetVisibility(State == ECheckBoxState::Checked);
		if (State == ECheckBoxState::Unchecked)
		{
			if (!LocalCopies.Contains(Data->ComponentId))
			{
				LocalCopies.Add(Data->ComponentId, *Data); // Keep a local copy to prevent the primitive from disappearing during visibility check
			}
			HiddenEntries.Add(Data->ComponentId);
		}
		else if (State == ECheckBoxState::Checked)
		{
			HiddenEntries.Remove(Data->ComponentId);
			if (!IsEntryPinned(Data))
			{
				LocalCopies.Remove(Data->ComponentId); // Remove the local copy when no longer needed
			}
		}
	}
}

bool SDrawPrimitiveDebugger::IsEntryVisible(FPrimitiveRowDataPtr Data) const
{
	return !HiddenEntries.Contains(Data->ComponentId);
}

void SDrawPrimitiveDebugger::OnChangeEntryPinned(ECheckBoxState State, FPrimitiveRowDataPtr Data)
{
	if (State != ECheckBoxState::Undetermined)
	{
		if (State == ECheckBoxState::Checked)
		{
			if (!LocalCopies.Contains(Data->ComponentId))
			{
				LocalCopies.Add(Data->ComponentId, *Data); // Keep a local copy to prevent the primitive from disappearing during visibility check
			}
			PinnedEntries.Add(Data->ComponentId);
		}
		else if (State == ECheckBoxState::Unchecked)
		{
			PinnedEntries.Remove(Data->ComponentId);
			if (IsEntryVisible(Data))
			{
				LocalCopies.Remove(Data->ComponentId); // Remove the local copy when no longer needed
			}
		}
	}
	UpdateVisibleRows();
	if (Table.IsValid())
	{
		Table->RequestListRefresh();
	}
}

bool SDrawPrimitiveDebugger::IsEntryPinned(FPrimitiveRowDataPtr Data) const
{
	return PinnedEntries.Contains(Data->ComponentId);
}

bool SDrawPrimitiveDebugger::CanCaptureSingleFrame() const
{
	return IDrawPrimitiveDebugger::IsAvailable() && !IDrawPrimitiveDebugger::Get().IsLiveCaptureEnabled();
}

FReply SDrawPrimitiveDebugger::OnRefreshClick()
{
	IDrawPrimitiveDebugger::Get().CaptureSingleFrame();
	return FReply::Handled();
}

FReply SDrawPrimitiveDebugger::OnSaveClick()
{
	FViewDebugInfo::Get().DumpToCSV();
	return FReply::Handled();
}

ECheckBoxState SDrawPrimitiveDebugger::IsLiveCaptureChecked() const
{
	return IDrawPrimitiveDebugger::Get().IsLiveCaptureEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDrawPrimitiveDebugger::OnToggleLiveCapture(ECheckBoxState state)
{
	if (state == ECheckBoxState::Checked)
	{
		IDrawPrimitiveDebugger::Get().EnableLiveCapture();
	}
	else if (state == ECheckBoxState::Unchecked)
	{
		IDrawPrimitiveDebugger::Get().DisableLiveCapture();
	}
}

void SDrawPrimitiveDebuggerListViewRow::Construct(const FArguments& InArgs,
                                             const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowDataPtr = InArgs._RowDataPtr;
	DrawPrimitiveDebugger = InArgs._DrawPrimitiveDebugger;
	SMultiColumnTableRow<FPrimitiveRowDataPtr>::Construct(
		FSuperRowType::FArguments(),
		InOwnerTableView
	);
}

TSharedRef<SWidget> SDrawPrimitiveDebuggerListViewRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	const TSharedPtr<SDrawPrimitiveDebugger> DrawPrimitiveDebuggerPtr = DrawPrimitiveDebugger.Pin();
	return (DrawPrimitiveDebuggerPtr.IsValid())
		? MakeCellWidget(IndexInList, ColumnName)
		: SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDrawPrimitiveDebuggerListViewRow::MakeCellWidget(const int32 InRowIndex, const FName& InColumnId)
{
	SDrawPrimitiveDebugger* DrawPrimitiveDebuggerPtr = DrawPrimitiveDebugger.Pin().Get();
	if (DrawPrimitiveDebuggerPtr && RowDataPtr.IsValid())
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateFontInfo FontInfo = FSlateFontInfo(FCoreStyle::GetDefaultFont(), UDrawPrimitiveDebuggerUserSettings::GetFontSize());
		FText Value;
		const FName VisibilityColumn("Visible");
		const FName PinColumn("Pin");
		const FName NameColumn("Name");
		const FName ActorClassColumn("ActorClass");
		const FName ActorColumn("Actor");
		const FName LocationColumn("Location");
		const FName MaterialsColumn("Materials");
		const FName NumDrawsColumn("NumDraws");
		const FName LODColumn("LOD");
		const FName TrianglesColumn("Triangles");
		if (InColumnId.IsEqual(VisibilityColumn))
		{
			return SNew(SBox)
				.Padding(FMargin(5, 2, 5, 2))
				.HAlign(HAlign_Center)
				[
					SNew(SCheckBox)
						.IsChecked(this, &SDrawPrimitiveDebuggerListViewRow::IsVisible)
						.OnCheckStateChanged(DrawPrimitiveDebuggerPtr, &SDrawPrimitiveDebugger::OnChangeEntryVisibility, RowDataPtr)
						.HAlign(HAlign_Center)
				];
		}
		if (InColumnId.IsEqual(PinColumn))
		{
			return SNew(SBox)
				.Padding(FMargin(5, 2, 5, 2))
				.HAlign(HAlign_Center)
				[
					SNew(SCheckBox)
						.IsChecked(this, &SDrawPrimitiveDebuggerListViewRow::IsPinned)
						.OnCheckStateChanged(DrawPrimitiveDebuggerPtr, &SDrawPrimitiveDebugger::OnChangeEntryPinned, RowDataPtr)
						.HAlign(HAlign_Center)
				];
		}
		if (InColumnId.IsEqual(NameColumn))
		{
			Value = FText::FromString(RowDataPtr->Name);
		}
		else if (InColumnId.IsEqual(ActorClassColumn))
		{
			Value = IsValid(RowDataPtr->Owner) ?
				FText::FromString(RowDataPtr->Owner->GetClass()->GetName()) :
				FText::FromString("INVALID");
		}
		else if (InColumnId.IsEqual(ActorColumn))
		{			
			auto GetHumanReadableName = [](FPrimitiveRowDataPtr& InRowDataPtr) 
			{
				if (AActor* Actor = Cast<AActor>(InRowDataPtr->Owner))
				{
					return Actor->GetHumanReadableName();
				}
				else
				{
					return InRowDataPtr->ComponentInterface->GetOwnerName();
				}
			};
			Value = IsValid(RowDataPtr->Owner) ?
				FText::FromString(GetHumanReadableName(RowDataPtr)) :
				FText::FromString("INVALID");
		}
		else if (InColumnId.IsEqual(LocationColumn))
		{
			Value = IsValid(RowDataPtr->ComponentInterface->GetUObject()) ?
				FText::FromString(RowDataPtr->ComponentInterface->GetTransform().GetLocation().ToString()) :
				FText::FromString("INVALID");
		}
		else if (InColumnId.IsEqual(MaterialsColumn))
		{
			const int32 Count = RowDataPtr->Materials.Num();
			FString Materials = FString::Printf(TEXT("[%d] {"), Count);
			for (int i = 0; i < Count; i++)
			{
				const UMaterialInterface* MI = RowDataPtr->Materials[i];
				if (MI && MI->GetMaterial())
				{
					Materials += MI->GetMaterial()->GetName();
				}
				else
				{
					Materials += "Null";
				}
				
				if (i < Count - 1)
				{
					Materials += ", ";
				}
			}
			Materials += "}";
			Value = FText::FromString(Materials);
		}
		else if (InColumnId.IsEqual(NumDrawsColumn))
		{
			Value = FText::FromString(FString::FromInt(RowDataPtr->DrawCount));
		}
		else if (InColumnId.IsEqual(LODColumn))
		{
			Value = FText::FromString(FString::FromInt(RowDataPtr->LOD));
		}
		else if (InColumnId.IsEqual(TrianglesColumn))
		{
			Value = FText::FromString(FString::FromInt(RowDataPtr->TriangleCount));
		}
		else
		{
			// Invalid Column name
			return SNullWidget::NullWidget;
		}
		return SNew(SBox)
			.Padding(FMargin(5, 2, 5, 2))
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Text(Value)
				.Font(FontInfo)
				.IsEnabled(DrawPrimitiveDebuggerPtr, &SDrawPrimitiveDebugger::IsEntryVisible, RowDataPtr)
				.Justification(ETextJustify::Left)
				.HighlightText(DrawPrimitiveDebuggerPtr, &SDrawPrimitiveDebugger::GetFilterText)
			];
	}
	return SNullWidget::NullWidget;
}

ECheckBoxState SDrawPrimitiveDebuggerListViewRow::IsVisible() const
{
	const SDrawPrimitiveDebugger* DrawPrimitiveDebuggerPtr = DrawPrimitiveDebugger.Pin().Get();
	return DrawPrimitiveDebuggerPtr && DrawPrimitiveDebuggerPtr->IsEntryVisible(RowDataPtr) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SDrawPrimitiveDebuggerListViewRow::IsPinned() const
{
	const SDrawPrimitiveDebugger* DrawPrimitiveDebuggerPtr = DrawPrimitiveDebugger.Pin().Get();
	return DrawPrimitiveDebuggerPtr && DrawPrimitiveDebuggerPtr->IsEntryPinned(RowDataPtr) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#endif