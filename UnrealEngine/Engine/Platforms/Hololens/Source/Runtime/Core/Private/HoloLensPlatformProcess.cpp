// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
HoloLensProcess.cpp: HoloLens implementations of Process functions
=============================================================================*/

#include "HoloLensPlatformProcess.h"
#include "Misc/SingleThreadEvent.h"
#include "HoloLensRunnableThread.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreStats.h"
#include "Misc/Paths.h"
#include "Microsoft/WindowsHWrapper.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"

// static variables
TArray<FString> FHoloLensProcess::DllDirectoryStack;

const TCHAR* FHoloLensProcess::BaseDir()
{
	static bool bFirstTime = true;
	static TCHAR Result[512]=TEXT("");
	if (!Result[0] && bFirstTime)
	{
		// Check the commandline for -BASEDIR=<base directory>
		const TCHAR* CmdLine = NULL;
		if (FCommandLine::IsInitialized())
		{
			CmdLine = FCommandLine::Get();
		}
		// Change from Microsoft, may require further review:
		// code path that was hitting BaseDir in static initializers was removed
		//                  and command line parsing was moved to earlier in launch to help avoid having 
		//                  to have command line parsing code in multiple location.
		//                  There's still potential for error here until Epic allows platform-subclassing the
		//                  command line with an overridable Initialize() or BeginInitialize().
		bool overridden = false;
		if (CmdLine != NULL)
		{
			FString BaseDirToken = TEXT("-BASEDIR=");
			FString NextToken;
			while (FParse::Token(CmdLine, NextToken, false))
			{
				if (NextToken.StartsWith(BaseDirToken) == true)
				{
					FString BaseDir = NextToken.Right(NextToken.Len() - BaseDirToken.Len());
					BaseDir.ReplaceInline(TEXT("\\"), TEXT("/"));
					FCString::Strcpy(Result, *BaseDir);
					overridden = true;
				}
			}
		}

		if (!overridden)
		{
			DWORD ResultCode = GetModuleFileName(NULL, Result, _countof(Result));
			verify(ResultCode != 0);
			DWORD ErrorCode = ::GetLastError();
			verify(ErrorCode != ERROR_INSUFFICIENT_BUFFER);

			// Strip out the filename and add the trailing '\'
			FString BaseDir;
			BaseDir = FPaths::GetPath(FString(Result));
			BaseDir += TEXT("\\");
			FCString::Strcpy(Result, *BaseDir);
		}
		bFirstTime = false;
	}
	return Result;
}

#include "Microsoft/HideMicrosoftPlatformTypes.h"

void FHoloLensProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
	FThreadIdleStats::FScopeIdle Scope;
	::Sleep((int32)(Seconds * 1000.0));
}

void FHoloLensProcess::SleepNoStats(float Seconds)
{
	::Sleep((uint32)(Seconds * 1000.0));
}

void FHoloLensProcess::SleepInfinite()
{
	while (true)
	{
		::Sleep(INFINITE);
	}
}

void FHoloLensProcess::YieldThread()
{
	::SwitchToThread();
}

#include "HoloLensEvent.h"

FEvent* FHoloLensProcess::CreateSynchEvent(bool bIsManualReset /*= false*/)
{
	// Allocate the new object
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		Event = new FEventHoloLens();
	}
	else
	{
		// Fake vent object.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!Event->Create(bIsManualReset))
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

#include "Microsoft/AllowMicrosoftPlatformTypes.h"

bool FEventHoloLens::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats /*= false*/)
{
	SCOPE_CYCLE_COUNTER(STAT_EventWait);
	FThreadIdleStats::FScopeIdle Scope(bIgnoreThreadIdleStats);
	check(Event);

	return WaitForSingleObjectEx(Event, WaitTime, FALSE) == WAIT_OBJECT_0;
}

FRunnableThread* FHoloLensProcess::CreateRunnableThread()
{
	return new FRunnableThreadHoloLens();
}

const TCHAR* FHoloLensProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512] = TEXT("");
	static TCHAR ResultWithExt[512] = TEXT("");
	if (!Result[0])
	{
		// Get complete path for the executable
		if (GetModuleFileName(NULL, Result, UE_ARRAY_COUNT(Result)) != 0)
		{
			// Remove all of the path information by finding the base filename
			FString FileName = Result;
			FString FileNameWithExt = Result;
			FCString::Strncpy(Result, *(FPaths::GetBaseFilename(FileName)), UE_ARRAY_COUNT(Result));
			FCString::Strncpy(ResultWithExt, *(FPaths::GetCleanFilename(FileNameWithExt)), UE_ARRAY_COUNT(ResultWithExt));
		}
		// If the call failed, zero out the memory to be safe
		else
		{
			FMemory::Memzero(Result, sizeof(Result));
			FMemory::Memzero(ResultWithExt, sizeof(ResultWithExt));
		}
	}

	return (bRemoveExtension ? Result : ResultWithExt);
}

void* FHoloLensProcess::GetDllHandle(const TCHAR* Filename)
{
	check(Filename);

	FString PackageRelativePath(Filename);

	if (DllDirectoryStack.Num() > 0)
	{
		// If a path has been pushed on the stack use that
		PackageRelativePath = DllDirectoryStack.Top() / PackageRelativePath;
	}
	else
	{
		// Incoming paths are relative to the BaseDir, but LoadPackagedLibrary wants package relative
		// which in UE terms is the RootDir.
		FPaths::MakePathRelativeTo(PackageRelativePath, *(FPaths::RootDir() + TEXT("/")));
	}

	DWORD ErrorMode = 0;
	DWORD PrevErrorMode = 0;
	BOOL bHavePrevErrorMode = ::SetThreadErrorMode(ErrorMode, &PrevErrorMode);

	// Load the DLL, avoiding windows dialog boxes if missing
	void* const Handle = ::LoadPackagedLibrary(*PackageRelativePath, 0ul);

	if (Handle == nullptr)
	{
		DWORD Error = GetLastError();
		ensureMsgf(DllDirectoryStack.Num() > 0, TEXT("GetDllHandle failed for %s Error %d"), Filename, Error);
	}

	if (bHavePrevErrorMode)
	{
		::SetThreadErrorMode(PrevErrorMode, NULL);
	}

	return Handle;
}

void FHoloLensProcess::FreeDllHandle(void* DllHandle)
{
	// It is okay to call FreeLibrary on 0
	::FreeLibrary((HMODULE)DllHandle);
}

void* FHoloLensProcess::GetDllExport(void* DllHandle, const TCHAR* ProcName)
{
	check(DllHandle);
	check(ProcName);
	return (void*)::GetProcAddress((HMODULE)DllHandle, TCHAR_TO_ANSI(ProcName));
}

void FHoloLensProcess::PushDllDirectory(const TCHAR* Directory)
{
	DllDirectoryStack.Push(Directory);
}

void FHoloLensProcess::PopDllDirectory(const TCHAR* Directory)
{
	// don't allow too many pops (indicates bad code that should be fixed, but won't kill anything, so using ensure)
	ensureMsgf(DllDirectoryStack.Num() > 0, TEXT("Tried to PopDllDirectory too many times"));
	// verify we are popping the top
	checkf(DllDirectoryStack.Top() == Directory, TEXT("There was a PushDllDirectory/PopDllDirectory mismatch (Popped %s, which didn't match %s)"), *DllDirectoryStack.Top(), Directory);
	// pop it off
	DllDirectoryStack.Pop();
}

void FHoloLensProcess::SetCurrentWorkingDirectoryToBaseDir()
{
	FPlatformMisc::CacheLaunchDir();
	verify(SetCurrentDirectoryW(BaseDir()));	// failure here usually means the ACLs got messed up, reregister the app
}

FString FHoloLensProcess::GetCurrentWorkingDirectory()
{
	// get the current working directory (uncached)
	TCHAR CurrentDirectory[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, CurrentDirectory);
	return CurrentDirectory;
}

const TCHAR* FHoloLensProcess::UserDir()
{
#if WITH_EDITOR
	return UE_BUILD_SHIPPING ? GetLocalAppDataLowLevelPath() : GetLocalAppDataRedirectPath();
#else
	return GetLocalAppDataLowLevelPath();
#endif
}

const TCHAR* FHoloLensProcess::UserSettingsDir()
{
	// @todo is there any reason to make this a sub-folder?
	return UserDir();
}

const TCHAR* FHoloLensProcess::UserTempDir()
{
#if WITH_EDITOR
	return UE_BUILD_SHIPPING ? GetTempAppDataLowLevelPath() : GetTempAppDataRedirectPath();
#else
	return GetTempAppDataLowLevelPath();
#endif
}

const TCHAR* FHoloLensProcess::ApplicationSettingsDir()
{
	// This is supposed to be a writeable location that exists across multiple users.
	// For HoloLens it will have to be user-specific.
	return UserSettingsDir();
}

#if WIN10_SDK_VERSION < 16225
// This is a drop in equivelent to the deprecated thread affinity APIs that most legacy code is based on.
// As with those older APIs, it's functionality may be unexpected on machines with more than 64 cores.
DWORD_PTR WINAPI SetThreadAffinityMask(
	_In_ HANDLE    hThread,
	_In_ DWORD_PTR dwThreadAffinityMask
)
{
	static struct CPU_INFO {
		int Count;
		ULONG CpuInfoBytes;
		uint8_t* CpuInfoBuffer;
		PSYSTEM_CPU_SET_INFORMATION* CpuInfoPtrs;

		CPU_INFO() {
			Count = 0;
			CpuInfoBytes = 0;
			GetSystemCpuSetInformation(nullptr, 0, &CpuInfoBytes, GetCurrentProcess(), 0);

			CpuInfoBuffer = reinterpret_cast<uint8_t*>(malloc(CpuInfoBytes));
			if (CpuInfoBuffer)
			{
				GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(CpuInfoBuffer),
					CpuInfoBytes, &CpuInfoBytes, GetCurrentProcess(), 0);

				BYTE* firstCpu = CpuInfoBuffer;
				for (BYTE* currentCpu = firstCpu; currentCpu < firstCpu + CpuInfoBytes; currentCpu += reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu)->Size)
				{
					Count++;
				}

				CpuInfoPtrs = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION*>(calloc(Count, sizeof(PSYSTEM_CPU_SET_INFORMATION)));

				if (CpuInfoPtrs)
				{
					firstCpu = CpuInfoBuffer;
					int currentCpuNum = 0;
					for (BYTE* currentCpu = firstCpu; currentCpu < firstCpu + CpuInfoBytes; currentCpu += reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu)->Size)
					{
						CpuInfoPtrs[currentCpuNum] = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu);
						currentCpuNum++;
					}
				}
			}
		}
	} Cpu_Info;

	// if static initialization got Cpu info....
	if (Cpu_Info.Count > 0 && Cpu_Info.CpuInfoBytes && Cpu_Info.CpuInfoBuffer && Cpu_Info.CpuInfoPtrs)
	{
#ifdef _WIN64
		DWORD_PTR priorMask = 0xffffffffffffffffull;
		ULONG coreIds[64];
#else
		DWORD_PTR priorMask = 0xfffffffful;
		ULONG coreIds[32];
#endif
		ULONG priorCoreCount = 0;

		// this is simplified, assuming that not other setter will be setting affinity
		if (GetThreadSelectedCpuSets(hThread, coreIds, ARRAYSIZE(coreIds), &priorCoreCount))
		{
			if (priorCoreCount > 0 && priorCoreCount <= ARRAYSIZE(coreIds))
			{
				priorMask = 0;
				for (unsigned int currentCoreEntry = 0; currentCoreEntry < priorCoreCount; currentCoreEntry++)
				{
					for (int i = 0; i < Cpu_Info.Count; i++)
					{
						if (coreIds[currentCoreEntry] == Cpu_Info.CpuInfoPtrs[i]->CpuSet.Id)
						{
							priorMask |= (1ull << i);
							break;
						}
					}
				}
			}
		}

		int markedCount = 0;
		for (int coreNum = 0; coreNum < ARRAYSIZE(coreIds) && coreNum < Cpu_Info.Count; coreNum++)
		{
			if (dwThreadAffinityMask & (1ull << coreNum))
			{
				coreIds[markedCount] = Cpu_Info.CpuInfoPtrs[coreNum]->CpuSet.Id;
				markedCount++;
			}
		}

		if (SetThreadSelectedCpuSets(hThread, coreIds, markedCount))
		{
			return priorMask;
		}
		else
		{
			SetLastError(ERROR_BAD_ARGUMENTS);
			return 0;
		}
	}
	else
	{
		SetLastError(ERROR_DEVICE_ENUMERATION_ERROR);
		return 0;
	}
}
#endif

void FHoloLensProcess::SetThreadAffinityMask(uint64 AffinityMask)
{
	::SetThreadAffinityMask(::GetCurrentThread(), (DWORD_PTR)AffinityMask);
}

const TCHAR* FHoloLensProcess::GetLocalAppDataLowLevelPath()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");
	if (!Result[0])
	{
		Windows::Storage::StorageFolder ^ TempFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
		if (nullptr != TempFolder)
		{
			Platform::String^ TempPath = TempFolder->Path;
			if (nullptr != TempPath)
			{
				FCString::Strncpy(Result, TempPath->Data(), PLATFORM_MAX_FILEPATH_LENGTH);
			}
		}

		check(Result[0]);
	}

	return Result;
}

const TCHAR* FHoloLensProcess::GetTempAppDataLowLevelPath()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");
	if (!Result[0])
	{
		Windows::Storage::StorageFolder ^ TempFolder = Windows::Storage::ApplicationData::Current->TemporaryFolder;
		if (nullptr != TempFolder)
		{
			Platform::String^ TempPath = TempFolder->Path;
			if (nullptr != TempPath)
			{
				FCString::Strncpy(Result, TempPath->Data(), PLATFORM_MAX_FILEPATH_LENGTH);
			}
		}

		check(Result[0]);
	}

	return Result;
}

const TCHAR* FHoloLensProcess::GetLocalAppDataRedirectPath()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");
	if (!Result[0])
	{
		FCString::Strcpy(Result, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("HoloLensLocalAppData")));
	}
	return Result;
}

const TCHAR* FHoloLensProcess::GetTempAppDataRedirectPath()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");
	if (!Result[0])
	{
		FCString::Strcpy(Result, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("HoloLensTempAppData")));
	}
	return Result;
}

bool FHoloLensProcess::ShouldSaveToUserDir()
{
	return true;
}

bool FHoloLensProcess::CanLaunchURL(const TCHAR* URL)
{
	return URL != nullptr;
}

void FHoloLensProcess::LaunchURL(const TCHAR* URL, const TCHAR* Params, FString* Error)
{
#if PLATFORM_HOLOLENS
	try
	{
		Windows::Foundation::Uri^ uri = ref new Windows::Foundation::Uri(ref new Platform::String(URL));
		Windows::System::Launcher::LaunchUriAsync(uri);
	}
	catch (Platform::Exception^)
	{
		if (Error != nullptr)
		{
			*Error = TEXT("Failed to launch URL");
		}
	}
#endif
}

#include "Microsoft/HideMicrosoftPlatformTypes.h"
