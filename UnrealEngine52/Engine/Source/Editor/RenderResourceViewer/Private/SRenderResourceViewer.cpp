// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRenderResourceViewer.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "SRenderResourceView"

namespace RenderResourceViewerInternal
{
	const FName ColumnName("Name");
	const FName ColumnType("Type");
	const FName ColumnSize("Size");
	const FName ColumnFlags("Flags");
	const FName ColumnOwner("Owner");
	
	// Format the size to nearest size unit of Byte/KB/MB
	FString GetFormatedSize(uint64 SizeInBytes)
	{
		const uint64 KB = 1024;
		const uint64 MB = KB * KB;
		if (SizeInBytes < KB)
		{
			return FString::FromInt(SizeInBytes) + TEXT(" B");
		}
		else if (SizeInBytes < MB)
		{
			uint32 SizeInKB = SizeInBytes / KB;
			return FString::FromInt(SizeInKB) + TEXT(" KB");
		}
		else
		{
			uint32 SizeInMB = SizeInBytes / MB;
			return FString::FromInt(SizeInMB) + TEXT(" MB");
		}
	}
	
	struct FResourceInfoRow : public SMultiColumnTableRow<TSharedPtr<FRHIResourceStats>>
	{
		SLATE_BEGIN_ARGS(FResourceInfoRow) {}
		SLATE_END_ARGS()
	
		TSharedPtr<FRHIResourceStats> Info;
	
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FRHIResourceStats> InItem)
		{
			Info = InItem;
			SMultiColumnTableRow<TSharedPtr<FRHIResourceStats>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
		}
	
		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column)
		{
			if (Column == RenderResourceViewerInternal::ColumnName)
			{
				return SNew(STextBlock)
					.Text(FText::FromName(Info->Name));
			}
			else if (Column == RenderResourceViewerInternal::ColumnType)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Info->Type));
			}
			else if (Column == RenderResourceViewerInternal::ColumnSize)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(RenderResourceViewerInternal::GetFormatedSize(Info->SizeInBytes)));
			}
			else if (Column == RenderResourceViewerInternal::ColumnFlags)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Info->Flags));
			}
			else if (Column == RenderResourceViewerInternal::ColumnOwner)
			{
				return SNew(STextBlock)
					.Text(FText::FromName(Info->OwnerName));
			}
			return SNullWidget::NullWidget;
		}
	};
}

#define RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bIsChecked, DisplayText, Tooltip, OnCheckStateChangedFunc) \
	+ SHorizontalBox::Slot() \
		.AutoWidth() \
		.Padding(FMargin(0.0f, 2.0f, 2.0f, 0.0f)) \
		[ \
			SNew(STextBlock) \
			.Text(DisplayText) \
			.ToolTipText(Tooltip) \
		] \
	+ SHorizontalBox::Slot() \
		.AutoWidth() \
		.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) \
		[ \
			SNew(SCheckBox) \
			.IsChecked(bIsChecked) \
			.OnCheckStateChanged(this, &OnCheckStateChangedFunc) \
		]

#define RENDER_RESOURCE_VIEWER_ADD_COLUMN(Name, Width, Label) \
	+ SHeaderRow::Column(Name) \
		.FillWidth(Width) \
		.DefaultLabel(Label) \
		.SortMode(this, &SRenderResourceViewerWidget::GetColumnSortMode, Name) \
		.OnSort(this, &SRenderResourceViewerWidget::OnColumnSortModeChanged)

void SRenderResourceViewerWidget::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	SortByColumn = RenderResourceViewerInternal::ColumnSize;
	SortMode = EColumnSortMode::Descending;

	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.FillHeight(0.5f)
		[
			SNew(SBox)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.0f, 2.0f, 10.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FilterText", "FilterByFlags:"))
					]

					// A row of checkboxes to filter resource list by flags
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowTransient, LOCTEXT("TransientText", "Transient"), LOCTEXT("TransientTooltip", "Transient"), SRenderResourceViewerWidget::OnTransientCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowStreaming, LOCTEXT("StreamingText", "Streaming"), LOCTEXT("StreamingTooltip", "Streaming"), SRenderResourceViewerWidget::OnStreamingCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowRT, LOCTEXT("RTText", "RT"), LOCTEXT("RTTooltip", "Render Target"), SRenderResourceViewerWidget::OnRTCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowDS, LOCTEXT("DSText", "DS"), LOCTEXT("DSTooltip", "Depth Stencil"), SRenderResourceViewerWidget::OnDSCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowUAV, LOCTEXT("UAVText", "UAV"), LOCTEXT("UAVTooltip", "Unordered Access View"), SRenderResourceViewerWidget::OnUAVCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowRTAS, LOCTEXT("RTASText", "RTAS"), LOCTEXT("RTASTooltip", "Ray Tracing Acceleration Structure"), SRenderResourceViewerWidget::OnRTASCheckboxChanged)
					RENDER_RESOURCE_VIEWER_ADD_CHECKBOX(bShowNone, LOCTEXT("NoneText", "None"), LOCTEXT("NoneTooltip", "Resource with no flags set"), SRenderResourceViewerWidget::OnNoneCheckboxChanged)

					// Refresh button to update the resource list
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(SButton)
						.Text(LOCTEXT("RefreshText", "Refresh"))
						.ToolTipText(LOCTEXT("RefreshTooltip", "Refresh the resource list"))
						.OnClicked(this, &SRenderResourceViewerWidget::OnRefreshButtonClicked)
					]
				]

				// Text filter box to search by resource name
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(FilterTextBox, SSearchBox)
					.HintText(LOCTEXT("FilterTextBoxHint", "Search resources by name or owner."))
					.ToolTipText(LOCTEXT("FilterTextBoxToolTip", "Type here to filter the list of render resources by name or owner."))
					.OnTextChanged(this, &SRenderResourceViewerWidget::FilterTextBox_OnTextChanged)
				]

				// Display total resource count and size in text
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TotalResourceCountText", "Total Resource Count:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(this, &SRenderResourceViewerWidget::GetResourceCountText)
					]

					+ SHorizontalBox::Slot()
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TotalResourceSizeText", "Total Resource Size:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(this, &SRenderResourceViewerWidget::GetResourceSizeText)
					]
				]

				// Display resource list
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(ResourceListView, SListView<TSharedPtr<FRHIResourceStats>>)
					.ScrollbarVisibility(EVisibility::Visible)
					.ItemHeight(24.0f)
					.ListItemsSource(&ResourceInfos)
					.SelectionMode(ESelectionMode::SingleToggle)
					.OnGenerateRow(this, &SRenderResourceViewerWidget::HandleResourceGenerateRow)
					.HeaderRow
					(
						SNew(SHeaderRow)

						RENDER_RESOURCE_VIEWER_ADD_COLUMN(RenderResourceViewerInternal::ColumnName, 0.3f, LOCTEXT("NameColumn", "Resource Name"))
						RENDER_RESOURCE_VIEWER_ADD_COLUMN(RenderResourceViewerInternal::ColumnType, 0.1f, LOCTEXT("TypeColumn", "Type"))
						RENDER_RESOURCE_VIEWER_ADD_COLUMN(RenderResourceViewerInternal::ColumnSize, 0.1f, LOCTEXT("SizeColumn", "Size"))
						RENDER_RESOURCE_VIEWER_ADD_COLUMN(RenderResourceViewerInternal::ColumnFlags, 0.15f, LOCTEXT("FlagsColumn", "Flags"))
						RENDER_RESOURCE_VIEWER_ADD_COLUMN(RenderResourceViewerInternal::ColumnOwner, 0.4f, LOCTEXT("OwnerColumn", "Owner"))
					)
				]
			]
		]
	];

	RefreshNodes(true);
}

void SRenderResourceViewerWidget::RefreshNodes(bool bUpdateRHIResources)
{
	const int32 NumberOfResourcesToShow = -1;

	// Retrieve the whole list of RHI tracked resources
#if RHI_ENABLE_RESOURCE_INFO
	if (bUpdateRHIResources)
	{
		RHIResources.Reset();
		RHIGetTrackedResourceStats(RHIResources);
	}
#endif

	// Filter the resource list by search keyword and flags
	ResourceInfos.Reset();
	TotalResourceSize = 0;
	for (const TSharedPtr<FRHIResourceStats>& Info : RHIResources)
	{
		bool bContainsFilterText = FilterText.IsEmpty() || Info->Name.ToString().Contains(FilterText.ToString()) || Info->OwnerName.ToString().Contains(FilterText.ToString());
		// Note bMarkedForDelete resources are excluded from display
		bool bContainsFilterFlags = (bShowTransient && Info->bTransient) || (bShowStreaming && Info->bStreaming)
									|| (bShowRT && Info->bRenderTarget) || (bShowDS && Info->bDepthStencil) || (bShowUAV && Info->bUnorderedAccessView) || (bShowRTAS && Info->bRayTracingAccelerationStructure)
									|| (bShowNone && !Info->bHasFlags);
		if (bContainsFilterText && bContainsFilterFlags)
		{
			ResourceInfos.Add(Info);
			TotalResourceSize += Info->SizeInBytes;
		}
	}

	TotalResourceCount = ResourceInfos.Num();

	// Sort the resource list based on columns.
	{
		if (SortByColumn == RenderResourceViewerInternal::ColumnName)
		{
			ResourceInfos.Sort([this](const TSharedPtr<FRHIResourceStats>& A, const TSharedPtr<FRHIResourceStats>& B) {
				if (SortMode == EColumnSortMode::Ascending) return (A->Name.LexicalLess(B->Name));
				else if (SortMode == EColumnSortMode::Descending) return (!A->Name.LexicalLess(B->Name));
				return true;
			});
		}
		else if (SortByColumn == RenderResourceViewerInternal::ColumnType)
		{
			ResourceInfos.Sort([this](const TSharedPtr<FRHIResourceStats>& A, const TSharedPtr<FRHIResourceStats>& B) {
				if (SortMode == EColumnSortMode::Ascending) return (A->Type < B->Type);
				else if (SortMode == EColumnSortMode::Descending) return (A->Type >= B->Type);
				return true;
			});
		}
		else if (SortByColumn == RenderResourceViewerInternal::ColumnSize)
		{
			ResourceInfos.Sort([this](const TSharedPtr<FRHIResourceStats>& A, const TSharedPtr<FRHIResourceStats>& B) {
				if (SortMode == EColumnSortMode::Ascending) return (A->SizeInBytes < B->SizeInBytes);
				else if (SortMode == EColumnSortMode::Descending) return (A->SizeInBytes >= B->SizeInBytes);
				return true;
			});
		}
		else if (SortByColumn == RenderResourceViewerInternal::ColumnFlags)
		{
			ResourceInfos.Sort([this](const TSharedPtr<FRHIResourceStats>& A, const TSharedPtr<FRHIResourceStats>& B) {
				if (SortMode == EColumnSortMode::Ascending) return (A->Flags < B->Flags);
				else if (SortMode == EColumnSortMode::Descending) return (A->Flags >= B->Flags);
				return true;
			});
		}
		else if (SortByColumn == RenderResourceViewerInternal::ColumnOwner)
		{
			ResourceInfos.Sort([this](const TSharedPtr<FRHIResourceStats>& A, const TSharedPtr<FRHIResourceStats>& B) {
				if (SortMode == EColumnSortMode::Ascending) return (A->OwnerName.LexicalLess(B->OwnerName));
				else if (SortMode == EColumnSortMode::Descending) return (!A->OwnerName.LexicalLess(B->OwnerName));
				return true;
			});
		}
	}

	ResourceListView->RebuildList();
}

TSharedRef<ITableRow> SRenderResourceViewerWidget::HandleResourceGenerateRow(TSharedPtr<FRHIResourceStats> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(RenderResourceViewerInternal::FResourceInfoRow, OwnerTable, Item);
}

void SRenderResourceViewerWidget::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortByColumn = ColumnId;
	SortMode = InSortMode;
	RefreshNodes();
}

FText SRenderResourceViewerWidget::GetResourceSizeText() const
{
	return FText::FromString(RenderResourceViewerInternal::GetFormatedSize(TotalResourceSize));
}

#undef LOCTEXT_NAMESPACE
