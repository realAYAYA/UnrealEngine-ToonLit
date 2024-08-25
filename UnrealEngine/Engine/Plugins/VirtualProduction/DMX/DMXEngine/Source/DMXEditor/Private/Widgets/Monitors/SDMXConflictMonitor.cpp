// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXConflictMonitor.h"

#include "Algo/Transform.h"
#include "Analytics/DMXEditorToolAnalyticsProvider.h"
#include "DMXConflictMonitorConflictModel.h"
#include "DMXEditorLog.h"
#include "Commands/DMXConflictMonitorCommands.h"
#include "DMXEditorSettings.h"
#include "DMXEditorStyle.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Application/SlateApplication.h"
#include "IO/DMXConflictMonitor.h"
#include "IO/DMXPortManager.h"
#include "SDMXConflictMonitorToolbar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/SRichTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXConflictMonitor"

namespace UE::DMX
{
	const FName SDMXConflictMonitor::FColumnIds::Ports = "Ports";
	const FName SDMXConflictMonitor::FColumnIds::Universe = "Universe";
	const FName SDMXConflictMonitor::FColumnIds::Conflicts = "Conflicts";
	const FName SDMXConflictMonitor::FColumnIds::Channels = "Channels";

	SDMXConflictMonitor::SDMXConflictMonitor()
		: StatusInfo(EDMXConflictMonitorStatusInfo::Idle)
		, AnalyticsProvider("ConflictMonitor")
	{}

	void SDMXConflictMonitor::Construct(const FArguments& InArgs)
	{
		SetupCommandList();
		SetCanTick(false);

		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SDMXConflictMonitorToolbar, CommandList.ToSharedRef())
				.StatusInfo_Lambda([this]()
					{
						return StatusInfo;
					})
				.OnDepthChanged_Lambda([this]()
					{
						Refresh();
					})
			]

			// Log
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(16.f)
			[
				SNew(SScrollBox)
				.Orientation(EOrientation::Orient_Vertical)
					
				+ SScrollBox::Slot()
				.AutoSize()
				[
					SNew(SBorder)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					[
						SAssignNew(TextBlock, SRichTextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.AutoWrapText(true)
						.TextStyle(FAppStyle::Get(), "MessageLog")
						.DecoratorStyleSet(&FDMXEditorStyle::Get())
					]
				]
			]
		];

		Refresh();

		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		if (EditorSettings->ConflictMonitorSettings.bRunWhenOpened)
		{
			Play();
		}
	}

	void SDMXConflictMonitor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (FSlateApplication::Get().AnyMenusVisible())
		{
			return;
		}

		const FDMXConflictMonitor* ConflictMonitor = FDMXConflictMonitor::Get();
		if (!ConflictMonitor)
		{
			return;
		}

		const TMap<FName, TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>> NewOutboundConflicts = ConflictMonitor->GetOutboundConflictsSynchronous();

		if (!CachedOutboundConflicts.OrderIndependentCompareEqual(NewOutboundConflicts) &&
			!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
		{
			CachedOutboundConflicts = NewOutboundConflicts;
			Refresh();
		}

		UpdateStatusInfo();
	}

	void SDMXConflictMonitor::Refresh()
	{
		// Test new items for changes
		TArray<TSharedPtr<FDMXConflictMonitorConflictModel>> NewModels;
		Algo::Transform(CachedOutboundConflicts, NewModels, [](const TPair<FName, TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>>& Conflicts)
			{
				return MakeShared<FDMXConflictMonitorConflictModel>(Conflicts.Value);
			});

		FString NewText;
		for (const TSharedPtr<FDMXConflictMonitorConflictModel>& Model : NewModels)
		{
			constexpr bool bWithMarkup = true;
			NewText.Append(Model->GetConflictAsString(bWithMarkup));
			NewText.Append(TEXT("\n"));
		}

		// Auto-pause even if the data hasn't changed
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		if (!NewModels.IsEmpty() && IsScanning() && EditorSettings->ConflictMonitorSettings.bAutoPause)
		{
			Pause();
		}

		// Skip if text did not change
		if (TextBlock->GetText().ToString() == NewText)
		{
			return;
		}

		Models = NewModels;
		TextBlock->SetText(FText::FromString(NewText));

		// Log conflicts (without markup)
		if (IsPrintingToLog())
		{
			for (const TSharedPtr<FDMXConflictMonitorConflictModel>& Model : NewModels)
			{
				UE_LOG(LogDMXEditor, Log, TEXT("%s"), *Model->GetConflictAsString());
			}
		}
	}

	void SDMXConflictMonitor::SetupCommandList()
	{
		CommandList = MakeShared<FUICommandList>();

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().StartScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Play),
			FCanExecuteAction::CreateLambda([this]
				{
					return !GetCanTick() && !bIsPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !GetCanTick() && !bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().PauseScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Pause),
			FCanExecuteAction::CreateLambda([this]
				{
					return GetCanTick();
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return GetCanTick();
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ResumeScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Play),
			FCanExecuteAction::CreateLambda([this]
				{
					return !GetCanTick() && bIsPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !GetCanTick() && bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().StopScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Stop),
			FCanExecuteAction::CreateLambda([this]
				{
					return GetCanTick() || bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ToggleAutoPause,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::ToggleAutoPause),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsAutoPause)
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().TogglePrintToLog,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::TogglePrintToLog),
			FCanExecuteAction::CreateLambda([this]
				{
					return IsAutoPause();
				}),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsPrintingToLog)
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ToggleRunWhenOpened,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::ToggleRunWhenOpened),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsRunWhenOpened)		
		);
	}

	void SDMXConflictMonitor::Play()
	{
		UserSession = FDMXConflictMonitor::Join("SDMXConflictMonitor");

		bIsPaused = false;
		SetCanTick(true);

		UpdateStatusInfo();
	}

	void SDMXConflictMonitor::Pause()
	{
		UserSession.Reset();

		bIsPaused = true;
		SetCanTick(false);

		UpdateStatusInfo();
	}

	void SDMXConflictMonitor::Stop()
	{
		UserSession.Reset();

		bIsPaused = false;
		SetCanTick(false);

		CachedOutboundConflicts.Reset();
		Models.Reset();
		Refresh();

		UpdateStatusInfo();
	}

	void SDMXConflictMonitor::SetAutoPause(bool bEnabled)
	{
		UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
		EditorSettings->ConflictMonitorSettings.bAutoPause = bEnabled;

		EditorSettings->SaveConfig();
	}

	void SDMXConflictMonitor::ToggleAutoPause()
	{
		SetAutoPause(!IsAutoPause());
	}

	bool SDMXConflictMonitor::IsAutoPause() const
	{
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		return EditorSettings->ConflictMonitorSettings.bAutoPause;
	}

	void SDMXConflictMonitor::SetPrintToLog(bool bEnabled)
	{
		UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
		EditorSettings->ConflictMonitorSettings.bPrintToLog = bEnabled;

		EditorSettings->SaveConfig();
	}

	void SDMXConflictMonitor::TogglePrintToLog()
	{
		SetPrintToLog(!IsPrintingToLog());
	}

	bool SDMXConflictMonitor::IsPrintingToLog() const
	{
		// Only available when auto-pause
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		return EditorSettings->ConflictMonitorSettings.bPrintToLog && IsAutoPause();
	}

	void SDMXConflictMonitor::SetRunWhenOpened(bool bEnabled)
	{
		UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
		if (EditorSettings->ConflictMonitorSettings.bRunWhenOpened != bEnabled)
		{
			EditorSettings->ConflictMonitorSettings.bRunWhenOpened = bEnabled;
			EditorSettings->SaveConfig();
		}
	}
	
	void SDMXConflictMonitor::ToggleRunWhenOpened()
	{
		SetRunWhenOpened(!IsRunWhenOpened());
	}
	
	bool SDMXConflictMonitor::IsRunWhenOpened() const
	{
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		return EditorSettings->ConflictMonitorSettings.bRunWhenOpened;
	}

	bool SDMXConflictMonitor::IsScanning() const
	{
		return GetCanTick() && !bIsPaused;
	}

	void SDMXConflictMonitor::UpdateStatusInfo()
	{
		if (bIsPaused)
		{
			StatusInfo = EDMXConflictMonitorStatusInfo::Paused;
		}
		else if (!GetCanTick())
		{
			StatusInfo = EDMXConflictMonitorStatusInfo::Idle;
		}
		else if (Models.IsEmpty())
		{
			StatusInfo = EDMXConflictMonitorStatusInfo::OK;
		}
		else
		{
			StatusInfo = EDMXConflictMonitorStatusInfo::Conflict;
		}
	}
}

#undef LOCTEXT_NAMESPACE
