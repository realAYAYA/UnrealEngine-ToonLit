// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutomationWorkerModule.h"

#include "AutomationAnalytics.h"
#include "AutomationWorkerMessages.h"
#include "AutomationTestExcludelist.h"
#include "HAL/FileManager.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"

#if WITH_ENGINE
	#include "Engine/Engine.h"
	#include "Engine/GameViewportClient.h"
	#include "ImageUtils.h"
	#include "Tests/AutomationCommon.h"
	#include "UnrealClient.h"
#endif

#if WITH_EDITOR
	#include "AssetRegistry/AssetRegistryModule.h"
#endif


#define LOCTEXT_NAMESPACE "AutomationTest"

DEFINE_LOG_CATEGORY_STATIC(LogAutomationWorker, Log, All);

IMPLEMENT_MODULE(FAutomationWorkerModule, AutomationWorker);


/* IModuleInterface interface
 *****************************************************************************/

void FAutomationWorkerModule::StartupModule()
{
	Initialize();

	FAutomationTestFramework::Get().PreTestingEvent.AddRaw(this, &FAutomationWorkerModule::HandlePreTestingEvent);
	FAutomationTestFramework::Get().PostTestingEvent.AddRaw(this, &FAutomationWorkerModule::HandlePostTestingEvent);
}

void FAutomationWorkerModule::ShutdownModule()
{
	FAutomationTestFramework::Get().PreTestingEvent.RemoveAll(this);
	FAutomationTestFramework::Get().PostTestingEvent.RemoveAll(this);
}

bool FAutomationWorkerModule::SupportsDynamicReloading()
{
	return true;
}


/* IAutomationWorkerModule interface
 *****************************************************************************/

void FAutomationWorkerModule::Tick()
{
	//execute latent commands from the previous frame.  Gives the rest of the engine a turn to tick before closing the test
	bool bAllLatentCommandsComplete  = ExecuteLatentCommands();
	if (bAllLatentCommandsComplete)
	{
		//if we were running the latent commands as a result of executing a network command, report that we are now done
		if (bExecutingNetworkCommandResults)
		{
			ReportNetworkCommandComplete();
			bExecutingNetworkCommandResults = false;
		}

		//if the controller has requested the next network command be executed
		if (bExecuteNextNetworkCommand)
		{
			//execute network commands if there are any queued up and our role is appropriate
			bool bAllNetworkCommandsComplete = ExecuteNetworkCommands();
			if (bAllNetworkCommandsComplete)
			{
				ReportTestComplete();
			}

			//we've now executed a network command which may have enqueued further latent actions
			bExecutingNetworkCommandResults = true;

			//do not execute anything else until expressly told to by the controller
			bExecuteNextNetworkCommand = false;
		}
	}

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->ProcessInbox();
	}
}


/* ISessionManager implementation
 *****************************************************************************/

bool FAutomationWorkerModule::ExecuteLatentCommands()
{
	bool bAllLatentCommandsComplete = false;
	
	if (GIsAutomationTesting)
	{
		// Ensure that latent automation commands have time to execute
		bAllLatentCommandsComplete = FAutomationTestFramework::Get().ExecuteLatentCommands();
	}
	
	return bAllLatentCommandsComplete;
}


bool FAutomationWorkerModule::ExecuteNetworkCommands()
{
	bool bAllLatentCommandsComplete = false;
	
	if (GIsAutomationTesting)
	{
		// Ensure that latent automation commands have time to execute
		bAllLatentCommandsComplete = FAutomationTestFramework::Get().ExecuteNetworkCommands();
	}

	return bAllLatentCommandsComplete;
}


void FAutomationWorkerModule::Initialize()
{
	if (FPlatformProcess::SupportsMultithreading())
	{
		// initialize messaging
		MessageEndpoint = FMessageEndpoint::Builder("FAutomationWorkerModule")
			.Handling<FAutomationWorkerFindWorkers>(this, &FAutomationWorkerModule::HandleFindWorkersMessage)
			.Handling<FAutomationWorkerNextNetworkCommandReply>(this, &FAutomationWorkerModule::HandleNextNetworkCommandReplyMessage)
			.Handling<FAutomationWorkerPing>(this, &FAutomationWorkerModule::HandlePingMessage)
			.Handling<FAutomationWorkerResetTests>(this, &FAutomationWorkerModule::HandleResetTests)
			.Handling<FAutomationWorkerRequestTests>(this, &FAutomationWorkerModule::HandleRequestTestsMessage)
			.Handling<FAutomationWorkerRunTests>(this, &FAutomationWorkerModule::HandleRunTestsMessage)
			.Handling<FAutomationWorkerImageComparisonResults>(this, &FAutomationWorkerModule::HandleScreenShotCompared)
			.Handling<FAutomationWorkerTestDataResponse>(this, &FAutomationWorkerModule::HandleTestDataRetrieved)
			.Handling<FAutomationWorkerStopTests>(this, &FAutomationWorkerModule::HandleStopTestsMessage)
			.WithInbox();

		if (MessageEndpoint.IsValid())
		{
			MessageEndpoint->Subscribe<FAutomationWorkerFindWorkers>();
		}

		bExecuteNextNetworkCommand = true;		
	}
	else
	{
		bExecuteNextNetworkCommand = false;
	}
	ExecutionCount = INDEX_NONE;
	bExecutingNetworkCommandResults = false;
	bSendAnalytics = false;

	FParse::Value(FCommandLine::Get(), TEXT("-DeviceTag="), DeviceTag);
}

void FAutomationWorkerModule::ReportNetworkCommandComplete()
{
	if (GIsAutomationTesting)
	{
		MessageEndpoint->Send(FMessageEndpoint::MakeMessage<FAutomationWorkerRequestNextNetworkCommand>(ExecutionCount), TestRequesterAddress);
		if (StopTestEvent.IsBound())
		{
			// this is a local test; the message to continue will never arrive, so lets not wait for it
			bExecuteNextNetworkCommand = true;
		}
	}
}

void FAutomationWorkerModule::ReportTestComplete()
{
	if (GIsAutomationTesting)
	{
		//see if there are any more network commands left to execute
		bool bAllLatentCommandsComplete = FAutomationTestFramework::Get().ExecuteLatentCommands();

		FString TestFullName = FAutomationTestFramework::Get().GetCurrentTest()->GetTestFullName();

		//structure to track error/warning/log messages
		FAutomationTestExecutionInfo ExecutionInfo;

		bool bSuccess = FAutomationTestFramework::Get().StopTest(ExecutionInfo);

		if (StopTestEvent.IsBound())
		{
			StopTestEvent.Execute(bSuccess, TestName, ExecutionInfo);
		}
		else
		{
			// send the results to the controller
			FAutomationWorkerRunTestsReply* Message = FMessageEndpoint::MakeMessage<FAutomationWorkerRunTestsReply>();

			Message->TestName = TestName;
			Message->ExecutionCount = ExecutionCount;
			Message->State = bSuccess ? EAutomationState::Success : EAutomationState::Fail;
			Message->Duration = ExecutionInfo.Duration;
			Message->Entries = ExecutionInfo.GetEntries();
			Message->WarningTotal = ExecutionInfo.GetWarningTotal();
			Message->ErrorTotal = ExecutionInfo.GetErrorTotal();

			// sending though the endpoint will free Message memory, so analytics need to be sent first
			if (bSendAnalytics)
			{
				if (!FAutomationAnalytics::IsInitialized())
				{
					FAutomationAnalytics::Initialize();
				}
				FAutomationAnalytics::FireEvent_AutomationTestResults(Message, BeautifiedTestName);
				SendAnalyticsEvents(ExecutionInfo.AnalyticsItems);
			}

			if (ExecutionInfo.TelemetryItems.Num() > 0)
			{
				HandleTelemetryData(ExecutionInfo.TelemetryStorage, TestFullName, ExecutionInfo.TelemetryItems);
			}

			MessageEndpoint->Send(Message, TestRequesterAddress);
		}


		// reset local state
		TestRequesterAddress.Invalidate();
		ExecutionCount = INDEX_NONE;
		TestName.Empty();
		StopTestEvent.Unbind();
	}
}


void FAutomationWorkerModule::SendTests( const FMessageAddress& ControllerAddress )
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Worker"));
	FAutomationWorkerRequestTestsReplyComplete* Reply = FMessageEndpoint::MakeMessage<FAutomationWorkerRequestTestsReplyComplete>();
	for( int32 TestIndex = 0; TestIndex < TestInfo.Num(); TestIndex++ )
	{
		Reply->Tests.Emplace(FAutomationWorkerSingleTestReply(TestInfo[TestIndex]));
	}

	UE_LOG(LogAutomationWorker, Log, TEXT("Set %d tests to %s"), TestInfo.Num(), *ControllerAddress.ToString());

	MessageEndpoint->Send(Reply, ControllerAddress);
}


/* FAutomationWorkerModule callbacks
 *****************************************************************************/

void FAutomationWorkerModule::HandleFindWorkersMessage(const FAutomationWorkerFindWorkers& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received FindWorkersMessage from %s"), *Context->GetSender().ToString());

	// Set the Instance name to be the same as the session browser. This information should be shared at some point
	if ((Message.SessionId == FApp::GetSessionId()) && (Message.Changelist == 10000))
	{
#if WITH_EDITOR
		//If the asset registry is loading assets then we'll wait for it to stop before running our automation tests.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
			{
				AssetRegistryModule.Get().OnFilesLoaded().AddLambda([this, Context] {
						SendWorkerFound(Context->GetSender());
					}
				);
				GLog->Logf(ELogVerbosity::Log, TEXT("...Forcing Asset Registry Load For Automation"));
			}
		}
		else
#endif
		{
			//If the registry is not loading then we'll just go ahead and run our tests.
			SendWorkerFound(Context->GetSender());
		}
	}
}


void FAutomationWorkerModule::SendWorkerFound(const FMessageAddress& ControllerAddress)
{
	FAutomationWorkerFindWorkersResponse* Response = FMessageEndpoint::MakeMessage<FAutomationWorkerFindWorkersResponse>();

	FString OSMajorVersionString, OSSubVersionString;
	FPlatformMisc::GetOSVersions(OSMajorVersionString, OSSubVersionString);

	FString OSVersionString = OSMajorVersionString + TEXT(" ") + OSSubVersionString;
	FString CPUModelString = FPlatformMisc::GetCPUBrand().TrimStart();

	FString DeviceName = DeviceTag.IsEmpty() ? FPlatformProcess::ComputerName() : DeviceTag;
	FString DeviceId = FPlatformMisc::GetDeviceId().IsEmpty() ? DeviceName : FPlatformMisc::GetDeviceId();

	Response->DeviceName = DeviceName;
	Response->InstanceName = FString::Printf(TEXT("%s-%s"), *DeviceId, *FApp::GetSessionId().ToString());
	Response->Platform = FPlatformProperties::PlatformName();
	Response->SessionId = FApp::GetSessionId();
	Response->OSVersionName = OSVersionString;
	Response->ModelName = FPlatformMisc::GetDefaultDeviceProfileName();
	Response->GPUName = FPlatformMisc::GetPrimaryGPUBrand();
	Response->CPUModelName = CPUModelString;
	Response->RAMInGB = FPlatformMemory::GetPhysicalGBRam();
	Response->RHIName = FApp::GetGraphicsRHI();
#if WITH_ENGINE && WITH_AUTOMATION_TESTS
	Response->RenderModeName = AutomationCommon::GetRenderDetailsString();
#else
	Response->RenderModeName = TEXT("Unknown");
#endif

	MessageEndpoint->Send(Response, ControllerAddress);
}


void FAutomationWorkerModule::HandleNextNetworkCommandReplyMessage( const FAutomationWorkerNextNetworkCommandReply& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received NextNetworkCommandReplyMessage from %s"), *Context->GetSender().ToString());

	// Allow the next command to execute
	bExecuteNextNetworkCommand = true;

	// We should never be executing sub-commands of a network command when we're waiting for a cue for the next network command
	check(bExecutingNetworkCommandResults == false);
}


void FAutomationWorkerModule::HandlePingMessage( const FAutomationWorkerPing& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	MessageEndpoint->Send(FMessageEndpoint::MakeMessage<FAutomationWorkerPong>(), Context->GetSender());
}


void FAutomationWorkerModule::HandleResetTests( const FAutomationWorkerResetTests& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received ResetTests from %s"), *Context->GetSender().ToString());

	FAutomationTestFramework::Get().ResetTests();
}


void FAutomationWorkerModule::HandleRequestTestsMessage( const FAutomationWorkerRequestTests& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received RequestTestsMessage from %s"), *Context->GetSender().ToString());

	FAutomationTestFramework::Get().LoadTestModules();
	FAutomationTestFramework::Get().SetDeveloperDirectoryIncluded(Message.DeveloperDirectoryIncluded);
	FAutomationTestFramework::Get().SetRequestedTestFilter(Message.RequestedTestFlags);
	FAutomationTestFramework::Get().GetValidTestNames( TestInfo );

	SendTests(Context->GetSender());
}


void FAutomationWorkerModule::HandlePreTestingEvent()
{
#if WITH_ENGINE
	FAutomationTestFramework::Get().OnScreenshotCaptured().BindRaw(this, &FAutomationWorkerModule::HandleScreenShotCapturedWithName);
	FAutomationTestFramework::Get().OnScreenshotAndTraceCaptured().BindRaw(this, &FAutomationWorkerModule::HandleScreenShotAndTraceCapturedWithName);
#endif
}


void FAutomationWorkerModule::HandlePostTestingEvent()
{
#if WITH_ENGINE
	FAutomationTestFramework::Get().OnScreenshotAndTraceCaptured().Unbind();
	FAutomationTestFramework::Get().OnScreenshotCaptured().Unbind();
#endif
}


void FAutomationWorkerModule::HandleScreenShotCompared(const FAutomationWorkerImageComparisonResults& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received ScreenShotCompared from %s"), *Context->GetSender().ToString());

	// Image comparison finished.
	FAutomationScreenshotCompareResults CompareResults;
	CompareResults.UniqueId = Message.UniqueId;
	CompareResults.bWasNew = Message.bNew;
	CompareResults.bWasSimilar = Message.bSimilar;
	CompareResults.MaxLocalDifference = Message.MaxLocalDifference;
	CompareResults.GlobalDifference = Message.GlobalDifference;
	CompareResults.ErrorMessage = Message.ErrorMessage;

	FAutomationTestFramework::Get().NotifyScreenshotComparisonComplete(CompareResults);
}

void FAutomationWorkerModule::HandleTestDataRetrieved(const FAutomationWorkerTestDataResponse& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received TestDataRetrieved from %s"), *Context->GetSender().ToString());

	FAutomationTestFramework::Get().NotifyTestDataRetrieved(Message.bIsNew, Message.JsonData);
}

void FAutomationWorkerModule::HandlePerformanceDataRetrieved(const FAutomationWorkerPerformanceDataResponse& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received PerformanceDataRetrieved from %s"), *Context->GetSender().ToString());

	FAutomationTestFramework::Get().NotifyPerformanceDataRetrieved(Message.bSuccess, Message.ErrorMessage);
}

#if WITH_ENGINE
void FAutomationWorkerModule::HandleScreenShotCapturedWithName(const TArray<FColor>& RawImageData, const FAutomationScreenshotData& Data)
{
	HandleScreenShotAndTraceCapturedWithName(RawImageData, TArray<uint8>(), Data);
}

void FAutomationWorkerModule::HandleScreenShotAndTraceCapturedWithName(const TArray<FColor>& RawImageData, const TArray<uint8>& CapturedFrameTrace, const FAutomationScreenshotData& Data)
{
#if WITH_AUTOMATION_TESTS
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/ImageCompare"));
	int32 NewHeight = Data.Height;
	int32 NewWidth = Data.Width;

	TArray64<uint8> CompressedBitmap;
	FImageUtils::PNGCompressImageArray(NewWidth, NewHeight, TArrayView64<const FColor>(RawImageData.GetData(), RawImageData.Num()), CompressedBitmap);

	FAutomationScreenshotMetadata Metadata(Data);
		
	// Send the screen shot if we have a target
	if (TestRequesterAddress.IsValid())
	{
		FAutomationWorkerScreenImage* Message = FMessageEndpoint::MakeMessage<FAutomationWorkerScreenImage>();

		Message->ScreenShotName = Data.ScreenshotName;
		Message->ScreenImage = CompressedBitmap;
		Message->FrameTrace = CapturedFrameTrace;
		Message->Metadata = Metadata;

		UE_LOG(LogAutomationWorker, Log, TEXT("Sending screenshot %s to %s"), *Message->ScreenShotName, *TestRequesterAddress.ToString());

		MessageEndpoint->Send(Message, TestRequesterAddress);
	}
	else
	{
		//Save locally
		const bool bTree = true;

		FString LocalFile = AutomationCommon::GetLocalPathForScreenshot(Data.ScreenshotName);
		FString LocalTraceFile = FPaths::ChangeExtension(LocalFile, TEXT(".rdc"));
		FString DirectoryPath = FPaths::GetPath(LocalFile);

		UE_LOG(LogAutomationWorker, Log, TEXT("Saving screenshot %s as %s"),*Data.ScreenshotName, *LocalFile);

		if (!IFileManager::Get().MakeDirectory(*DirectoryPath, bTree))
		{
			UE_LOG(LogAutomationWorker, Error, TEXT("Failed to create directory %s for incoming screenshot"), *DirectoryPath);
			return;
		}

		if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *LocalFile))
		{
			uint32 WriteErrorCode = FPlatformMisc::GetLastError();
			TCHAR WriteErrorBuffer[2048];
			FPlatformMisc::GetSystemErrorMessage(WriteErrorBuffer, 2048, WriteErrorCode);
			UE_LOG(LogAutomationWorker, Warning, TEXT("Fail to save screenshot to %s. WriteError: %u (%s)"), *LocalFile, WriteErrorCode, WriteErrorBuffer);
			return;
		}

		if (CapturedFrameTrace.Num() > 0)
		{
			if (!FFileHelper::SaveArrayToFile(CapturedFrameTrace, *LocalTraceFile))
			{
				uint32 WriteErrorCode = FPlatformMisc::GetLastError();
				TCHAR WriteErrorBuffer[2048];
				FPlatformMisc::GetSystemErrorMessage(WriteErrorBuffer, 2048, WriteErrorCode);
				UE_LOG(LogAutomationWorker, Warning, TEXT("Failed to save frame trace to %s. WriteError: %u (%s)"), *LocalTraceFile, WriteErrorCode, WriteErrorBuffer);
			}
		}

		FString Json;
		if ( FJsonObjectConverter::UStructToJsonObjectString(Metadata, Json) )
		{
			FString MetadataPath = FPaths::ChangeExtension(LocalFile, TEXT("json"));
			FFileHelper::SaveStringToFile(Json, *MetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	}
#endif // WITH_AUTOMATION_TESTS
}
#endif

FString GetRHIForAutomation()
{
	// Remove any extra information in () from RHI string
	FString RHI = FApp::GetGraphicsRHI();
	int Pos;
	if (RHI.FindChar(*TEXT("("), Pos))
	{
		RHI = RHI.Left(Pos).TrimEnd();
	}
	return RHI;
}

void FAutomationWorkerModule::HandleRunTestsMessage( const FAutomationWorkerRunTests& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received RunTests %s from %s"), *Message.BeautifiedTestName, *Context->GetSender().ToString());

	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Worker"));
	
	if (TestRequesterAddress.IsValid() && !TestName.IsEmpty())
	{
		if (TestRequesterAddress == Context->GetSender())
		{
			UE_LOG(LogAutomationWorker, Log, TEXT("Worker is already running test '%s' from %s. Request is ignored."), *BeautifiedTestName, *TestRequesterAddress.ToString());
			return;
		}

		FString LogMessage = FString::Format(TEXT("Worker is already running test '%s' from %s. '%s' won't be run."), 
			{ *BeautifiedTestName, *TestRequesterAddress.ToString(), *Message.BeautifiedTestName });
		UE_LOG(LogAutomationWorker, Warning, TEXT("%s"), *LogMessage);

		// Let the sender know it won't happen
		FAutomationWorkerRunTestsReply* OutMessage = FMessageEndpoint::MakeMessage<FAutomationWorkerRunTestsReply>();
		OutMessage->TestName = Message.TestName;
		OutMessage->ExecutionCount = Message.ExecutionCount;
		OutMessage->State = EAutomationState::Skipped;
		OutMessage->Entries.Add(FAutomationExecutionEntry(FAutomationEvent(EAutomationEventType::Error, LogMessage)));
		OutMessage->ErrorTotal = 1;
		MessageEndpoint->Send(OutMessage, Context->GetSender());

		return;
	}

	// Do we need to skip the test
	FName SkipReason;
	bool bWarn(false);
	UAutomationTestExcludelist* Excludelist = UAutomationTestExcludelist::Get();
	static FString RHI = GetRHIForAutomation();
	if (Excludelist->IsTestExcluded(Message.FullTestPath, RHI, &SkipReason, &bWarn))
	{
		FString SkippingMessage = FString::Format(TEXT("Test Skipped. Name={{0}} Reason={{1}} Path={{2}}"),
			{ *Message.BeautifiedTestName, *SkipReason.ToString(), *Message.FullTestPath });
		if (bWarn)
		{
			UE_LOG(LogAutomationWorker, Warning, TEXT("%s"), *SkippingMessage);
		}
		else
		{
			UE_LOG(LogAutomationWorker, Display, TEXT("%s"), *SkippingMessage);
		}

		FAutomationWorkerRunTestsReply* OutMessage = FMessageEndpoint::MakeMessage<FAutomationWorkerRunTestsReply>();
		OutMessage->TestName = Message.TestName;
		OutMessage->ExecutionCount = Message.ExecutionCount;
		OutMessage->State = EAutomationState::Skipped;
		OutMessage->Entries.Add(FAutomationExecutionEntry(FAutomationEvent(EAutomationEventType::Info, FString::Printf(TEXT("Skipping test because of exclude list: %s"), *SkipReason.ToString()))));
		MessageEndpoint->Send(OutMessage, Context->GetSender());

		return;
	}

	ExecutionCount = Message.ExecutionCount;
	TestName = Message.TestName;
	BeautifiedTestName = Message.BeautifiedTestName;
	bSendAnalytics = Message.bSendAnalytics;
	TestRequesterAddress = Context->GetSender();

	// Always allow the first network command to execute
	bExecuteNextNetworkCommand = true;

	// We are not executing network command sub-commands right now
	bExecutingNetworkCommandResults = false;

	FAutomationTestFramework::Get().StartTestByName(Message.TestName, Message.RoleIndex);
}


void FAutomationWorkerModule::HandleStopTestsMessage(const FAutomationWorkerStopTests& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogAutomationWorker, Log, TEXT("Received StopTests from %s"), *Context->GetSender().ToString());

	if (GIsAutomationTesting)
	{
		FAutomationTestFramework::Get().DequeueAllCommands();
	}
	ReportTestComplete();
}


//dispatches analytics events to the data collector
void FAutomationWorkerModule::SendAnalyticsEvents(TArray<FString>& InAnalyticsItems)
{
	for (int32 i = 0; i < InAnalyticsItems.Num(); ++i)
	{
		FString EventString = InAnalyticsItems[i];
		if( EventString.EndsWith( TEXT( ",PERF" ) ) )
		{
			// Chop the ",PERF" off the end
			EventString.LeftInline( EventString.Len() - 5, false );

			FAutomationPerformanceSnapshot PerfSnapshot;
			PerfSnapshot.FromCommaDelimitedString( EventString );
			
			RecordPerformanceAnalytics( PerfSnapshot );
		}
	}
}

void FAutomationWorkerModule::HandleTelemetryData(const FString& StorageName, const FString& InTestName, const TArray<FAutomationTelemetryData>& InItems)
{
	FAutomationWorkerTelemetryData* Message = FMessageEndpoint::MakeMessage<FAutomationWorkerTelemetryData>();

	Message->Storage = StorageName;
	Message->Platform = FPlatformProperties::PlatformName();
	Message->Configuration = LexToString(FApp::GetBuildConfiguration());
	Message->TestName = InTestName;
	for (const FAutomationTelemetryData& Item : InItems)
	{
		Message->Items.Add(FAutomationWorkerTelemetryItem(Item));
	}

	UE_LOG(LogAutomationWorker, Log, TEXT("Sending Telemetry Data for %s"), *Message->TestName);

	MessageEndpoint->Send(Message, TestRequesterAddress);
}

void FAutomationWorkerModule::RecordPerformanceAnalytics( const FAutomationPerformanceSnapshot& PerfSnapshot )
{
	// @todo: Pass in additional performance capture data from incoming FAutomationPerformanceSnapshot!

	FAutomationAnalytics::FireEvent_FPSCapture(PerfSnapshot);
}

#undef LOCTEXT_NAMESPACE
