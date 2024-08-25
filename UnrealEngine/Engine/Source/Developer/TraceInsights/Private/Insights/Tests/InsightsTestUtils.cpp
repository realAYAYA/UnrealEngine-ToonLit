// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/Tests/InsightsTestUtils.h"

#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManagerGeneric.h"

#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/ModuleService.h"
#include "Trace/StoreClient.h"
#include "Insights/Common/Stopwatch.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/InsightsManager.h"

#include "Misc/AutomationTest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

FInsightsTestUtils::FInsightsTestUtils(FAutomationTestBase* InTest) :
	Test(InTest)
{
#if WITH_EDITOR
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TSharedPtr<TraceServices::IModuleService> ModuleService = TraceServicesModule.GetModuleService();
	ModuleService->SetModuleEnabled(FName("TraceModule_LoadTimeProfiler"), true);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::AnalyzeTrace(const TCHAR* Path) const
{
	IUnrealInsightsModule& TraceInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");

	if (!FPaths::FileExists(Path))
	{
		Test->AddError(FString::Printf(TEXT("File does not exist: %s."), Path));
		return false;
	}

	TraceInsightsModule.StartAnalysisForTraceFile(Path);
	auto Session = TraceInsightsModule.GetAnalysisSession();
	if (Session == nullptr)
	{
		Test->AddError(TEXT("Session analysis failed to start."));
		return false;
	}

	FStopwatch StopWatch;
	StopWatch.Start();

	double Duration = 0.0f;
	constexpr double MaxDuration = 75.0f;
	while (!Session->IsAnalysisComplete())
	{
		FPlatformProcess::Sleep(0.033f);

		if (Duration > MaxDuration)
		{
			Test->AddError(FString::Format(TEXT("Session analysis took longer than the maximum allowed time of {0} seconds. Aborting test."), { MaxDuration }));
			return false;
		}

		StopWatch.Update();
		Duration = StopWatch.GetAccumulatedTime();
	}

	StopWatch.Stop();
	Duration = StopWatch.GetAccumulatedTime();

	Test->AddInfo(FString::Format(TEXT("Session analysis took {0} seconds."), { Duration }));

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::FileContainsString(const FString& PathToFile, const FString& ExpectedString, double Timeout) const
{
	double StartTime = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() - StartTime < Timeout)
	{
		if (!FPaths::FileExists(PathToFile))
		{
			FPlatformProcess::Sleep(0.1f);
		}
		else
		{
			FString LogFileContents;
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*PathToFile, true)); // Open the file with shared read access
			if (FileHandle)
			{
				TArray<uint8> FileData;
				FileData.SetNumUninitialized(static_cast<int32>(FileHandle->Size()));
				FileHandle->Read(FileData.GetData(), FileData.Num());
				FFileHelper::BufferToString(LogFileContents, FileData.GetData(), FileData.Num());

				if (LogFileContents.Contains(ExpectedString))
				{
					return true;
				}
			}
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::IsUnrealTraceServerReady(const TCHAR* Host, int32 Port) const
{
	UE::Trace::FStoreClient* StoreClient = UE::Trace::FStoreClient::Connect(Host, Port);
	if (!StoreClient)
	{
		Test->AddInfo(TEXT("Cannot connect to UTS. Trying again"));
		return false;
	}
	const UE::Trace::FStoreClient::FVersion* Version = StoreClient->GetVersion();
	if (Version == nullptr)
	{
		Test->AddError(TEXT("Cannot get version of UTS"));
		delete StoreClient;
		return false;
	}
	uint32 MajorVersion = Version->GetMajorVersion();
	uint32 MinorVersion = Version->GetMinorVersion();
	delete StoreClient;
	Test->AddInfo(FString::Printf(TEXT("Connected to UTS version %u.%u"), MajorVersion, MinorVersion));
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::StartTracing(FTraceAuxiliary::EConnectionType ConnectionType, double Timeout) const
{
	bool bStarted = false;

	double TraceVerifyStartTime = FPlatformTime::Seconds();
	while(FPlatformTime::Seconds() - TraceVerifyStartTime < Timeout)
	{
		if (ConnectionType == FTraceAuxiliary::EConnectionType::Network)
		{
			bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("localhost"), nullptr);
		}
		else if (ConnectionType == FTraceAuxiliary::EConnectionType::File)
		{
			bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, nullptr, nullptr);
		}

		if (FTraceAuxiliary::IsConnected())
		{
			FPlatformProcess::Sleep(0.5f);
			return bStarted;
		}
		FPlatformProcess::Sleep(0.1f);
	}

	return bStarted;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::SetupUTS(double Timeout, bool bUseFork) const
{
	const FString UnrealTraceServerName = TEXT("UnrealTraceServer");

	if (FPlatformProcess::IsApplicationRunning(*UnrealTraceServerName))
	{
		Test->AddInfo(TEXT("UTS is already running"));
		return true;
	}

	FString UTSPath = FPlatformProcess::GenerateApplicationPath("UnrealTraceServer", EBuildConfiguration::Development);
	if (!FPaths::FileExists(UTSPath))
	{
		Test->AddError(FString::Printf(TEXT("UTS executable can't be found at '%s'"), *UTSPath));
		return false;
	}

	FString UTSParameters;
	if (bUseFork)
	{
		UTSParameters = TEXT("fork");
	}
	else
	{
		UTSParameters = TEXT("daemon");
	}
	UTSParameters += FString::Printf(TEXT(" --sponsor %d"), FPlatformProcess::GetCurrentProcessId());
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;
	uint32 ProcessID = 0;
	const int32 PriorityModifier = 0;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	void* PipeOutput = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeOutput, PipeWriteChild));
	FProcHandle UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, nullptr);
	if (!UTSHandle.IsValid())
	{
		Test->AddError(TEXT("The UTSHandle should be valid"));
		return false;
	}

	double StartTime = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() - StartTime < Timeout && FPlatformProcess::IsProcRunning(UTSHandle))
	{
		FPlatformProcess::Sleep(0.1f);
		if (!FPlatformProcess::IsApplicationRunning(*UnrealTraceServerName))
		{
			Test->AddInfo(TEXT("UTS not started yet"));
			continue;
		}
		if (!IsUnrealTraceServerReady())
		{
			Test->AddInfo(TEXT("UTS not ready yet"));
			continue;
		}
		Test->AddInfo(TEXT("UTS is ready"));
		return true;
	}

	Test->AddError(TEXT("UTS failed to start"));
	if (!FPlatformProcess::IsProcRunning(UTSHandle))
	{
		FString StringOutput = FPlatformProcess::ReadPipe(PipeOutput);
		int32 ExitCode = 0;
		FPlatformProcess::GetProcReturnCode(UTSHandle, &ExitCode);
		Test->AddError(FString::Printf(TEXT("UTS exitcode=%d stdout:\n%s"), ExitCode, *StringOutput));
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::KillUTS(double Timeout) const
{
	const FString UnrealTraceServerName = TEXT("UnrealTraceServer");

	FString UTSPath = FPlatformProcess::GenerateApplicationPath("UnrealTraceServer", EBuildConfiguration::Development);
	FString UTSParameters = TEXT("kill");
	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;
	uint32 ProcessID = 0;
	const int32 PriorityModifier = 0;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	void* PipeWriteChild = nullptr;
	void* PipeReadChild = nullptr;
	FProcHandle UTSHandle = FPlatformProcess::CreateProc(*UTSPath, *UTSParameters, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild);
	if (!UTSHandle.IsValid())
	{
		Test->AddError(TEXT("The UTSHandle should be valid"));
		return false;
	}

	double StartTime = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() - StartTime < Timeout)
	{
		FPlatformProcess::Sleep(0.1f);
		if (!FPlatformProcess::IsApplicationRunning(*UnrealTraceServerName))
		{
			Test->AddInfo(TEXT("The UTS successfully killed"));
			return true;
		}
	}

	Test->AddError(TEXT("UTS failed to kill"));
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FInsightsTestUtils::ResetSession() const
{
	TSharedPtr<FInsightsManager> InsightsManager = FInsightsManager::Get();
	InsightsManager->ResetSession();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FInsightsTestUtils::IsTraceHasLiveStatus(const FString& TraceName, const TCHAR* Host, int32 Port) const
{
	UE::Trace::FStoreClient* StoreClient = UE::Trace::FStoreClient::Connect(Host, Port);
	if (!StoreClient)
	{
		Test->AddInfo(TEXT("The StoreClient shouldn't be null"));
		return false;
	}
	uint32 SessionCount = StoreClient->GetSessionCount();
	if (!SessionCount)
	{
		Test->AddInfo(TEXT("The SessionCount shouldn't be 0"));
		delete StoreClient;
		return false;
	}

	for (uint32 Index = 0; Index < SessionCount; ++Index)
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfo(Index);
		if (!SessionInfo)
		{
			continue;
		}
		uint32 TraceId = SessionInfo->GetTraceId();
		const UE::Trace::FStoreClient::FTraceInfo* Info = StoreClient->GetTraceInfoById(TraceId);
		if (!Info)
		{
			continue;
		}
		const FUtf8StringView Utf8TraceNameView = Info->GetName();
		FString ActualTraceName(Utf8TraceNameView);
		if (TraceName.Contains(ActualTraceName))
		{
			Test->AddInfo(TEXT("Trace is live"));
			delete StoreClient;
			return true;
		}
		Test->AddInfo(FString::Printf(TEXT("The live trace is %s"), *ActualTraceName));
	}

	Test->AddInfo(FString::Printf(TEXT("The trace with name %s does not have live status. Trying to find live trace"), *TraceName));
	delete StoreClient;
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FInsightsTestUtils::GetLiveTrace(const TCHAR* Host, int32 Port) const
{
	UE::Trace::FStoreClient* StoreClient = UE::Trace::FStoreClient::Connect(Host, Port);
	if (!StoreClient)
	{
		Test->AddInfo(TEXT("The StoreClient shouldn't be null"));
		return TEXT("");
	}
	uint32 SessionCount = StoreClient->GetSessionCount();
	if (!SessionCount)
	{
		Test->AddInfo(TEXT("The SessionCount shouldn't be 0"));
		delete StoreClient;
		return TEXT("");
	}

	for (uint32 Index = 0; Index < SessionCount; ++Index)
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfo(Index);
		if (!SessionInfo)
		{
			continue;
		}

		uint32 TraceId = SessionInfo->GetTraceId();
		const UE::Trace::FStoreClient::FTraceInfo* Info = StoreClient->GetTraceInfoById(TraceId);
		FString LiveTraceName = static_cast<FString>(Info->GetName());
		LiveTraceName = LiveTraceName + TEXT(".utrace");
		delete StoreClient;
		return LiveTraceName;
	}

	Test->AddInfo(TEXT("There isn't any live trace"));
	delete StoreClient;
	return TEXT("");
}