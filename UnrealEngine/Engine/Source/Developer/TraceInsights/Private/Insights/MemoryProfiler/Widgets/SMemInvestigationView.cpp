// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemInvestigationView.h"

#include "DesktopPlatformModule.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"

// Insights
#include "Insights/Common/Stopwatch.h"
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsStyle.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/MemoryProfiler/Common/SymbolSearchPathsHelper.h"
#include "Insights/MemoryProfiler/ViewModels/MemorySharedState.h"
#include "Insights/MemoryProfiler/Widgets/SMemAllocTableTreeView.h"
#include "Insights/MemoryProfiler/Widgets/SMemoryProfilerWindow.h"
#include "Insights/ViewModels/TimeRulerTrack.h"
#include "Insights/Widgets/STimingView.h"

#define LOCTEXT_NAMESPACE "SMemInvestigationView"

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemInvestigationView::SMemInvestigationView()
	: ProfilerWindowWeakPtr()
	, bIncludeHeapAllocs(false)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemInvestigationView::~SMemInvestigationView()
{
	// Remove ourselves from the Insights manager.
	if (FInsightsManager::Get().IsValid())
	{
		FInsightsManager::Get()->GetSessionChangedEvent().RemoveAll(this);
	}

	Session.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMemInvestigationView::Construct(const FArguments& InArgs, TSharedPtr<SMemoryProfilerWindow> InProfilerWindow)
{
	check(InProfilerWindow.IsValid());
	ProfilerWindowWeakPtr = InProfilerWindow;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.Padding(8.0f)
		[
			ConstructInvestigationWidgetArea()
		]
	];

	// Register ourselves with the Insights manager.
	FInsightsManager::Get()->GetSessionChangedEvent().AddSP(this, &SMemInvestigationView::InsightsManager_OnSessionChanged);

	// Update the Session (i.e. when analysis session was already started).
	InsightsManager_OnSessionChanged();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SMemInvestigationView::ConstructInvestigationWidgetArea()
{
	TSharedRef<SWidget> Widget =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QueryRuleText", "Rule:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(QueryRuleComboBox, SComboBox<TSharedPtr<Insights::FMemoryRuleSpec>>)
				.ToolTipText(this, &SMemInvestigationView::QueryRule_GetTooltipText)
				.OptionsSource(GetAvailableQueryRules())
				.OnSelectionChanged(this, &SMemInvestigationView::QueryRule_OnSelectionChanged)
				.OnGenerateWidget(this, &SMemInvestigationView::QueryRule_OnGenerateWidget)
				[
					SNew(STextBlock)
					.Text(this, &SMemInvestigationView::QueryRule_GetSelectedText)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructTimeMarkerWidget(0)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructTimeMarkerWidget(1)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructTimeMarkerWidget(2)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructTimeMarkerWidget(3)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SMemInvestigationView::QueryRule_GetTooltipText)
			.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
			{
				return bIncludeHeapAllocs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				bIncludeHeapAllocs = (InCheckBoxState == ECheckBoxState::Checked);
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IncludeHeapAllocsText", "Include Heap Allocs"))
			]
			.ToolTipText(LOCTEXT("IncludeHeapAllocsToolTipText", "Include heap allocs."))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QueryTargetWindow", "Target Window:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(QueryTargetComboBox, SComboBox<TSharedPtr<Insights::FQueryTargetWindowSpec>>)
				.ToolTipText(LOCTEXT("QueryTargetWindowTooltip", "Select an existing or new window where the query results will be displayed"))
				.OptionsSource(GetAvailableQueryTargets())
				.OnSelectionChanged(this, &SMemInvestigationView::QueryTarget_OnSelectionChanged)
				.OnGenerateWidget(this, &SMemInvestigationView::QueryTarget_OnGenerateWidget)
				[
					SNew(STextBlock)
					.Text(this, &SMemInvestigationView::QueryTarget_GetSelectedText)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("RunQueryBtnText", "Run Query"))
			.ToolTipText(LOCTEXT("RunQueryBtnToolTipText", "Run Memory Query.\nThe resulting list of allocations will be available in a tree view."))
			.OnClicked(this, &SMemInvestigationView::RunQuery)
		]
	
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Fill)
		[
			SAssignNew(SymbolPathsTextBlock, STextBlock)
			.ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f))
			.AutoWrapText(true)
		]
	;

	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		QueryRuleComboBox->SetSelectedItem(SharedState.GetCurrentMemoryRule());
	}

	return Widget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SMemInvestigationView::QueryRule_OnGenerateWidget(TSharedPtr<Insights::FMemoryRuleSpec> InRule)
{
	const FText QueryRuleText = FText::Format(LOCTEXT("QueryRuleComboBox_TextFmt", "{0} ({1})"), InRule->GetVerboseName(), InRule->GetShortName());

	TSharedRef<SHorizontalBox> Widget = SNew(SHorizontalBox);

	Widget->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(0.0f, 2.0f, 2.0f, 2.0f)
		[
			SNew(SImage)
			.Visibility_Lambda([Widget]() { return Widget->GetParentWidget()->IsHovered() ? EVisibility::Visible : EVisibility::Hidden; })
			.Image(FInsightsStyle::GetBrush("Icons.Hint.TreeItem"))
			.ToolTipText(InRule->GetDescription())
		];

	Widget->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(QueryRuleText)
			.Margin(2.0f)
		];

	return Widget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SMemInvestigationView::ConstructTimeMarkerWidget(uint32 TimeMarkerIndex)
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (!ProfilerWindow.IsValid())
	{
		return SNew(SBox);
	}

	FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
	const uint32 NumTimeMarkers = ProfilerWindow->GetNumCustomTimeMarkers();
	if (TimeMarkerIndex >= NumTimeMarkers)
	{
		return SNew(SBox);
	}

	const TSharedRef<Insights::FTimeMarker>& TimeMarker = ProfilerWindow->GetCustomTimeMarker(TimeMarkerIndex);
	TSharedPtr<Insights::FTimeMarker> PreviousTimeMarker;
	if (TimeMarkerIndex > 0)
	{
		PreviousTimeMarker = ProfilerWindow->GetCustomTimeMarker(TimeMarkerIndex - 1);
	}

	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
		.Visibility_Lambda([TimeMarker]()
			{
				return TimeMarker->IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
			})

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(20.0f, 1.0f, 4.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TimeMarker->GetName()))
			.ColorAndOpacity(FSlateColor(TimeMarker->GetColor()))
			.OnDoubleClicked(this, &SMemInvestigationView::OnTimeMarkerLabelDoubleClicked, TimeMarkerIndex)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 1.0f, 0.0f, 1.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(110.0f)
			.Text_Lambda([TimeMarker]()
				{
					return FText::FromString(FString::Printf(TEXT("%.9f"), TimeMarker->GetTime()));
				})
			.OnTextCommitted_Lambda([this, TimeMarker](const FText& InText, ETextCommit::Type InCommitType)
				{
					const double Time = FCString::Atod(*InText.ToString());
					TimeMarker->SetTime(Time);
					TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
					if (ProfilerWindow.IsValid())
					{
						ProfilerWindow->OnTimeMarkerChanged(Insights::ETimeChangedFlags::None, TimeMarker);
					}
				})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 1.0f, 0.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([TimeMarker, PreviousTimeMarker]()
				{
					if (PreviousTimeMarker.IsValid())
					{
						return FText::FromString(FString::Printf(TEXT("%s (+%s)"),
							*TimeUtils::FormatTime(TimeMarker->GetTime(), 0.1),
							*TimeUtils::FormatTime(TimeMarker->GetTime() - PreviousTimeMarker->GetTime(), 0.1)));
					}
					else
					{
						return FText::FromString(TimeUtils::FormatTime(TimeMarker->GetTime(), 0.1));
					}
				})
		];

	return Widget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::InsightsManager_OnSessionChanged()
{
	TSharedPtr<const TraceServices::IAnalysisSession> NewSession = FInsightsManager::Get()->GetSession();
	if (NewSession != Session)
	{
		Session = NewSession;
		Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::Reset()
{
	SymbolPathsTextBlock->SetText(FText::GetEmpty());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateSymbolPathsText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Query Rules
////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<TSharedPtr<Insights::FMemoryRuleSpec>>* SMemInvestigationView::GetAvailableQueryRules()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		return &SharedState.GetMemoryRules();
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::QueryRule_OnSelectionChanged(TSharedPtr<Insights::FMemoryRuleSpec> InRule, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
		if (ProfilerWindow.IsValid() && InRule)
		{
			FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
			SharedState.SetCurrentMemoryRule(InRule);
			ProfilerWindow->OnMemoryRuleChanged();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemInvestigationView::QueryRule_GetSelectedText() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		TSharedPtr<Insights::FMemoryRuleSpec> Rule = SharedState.GetCurrentMemoryRule();
		if (Rule)
		{
			return FText::Format(LOCTEXT("QueryRuleComboBox_TextFmt", "{0} ({1})"), Rule->GetVerboseName(), Rule->GetShortName());
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemInvestigationView::QueryRule_GetTooltipText() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		TSharedPtr<Insights::FMemoryRuleSpec> Rule = SharedState.GetCurrentMemoryRule();
		if (Rule.IsValid())
		{
			return Rule->GetDescription();
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>>* SMemInvestigationView::GetAvailableQueryTargets()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		return &SharedState.GetQueryTargets();
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::QueryTarget_OnSelectionChanged(TSharedPtr<Insights::FQueryTargetWindowSpec> InTarget, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Type::Direct)
	{
		TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
		if (ProfilerWindow.IsValid() && InTarget)
		{
			FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
			SharedState.SetCurrentQueryTarget(InTarget);
		}
	}
	else
	{
		QueryTargetComboBox->SetSelectedItem(InTarget);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemInvestigationView::UpdateSymbolPathsText() const
{
	if (SymbolPathsTextBlock->GetText().IsEmpty() && Session)
	{
		if (const TraceServices::IModuleProvider* ModuleProvider = ReadModuleProvider(*Session.Get()))
		{
			SymbolPathsTextBlock->SetText(FSymbolSearchPathsHelper::GetLocalizedSymbolSearchPathsText(ModuleProvider));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemInvestigationView::QueryTarget_OnGenerateWidget(TSharedPtr<Insights::FQueryTargetWindowSpec> InTarget)
{
	return SNew(STextBlock)
		.Text(InTarget->GetText())
		.Margin(2.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemInvestigationView::QueryTarget_GetSelectedText() const
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
		TSharedPtr<Insights::FQueryTargetWindowSpec> Target = SharedState.GetCurrentQueryTarget();
		if (Target)
		{
			return Target->GetText();
		}
	}
	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemInvestigationView::RunQuery()
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (!ProfilerWindow.IsValid())
	{
		UE_LOG(MemoryProfiler, Error, TEXT("[MemQuery] Invalid Profiler Window!"));
		return FReply::Handled();
	}

	FMemorySharedState& SharedState = ProfilerWindow->GetSharedState();
	TSharedPtr<Insights::FMemoryRuleSpec> Rule = SharedState.GetCurrentMemoryRule();
	if (!Rule)
	{
		UE_LOG(MemoryProfiler, Error, TEXT("[MemQuery] Invalid Rule!"));
		return FReply::Handled();
	}

	const uint32 NumTimeMarkers = ProfilerWindow->GetNumCustomTimeMarkers();
	const uint32 RuleNumTimeMarkers = Rule->GetNumTimeMarkers();
	if (RuleNumTimeMarkers > NumTimeMarkers)
	{
		UE_LOG(MemoryProfiler, Error, TEXT("[MemQuery] Only %d time markers available. Current rule (%s) requires %u time markers!"),
			NumTimeMarkers, *Rule->GetShortName().ToString(), RuleNumTimeMarkers);
		return FReply::Handled();
	}

#if !NO_LOGGING
	TStringBuilder<1024> Builder;
	if (RuleNumTimeMarkers > 0)
	{
		Builder.Append(TEXT(" ("));
		for (uint32 TimeMarkerIndex = 0; TimeMarkerIndex < RuleNumTimeMarkers; ++TimeMarkerIndex)
		{
			if (TimeMarkerIndex != 0)
			{
				Builder.Append(TEXT(", "));
			}
			Builder.AppendChar((TCHAR)(TEXT('A') + TimeMarkerIndex));
			const TSharedRef<Insights::FTimeMarker>& TimeMarker = ProfilerWindow->GetCustomTimeMarker(TimeMarkerIndex);
			Builder.Appendf(TEXT("=%.9f"), TimeMarker->GetTime());
		}
		Builder.Append(TEXT(")"));
	}
	UE_LOG(MemoryProfiler, Log, TEXT("[MemQuery] Run Query %s%s..."), *Rule->GetShortName().ToString(), Builder.ToString());
#endif

	TSharedPtr<Insights::SMemAllocTableTreeView> MemAllocTableTreeView = ProfilerWindow->ShowMemAllocTableTreeViewTab();
	if (MemAllocTableTreeView)
	{
		Insights::SMemAllocTableTreeView::FQueryParams QueryParams;
		QueryParams.Rule = Rule;
		QueryParams.TimeMarkers[0] = (RuleNumTimeMarkers > 0) ? ProfilerWindow->GetCustomTimeMarker(0)->GetTime() : 0.0;
		QueryParams.TimeMarkers[1] = (RuleNumTimeMarkers > 1) ? ProfilerWindow->GetCustomTimeMarker(1)->GetTime() : 0.0;
		QueryParams.TimeMarkers[2] = (RuleNumTimeMarkers > 2) ? ProfilerWindow->GetCustomTimeMarker(2)->GetTime() : 0.0;
		QueryParams.TimeMarkers[3] = (RuleNumTimeMarkers > 3) ? ProfilerWindow->GetCustomTimeMarker(3)->GetTime() : 0.0;
		QueryParams.bIncludeHeapAllocs = bIncludeHeapAllocs;
		MemAllocTableTreeView->SetQueryParams(QueryParams);
	}

	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemInvestigationView::OnTimeMarkerLabelDoubleClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, uint32 TimeMarkerIndex)
{
	TSharedPtr<SMemoryProfilerWindow> ProfilerWindow = GetProfilerWindow();
	if (ProfilerWindow.IsValid())
	{
		const uint32 NumTimeMarkers = ProfilerWindow->GetNumCustomTimeMarkers();
		if (TimeMarkerIndex < NumTimeMarkers)
		{
			const TSharedRef<Insights::FTimeMarker>& TimeMarker = ProfilerWindow->GetCustomTimeMarker(TimeMarkerIndex);
			TSharedPtr<STimingView> TimingView = ProfilerWindow->GetTimingView();
			if (TimingView.IsValid())
			{
				// Move timer to the center of the timing view.
				const double Time = (TimingView->GetViewport().GetStartTime() + TimingView->GetViewport().GetEndTime()) / 2.0;
				TimeMarker->SetTime(Time);
			}
		}
	}
	return FReply::Handled();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
