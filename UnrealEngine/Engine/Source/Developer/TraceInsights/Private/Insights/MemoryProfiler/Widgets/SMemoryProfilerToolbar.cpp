// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemoryProfilerToolbar.h"

#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SBoxPanel.h"

// Insights
#include "Insights/InsightsCommands.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/MemoryProfiler/MemoryProfilerCommands.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "SMemoryProfilerToolbar"

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemoryProfilerToolbar::SMemoryProfilerToolbar()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SMemoryProfilerToolbar::~SMemoryProfilerToolbar()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemoryProfilerToolbar::Construct(const FArguments& InArgs)
{
	struct Local
	{
		static void FillViewToolbar(FToolBarBuilder& ToolbarBuilder, const FArguments &InArgs)
		{
			ToolbarBuilder.BeginSection("View");
			{
				ToolbarBuilder.AddToolBarButton(FMemoryProfilerCommands::Get().ToggleTimingViewVisibility,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TimingView.ToolBar"));
				ToolbarBuilder.AddToolBarButton(FMemoryProfilerCommands::Get().ToggleMemInvestigationViewVisibility,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.MemInvestigationView.ToolBar"));
				ToolbarBuilder.AddToolBarButton(FMemoryProfilerCommands::Get().ToggleMemTagTreeViewVisibility,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.MemTagTreeView.ToolBar"));
				ToolbarBuilder.AddToolBarButton(FMemoryProfilerCommands::Get().ToggleModulesViewVisibility,
					NAME_None, TAttribute<FText>(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ModulesView.ToolBar"));
			}
			ToolbarBuilder.EndSection();

			if (InArgs._ToolbarExtender.IsValid())
			{
				InArgs._ToolbarExtender->Apply("MainToolbar", EExtensionHook::First, ToolbarBuilder);
			}
		}

		static void FillRightSideToolbar(FToolBarBuilder& ToolbarBuilder, const FArguments &InArgs)
		{
			ToolbarBuilder.BeginSection("Debug");
			{
				ToolbarBuilder.AddToolBarButton(FInsightsCommands::Get().ToggleDebugInfo,
					NAME_None, FText(), TAttribute<FText>(),
					FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.Debug.ToolBar"));
			}
			ToolbarBuilder.EndSection();

			if (InArgs._ToolbarExtender.IsValid())
			{
				InArgs._ToolbarExtender->Apply("RightSideToolbar", EExtensionHook::First, ToolbarBuilder);
			}
		}
	};

	TSharedPtr<FUICommandList> CommandList = FInsightsManager::Get()->GetCommandList();

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList.ToSharedRef(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FInsightsStyle::Get(), "PrimaryToolbar");
	Local::FillViewToolbar(ToolbarBuilder, InArgs);

	FSlimHorizontalToolBarBuilder RightSideToolbarBuilder(CommandList.ToSharedRef(), FMultiBoxCustomization::None);
	RightSideToolbarBuilder.SetStyle(&FInsightsStyle::Get(), "PrimaryToolbar");
	Local::FillRightSideToolbar(RightSideToolbarBuilder, InArgs);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.0)
		.Padding(0.0f)
		[
			ToolbarBuilder.MakeWidget()
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f)
		[
			RightSideToolbarBuilder.MakeWidget()
		]
	];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
