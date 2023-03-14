// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProcessUnitTest.h"
#include "Containers/ArrayBuilder.h"
#include "Misc/FeedbackContext.h"

#include "UnitTestManager.h"
#include "UnitTestEnvironment.h"
#include "NUTUtil.h"

#include "UI/SLogWindow.h"
#include "UI/SLogWidget.h"

#include "Internationalization/Regex.h"


/**
 * UProcessUnitTest
 */
UProcessUnitTest::UProcessUnitTest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActiveProcesses()
	, LastBlockingProcessCheck(0)
	, OnSuspendStateChange()
	, ProcessLogWatches()
{
}

void UProcessUnitTest::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	for (int32 i=0; i<ProcessLogWatches.Num(); i++)
	{
		if (ProcessLogWatches[i].Execute(InProcess, InLogLines))
		{
			ProcessLogWatches.RemoveAt(i);
			i--;
		}
	}
}

void UProcessUnitTest::NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState)
{
	if (InProcess.IsValid())
	{
		InProcess.Pin()->SuspendState = InSuspendState;
	}
}


void UProcessUnitTest::CleanupUnitTest(EUnitTestResetStage ResetStage)
{
	if (ResetStage <= EUnitTestResetStage::FullReset)
	{
		for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
		{
			if (ActiveProcesses[i].IsValid())
			{
				ShutdownUnitTestProcess(ActiveProcesses[i]);
			}
		}
	}

	if (ResetStage != EUnitTestResetStage::None)
	{
		if (ResetStage <= EUnitTestResetStage::FullReset)
		{
			ProcessLogWatches.Empty();
		}
	}

	Super::CleanupUnitTest(ResetStage);
}

TWeakPtr<FUnitTestProcess> UProcessUnitTest::StartUnitTestProcess(FString Path, FString InCommandline, bool bMinimized/*=true*/)
{
	TSharedPtr<FUnitTestProcess> ReturnVal = MakeShareable(new FUnitTestProcess());

	verify(FPlatformProcess::CreatePipe(ReturnVal->ReadPipe, ReturnVal->WritePipe));

	UNIT_LOG(ELogType::StatusImportant, TEXT("Starting process '%s' with parameters: %s"), *Path, *InCommandline);

	ReturnVal->ProcessHandle = FPlatformProcess::CreateProc(*Path, *InCommandline, true, bMinimized, false, &ReturnVal->ProcessID,
															0, NULL, ReturnVal->WritePipe);

	if (ReturnVal->ProcessHandle.IsValid())
	{
		ReturnVal->ProcessTag = FString::Printf(TEXT("Process_%i"), ReturnVal->ProcessID);
		ReturnVal->LogPrefix = FString::Printf(TEXT("[PROC%i]"), ReturnVal->ProcessID);

		ActiveProcesses.Add(ReturnVal);
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to start process: %s (%s)"), *Path, *InCommandline);
	}

	return ReturnVal;
}

TWeakPtr<FUnitTestProcess> UProcessUnitTest::StartUEUnitTestProcess(FString InCommandline, bool bMinimized/*=true*/,
																		EBuildTargetType Type/*=EBuildTargetType::Game*/)
{
	TWeakPtr<FUnitTestProcess> ReturnVal = nullptr;
	FString TargetExecutable = FApp::GetName();

#if UE_BUILD_DEVELOPMENT && !WITH_EDITOR
	// Development modes other than Dev Editor, must target the separate Server process
	if (Type == EBuildTargetType::Server)
	{
		TargetExecutable = TargetExecutable.Replace(TEXT("Game"), TEXT("Server"));

		UNIT_LOG(, TEXT("Targeting server process '%s'. If this fails, make sure you compiled Development Server and cooked it in UnrealFrontend."),
					*TargetExecutable);
	}

	FString GamePath = FPaths::Combine(FPlatformMisc::ProjectDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(), TargetExecutable);
#else
	FString GamePath = FPlatformProcess::GenerateApplicationPath(TargetExecutable, FApp::GetBuildConfiguration());
#endif

	ReturnVal = StartUnitTestProcess(GamePath, InCommandline, bMinimized);

	if (ReturnVal.IsValid())
	{
		auto CurHandle = ReturnVal.Pin();

		if (CurHandle->ProcessHandle.IsValid())
		{
			CurHandle->ProcessTag = FString::Printf(TEXT("UE_%i"), CurHandle->ProcessID);
			CurHandle->LogPrefix = FString::Printf(TEXT("[UE%i]"), CurHandle->ProcessID);
		}
	}

	return ReturnVal;
}

void UProcessUnitTest::ShutdownUnitTestProcess(TSharedPtr<FUnitTestProcess> InHandle)
{
	if (InHandle->ProcessHandle.IsValid())
	{
		FString LogMsg = FString::Printf(TEXT("Shutting down process '%s'."), *InHandle->ProcessTag);

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);


		// @todo #JohnBHighPri: Restore 'true' here, once the issue where killing child processes sometimes kills all processes,
		//						is fixed.
		FPlatformProcess::TerminateProc(InHandle->ProcessHandle);//, true);

		FPlatformProcess::CloseProc(InHandle->ProcessHandle);
	}

	FPlatformProcess::ClosePipe(InHandle->ReadPipe, InHandle->WritePipe);
	InHandle->ReadPipe = NULL;
	InHandle->WritePipe = NULL;

	ActiveProcesses.Remove(InHandle);

	// Print out any detected error logs
	if (InHandle->ErrorLogStage != EErrorLogStage::ELS_NoError && InHandle->ErrorText.Num() > 0)
	{
		PrintUnitTestProcessErrors(InHandle);
	}
}

void UProcessUnitTest::PrintUnitTestProcessErrors(TSharedPtr<FUnitTestProcess> InHandle)
{
	FString LogMsg = FString::Printf(TEXT("Detected a crash in process '%s':"), *InHandle->ProcessTag);

	UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
	UNIT_STATUS_LOG(ELogType::StatusImportant, TEXT("%s (click 'Advanced Summary' for more detail)"), *LogMsg);


	uint32 CallstackCount = 0;

	for (auto CurErrorLine : InHandle->ErrorText)
	{
		ELogType CurLogType = ELogType::None;

		if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorStart)
		{
			CurLogType = ELogType::StatusAdvanced;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorDesc)
		{
			CurLogType = ELogType::StatusImportant | ELogType::StatusAdvanced | ELogType::StyleBold;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorCallstack)
		{
			CurLogType = ELogType::StatusAdvanced;

			// Include the first five callstack lines in the main summary printout
			if (CallstackCount < 5)
			{
				CurLogType |= ELogType::StatusImportant;
			}

			CallstackCount++;
		}
		else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorExit)
		{
			continue;
		}
		else
		{
			// Make sure there's a check for all error log stages
			UNIT_ASSERT(false);
		}

		// For the current unit-test log window, all entries are marked 'StatusImportant'
		UNIT_LOG(CurLogType | ELogType::StatusImportant, TEXT("%s"), *CurErrorLine.Line);

		UNIT_STATUS_LOG(CurLogType, TEXT("%s"), *CurErrorLine.Line);
	}
}

void UProcessUnitTest::UpdateProcessStats()
{
	SIZE_T TotalProcessMemoryUsage = 0;

	for (auto CurHandle : ActiveProcesses)
	{
		// Check unit test memory stats, and update if necessary (NOTE: Processes not guaranteed to still be running)
		if (CurHandle.IsValid() && CurHandle->ProcessID != 0)
		{
			SIZE_T ProcessMemUsage = 0;

			if (FPlatformProcess::GetApplicationMemoryUsage(CurHandle->ProcessID, &ProcessMemUsage) && ProcessMemUsage != 0)
			{
				TotalProcessMemoryUsage += ProcessMemUsage;
			}
		}
	}

	if (TotalProcessMemoryUsage > 0)
	{
		CurrentMemoryUsage = TotalProcessMemoryUsage;

		float RunningTime = (float)(FPlatformTime::Seconds() - StartTime);

		// Update saved memory usage stats, to help with tracking estimated memory usage, on future unit test launches
		if (CurrentMemoryUsage > PeakMemoryUsage)
		{
			if (PeakMemoryUsage == 0)
			{
				bFirstTimeStats = true;
			}

			PeakMemoryUsage = CurrentMemoryUsage;

			// Reset TimeToPeakMem
			TimeToPeakMem = RunningTime;

			SaveConfig();
		}
		// Check if we have hit a new record, for the time it takes to get within 90% of PeakMemoryUsage
		else if (RunningTime < TimeToPeakMem && (CurrentMemoryUsage * 100) >= (PeakMemoryUsage * 90))
		{
			TimeToPeakMem = RunningTime;
			SaveConfig();
		}
	}
}

void UProcessUnitTest::CheckOutputForError(TSharedPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLines)
{
	// Perform error parsing, for UE processes
	if (InProcess->ProcessTag.StartsWith(TEXT("UE_")))
	{
		// Array of log messages that can indicate the start of an error
		static const TArray<FString> ErrorStartLogs = TArrayBuilder<FString>()
			.Add(FString(TEXT("Windows GetLastError: ")))
			.Add(FString(TEXT("=== Critical error: ===")));


		for (FString CurLine : InLines)
		{
			// Using 'ContainsByPredicate' as an iterator
			const auto CheckForErrorLog =
				[&CurLine](const FString& ErrorLine)
				{
					return CurLine.Contains(ErrorLine);
				};

			// Search for the beginning of an error log message
			if (InProcess->ErrorLogStage == EErrorLogStage::ELS_NoError)
			{
				if (ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorStart;

					// Reset the timeout for both the unit test and unit test connection here, as callstack logs are prone to failure
					//	(do it for at least two minutes as well, as it can take a long time to get a crashlog)
					ResetTimeout(TEXT("Detected crash."), true, 120);
				}
			}


			if (InProcess->ErrorLogStage != EErrorLogStage::ELS_NoError)
			{
				// Regex pattern for matching callstack logs - matches:
				//	" (0x000007fefe22cacd) + 0 bytes"
				//	" {0x000007fefe22cacd} + 0 bytes"
				const FRegexPattern CallstackPattern(TEXT("\\s[\\(|\\{]0x[0-9,a-f]+[\\)|\\}] \\+ [0-9]+ bytes"));

				// Matches:
				//	"ntldll.dll"
				const FRegexPattern AltCallstackPattern(TEXT("^[a-z,A-Z,0-9,\\-,_]+\\.[exe|dll]"));


				// Check for the beginning of description logs
				if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorStart &&
					!ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorDesc;
				}

				// Check-for/verify callstack logs
				if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorDesc ||
					InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
				{
					FRegexMatcher CallstackMatcher(CallstackPattern, CurLine);
					FRegexMatcher AltCallstackMatcher(AltCallstackPattern, CurLine);

					if (CallstackMatcher.FindNext() || AltCallstackMatcher.FindNext())
					{
						InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorCallstack;
					}
					else if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
					{
						InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorExit;
					}
				}

				// The rest of the lines, after callstack parsing, should be ELS_ErrorExit logs - no need to verify these


				InProcess->ErrorText.Add(FErrorLog(InProcess->ErrorLogStage, CurLine));
			}
		}
	}
}

void UProcessUnitTest::UnitTick(float DeltaTime)
{
	Super::UnitTick(DeltaTime);

	// Detect processes which indicate a blocking task, and reset timeouts while this is occurring
	if ((FPlatformTime::Seconds() - LastBlockingProcessCheck) > 5.0 && IsBlockingProcessPresent(true))
	{
		ResetTimeout(TEXT("Blocking Child Process"), true, 60);

		LastBlockingProcessCheck = FPlatformTime::Seconds();
	}


	PollProcessOutput();

	// Update/save memory stats
	UpdateProcessStats();
}

void UProcessUnitTest::PostUnitTick(float DeltaTime)
{
	Super::PostUnitTick(DeltaTime);

	TArray<TSharedPtr<FUnitTestProcess>> HandlesPendingShutdown;

	for (auto CurHandle : ActiveProcesses)
	{
		if (CurHandle.IsValid() && !FPlatformProcess::IsProcRunning(CurHandle->ProcessHandle))
		{
			FString LogMsg = FString::Printf(TEXT("Process '%s' has finished."), *CurHandle->ProcessTag);

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

			HandlesPendingShutdown.Add(CurHandle);
		}
	}

	for (auto CurProcHandle : HandlesPendingShutdown)
	{
		NotifyProcessFinished(CurProcHandle);
	}


	// Finally, clean up all handles pending shutdown (if they aren't already cleaned up)
	if (HandlesPendingShutdown.Num() > 0)
	{
		for (auto CurHandle : HandlesPendingShutdown)
		{
			if (CurHandle.IsValid())
			{
				ShutdownUnitTestProcess(CurHandle);
			}
		}

		HandlesPendingShutdown.Empty();
	}
}

bool UProcessUnitTest::IsTickable() const
{
	bool bReturnVal = Super::IsTickable();

	bReturnVal = bReturnVal || ActiveProcesses.Num() > 0;

	return bReturnVal;
}


void UProcessUnitTest::PollProcessOutput()
{
	static bool bCheckedForceLogFlush = false;
	static bool bForceLogFlush = false;

	if (!bCheckedForceLogFlush)
	{
		bForceLogFlush = FParse::Param(FCommandLine::Get(), TEXT("ForceLogFlush"));

		bCheckedForceLogFlush = true;
	}

	TMap<TSharedPtr<FUnitTestProcess>, FString> PendingLogDumps;
	TMap<TSharedPtr<FUnitTestProcess>, FString> CompleteLogDumps;

	for (TSharedPtr<FUnitTestProcess> CurHandle : ActiveProcesses)
	{
		PendingLogDumps.Add(CurHandle);
	}

	while (PendingLogDumps.Num() > 0)
	{
		// Split pipe reading and data processing into two steps, to reduce UI blockage due to thread sleeps
		for (TMap<TSharedPtr<FUnitTestProcess>, FString>::TIterator It=PendingLogDumps.CreateIterator(); It; ++It)
		{
			TSharedPtr<FUnitTestProcess> CurHandle = It.Key();
			FString& CurValue = It.Value();

			if (CurHandle.IsValid())
			{
				bool bProcessedPipeRead = false;

				// Need to iteratively grab data, as it has a buffer (can miss data near process-end, for large logs, e.g. stack dumps)
				while (true)
				{
					FString CurStdOut = FPlatformProcess::ReadPipe(CurHandle->ReadPipe);

					if (CurStdOut.Len() > 0)
					{
						CurValue += CurStdOut;
						bProcessedPipeRead = true;
					}
					else
					{
						break;
					}
				}

				// Sometimes large reads (typically > 4096 on the write side) clog the pipe buffer,
				// and even looped reads won't receive it all, so when a large enough amount of data gets read,
				// sleep momentarily (not ideal, but it works - spent a long time trying to find a better solution)
				if (!bProcessedPipeRead || CurValue.Len() < 2048)
				{
					if (CurValue.Len() > 0)
					{
						CompleteLogDumps.Add(CurHandle, CurValue);
					}

					It.RemoveCurrent();
				}
			}
		}

		double PostPipeTime = FPlatformTime::Seconds();


		for (TMap<TSharedPtr<FUnitTestProcess>, FString>::TConstIterator It=CompleteLogDumps.CreateConstIterator(); It; ++It)
		{
			TSharedPtr<FUnitTestProcess> CurHandle = It.Key();
			const FString& CurValue = It.Value();

			if (CurHandle.IsValid() && CurValue.Len() > 0)
			{
				bool bStripErrorLogs = CurHandle->bStripErrorLogs;

				// Every log line should start with an endline, so if one is missing, print that into the log as an error
				bool bPartialRead = !CurValue.StartsWith(FString(LINE_TERMINATOR));
				FString PartialLog = FString::Printf(TEXT("--MISSING ENDLINE - PARTIAL PIPE READ (First 32 chars: %s)--"),
														*CurValue.Left(32));

				// @todo #JohnB: I don't remember why I implemented this with StartsWith, but it worked for ~3-4 years,
				//					and now it is throwing up 'partial pipe read' errors for Fortnite,
				//					and I can't figure out why it worked at all, and why it's not working now.
#if 1
				bPartialRead = !CurValue.EndsWith(FString(LINE_TERMINATOR));
#endif

				// Now split up the log into multiple lines
				TArray<FString> LogLines;
				
				// @todo #JohnB: Perhaps add support for both platforms line terminator, at some stage
				CurValue.ParseIntoArray(LogLines, LINE_TERMINATOR, true);


				// For process logs, replace the timestamp/category with a tag (e.g. [SERVER]), and set a unique colour so it stands out
				ELogTimes::Type bPreviousPrintLogTimes = GPrintLogTimes;
				GPrintLogTimes = ELogTimes::None;

				SET_WARN_COLOR(CurHandle->MainLogColor);

				// Clear the engine-event log hook, to prevent duplication of the below log entry
				UNIT_EVENT_CLEAR;

				// Disable category names for NetCodeTestNone log statements
				bool bOldPrintLogCategory = GPrintLogCategory;
				GPrintLogCategory = false;

				if (bPartialRead)
				{
					UE_LOG(NetCodeTestNone, Log, TEXT("%s"), *PartialLog);
				}

				for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
				{
					FString& CurLine = LogLines[LineIdx];

					if (bStripErrorLogs)
					{
						static const TArray<FString> ErrorStrs = TArrayBuilder<FString>()
							.Add(TEXT("Error: "))
							.Add(TEXT("begin: stack for UAT"));

						// If specified, strip 'Error: ' type entries from the log (e.g. in order to prevent automation test failures)
						for (const FString& CurError : ErrorStrs)
						{
							if (CurLine.Contains(CurError))
							{
								CurLine = CurLine.Replace(*CurError, TEXT(""));
							}
						}
					}

					UE_LOG(NetCodeTestNone, Log, TEXT("%s%s"), *CurHandle->LogPrefix, *CurLine);
				}

				GPrintLogCategory = bOldPrintLogCategory;

				// Output to the unit log file
				if (UnitLog.IsValid())
				{
					if (bPartialRead)
					{
						UnitLog->Serialize(*PartialLog, ELogVerbosity::Log, NAME_None);
					}

					for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
					{
						NUTUtil::SpecialLog(UnitLog.Get(), *CurHandle->LogPrefix, *(LogLines[LineIdx]), ELogVerbosity::Log, NAME_None);
					}
				}

				// Restore the engine-event log hook
				UNIT_EVENT_RESTORE;

				CLEAR_WARN_COLOR();

				GPrintLogTimes = bPreviousPrintLogTimes;


				// Also output to the unit test log window
				if (LogWindow.IsValid())
				{
					TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

					if (LogWidget.IsValid())
					{
						if (bPartialRead)
						{
							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(PartialLog)),
												CurHandle->SlateLogColor);
						}

						for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
						{
							FString LogLine = CurHandle->LogPrefix + LogLines[LineIdx];

							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(LogLine)), CurHandle->SlateLogColor);
						}
					}
				}


				// Now trigger notification of log lines (only if the unit test is not yet verified though)
				if (VerificationState == EUnitTestVerification::Unverified)
				{
					NotifyProcessLog(CurHandle, LogLines);
				}


				// If the verification state has not changed, pass on the log lines to the error checking code
				CheckOutputForError(CurHandle, LogLines);
			}
		}

		CompleteLogDumps.Empty();


		// If any pipe reads are still pending, then we need to sleep for a certain amount of time, to allow pending pipe data to arrive
		if (PendingLogDumps.Num() > 0)
		{
			// Limit blocking pipe sleeps, to 5 seconds per every minute, to limit UI freezes
			static double LastPipeCounterReset = 0.0;
			static double PipeCounter = 0;
			const double PipeDelay = 0.1;
			const double MaxPipeDelay = 5.0;

			double CurTime = FPlatformTime::Seconds();

			if ((CurTime - LastPipeCounterReset) - 60.0 >= 0)
			{
				LastPipeCounterReset = CurTime;
				PipeCounter = 0.0;
			}

			// Offset the pipe delay, by the time it took to process the last batch of log data
			double CurPipeDelay = PipeDelay - (CurTime - PostPipeTime);

			if ((float)CurPipeDelay > 0.f)
			{
				if (MaxPipeDelay - PipeCounter > 0.0)
				{
					FPlatformProcess::SleepNoStats(CurPipeDelay);

					PipeCounter += CurPipeDelay;
				}
				// Do NOT sleep, if -ForceLogFlush is set, as that already triggers a sleep each log!
				else if (!bForceLogFlush)
				{
					// NOTE: This MUST be set to 0.0, because in extreme circumstances (such as DDoS outputting lots of log data),
					//			this can actually block the current thread, due to the sleep being too long
					FPlatformProcess::SleepNoStats(0.0f);
				}
			}
		}
	}
}

bool UProcessUnitTest::IsBlockingProcessPresent(bool bLogIfFound/*=false*/)
{
	bool bReturnVal = false;
	const TArray<FString>* BlockingProcesses = NULL;

	UnitEnv->GetProgressBlockingProcesses(BlockingProcesses);


	// Map all process id's and parent ids, to generate the entire child process tree
	TMap<uint32, uint32> ProcMap;

	{
		FPlatformProcess::FProcEnumerator ProcIt;

		while (ProcIt.MoveNext())
		{
			auto CurProc = ProcIt.GetCurrent();
			ProcMap.Add(CurProc.GetPID(), CurProc.GetParentPID());
		}
	}


	TArray<uint32> ChildProcs;
	ChildProcs.Add(FPlatformProcess::GetCurrentProcessId());

	for (int32 i=0; i<ChildProcs.Num(); i++)
	{
		uint32 SearchParentId = ChildProcs[i];

		for (TMap<uint32, uint32>::TConstIterator It(ProcMap); It; ++It)
		{
			if (It.Value() == SearchParentId)
			{
				ChildProcs.AddUnique(It.Key());
			}
		}
	}


	// Now check if any of those child processes are a blocking process
	TArray<FString> BlockingProcNames;

	{
		FPlatformProcess::FProcEnumerator ProcIt;

		while (ProcIt.MoveNext())
		{
			auto CurProc = ProcIt.GetCurrent();

			if (ChildProcs.Contains(CurProc.GetPID()))
			{
				FString CurProcName = CurProc.GetName();
				int32 Delim = CurProcName.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

				if (Delim != INDEX_NONE)
				{
					CurProcName = CurProcName.Left(Delim);
				}

				if (BlockingProcesses->Contains(CurProcName))
				{
					bReturnVal = true;
					BlockingProcNames.AddUnique(CurProcName);
				}
			}
		}
	}

	if (bLogIfFound && bReturnVal)
	{
		FString ProcString;

		for (auto CurProc : BlockingProcNames)
		{
			if (ProcString.Len() != 0)
			{
				ProcString += TEXT(", ");
			}

			ProcString += CurProc;
		}

		UNIT_LOG(ELogType::StatusImportant, TEXT("Detected blocking child process '%s'."), *ProcString);


		// Longstanding source of issues, with running unit tests - haven't found a good fully-automated solution yet
		if (BlockingProcNames.Contains(TEXT("ShaderCompileWorker")))
		{
			UNIT_LOG(ELogType::StatusImportant,
					TEXT("Recommend generating shaders separately before running unit tests, to avoid bugs and unit test failure."));

			UNIT_LOG(ELogType::StatusImportant,
					TEXT("One way to do this (takes a long time), is through: UnrealEditor-Cmd YourGame -run=DerivedDataCache -fill"));
		}
	}

	return bReturnVal;
}

void UProcessUnitTest::FinishDestroy()
{
	Super::FinishDestroy();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

void UProcessUnitTest::ShutdownAfterError()
{
	Super::ShutdownAfterError();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

