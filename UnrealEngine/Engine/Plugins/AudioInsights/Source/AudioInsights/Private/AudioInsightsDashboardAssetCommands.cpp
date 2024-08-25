// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioInsightsDashboardAssetCommands.h"

#include "AudioInsightsModule.h"
#include "AudioInsightsStyle.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Misc/Attribute.h"

#define LOCTEXT_NAMESPACE "AudioInsights"


namespace UE::Audio::Insights
{
	FDashboardAssetCommands::FDashboardAssetCommands()
		: TCommands<FDashboardAssetCommands>("AudioInsightsDashboardAssetCommands", LOCTEXT("AudioInsightsDashboardAssetCommands", "Dashboard Asset Commands"), NAME_None, FSlateStyle::GetStyleName())
	{
	}

	void FDashboardAssetCommands::RegisterCommands()
	{
		UI_COMMAND(BrowserSync, "Browse", "Browses to the selected asset in the content browser.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Open, "Open", "Opens the selected asset(s) in respective editor(s).", EUserInterfaceActionType::Button, FInputChord());

		UI_COMMAND(Start, "Start Trace", "Starts the active trace session used by Audio Insights.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Stop, "Stop Trace", "Stops the active trace session used by Audio Insights.", EUserInterfaceActionType::Button, FInputChord());

		UI_COMMAND(Mute, "Mute (Toggle)", "Mutes the selected item in the Mixer Source View.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(Solo, "Solo (Toggle)", "Solo the selected item in the Mixer Source View.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ClearMuteSolo, "Clear All Mute/Solo", "Clears any Mute/Solo state from any of the items in the Mixer Source View.", EUserInterfaceActionType::Button, FInputChord());
	}

	void FDashboardAssetCommands::AddAssetCommands(FToolBarBuilder& OutToolbarBuilder) const
	{
		OutToolbarBuilder.AddToolBarButton(
			BrowserSync,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>::Create([]() { return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.ContentBrowser"); }),
			"BrowserSync"
		);

		OutToolbarBuilder.AddToolBarButton(
			Open,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>::Create([]() { return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Open"); }),
			"Open"
		);
	}

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetBrowserSyncCommand() const
	{
		return BrowserSync;
	};

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetOpenCommand() const
	{
		return Open;
	}

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetStartCommand() const
	{
		return Start;
	}

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetStopCommand() const
	{
		return Stop;
	}

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetMuteCommand() const
	{
		return Mute;
	};

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetSoloCommand() const
	{
		return Solo;
	}

	TSharedPtr<const FUICommandInfo> FDashboardAssetCommands::GetClearMuteSoloCommand() const
	{
		return ClearMuteSolo;
	}

	FSlateIcon FDashboardAssetCommands::GetStartIcon() const
	{
		const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
		return FSlateStyle::Get().CreateIcon(TraceModule.IsTraceAnalysisActive()
			? "AudioInsights.Icon.Start.Inactive"
			: "AudioInsights.Icon.Start.Active"
		);
	}

	FSlateIcon FDashboardAssetCommands::GetStopIcon() const
	{
		const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
		return FSlateStyle::Get().CreateIcon(TraceModule.IsTraceAnalysisActive()
			? "AudioInsights.Icon.Stop.Active"
			: "AudioInsights.Icon.Stop.Inactive"
		);
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
