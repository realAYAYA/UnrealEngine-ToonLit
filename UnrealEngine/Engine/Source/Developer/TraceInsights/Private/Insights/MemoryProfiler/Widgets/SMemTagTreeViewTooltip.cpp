// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemTagTreeViewTooltip.h"

#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "TraceServices/Model/NetProfiler.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNode.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNodeHelper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "SMemTagTreeView"

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedPtr<SToolTip> SMemTagTreeViewTooltip::GetTableTooltip(const Insights::FTable& Table)
{
	TSharedPtr<SToolTip> ColumnTooltip =
		SNew(SToolTip)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(Table.GetDisplayName())
				.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(Table.GetDescription())
				.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
			]
		];

	return ColumnTooltip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SToolTip> SMemTagTreeViewTooltip::GetColumnTooltip(const Insights::FTableColumn& Column)
{
	TSharedPtr<SToolTip> ColumnTooltip =
		SNew(SToolTip)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(Column.GetTitleName())
				.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(Column.GetDescription())
				.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
			]
		];

	return ColumnTooltip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#define TOOLTIP_SHOW_AGGREGATED_STATS 0 // TODO: to be enabled when aggregated stats will be available

TSharedPtr<SToolTip> SMemTagTreeViewTooltip::GetRowTooltip(const TSharedPtr<FMemTagNode> MemTagNodePtr)
{
	const FText TrackersText = MemTagNodePtr->GetTrackerText();

	FText TagText = FText::GetEmpty();
	FText TagTextEx = LOCTEXT("MemTagNA", "N/A");
	Insights::FMemoryTag* MemTag = MemTagNodePtr.IsValid() ? MemTagNodePtr->GetMemTag() : nullptr;
	if (MemTag)
	{
		TagText = FText::FromString(MemTag->GetStatName());
		TagTextEx = FText::FromString(FString::Printf(TEXT(" (0x%llX)"), (uint64)MemTag->GetId()));
	}

	FText ParentTagText = FText::GetEmpty();
	FText ParentTagTextEx = LOCTEXT("MemTagNA", "N/A");
	Insights::FMemoryTag* ParentMemTag = MemTagNodePtr.IsValid() ? MemTagNodePtr->GetParentMemTag() : nullptr;
	if (ParentMemTag)
	{
		ParentTagText = FText::FromString(ParentMemTag->GetStatName());
		ParentTagTextEx = FText::FromString(FString::Printf(TEXT(" (0x%llX)"), (uint64)ParentMemTag->GetId()));
	}

#if TOOLTIP_SHOW_AGGREGATED_STATS
	const TraceServices::FMemoryProfilerAggregatedStats& Stats = MemTagNodePtr->GetAggregatedStats();
	const FText InstanceCountText = FText::AsNumber(Stats.InstanceCount);
	const FText MinValueText = FText::AsNumber(Stats.Min);
	const FText MaxValueText = FText::AsNumber(Stats.Max);
	const FText AvgValueText = FText::AsNumber(Stats.Average);

	TSharedPtr<SGridPanel> GridPanel;
#endif // TOOLTIP_SHOW_AGGREGATED_STATS

	TSharedPtr<SToolTip> TableCellTooltip =
		SNew(SToolTip)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SGridPanel)

					// Name: [Name]
					+ SGridPanel::Slot(0, 0)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_Name", "Name:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 0)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.WrapTextAt(512.0f)
						.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
						.Text(FText::FromName(MemTagNodePtr->GetName()))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
					]

					// Type: [Type]
					+ SGridPanel::Slot(0, 1)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_Type", "Type:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 1)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(MemTagNodeTypeHelper::ToText(MemTagNodePtr->GetType()))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
					]

					// Tag: [Tag]
					+ SGridPanel::Slot(0, 2)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_Tag", "Tag:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 2)
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(TagText)
							.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(TagTextEx)
							.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
						]
					]

					// Parent Tag: [Tag]
					+ SGridPanel::Slot(0, 3)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_ParentTag", "Parent Tag:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 3)
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(ParentTagText)
							.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(ParentTagTextEx)
							.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
						]
					]

					// Tracker: [Tracker]
					+ SGridPanel::Slot(0, 4)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_Tracker", "Tracker:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 4)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(MemTagNodePtr->GetTrackerText())
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

#if TOOLTIP_SHOW_AGGREGATED_STATS
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SGridPanel)

					+ SGridPanel::Slot(0, 0)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TT_NumInstances", "Num Instances:"))
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
					]
					+ SGridPanel::Slot(1, 0)
					.Padding(2.0f)
					[
						SNew(STextBlock)
						.Text(InstanceCountText)
						.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SAssignNew(GridPanel, SGridPanel)

					// Stats are added here.
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]
#endif // TOOLTIP_SHOW_AGGREGATED_STATS
			]
		];

#if TOOLTIP_SHOW_AGGREGATED_STATS
	int32 Row = 0;
	AddStatsRow(GridPanel, Row, LOCTEXT("TT_Min",     "Min:"),     MinValueText);
	AddStatsRow(GridPanel, Row, LOCTEXT("TT_Max",     "Max:"),     MaxValueText);
	AddStatsRow(GridPanel, Row, LOCTEXT("TT_Average", "Average:"), AvgValueText);
#endif // TOOLTIP_SHOW_AGGREGATED_STATS

	return TableCellTooltip;
}

#undef TOOLTIP_SHOW_AGGREGATED_STATS

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeViewTooltip::AddStatsRow(TSharedPtr<SGridPanel> Grid, int32& Row, const FText& Name, const FText& Value)
{
	Grid->AddSlot(0, Row)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(Name)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
		];

	Grid->AddSlot(1, Row)
		.Padding(2.0f)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(Value)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
		];

	Row++;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
