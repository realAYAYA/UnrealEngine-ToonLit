// Copyright Epic Games, Inc. All Rights Reserved.

#include "SZenCacheStatistics.h"
#include "ZenServerInterface.h"
#include "Math/UnitConversion.h"
#include "Math/BasicMathExpressionEvaluator.h"
#include "Misc/ExpressionParser.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Internationalization/FastDecimalFormat.h"

#define LOCTEXT_NAMESPACE "ZenEditor"

static FString SingleDecimalFormat(double Value)
{
	const FNumberFormattingOptions NumberFormattingOptions = FNumberFormattingOptions()
		.SetUseGrouping(true)
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(1);
	return FastDecimalFormat::NumberToString(Value, ExpressionParser::GetLocalizedNumberFormattingRules(), NumberFormattingOptions);
};

void SZenCacheStatisticsDialog::Construct(const FArguments& InArgs)
{
	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 0)
		.Expose(GridSlot)
		[
			GetGridPanel()
		]
	];

	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SZenCacheStatisticsDialog::UpdateGridPanels));
}

EActiveTimerReturnType SZenCacheStatisticsDialog::UpdateGridPanels(double InCurrentTime, float InDeltaTime)
{
	(*GridSlot)
	[
		GetGridPanel()
	];

	SlatePrepass(GetPrepassLayoutScaleMultiplier());

	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SZenCacheStatisticsDialog::GetGridPanel()
{
	TSharedRef<SGridPanel> Panel = SNew(SGridPanel);

#if UE_WITH_ZEN

	UE::Zen::FZenStats ZenStats;

	UE::Zen::GetDefaultServiceInstance().GetStats(ZenStats);

	int32 Row = 0;
	double SumTotalGetMB = 0.0;
	double SumTotalPutMB = 0.0;

	const float RowMargin = 0.0f;
	const float TitleMargin = 10.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	const double CASDiskUsageMB = FUnitConversion::Convert(ZenStats.CASStats.Size.Total, EUnit::Bytes, EUnit::Megabytes);
	const double CacheDiskUsageMB = FUnitConversion::Convert(ZenStats.CacheStats.Size.Disk, EUnit::Bytes, EUnit::Megabytes);
	const double CacheMemoryUsageMB = FUnitConversion::Convert(ZenStats.CacheStats.Size.Memory, EUnit::Bytes, EUnit::Megabytes);

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("Cache", "Local Cache"))
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("CAS", "Local Content Store"))
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("DiskSpace", "Disk Space"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([CacheDiskUsageMB]
		{
			if (CacheDiskUsageMB > 1024.0)
			{
				return FText::FromString(SingleDecimalFormat(FUnitConversion::Convert(CacheDiskUsageMB, EUnit::Megabytes, EUnit::Gigabytes)) + TEXT(" GB"));
			}
			else
			{
				return FText::FromString(SingleDecimalFormat(CacheDiskUsageMB) + TEXT(" MB"));
			}
		})
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("DiskSpace", "Disk Space"))
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([CASDiskUsageMB]
		{
			if (CASDiskUsageMB > 1024.0)
			{
				return FText::FromString(SingleDecimalFormat(FUnitConversion::Convert(CASDiskUsageMB, EUnit::Megabytes, EUnit::Gigabytes)) + TEXT(" GB"));
			}
			else
			{
				return FText::FromString(SingleDecimalFormat(CASDiskUsageMB) + TEXT(" MB"));
			}
		})
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("MemoryUsage", "Memory Usage"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([CacheMemoryUsageMB] 
			{
				if (CacheMemoryUsageMB > 1024.0)
				{
					return FText::FromString(SingleDecimalFormat(FUnitConversion::Convert(CacheMemoryUsageMB, EUnit::Megabytes, EUnit::Gigabytes)) + TEXT(" GB"));
				}
				else
				{
					return FText::FromString(SingleDecimalFormat(CacheMemoryUsageMB) + TEXT(" MB"));
				}
			})
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text(LOCTEXT("CacheHitRate", "Hit Rate"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, RowMargin))
		.Text_Lambda([ZenStats] { return FText::FromString(SingleDecimalFormat(ZenStats.CacheStats.HitRatio * 100.0) + TEXT(" %")); })
	];

	Row++;

	Panel->AddSlot(0, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, TitleMargin, ColumnMargin, 0.0f))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("UpstreamServer", "Upstream Server"))
	];

	Panel->AddSlot(1, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, TitleMargin, ColumnMargin, 0.0f))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("HitRate", "Hit Rate"))
	];

	Panel->AddSlot(2, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, TitleMargin, ColumnMargin, 0.0f))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("Downloaded", "Downloaded"))
	];

	Panel->AddSlot(3, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, TitleMargin, ColumnMargin, 0.0f))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("Uploaded", "Uploaded"))
	];

	Panel->AddSlot(4, Row)
	[
		SNew(STextBlock)
		.Margin(FMargin(ColumnMargin, TitleMargin, ColumnMargin, 0.0f))
		.ColorAndOpacity(TitleColor)
		.Font(TitleFont)
		.Text(LOCTEXT("URL", "URL"))
	];
	Row++;
	
	int32 EndpointIndex = 1;

	for (const UE::Zen::FZenEndPointStats& EndpointStats : ZenStats.UpstreamStats.EndPointStats)
	{
		Panel->AddSlot(0, Row)
		[
			SNew(STextBlock)
			.Margin(FMargin(ColumnMargin, RowMargin))
			.Text_Lambda([EndpointStats] { return FText::FromString(EndpointStats.Name); })
		];

		Panel->AddSlot(1, Row)
		[
			SNew(STextBlock)
			.Margin(FMargin(ColumnMargin, RowMargin))
			.Text_Lambda([EndpointStats] { return FText::FromString(SingleDecimalFormat(EndpointStats.HitRatio * 100.0) + TEXT(" %")); })
		];

		Panel->AddSlot(2, Row)
		[
			SNew(STextBlock)
			.Margin(FMargin(ColumnMargin, RowMargin))
			.Text_Lambda([EndpointStats] 
				{ 
					if (EndpointStats.DownloadedMB> 1024.0)
					{
						return FText::FromString(SingleDecimalFormat(FUnitConversion::Convert(EndpointStats.DownloadedMB, EUnit::Megabytes, EUnit::Gigabytes) ) + TEXT(" GB")); 
					}
					else
					{
						return FText::FromString(SingleDecimalFormat(EndpointStats.DownloadedMB) + TEXT(" MB"));
					}
				})
											
		];

		Panel->AddSlot(3, Row)
		[
			SNew(STextBlock)
			.Margin(FMargin(ColumnMargin, RowMargin))
			.Text_Lambda([EndpointStats]
				{ 
					if (EndpointStats.UploadedMB > 1024.0)
					{
						return FText::FromString(SingleDecimalFormat(FUnitConversion::Convert(EndpointStats.UploadedMB, EUnit::Megabytes, EUnit::Gigabytes) ) + TEXT(" GB")); 
					}
					else
					{
						return FText::FromString(SingleDecimalFormat(EndpointStats.UploadedMB) + TEXT(" MB"));
					}
				})
		];

		Panel->AddSlot(4, Row)
		[
			SNew(STextBlock)
			.Margin(FMargin(ColumnMargin, RowMargin))
			.Text_Lambda([EndpointStats] { return FText::FromString(EndpointStats.Url); })
		];

		SumTotalGetMB += EndpointStats.DownloadedMB;
		SumTotalPutMB += EndpointStats.UploadedMB;

		Row++;
	}

#endif

	return Panel;
}

#undef LOCTEXT_NAMESPACE
