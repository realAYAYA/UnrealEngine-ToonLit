// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include <inttypes.h>

#include "Algo/Copy.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/ThreadHeartBeat.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Regex.h"
#include "Logging/StructuredLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeRWLock.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogLatentCommands)
DEFINE_LOG_CATEGORY_STATIC(LogAutomationTestStateTrace, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAutomationTest, Warning, All);

namespace AutomationTest
{
	static bool bCaptureLogEvents = true;
	static FAutoConsoleVariableRef CVarAutomationCaptureLogEvents(
		TEXT("Automation.CaptureLogEvents"),
		bCaptureLogEvents,
		TEXT("Consider warning/error log events during a test as impacting the test itself"));

	static bool bSkipStackWalk = false;
	static FAutoConsoleVariableRef CVarAutomationSkipStackWalk(
		TEXT("Automation.SkipStackWalk"),
		bSkipStackWalk,
		TEXT("Whether to skip any stack issues that the automation test framework triggers"));

	static bool bLogBPTestMetadata = false;
	static FAutoConsoleVariableRef CVarAutomationLogBPTestMetadata(
		TEXT("Automation.LogBPTestMetadata"),
		bLogBPTestMetadata,
		TEXT("Whether to output blueprint functional test metadata to the log when test is running"));

	static bool bLogTestStateTrace = false;
	static FAutoConsoleVariableRef CVarAutomationLogTestStateTrace(
		TEXT("Automation.LogTestStateTrace"),
		bLogTestStateTrace,
		TEXT("Whether to enable or disable logging of test state trace"));

	static bool bEnableStereoTestVariants = false;
	static FAutoConsoleVariableRef CVarAutomationEnableStereoTestVariants(
		TEXT("Automation.EnableStereoTestVariants"),
		bEnableStereoTestVariants,
		TEXT("Whether to enable stereo test variants for screenshot functional tests"));

	static bool bLightweightStereoTestVariants = true;
	static FAutoConsoleVariableRef CVarAutomationLightweightStereoTestVariants(
		TEXT("Automation.LightweightStereoTestVariants"),
		bLightweightStereoTestVariants,
		TEXT("Whether to skip variants when the baseline test fails, and skip saving screenshots for successful variants"));

	// The method prepares the filename and LineNumber to be placed in the form that could be extracted by SAutomationWindow widget if it is additionally eclosed into []
	// The result format is filename(line)
	static FString CreateFileLineDescription(const FString& Filename, const int32 LineNumber)
	{
		FString Result;

		if (!Filename.IsEmpty() && LineNumber > 0)
		{
			Result += Filename;
			Result += TEXT("(");
			Result += FString::FromInt(LineNumber);
			Result += TEXT(")");
		}

		return Result;
	}

	/*
		Determine the level that a log item should be written to the automation log based on the properties of the current test.
		only Display/Warning/Error are supported in the automation log so anything with NoLogging/Log will not be shown
	*/
	static ELogVerbosity::Type GetAutomationLogLevel(ELogVerbosity::Type LogVerbosity, FName LogCategory, FAutomationTestBase* CurrentTest)
	{
		ELogVerbosity::Type EffectiveVerbosity = LogVerbosity;

		static FCriticalSection ActionCS;
		static FAutomationTestBase* LastTest = nullptr;

		if (AutomationTest::bCaptureLogEvents == false)
		{
			return ELogVerbosity::NoLogging;
		}

		{
			FScopeLock Lock(&ActionCS);
			if (CurrentTest != LastTest)
			{
				FAutomationTestBase::SuppressedLogCategories.Empty();
				FAutomationTestBase::LoadDefaultLogSettings();
				LastTest = CurrentTest;
			}
		}

		if (CurrentTest)
		{
			if (CurrentTest->SuppressLogs() || CurrentTest->GetSuppressedLogCategories().Contains(LogCategory.ToString()))
			{
				EffectiveVerbosity = ELogVerbosity::NoLogging;
			}
			else
			{
				if (EffectiveVerbosity == ELogVerbosity::Warning)
				{
					if (CurrentTest->SuppressLogWarnings())
					{
						EffectiveVerbosity = ELogVerbosity::NoLogging;
					}
					else if (CurrentTest->ElevateLogWarningsToErrors())
					{
						EffectiveVerbosity = ELogVerbosity::Error;
					}
				}

				if (EffectiveVerbosity == ELogVerbosity::Error)
				{
					if (CurrentTest->SuppressLogErrors())
					{
						EffectiveVerbosity = ELogVerbosity::NoLogging;
					}
				}
			}
		}

		return EffectiveVerbosity;
	}
};

bool FAutomationTestBase::bSuppressLogWarnings = false;
bool FAutomationTestBase::bSuppressLogErrors = false;
bool FAutomationTestBase::bElevateLogWarningsToErrors = false;
TArray<FString> FAutomationTestBase::SuppressedLogCategories;

CORE_API const TMap<FString, EAutomationTestFlags::Type>& EAutomationTestFlags::GetTestFlagsMap()
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));
	/** String to EAutomationTestFlags map */
	static const TMap<FString, Type> FlagsMap = {
		{ TEXT("EditorContext"), Type::EditorContext},
		{ TEXT("ClientContext"), Type::ClientContext},
		{ TEXT("ServerContext"), Type::ServerContext},
		{ TEXT("CommandletContext"), Type::CommandletContext},
		{ TEXT("ApplicationContextMask"), Type::ApplicationContextMask},
		{ TEXT("NonNullRHI"), Type::NonNullRHI},
		{ TEXT("RequiresUser"), Type::RequiresUser},
		{ TEXT("FeatureMask"), Type::FeatureMask},
		{ TEXT("Disabled"), Type::Disabled},
		{ TEXT("CriticalPriority"), Type::CriticalPriority},
		{ TEXT("HighPriority"), Type::HighPriority},
		{ TEXT("HighPriorityAndAbove"), Type::HighPriorityAndAbove},
		{ TEXT("MediumPriority"), Type::MediumPriority},
		{ TEXT("MediumPriorityAndAbove"), Type::MediumPriorityAndAbove},
		{ TEXT("LowPriority"), Type::LowPriority},
		{ TEXT("PriorityMask"), Type::PriorityMask},
		{ TEXT("SmokeFilter"), Type::SmokeFilter},
		{ TEXT("EngineFilter"), Type::EngineFilter},
		{ TEXT("ProductFilter"), Type::ProductFilter},
		{ TEXT("PerfFilter"), Type::PerfFilter},
		{ TEXT("StressFilter"), Type::StressFilter},
		{ TEXT("NegativeFilter"), Type::NegativeFilter},
		{ TEXT("FilterMask"), Type::FilterMask}
	};
	return FlagsMap;
};

void FAutomationTestFramework::FAutomationTestOutputDevice::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	const int32 STACK_OFFSET = 8;//FMsg::Logf_InternalImpl
	// TODO would be nice to search for the first stack frame that isn't in output device or other logging files, would be more robust.

	if (!IsRunningCommandlet() && (Verbosity == ELogVerbosity::SetColor))
	{
		return;
	}

	// Ensure there's a valid unit test associated with the context
	FAutomationTestBase* const LocalCurTest = CurTest.load(std::memory_order_relaxed);
	if (LocalCurTest)
	{
		bool CaptureLog = !LocalCurTest->SuppressLogs()
			&& (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Display)
			&& LocalCurTest->ShouldCaptureLogCategory(Category);

		if (CaptureLog)
		{
			ELogVerbosity::Type EffectiveVerbosity = AutomationTest::GetAutomationLogLevel(Verbosity, Category, LocalCurTest);

			FString FormattedMsg = FString::Printf(TEXT("%s: %s [log]"), *Category.ToString(), V);
			
			// Errors
			if (EffectiveVerbosity == ELogVerbosity::Error)
			{
				LocalCurTest->AddError(FormattedMsg, STACK_OFFSET);
			}
			// Warnings
			else if (EffectiveVerbosity == ELogVerbosity::Warning)
			{
				LocalCurTest->AddWarning(FormattedMsg, STACK_OFFSET);
			}
			// Display
			else if (EffectiveVerbosity != ELogVerbosity::NoLogging)
			{
				LocalCurTest->AddInfo(FormattedMsg, STACK_OFFSET);
			}
		}
		// Log...etc
		else
		{
			// IMPORTANT NOTE: This code will never be called in a build with NO_LOGGING defined, which means pretty much
			// any Test or Shipping config build.  If you're trying to use the automation test framework for performance
			// data capture in a Test config, you'll want to call the AddAnalyticsItemToCurrentTest() function instead of
			// using this log interception stuff.

			FString LogString = FString(V);
			FString AnalyticsString = TEXT("AUTOMATIONANALYTICS");
			if (LogString.StartsWith(*AnalyticsString))
			{
				//Remove "analytics" from the string
				LogString.RightInline(LogString.Len() - (AnalyticsString.Len() + 1), EAllowShrinking::No);

				LocalCurTest->AddAnalyticsItem(LogString);
			}
			//else
			//{
			//	LocalCurTest->AddInfo(LogString, STACK_OFFSET);
			//}
		}
	}
}

void FAutomationTestFramework::FAutomationTestMessageFilter::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	Serialize(V, Verbosity, Category, -1.0);
}

void FAutomationTestFramework::FAutomationTestMessageFilter::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time)
{
	// Prevent null dereference if logging happens in async tasks while changing DestinationContext
	FFeedbackContext* const LocalDestinationContext = DestinationContext.load(std::memory_order_relaxed);
	FAutomationTestBase* const LocalCurTest = CurTest.load(std::memory_order_relaxed);
	if (LocalDestinationContext)
	{
		if (LocalCurTest && LocalCurTest->IsExpectedMessage(FString(V), Verbosity))
		{
			Verbosity = ELogVerbosity::Verbose;
		}
		{
			FScopeLock CriticalSection(&ActionCS);
			LocalDestinationContext->Serialize(V, Verbosity, Category, Time);
		}
	}
}

void FAutomationTestFramework::FAutomationTestMessageFilter::SerializeRecord(const UE::FLogRecord& Record)
{
	// Prevent null dereference if logging happens in async tasks while changing DestinationContext
	FFeedbackContext* const LocalDestinationContext = DestinationContext.load(std::memory_order_relaxed);
	FAutomationTestBase* const LocalCurTest = CurTest.load(std::memory_order_relaxed);
	if (LocalDestinationContext)
	{
		UE::FLogRecord LocalRecord = Record;
		const ELogVerbosity::Type Verbosity = LocalRecord.GetVerbosity();
		if ((Verbosity == ELogVerbosity::Warning) || (Verbosity == ELogVerbosity::Error))
		{
			TStringBuilder<512> Line;
			Record.FormatMessageTo(Line);
			if (LocalCurTest && LocalCurTest->IsExpectedMessage(FString(Line), ELogVerbosity::Warning))
			{
				LocalRecord.SetVerbosity(ELogVerbosity::Verbose);
			}
		}
		{
			FScopeLock CriticalSection(&ActionCS);
			LocalDestinationContext->SerializeRecord(LocalRecord);
		}
	}
}

FAutomationTestFramework& FAutomationTestFramework::Get()
{
	static FAutomationTestFramework Framework;
	return Framework;
}

FString FAutomationTestFramework::GetUserAutomationDirectory() const
{
	const FString DefaultAutomationSubFolder = TEXT("Unreal Automation");
	return FString(FPlatformProcess::UserDir()) + DefaultAutomationSubFolder;
}

bool FAutomationTestFramework::NeedSkipStackWalk()
{
	return AutomationTest::bSkipStackWalk;
}


bool FAutomationTestFramework::NeedLogBPTestMetadata()
{
	return AutomationTest::bLogBPTestMetadata;
}

bool FAutomationTestFramework::NeedPerformStereoTestVariants()
{
	return AutomationTest::bEnableStereoTestVariants;
}

bool FAutomationTestFramework::NeedUseLightweightStereoTestVariants()
{
	return AutomationTest::bLightweightStereoTestVariants;
}

bool FAutomationTestFramework::RegisterAutomationTest( const FString& InTestNameToRegister, FAutomationTestBase* InTestToRegister )
{
	const bool bAlreadyRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToRegister );
	if ( !bAlreadyRegistered )
	{
		LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));
		AutomationTestClassNameToInstanceMap.Add( InTestNameToRegister, InTestToRegister );
	}
	return !bAlreadyRegistered;
}

bool FAutomationTestFramework::UnregisterAutomationTest( const FString& InTestNameToUnregister )
{
	const bool bRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToUnregister );
	if ( bRegistered )
	{
		AutomationTestClassNameToInstanceMap.Remove( InTestNameToUnregister );
	}
	return bRegistered;
}

void FAutomationTestFramework::EnqueueLatentCommand(TSharedPtr<IAutomationLatentCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests - will only catch when smokes are exclusively requested
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	LatentCommands.Enqueue(NewCommand);
}

void FAutomationTestFramework::EnqueueNetworkCommand(TSharedPtr<IAutomationNetworkCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	NetworkCommands.Enqueue(NewCommand);
}

bool FAutomationTestFramework::ContainsTest( const FString& InTestName ) const
{
	return AutomationTestClassNameToInstanceMap.Contains( InTestName );
}

static double SumDurations(const TMap<FString, FAutomationTestExecutionInfo>& Executions)
{
	double Sum = 0;
	for (const TPair<FString, FAutomationTestExecutionInfo>& Execution : Executions)
	{
		Sum += Execution.Value.Duration;
	}
	return Sum;
}

static const TCHAR* FindSlowestTest(const TMap<FString, FAutomationTestExecutionInfo>& Executions, double& OutMaxDuration)
{
	check(Executions.Num() > 0);

	const TCHAR* OutName = nullptr;
	OutMaxDuration = 0;
	for (const TPair<FString, FAutomationTestExecutionInfo>& Execution : Executions)
	{
		if (OutMaxDuration <= Execution.Value.Duration)
		{
			OutMaxDuration = Execution.Value.Duration;
			OutName = *Execution.Key;
		}
	}

	return OutName;
}

bool FAutomationTestFramework::RunSmokeTests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationTestFramework::RunSmokeTests);

	bool bAllSuccessful = true;

	uint32 PreviousRequestedTestFilter = RequestedTestFilter;
	//so extra log spam isn't generated
	RequestedTestFilter = EAutomationTestFlags::SmokeFilter;
	
	// Skip running on cooked platforms like mobile
	//@todo - better determination of whether to run than requires cooked data
	// Ensure there isn't another slow task in progress when trying to run unit tests
	const bool bRequiresCookedData = FPlatformProperties::RequiresCookedData();
	if ((!bRequiresCookedData && !GIsSlowTask && !GIsPlayInEditorWorld && !FPlatformProperties::IsProgram() && !IsRunningCommandlet()) || bForceSmokeTests)
	{
		TArray<FAutomationTestInfo> TestInfo;

		GetValidTestNames( TestInfo );

		if ( TestInfo.Num() > 0 )
		{
			// Output the results of running the automation tests
			TMap<FString, FAutomationTestExecutionInfo> OutExecutionInfoMap;

			// Run each valid test
			FScopedSlowTask SlowTask((float)TestInfo.Num());

			// We disable capturing the stack when running smoke tests, it adds too much overhead to do it at startup.
			FAutomationTestFramework::Get().SetCaptureStack(false);

			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				SlowTask.EnterProgressFrame(1);
				if (TestInfo[TestIndex].GetTestFlags() & EAutomationTestFlags::SmokeFilter )
				{
					FString TestCommand = TestInfo[TestIndex].GetTestName();
					FAutomationTestExecutionInfo& CurExecutionInfo = OutExecutionInfoMap.Add( TestCommand, FAutomationTestExecutionInfo() );
					
					int32 RoleIndex = 0;  //always default to "local" role index.  Only used for multi-participant tests
					StartTestByName( TestCommand, RoleIndex );
					const bool CurTestSuccessful = StopTest(CurExecutionInfo);

					bAllSuccessful = bAllSuccessful && CurTestSuccessful;
				}
			}

			FAutomationTestFramework::Get().SetCaptureStack(true);

#if !UE_BUILD_DEBUG
			const double TotalDuration = SumDurations(OutExecutionInfoMap);
			if (bAllSuccessful && !FPlatformMisc::IsDebuggerPresent() && TotalDuration > 2.0)
			{
				//force a failure if a smoke test takes too long
				double SlowestTestDuration = 0;
				const TCHAR* SlowestTestName = FindSlowestTest(OutExecutionInfoMap, /* out */ SlowestTestDuration);
				UE_LOG(LogAutomationTest, Warning, TEXT("Smoke tests took >2s to run (%.2fs). '%s' took %dms. "
					"SmokeFilter tier tests should take less than 1ms. Please optimize or move '%s' to a slower tier than SmokeFilter."), 
					TotalDuration, SlowestTestName, static_cast<int32>(1000*SlowestTestDuration), SlowestTestName);
			}
#endif

			FAutomationTestFramework::DumpAutomationTestExecutionInfo( OutExecutionInfoMap );
		}
	}
	else if( bRequiresCookedData || IsRunningCommandlet())
	{
		UE_LOG( LogAutomationTest, Log, TEXT( "Skipping unit tests for the cooked build and commandlet." ) );
	}
	else if (!FPlatformProperties::IsProgram())
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Skipping unit tests.") );
		bAllSuccessful = false;
	}

	//revert to allowing all logs
	RequestedTestFilter = PreviousRequestedTestFilter;

	return bAllSuccessful;
}

void FAutomationTestFramework::ResetTests()
{
	bool bEnsureExists = false;
	bool bDeleteEntireTree = true;
	//make sure all transient files are deleted successfully
	IFileManager::Get().DeleteDirectory(*FPaths::AutomationTransientDir(), bEnsureExists, bDeleteEntireTree);
}

void FAutomationTestFramework::StartTestByName( const FString& InTestToRun, const int32 InRoleIndex, const FString& InFullTestPath )
{
	if (GIsAutomationTesting)
	{
		while(!LatentCommands.IsEmpty())
		{
			TSharedPtr<IAutomationLatentCommand> TempCommand;
			LatentCommands.Dequeue(TempCommand);
		}
		while(!NetworkCommands.IsEmpty())
		{
			TSharedPtr<IAutomationNetworkCommand> TempCommand;
			NetworkCommands.Dequeue(TempCommand);
		}
		FAutomationTestExecutionInfo TempExecutionInfo;
		StopTest(TempExecutionInfo);
	}

	FString TestName;
	FString Params;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Params, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}
	FString TestPath = InFullTestPath.IsEmpty() ? InTestToRun : InFullTestPath;

	NetworkRoleIndex = InRoleIndex;

	// Ensure there isn't another slow task in progress when trying to run unit tests
	if ( !GIsSlowTask && !GIsPlayInEditorWorld )
	{
		// Ensure the test exists in the framework and is valid to run
		if ( ContainsTest( TestName ) )
		{
			// Make any setting changes that have to occur to support unit testing
			PrepForAutomationTests();

			InternalStartTest( InTestToRun, TestPath);
		}
		else
		{
			UE_LOG(LogAutomationTest, Error, TEXT("Test %s does not exist and could not be run."), *TestPath);
		}
	}
	else
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Test %s is too slow and could not be run."), *TestPath);
	}
}

bool FAutomationTestFramework::StopTest( FAutomationTestExecutionInfo& OutExecutionInfo )
{
	check(GIsAutomationTesting);
	
	bool bSuccessful = InternalStopTest(OutExecutionInfo);

	// Restore any changed settings now that unit testing has completed
	ConcludeAutomationTests();

	return bSuccessful;
}


bool FAutomationTestFramework::ExecuteLatentCommands()
{
	check(GIsAutomationTesting);

	bool bHadAnyLatentCommands = !LatentCommands.IsEmpty();
	while (!LatentCommands.IsEmpty())
	{
		//get the next command to execute
		TSharedPtr<IAutomationLatentCommand> NextCommand;
		LatentCommands.Peek(NextCommand);

		bool bComplete = NextCommand->InternalUpdate();
		if (bComplete)
		{
			TSharedPtr<IAutomationLatentCommand>* TailCommand = LatentCommands.Peek();
			if (TailCommand != nullptr && NextCommand == *TailCommand)
			{
				//all done. remove the tail
				LatentCommands.Pop();
			}
			else
			{
				UE_LOG(LogAutomationTest, Verbose, TEXT("Tail of latent command queue is not removed, because last completed automation latent command is not corresponding."));
			}
		}
		else
		{
			break;
		}
	}
	//need more processing on the next frame
	if (bHadAnyLatentCommands)
	{
		return false;
	}

	return true;
}

bool FAutomationTestFramework::ExecuteNetworkCommands()
{
	check(GIsAutomationTesting);
	bool bHadAnyNetworkCommands = !NetworkCommands.IsEmpty();

	if( bHadAnyNetworkCommands )
	{
		// Get the next command to execute
		TSharedPtr<IAutomationNetworkCommand> NextCommand;
		NetworkCommands.Dequeue(NextCommand);
		if (NextCommand->GetRoleIndex() == NetworkRoleIndex)
		{
			NextCommand->Run();
		}
	}

	return !bHadAnyNetworkCommands;
}

void FAutomationTestFramework::DequeueAllCommands()
{
	while (!LatentCommands.IsEmpty())
	{
		TSharedPtr<IAutomationLatentCommand> TempCommand;
		LatentCommands.Dequeue(TempCommand);
	}
	while (!NetworkCommands.IsEmpty())
	{
		TSharedPtr<IAutomationNetworkCommand> TempCommand;
		NetworkCommands.Dequeue(TempCommand);
	}
}

void FAutomationTestFramework::LoadTestModules( )
{
	const bool bRunningEditor = GIsEditor && !IsRunningCommandlet();

	bool bRunningSmokeTests = ((RequestedTestFilter & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter);
	if( !bRunningSmokeTests )
	{
		TArray<FString> EngineTestModules;
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EngineTestModules"), EngineTestModules, GEngineIni);
		//Load any engine level modules.
		for( int32 EngineModuleId = 0; EngineModuleId < EngineTestModules.Num(); ++EngineModuleId)
		{
			const FName ModuleName = FName(*EngineTestModules[EngineModuleId]);
			//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
			if (ModuleName == NAME_None || ModuleName == TEXT("None"))
			{
				UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
				continue;
			}
			if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
			{
				UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
				FModuleManager::Get().LoadModule(ModuleName);
			}
		}
		//Load any editor modules.
		if( bRunningEditor )
		{
			TArray<FString> EditorTestModules;
			GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EditorTestModules"), EditorTestModules, GEngineIni);
			for( int32 EditorModuleId = 0; EditorModuleId < EditorTestModules.Num(); ++EditorModuleId )
			{
				const FName ModuleName = FName(*EditorTestModules[EditorModuleId]);
				//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
				if (ModuleName == NAME_None || ModuleName == TEXT("None"))
				{
					UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
					continue;
				}
				if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
					FModuleManager::Get().LoadModule(ModuleName);
				}
			}
		}
	}
}

void FAutomationTestFramework::GetValidTestNames( TArray<FAutomationTestInfo>& TestInfo ) const
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));
	TestInfo.Empty();

	// Determine required application type (Editor, Game, or Commandlet)
	const bool bRunningCommandlet = IsRunningCommandlet();
	const bool bRunningEditor = GIsEditor && !bRunningCommandlet;
	const bool bRunningClient = !GIsEditor && !IsRunningDedicatedServer();
	const bool bRunningServer = !GIsEditor && !IsRunningClientOnly();

	//application flags
	uint32 ApplicationSupportFlags = 0;
	if ( bRunningEditor )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::EditorContext;
	}
	if ( bRunningClient )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::ClientContext;
	}
	if ( bRunningServer )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::ServerContext;
	}
	if ( bRunningCommandlet )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::CommandletContext;
	}

	//Feature support - assume valid RHI until told otherwise
	uint32 FeatureSupportFlags = EAutomationTestFlags::FeatureMask;
	// @todo: Handle this correctly. GIsUsingNullRHI is defined at Engine-level, so it can't be used directly here in Core.
	// For now, assume Null RHI is only used for commandlets, servers, and when the command line specifies to use it.
	if (FPlatformProperties::SupportsWindowedMode())
	{
		bool bUsingNullRHI = FParse::Param( FCommandLine::Get(), TEXT("nullrhi") ) || IsRunningCommandlet() || IsRunningDedicatedServer();
		if (bUsingNullRHI)
		{
			FeatureSupportFlags &= (~EAutomationTestFlags::NonNullRHI);
		}
	}
	if (FApp::IsUnattended())
	{
		FeatureSupportFlags &= (~EAutomationTestFlags::RequiresUser);
	}

	for ( TMap<FString, FAutomationTestBase*>::TConstIterator TestIter( AutomationTestClassNameToInstanceMap ); TestIter; ++TestIter )
	{
		const FAutomationTestBase* CurTest = TestIter.Value();
		check( CurTest );

		uint32 CurTestFlags = CurTest->GetTestFlags();

		//filter out full tests when running smoke tests
		const bool bPassesFilterRequirement = ((CurTestFlags & RequestedTestFilter) != 0);

		//Application Tests
		uint32 CurTestApplicationFlags = (CurTestFlags & EAutomationTestFlags::ApplicationContextMask);
		const bool bPassesApplicationRequirements = (CurTestApplicationFlags == 0) || (CurTestApplicationFlags & ApplicationSupportFlags);
		
		//Feature Tests
		uint32 CurTestFeatureFlags = (CurTestFlags & EAutomationTestFlags::FeatureMask);
		const bool bPassesFeatureRequirements = (CurTestFeatureFlags == 0) || (CurTestFeatureFlags & FeatureSupportFlags);

		const bool bEnabled = (CurTestFlags & EAutomationTestFlags::Disabled) == 0;

		const double GenerateTestNamesStartTime = FPlatformTime::Seconds();
		
		if (bEnabled && bPassesApplicationRequirements && bPassesFeatureRequirements && bPassesFilterRequirement)
		{
			TArray<FAutomationTestInfo> TestsToAdd;
			CurTest->GenerateTestNames(TestsToAdd);
			TestInfo.Append(TestsToAdd);			
		}

		// Make sure people are not writing complex tests that take forever to return the names of the tests
		// otherwise the session frontend locks up when looking at your local tests.
		const double GenerateTestNamesEndTime = FPlatformTime::Seconds();
		const double TimeForGetTests = static_cast<float>(GenerateTestNamesEndTime - GenerateTestNamesStartTime);
		if (TimeForGetTests > 10.0f)
		{
			//force a failure if a smoke test takes too long
			UE_LOG(LogAutomationTest, Warning, TEXT("Automation Test '%s' took > 10 seconds to return from GetTests(...): %.2fs"), *CurTest->GetTestName(), (float)TimeForGetTests);
		}
	}
}

bool FAutomationTestFramework::ShouldTestContent(const FString& Path) const
{
	static TArray<FString> TestLevelFolders;
	if ( TestLevelFolders.Num() == 0 )
	{
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("TestLevelFolders"), TestLevelFolders, GEngineIni);
	}

	bool bMatchingDirectory = false;
	for ( const FString& Folder : TestLevelFolders )
	{
		const FString PatternToCheck = FString::Printf(TEXT("/%s/"), *Folder);
		if ( Path.Contains(*PatternToCheck) )
		{
			bMatchingDirectory = true;
		}
	}
	if (bMatchingDirectory)
	{
		return true;
	}

	const FString RelativePath = FPaths::ConvertRelativePathToFull(Path);
	const FString DevelopersPath = FPaths::ConvertRelativePathToFull(FPaths::GameDevelopersDir());
	return bDeveloperDirectoryIncluded || !RelativePath.StartsWith(DevelopersPath);
}

void FAutomationTestFramework::SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded)
{
	bDeveloperDirectoryIncluded = bInDeveloperDirectoryIncluded;
}

void FAutomationTestFramework::SetRequestedTestFilter(const uint32 InRequestedTestFlags)
{
	RequestedTestFilter = InRequestedTestFlags;
}

FOnTestScreenshotCaptured& FAutomationTestFramework::OnScreenshotCaptured()
{
	return TestScreenshotCapturedDelegate;
}

FOnTestScreenshotAndTraceCaptured& FAutomationTestFramework::OnScreenshotAndTraceCaptured()
{
	return TestScreenshotAndTraceCapturedDelegate;
}

FOnTestSectionEvent& FAutomationTestFramework::GetOnEnteringTestSection(const FString& Section)
{
	if (!OnEnteringTestSectionEvent.Contains(Section))
	{
		OnEnteringTestSectionEvent.Emplace(Section);
	}

	return *OnEnteringTestSectionEvent.Find(Section);
}

void FAutomationTestFramework::TriggerOnEnteringTestSection(const FString& Section) const
{
	if (const FOnTestSectionEvent* Delegate = OnEnteringTestSectionEvent.Find(Section))
	{
		Delegate->Broadcast(Section);
	}
}

bool FAutomationTestFramework::IsAnyOnEnteringTestSectionBound() const
{
	if (!OnEnteringTestSectionEvent.IsEmpty())
	{
		for (auto& SectionPair : OnEnteringTestSectionEvent)
		{
			if (SectionPair.Value.IsBound())
			{
				return true;
			}
		}
	}

	return false;
}

FOnTestSectionEvent& FAutomationTestFramework::GetOnLeavingTestSection(const FString& Section)
{
	if (!OnLeavingTestSectionEvent.Contains(Section))
	{
		OnLeavingTestSectionEvent.Emplace(Section);
	}

	return *OnLeavingTestSectionEvent.Find(Section);
}

void FAutomationTestFramework::TriggerOnLeavingTestSection(const FString& Section) const
{
	if (const FOnTestSectionEvent* Delegate = OnLeavingTestSectionEvent.Find(Section))
	{
		Delegate->Broadcast(Section);
	}
}

bool FAutomationTestFramework::IsAnyOnLeavingTestSectionBound() const
{
	if (!OnLeavingTestSectionEvent.IsEmpty())
	{
		for (auto& SectionPair : OnLeavingTestSectionEvent)
		{
			if (SectionPair.Value.IsBound())
			{
				return true;
			}
		}
	}

	return false;
}

void FAutomationTestFramework::PrepForAutomationTests()
{
	check(!GIsAutomationTesting);

	// Fire off callback signifying that unit testing is about to begin. This allows
	// other systems to prepare themselves as necessary without the unit testing framework having to know
	// about them.
	PreTestingEvent.Broadcast();

	OriginalGWarn = GWarn;
	AutomationTestMessageFilter.SetDestinationContext(GWarn);
	GWarn = &AutomationTestMessageFilter;
	GLog->AddOutputDevice(&AutomationTestOutputDevice);

	// Mark that unit testing has begun
	GIsAutomationTesting = true;
}

void FAutomationTestFramework::ConcludeAutomationTests()
{
	check(GIsAutomationTesting);
	
	// Mark that unit testing is over
	GIsAutomationTesting = false;

	GLog->RemoveOutputDevice(&AutomationTestOutputDevice);
	GWarn = OriginalGWarn;
	AutomationTestMessageFilter.SetDestinationContext(nullptr);
	OriginalGWarn = nullptr;

	// Fire off callback signifying that unit testing has concluded.
	PostTestingEvent.Broadcast();
}

/**
 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
 *
 * @param	InContext		Context to dump the execution info to
 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
 */
void FAutomationTestFramework::DumpAutomationTestExecutionInfo( const TMap<FString, FAutomationTestExecutionInfo>& InInfoToDump )
{
	const FString SuccessMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Success", "Success").ToString();
	const FString FailMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Fail", "Fail").ToString();
	for ( TMap<FString, FAutomationTestExecutionInfo>::TConstIterator MapIter(InInfoToDump); MapIter; ++MapIter )
	{
		const FString& CurTestName = MapIter.Key();
		const FAutomationTestExecutionInfo& CurExecutionInfo = MapIter.Value();

		UE_LOG(LogAutomationTest, Log, TEXT("%s: %s"), *CurTestName, CurExecutionInfo.bSuccessful ? *SuccessMessage : *FailMessage);

		for ( const FAutomationExecutionEntry& Entry : CurExecutionInfo.GetEntries() )
		{
			switch (Entry.Event.Type )
			{
				case EAutomationEventType::Info:
					UE_LOG(LogAutomationTest, Display, TEXT("%s"), *Entry.Event.Message);
					break;
				case EAutomationEventType::Warning:
					UE_LOG(LogAutomationTest, Warning, TEXT("%s"), *Entry.Event.Message);
					break;
				case EAutomationEventType::Error:
					UE_LOG(LogAutomationTest, Error, TEXT("%s"), *Entry.Event.Message);
					break;
			}
		}
	}
}

void FAutomationTestFramework::InternalStartTest( const FString& InTestToRun, const FString& InFullTestPath)
{
	Parameters.Empty();
	CurrentTestFullPath.Empty();

	FString TestName;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Parameters, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}

	if ( ContainsTest( TestName ) )
	{
		CurrentTest = *( AutomationTestClassNameToInstanceMap.Find( TestName ) );
		check( CurrentTest );

		// Clear any execution info from the test in case it has been run before
		CurrentTest->ClearExecutionInfo();

		// Associate the test that is about to be run with the special unit test output device and feedback context
		AutomationTestOutputDevice.SetCurrentAutomationTest(CurrentTest);
		AutomationTestMessageFilter.SetCurrentAutomationTest(CurrentTest);

		StartTime = FPlatformTime::Seconds();

		CurrentTest->SetTestContext(Parameters);
		CurrentTestFullPath = InFullTestPath;

		// If not a smoke test, log the test has started.
		uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
		if (RequestedTestFilter & NonSmokeTestFlags)
		{
			if (AutomationTest::bLogTestStateTrace)
			{
				UE_LOG(LogAutomationTestStateTrace, Log, TEXT("Test is about to start. Name={%s}"), *CurrentTestFullPath);
			}
			UE_LOG(LogAutomationTest, Log, TEXT("%s %s is starting at %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, StartTime);
		}

		OnTestStartEvent.Broadcast(CurrentTest);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("AutomationTest %s"), *CurrentTest->GetBeautifiedTestName()));
			// Run the test!
			bTestSuccessful = CurrentTest->RunTest(Parameters);
		}
	}
}

bool FAutomationTestFramework::InternalStopTest(FAutomationTestExecutionInfo& OutExecutionInfo)
{
	check(GIsAutomationTesting);
	check(LatentCommands.IsEmpty());

	// Determine if the test was successful based on three criteria:
	// 1) Did the test itself report success?
	// 2) Did any errors occur and were logged by the feedback context during execution?++----
	// 3) Did we meet any errors that were expected with this test
	bTestSuccessful = bTestSuccessful && !CurrentTest->HasAnyErrors() && CurrentTest->HasMetExpectedMessages();

	{
		FWriteScopeLock Lock(CurrentTest->ActionCS);
		CurrentTest->ExpectedMessages.Empty();
	}

	// Set the success state of the test based on the above criteria
	CurrentTest->InternalSetSuccessState(bTestSuccessful);

	OnTestEndEvent.Broadcast(CurrentTest);

	double EndTime = FPlatformTime::Seconds();
	double TimeForTest = static_cast<float>(EndTime - StartTime);
	uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
	if (RequestedTestFilter & NonSmokeTestFlags)
	{
		UE_LOG(LogAutomationTest, Log, TEXT("%s %s ran in %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, TimeForTest);
		if (AutomationTest::bLogTestStateTrace)
		{
			UE_LOG(LogAutomationTestStateTrace, Log, TEXT("Test has stopped execution. Name={%s}"), *CurrentTestFullPath);
		}
	}

	// Fill out the provided execution info with the info from the test
	CurrentTest->GetExecutionInfo( OutExecutionInfo );

	// Save off timing for the test
	OutExecutionInfo.Duration = TimeForTest;

	// Disassociate the test from the output device and feedback context
	AutomationTestOutputDevice.SetCurrentAutomationTest(nullptr);
	AutomationTestMessageFilter.SetCurrentAutomationTest(nullptr);

	// Release pointers to now-invalid data
	CurrentTest = NULL;

	return bTestSuccessful;
}

bool FAutomationTestFramework::CanRunTestInEnvironment(const FString& InTestToRun, FString* OutReason, bool* OutWarn) const
{
	FString TestClassName;
	FString TestParameters;
	if (!InTestToRun.Split(TEXT(" "), &TestClassName, &TestParameters, ESearchCase::CaseSensitive))
	{
		TestClassName = InTestToRun;
	}

	if (!ContainsTest(TestClassName))
	{
		return false;
	}

	const FAutomationTestBase* const Test = *(AutomationTestClassNameToInstanceMap.Find(TestClassName));

	if (nullptr == Test)
	{
		return false;
	}

	if (!Test->CanRunInEnvironment(TestParameters, OutReason, OutWarn))
	{
		if (nullptr != OutReason)
		{
			if (OutReason->IsEmpty())
			{
				*OutReason = TEXT("unknown reason");
			}

			*OutReason += TEXT(" [code]");
			FString Filename = Test->GetTestSourceFileName();
			FPaths::MakePlatformFilename(Filename);
			const FString FileLineDescription = AutomationTest::CreateFileLineDescription(Filename, Test->GetTestSourceFileLine());
			if (!FileLineDescription.IsEmpty())
			{
				*OutReason += TEXT(" [");
				*OutReason += FileLineDescription;
				*OutReason += TEXT("]");
			}
		}
		
		return false;
	}

	return true;
}

void FAutomationTestFramework::AddAnalyticsItemToCurrentTest( const FString& AnalyticsItem )
{
	if( CurrentTest != nullptr )
	{
		CurrentTest->AddAnalyticsItem( AnalyticsItem );
	}
	else
	{
		UE_LOG( LogAutomationTest, Warning, TEXT( "AddAnalyticsItemToCurrentTest() called when no automation test was actively running!" ) );
	}
}

void FAutomationTestFramework::NotifyScreenshotComparisonComplete(const FAutomationScreenshotCompareResults& CompareResults)
{
	OnScreenshotCompared.Broadcast(CompareResults);
}

void FAutomationTestFramework::NotifyScreenshotComparisonReport(const FAutomationScreenshotCompareResults& CompareResults)
{
	OnScreenshotComparisonReport.Broadcast(CompareResults);
}

void FAutomationTestFramework::NotifyTestDataRetrieved(bool bWasNew, const FString& JsonData)
{
	OnTestDataRetrieved.Broadcast(bWasNew, JsonData);
}

void FAutomationTestFramework::NotifyPerformanceDataRetrieved(bool bSuccess, const FString& ErrorMessage)
{
	OnPerformanceDataRetrieved.Broadcast(bSuccess, ErrorMessage);
}

void FAutomationTestFramework::NotifyScreenshotTakenAndCompared()
{
	OnScreenshotTakenAndCompared.Broadcast();
}

FAutomationTestFramework::FAutomationTestFramework()
	: RequestedTestFilter(EAutomationTestFlags::SmokeFilter)
	, StartTime(0.0f)
	, bTestSuccessful(false)
	, CurrentTest(nullptr)
	, bDeveloperDirectoryIncluded(false)
	, NetworkRoleIndex(0)
	, bForceSmokeTests(false)
	, bCaptureStack(true)
{
}

FAutomationTestFramework::~FAutomationTestFramework()
{
	AutomationTestClassNameToInstanceMap.Empty();
}

FString FAutomationExecutionEntry::ToString() const
{
	FString ComplexString;

	ComplexString = Event.Message;

	if (!Event.Context.IsEmpty())
	{
		ComplexString += TEXT(" [");
		ComplexString += Event.Context;
		ComplexString += TEXT("] ");
	}

	// Place the filename at the end so it can be extracted by the SAutomationWindow widget
	// Expectation is "[filename(line)]"
	const FString FileLineDescription = AutomationTest::CreateFileLineDescription(Filename, LineNumber);
	if ( !FileLineDescription.IsEmpty() )
	{
		ComplexString += TEXT(" [");
		ComplexString += FileLineDescription;
		ComplexString += TEXT("]");
	}

	return ComplexString;
}

FString FAutomationExecutionEntry::ToStringFormattedEditorLog() const
{
	FString ComplexString;

	ComplexString = Event.Message;

	if (!Event.Context.IsEmpty())
	{
		ComplexString += TEXT(" [");
		ComplexString += Event.Context;
		ComplexString += TEXT("] ");
	}

	const FString FileLineDescription = AutomationTest::CreateFileLineDescription(Filename, LineNumber);
	if (!FileLineDescription.IsEmpty())
	{
		ComplexString += TEXT(" ");
		ComplexString += FileLineDescription;
	}

	return ComplexString;
}
//------------------------------------------------------------------------------

void FAutomationTestExecutionInfo::Clear()
{
	ContextStack.Reset();

	Entries.Empty();
	AnalyticsItems.Empty();
	TelemetryItems.Empty();
	TelemetryStorage.Empty();

	Errors = 0;
	Warnings = 0;
}

int32 FAutomationTestExecutionInfo::RemoveAllEvents(EAutomationEventType EventType)
{
	return RemoveAllEvents([EventType] (const FAutomationEvent& Event) {
		return Event.Type == EventType;
	});
}

int32 FAutomationTestExecutionInfo::RemoveAllEvents(TFunctionRef<bool(FAutomationEvent&)> FilterPredicate)
{
	int32 TotalRemoved = Entries.RemoveAll([this, &FilterPredicate](FAutomationExecutionEntry& Entry) {
		if (FilterPredicate(Entry.Event))
		{
			switch (Entry.Event.Type)
			{
			case EAutomationEventType::Warning:
				Warnings--;
				break;
			case EAutomationEventType::Error:
				Errors--;
				break;
			}

			return true;
		}
		return false;
	});

	return TotalRemoved;
}

void FAutomationTestExecutionInfo::AddEvent(const FAutomationEvent& Event, int StackOffset, bool bCaptureStack)
{
	LLM_SCOPE_BYNAME(TEXT("AutomationTest/Framework"));

	switch (Event.Type)
	{
	case EAutomationEventType::Warning:
		Warnings++;
		break;
	case EAutomationEventType::Error:
		Errors++;
		break;
	}

	int32 EntryIndex = -1;
	if (FAutomationTestFramework::Get().GetCaptureStack() && bCaptureStack)
	{
		SAFE_GETSTACK(Stack, StackOffset + 1, 1);
		if (Stack.Num())
		{
			EntryIndex = Entries.Add(FAutomationExecutionEntry(Event, Stack[0].Filename, Stack[0].LineNumber));
		}
	}
	if (EntryIndex == -1)
	{
		EntryIndex = Entries.Add(FAutomationExecutionEntry(Event));
	}

	FAutomationExecutionEntry& NewEntry = Entries[EntryIndex];

	if (NewEntry.Event.Context.IsEmpty())
	{
		NewEntry.Event.Context = GetContext();
	}
}

void FAutomationTestExecutionInfo::AddWarning(const FString& WarningMessage)
{
	AddEvent(FAutomationEvent(EAutomationEventType::Warning, WarningMessage));
}

void FAutomationTestExecutionInfo::AddError(const FString& ErrorMessage)
{
	AddEvent(FAutomationEvent(EAutomationEventType::Error, ErrorMessage));
}

//------------------------------------------------------------------------------

FAutomationEvent FAutomationScreenshotCompareResults::ToAutomationEvent() const
{
	FAutomationEvent Event(EAutomationEventType::Info, TEXT(""));
	FString OutputScreenshotName = ScreenshotPath;
	FPaths::NormalizeDirectoryName(OutputScreenshotName);
	OutputScreenshotName.ReplaceInline(TEXT("/"), TEXT("."));

	if (bWasNew)
	{
		Event.Type = EAutomationEventType::Warning;
		Event.Message = FString::Printf(TEXT("New Screenshot '%s' was discovered!  Please add a ground truth version of it."), *OutputScreenshotName);
	}
	else
	{
		if (bWasSimilar)
		{
			Event.Type = EAutomationEventType::Info;
			Event.Message = FString::Printf(TEXT("Screenshot '%s' was similar!  Global Difference = %f, Max Local Difference = %f"),
				*OutputScreenshotName, GlobalDifference, MaxLocalDifference);
		}
		else
		{
			Event.Type = EAutomationEventType::Error;

			if (ErrorMessage.IsEmpty())
			{
				Event.Message = FString::Printf(TEXT("Screenshot '%s' test failed, Screenshots were different!  Global Difference = %f, Max Local Difference = %f"),
					*OutputScreenshotName, GlobalDifference, MaxLocalDifference);
			}
			else
			{
				Event.Message = FString::Printf(TEXT("Screenshot '%s' test failed; Error = %s"), *OutputScreenshotName, *ErrorMessage);
			}
		}
	}

	Event.Artifact = UniqueId;
	return Event;
}

//------------------------------------------------------------------------------

void FAutomationTestBase::ClearExecutionInfo()
{
	ExecutionInfo.Clear();
}

void FAutomationTestBase::AddError(const FString& InError, int32 StackOffset)
{
	if( !IsExpectedMessage(InError, ELogVerbosity::Warning))
	{
		FWriteScopeLock Lock(ActionCS);
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, InError), StackOffset + 1);
	}
}

bool FAutomationTestBase::AddErrorIfFalse(bool bCondition, const FString& InError, int32 StackOffset)
{
	if (!bCondition)
	{
		AddError(InError, StackOffset + 1);
	}
	return bCondition;
}

void FAutomationTestBase::AddErrorS(const FString& InError, const FString& InFilename, int32 InLineNumber)
{
	if ( !IsExpectedMessage(InError, ELogVerbosity::Warning))
	{
		FWriteScopeLock Lock(ActionCS);
		//ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, InError, ExecutionInfo.GetContext(), InFilename, InLineNumber));
	}
}

void FAutomationTestBase::AddWarningS(const FString& InWarning, const FString& InFilename, int32 InLineNumber)
{
	if ( !IsExpectedMessage(InWarning, ELogVerbosity::Warning))
	{
		FWriteScopeLock Lock(ActionCS);
		//ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning, ExecutionInfo.GetContext(), InFilename, InLineNumber));
	}
}

void FAutomationTestBase::AddWarning( const FString& InWarning, int32 StackOffset )
{
	if ( !IsExpectedMessage(InWarning, ELogVerbosity::Warning))
	{
		FWriteScopeLock Lock(ActionCS);
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning), StackOffset + 1);
	}
}

void FAutomationTestBase::AddInfo( const FString& InLogItem, int32 StackOffset, bool bCaptureStack )
{
	if ( !IsExpectedMessage(InLogItem, ELogVerbosity::Display))
	{
		FWriteScopeLock Lock(ActionCS);
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, InLogItem), StackOffset + 1, bCaptureStack);
	}
}

void FAutomationTestBase::AddAnalyticsItem(const FString& InAnalyticsItem)
{
	FWriteScopeLock Lock(ActionCS);
	ExecutionInfo.AnalyticsItems.Add(InAnalyticsItem);
}

void FAutomationTestBase::AddTelemetryData(const FString& DataPoint, double Measurement, const FString& Context)
{
	FWriteScopeLock Lock(ActionCS);
	ExecutionInfo.TelemetryItems.Add(FAutomationTelemetryData(DataPoint, Measurement, Context));
}

void FAutomationTestBase::AddTelemetryData(const TMap <FString, double>& ValuePairs, const FString& Context)
{
	FWriteScopeLock Lock(ActionCS);
	for (const TPair<FString, double>& Item : ValuePairs)
	{
		ExecutionInfo.TelemetryItems.Add(FAutomationTelemetryData(Item.Key, Item.Value, Context));
	}
}

void FAutomationTestBase::SetTelemetryStorage(const FString& StorageName)
{
	ExecutionInfo.TelemetryStorage = StorageName;
}

void FAutomationTestBase::AddEvent(const FAutomationEvent& InEvent, int32 StackOffset, bool bCaptureStack)
{
	FWriteScopeLock Lock(ActionCS);
	ExecutionInfo.AddEvent(InEvent, StackOffset + 1, bCaptureStack);
}

bool FAutomationTestBase::HasAnyErrors() const
{
	return ExecutionInfo.GetErrorTotal() > 0;
}

bool FAutomationTestBase::HasMetExpectedMessages(ELogVerbosity::Type VerbosityType)
{
	bool bHasMetAllExpectedMessages = true;
	TArray<FAutomationExpectedMessage> ExpectedMessagesArray;
	{
		FReadScopeLock RLock(ActionCS);
		ExpectedMessagesArray = ExpectedMessages.Array();
	}
	for (FAutomationExpectedMessage& ExpectedMessage : ExpectedMessagesArray)
	{
		if (!LogCategoryMatchesSeverityInclusive(ExpectedMessage.Verbosity, VerbosityType))
		{
			continue;
		}

		// Avoid ambiguity of the messages below when the verbosity is "All"
		const TCHAR* LogVerbosityString = ExpectedMessage.Verbosity == ELogVerbosity::All ? TEXT("Any") : ToString(ExpectedMessage.Verbosity);

		const bool bExpectsOneOrMore = ExpectedMessage.ExpectedNumberOfOccurrences == 0;
		if (!bExpectsOneOrMore && (ExpectedMessage.ExpectedNumberOfOccurrences != ExpectedMessage.ActualNumberOfOccurrences))
		{
			FWriteScopeLock WLock(ActionCS);
			bHasMetAllExpectedMessages = false;

			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error,
				FString::Printf(TEXT("Expected ('%s') level log message or higher matching '%s' to occur %d times with %s match type, but it was found %d time(s).")
					, LogVerbosityString
					, *ExpectedMessage.MessagePatternString
					, ExpectedMessage.ExpectedNumberOfOccurrences
					, EAutomationExpectedMessageFlags::ToString(ExpectedMessage.CompareType)
					, ExpectedMessage.ActualNumberOfOccurrences)
				, ExecutionInfo.GetContext()));
		}
		else if (bExpectsOneOrMore)
		{
			FWriteScopeLock WLock(ActionCS);
			if (ExpectedMessage.ActualNumberOfOccurrences == 0)
			{
				bHasMetAllExpectedMessages = false;

				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error,
					FString::Printf(TEXT("Expected suppressed ('%s') level log message or higher matching '%s' did not occur.")
						, LogVerbosityString
						, *ExpectedMessage.MessagePatternString)
					, ExecutionInfo.GetContext()));
			}
			else
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info,
					FString::Printf(TEXT("Suppressed expected ('%s') level log message or higher matching '%s' %d times.")
						, LogVerbosityString
						, *ExpectedMessage.MessagePatternString
						, ExpectedMessage.ActualNumberOfOccurrences)
					, ExecutionInfo.GetContext()));
			}
		}
	}

	return bHasMetAllExpectedMessages;
}

bool FAutomationTestBase::HasMetExpectedErrors()
{
	return HasMetExpectedMessages(ELogVerbosity::Warning);
}

void FAutomationTestBase::InternalSetSuccessState( bool bSuccessful )
{
	ExecutionInfo.bSuccessful = bSuccessful;
}

bool FAutomationTestBase::GetLastExecutionSuccessState()
{
	return ExecutionInfo.bSuccessful;
}

void FAutomationTestBase::GetExecutionInfo( FAutomationTestExecutionInfo& OutInfo ) const
{
	OutInfo = ExecutionInfo;
}

void FAutomationTestBase::AddExpectedMessage(
	FString ExpectedPatternString,
	ELogVerbosity::Type ExpectedVerbosity,
	EAutomationExpectedMessageFlags::MatchType CompareType,
	int32 Occurrences,
	bool IsRegex)
{
	if (Occurrences >= 0)
	{
		FWriteScopeLock Lock(ActionCS);
		ExpectedMessages.Add(FAutomationExpectedMessage(ExpectedPatternString, ExpectedVerbosity, CompareType, Occurrences, IsRegex));
	}
	else
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Adding expected log message matching '%s' failed: number of expected occurrences must be >= 0"), *ExpectedPatternString);
	}
}

void FAutomationTestBase::AddExpectedMessage(
	FString ExpectedPatternString,
	EAutomationExpectedMessageFlags::MatchType CompareType,
	int32 Occurrences,
	bool IsRegex)
{
	AddExpectedMessage(MoveTemp(ExpectedPatternString), ELogVerbosity::All, CompareType, Occurrences, IsRegex);	
}

void FAutomationTestBase::AddExpectedMessagePlain(
	FString ExpectedString,
	ELogVerbosity::Type ExpectedVerbosity,
	EAutomationExpectedMessageFlags::MatchType CompareType,
	int32 Occurrences)
{
	AddExpectedMessage(MoveTemp(ExpectedString), ExpectedVerbosity, CompareType, Occurrences, false);
}

void FAutomationTestBase::AddExpectedMessagePlain(
	FString ExpectedString,
	EAutomationExpectedMessageFlags::MatchType CompareType,
	int32 Occurrences)
{
	AddExpectedMessagePlain(MoveTemp(ExpectedString), ELogVerbosity::All, CompareType, Occurrences);
}

void FAutomationTestBase::GetExpectedMessages(
	TArray<FAutomationExpectedMessage>& OutInfo,
	ELogVerbosity::Type Verbosity) const
{
	if (Verbosity == ELogVerbosity::All)
	{
		OutInfo = ExpectedMessages.Array();
	}
	else
	{
		OutInfo.Reserve(ExpectedMessages.Num());
		Algo::CopyIf(ExpectedMessages, OutInfo, [Verbosity](const FAutomationExpectedMessage& Message)
		{
			return FAutomationTestBase::LogCategoryMatchesSeverityInclusive(Message.Verbosity, Verbosity);
		});
	}
	OutInfo.Sort();
}

void FAutomationTestBase::AddExpectedError(FString ExpectedErrorPattern, EAutomationExpectedErrorFlags::MatchType InCompareType, int32 Occurrences, bool IsRegex)
{
	// Set verbosity to Warning as it's inclusive, and so checks for both Warnings and Errors
	AddExpectedMessage(MoveTemp(ExpectedErrorPattern), ELogVerbosity::Warning, static_cast<EAutomationExpectedMessageFlags::MatchType>(InCompareType), Occurrences, IsRegex);
}

void FAutomationTestBase::AddExpectedErrorPlain(
	FString ExpectedString,
	EAutomationExpectedErrorFlags::MatchType CompareType,
	int32 Occurrences)
{
	AddExpectedMessagePlain(MoveTemp(ExpectedString), ELogVerbosity::Warning, static_cast<EAutomationExpectedMessageFlags::MatchType>(CompareType), Occurrences);
}

uint32 FAutomationTestBase::ExtractAutomationTestFlags(FString InTagNotation)
{
	uint32 Result = 0;
	TArray<FString> OutputParts;
	InTagNotation
		.Replace(TEXT("["), TEXT(""))
		.Replace(TEXT("]"), TEXT(";"))
		.ParseIntoArray(OutputParts, TEXT(";"), true);
	for (auto it = OutputParts.begin(); it != OutputParts.end(); ++it)
	{
		auto Value = EAutomationTestFlags::FromString(*it);
		if (Value != EAutomationTestFlags::None)
		{
			Result |= Value;
		}
	}
	return Result;
}

void FAutomationTestBase::GenerateTestNames(TArray<FAutomationTestInfo>& TestInfo) const
{
	// This can take a while, particularly as spec tests walk the callstack, so suspend the heartbeat watchdog and hitch detector
	FSlowHeartBeatScope SuspendHeartBeat;
	FDisableHitchDetectorScope SuspendGameThreadHitch;

	TArray<FString> BeautifiedNames;
	TArray<FString> ParameterNames;
	GetTests(BeautifiedNames, ParameterNames);

	FString BeautifiedTestName = GetBeautifiedTestName();

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		FString CompleteBeautifiedNames = BeautifiedTestName;
		FString CompleteTestName = TestName;

		if (ParameterNames[ParameterIndex].Len())
		{
			CompleteBeautifiedNames = FString::Printf(TEXT("%s.%s"), *BeautifiedTestName, *BeautifiedNames[ParameterIndex]);
			CompleteTestName = FString::Printf(TEXT("%s %s"), *TestName, *ParameterNames[ParameterIndex]);
		}

		// Add the test info to our collection
		FAutomationTestInfo NewTestInfo(
			CompleteBeautifiedNames,
			CompleteBeautifiedNames,
			CompleteTestName,
			GetTestFlags(),
			GetRequiredDeviceNum(),
			ParameterNames[ParameterIndex],
			GetTestSourceFileName(CompleteTestName),
			GetTestSourceFileLine(CompleteTestName),
			GetTestAssetPath(ParameterNames[ParameterIndex]),
			GetTestOpenCommand(ParameterNames[ParameterIndex])
		);
		
		TestInfo.Add( NewTestInfo );
	}
}

bool FAutomationTestBase::LogCategoryMatchesSeverityInclusive(
	ELogVerbosity::Type Actual,
	ELogVerbosity::Type MaximumVerbosity)
{
	// Special case for "all", which should always match
	return Actual == ELogVerbosity::All || MaximumVerbosity == ELogVerbosity::All || Actual <= MaximumVerbosity;
}

void FAutomationTestBase::LoadDefaultLogSettings()
{
	GConfig->GetBool(TEXT("/Script/AutomationController.AutomationControllerSettings"), TEXT("bSuppressLogErrors"), bSuppressLogErrors, GEngineIni);
	GConfig->GetBool(TEXT("/Script/AutomationController.AutomationControllerSettings"), TEXT("bSuppressLogWarnings"), bSuppressLogWarnings, GEngineIni);
	GConfig->GetBool(TEXT("/Script/AutomationController.AutomationControllerSettings"), TEXT("bElevateLogWarningsToErrors"), bElevateLogWarningsToErrors, GEngineIni);
	GConfig->GetArray(TEXT("/Script/AutomationController.AutomationControllerSettings"), TEXT("SuppressedLogCategories"), SuppressedLogCategories, GEngineIni);
}

// --------------------------------------------------------------------------------------

bool FAutomationTestBase::TestEqual(const TCHAR* What, const int32 Actual, const int32 Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %d, but it was %d."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const int64 Actual, const int64 Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %" PRId64 ", but it was %" PRId64 "."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}

#if PLATFORM_64BITS
bool FAutomationTestBase::TestEqual(const TCHAR* What, const SIZE_T Actual, const SIZE_T Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %" PRIuPTR ", but it was %" PRIuPTR "."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}
#endif

bool FAutomationTestBase::TestEqual(const TCHAR* What, const float Actual, const float Expected, float Tolerance)
{
	if (!FMath::IsNearlyEqual(Actual, Expected, Tolerance))
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %f, but it was %f within tolerance %f."), What, Expected, Actual, Tolerance), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const double Actual, const double Expected, double Tolerance)
{
	if (!FMath::IsNearlyEqual(Actual, Expected, Tolerance))
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %f, but it was %f within tolerance %f."), What, Expected, Actual, Tolerance), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FVector Actual, const FVector Expected, float Tolerance)
{
	if (!Expected.Equals(Actual, Tolerance))
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FTransform Actual, const FTransform Expected, float Tolerance)
{
	if (!Expected.Equals(Actual, Tolerance))
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FRotator Actual, const FRotator Expected, float Tolerance)
{
	if (!Expected.Equals(Actual, Tolerance))
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FColor Actual, const FColor Expected)
{
	if (Expected != Actual)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), What, *Expected.ToString(), *Actual.ToString()), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FLinearColor Actual, const FLinearColor Expected)
{
	if (Expected != Actual)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), What, *Expected.ToString(), *Actual.ToString()), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
{
	if (FCString::Strcmp(Actual, Expected) != 0)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be \"%s\", but it was \"%s\"."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqualInsensitive(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
{
	if (FCString::Stricmp(Actual, Expected) != 0)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be \"%s\", but it was \"%s\"."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestNotEqualInsensitive(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
{
	if (FCString::Stricmp(Actual, Expected) == 0)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to differ from \"%s\", but it was \"%s\"."), What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestNearlyEqual(const TCHAR* What, const float Actual, const float Expected, float Tolerance)
{
	return TestEqual(What, Actual, Expected, Tolerance);
}

bool FAutomationTestBase::TestNearlyEqual(const TCHAR* What, const double Actual, const double Expected, double Tolerance)
{
	return TestEqual(What, Actual, Expected, Tolerance);
}

bool FAutomationTestBase::TestNearlyEqual(const TCHAR* What, const FVector Actual, const FVector Expected, float Tolerance)
{
	return TestEqual(What, Actual, Expected, Tolerance);
}

bool FAutomationTestBase::TestNearlyEqual(const TCHAR* What, const FTransform Actual, const FTransform Expected, float Tolerance)
{
	return TestEqual(What, Actual, Expected, Tolerance);
}

bool FAutomationTestBase::TestNearlyEqual(const TCHAR* What, const FRotator Actual, const FRotator Expected, float Tolerance)
{
	return TestEqual(What, Actual, Expected, Tolerance);
}

bool FAutomationTestBase::TestFalse(const TCHAR* What, bool Value)
{
	if (Value)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be false."), What), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestTrue(const TCHAR* What, bool Value)
{
	if (!Value)
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be true."), What), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestNull(const TCHAR* What, const void* Pointer)
{
	if ( Pointer != nullptr )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be null."), What), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::IsExpectedMessage(
	const FString& Message,
	const ELogVerbosity::Type& Verbosity)
{
	FReadScopeLock Lock(ActionCS);
	for (FAutomationExpectedMessage& ExpectedMessage : ExpectedMessages)
	{
		// Maintains previous behavior: Adjust so that error and fatal messages are tested against when the input verbosity is "Warning"
		// Similarly, any message above warning should be considered an "info" message
		const ELogVerbosity::Type AdjustedMessageVerbosity =
			ExpectedMessage.Verbosity <= ELogVerbosity::Warning 
			? ELogVerbosity::Warning
			: ELogVerbosity::VeryVerbose;

		// Compare the incoming message verbosity with the expected verbosity,
		if (LogCategoryMatchesSeverityInclusive(Verbosity, AdjustedMessageVerbosity) && ExpectedMessage.Matches(Message))
		{
			return true;
		}
	}

	return false;
}
