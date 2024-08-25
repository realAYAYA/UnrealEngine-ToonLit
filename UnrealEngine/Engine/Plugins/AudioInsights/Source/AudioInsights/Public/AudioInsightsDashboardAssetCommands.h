// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Templates/SharedPointer.h"


namespace UE::Audio::Insights
{
	class AUDIOINSIGHTS_API FDashboardAssetCommands : public TCommands<FDashboardAssetCommands>
	{
	public:
		FDashboardAssetCommands();

		virtual void RegisterCommands() override;

		virtual void AddAssetCommands(FToolBarBuilder& OutToolbarBuilder) const;

		virtual TSharedPtr<const FUICommandInfo> GetBrowserSyncCommand() const;
		virtual TSharedPtr<const FUICommandInfo> GetOpenCommand() const;

		virtual TSharedPtr<const FUICommandInfo> GetStartCommand() const;
		virtual TSharedPtr<const FUICommandInfo> GetStopCommand() const;

		virtual TSharedPtr<const FUICommandInfo> GetMuteCommand() const;
		virtual TSharedPtr<const FUICommandInfo> GetSoloCommand() const;
		virtual TSharedPtr<const FUICommandInfo> GetClearMuteSoloCommand() const;

	private:
		FSlateIcon GetStartIcon() const;
		FSlateIcon GetStopIcon() const;

		/** Selects the sound in the content browser. */
		TSharedPtr<FUICommandInfo> BrowserSync;

		/** Opens the sound asset in the content browser. */
		TSharedPtr<FUICommandInfo> Open;

		/** Starts the trace session & enables the required audio channels on. */
		TSharedPtr<FUICommandInfo> Start;

		/** Stops the trace session & disables the required audio channels off. */
		TSharedPtr<FUICommandInfo> Stop;

		/** Mutes the selected item in the Mixer Source View. */
		TSharedPtr<FUICommandInfo> Mute;

		/** Solo the selected item in the Mixer Source View. */
		TSharedPtr<FUICommandInfo> Solo;

		/** Clears any Mute/Solo state from any of the items in the Mixer Source View. */
		TSharedPtr<FUICommandInfo> ClearMuteSolo;
	};
} // namespace UE::Audio::Insights
