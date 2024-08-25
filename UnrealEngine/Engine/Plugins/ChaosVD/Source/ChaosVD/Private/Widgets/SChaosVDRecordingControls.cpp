// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDRecordingControls.h"

#include "AsyncCompilationHelpers.h"
#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "ChaosVDStyle.h"
#include "ChaosVDRuntimeModule.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Editor.h"
#include "Input/Reply.h"
#include "Misc/MessageDialog.h"
#include "StatusBarSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDRecordingControls::Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef)
{
	MainTabWeakPtr = InMainTabSharedRef;
	StatusBarID = InMainTabSharedRef->GetStatusBarName();
	
	RecordingAnimation = FCurveSequence();
	RecordingAnimation.AddCurve(0.f, 1.5f, ECurveEaseFunction::Linear);

	ChildSlot
	[
		GenerateToolbarWidget()
	];

	RecordingStartedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartedCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &SChaosVDRecordingControls::HandleRecordingStart));
	RecordingStoppedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &SChaosVDRecordingControls::HandleRecordingStop));
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip)
{
	return SNew(SButton)
			.OnClicked(FOnClicked::CreateRaw(this, &SChaosVDRecordingControls::ToggleRecordingState, RecordingMode))
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.IsEnabled_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonEnabled, RecordingMode)
			.Visibility_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonVisible, RecordingMode)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnHovered_Lambda([this](){ bRecordingButtonHovered = true;})
			.OnUnhovered_Lambda([this](){ bRecordingButtonHovered = false;})
			.ToolTipText_Lambda([this, StartRecordingTooltip]()
			{
				return IsRecording() ? LOCTEXT("StopRecordButtonDesc", "Stop the current recording ") : StartRecordingTooltip;
			})
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(FMargin(0, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image_Raw(this, &SChaosVDRecordingControls::GetRecordOrStopButton, RecordingMode)
					.ColorAndOpacity_Lambda([this]()
					{
						if (IsRecording())
						{
							if (!RecordingAnimation.IsPlaying())
							{
								RecordingAnimation.Play(AsShared(), true);
							}

							const FLinearColor Color = bRecordingButtonHovered ? FLinearColor::Red : FLinearColor::White;
							return FSlateColor(bRecordingButtonHovered ? Color : Color.CopyWithNewOpacity(0.2f + 0.8f * RecordingAnimation.GetLerp()));
						}

						RecordingAnimation.Pause();
						return FSlateColor::UseSubduedForeground();
					})
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin(4, 0, 0, 0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Visibility_Lambda([this](){return IsRecording() ? EVisibility::Collapsed : EVisibility::Visible;})
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
					.Text_Lambda( [RecordingMode]()
					{
						return RecordingMode == EChaosVDRecordingMode::File ? LOCTEXT("RecordToFileButtonLabel", "Record To File") : LOCTEXT("RecordToLiveButtonLabel", "Record Live Session");
					})
				]
			];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateRecordingTimeTextBlock()
{
	return SNew(SBox)
			.VAlign(VAlign_Center)
			.Visibility_Lambda([this]() { return IsRecording() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallButtonText")
				.Text_Raw(this, &SChaosVDRecordingControls::GetRecordingTimeText)
				.ColorAndOpacity(FColor::White)
			];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateToolbarWidget()
{
	RegisterMenus();

	FToolMenuContext MenuContext;

	UChaosVDRecordingToolbarMenuContext* CommonContextObject = NewObject<UChaosVDRecordingToolbarMenuContext>();
	CommonContextObject->RecordingControlsWidget = SharedThis(this);

	MenuContext.AddObject(CommonContextObject);

	return UToolMenus::Get()->GenerateWidget(RecordingControlsToolbarName, MenuContext);
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateDataChannelsButton()
{
	return SNew(SComboButton)
			.ContentPadding(FMargin(6.0f, 0.0f))
			.IsEnabled_Raw(this, &SChaosVDRecordingControls::HasDataChannelsSupport)
			.MenuPlacement(MenuPlacement_AboveAnchor).ComboButtonStyle(&FAppStyle::Get()
			.GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.OnGetMenuContent(this, &SChaosVDRecordingControls::GenerateDataChannelsMenu)
			.HasDownArrow(true)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DataChannelsButton", "Data Channels"))
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("DialogButtonText"))
				]
			];
}

TSharedRef<SWidget> SChaosVDRecordingControls::GenerateDataChannelsMenu()
{
	using namespace Chaos::VisualDebugger;

	FMenuBuilder MenuBuilder(true, nullptr);

#if WITH_CHAOS_VISUAL_DEBUGGER
	MenuBuilder.BeginSection("CVDRecordingWidget", LOCTEXT("CVDRecordingMenuChannels", "Data Channels"));
	{
		FChaosVDDataChannelsManager::Get().EnumerateChannels([this, &MenuBuilder](const TSharedRef<FCVDDataChannel>& Channel)
		{
			MenuBuilder.AddMenuEntry(
				Channel->GetDisplayName(),
				FText::Format(LOCTEXT("ChannelDesc", "Enable/disable the {0} channel"), Channel->GetDisplayName()),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SChaosVDRecordingControls::ToggleChannelEnabledState, Channel.ToWeakPtr()),
					FCanExecuteAction::CreateSP(this, &SChaosVDRecordingControls::CanChangeChannelEnabledState, Channel.ToWeakPtr()), FIsActionChecked::CreateSP(this, &SChaosVDRecordingControls::IsChannelEnabled,  Channel.ToWeakPtr())), NAME_None, EUserInterfaceActionType::ToggleButton);

			return true;
		});
	}
#endif //WITH_CHAOS_VISUAL_DEBUGGER

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#if WITH_CHAOS_VISUAL_DEBUGGER

void SChaosVDRecordingControls::ToggleChannelEnabledState(TWeakPtr<FCVDDataChannel> Channel)
{
	if (const TSharedPtr<FCVDDataChannel>& ChannelPtr = Channel.Pin())
	{
		const bool bIsEnabled = ChannelPtr->IsChannelEnabled();
		ChannelPtr->SetChannelEnabled(!bIsEnabled);
	}
}

bool SChaosVDRecordingControls::IsChannelEnabled(TWeakPtr<FCVDDataChannel> Channel)
{
	if (const TSharedPtr<Chaos::VisualDebugger::FChaosVDOptionalDataChannel>& ChannelPtr = Channel.Pin())
	{
		return ChannelPtr->IsChannelEnabled();
	}
	
	return false;
}

bool SChaosVDRecordingControls::CanChangeChannelEnabledState(TWeakPtr<FCVDDataChannel> Channel)
{
	if (const TSharedPtr<FCVDDataChannel>& ChannelPtr = Channel.Pin())
	{
		return ChannelPtr->CanChangeEnabledState();
	}
	
	return false;
}

#endif // WITH_CHAOS_VISUAL_DEBUGGER

bool SChaosVDRecordingControls::HasDataChannelsSupport() const
{
#if WITH_CHAOS_VISUAL_DEBUGGER
	return true;
#else
	return false;
#endif
}

SChaosVDRecordingControls::~SChaosVDRecordingControls()
{
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStartedCallback(RecordingStartedHandle);
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);
	}

	if (ConnectionAttemptNotification.IsValid())
	{
		ConnectionAttemptNotification->ExpireAndFadeout();
	}
}

const FSlateBrush* SChaosVDRecordingControls::GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const
{
	const FSlateBrush* RecordIconBrush = FChaosVDStyle::Get().GetBrush("RecordIcon");
	return bRecordingButtonHovered && IsRecording() ? FChaosVDStyle::Get().GetBrush("StopIcon") : RecordIconBrush;
}

void SChaosVDRecordingControls::HandleRecordingStop()
{
	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!MainTabSharedPtr.IsValid())
	{
		return;
	}

	const bool bIsLiveSession = MainTabSharedPtr->GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().bIsLiveSession;

	if (UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr)
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingMessageHandle);

		if (bIsLiveSession)
		{
			const FText LiveSessionEnded = LOCTEXT("LiveSessionEndedMessage"," Live session has ended");
			LiveSessionEndedMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LiveSessionEnded);
		}
		else
		{
			const FText RecordingPathMessage = FText::Format(LOCTEXT("RecordingSavedPathMessage"," Recoring saved at {0} "), FText::AsCultureInvariant(FChaosVDRuntimeModule::Get().GetLastRecordingFileNamePath()));
			RecordingPathMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, RecordingPathMessage);
		}
	}
	
	if (!bIsLiveSession)
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("OpenLastRecordingMessage", "Do you want to load the recorded file now? ")) == EAppReturnType::Yes)
		{
			MainTabSharedPtr->GetChaosVDEngineInstance()->LoadRecording(FChaosVDRuntimeModule::Get().GetLastRecordingFileNamePath());
		}
	}
}

void SChaosVDRecordingControls::HandleRecordingStart()
{
	UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr;
	if (!StatusBarSubsystem)
	{
		return;
	}
	
	if (RecordingPathMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingPathMessageHandle);
		RecordingPathMessageHandle = FStatusBarMessageHandle();
	}
	
	if (LiveSessionEndedMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, LiveSessionEndedMessageHandle);
		LiveSessionEndedMessageHandle = FStatusBarMessageHandle();
	}

	RecordingMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LOCTEXT("RecordingMessgae", "Recording..."));
}

void SChaosVDRecordingControls::AttemptToConnectToLiveSession()
{
	if(!bAutoConnectionAttemptInProgress)
	{
		bAutoConnectionAttemptInProgress = true;
		PushConnectionAttemptNotification();
	}

	// We need to wait at least one tick before attempting to connect
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis = AsWeak()](float DeltaTime)
	{
		if (const TSharedPtr<SChaosVDRecordingControls> RecordingControlsPtr = StaticCastSharedPtr<SChaosVDRecordingControls>(WeakThis.Pin()))
		{
			if (const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = RecordingControlsPtr->MainTabWeakPtr.Pin())
			{
				RecordingControlsPtr->CurrentConnectionAttempts++;

				RecordingControlsPtr->UpdateConnectionAttemptNotification();

				static FString SessionAddress(TEXT("127.0.0.1"));
			
				int32 SessionID = 0;

				FChaosVDTraceManager::EnumerateActiveSessions(SessionAddress, [&SessionID](const UE::Trace::FStoreClient::FSessionInfo& InSessionInfo)
				{
					SessionID = InSessionInfo.GetTraceId();

					// CVD stops all active sessions before staring a recording, so we know our session ID will be first;
					return false;
				});

				// CVD needs the trace session name to be able to load a live session. Although the session exist, the session name might not be written right away
				// Trace files don't really have metadata, it is all part of the same stream, so we need to wait until it is written which might take a few ticks.
				// Therefore if it is not ready, try again a few times.
				if (!MainTabSharedPtr->ConnectToLiveSession(SessionID, SessionAddress))
				{
					if (RecordingControlsPtr->CurrentConnectionAttempts <= RecordingControlsPtr->MaxAutoplayConnectionAttempts)
					{
						UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] Failed to connect to live session | Attempting again in [%f]..."), ANSI_TO_TCHAR(__FUNCTION__), RecordingControlsPtr->IntervalBetweenAutoplayConnectionAttemptsSeconds);
						RecordingControlsPtr->AttemptToConnectToLiveSession();
					}
					else
					{
						RecordingControlsPtr->HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult::Failed);
						UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to live session | [%d] attempts exhausted..."), ANSI_TO_TCHAR(__FUNCTION__), RecordingControlsPtr->MaxAutoplayConnectionAttempts);	
					}
				}
				else
				{
					RecordingControlsPtr->HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult::Success);
				}
			}
		}
		return false;
	}), IntervalBetweenAutoplayConnectionAttemptsSeconds);
}

FReply SChaosVDRecordingControls::ToggleRecordingState(EChaosVDRecordingMode RecordingMode)
{
	if (!IsRecording())
	{
		TArray<FString, TInlineAllocator<1>> RecordingArgs;

		if (RecordingMode == EChaosVDRecordingMode::Live)
		{
			RecordingArgs.Emplace(TEXT("Server"));

			FChaosVDRuntimeModule::Get().StartRecording(RecordingArgs);

			// Only attempt to connect if we managed to start a recording.
			// If we failed, the runtime module takes care of showing a pop-up error in the editor already
			if (FChaosVDRuntimeModule::Get().IsRecording())
			{
				AttemptToConnectToLiveSession();
			}	
		}
		else
		{
			FChaosVDRuntimeModule::Get().StartRecording(RecordingArgs);
		}
	}
	else
	{
		FChaosVDRuntimeModule::Get().StopRecording();
	}

	return FReply::Handled();
}

bool SChaosVDRecordingControls::IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const
{
	if (bAutoConnectionAttemptInProgress)
	{
		return false;
	}

	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!MainTabSharedPtr.IsValid())
	{
		return false;

	}
	const bool bIsLiveSession = MainTabSharedPtr->GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().bIsLiveSession;

	const bool bIsRecording = IsRecording();

	if (RecordingMode == EChaosVDRecordingMode::File)
	{
		return (bIsRecording && !bIsLiveSession) || !bIsRecording;
	}
	else if (RecordingMode == EChaosVDRecordingMode::Live && GEditor)
	{
		return (bIsRecording && bIsLiveSession) || (!bIsRecording && GEditor->IsPlayingSessionInEditor());
	}

	return false;
}

EVisibility SChaosVDRecordingControls::IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const
{
	// If we are recording, don't show the stop button for the mode that is disabled
	const bool bIsRecording = IsRecording();
	const bool bShouldButtonBeVisible = bIsRecording ? bIsRecording && IsRecordingToggleButtonEnabled(RecordingMode) : true;
	return bShouldButtonBeVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

void SChaosVDRecordingControls::RegisterMenus()
{
	const UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(RecordingControlsToolbarName))
	{
		return;
	}

	UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(RecordingControlsToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);

	FToolMenuSection& Section = ToolBar->AddSection("LoadRecording");
	Section.AddDynamicEntry("OpenFile", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UChaosVDRecordingToolbarMenuContext* Context = InSection.FindContext<UChaosVDRecordingToolbarMenuContext>();
		TSharedRef<SChaosVDRecordingControls> RecordingControls = Context->RecordingControlsWidget.Pin().ToSharedRef();

		TSharedRef<SWidget> RecordToFileButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateToggleRecordingStateButton(EChaosVDRecordingMode::File, LOCTEXT("RecordToFileButtonDesc", "Starts a recording for the current session, saving it directly to file")) ];
		TSharedRef<SWidget> RecordToLiveButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateToggleRecordingStateButton(EChaosVDRecordingMode::Live, LOCTEXT("RecordLiveButtonDesc", "Starts a recording and automatically connects to it playing it back in real time")) ];
		TSharedRef<SWidget> RecordingTime = RecordingControls->GenerateRecordingTimeTextBlock();
		TSharedRef<SWidget> DataChannelsButton = SNew(SBox).Padding(4.0f,0.0f)[ RecordingControls->GenerateDataChannelsButton() ];

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordToFileButton",
				RecordToFileButton,
				FText::GetEmpty(),
				true,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordToLiveButton",
				RecordToLiveButton,
				FText::GetEmpty(),
				false,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"RecordingTime",
				RecordingTime,
				FText::GetEmpty(),
				false,
				false
			));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"DataChannelsButton",
				DataChannelsButton,
				FText::GetEmpty(),
				false,
				false
			));
	}));
}


bool SChaosVDRecordingControls::IsRecording() const
{
#if WITH_CHAOS_VISUAL_DEBUGGER
	return FChaosVisualDebuggerTrace::IsTracing();
#else
	return false;
#endif
}

FText SChaosVDRecordingControls::GetRecordingTimeText() const
{
	FNumberFormattingOptions FormatOptions;
	FormatOptions.MinimumFractionalDigits = 2;
	FormatOptions.MaximumFractionalDigits = 2;
	FText SecondsText = FText::AsNumber(FChaosVDRuntimeModule::Get().GetAccumulatedRecordingTime(), &FormatOptions);
	
	return FText::Format(LOCTEXT("RecordingTimer","{0} s"), SecondsText);
}

void SChaosVDRecordingControls::PushConnectionAttemptNotification()
{
	FNotificationInfo Info(LOCTEXT("ConnectingToLiceSessionMessge", "Connecting to Live Session ..."));
	Info.bFireAndForget = false;
	Info.FadeOutDuration = 3.0f;
	Info.ExpireDuration = 0.0f;

	ConnectionAttemptNotification = FSlateNotificationManager::Get().AddNotification(Info);
	
	if (ConnectionAttemptNotification.IsValid())
	{
		ConnectionAttemptNotification->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void SChaosVDRecordingControls::UpdateConnectionAttemptNotification()
{
	if (ConnectionAttemptNotification.IsValid())
	{
		ConnectionAttemptNotification->SetSubText(FText::FormatOrdered(LOCTEXT("SessionConnectionAttemptSubText", "Attempt {0} / {1}"), CurrentConnectionAttempts, MaxAutoplayConnectionAttempts));
	}
}

void SChaosVDRecordingControls::HandleConnectionAttemptResult(EChaosVDLiveConnectionAttemptResult Result)
{
	CurrentConnectionAttempts = 0;
	bAutoConnectionAttemptInProgress = false;

	if (ConnectionAttemptNotification.IsValid())
	{
		if (Result == EChaosVDLiveConnectionAttemptResult::Success)
		{
			ConnectionAttemptNotification->SetText(LOCTEXT("SessionConnectionSuccess", "Connected!"));
			ConnectionAttemptNotification->SetSubText(FText::GetEmpty());
			ConnectionAttemptNotification->SetCompletionState(SNotificationItem::ECompletionState::CS_Success);
		}
		else
		{
			ConnectionAttemptNotification->SetText(LOCTEXT("SessionConnectionFailedText", "Failed to connect"));
			ConnectionAttemptNotification->SetSubText(LOCTEXT("SessionConnectionFailedSubText", "See the logs for more details..."));
			ConnectionAttemptNotification->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
		}

		ConnectionAttemptNotification->ExpireAndFadeout();
	}
}

#undef LOCTEXT_NAMESPACE 
