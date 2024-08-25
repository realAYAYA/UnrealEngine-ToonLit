// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Analytics/DMXEditorToolAnalyticsProvider.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class SRichTextBlock;


namespace UE::DMX
{
	class FDMXConflictMonitorConflictModel;
	class FDMXConflictMonitorUserSession;
	struct FDMXMonitoredOutboundDMXData;
	enum class EDMXConflictMonitorStatusInfo : uint8;

	/** Monitors conflicts. */
	class SDMXConflictMonitor
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXConflictMonitor)
			{}

		SLATE_END_ARGS()

		/** ColumnIds for the list view */
		struct FColumnIds
		{
			static const FName Conflicts;
			static const FName Ports;
			static const FName Universe;
			static const FName Channels;
		};

		SDMXConflictMonitor();

		/** Constructs the widget */
		void Construct(const FArguments& InArgs);

	protected:
		//~ Begin SWidget interface
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End SWidget interface

	private:
		/** Refreshes the widget */
		void Refresh();

		/** Initializes the command list for this widget */
		void SetupCommandList();

		/** Updates the status info for this monitor */
		void UpdateStatusInfo();

		/** Returns true if the monitor is scanning */
		bool IsScanning() const;

		void Play();
		void Pause();
		void Stop();

		void SetAutoPause(bool bEnabled);
		void ToggleAutoPause();
		bool IsAutoPause() const;

		void SetPrintToLog(bool bEnabled);
		void TogglePrintToLog();
		bool IsPrintingToLog() const;

		void SetRunWhenOpened(bool bEnabled);
		void ToggleRunWhenOpened();
		bool IsRunWhenOpened() const;

		/** Cached outbound conflicts */
		TMap<FName, TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>> CachedOutboundConflicts;

		/** Items displayed in the list */
		TArray<TSharedPtr<FDMXConflictMonitorConflictModel>> Models;

		/** Text block displaying outbound conflicts, one conflict per row */
		TSharedPtr<SRichTextBlock> TextBlock;

		/** Timer to refresh at refresh period */
		double Timer = 0.0;

		/** True if paused */
		bool bIsPaused = false;

		/** The status of the monitor. Note status info is ment for UI purposes, and not the state of the monitor. */
		EDMXConflictMonitorStatusInfo StatusInfo;

		/** The conflict montitor user session used by this widget */
		TSharedPtr<FDMXConflictMonitorUserSession> UserSession;

		/** Commandlist specific to this widget (only one can ever be displayed) */
		TSharedPtr<FUICommandList> CommandList;

		/** The analytics provider for this tool */
		FDMXEditorToolAnalyticsProvider AnalyticsProvider;

		// Slate args
		TAttribute<double> UpdateInterval;
	};
}
