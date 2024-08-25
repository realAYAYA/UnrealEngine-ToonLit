// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDRuntimeModule.h"

#include "Containers/Array.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

IMPLEMENT_MODULE(FChaosVDRuntimeModule, ChaosVDRuntime);

DEFINE_LOG_CATEGORY_STATIC( LogChaosVDRuntime, Log, All );

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FAutoConsoleCommand ChaosVDStartRecordingCommand(
	TEXT("p.Chaos.StartVDRecording"),
	TEXT("Turn on the recording of debugging data"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FChaosVDRuntimeModule::Get().StartRecording(Args);
	})
);

FAutoConsoleCommand StopVDStartRecordingCommand(
	TEXT("p.Chaos.StopVDRecording"),
	TEXT("Turn off the recording of debugging data"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		FChaosVDRuntimeModule::Get().StopRecording();
	})
);

static FAutoConsoleVariable CVarChaosVDGTimeBetweenFullCaptures(
	TEXT("p.Chaos.VD.TimeBetweenFullCaptures"),
	10,
	TEXT("Time interval in seconds after which a full capture (not only delta changes) should be recorded"));

FChaosVDRecordingStateChangedDelegate FChaosVDRuntimeModule::RecordingStartedDelegate = FChaosVDRecordingStateChangedDelegate();
FChaosVDRecordingStateChangedDelegate FChaosVDRuntimeModule::RecordingStopDelegate = FChaosVDRecordingStateChangedDelegate();
FChaosVDRecordingStartFailedDelegate FChaosVDRuntimeModule::RecordingStartFailedDelegate = FChaosVDRecordingStartFailedDelegate();
FChaosVDCaptureRequestDelegate FChaosVDRuntimeModule::PerformFullCaptureDelegate = FChaosVDCaptureRequestDelegate();
FRWLock FChaosVDRuntimeModule::DelegatesRWLock = FRWLock();

FChaosVDRuntimeModule& FChaosVDRuntimeModule::Get()
{
	return FModuleManager::Get().LoadModuleChecked<FChaosVDRuntimeModule>(TEXT("ChaosVDRuntime"));
}

bool FChaosVDRuntimeModule::IsLoaded()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("ChaosVDRuntime"));
}

void FChaosVDRuntimeModule::StartupModule()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("StartCVDRecording")))
	{
		TArray<FString, TInlineAllocator<1>> CVDOptions;

		{
			FString CVDHostAddress;
			if (FParse::Value(FCommandLine::Get(), TEXT("CVDHost="), CVDHostAddress))
			{
				CVDOptions.Emplace(MoveTemp(CVDHostAddress));
			}
		}
        
        StartRecording(CVDOptions);
	}
	else
	{
		
#if UE_TRACE_ENABLED
		UE::Trace::ToggleChannel(TEXT("ChaosVDChannel"), false);
#endif

	}
}

void FChaosVDRuntimeModule::ShutdownModule()
{
	if (bIsRecording)
	{
		StopRecording();
	}

	FTraceAuxiliary::OnTraceStopped.RemoveAll(this);
}

int32 FChaosVDRuntimeModule::GenerateUniqueID()
{
	return LastGeneratedID.Increment();
}

FString FChaosVDRuntimeModule::GetLastRecordingFileNamePath() const
{
	return LastRecordingFileNamePath;
}

void FChaosVDRuntimeModule::StopTrace()
{
	bRequestedStop = true;
	FTraceAuxiliary::Stop();
}

void FChaosVDRuntimeModule::GenerateRecordingFileName(FString& OutFileName)
{
	OutFileName = TEXT("ChaosVD-") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S.utrace"));
}

bool FChaosVDRuntimeModule::RequestFullCapture(float DeltaTime)
{
	// Full capture intervals are clamped to be no lower than 1 sec
	FReadScopeLock ReadLock (DelegatesRWLock);
	PerformFullCaptureDelegate.Broadcast(EChaosVDFullCaptureFlags::Particles);
	return true;
}

bool FChaosVDRuntimeModule::RecordingTimerTick(float DeltaTime)
{
	if (bIsRecording)
	{
		AccumulatedRecordingTime += DeltaTime;
	}
	
	return true;
}

void FChaosVDRuntimeModule::StartRecording(TConstArrayView<FString> Args)
{
	if (bIsRecording)
	{
		return;
	}

	// Start Listening for Trace Stopped events, in case Trace is stopped outside our control so we can gracefully stop CVD recording and log a warning 
	FTraceAuxiliary::OnTraceStopped.AddRaw(this, &FChaosVDRuntimeModule::HandleTraceStopRequest);

	// Start with a generic Failure reason
	FText FailureReason = LOCTEXT("SeeLogsForErrorDetailsText","Please see the logs for more details...");

#if UE_TRACE_ENABLED

	// Other tools could bee using trace
	// This is aggressive but until Trace supports multi-sessions, just take over.
	if (FTraceAuxiliary::IsConnected())
	{
		StopTrace();
	}

	// Until we support allowing other channels, indicate in the logs that we are disabling everything else
	UE_LOG(LogChaosVDRuntime, Log, TEXT("[%s] Disabling additional trace channels..."), ANSI_TO_TCHAR(__FUNCTION__));

	// Disable any enabled additional channel
	UE::Trace::EnumerateChannels([](const ANSICHAR* ChannelName, bool bEnabled, void*)
		{
			if (bEnabled)
			{
				FString ChannelNameFString(ChannelName);
				UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), false);
			}
		}
		, nullptr);


	UE::Trace::ToggleChannel(TEXT("ChaosVDChannel"), true); 
	UE::Trace::ToggleChannel(TEXT("Frame"), true);

	FTraceAuxiliary::FOptions TracingOptions;
	TracingOptions.bExcludeTail = true;

	if (Args.Num() == 0 || Args[0] == TEXT("File"))
	{
		LastRecordingFileNamePath.Empty();
		GenerateRecordingFileName(LastRecordingFileNamePath);

		UE_LOG(LogChaosVDRuntime, Log, TEXT("[%s] Generated trace file name [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *LastRecordingFileNamePath);

		bIsRecording = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *LastRecordingFileNamePath, nullptr, &TracingOptions);

		LastRecordingFileNamePath = bIsRecording ? FTraceAuxiliary::GetTraceDestinationString() : TEXT("");
	}
	else if(Args[0] == TEXT("Server"))
	{
		if (FTraceAuxiliary::IsConnected())
		{
			FTraceAuxiliary::Stop();
		}

		const FString Target = Args.IsValidIndex(1) ? Args[1] : TEXT("127.0.0.1");

		bIsRecording = FTraceAuxiliary::Start(
		FTraceAuxiliary::EConnectionType::Network,
		*Target,
		nullptr, &TracingOptions);
	}
	else
	{
		FailureReason = LOCTEXT("WrongCommandArgumentsError", "The start recording command was called with invalid arguments");
	}
#endif
	
	AccumulatedRecordingTime = 0.0f;

	if (ensure(bIsRecording))
	{
		{
			FReadScopeLock ReadLock(DelegatesRWLock);
			RecordingStartedDelegate.Broadcast();
		}
		
		constexpr int32 MinAllowedTimeInSecondsBetweenCaptures = 1;
		int32 ConfiguredTimeBetweenCaptures = CVarChaosVDGTimeBetweenFullCaptures->GetInt();

		ensureAlwaysMsgf(ConfiguredTimeBetweenCaptures > MinAllowedTimeInSecondsBetweenCaptures,
			TEXT("The minimum allowed time interval between full captures is [%d] seconds, but [%d] seconds were configured. Clamping to [%d] seconds"),
			MinAllowedTimeInSecondsBetweenCaptures, ConfiguredTimeBetweenCaptures, MinAllowedTimeInSecondsBetweenCaptures);

		FullCaptureRequesterHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FChaosVDRuntimeModule::RequestFullCapture),
			FMath::Clamp(ConfiguredTimeBetweenCaptures, MinAllowedTimeInSecondsBetweenCaptures, TNumericLimits<int32>::Max()));

		RecordingTimerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FChaosVDRuntimeModule::RecordingTimerTick));
	}
	else
	{
		UE_LOG(LogChaosVDRuntime, Error, TEXT("[%s] Failed to start CVD recording | Reason: [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *FailureReason.ToString());

#if WITH_EDITOR
		FMessageDialog::Open(EAppMsgType::Ok, FText::FormatOrdered(LOCTEXT("StartRecordingFailedMessage", "Failed to start CVD recording. \n\n{0}"), FailureReason));
#endif

		{
			FReadScopeLock ReadLock(DelegatesRWLock);
			RecordingStartFailedDelegate.Broadcast(FailureReason);
		}	
	}

}

void FChaosVDRuntimeModule::StopRecording()
{
	if (!bIsRecording)
	{
		UE_LOG(LogChaosVDRuntime, Warning, TEXT("[%s] Attempted to stop recorded when there is no CVD recording active."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
	
	FTraceAuxiliary::OnTraceStopped.RemoveAll(this);

#if UE_TRACE_ENABLED

	UE::Trace::ToggleChannel(TEXT("ChaosVDChannel"), false);
	UE::Trace::ToggleChannel(TEXT("Frame"), false); 

	StopTrace();
#endif

	if (FullCaptureRequesterHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FullCaptureRequesterHandle);

		FullCaptureRequesterHandle.Reset();
	}
	
	if (RecordingTimerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RecordingTimerHandle);
		RecordingTimerHandle.Reset();
	}

	{
		FReadScopeLock ReadLock(DelegatesRWLock);
		RecordingStopDelegate.Broadcast();
	}

	bIsRecording = false;
	AccumulatedRecordingTime = 0.0f;
}

void FChaosVDRuntimeModule::HandleTraceStopRequest(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
{
	if (bIsRecording)
	{
		if (!ensure(bRequestedStop))
		{
			UE_LOG(LogChaosVDRuntime, Warning, TEXT("Trace Recording has been stopped unexpectedly"));

#if WITH_EDITOR
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnexpectedStopMessage", "Trace recording has been stopped unexpectedly. CVD cannot continue with the recording session... "));
#endif
		}

		StopRecording();
	}

	bRequestedStop = false;
}

#undef LOCTEXT_NAMESPACE 
