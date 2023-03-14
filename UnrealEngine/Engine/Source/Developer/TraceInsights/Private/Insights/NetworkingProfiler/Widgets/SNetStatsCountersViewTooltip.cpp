// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNetStatsCountersViewTooltip.h"

#include "SlateOptMacros.h"
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
#include "Insights/NetworkingProfiler/ViewModels/NetStatsCounterNode.h"
#include "Insights/NetworkingProfiler/ViewModels/NetStatsCounterNodeHelper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "SNetStatsCountersView"

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedPtr<SToolTip> SNetStatsCountersViewTooltip::GetTableTooltip(const Insights::FTable& Table)
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

TSharedPtr<SToolTip> SNetStatsCountersViewTooltip::GetColumnTooltip(const Insights::FTableColumn& Column)
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

TSharedPtr<SToolTip> SNetStatsCountersViewTooltip::GetRowTooltip(const TSharedPtr<FNetStatsCounterNode> NetStatsCounterNodePtr)
{
	const TraceServices::FNetProfilerAggregatedStatsCounterStats& Stats = NetStatsCounterNodePtr->GetAggregatedStats();
	const FText InstanceCountText = FText::AsNumber(Stats.Count);

	FText SumText = NetStatsCounterNodePtr->GetTextForAggregatedStatsSum(true);
	FText MinText = NetStatsCounterNodePtr->GetTextForAggregatedStatsMin(true);
	FText MaxText = NetStatsCounterNodePtr->GetTextForAggregatedStatsMax(true);
	FText AvgText = NetStatsCounterNodePtr->GetTextForAggregatedStatsAverage(true);

	TSharedPtr<SGridPanel> GridPanel;
	TSharedPtr<SHorizontalBox> HBox;

	TSharedPtr<SToolTip> TableCellTooltip =
		SNew(SToolTip)
		[
			SAssignNew(HBox, SHorizontalBox)

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

			// Id: [Id]
		+ SGridPanel::Slot(0, 0)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TT_Id", "Id:"))
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
		]
	+ SGridPanel::Slot(1, 0)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(NetStatsCounterNodePtr->GetCounterTypeIndex()))
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
		]

	// Name: [Name]
	+ SGridPanel::Slot(0, 1)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TT_Name", "Name:"))
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
		]
	+ SGridPanel::Slot(1, 1)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.WrapTextAt(512.0f)
		.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
		.Text(FText::FromName(NetStatsCounterNodePtr->GetName()))
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
		]

	// Counter Type: [Type]
	+ SGridPanel::Slot(0, 3)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TT_Type", "Node Type:"))
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.TooltipBold"))
		]
	+ SGridPanel::Slot(1, 3)
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(NetStatsCounterNodeTypeHelper::ToText(NetStatsCounterNodePtr->GetType()))
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

			// Aggregated stats are added here.
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]
		]
		];

	int32 Row = 1;
	AddAggregatedStatsRow(GridPanel, Row, LOCTEXT("TT_Sum", "Sum:"), SumText);
	AddAggregatedStatsRow(GridPanel, Row, LOCTEXT("TT_Max", "Max:"), MaxText);
	AddAggregatedStatsRow(GridPanel, Row, LOCTEXT("TT_Average", "Average:"), AvgText);
	AddAggregatedStatsRow(GridPanel, Row, LOCTEXT("TT_Min", "Min:"), MinText);

	return TableCellTooltip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SNetStatsCountersViewTooltip::AddAggregatedStatsRow(TSharedPtr<SGridPanel> Grid, int32& Row, const FText& Name, const FText& Value)
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
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(Value)
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
		.ColorAndOpacity(Row == 1 ? FLinearColor::Gray : FLinearColor::White)
		];

	Row++;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
