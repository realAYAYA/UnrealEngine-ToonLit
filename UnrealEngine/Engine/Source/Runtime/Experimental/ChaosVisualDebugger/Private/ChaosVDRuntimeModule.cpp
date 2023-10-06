// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDRuntimeModule.h"

#include "Containers/Array.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

IMPLEMENT_MODULE(FChaosVDRuntimeModule, ChaosVDRuntime);

DEFINE_LOG_CATEGORY_STATIC( LogChaosVDRuntime, Log, All );

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
FChaosVDCaptureRequestDelegate FChaosVDRuntimeModule::PerformFullCaptureDelegate = FChaosVDCaptureRequestDelegate();
FRWLock FChaosVDRuntimeModule::DelegatesRWLock = FRWLock();

FChaosVDRuntimeModule& FChaosVDRuntimeModule::Get()
{
	return FModuleManager::Get().LoadModuleChecked<FChaosVDRuntimeModule>(TEXT("ChaosVDRuntime"));
}

void FChaosVDRuntimeModule::StartupModule()
{
#if UE_TRACE_ENABLED
	// Make sure it is off until we support auto trace
	UE::Trace::ToggleChannel(TEXT("ChaosVDChannel"), false);
#endif

	FTraceAuxiliary::OnTraceStopped.AddRaw(this, &FChaosVDRuntimeModule::HandleTraceStopRequest);
}

void FChaosVDRuntimeModule::ShutdownModule()
{
	FTraceAuxiliary::OnTraceStopped.RemoveAll(this);
}

int32 FChaosVDRuntimeModule::GenerateUniqueID()
{
	return LastGeneratedID.Increment();
}

void FChaosVDRuntimeModule::StopTrace()
{
	bRequestedStop = true;
	FTraceAuxiliary::Stop();
}

void FChaosVDRuntimeModule::GenerateRecordingFileName(FString& OutFileName)
{
	const TCHAR* FilePrefix = TEXT("ChaosVD");
	const FString FullPathPrefix = FPaths::ProfilingDir() / FilePrefix;

	int32 Tries = 0;
	do
	{
		OutFileName = FString::Printf(TEXT("%s_%d.utrace"), *FullPathPrefix, Tries++);
	} while (IFileManager::Get().FileExists(*OutFileName));
}

bool FChaosVDRuntimeModule::RequestFullCapture(float DeltaTime)
{
	// Full capture intervals are clamped to be no lower than 1 sec
	FReadScopeLock ReadLock (DelegatesRWLock);
	PerformFullCaptureDelegate.Broadcast(EChaosVDFullCaptureFlags::Particles);
	return true;
}

void FChaosVDRuntimeModule::StartRecording(const TArray<FString>& Args)
{
	if (bIsRecording)
	{
		return;
	}

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

#if UE_TRACE_ENABLED

	// Other tools could bee using trace
	// This is aggressive but until Trace supports multi-sessions, just take over.
	if (FTraceAuxiliary::IsConnected())
	{
		StopTrace();
	}

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

	if (Args.Num() == 0 || Args[0] == TEXT("File"))
	{
		FString RecordingFileName;
		GenerateRecordingFileName(RecordingFileName);

		bIsRecording = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *RecordingFileName);
	}
	else if(Args[0] == TEXT("Server"))
	{
		const FString Target = Args.IsValidIndex(1) ? Args[1] : TEXT("127.0.0.1");

		FTraceAuxiliary::Start(
		FTraceAuxiliary::EConnectionType::Network,
		*Target,
		nullptr);
	}
#endif

	ensure(bIsRecording);
}

void FChaosVDRuntimeModule::StopRecording()
{
	if (!ensure(bIsRecording))
	{
		return;
	}

#if UE_TRACE_ENABLED
	

	UE::Trace::ToggleChannel(TEXT("ChaosVDChannel"), false);
	UE::Trace::ToggleChannel(TEXT("Frame"), false); 

	StopTrace();
#endif

	if (FullCaptureRequesterHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FullCaptureRequesterHandle);
	}

	{
		FReadScopeLock ReadLock(DelegatesRWLock);
		RecordingStopDelegate.Broadcast();
	}

	bIsRecording = false;
}

void FChaosVDRuntimeModule::HandleTraceStopRequest(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
{
	if (bIsRecording)
	{
		if (!ensure(bRequestedStop))
		{
			UE_LOG(LogChaosVDRuntime, Warning, TEXT("Trace Recording has been stopped unexpectedly"));
		}
	}

	bRequestedStop = false;
}
