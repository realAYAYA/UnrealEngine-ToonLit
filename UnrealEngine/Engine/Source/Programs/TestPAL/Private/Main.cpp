// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "TestPALLog.h"
#include "Parent.h"
#include "Misc/Guid.h"
#include "Stats/StatsMisc.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericApplication.h"

#include "TestDirectoryWatcher.h"
#include "RequiredProgramMainCPPInclude.h"
#include "HAL/MallocPoisonProxy.h"
#include "HAL/ThreadSafeCounter64.h"

#include "Misc/Fork.h"

DEFINE_LOG_CATEGORY(LogTestPAL);

IMPLEMENT_APPLICATION(TestPAL, "TestPAL");

#define ARG_PROC_TEST						"proc"
#define ARG_PROC_TEST_CHILD					"proc-child"
#define ARG_CASE_SENSITIVITY_TEST			"case"
#define ARG_MESSAGEBOX_TEST					"messagebox"
#define ARG_DIRECTORY_WATCHER_TEST			"dirwatcher"
#define ARG_THREAD_SINGLETON_TEST			"threadsingleton"
#define ARG_SYSINFO_TEST					"sysinfo"
#define ARG_CRASH_TEST						"crash"
#define ARG_ENSURE_TEST						"ensure"
#define ARG_STRINGPRECISION_TEST			"stringprecision"
#define ARG_DSO_TEST						"dso"
#define ARG_GET_ALLOCATION_SIZE_TEST		"getallocationsize"
#define ARG_MALLOC_THREADING_TEST			"mallocthreadtest"
#define ARG_MALLOC_REPLAY					"mallocreplay"
#define ARG_THREAD_PRIO_TEST				"threadpriotest"
#define ARG_INLINE_CALLSTACK_TEST			"inline"
#define ARG_STRINGS_ALLOCATION_TEST			"stringsallocation"
#define ARG_CREATEGUID_TEST					"createguid"
#define ARG_THREADSTACK_TEST				"threadstack"
#define ARG_EXEC_PROCESS_TEST				"exec-process"
#define ARG_FORK_TEST						"fork"
#define ARG_CMDLINE_PARSE_TEST				"cmdline"

namespace TestPAL
{
	FString CommandLine;
};

/**
 * FProcHandle test (child instance)
 */
int32 ProcRunAsChild(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	// set a random delay pretending to do some useful work up to a minute. 
	srand(FPlatformProcess::GetCurrentProcessId());
	double RandomWorkTime = FMath::FRandRange(0.0f, 6.0f);

	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as child (pid %d), will be doing work for %f seconds."), FPlatformProcess::GetCurrentProcessId(), RandomWorkTime);

	double StartTime = FPlatformTime::Seconds();

	// Use all the CPU!
	for (;;)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - StartTime >= RandomWorkTime)
		{
			break;
		}
	}

	UE_LOG(LogTestPAL, Display, TEXT("Child (pid %d) finished work."), FPlatformProcess::GetCurrentProcessId());

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * FProcHandle test (parent instance)
 */
int32 ProcRunAsParent(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as parent."));

	// Run child instance continuously
	int NumChildrenToSpawn = 255, MaxAtOnce = 5;
	FParent Parent(NumChildrenToSpawn, MaxAtOnce);

	Parent.Run();

	UE_LOG(LogTestPAL, Display, TEXT("Parent quit."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Tests a single file.
 */
void TestCaseInsensitiveFile(const FString & Filename, const FString & WrongFilename)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	IFileHandle * CreationHandle = PlatformFile.OpenWrite(*Filename);
	checkf(CreationHandle, TEXT("Could not create a test file for '%s'"), *Filename);
	delete CreationHandle;
	
	IFileHandle* CheckGoodHandle = PlatformFile.OpenRead(*Filename);
	checkf(CheckGoodHandle, TEXT("Could not open a test file for '%s' (zero probe)"), *Filename);
	delete CheckGoodHandle;
	
	IFileHandle* CheckWrongCaseRelHandle = PlatformFile.OpenRead(*WrongFilename);
	checkf(CheckWrongCaseRelHandle, TEXT("Could not open a test file for '%s'"), *WrongFilename);
	delete CheckWrongCaseRelHandle;
	
	PlatformFile.DeleteFile(*Filename);
}

/**
 * Case-(in)sensitivity test/
 */
int32 CaseTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running case sensitivity test."));
	
	TestCaseInsensitiveFile(TEXT("Test.Test"), TEXT("teSt.teSt"));
	
	FString File(TEXT("Test^%!CaseInsens"));
	FString AbsFile = FPaths::ConvertRelativePathToFull(File);
	FString AbsFileUpper = AbsFile.ToUpper();

	TestCaseInsensitiveFile(AbsFile, AbsFileUpper);
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Message box test/
 */
int32 MessageBoxTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running message box test."));
	
	FString Display(TEXT("I am a big big string in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong...  or not."));
	FString Caption(TEXT("I am a big big caption in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong... or not."));
	EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Display, *Caption);

	UE_LOG(LogTestPAL, Display, TEXT("MessageBoxExt result: %d."), static_cast<int32>(Result));
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * ************  Thread singleton test *****************
 */

/**
 * Per-thread singleton
 */
struct FPerThreadTestSingleton : public TThreadSingleton<FPerThreadTestSingleton>
{
	FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (this=%p) created for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}

	void DoSomething()
	{
		UE_LOG(LogTestPAL, Log, TEXT("Thread %d is about to quit"), FPlatformTLS::GetCurrentThreadId());
	}

	virtual ~FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (%p) destroyed for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}
};

//DECLARE_THREAD_SINGLETON( FPerThreadTestSingleton );

/**
 * Thread runnable
 */
struct FSingletonTestingThread : public FRunnable
{
	virtual uint32 Run()
	{
		FPerThreadTestSingleton& Dummy = FPerThreadTestSingleton::Get();

		FPlatformProcess::Sleep(3.0f);

		Dummy.DoSomething();
		return 0;
	}
};

/**
 * Thread singleton test
 */
int32 ThreadSingletonTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running thread singleton test."));

	const int kNumTestThreads = 10;

	FSingletonTestingThread * RunnableArray[kNumTestThreads] = { nullptr };
	FRunnableThread * ThreadArray[kNumTestThreads] = { nullptr };

	// start all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		RunnableArray[Idx] = new FSingletonTestingThread();
		ThreadArray[Idx] = FRunnableThread::Create(RunnableArray[Idx],
			*FString::Printf(TEXT("TestThread%d"), Idx));
	}

	GLog->FlushThreadedLogs();
	GLog->Flush();

	// join all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		ThreadArray[Idx]->WaitForCompletion();
		delete ThreadArray[Idx];
		ThreadArray[Idx] = nullptr;
		delete RunnableArray[Idx];
		RunnableArray[Idx] = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

#if PLATFORM_LINUX

static float ToMB(uint64 Value)
{
	return Value * (1.0 / (1024.0 * 1024.0));
}

#endif // PLATFORM_LINUX

/**
 * Sysinfo test
 */
int32 SysInfoTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running system info test."));

	bool bIsRunningOnBattery = FPlatformMisc::IsRunningOnBattery();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsRunningOnBattery() = %s"), bIsRunningOnBattery ? TEXT("true") : TEXT("false"));

	GenericApplication * PlatformApplication = FPlatformApplicationMisc::CreateApplication();
	checkf(PlatformApplication, TEXT("Could not create platform application!"));
	bool bIsMouseAttached = PlatformApplication->IsMouseAttached();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsMouseAttached() = %s"), bIsMouseAttached ? TEXT("true") : TEXT("false"));

	FString OSInstanceGuid = FPlatformMisc::GetOperatingSystemId();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::GetOperatingSystemId() = '%s'"), *OSInstanceGuid);

	FString UserDir = FPlatformProcess::UserDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformProcess::UserDir() = '%s'"), *UserDir);

	FString UserHomeDir = FPlatformProcess::UserHomeDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformProcess::UserHomeDir() = '%s'"), *UserHomeDir);

	FString UserTempDir = FPlatformProcess::UserTempDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformProcess::UserTempDir() = '%s'"), *UserTempDir);

	FString ApplicationSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformProcess::ApplicationSettingsDir() = '%s'"), *ApplicationSettingsDir);

	FString ApplicationCurrentWorkingDir = FPlatformProcess::GetCurrentWorkingDirectory();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformProcess::GetCurrentWorkingDirectory() = '%s'"), *ApplicationCurrentWorkingDir);

	FPlatformMemory::DumpStats(*GLog);

#if PLATFORM_LINUX
	FExtendedPlatformMemoryStats StatsEx = FUnixPlatformMemory::GetExtendedStats();

	UE_LOG(LogTestPAL, Display, TEXT("Shared_Clean:%.2f MB, Shared_Dirty:%.2f MB"),
		ToMB(StatsEx.Shared_Clean), ToMB(StatsEx.Shared_Dirty));
	UE_LOG(LogTestPAL, Display, TEXT("Private_Clean:%.2fMB Private_Dirty:%.2fMB"),
		ToMB(StatsEx.Private_Clean), ToMB(StatsEx.Private_Dirty));
#endif // PLATFORM_LINUX

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Crash test
 */
int32 CrashTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running crash test (this should not exit)."));

	if (FParse::Param(CommandLine, TEXT("logfatal")))
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("  LogFatal!"));
	}
	else if (FParse::Param(CommandLine, TEXT("check")))
	{
		checkf(false, TEXT("  checkf!"));
	}
	else if (FParse::Param(CommandLine, TEXT("ensure")))
	{
		ensureMsgf(false, TEXT("  ensureMsgf!"));
	}
	else if (FParse::Param(CommandLine, TEXT("unaligned")))
	{
		FQuat Quat[2];
		uint8* BytePtr = (uint8*) &Quat;
		FQuat* QuatBadPtr = (FQuat*)(BytePtr + 3);
		*QuatBadPtr = FQuat::Identity;

		// This never going to be called, but kept here so entire block is not optimized out
		UE_LOG(LogTestPAL, Warning, TEXT("Quat.X = %f"), Quat[0].X);
	}
	else
	{
		*(int *)0x10 = 0x11;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Ensure test
 */
int32 EnsureTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running ensure test."));

	// try ensures first (each ensure fires just once)
	{
		UE_LOG(LogTestPAL, Display, TEXT("Trying 5 ensureAlways 5 times."));
		UE_LOG(LogTestPAL, Display, TEXT("-------------------------------------------------------------------------"));
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			{
				UE_LOG(LogTestPAL, Display, TEXT("*************** FIRST ensureAlways #%d time ***************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIRST ensureAlways() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensureAlways(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("*************** SECOND ensureAlways #%d time ***************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SECOND ensureAlways() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensureAlways(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("*************** THIRD ensureAlways #%d time ***************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled THIRD ensureAlways() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensureAlways(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("*************** FOURTH ensureAlways #%d time ***************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FOURTH ensureAlways() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensureAlways(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("*************** FIFTH ensureAlways #%d time ***************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIFTH ensureAlways() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensureAlways(false);
			}
		}

		UE_LOG(LogTestPAL, Display, TEXT("Trying 10 ensures 5 times."));
		UE_LOG(LogTestPAL, Display, TEXT("-------------------------------------------------------------------------"));
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** FIRST ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIRST ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** SECOND ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SECOND ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** THIRD ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled THIRD ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** FOURTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FOURTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** FIFTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIFTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** SIXTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SIXTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** SEVENTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SEVENTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** EIGTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled EIGHTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** NINETH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled NINETH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

			{
				UE_LOG(LogTestPAL, Display, TEXT("****************** TENTH ensure #%d time ******************"), IdxEnsure);
				FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled TENTH ensure() #%d time"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
				ensure(false);
			}

		}

	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * String Precision test
 */
int32 StringPrecisionTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running string precision test."));

	const FString TestString(TEXT("TestString"));
	int32 Indent = 15;
	UE_LOG(LogTestPAL, Display, TEXT("%*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("Begining of the line %*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("%*s end of the line"), Indent, *TestString);

	int Width = 2;
	for (uint32 Idx = 0; Idx < 10; ++Idx)
	{
		UE_LOG(LogTestPAL, Display, TEXT("DynSize: %d SignedDynFormat%0*d"), Width, Width, Idx);
		UE_LOG(LogTestPAL, Display, TEXT("DynSize: %d Size_tDynFormat%0*zd"), Width, Width, static_cast<size_t>(Idx));
		++Width;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Test Push/PopDll
 */
int32 DynamicLibraryTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Attempting to load Steam library"));

	FString RootSteamPath;
	FString LibraryName;

	if (PLATFORM_LINUX)
	{
		RootSteamPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Steamworks/*"));

		IFileManager& PlatformFileManager = IFileManager::Get();
		TArray<FString> FoundDirectories;
		PlatformFileManager.FindFiles(FoundDirectories, *RootSteamPath, false, true);

		// Just use the first directory we find, this test does not have to be very sophisticated.
		if (FoundDirectories.Num() > 0)
		{
			// This only gives us directories, so remove the wildcard from our initial search
			RootSteamPath.RemoveFromEnd(TEXT("*"));
			// And append the directory name we found.
			RootSteamPath += FoundDirectories[0];
		}
		else
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("Could not find any steam versions."));
		}

		RootSteamPath += TEXT("/x86_64-unknown-linux-gnu/");
		LibraryName = TEXT("libsteam_api.so");
	}
	else
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("This test is not implemented for this platform."));
	}

	FPlatformProcess::PushDllDirectory(*RootSteamPath);
	void* SteamDLLHandle = FPlatformProcess::GetDllHandle(*LibraryName);
	FPlatformProcess::PopDllDirectory(*RootSteamPath);

	if (SteamDLLHandle == nullptr)
	{
		// try bundled one
		UE_LOG(LogTestPAL, Error, TEXT("Could not load via Push/PopDll, loading directly."));
		SteamDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + LibraryName));

		if (SteamDLLHandle == nullptr)
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("Could not load Steam library!"));
		}
	}

	if (SteamDLLHandle)
	{
		UE_LOG(LogTestPAL, Log, TEXT("Loaded Steam library at %p"), SteamDLLHandle);
		FPlatformProcess::FreeDllHandle(SteamDLLHandle);
		SteamDLLHandle = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}



/**
 * FMalloc::GetAllocationSize() test
 */
int32 GetAllocationSizeTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running GMalloc->GetAllocationSize() test."));

	struct Allocation
	{
		void *		Memory;
		SIZE_T		RequestedSize;
		SIZE_T		Alignment;
		SIZE_T		ActualSize;
	};

	TArray<Allocation> Allocs;
	SIZE_T TotalMemoryRequested = 0, TotalMemoryAllocated = 0;

	// force proxy malloc
	FMalloc* OldGMalloc = GMalloc;
	GMalloc = new FMallocPoisonProxy(GMalloc);

	// allocate all the memory and initialize with 0
	for (uint32 Size = 16; Size < 4096; Size += 16)
	{
		for (SIZE_T AlignmentPower = 4; AlignmentPower <= 7; ++AlignmentPower)
		{
			SIZE_T Alignment = ((SIZE_T)1 << AlignmentPower);

			Allocation New;
			New.RequestedSize = Size;
			New.Alignment = Alignment;
			New.Memory = GMalloc->Malloc(New.RequestedSize, New.Alignment);
			if (!GMalloc->GetAllocationSize(New.Memory, New.ActualSize))
			{
				UE_LOG(LogTestPAL, Fatal, TEXT("Could not get allocation size for %p"), New.Memory);
			}
			FMemory::Memzero(New.Memory, New.RequestedSize);

			TotalMemoryRequested += New.RequestedSize;
			TotalMemoryAllocated += New.ActualSize;

			Allocs.Add(New);
		}
	}

	UE_LOG(LogTestPAL, Log, TEXT("Allocated %llu memory (%llu requested) in %d chunks"), TotalMemoryAllocated, TotalMemoryRequested, Allocs.Num());

	if (FParse::Param(CommandLine, TEXT("realloc")))
	{
		for (Allocation & Alloc : Allocs)
		{
			// resize
			Alloc.RequestedSize += 16;
			Alloc.Memory = GMalloc->Realloc(Alloc.Memory, Alloc.RequestedSize, Alloc.Alignment);
			FMemory::Memzero(Alloc.Memory, Alloc.RequestedSize);
		}
	}
	else
	{
		for (Allocation & Alloc : Allocs)
		{
			// only fill the difference, if any
			if (Alloc.ActualSize > Alloc.RequestedSize)
			{
				SIZE_T Difference = Alloc.ActualSize - Alloc.RequestedSize;
				FMemory::Memset((void *)((SIZE_T)Alloc.Memory + Alloc.RequestedSize), 0xAA, Difference);
			}
		}
	}

	// check if any alloc got stomped
	for (Allocation & Alloc : Allocs)
	{
		for (SIZE_T Idx = 0; Idx < Alloc.RequestedSize; ++Idx)
		{
			if (((const uint8 *)Alloc.Memory)[Idx] != 0)
			{
				UE_LOG(LogTestPAL, Fatal, TEXT("Allocation at %p (offset %llu) got stomped with 0x%x"),
					Alloc.Memory, Idx, ((const uint8 *)Alloc.Memory)[Idx]
					);
			}
		}
	}

	UE_LOG(LogTestPAL, Log, TEXT("No memory stomping detected"));

	for (Allocation & Alloc : Allocs)
	{
		GMalloc->Free(Alloc.Memory);
	}
	GMalloc = OldGMalloc;

	Allocs.Empty();

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Strings allocation test
 */
int32 StringsAllocationTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running StringsAllocationTest test."));

	const int32 NumOfStrings = 1000000;
	const TCHAR* SampleText = TEXT("Lorem ipsum dolor sit amet");

	FString** Strings = new FString*[NumOfStrings];

	UE_LOG(LogTestPAL, Display, TEXT("Allocating %u strings '%s'"), NumOfStrings, SampleText);

	for(int32 i = 0; i < NumOfStrings; ++i)
	{
		Strings[i] = new FString(SampleText);
	}

	if (GWarn)
	{
		GMalloc->DumpAllocatorStats(*GWarn);
	}

	for(int32 i = 0; i < NumOfStrings; ++i)
	{
		delete Strings[i];
	}
	delete [] Strings;

	// GMalloc = OldGMalloc;
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Thread runnable
 * bUseGenericMisc Whether to use FGenericPlatformMisc CreateGuid
 */
template<bool bUseGenericMisc>
struct FCreateGuidThread : public FRunnable
{
	/** Number of CreateGuid calls to make. */
	int32	NumCalls;

	FCreateGuidThread(int32 InNumCalls)
		:	FRunnable()
		,	NumCalls(InNumCalls)
	{
	}

	virtual uint32 Run()
	{
		FGuid Result;

		for (int32 IdxRun = 0; IdxRun < NumCalls; ++IdxRun)
		{
			if (bUseGenericMisc)
			{
				FGenericPlatformMisc::CreateGuid(Result);
			}
			else
			{
				FPlatformMisc::CreateGuid(Result);
			}
		}

		return 0;
	}
};


/**
 * CreateGuid test
 */
int32 CreateGuidTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running CreateGuid test."));

	TArray<FRunnable*> RunnableArray;
	TArray<FRunnableThread*> ThreadArray;

	UE_LOG(LogTestPAL, Display, TEXT("Accepted options:"));
	UE_LOG(LogTestPAL, Display, TEXT("    -numthreads=N"));
	UE_LOG(LogTestPAL, Display, TEXT("    -numcalls=N (how many times each thread will call CreateGuid)"));
	UE_LOG(LogTestPAL, Display, TEXT("    -norandomguids (disable SYS_getrandom syscall)"));

	int32 NumTestThreads = 4, NumCalls = 1000000;
	if (FParse::Value(CommandLine, TEXT("numthreads="), NumTestThreads))
	{
		NumTestThreads = FMath::Max(1, NumTestThreads);
	}
	if (FParse::Value(CommandLine, TEXT("numcalls="), NumCalls))
	{
		NumCalls = FMath::Max(1, NumCalls);
	}

	// start all threads
	for (int Idx = 0; Idx < 2; ++Idx )
	{
		bool bUseGenericMisc = (Idx == 0);
		double WallTimeDuration = FPlatformTime::Seconds();

		for (int IdxThread = 0; IdxThread < NumTestThreads; ++IdxThread)
		{
			RunnableArray.Add(bUseGenericMisc ?
				static_cast<FRunnable*>(new FCreateGuidThread<true>(NumCalls)) :
				static_cast<FRunnable*>(new FCreateGuidThread<false>(NumCalls)) );

			ThreadArray.Add( FRunnableThread::Create(RunnableArray[IdxThread],
				*FString::Printf(TEXT("GuidTest%d"), IdxThread)) );
		}

		GLog->FlushThreadedLogs();
		GLog->Flush();

		// join all threads
		for (int IdxThread = 0; IdxThread < NumTestThreads; ++IdxThread)
		{
			ThreadArray[IdxThread]->WaitForCompletion();

			delete ThreadArray[IdxThread];
			ThreadArray[IdxThread] = nullptr;

			delete RunnableArray[IdxThread];
			RunnableArray[IdxThread] = nullptr;
		}

		WallTimeDuration = FPlatformTime::Seconds() - WallTimeDuration;

		UE_LOG(LogTestPAL, Display, TEXT("--- Results for %s ---"),
			bUseGenericMisc ? TEXT("FGenericPlatformMisc::CreateGuid") : TEXT("FPlatformMisc::CreateGuid"));
		UE_LOG(LogTestPAL, Display, TEXT("Total wall time: %f seconds, Threads: %d, Calls: %d"),
				WallTimeDuration, NumTestThreads, NumCalls);

		ThreadArray.Empty();
		RunnableArray.Empty();
	}

	// Spew out a small sample of GUIDs to visually check we haven't horribly broken something
	UE_LOG(LogTestPAL, Display, TEXT("--- CreateGuid Samples ---"));

	for (int Idx = 0; Idx < 20; ++Idx)
	{
		FGuid GuidPlatform;
		FGuid GuidGeneric;

		FPlatformMisc::CreateGuid(GuidPlatform);
		FGenericPlatformMisc::CreateGuid(GuidGeneric);

		UE_LOG(LogTestPAL, Display, TEXT("%3d GuidGeneric:%s GuidPlatform:%s"), Idx,
			*GuidGeneric.ToString(EGuidFormats::DigitsWithHyphensInBraces),
			*GuidPlatform.ToString(EGuidFormats::DigitsWithHyphensInBraces));
	}

	// GMalloc = OldGMalloc;
	FEngineLoop::AppExit();
	return 0;
}


/** An ugly way to pass a parameters to FRunnable; shouldn't matter for this test code. */
int32 GMallocTestNumRuns = 500;
int32 GMallocTestMemoryPerThreadKB = 64*1024;	// 64 MB
bool GMallocTestTouchTMemory = false;

FThreadSafeCounter64 GTotalAllocsDone;
FThreadSafeCounter64 GTotalMemoryAllocated;

/**
 * Thread runnable
 * bUseSystemMalloc Whether to use system malloc for speed comparison.
 */
template<bool bUseSystemMalloc>
struct FMemoryAllocatingThread : public FRunnable
{
	virtual uint32 Run()
	{
		void* Ptrs[8192];

		for (int32 IdxRun = 0; IdxRun < GMallocTestNumRuns; ++IdxRun)
		{
			FMemory::Memzero(Ptrs);

			int32 NumAllocs = 0;
			uint64 MemAllocatedThisRun = 0;

			while(MemAllocatedThisRun < GMallocTestMemoryPerThreadKB * 1024 && NumAllocs < UE_ARRAY_COUNT(Ptrs))
			{
				// increase the probability of the smallest chunks, but allow fairly large (up to 16 MB)
				double Uniform = FMath::FRand();
				double Asymptote = (Uniform >= DBL_EPSILON) ? (1 / Uniform) : 1.0;
				double SkewedTowardsSmall = FMath::Min(1.0, Asymptote / 4096.0);	// 4096 is an arbitrary constant

				const int32 ChunkSize = 1 + static_cast<int32>(16384.0 * 1024.0 * SkewedTowardsSmall);
				//printf("Uniform: %f, Asymptote: %f, SkewedTowardsSmall: %f, ChunkSize: %d bytes\n", Uniform, Asymptote, SkewedTowardsSmall, ChunkSize);
				//fflush(stdout);

				void* Ptr = nullptr;
				if (bUseSystemMalloc)
				{
					Ptr = malloc(ChunkSize);
				}
				else
				{
					Ptr = FMemory::Malloc(ChunkSize);
				}

				if (LIKELY(Ptr))
				{
					Ptrs[NumAllocs++] = Ptr;
					MemAllocatedThisRun += ChunkSize;

					// touch the memory if not measuring the speed
					if (UNLIKELY(GMallocTestTouchTMemory))
					{
						FMemory::Memset(Ptr, 0xff, ChunkSize);
					}
				}
				else
				{
					break;
				}
			}

			//UE_LOG(LogTestPAL, Display, TEXT("NumAllocs = %d, MemAllocatedThisRun=%llu"), NumAllocs, MemAllocatedThisRun);
			GTotalAllocsDone.Add(NumAllocs);
			GTotalMemoryAllocated.Add(MemAllocatedThisRun);

			// allocate in somewhat atypical order to make it harder for malloc
			// (reinterpret Ptrs table as a 2D array of kAllocFieldWidth x NumAllocH allocs)
			const int32 kAllocFieldWidth = 4;
			int32 NumAllocsH = (NumAllocs / kAllocFieldWidth) + 1;
			for (int32 IdxAllocX = kAllocFieldWidth - 1; IdxAllocX >= 0; --IdxAllocX)
			{
				for (int32 IdxAllocY = 0; IdxAllocY < NumAllocsH; ++IdxAllocY)
				{
					int32 IdxAlloc = IdxAllocY * kAllocFieldWidth + IdxAllocX;
					if (LIKELY(IdxAlloc < NumAllocs))
					{
						void* Ptr = Ptrs[IdxAlloc];
						if (LIKELY(Ptr))
						{
							if (bUseSystemMalloc)
							{
								free(Ptr);
							}
							else
							{
								FMemory::Free(Ptr);
							}
						}
					}
				}
			}
		}
		return 0;
	}
};

/**
 * Malloc threading test
 */
int32 MallocThreadingTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	TArray<FRunnable*> RunnableArray;
	TArray<FRunnableThread*> ThreadArray;

	UE_LOG(LogTestPAL, Display, TEXT("Accepted options:"));
	UE_LOG(LogTestPAL, Display, TEXT("    -systemmalloc to use system malloc"));
	UE_LOG(LogTestPAL, Display, TEXT("    -numthreads=N"));
	UE_LOG(LogTestPAL, Display, TEXT("    -memperthread=N (how much memory each thread will allocate in KB)"));
	UE_LOG(LogTestPAL, Display, TEXT("    -numruns=N (how many times the thread will allocate and free that memory)"));
	UE_LOG(LogTestPAL, Display, TEXT("    -touchmem (touch the memory to make sure it's backed by physical RAM - this can dominate the execution time)"));

	bool bUseSystemMalloc = FParse::Param(CommandLine, TEXT("systemmalloc"));
	int32 NumTestThreads = 4, InNumTestThreads = 0, InNumRuns = 0, InMemPerThreadKB = 0;
	if (FParse::Value(CommandLine, TEXT("numthreads="), InNumTestThreads))
	{
		NumTestThreads = FMath::Max(1, InNumTestThreads);
	}
	if (FParse::Value(CommandLine, TEXT("numruns="), InNumRuns))
	{
		GMallocTestNumRuns = FMath::Max(1, InNumRuns);
	}
	if (FParse::Value(CommandLine, TEXT("memperthread="), InMemPerThreadKB))
	{
		GMallocTestMemoryPerThreadKB = FMath::Max(0, InMemPerThreadKB);
	}
	if (FParse::Param(CommandLine, TEXT("touchmem")))
	{
		GMallocTestTouchTMemory = true;
	}

	UE_LOG(LogTestPAL, Display, TEXT("Running malloc threading test using %s malloc and %d threads, each allocating %sup to %d KB (%d MB) memory %d times."),
		bUseSystemMalloc ? TEXT("libc") : GMalloc->GetDescriptiveName(),
		NumTestThreads,
		GMallocTestTouchTMemory ? TEXT("and touching ") : TEXT(""),
		GMallocTestMemoryPerThreadKB, GMallocTestMemoryPerThreadKB / 1024,
		GMallocTestNumRuns
		);

	// start all threads
	double WallTimeDuration = 0.0;
	{
		FSimpleScopeSecondsCounter Duration(WallTimeDuration);
		for (int Idx = 0; Idx < NumTestThreads; ++Idx)
		{
			RunnableArray.Add( bUseSystemMalloc ? static_cast<FRunnable*>(new FMemoryAllocatingThread<true>()) : static_cast<FRunnable*>(new FMemoryAllocatingThread<false>()) );
			ThreadArray.Add( FRunnableThread::Create(RunnableArray[Idx],
				*FString::Printf(TEXT("MallocTest%d"), Idx)) );
		}

		GLog->FlushThreadedLogs();
		GLog->Flush();

		// join all threads
		for (int Idx = 0; Idx < NumTestThreads; ++Idx)
		{
			ThreadArray[Idx]->WaitForCompletion();
			delete ThreadArray[Idx];
			ThreadArray[Idx] = nullptr;
			delete RunnableArray[Idx];
			RunnableArray[Idx] = nullptr;
		}
	}
	UE_LOG(LogTestPAL, Display, TEXT("--- Results for %s malloc ---"), bUseSystemMalloc ? TEXT("libc") : GMalloc->GetDescriptiveName());
	UE_LOG(LogTestPAL, Display, TEXT("Total wall time:        %f seconds"), WallTimeDuration);
	UE_LOG(LogTestPAL, Display, TEXT("Total allocations done: %llu"), GTotalAllocsDone.GetValue());
	UE_LOG(LogTestPAL, Display, TEXT("Total memory allocated: %llu bytes (%llu KB, %llu MB)"), GTotalMemoryAllocated.GetValue(),
		GTotalMemoryAllocated.GetValue() / 1024, GTotalMemoryAllocated.GetValue() / (1024 * 1024));

	double AllocsPerSecond = (WallTimeDuration > 0.0) ? GTotalAllocsDone.GetValue() / WallTimeDuration : 0.0;
	double BytesPerSecond = (WallTimeDuration > 0.0) ? GTotalMemoryAllocated.GetValue() / WallTimeDuration : 0.0;

	UE_LOG(LogTestPAL, Display, TEXT("Speed in allocs:        %.1f Kallocs/sec (%.1f allocs/sec)%s"), AllocsPerSecond / 1000.0, AllocsPerSecond,
		GMallocTestTouchTMemory ? TEXT("- less relevant since memory was touched") : TEXT(""));
	UE_LOG(LogTestPAL, Display, TEXT("Speed in bytes:         %.1f MB/sec (%.1f KB/sec, %.1f bytes/sec)%s"), BytesPerSecond / (1024.0 * 1024.0), BytesPerSecond / 1024.0, BytesPerSecond,
		GMallocTestTouchTMemory ? TEXT("- less relevant since memory is touched") : TEXT(""));

	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	UE_LOG(LogTestPAL, Display, TEXT("Peak used resident RAM: %llu MB (%llu KB, %llu bytes)"), Stats.PeakUsedPhysical / (1024 * 1024), Stats.PeakUsedPhysical / 1024, Stats.PeakUsedPhysical);
	UE_LOG(LogTestPAL, Display, TEXT("Peak used virtual RAM:  %llu MB (%llu KB, %llu bytes)"), Stats.PeakUsedVirtual / (1024 * 1024), Stats.PeakUsedVirtual / 1024, Stats.PeakUsedVirtual);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Replays a malloc save file, streaming it from the disk. Waits for Ctrl-C until exiting.
 * 
 * @param ReplayFileName file name to read. Must be reachable from cwd (current working directory) or absolute
 * @param OperationToStopAfter number of operation to stop on - no further reads will be done. Useful to compare peak usage in the middle of file.
 * @param bSuppressErrors Whether to print errors
 */
void ReplayMallocFile(const FString& ReplayFileName, uint64 OperationToStopAfter, bool bSuppressErrors)
{
#if PLATFORM_WINDOWS
	FILE* ReplayFile = nullptr;
	if (fopen_s(&ReplayFile, TCHAR_TO_UTF8(*ReplayFileName), "rb") != 0 || ReplayFile == nullptr)
#else
	FILE* ReplayFile = fopen(TCHAR_TO_UTF8(*ReplayFileName), "rb");
	if (ReplayFile == nullptr)
#endif // PLATFORM_WINDOWS
	{
		return;
	}

	// For file format, see FMallocReplayProxy

	// skip the first line since it contains column headers
	{
		char DummyBuffer[4096];
		fgets(DummyBuffer, sizeof(DummyBuffer), ReplayFile);
	}

	double WallTimeDuration = 0.0;

	uint64 OperationNumber = 0;
	TMap<uint64, void *> FilePointerToRamPointers;
	for(;;)
	{
		FSimpleScopeSecondsCounter Duration(WallTimeDuration);

		char OpBuffer[128] = {0};
		uint64 PtrOut, PtrIn, Size, Ordinal;
		uint32 Alignment;
#if PLATFORM_WINDOWS
		if (fscanf_s(ReplayFile, "%s %llu %llu %llu %u\t# %llu\n", OpBuffer, static_cast<unsigned int>(sizeof(OpBuffer)), &PtrOut, &PtrIn, &Size, &Alignment, &Ordinal) != 6)
#else
		if (fscanf(ReplayFile, "%s %llu %llu %llu %u\t# %llu\n", OpBuffer, &PtrOut, &PtrIn, &Size, &Alignment, &Ordinal) != 6)
#endif // PLATFORM_WINDOWS
		{
			UE_LOG(LogTestPAL, Display, TEXT("Hit end of the replay file on %llu-th operation."), OperationNumber);
			break;
		}

		if (!FCStringAnsi::Strcmp(OpBuffer, "Malloc"))
		{
			if (FilePointerToRamPointers.Find(PtrOut) == nullptr)
			{
				void* Result = FMemory::Malloc(Size, Alignment);
				FilePointerToRamPointers.Add(PtrOut, Result);
			}
			else if (!bSuppressErrors)
			{
				UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu that returned pointer %llu, which was already allocated at that moment. Skipping."), Ordinal, PtrOut);
			}
		}
		else if (!FCStringAnsi::Strcmp(OpBuffer, "Realloc"))
		{
			void* PtrToRealloc = nullptr;
			if (PtrIn != 0)
			{
				void** RamPointer = FilePointerToRamPointers.Find(PtrIn);
				if (RamPointer != nullptr)
				{
					PtrToRealloc = *RamPointer;
				}
				else if (!bSuppressErrors)
				{
					UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu to reallocate pointer %llu, which was not allocated at that moment. Substituting with nullptr."), Ordinal, PtrIn);
				}
			}

			void* Result = FMemory::Realloc(PtrToRealloc, Size, Alignment);
			FilePointerToRamPointers.Remove(PtrIn);
			FilePointerToRamPointers.Add(PtrOut, Result);
		}
		else if (!FCStringAnsi::Strcmp(OpBuffer, "Free"))
		{
			void* PtrToFree = nullptr;
			if (PtrIn != 0)
			{
				void** RamPointer = FilePointerToRamPointers.Find(PtrIn);
				if (RamPointer != nullptr)
				{
					PtrToFree = *RamPointer;
				}
				else if (!bSuppressErrors)
				{
					UE_LOG(LogTestPAL, Error, TEXT("Replay file contains operation # %llu to free pointer %llu, which was not allocated at that moment. Substituting with nullptr."), Ordinal, PtrIn);
				}
			}

			FMemory::Free(PtrToFree);
			FilePointerToRamPointers.Remove(PtrIn);
		}
		else if (!bSuppressErrors)
		{
			UE_LOG(LogTestPAL, Error, TEXT("Replay file contains unknown operation '%s', skipping."), ANSI_TO_TCHAR(OpBuffer));
		}

		++OperationNumber;
		if (OperationNumber >= OperationToStopAfter)
		{
			UE_LOG(LogTestPAL, Display, TEXT("Stopping after %llu-th operation."), OperationNumber);
			break;
		}
	}

	UE_LOG(LogTestPAL, Display, TEXT("Replayed %llu operations in %f seconds, waiting for Ctrl-C to proceed further. You can now examine heap/process state."), OperationNumber, WallTimeDuration);

	while(!IsEngineExitRequested())
	{
		FPlatformProcess::Sleep(1.0);
	}
}

/**
 * Malloc replaying test
 */
int32 MallocReplayTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	FString ReplayFileName;
	if (FParse::Value(CommandLine, TEXT("replayfile="), ReplayFileName))
	{
		uint64 OperationToStopAfter = (uint64)(-1);
		if (!FParse::Value(CommandLine, TEXT("stopafter="), OperationToStopAfter))
		{
			UE_LOG(LogTestPAL, Display, TEXT("You can pass -stopafter=N to stop after Nth operation."));
		}
		
		bool bSuppressErrors = FParse::Param(CommandLine, TEXT("suppresserrors"));

		ReplayMallocFile(ReplayFileName, OperationToStopAfter, bSuppressErrors);
	}
	else
	{
		UE_LOG(LogTestPAL, Error, TEXT("No file to replay. Pass -replayfile=PathToFile.txt"));
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}


/**
 * Translates priorities into strings.
 * For priorities values, see GenericPlatformAffinity.h.
 */
const TCHAR* ThreadPrioToString(EThreadPriority Prio)
{
	switch(Prio)
	{
#define RET_PRIO(PrioName)	case PrioName: return TEXT(#PrioName);
		RET_PRIO(TPri_Normal)
		RET_PRIO(TPri_AboveNormal)
		RET_PRIO(TPri_BelowNormal)
		RET_PRIO(TPri_Highest)
		RET_PRIO(TPri_Lowest)
		RET_PRIO(TPri_SlightlyBelowNormal)
		RET_PRIO(TPri_TimeCritical)
#undef RET_PRIO
		default:
			checkf(false, TEXT("Unknown priority!"));
			break;
	}

	return nullptr;
}

/**
 * Thread (will be run with different prios)
 */
struct FThreadPrioTester : public FRunnable
{
	/** Priority assigned to this thread (for printing purposes). */
	EThreadPriority			Prio;

	/** Counter incremented by this thread. */
	uint64					Counter;

	/** Time to run (all threads should have the same value. */
	double					SecondsToRun;

	/** How much time the thread was actually running. Should be close to SecondsToRun, but may differ. */
	double					SecondsActuallyRan;

	FThreadPrioTester(EThreadPriority InPrio, double InSecondsToRun)
		:	FRunnable()
		,	Prio(InPrio)
		,	Counter(0)
		,	SecondsToRun(InSecondsToRun)
	{
	}

	virtual uint32 Run() override
	{
		double StartTime = FPlatformTime::Seconds();
		for(;;)
		{
			// don't check too often to avoid threads bottlenecking on access to clock
			if (Counter % 65536 == 0)
			{
				// account for an unlikely case that we will never get to run in the allotted time and check first
				double RanForSoFar = FPlatformTime::Seconds() - StartTime;
				if (RanForSoFar >= SecondsToRun)
				{
					SecondsActuallyRan = RanForSoFar;
					break;
				}
			}

			++Counter;
		}

		return 0;
	}
};

/**
 * Thread priorities test
 */
int32 ThreadPriorityTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	UE_LOG(LogTestPAL, Display, TEXT("Accepted options:"));
	UE_LOG(LogTestPAL, Display, TEXT("    -secondstorun=N (for how long to run)"));
	UE_LOG(LogTestPAL, Display, TEXT("    -numthreadgroups=N (how many groups of threads to run - so we can saturate the CPU)"));

	float SecondsToRun = 16.0f, InSecondsToRun = 0.0f;
	int32 NumThreadGroups = 8, InNumThreadGroups = 0;
	if (FParse::Value(CommandLine, TEXT("secondstorun="), InSecondsToRun))
	{
		SecondsToRun = FMath::Max(1.0f, InSecondsToRun);
	}
	if (FParse::Value(CommandLine, TEXT("numthreadgroups="), InNumThreadGroups))
	{
		NumThreadGroups = FMath::Max(1, InNumThreadGroups);
	}

	UE_LOG(LogTestPAL, Display, TEXT("Running thread priority test (%d thread groups) for %.1f seconds."),
		NumThreadGroups,
		SecondsToRun
		);

	/*  Note that enum - as of now at least - is not ordered in any way and looks like this:
		TPri_Normal,
		TPri_AboveNormal,
		TPri_BelowNormal,
		TPri_Highest,
		TPri_Lowest,
		TPri_SlightlyBelowNormal,
		TPri_TimeCritical */

	TArray<FThreadPrioTester*> RunnableArray;
	TArray<FRunnableThread*> ThreadArray;
	for (int32 IdxGroup = 0; IdxGroup < NumThreadGroups; ++IdxGroup)
	{
		RunnableArray.Add(new FThreadPrioTester(TPri_Normal, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_AboveNormal, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_BelowNormal, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_Highest, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_Lowest, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_SlightlyBelowNormal, SecondsToRun));
		RunnableArray.Add(new FThreadPrioTester(TPri_TimeCritical, SecondsToRun));
	}

	UE_LOG(LogTestPAL, Display, TEXT("Creating %d threads"), RunnableArray.Num());
	for (int32 Idx = 0, NumThreads = RunnableArray.Num(); Idx < NumThreads; ++Idx)
	{
		ThreadArray.Add( FRunnableThread::Create(RunnableArray[Idx],
			*FString::Printf(TEXT("(%d)%s"), Idx / 7, ThreadPrioToString(RunnableArray[Idx]->Prio)), 0, RunnableArray[Idx]->Prio)
			);
	}

	// join all threads
	for (int32 Idx = 0, NumThreads = RunnableArray.Num(); Idx < NumThreads; ++Idx)
	{
		ThreadArray[Idx]->WaitForCompletion();
		delete ThreadArray[Idx];
		ThreadArray[Idx] = nullptr;
	}

	GLog->FlushThreadedLogs();
	GLog->Flush();

	UE_LOG(LogTestPAL, Display, TEXT("--- Results ---"));

	// tally up all threads (note that as of now there are seven prios)
	uint64 AllThreadsCounters[7] = {0};
	double AllThreadsTimes[7] = {0};
	uint64 TotalCount = 0;

	// describe all threads
	for (int32 Idx = 0, NumThreads = RunnableArray.Num(); Idx < NumThreads; ++Idx)
	{
		uint32 PrioIndex = static_cast<uint32>(RunnableArray[Idx]->Prio);
		if (PrioIndex >= UE_ARRAY_COUNT(AllThreadsCounters))
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("EThreadPriority enum changed and has values larger than 7 now. Revisit this code."));
		}
		else
		{
			AllThreadsCounters[PrioIndex] += RunnableArray[Idx]->Counter;
			AllThreadsTimes[PrioIndex] += RunnableArray[Idx]->SecondsActuallyRan;
			TotalCount += RunnableArray[Idx]->Counter;
		}
		delete RunnableArray[Idx];
		RunnableArray[Idx] = nullptr;
	}

	// compare to TPri_Normal priority
	uint64 BaseCount = FMath::Max(1ULL, AllThreadsCounters[static_cast<uint32>(TPri_Normal)]);

	double BaseCountDbl = static_cast<double>(BaseCount);
	uint32 OrderToPrint[7] =
	{
		static_cast<uint32>(TPri_Lowest),
		static_cast<uint32>(TPri_BelowNormal),
		static_cast<uint32>(TPri_SlightlyBelowNormal),
		static_cast<uint32>(TPri_Normal),
		static_cast<uint32>(TPri_AboveNormal),
		static_cast<uint32>(TPri_Highest),
		static_cast<uint32>(TPri_TimeCritical)
	};

	for (uint32 IdxPrio = 0; IdxPrio < UE_ARRAY_COUNT(OrderToPrint); ++IdxPrio)
	{
		uint32 Prio = OrderToPrint[IdxPrio];
		checkf(Prio < UE_ARRAY_COUNT(AllThreadsCounters), TEXT("Number of thread priority enums changed, revisit this code."));
		UE_LOG(LogTestPAL, Display, TEXT("Threads with prio %24s incremented counters %14llu times (%.1f x speed of TPri_Normal) during %1.f sec total"),
			ThreadPrioToString(static_cast<EThreadPriority>(Prio)), AllThreadsCounters[Prio],
			static_cast<double>(AllThreadsCounters[Prio]) / BaseCountDbl,
			AllThreadsTimes[Prio]);
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

// inlined/non-inlined functions for testing
namespace
{
	void FORCENOINLINE LexicalBlock()
	{
		{
			{
				ensure(false);
			}
		}
	}

	void FORCENOINLINE LabelSwitch()
	{
		switch (1)
		{
			case 1:
				ensure(false);
				break;
			default:
				break;
		}
	}

	PRAGMA_DISABLE_UNREACHABLE_CODE_WARNINGS
	void FORCENOINLINE LabelGoto()
	{
		goto end;
		ensure(false); // skips all of these. Just want to create a bunch of inline statements in this single non inlined one
		ensure(false);
		ensure(false);
		ensure(false);
		ensure(false);
		ensure(false);
end:
		ensure(false);
	}
	PRAGMA_RESTORE_UNREACHABLE_CODE_WARNINGS

	void FORCEINLINE inline_three_ensures()
	{
		ensure(false);
	}

	void FORCEINLINE inline_two_calls_inline_three()
	{
		inline_three_ensures();
	}

	void FORCEINLINE inline_one_calls_inline_two()
	{
		inline_two_calls_inline_three();
	}

	void FORCENOINLINE MultipleInlineDeep()
	{
		inline_one_calls_inline_two();
	}

	void FORCEINLINE inline_crash()
	{
		*(int*)0x1 = 0x0;
	}

	void FORCENOINLINE no_inline_to_inline_crash()
	{
		inline_crash();
	}
}

int32 InlineCallstacksTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	/*  Three unique cases that we can only test two:
	 *
	 *  LexicalBlocks (ie. scope/if/for/while blocks
	 *  Labels (ie. goto/switch statements)
	 *  Try/Catch (cannot use when execptions are disabled, but we *should* handle this case)
	 *
	 *  As well as testing, multiple deep inlining. ie. Calling multiple inline functions and still
	 *  seeing the call site
	 *
	 *  Just make sure that the functions used + their callsite into ensure(false) are correctly reported in
	 *  the callstack
	 */

	UE_LOG(LogTestPAL, Warning, TEXT("\n*** Lexical Block ***"));
	LexicalBlock();
	UE_LOG(LogTestPAL, Warning, TEXT("\n*** Label Switch ***"));
	LabelSwitch();

	UE_LOG(LogTestPAL, Warning, TEXT("\n*** Label Goto ***"));
	LabelGoto();

	UE_LOG(LogTestPAL, Warning, TEXT("\n*** Multiple Inlined ***"));
	MultipleInlineDeep();

	// Should always be the last case, as this crashes
	UE_LOG(LogTestPAL, Warning, TEXT("\n*** Array delegate to crash***"));
	TArray<TFunction<void()>> a;
	a.Push([] { no_inline_to_inline_crash(); });
	a[0]();

	return 0;
}

struct FThreadStackTest : public FRunnable
{
	FThreadStackTest(uint64 InThreadId = ~0) :
		ThreadId(InThreadId)
	{
	}

	uint64 ThreadId;

	virtual uint32 Run()
	{
		// If we dont have a valid ThreadId lets use the threads id as default
		if (ThreadId == ~0)
		{
			ThreadId = FPlatformTLS::GetCurrentThreadId();
		}

		// Hopefully this wont be to much extra stack space for a testing thread
		const SIZE_T StackTraceSize = 65536;
		ANSICHAR StackTrace[StackTraceSize] = {0};
		FPlatformStackWalk::ThreadStackWalkAndDump(StackTrace, StackTraceSize, 0, ThreadId);

		UE_LOG(LogTestPAL, Warning, TEXT("***** ThreadStackWalkAndDump for ThreadId(%lu) ******\n%s"), ThreadId, ANSI_TO_TCHAR(StackTrace));

		const SIZE_T BackTraceSize = 100;
		uint64 BackTrace[BackTraceSize];
		int32 BackTraceCount = FPlatformStackWalk::CaptureThreadStackBackTrace(ThreadId, BackTrace, BackTraceSize);

		UE_LOG(LogTestPAL, Warning, TEXT("***** CaptureThreadStackBackTrace for ThreadId(%lu) ******"), ThreadId);
		for (int i = 0; i < BackTraceCount; i++)
		{
			UE_LOG(LogTestPAL, Warning, TEXT("0x%llx"), BackTrace[i]);
		}
		UE_LOG(LogTestPAL, Warning, TEXT("\n\n"));

		UE_LOG(LogTestPAL, Warning, TEXT("***** ProgramCounterToHumanReadableString for BackTrace for ThreadId(%lu) ******"), ThreadId);
		for (int i = 0; i < BackTraceCount; i++)
		{
			ANSICHAR TempString[1024] = {0};
			FPlatformStackWalk::ProgramCounterToHumanReadableString(i, BackTrace[i], TempString, UE_ARRAY_COUNT(TempString));

			UE_LOG(LogTestPAL, Warning, TEXT("%s"), ANSI_TO_TCHAR(TempString));
		}

		return 0;
	}
};


int32 ThreadTraceTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	// Dump the callstack/backtrace of the thread that is running
	{
		FThreadStackTest* ThreadStackTest = new FThreadStackTest;
		FRunnableThread* Runnable = FRunnableThread::Create(ThreadStackTest, ANSI_TO_TCHAR("ThreadStackTest"), 0);

		Runnable->WaitForCompletion();
		delete ThreadStackTest;
	}

	// Dump the GGameThreadId callstack/backtrace from the thread
	{
		FThreadStackTest* ThreadStackTest = new FThreadStackTest(GGameThreadId);
		FRunnableThread* Runnable = FRunnableThread::Create(ThreadStackTest, ANSI_TO_TCHAR("ThreadStackTest"), 0);

		Runnable->WaitForCompletion();
		delete ThreadStackTest;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return 0;
}

int32 ForkTest(const TCHAR* CommandLine)
{
	// Only supported on Linux
	if (PLATFORM_LINUX)
	{
		FPlatformMisc::SetCrashHandler(NULL);
		FPlatformMisc::SetGracefulTerminationHandler();

		GEngineLoop.PreInit(CommandLine);

		if (FPlatformProcess::SupportsMultithreading())
		{
			UE_LOG(LogTestPAL, Error, TEXT("WaitAndFork is only supported with '-nothreading' command line"));
		}
		else
		{
			UE_LOG(LogTestPAL, Warning, TEXT("About to fork. Press Ctrl+C to close parent process"));

			FPlatformProcess::EWaitAndForkResult Result = FPlatformProcess::WaitAndFork();

			if (Result == FPlatformProcess::EWaitAndForkResult::Child)
			{
				UE_LOG(LogTestPAL, Display, TEXT("Child process: %d IsChildProcess: %d"), FPlatformProcess::GetCurrentProcessId(), FForkProcessHelper::IsForkedChildProcess());
			}
			else
			{
				// parent, WaitAndFork holds the parent process to spawn more children until the we are closing
				UE_LOG(LogTestPAL, Display, TEXT("Parent process: %d IsChildProcess: %d"), FPlatformProcess::GetCurrentProcessId(), FForkProcessHelper::IsForkedChildProcess());
			}
		}

		FEngineLoop::AppPreExit();
		FEngineLoop::AppExit();
	}

	return 0;
}

/** 
 * Launch the external tool as a process and wait for it to complete
 *
 * Set executable and parameters via something like:
 *
 *  ./Engine/Binaries/Linux/TestPAL exec-process exe=/bin/ls params=/tmp
 *
 */
int32 ExecProcessTest(const TCHAR *CommandLine)
{
	FString Exe;
	FString Params;

	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	if (!FParse::Value(CommandLine, TEXT("exe="), Exe))
	{
		Exe = TEXT("nonexistent-exe");
	}
	if (!FParse::Value(CommandLine, TEXT("params="), Params))
	{
		Params = TEXT("");
	}

	UE_LOG(LogTestPAL, Display, TEXT("  Exe:'%s' Parameters:'%s'"), *Exe, *Params);

	int32 ReturnCode;
	FString OutStdOut;
	FString OutStdErr;
	bool Ret = FPlatformProcess::ExecProcess(*Exe, *Params, &ReturnCode, &OutStdOut, &OutStdErr);

	UE_LOG(LogTestPAL, Display, TEXT("ExecProcess returns %d"), Ret);
	UE_LOG(LogTestPAL, Display, TEXT("  ReturnCode:%d"), ReturnCode);
	UE_LOG(LogTestPAL, Display, TEXT("  StdOut:%s"), *OutStdOut);
	UE_LOG(LogTestPAL, Display, TEXT("  StdErr:%s"), *OutStdErr);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return 0;
}

int32 CmdlineParseTest(const TCHAR *CommandLine)
{
	int32 NotOk = 0;	// zero when everything has passed so far
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	verify(FPlatformProcess::CreatePipe(ReadPipe, WritePipe));

	uint32 ProcessID;

	// key is the command line to test, value is the expected parse result when run through 'echo'
	TArray<TTuple<FString, FString>> tests = 
	{ 
		{ TEXT("foo"), TEXT("foo") },
		{ TEXT("\"foo bar\""), TEXT("foo bar") },
		{ TEXT("\"\"foo bar\"\""), TEXT("\"foo bar\"") },
		{ TEXT("\"foo"), TEXT("") },
		{ TEXT("\"\"foo"), TEXT("") },
		{ TEXT("-logpath=\"path with space\""), TEXT("-logpath=path with space") },
		{ TEXT("-logpath=\"\"double quoted path with space\"\""), TEXT("-logpath=\"double quoted path with space\"") },
		{ TEXT("bar \"\"foo blarg"), TEXT("bar") },
	};

	for(int i=0;i<tests.Num();i++)
	{
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(TEXT("/usr/bin/echo"), *tests[i].Key, false, false, false, &ProcessID, 0, nullptr, WritePipe);
		while (FPlatformProcess::IsProcRunning(ProcessHandle));
		FString Result = FPlatformProcess::ReadPipe(ReadPipe).TrimEnd();
		NotOk = Result.Compare(*tests[i].Value);
		if(NotOk)
		{
			UE_LOG(LogTestPAL, Display, TEXT("CmdLine test failed: '%s' != '%s'\n"), *Result, *tests[i].Value);
			break;
		}
	}
	
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	UE_LOG(LogTestPAL, Display, TEXT("Parse command line test %s"), (NotOk)?TEXT("failed!"):TEXT("succeeded!"));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return NotOk;
}

/**
 * Selects and runs one of test cases.
 *
 * @param ArgC Number of commandline arguments.
 * @param ArgV Commandline arguments.
 * @return Test case's return code.
 */
int32 MultiplexedMain(int32 ArgC, char* ArgV[])
{
	for (int32 IdxArg = 0; IdxArg < ArgC; ++IdxArg)
	{
		if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST_CHILD))
		{
			return ProcRunAsChild(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST))
		{
			return ProcRunAsParent(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CASE_SENSITIVITY_TEST))
		{
			return CaseTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MESSAGEBOX_TEST))
		{
			return MessageBoxTest(*TestPAL::CommandLine);
		}
#if USE_DIRECTORY_WATCHER
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DIRECTORY_WATCHER_TEST))
		{
			return DirectoryWatcherTest(*TestPAL::CommandLine);
		}
#endif
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_THREAD_SINGLETON_TEST))
		{
			return ThreadSingletonTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_SYSINFO_TEST))
		{
			return SysInfoTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CRASH_TEST))
		{
			return CrashTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_ENSURE_TEST))
		{
			return EnsureTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_STRINGPRECISION_TEST))
		{
			return StringPrecisionTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DSO_TEST))
		{
			return DynamicLibraryTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_GET_ALLOCATION_SIZE_TEST))
		{
			return GetAllocationSizeTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MALLOC_THREADING_TEST))
		{
			return MallocThreadingTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MALLOC_REPLAY))
		{
			return MallocReplayTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_THREAD_PRIO_TEST))
		{
			return ThreadPriorityTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_INLINE_CALLSTACK_TEST))
		{
			return InlineCallstacksTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_STRINGS_ALLOCATION_TEST))
		{
			return StringsAllocationTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CREATEGUID_TEST))
		{
			return CreateGuidTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_THREADSTACK_TEST))
		{
			return ThreadTraceTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_EXEC_PROCESS_TEST))
		{
			return ExecProcessTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_FORK_TEST))
		{
			return ForkTest(*TestPAL::CommandLine);
		}
#if PLATFORM_LINUX
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CMDLINE_PARSE_TEST))
		{
			return CmdlineParseTest(*TestPAL::CommandLine);
		}
#endif
	}

	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(*TestPAL::CommandLine);
	UE_LOG(LogTestPAL, Warning, TEXT("Unable to find any known test name, no test started."));

	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Available test cases:"));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test process handling API"), ANSI_TO_TCHAR( ARG_PROC_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test case-insensitive file operations"), ANSI_TO_TCHAR( ARG_CASE_SENSITIVITY_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test message box bug (too long strings)"), ANSI_TO_TCHAR( ARG_MESSAGEBOX_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test directory watcher"), ANSI_TO_TCHAR( ARG_DIRECTORY_WATCHER_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test per-thread singletons"), ANSI_TO_TCHAR( ARG_THREAD_SINGLETON_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test (some) system information"), ANSI_TO_TCHAR( ARG_SYSINFO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test crash handling (pass '-logfatal' for testing Fatal logs)"), ANSI_TO_TCHAR( ARG_CRASH_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test ensure handling (including ensureAlways and repeated ensures)"), ANSI_TO_TCHAR( ARG_ENSURE_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test passing %%*s in a format string"), ANSI_TO_TCHAR( ARG_STRINGPRECISION_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test APIs for dealing with dynamic libraries"), ANSI_TO_TCHAR( ARG_DSO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test GMalloc->GetAllocationSize()"), UTF8_TO_TCHAR(ARG_GET_ALLOCATION_SIZE_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test malloc for thread-safety and performance."), UTF8_TO_TCHAR(ARG_MALLOC_THREADING_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test by replaying a saved malloc history saved by -mallocsavereplay. Possible options: -replayfile=File, -stopafter=N (operation), -suppresserrors"), UTF8_TO_TCHAR(ARG_MALLOC_REPLAY));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test thread priorities."), UTF8_TO_TCHAR(ARG_THREAD_PRIO_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test inline callstacks through ensures and a final crash."), UTF8_TO_TCHAR(ARG_INLINE_CALLSTACK_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test string allocations."), UTF8_TO_TCHAR(ARG_STRINGS_ALLOCATION_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test CreateGuid."), UTF8_TO_TCHAR(ARG_CREATEGUID_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test ThreadWalkStackAndDump and CaptureThreadBackTrace."), UTF8_TO_TCHAR(ARG_THREADSTACK_TEST));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test ExecProcess. Possible options: [-exe=executable] [-params=parameters]"), UTF8_TO_TCHAR(ARG_EXEC_PROCESS_TEST));

	if (PLATFORM_LINUX)
	{
		UE_LOG(LogTestPAL, Warning, TEXT("  %s: test WaitAndFork"), UTF8_TO_TCHAR(ARG_FORK_TEST));
		UE_LOG(LogTestPAL, Warning, TEXT("  %s: test CmdlineParse"), UTF8_TO_TCHAR(ARG_CMDLINE_PARSE_TEST));
	}

	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Pass one of those to run an appropriate test."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 1;
}

int32 main(int32 ArgC, char* ArgV[])
{
	TestPAL::CommandLine = TEXT("");

	for (int32 Option = 1; Option < ArgC; Option++)
	{
		TestPAL::CommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(ArgV[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		TestPAL::CommandLine += Argument;
	}

	return MultiplexedMain(ArgC, ArgV);
}
