// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformMisc.h"
#include "Misc/DateTime.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDevice.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/CString.h"
#include "Misc/Parse.h"
#include "Misc/MessageDialog.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/FeedbackContext.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/SecureHash.h"
#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Windows/WindowsPlatformCrashContext.h"
#include "HAL/PlatformOutputDevices.h"

#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/ThreadHeartBeat.h"
#include "ProfilingDebugging/ExternalProfiler.h"

// Resource includes.
#include "Runtime/Launch/Resources/Windows/Resource.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <time.h>
	#include <mmsystem.h>
	#include <rpcsal.h>
	#include <gameux.h>
	#include <ShlObj.h>
	#include <IntShCut.h>
	#include <shellapi.h>
	#include <shlwapi.h>
	#include <IPHlpApi.h>
	#include <VersionHelpers.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#include "Modules/ModuleManager.h"

#include "Windows/AllowWindowsPlatformTypes.h"
	#include <Psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "psapi.lib")

#include <fcntl.h>
#include <io.h>

#include "Windows/AllowWindowsPlatformTypes.h"

#include "FramePro/FrameProProfiler.h"

// This might not be defined by Windows when maintaining backwards-compatibility to pre-Win8 builds
#ifndef SM_CONVERTIBLESLATEMODE
#define SM_CONVERTIBLESLATEMODE			0x2003
#endif

// this cvar can be removed once we have a single method that works well
static TAutoConsoleVariable<int32> CVarDriverDetectionMethod(
	TEXT("r.DriverDetectionMethod"),
	5,
	TEXT("Defines which implementation is used to detect the GPU driver (to check for old drivers, logs and statistics)\n"
	     "  0: Iterate available drivers in registry and choose the one with the same name, if in question use next method (happens)\n"
	     "  1: Get the driver of the primary adapter (might not be correct when dealing with multiple adapters)\n"
	     "  2: Use DirectX LUID (would be the best, not yet implemented)\n"
	     "  3: Use Windows functions, use the primary device (might be wrong when API is using another adapter)\n"
	     "  4: Use Windows functions, use names such as DirectX Device (newest, most promising)\n"
	     "  5: Use Windows SetupAPI functions"),
	ECVF_RenderThreadSafe);

int32 GetOSVersionsHelper( TCHAR* OutOSVersionLabel, int32 OSVersionLabelLength, TCHAR* OutOSSubVersionLabel, int32 OSSubVersionLabelLength )
{
	int32 ErrorCode = (int32)FWindowsOSVersionHelper::SUCCEEDED;

	// Get system info
	SYSTEM_INFO SystemInfo;
	if( FPlatformMisc::Is64bitOperatingSystem() )
	{
		GetNativeSystemInfo( &SystemInfo );
	}
	else
	{
		GetSystemInfo( &SystemInfo );
	}

	OSVERSIONINFOEX OsVersionInfo = {0};
	OsVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
	FString OSVersionLabel = TEXT("Windows (unknown version)");
	FString OSSubVersionLabel = TEXT("");
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(disable : 4996) // 'function' was declared deprecated
#endif
	CA_SUPPRESS(28159)
	if( GetVersionEx( (LPOSVERSIONINFO)&OsVersionInfo ) )
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma warning(pop)
#endif
	{
		bool bIsInvalidVersion = false;

		switch (OsVersionInfo.dwMajorVersion)
		{
		case 6:
			switch (OsVersionInfo.dwMinorVersion)
			{
			case 1:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows 7");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2008 R2");
				}
				break;
			case 2:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows 8");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2012");
				}
				break;
			case 3:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows 8.1");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2012 R2");
				}
				break;
			default:
				ErrorCode |= (int32)FWindowsOSVersionHelper::ERROR_UNKNOWNVERSION;
				break;
			}
			break;
		case 10:
			switch (OsVersionInfo.dwMinorVersion)
			{
			case 0:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					// Windows 11 still reports a major version of 10 and minor of 0, so it looks
					// like we need to use the build number as the discriminator
					if (OsVersionInfo.dwBuildNumber >= 22000)
					{
						OSVersionLabel = TEXT("Windows 11");
					}
					else
					{
						OSVersionLabel = TEXT("Windows 10");
					}
				}
				else
				{
					// Same thing here, Windows Server 2019 and 2022 both claim to be version 10.0, so use the
					// build number to decide.
					if (OsVersionInfo.dwBuildNumber >= 20348)
					{
						OSVersionLabel = TEXT("Windows Server 2022");
					}
					else
					{
						OSVersionLabel = TEXT("Windows Server 2019");
					}
				}

				// For Windows 10, get the release number and append that to the string too (eg. 1709 = Fall Creators Update). 
				// There doesn't seem to be any good way to get this other than grabbing an entry from the registry.
				// 
				// The new semi-annual release scheme 20H1/20H2 etc appears to use a different key so we query that first.
				{
					FString DisplayVersion;
					if (FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), TEXT("DisplayVersion"), DisplayVersion))
					{
						OSVersionLabel += FString::Printf(TEXT(" (%s)"), *DisplayVersion);
					}
					else
					{
						FString ReleaseId;
						if (FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), TEXT("ReleaseId"), ReleaseId))
						{
							OSVersionLabel += FString::Printf(TEXT(" (Release %s)"), *ReleaseId);
						}
					}

					FString UpdateBuildRevision;
					if (!FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), TEXT("UBR"), UpdateBuildRevision))
					{
						UpdateBuildRevision = TEXT("UNKNOWN");
					}

					// Add the build number as displayed by the winver utility.
					OSVersionLabel += FString::Printf(TEXT(" [%u.%u.%u.%s]"), OsVersionInfo.dwMajorVersion, OsVersionInfo.dwMinorVersion, OsVersionInfo.dwBuildNumber, *UpdateBuildRevision);
				}
				break;
			default:
				ErrorCode |= (int32)FWindowsOSVersionHelper::ERROR_UNKNOWNVERSION;
				break;
			}
			break;
		default:
			ErrorCode |= FWindowsOSVersionHelper::ERROR_UNKNOWNVERSION;
			break;
		}

		if(OsVersionInfo.dwMajorVersion >= 6)
		{
#pragma warning( push )
#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
			typedef BOOL( WINAPI *LPFN_GETPRODUCTINFO )(DWORD, DWORD, DWORD, DWORD, PDWORD);
			LPFN_GETPRODUCTINFO fnGetProductInfo = (LPFN_GETPRODUCTINFO)GetProcAddress( GetModuleHandle( TEXT( "kernel32.dll" ) ), "GetProductInfo" );
#pragma warning( pop )
			if( fnGetProductInfo != NULL )
			{
				DWORD Type;
				fnGetProductInfo( OsVersionInfo.dwMajorVersion, OsVersionInfo.dwMinorVersion, 0, 0, &Type );

				switch( Type )
				{
					case PRODUCT_ULTIMATE:
						OSSubVersionLabel = TEXT( "Ultimate Edition" );
						break;
					case PRODUCT_PROFESSIONAL:
						OSSubVersionLabel = TEXT( "Professional" );
						break;
					case PRODUCT_HOME_PREMIUM:
						OSSubVersionLabel = TEXT( "Home Premium Edition" );
						break;
					case PRODUCT_HOME_BASIC:
						OSSubVersionLabel = TEXT( "Home Basic Edition" );
						break;
					case PRODUCT_ENTERPRISE:
						OSSubVersionLabel = TEXT( "Enterprise Edition" );
						break;
					case PRODUCT_BUSINESS:
						OSSubVersionLabel = TEXT( "Business Edition" );
						break;
					case PRODUCT_STARTER:
						OSSubVersionLabel = TEXT( "Starter Edition" );
						break;
					case PRODUCT_CLUSTER_SERVER:
						OSSubVersionLabel = TEXT( "Cluster Server Edition" );
						break;
					case PRODUCT_DATACENTER_SERVER:
						OSSubVersionLabel = TEXT( "Datacenter Edition" );
						break;
					case PRODUCT_DATACENTER_SERVER_CORE:
						OSSubVersionLabel = TEXT( "Datacenter Edition (core installation)" );
						break;
					case PRODUCT_ENTERPRISE_SERVER:
						OSSubVersionLabel = TEXT( "Enterprise Edition" );
						break;
					case PRODUCT_ENTERPRISE_SERVER_CORE:
						OSSubVersionLabel = TEXT( "Enterprise Edition (core installation)" );
						break;
					case PRODUCT_ENTERPRISE_SERVER_IA64:
						OSSubVersionLabel = TEXT( "Enterprise Edition for Itanium-based Systems" );
						break;
					case PRODUCT_SMALLBUSINESS_SERVER:
						OSSubVersionLabel = TEXT( "Small Business Server" );
						break;
					case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
						OSSubVersionLabel = TEXT( "Small Business Server Premium Edition" );
						break;
					case PRODUCT_STANDARD_SERVER:
						OSSubVersionLabel = TEXT( "Standard Edition" );
						break;
					case PRODUCT_STANDARD_SERVER_CORE:
						OSSubVersionLabel = TEXT( "Standard Edition (core installation)" );
						break;
					case PRODUCT_WEB_SERVER:
						OSSubVersionLabel = TEXT( "Web Server Edition" );
						break;
				}
			}
			else
			{
				OSSubVersionLabel = TEXT( "(type unknown)" );
				ErrorCode |= (int32)FWindowsOSVersionHelper::ERROR_GETPRODUCTINFO_FAILED;
			}
		}

#if 0
		// THIS BIT ADDS THE SERVICE PACK INFO TO THE EDITION STRING
		// Append service pack info
		if( OsVersionInfo.szCSDVersion[0] != 0 )
		{
			OSSubVersionLabel += FString::Printf( TEXT( " (%s)" ), OsVersionInfo.szCSDVersion );
		}
#else
		// THIS BIT USES SERVICE PACK INFO ONLY
		OSSubVersionLabel = OsVersionInfo.szCSDVersion;
#endif
	}
	else
	{
		ErrorCode |= FWindowsOSVersionHelper::ERROR_GETVERSIONEX_FAILED;
	}

	FCString::Strcpy(OutOSVersionLabel, OSVersionLabelLength, *OSVersionLabel);
	FCString::Strcpy(OutOSSubVersionLabel, OSSubVersionLabelLength, *OSSubVersionLabel);

	return ErrorCode;
}

#if WINDOWS_USE_DEFAULT_OSVERSIONHELPER
int32 FWindowsOSVersionHelper::GetOSVersions( FString& OutOSVersionLabel, FString& OutOSSubVersionLabel )
{
	TCHAR OSVersionLabel[128];
	TCHAR OSSubVersionLabel[128];

	OSVersionLabel[0] = TEXT('\0');
	OSSubVersionLabel[0] = TEXT('\0');

	int32 Result = GetOSVersionsHelper( OSVersionLabel, UE_ARRAY_COUNT(OSVersionLabel), OSSubVersionLabel, UE_ARRAY_COUNT(OSSubVersionLabel) );

	OutOSVersionLabel = OSVersionLabel;
	OutOSSubVersionLabel = OSSubVersionLabel;

	return Result;
}
#endif //WINDOWS_USE_DEFAULT_OSVERSIONHELPER

namespace
{
	bool GetOSVersionHelper(TCHAR* OutString, int32 Length)
	{
		int32 ErrorCode = (int32)FWindowsOSVersionHelper::SUCCEEDED;

		// Get system info
		SYSTEM_INFO SystemInfo;
		const TCHAR* Architecture;
		if (FPlatformMisc::Is64bitOperatingSystem())
		{
			Architecture = TEXT("64bit");
			GetNativeSystemInfo(&SystemInfo);
		}
		else
		{
			Architecture = TEXT("32bit");
			GetSystemInfo(&SystemInfo);
		}

		OSVERSIONINFOEX OsVersionInfo = { 0 };
		OsVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	#else
	#pragma warning(push)
	#pragma warning(disable : 4996) // 'function' was declared deprecated
	#endif
		CA_SUPPRESS(28159)
		if (GetVersionEx((LPOSVERSIONINFO)&OsVersionInfo))
	#ifdef __clang__
	#pragma clang diagnostic pop
	#else
	#pragma warning(pop)
	#endif
		{
			FCString::Snprintf(OutString, Length, TEXT("%d.%d.%d.%d.%d.%s"), OsVersionInfo.dwMajorVersion, OsVersionInfo.dwMinorVersion, OsVersionInfo.dwBuildNumber, OsVersionInfo.wProductType, OsVersionInfo.wSuiteMask, Architecture);
			return true;
		}
		return false;
	}
}

#include "Windows/HideWindowsPlatformTypes.h"

/** 
 * Whether support for integrating into the firewall is there
 */
#define WITH_FIREWALL_SUPPORT	0

extern "C"
{
	CORE_API Windows::HINSTANCE hInstance = NULL;
}


/** Original C- Runtime pure virtual call handler that is being called in the (highly likely) case of a double fault */
_purecall_handler DefaultPureCallHandler;

/**
* Our own pure virtual function call handler, set by appPlatformPreInit. Falls back
* to using the default C- Runtime handler in case of double faulting.
*/
static void PureCallHandler()
{
	static bool bHasAlreadyBeenCalled = false;
	UE_DEBUG_BREAK();
	if( bHasAlreadyBeenCalled )
	{
		// Call system handler if we're double faulting.
		if( DefaultPureCallHandler )
		{
			DefaultPureCallHandler();
		}
	}
	else
	{
		bHasAlreadyBeenCalled = true;
		if( GIsRunning )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("Core", "PureVirtualFunctionCalledWhileRunningApp", "Pure virtual function being called while application was running (GIsRunning == 1).") );
		}
		UE_LOG(LogWindows, Fatal,TEXT("Pure virtual function being called") );
	}
}

/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
* Get the hash values out of the executable hash section
*
* NOTE: hash keys are stored in the executable, you will need to put a line like the
*		 following into your PCLaunch.rc settings:
*			ID_HASHFILE RCDATA "../../../../GameName/Build/Hashes.sha"
*
*		 Then, use the -sha option to the cooker (must be from commandline, not
*       frontend) to generate the hashes for .ini, loc, startup packages, and .usf shader files
*
*		 You probably will want to make and checkin an empty file called Hashses.sha
*		 into your source control to avoid linker warnings. Then for testing or final
*		 final build ONLY, use the -sha command and relink your executable to put the
*		 hashes for the current files into the executable.
*/
static void InitSHAHashes()
{
	uint32 SectionSize = 0;
	void* SectionData = NULL;
	// find the resource for the file hash in the exe by ID
	HRSRC HashFileFindResH = FindResource(NULL,MAKEINTRESOURCE(ID_HASHFILE),RT_RCDATA);
	if( HashFileFindResH )
	{
		// load it
		HGLOBAL HashFileLoadResH = LoadResource(NULL,HashFileFindResH);
		if( !HashFileLoadResH )
		{
			FMessageDialog::ShowLastError();
		}
		else
		{
			// get size
			SectionSize = SizeofResource(NULL,HashFileFindResH);
			// get the data. no need to unlock it
			SectionData = (uint8*)LockResource(HashFileLoadResH);
		}
	}

	// there may be a dummy byte for platforms that can't handle empty files for linking
	if (SectionSize <= 1)
	{
		return;
	}

	// look for the hash section
	if( SectionData )
	{
		FSHA1::InitializeFileHashesFromBuffer((uint8*)SectionData, SectionSize);
	}
}

/**
 *	Sets process memory limit using the job object, may fail under some situation like when Program Compatibility Assistant is enabled.
 *	Debugging purpose only.
 */
static void SetProcessMemoryLimit( SIZE_T ProcessMemoryLimitMB )
{
	HANDLE JobObject = ::CreateJobObject(nullptr, TEXT("UE4-JobObject"));
	check(JobObject);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobLimitInfo;
	FMemory::Memzero( JobLimitInfo );
	JobLimitInfo.ProcessMemoryLimit = 1024*1024*ProcessMemoryLimitMB;
	JobLimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
	const BOOL bSetJob = ::SetInformationJobObject(JobObject,JobObjectExtendedLimitInformation,&JobLimitInfo,sizeof(JobLimitInfo));

	const BOOL bAssign = ::AssignProcessToJobObject(JobObject, GetCurrentProcess());
}

void FWindowsPlatformMisc::PlatformPreInit()
{
	//SetProcessMemoryLimit( 92 );

	FGenericPlatformMisc::PlatformPreInit();

	// Load the bundled version of dbghelp.dll if necessary
#if USE_BUNDLED_DBGHELP
	// Loading newer versions of DbgHelp fails on Windows 7 since it is no longer supported.
	if (IsWindows8OrGreater())
	{
		// Try to load a bundled copy of dbghelp.dll. A bug with Windows 10 version 1709 
		FString DbgHlpPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/DbgHelp/dbghelp.dll");
		FPlatformProcess::GetDllHandle(*DbgHlpPath);
	}
#endif

	// Use our own handler for pure virtuals being called.
	DefaultPureCallHandler = _set_purecall_handler( PureCallHandler );

	const int32 MinResolution[] = {640,480};
	if ( ::GetSystemMetrics(SM_CXSCREEN) < MinResolution[0] || ::GetSystemMetrics(SM_CYSCREEN) < MinResolution[1] )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("Launch", "Error_ResolutionTooLow", "The current resolution is too low to run this game.") );
		FPlatformMisc::RequestExit( false );
	}

	// initialize the file SHA hash mapping
	InitSHAHashes();
}


void FWindowsPlatformMisc::PlatformInit()
{
	FGenericPlatformMisc::LogNameEventStatsInit();

#if defined(_MSC_VER) && _MSC_VER == 1800 && PLATFORM_64BITS
	// Work around bug in the VS 2013 math libraries in 64bit on certain windows versions. http://connect.microsoft.com/VisualStudio/feedback/details/811093 has details, remove this when runtime libraries are fixed
	_set_FMA3_enable(0);
#endif

	// Set granularity of sleep and such to 1 ms.
	timeBeginPeriod( 1 );

	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	// Get CPU info.
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );

	// Register on the game thread.
	FWindowsPlatformStackWalk::RegisterOnModulesChanged();
}

void FWindowsPlatformMisc::PlatformTearDown()
{
	FPlatformProcess::CeaseBeingFirstInstance();
}

/**
 * Handler called for console events like closure, CTRL-C, ...
 *
 * @param Type Ctrl-C, Ctrl-Break, Close Console Log Window, Logoff, or Shutdown
 */
static BOOL WINAPI ConsoleCtrlHandler(DWORD CtrlType)
{
	// Broadcast the termination the first time through.
	bool IsRequestingExit = IsEngineExitRequested();
	static bool AppTermDelegateBroadcast = false;
	if (!AppTermDelegateBroadcast)
	{
		RequestEngineExit(TEXT("ConsoleCtrl RequestExit"));
		FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
		AppTermDelegateBroadcast = true;
	}

	// Only "two-step Ctrl-C" if the termination event is Ctrl-C and the process
	// is considered interactive. Hard-terminate on all other cases.
	if (CtrlType != CTRL_C_EVENT || FApp::IsUnattended())
	{
		IsRequestingExit = true;
	}

	if (!IsRequestingExit)
	{
		UE_LOG(LogCore, Warning, TEXT("*** INTERRUPTED *** : SHUTTING DOWN"));
		UE_LOG(LogCore, Warning, TEXT("*** INTERRUPTED *** : CTRL-C TO FORCE QUIT"));
	}

	// make sure as much data is written to disk as possible
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
	}

	if (!IsRequestingExit)
	{
		// We'll two-step Ctrl-C events to give processes like servers time to
		// correctly terminate. Note the IsEngineExitRequested() is true now.
		return true;
	}

	// There's no guarantee that the process is paying attention to IsEngineExitRequested()
	// (e.g. some long-running commandlets). Using that for shutdown is therefore not
	// reliable. Deferring to the default CtrlHandler is also no good as that calls
	// ExitProcess() which will terminate all threads and detach all DLLS. This can
	// result in deadlocks and/or asserts as each DLL's atexit() is processed. So
	// let's hard terminate the process. 0xc000013a is what Windows' default handler
	// would normally pass to ExitProcess() and how ExitProc() terminates threads.
	TerminateProcess(GetCurrentProcess(), 0xc000013au);
	return false;
}

void FWindowsPlatformMisc::SetGracefulTerminationHandler()
{
	if (GetConsoleWindow() == nullptr)
	{
		return;
	}

	// Set console control handler so we can exit if requested.
	SetConsoleCtrlHandler(ConsoleCtrlHandler, true);

#if !UE_BUILD_SHIPPING && PLATFORM_CPU_X86_FAMILY
	// There are many places that can register a Ctrl-C handler, some of which
	// we are not in control of (third party Perforce libraries for example). This
	// can result in Ctrl-C doing nothing, or an abrupt call to ExitProcess() which
	// will cause asserts and/or deadlocks. So we're going to apply a patch so no
	// one else can register a handler.
	auto* SetCtrlCProc = (uint8*)SetConsoleCtrlHandler;
	if (SetCtrlCProc[0] == 0xff && SetCtrlCProc[1] == 0x25)
	{
#if PLATFORM_64BITS
		// Follow a possible "jmp [eip] + disp32" instruction to get to the actual
		// implementation of the SetConsoleCtrlHandler function.
		SetCtrlCProc = *(uint8**)(SetCtrlCProc + 6 + *(uint32*)(SetCtrlCProc + 2));
#else
		// Follow "jmp [disp32]"
		uintptr_t Disp32 = *(uintptr_t*)(SetCtrlCProc + 2);
		SetCtrlCProc = *(uint8**)(Disp32);
#endif
	}

	// Patch the start of the SetConsoleCtrlHandler function.
	uint8 Patch[] = {
		0xb8, 0x01, 0x00, 0x00, 0x00,	// mov eax, 1
		0xc3							// ret
	};
	DWORD PrevProtection;
	if (VirtualProtect(SetCtrlCProc, sizeof(Patch), PAGE_EXECUTE_READWRITE, &PrevProtection))
	{
		memcpy(SetCtrlCProc, Patch, sizeof(Patch));
		VirtualProtect(SetCtrlCProc, sizeof(Patch), PrevProtection, &PrevProtection);
		FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	}
#endif // !UE_BUILD_SHIPPING && PLATFORM_CPU_X86_FAMILY
}

void FWindowsPlatformMisc::CallGracefulTerminationHandler()
{
	ConsoleCtrlHandler(CTRL_CLOSE_EVENT);
}

static ECrashHandlingType GCrashHandlingType; /* = ECrashHandlingType::Default */

ECrashHandlingType FWindowsPlatformMisc::GetCrashHandlingType()
{
	return GCrashHandlingType;
}

ECrashHandlingType FWindowsPlatformMisc::SetCrashHandlingType(ECrashHandlingType Type)
{
	GCrashHandlingType = Type;
	return GCrashHandlingType;
}

int32 FWindowsPlatformMisc::GetMaxPathLength()
{
	struct FLongPathsEnabled
	{
		bool bValue;

		FLongPathsEnabled()
		{
			HMODULE Handle = GetModuleHandle(TEXT("ntdll.dll"));
			if (Handle == NULL)
			{
				bValue = false;
			}
			else
			{
				typedef BOOLEAN(NTAPI *RtlAreLongPathsEnabledFunc)();
				RtlAreLongPathsEnabledFunc RtlAreLongPathsEnabled = (RtlAreLongPathsEnabledFunc)(void*)GetProcAddress(Handle, "RtlAreLongPathsEnabled");
				bValue = (RtlAreLongPathsEnabled != NULL && RtlAreLongPathsEnabled());
			}
		}
	};

	static FLongPathsEnabled LongPathsEnabled;
	return LongPathsEnabled.bValue? 32767 : MAX_PATH;
}

void FWindowsPlatformMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	uint32 Error = ::GetEnvironmentVariableW(VariableName, Result, ResultLength);
	if (Error <= 0)
	{		
		*Result = TEXT('\0');
	}
}

FString FWindowsPlatformMisc::GetEnvironmentVariable(const TCHAR* VariableName)
{
	// Allocate the data for the string. Loop in case the variable happens to change while running, or the buffer isn't large enough.
	FString Buffer;
	for(uint32 Length = 128;;)
	{
		TArray<TCHAR, FString::AllocatorType>& CharArray = Buffer.GetCharArray();
		CharArray.SetNumUninitialized(Length);

		Length = ::GetEnvironmentVariableW(VariableName, CharArray.GetData(), CharArray.Num());
		if (Length == 0)
		{
			Buffer.Reset();
			break;
		}
		else if (Length < (uint32)CharArray.Num())
		{
			CharArray.SetNum(Length + 1);
			break;
		}
	}
	return Buffer;
}

void FWindowsPlatformMisc::SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value)
{
	uint32 Error = ::SetEnvironmentVariable(VariableName, Value);
	if (Error == 0)
	{
		UE_LOG(LogWindows, Warning, TEXT("Failed to set EnvironmentVariable: %s to : %s"), VariableName, Value);
	}
}

TArray<uint8> FWindowsPlatformMisc::GetMacAddress()
{
	TArray<uint8> Result;
	IP_ADAPTER_INFO IpAddresses[16];
	ULONG OutBufferLength = sizeof(IP_ADAPTER_INFO) * 16;
	// Read the adapters
	uint32 RetVal = GetAdaptersInfo(IpAddresses,&OutBufferLength);
	if (RetVal == NO_ERROR)
	{
		PIP_ADAPTER_INFO AdapterList = IpAddresses;
		// Walk the set of addresses copying each one
		while (AdapterList)
		{
			// If there is an address to read
			if (AdapterList->AddressLength > 0)
			{
				// Copy the data and say we did
				Result.AddZeroed(AdapterList->AddressLength);
				FMemory::Memcpy(Result.GetData(),AdapterList->Address,AdapterList->AddressLength);
				break;
			}
			AdapterList = AdapterList->Next;
		}
	}
	return Result;
}

/**
 * We need to see if we are doing AutomatedPerfTesting and we are -unattended if we are then we have
 * crashed in some terrible way and we need to make certain we can kill -9 the devenv process /
 * vsjitdebugger.exe and any other processes that are still running
 */
static void HardKillIfAutomatedTesting()
{
	// so here 
	int32 FromCommandLine = 0;
	FParse::Value( FCommandLine::Get(), TEXT("AutomatedPerfTesting="), FromCommandLine );
	if(( FApp::IsUnattended() == true ) && ( FromCommandLine != 0 ) && ( FParse::Param(FCommandLine::Get(), TEXT("KillAllPopUpBlockingWindows")) == true ))
	{

		UE_LOG(LogWindows, Warning, TEXT("Attempting to run KillAllPopUpBlockingWindows"));

		// .bat files never seem to launch correctly with FPlatformProcess::CreateProc so we just use the FPlatformProcess::LaunchURL which will call ShellExecute
		// we don't really care about the return code in this case 
		FPlatformProcess::LaunchURL( TEXT("KillAllPopUpBlockingWindows.bat"), NULL, NULL );
	}
}


void FWindowsPlatformMisc::SubmitErrorReport( const TCHAR* InErrorHist, EErrorReportMode::Type InMode )
{
	if ((!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash) && !FParse::Param(FCommandLine::Get(), TEXT("CrashForUAT")))
	{
		HardKillIfAutomatedTesting();
	}
}

#if !UE_BUILD_SHIPPING
bool FWindowsPlatformMisc::IsDebuggerPresent()
{
	return !GIgnoreDebugger && !!::IsDebuggerPresent();
}

EProcessDiagnosticFlags FWindowsPlatformMisc::GetProcessDiagnostics()
{
	static EProcessDiagnosticFlags FoundDiagnostics = []() -> EProcessDiagnosticFlags
	{
		EProcessDiagnosticFlags Result = FGenericPlatformMisc::GetProcessDiagnostics();

		TCHAR* ImageFileName = PathFindFileName(FPlatformProcess::ExecutablePath());
		FString ImageFileSubkey = FString::Printf(TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s"), ImageFileName);

		// via https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/gflags-flag-table
		constexpr uint32 MemorySanitizerMask =
			0x00000010| // FLG_HEAP_ENABLE_TAIL_CHECK
			0x00000020| // FLG_HEAP_ENABLE_FREE_CHECK
			0x00000080| // FLG_HEAP_VALIDATE_ALL
			0x00000100| // FLG_APPLICATION_VERIFIER
			0x00000800| // FLG_HEAP_ENABLE_TAGGING
			0x00008000| // FLG_HEAP_ENABLE_TAG_BY_DLL
			0x00200000| // FLG_HEAP_DISABLE_COALESCING
			0x02000000; // FLG_HEAP_PAGE_ALLOCS

		DWORD Data, DataCount = sizeof(Data);
		if (ERROR_SUCCESS == RegGetValue(HKEY_LOCAL_MACHINE, *ImageFileSubkey, TEXT("GlobalFlag"), RRF_RT_REG_DWORD, nullptr, &Data, &DataCount))
		{
			if (MemorySanitizerMask & Data)
			{
				Result |= EProcessDiagnosticFlags::MemorySanitizer;
			}
		}

		return Result;
	}();

	return FoundDiagnostics;
}
#endif //!UE_BUILD_SHIPPING

#if STATS || ENABLE_STATNAMEDEVENTS
void FWindowsPlatformMisc::CustomNamedStat(const TCHAR* Text, float Value, const TCHAR* Graph, const TCHAR* Unit)
{
	FRAMEPRO_DYNAMIC_CUSTOM_STAT(TCHAR_TO_WCHAR(Text), Value, TCHAR_TO_WCHAR(Graph), TCHAR_TO_WCHAR(Unit), FRAMEPRO_COLOUR(255,255,255));
}

void FWindowsPlatformMisc::CustomNamedStat(const ANSICHAR* Text, float Value, const ANSICHAR* Graph, const ANSICHAR* Unit)
{
	FRAMEPRO_DYNAMIC_CUSTOM_STAT(Text, Value, Graph, Unit, FRAMEPRO_COLOUR(255,255,255));
}

void FWindowsPlatformMisc::BeginNamedEventFrame()
{
	FGenericPlatformMisc::TickStatNamedEvents();

#if FRAMEPRO_ENABLED
	FFrameProProfiler::FrameStart();
#endif
}

void FWindowsPlatformMisc::BeginNamedEvent(const struct FColor& Color, const TCHAR* Text)
{
	FGenericPlatformMisc::StatNamedEvent(Text);

#if FRAMEPRO_ENABLED
	FFrameProProfiler::PushEvent(Text);
#elif UE_EXTERNAL_PROFILING_ENABLED
	FExternalProfilerTrace::StartScopedEvent(Color, Text);
#endif
}

void FWindowsPlatformMisc::BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
{
	FGenericPlatformMisc::StatNamedEvent(Text);

#if FRAMEPRO_ENABLED
	FFrameProProfiler::PushEvent(Text);
#elif UE_EXTERNAL_PROFILING_ENABLED
	FExternalProfilerTrace::StartScopedEvent(Color, Text);
#endif
}

void FWindowsPlatformMisc::EndNamedEvent()
{
#if FRAMEPRO_ENABLED
	FFrameProProfiler::PopEvent();
#elif UE_EXTERNAL_PROFILING_ENABLED
	FExternalProfilerTrace::EndScopedEvent();
#endif
}
#endif // STATS || ENABLE_STATNAMEDEVENTS

bool FWindowsPlatformMisc::IsRemoteSession()
{
	return ::GetSystemMetrics(SM_REMOTESESSION) != 0;
}

void FWindowsPlatformMisc::SetUTF8Output()
{
	CA_SUPPRESS(6031)
	_setmode(_fileno(stdout), _O_U8TEXT);
}

void FWindowsPlatformMisc::LocalPrint( const TCHAR *Message )
{
#if USE_DEBUG_LOGGING
	OutputDebugString(Message);
#endif
}

void FWindowsPlatformMisc::RequestExit( bool Force )
{
	UE_LOG(LogWindows, Log,  TEXT("FPlatformMisc::RequestExit(%i)"), Force );

	// Legacy behavior that now calls through to RequestExitWithStatus
	if( Force )
	{
		RequestExitWithStatus(Force, GIsCriticalError ? 3 : 0);
	}
	else
	{
		RequestExitWithStatus(false, 0);
	}
}

void FWindowsPlatformMisc::RequestExitWithStatus(bool Force, uint8 ReturnCode)
{
	UE_LOG(LogWindows, Log, TEXT("FPlatformMisc::RequestExitWithStatus(%i, %i)"), Force, ReturnCode);

	RequestEngineExit(TEXT("Win RequestExit"));
	FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();

	if (Force)
	{
		// Force immediate exit. In case of an error set the exit code to 3.
		// Dangerous because config code isn't flushed, global destructors aren't called, etc.
		// Suppress abort message and MS reports.
		//_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
		//abort();

		// Make sure the log is flushed.
		if (GLog)
		{
			GLog->Flush();
		}

		TerminateProcess(GetCurrentProcess(), ReturnCode);
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		PostQuitMessage(ReturnCode);
	}
}

const TCHAR* FWindowsPlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = GetLastError();
	}
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), OutBuffer, BufferCount, NULL );
	TCHAR* Found = FCString::Strchr(OutBuffer,TEXT('\r'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	Found = FCString::Strchr(OutBuffer,TEXT('\n'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	return OutBuffer;
}

void FWindowsPlatformMisc::CreateGuid(FGuid& Result)
{
	verify( CoCreateGuid( (GUID*)&Result )==S_OK );
}


#define HOTKEY_YES			100
#define HOTKEY_NO			101
#define HOTKEY_CANCEL		102

/**
 * Helper global variables, used in MessageBoxDlgProc for set message text.
 */
static TCHAR* GMessageBoxText = NULL;
static TCHAR* GMessageBoxCaption = NULL;
/**
 * Used by MessageBoxDlgProc to indicate whether a 'Cancel' button is present and
 * thus 'Esc should be accepted as a hotkey.
 */
static bool GCancelButtonEnabled = false;

/**
 * Calculates button position and size, localize button text.
 * @param HandleWnd handle to dialog window
 * @param Text button text to localize
 * @param DlgItemId dialog item id
 * @param PositionX current button position (x coord)
 * @param PositionY current button position (y coord)
 * @return true if succeeded
 */
static bool SetDlgItem( HWND HandleWnd, const TCHAR* Text, int32 DlgItemId, int32* PositionX, int32* PositionY )
{
	SIZE SizeButton;
		
	HDC DC = CreateCompatibleDC( NULL );
	GetTextExtentPoint32( DC, Text, wcslen(Text), &SizeButton );
	DeleteDC(DC);
	DC = NULL;

	SizeButton.cx += 14;
	SizeButton.cy += 8;

	HWND Handle = GetDlgItem( HandleWnd, DlgItemId );
	if( Handle )
	{
		*PositionX -= ( SizeButton.cx + 5 );
		SetWindowPos( Handle, HWND_TOP, *PositionX, *PositionY - SizeButton.cy, SizeButton.cx, SizeButton.cy, 0 );
		SetDlgItemText( HandleWnd, DlgItemId, Text );
		
		return true;
	}

	return false;
}

/**
 * Callback for MessageBoxExt dialog (allowing for Yes to all / No to all )
 * @return		One of EAppReturnType::Yes, EAppReturnType::YesAll, EAppReturnType::No, EAppReturnType::NoAll, EAppReturnType::Cancel.
 */
PTRINT CALLBACK MessageBoxDlgProc( HWND HandleWnd, uint32 Message, WPARAM WParam, LPARAM LParam )
{
	switch(Message)
	{
		case WM_INITDIALOG:
			{
				// Sets most bottom and most right position to begin button placement
				RECT Rect;
				POINT Point;
				
				GetWindowRect( HandleWnd, &Rect );
				Point.x = Rect.right;
				Point.y = Rect.bottom;
				ScreenToClient( HandleWnd, &Point );
				
				int32 PositionX = Point.x - 8;
				int32 PositionY = Point.y - 10;

				// Localize dialog buttons, sets position and size.
				FString CancelString;
				FString NoToAllString;
				FString NoString;
				FString YesToAllString;
				FString YesString;

				// The Localize* functions will return the Key if a dialog is presented before the config system is initialized.
				// Instead, we use hard-coded strings if config is not yet initialized.
				if( !GConfig )
				{
					CancelString = TEXT("Cancel");
					NoToAllString = TEXT("No to All");
					NoString = TEXT("No");
					YesToAllString = TEXT("Yes to All");
					YesString = TEXT("Yes");
				}
				else
				{
					CancelString = NSLOCTEXT("UnrealEd", "Cancel", "Cancel").ToString();
					NoToAllString = NSLOCTEXT("UnrealEd", "NoToAll", "No to All").ToString();
					NoString = NSLOCTEXT("UnrealEd", "No", "No").ToString();
					YesToAllString = NSLOCTEXT("UnrealEd", "YesToAll", "Yes to All").ToString();
					YesString = NSLOCTEXT("UnrealEd", "Yes", "Yes").ToString();
				}
				SetDlgItem( HandleWnd, *CancelString, IDC_CANCEL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *NoToAllString, IDC_NOTOALL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *NoString, IDC_NO_B, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *YesToAllString, IDC_YESTOALL, &PositionX, &PositionY );
				SetDlgItem( HandleWnd, *YesString, IDC_YES, &PositionX, &PositionY );

				SetDlgItemText( HandleWnd, IDC_MESSAGE, GMessageBoxText );
				SetWindowText( HandleWnd, GMessageBoxCaption );

				// If parent window exist, get it handle and make it foreground.
				HWND ParentWindow = GetTopWindow( HandleWnd );
				if( ParentWindow )
				{
					SetWindowPos( ParentWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				}

				SetForegroundWindow( HandleWnd );
				SetWindowPos( HandleWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

				RegisterHotKey( HandleWnd, HOTKEY_YES, 0, 'Y' );
				RegisterHotKey( HandleWnd, HOTKEY_NO, 0, 'N' );
				if ( GCancelButtonEnabled )
				{
					RegisterHotKey( HandleWnd, HOTKEY_CANCEL, 0, VK_ESCAPE );
				}

				// Windows are foreground, make them not top most.
				SetWindowPos( HandleWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				if( ParentWindow )
				{
					SetWindowPos( ParentWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
				}

				return true;
			}
		case WM_DESTROY:
			{
				UnregisterHotKey( HandleWnd, HOTKEY_YES );
				UnregisterHotKey( HandleWnd, HOTKEY_NO );
				if ( GCancelButtonEnabled )
				{
					UnregisterHotKey( HandleWnd, HOTKEY_CANCEL );
				}
				return true;
			}
		case WM_COMMAND:
			switch( LOWORD( WParam ) )
			{
				case IDC_YES:
					EndDialog( HandleWnd, EAppReturnType::Yes );
					break;
				case IDC_YESTOALL:
					EndDialog( HandleWnd, EAppReturnType::YesAll );
					break;
				case IDC_NO_B:
					EndDialog( HandleWnd, EAppReturnType::No );
					break;
				case IDC_NOTOALL:
					EndDialog( HandleWnd, EAppReturnType::NoAll );
					break;
				case IDC_CANCEL:
					if ( GCancelButtonEnabled )
					{
						EndDialog( HandleWnd, EAppReturnType::Cancel );
					}
					break;
			}
			break;
		case WM_HOTKEY:
			switch( WParam )
			{
			case HOTKEY_YES:
				EndDialog( HandleWnd, EAppReturnType::Yes );
				break;
			case HOTKEY_NO:
				EndDialog( HandleWnd, EAppReturnType::No );
				break;
			case HOTKEY_CANCEL:
				if ( GCancelButtonEnabled )
				{
					EndDialog( HandleWnd, EAppReturnType::Cancel );
				}
				break;
			}
			break;
		default:
			return false;
	}
	return true;
}

/**
 * Displays extended message box allowing for YesAll/NoAll
 * @return 3 - YesAll, 4 - NoAll, -1 for Fail
 */
int MessageBoxExtInternal( EAppMsgType::Type MsgType, HWND HandleWnd, const TCHAR* Text, const TCHAR* Caption )
{
	GMessageBoxText = (TCHAR *) Text;
	GMessageBoxCaption = (TCHAR *) Caption;

	switch (MsgType)
	{
		case EAppMsgType::YesNoYesAllNoAll:
		{
			GCancelButtonEnabled = false;
			return (int)DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_YESNO2ALL), HandleWnd, MessageBoxDlgProc);
		}
		case EAppMsgType::YesNoYesAllNoAllCancel:
		{
			GCancelButtonEnabled = true;
			return (int)DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_YESNO2ALLCANCEL), HandleWnd, MessageBoxDlgProc);
		}
		case EAppMsgType::YesNoYesAll:
		{
			GCancelButtonEnabled = false;
			return (int)DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_YESNOYESTOALL), HandleWnd, MessageBoxDlgProc);
		}
	}

	return -1;
}




EAppReturnType::Type FWindowsPlatformMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
	FSlowHeartBeatScope SuspendHeartBeat;

	HWND ParentWindow = (HWND)NULL;
	switch( MsgType )
	{
	case EAppMsgType::Ok:
		{
			MessageBox(ParentWindow, Text, Caption, MB_OK|MB_SYSTEMMODAL);
			return EAppReturnType::Ok;
		}
	case EAppMsgType::YesNo:
		{
			int32 Return = MessageBox( ParentWindow, Text, Caption, MB_YESNO|MB_SYSTEMMODAL );
			return Return == IDYES ? EAppReturnType::Yes : EAppReturnType::No;
		}
	case EAppMsgType::OkCancel:
		{
			int32 Return = MessageBox( ParentWindow, Text, Caption, MB_OKCANCEL|MB_SYSTEMMODAL );
			return Return == IDOK ? EAppReturnType::Ok : EAppReturnType::Cancel;
		}
	case EAppMsgType::YesNoCancel:
		{
			int32 Return = MessageBox(ParentWindow, Text, Caption, MB_YESNOCANCEL | MB_ICONQUESTION | MB_SYSTEMMODAL);
			return Return == IDYES ? EAppReturnType::Yes : (Return == IDNO ? EAppReturnType::No : EAppReturnType::Cancel);
		}
	case EAppMsgType::CancelRetryContinue:
		{
			int32 Return = MessageBox(ParentWindow, Text, Caption, MB_CANCELTRYCONTINUE | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_SYSTEMMODAL);
			return Return == IDCANCEL ? EAppReturnType::Cancel : (Return == IDTRYAGAIN ? EAppReturnType::Retry : EAppReturnType::Continue);
		}
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		return (EAppReturnType::Type)MessageBoxExtInternal( EAppMsgType::YesNoYesAllNoAll, ParentWindow, Text, Caption );
		//These return codes just happen to match up with ours.
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		return (EAppReturnType::Type)MessageBoxExtInternal( EAppMsgType::YesNoYesAllNoAllCancel, ParentWindow, Text, Caption );
		//These return codes just happen to match up with ours.
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll, 4 for Cancel
		break;

	case EAppMsgType::YesNoYesAll:
		return (EAppReturnType::Type)MessageBoxExtInternal(EAppMsgType::YesNoYesAll, ParentWindow, Text, Caption);
		//These return codes just happen to match up with ours.
		// return 0 for No, 1 for Yes, 2 for YesToAll
		break;

	default:
		break;
	}
	return EAppReturnType::Cancel;
}

static bool HandleGameExplorerIntegration()
{
	// skip this if running on WindowsServer (we get rare crashes that seem to stem from Windows Server builds, where GameExplorer isn't particularly useful)
	if (FPlatformProperties::SupportsWindowedMode() && !IsWindowsServer())
	{
		TCHAR AppPath[MAX_PATH];
		GetModuleFileName(NULL, AppPath, MAX_PATH - 1);

		// Initialize COM. We only want to do this once and not override settings of previous calls.
		if (!FWindowsPlatformMisc::CoInitialize())
		{
			return false;
		}
		
		// check to make sure we are able to run, based on parental rights
		IGameExplorer* GameExp;
		HRESULT hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**) &GameExp);

		BOOL bHasAccess = 1;
		BSTR AppPathBSTR = SysAllocString(AppPath);

		// @todo: This will allow access if the CoCreateInstance fails, but it should probaly disallow 
		// access if OS is Vista and it fails, succeed for XP
		if (SUCCEEDED(hr) && GameExp)
		{
			GameExp->VerifyAccess(AppPathBSTR, &bHasAccess);
		}


		// Guid for testing GE (un)installation
		static const GUID GEGuid = 
		{ 0x7089dd1d, 0xfe97, 0x4cc8, { 0x8a, 0xac, 0x26, 0x3e, 0x44, 0x1f, 0x3c, 0x42 } };

		// add the game to the game explorer if desired
		if (FParse::Param( FCommandLine::Get(), TEXT("installge")))
		{
			if (bHasAccess && GameExp)
			{
				BSTR AppDirBSTR = SysAllocString(FPlatformProcess::BaseDir());
				GUID Guid = GEGuid;
				hr = GameExp->AddGame(AppPathBSTR, AppDirBSTR, FParse::Param( FCommandLine::Get(), TEXT("allusers")) ? GIS_ALL_USERS : GIS_CURRENT_USER, &Guid);

				bool bWasSuccessful = false;
				// if successful
				if (SUCCEEDED(hr))
				{
					// get location of app local dir
					TCHAR UserPath[MAX_PATH];
					SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, UserPath);

					// convert guid to a string
					TCHAR GuidDir[MAX_PATH];
					CA_SUPPRESS(6031)
					StringFromGUID2(GEGuid, GuidDir, MAX_PATH - 1);

					// make the base path for all tasks
					FString BaseTaskDirectory = FString(UserPath) + TEXT("\\Microsoft\\Windows\\GameExplorer\\") + GuidDir;

					// make full paths for play and support tasks
					FString PlayTaskDirectory = BaseTaskDirectory + TEXT("\\PlayTasks");
					FString SupportTaskDirectory = BaseTaskDirectory + TEXT("\\SupportTasks");
				
					// make sure they exist
					IFileManager::Get().MakeDirectory(*PlayTaskDirectory, true);
					IFileManager::Get().MakeDirectory(*SupportTaskDirectory, true);

					// interface for creating a shortcut
					IShellLink* Link;
					hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,	IID_IShellLink, (void**)&Link);

					// get the persistent file interface of the link
					IPersistFile* LinkFile;
					Link->QueryInterface(IID_IPersistFile, (void**)&LinkFile);

					Link->SetPath(AppPath);

					// create all of our tasks

					// first is just the game
					Link->SetArguments(TEXT(""));
					Link->SetDescription(TEXT("Play"));
					IFileManager::Get().MakeDirectory(*(PlayTaskDirectory + TEXT("\\0")), true);
					LinkFile->Save(*(PlayTaskDirectory + TEXT("\\0\\Play.lnk")), true);

					Link->SetArguments(TEXT("editor"));
					Link->SetDescription(TEXT("Editor"));
					IFileManager::Get().MakeDirectory(*(PlayTaskDirectory + TEXT("\\1")), true);
					LinkFile->Save(*(PlayTaskDirectory + TEXT("\\1\\Editor.lnk")), true);

					LinkFile->Release();
					Link->Release();

					IUniformResourceLocator* InternetLink;
					CA_SUPPRESS(6031)
					CoCreateInstance (CLSID_InternetShortcut, NULL, 
						CLSCTX_INPROC_SERVER, IID_IUniformResourceLocator, (LPVOID*) &InternetLink);

					InternetLink->QueryInterface(IID_IPersistFile, (void**)&LinkFile);

					// make an internet shortcut
					InternetLink->SetURL(TEXT("http://www.unrealtournament3.com/"), 0);
					IFileManager::Get().MakeDirectory(*(SupportTaskDirectory + TEXT("\\0")), true);
					LinkFile->Save(*(SupportTaskDirectory + TEXT("\\0\\UT3.url")), true);

					LinkFile->Release();
					InternetLink->Release();
				}

				if ( SUCCEEDED(hr) ) 
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerInstallationSuccessful", "GameExplorer installation was successful, quitting now.") );
				}
				else
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerInstallationFailed", "GameExplorer installation was a failure, quitting now.") );
				}

				SysFreeString(AppDirBSTR);
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerInstallationFailedDoToAccessPermissions", "GameExplorer installation failed because you don't have access (check parental control levels and that you are running XP). You should not need Admin access"));
			}

			// free the string and shutdown COM
			SysFreeString(AppPathBSTR);
			SAFE_RELEASE(GameExp);
			FWindowsPlatformMisc::CoUninitialize();

			return false;
		}
		else if (FParse::Param( FCommandLine::Get(), TEXT("uninstallge")))
		{
			if (GameExp)
			{
				hr = GameExp->RemoveGame(GEGuid);
				if ( SUCCEEDED(hr) ) 
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerUninstallationSuccessful", "GameExplorer uninstallation was successful, quitting now.") );
				}
				else
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerUninstallationFailed", "GameExplorer uninstallation was a failure, quitting now.") );
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("WindowsPlatform", "GameExplorerUninstallationFailedDoToNotRunningVista", "GameExplorer uninstallation failed because you are probably not running Vista."));
			}

			// free the string and shutdown COM
			SysFreeString(AppPathBSTR);
			SAFE_RELEASE(GameExp);
			FWindowsPlatformMisc::CoUninitialize();

			return false;
		}

		// free the string and shutdown COM
		SysFreeString(AppPathBSTR);
		SAFE_RELEASE(GameExp);
		FWindowsPlatformMisc::CoUninitialize();

		// if we don't have access, we must quit ASAP after showing a message
		if (!bHasAccess)
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_ParentalControls", "The current level of parental controls do not allow you to run this game." ) );
			return false;
		}
	}
	return true;
}

#if WITH_FIREWALL_SUPPORT
/** 
 * Get the INetFwProfile interface for current profile
 */
INetFwProfile* GetFirewallProfile( void )
{
	HRESULT hr;
	INetFwMgr* pFwMgr = NULL;
	INetFwPolicy* pFwPolicy = NULL;
	INetFwProfile* pFwProfile = NULL;

	// Create an instance of the Firewall settings manager
	hr = CoCreateInstance( __uuidof( NetFwMgr ), NULL, CLSCTX_INPROC_SERVER, __uuidof( INetFwMgr ), ( void** )&pFwMgr );
	if( SUCCEEDED( hr ) )
	{
		hr = pFwMgr->get_LocalPolicy( &pFwPolicy );
		if( SUCCEEDED( hr ) )
		{
			pFwPolicy->get_CurrentProfile( &pFwProfile );
		}
	}

	// Cleanup
	if( pFwPolicy )
	{
		pFwPolicy->Release();
	}
	if( pFwMgr )
	{
		pFwMgr->Release();
	}

	return( pFwProfile );
}
#endif

static bool HandleFirewallIntegration()
{
#if WITH_FIREWALL_SUPPORT
	// only do with with the given commandlines
	if( !(FParse::Param( FCommandLine::Get(), TEXT( "installfw" ) ) || FParse::Param( FCommandLine::Get(), TEXT( "uninstallfw" ) )) )
#endif
	{
		return true; // allow the game to continue;
	}
#if WITH_FIREWALL_SUPPORT

	TCHAR AppPath[MAX_PATH];

	GetModuleFileName( NULL, AppPath, MAX_PATH - 1 );
	BSTR bstrGameExeFullPath = SysAllocString( AppPath );
	BSTR bstrFriendlyAppName = SysAllocString( TEXT( "Unreal Tournament 3" ) );

	if( bstrGameExeFullPath && bstrFriendlyAppName )
	{
		HRESULT hr = S_OK;
				
		if( FWindowsPlatformMisc::CoInitialize() )
		{
			INetFwProfile* pFwProfile = GetFirewallProfile();
			if( pFwProfile )
			{
				INetFwAuthorizedApplications* pFwApps = NULL;

				hr = pFwProfile->get_AuthorizedApplications( &pFwApps );
				if( SUCCEEDED( hr ) && pFwApps ) 
				{
					// add the game to the game explorer if desired
					if( FParse::Param( CmdLine, TEXT( "installfw" ) ) )
					{
						INetFwAuthorizedApplication* pFwApp = NULL;

						// Create an instance of an authorized application.
						hr = CoCreateInstance( __uuidof( NetFwAuthorizedApplication ), NULL, CLSCTX_INPROC_SERVER, __uuidof( INetFwAuthorizedApplication ), ( void** )&pFwApp );
						if( SUCCEEDED( hr ) && pFwApp )
						{
							// Set the process image file name.
							hr = pFwApp->put_ProcessImageFileName( bstrGameExeFullPath );
							if( SUCCEEDED( hr ) )
							{
								// Set the application friendly name.
								hr = pFwApp->put_Name( bstrFriendlyAppName );
								if( SUCCEEDED( hr ) )
								{
									// Add the application to the collection.
									hr = pFwApps->Add( pFwApp );
								}
							}

							pFwApp->Release();
						}
					}
					else if( FParse::Param( CmdLine, TEXT( "uninstallfw" ) ) )
					{
						// Remove the application from the collection.
						hr = pFwApps->Remove( bstrGameExeFullPath );
					}

					pFwApps->Release();
				}

				pFwProfile->Release();
			}

			FWindowsPlatformMisc::CoUninitialize();
		}

		SysFreeString( bstrFriendlyAppName );
		SysFreeString( bstrGameExeFullPath );
	}
	return false; // terminate the game
#endif // WITH_FIREWALL_SUPPORT
}

static bool HandleFirstInstall()
{
	if (FParse::Param( FCommandLine::Get(), TEXT("firstinstall")))
	{
		GLog->Flush();

		// Flush config to ensure culture changes are written to disk.
		GConfig->Flush(false);

		return false; // terminate the game
	}
	return true; // allow the game to continue;
}

bool FWindowsPlatformMisc::CommandLineCommands()
{
	return HandleFirstInstall() && HandleGameExplorerIntegration() && HandleFirewallIntegration();
}

/**
 * Detects whether we're running in a 64-bit operating system.
 *
 * @return	true if we're running in a 64-bit operating system
 */
bool FWindowsPlatformMisc::Is64bitOperatingSystem()
{
#if PLATFORM_64BITS
	return true;
#else
	#pragma warning( push )
	#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress( GetModuleHandle(TEXT("kernel32")), "IsWow64Process" );
	BOOL bIsWoW64Process = 0;
	if ( fnIsWow64Process != NULL )
	{
		if ( fnIsWow64Process(GetCurrentProcess(), &bIsWoW64Process) == 0 )
		{
			bIsWoW64Process = 0;
		}
	}
	#pragma warning( pop )
	return bIsWoW64Process == 1;
#endif
}

bool FWindowsPlatformMisc::VerifyWindowsVersion(uint32 MajorVersion, uint32 MinorVersion, uint32 BuildNumber)
{
	OSVERSIONINFOEX Version;
	Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	Version.dwMajorVersion = MajorVersion;
	Version.dwMinorVersion = MinorVersion;
	Version.dwBuildNumber  = BuildNumber;

	ULONGLONG ConditionMask = 0;
	ConditionMask = VerSetConditionMask(ConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	ConditionMask = VerSetConditionMask(ConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	ConditionMask = VerSetConditionMask(ConditionMask, VER_BUILDNUMBER,  VER_GREATER_EQUAL);

	return !!VerifyVersionInfo(&Version, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, ConditionMask);
}

bool FWindowsPlatformMisc::IsValidAbsolutePathFormat(const FString& Path)
{
	bool bIsValid = true;
	const FString OnlyPath = FPaths::GetPath(Path);
	if ( OnlyPath.IsEmpty() )
	{
		bIsValid = false;
	}

	// Must begin with a drive letter
	if ( bIsValid && !FChar::IsAlpha(OnlyPath[0]) )
	{
		bIsValid = false;
	}

	// On Windows the path must be absolute, i.e: "D:/" or "D:\\"
	if ( bIsValid && !(Path.Find(TEXT(":/"))==1 || Path.Find(TEXT(":\\"))==1) )
	{
		bIsValid = false;
	}

	// Find any unnamed directory changes
	if ( bIsValid && (Path.Find(TEXT("//"))!=INDEX_NONE) || (Path.Find(TEXT("\\/"))!=INDEX_NONE) || (Path.Find(TEXT("/\\"))!=INDEX_NONE) || (Path.Find(TEXT("\\\\"))!=INDEX_NONE) )
	{
		bIsValid = false;
	}

	// ensure there's no further instances of ':' in the string
	if ( bIsValid && !(Path.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 2)==INDEX_NONE) )
	{
		bIsValid = false;
	}

	return bIsValid;
}

static void QueryCpuInformation(FProcessorGroupDesc& OutGroupDesc, uint32& OutNumaNodeCount, uint32& OutCoreCount, uint32& OutLogicalProcessorCount, bool bForceSingleNumaNode = false)
{
	GROUP_AFFINITY FilterGroupAffinity = {};

	if (bForceSingleNumaNode)
	{
		PROCESSOR_NUMBER ProcessorNumber = {};
		USHORT NodeNumber = 0;

		GetThreadIdealProcessorEx(GetCurrentThread(), &ProcessorNumber);
		GetNumaProcessorNodeEx(&ProcessorNumber, &NodeNumber);
		GetNumaNodeProcessorMaskEx(NodeNumber, &FilterGroupAffinity);
	}

	OutNumaNodeCount = OutCoreCount = OutLogicalProcessorCount = 0;
	uint8* BufferPtr = nullptr;
	DWORD BufferBytes = 0;

	if (false == GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) BufferPtr, &BufferBytes))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			BufferPtr = reinterpret_cast<uint8*>(FMemory::Malloc(BufferBytes));

			if (GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) BufferPtr, &BufferBytes))
			{
				uint8* InfoPtr = BufferPtr;

				while (InfoPtr < BufferPtr + BufferBytes)
				{
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ProcessorInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) InfoPtr;

					if (nullptr == ProcessorInfo)
					{
						break;
					}

					if (ProcessorInfo->Relationship == RelationProcessorCore)
					{
						if (bForceSingleNumaNode)
						{
							for (int GroupIdx = 0; GroupIdx < ProcessorInfo->Processor.GroupCount; ++GroupIdx)
							{
								if (FilterGroupAffinity.Group == ProcessorInfo->Processor.GroupMask[GroupIdx].Group)
								{
									KAFFINITY Intersection = FilterGroupAffinity.Mask & ProcessorInfo->Processor.GroupMask[GroupIdx].Mask;

									if (Intersection > 0)
									{
										OutCoreCount++;

										OutLogicalProcessorCount += FMath::CountBits(Intersection);
									}
								}
							}
						}
						else
						{
							OutCoreCount++;

							for (int GroupIdx = 0; GroupIdx < ProcessorInfo->Processor.GroupCount; ++GroupIdx)
							{
								OutLogicalProcessorCount += FMath::CountBits(ProcessorInfo->Processor.GroupMask[GroupIdx].Mask);
							}
						}
					}
					if (ProcessorInfo->Relationship == RelationNumaNode)
					{
						OutNumaNodeCount++;
					}

					if (ProcessorInfo->Relationship == RelationGroup)
					{
						OutGroupDesc.NumProcessorGroups = FMath::Min<uint16>(FProcessorGroupDesc::MaxNumProcessorGroups, ProcessorInfo->Group.ActiveGroupCount);
						for(int32 GroupIndex = 0; GroupIndex < OutGroupDesc.NumProcessorGroups; GroupIndex++)
						{
							OutGroupDesc.ThreadAffinities[GroupIndex] = ProcessorInfo->Group.GroupInfo[GroupIndex].ActiveProcessorMask;
						}
					}

					InfoPtr += ProcessorInfo->Size;
				}
			}

			FMemory::Free(BufferPtr);
		}
	}
}

int32 FWindowsPlatformMisc::NumberOfCores()
{
	static int32 CoreCount = 0;
	if (CoreCount > 0)
	{
		return CoreCount;
	}

	FProcessorGroupDesc GroupDesc;
	uint32 NumaNodeCount = 0;
	uint32 NumCores = 0;
	uint32 LogicalProcessorCount = 0;
	QueryCpuInformation(GroupDesc, NumaNodeCount, NumCores, LogicalProcessorCount);

	bool bLimitsInitialized;
	int32 PhysicalCoreLimit;
	int32 LogicalCoreLimit;
	bool bSetPhysicalCountToLogicalCount;
	GetConfiguredCoreLimits(NumCores, LogicalProcessorCount, bLimitsInitialized, PhysicalCoreLimit,
		LogicalCoreLimit, bSetPhysicalCountToLogicalCount);

	CoreCount = bSetPhysicalCountToLogicalCount ? LogicalProcessorCount : NumCores;

	// Optionally limit number of threads (we don't necessarily scale super well with very high core counts)
	if (PhysicalCoreLimit > 0)
	{
		CoreCount = FMath::Min(CoreCount, PhysicalCoreLimit);
	}

	return CoreCount;
}

FProcessorGroupDesc NumberOfProcessorGroupsInternal()
{
	FProcessorGroupDesc GroupDesc;
	uint32 NumaNodeCount = 0;
	uint32 NumCores = 0;
	uint32 LogicalProcessorCount = 0;
	QueryCpuInformation(GroupDesc, NumaNodeCount, NumCores, LogicalProcessorCount);
	return GroupDesc;
}

const FProcessorGroupDesc& FWindowsPlatformMisc::GetProcessorGroupDesc()
{
	static FProcessorGroupDesc GroupDesc(NumberOfProcessorGroupsInternal());
	return GroupDesc;
}

int32 FWindowsPlatformMisc::NumberOfCoresIncludingHyperthreads()
{
	static int32 CoreCount = 0;
	if (CoreCount > 0)
	{
		return CoreCount;
	}

	FProcessorGroupDesc GroupDesc;
	uint32 NumaNodeCount = 0;
	uint32 NumCores = 0;
	uint32 LogicalProcessorCount = 0;
	QueryCpuInformation(GroupDesc, NumaNodeCount, NumCores, LogicalProcessorCount);

	bool bLimitsInitialized;
	int32 PhysicalCoreLimit;
	int32 LogicalCoreLimit;
	bool bSetPhysicalCountToLogicalCount;
	GetConfiguredCoreLimits(NumCores, LogicalProcessorCount, bLimitsInitialized, PhysicalCoreLimit,
		LogicalCoreLimit, bSetPhysicalCountToLogicalCount);

	CoreCount = LogicalProcessorCount;

	// Optionally limit number of threads (we don't necessarily scale super well with very high core counts)
	if (LogicalCoreLimit > 0)
	{
		CoreCount = FMath::Min(CoreCount, LogicalCoreLimit);
	}

	return CoreCount;
}

const TCHAR* FWindowsPlatformMisc::GetPlatformFeaturesModuleName()
{
	bool bModuleExists = FModuleManager::Get().ModuleExists(TEXT("WindowsPlatformFeatures"));
	// If running a dedicated server then we use the default PlatformFeatures
	if (bModuleExists && !IsRunningDedicatedServer())
	{
		UE_LOG(LogWindows, Log, TEXT("WindowsPlatformFeatures enabled"));
		return TEXT("WindowsPlatformFeatures");
	}
	else
	{
		UE_LOG(LogWindows, Log, TEXT("WindowsPlatformFeatures disabled or dedicated server build"));
		return nullptr;
	}
}

int32 FWindowsPlatformMisc::NumberOfWorkerThreadsToSpawn()
{	
	static int32 MaxServerWorkerThreads = 4;

	extern CORE_API int32 GUseNewTaskBackend;
	int32 MaxWorkerThreads = GUseNewTaskBackend ? INT32_MAX : 26;

	int32 NumberOfCores = FWindowsPlatformMisc::NumberOfCores();
	int32 NumberOfCoresIncludingHyperthreads = FWindowsPlatformMisc::NumberOfCoresIncludingHyperthreads();
	int32 NumberOfThreads = 0;

	if (NumberOfCoresIncludingHyperthreads > NumberOfCores)
	{
		NumberOfThreads = NumberOfCoresIncludingHyperthreads - 2;
	}
	else
	{
		NumberOfThreads = NumberOfCores - 1;
	}

	int32 MaxWorkerThreadsWanted = IsRunningDedicatedServer() ? MaxServerWorkerThreads : MaxWorkerThreads;
	// need to spawn at least one worker thread (see FTaskGraphImplementation)
	return FMath::Max(FMath::Min(NumberOfThreads, MaxWorkerThreadsWanted), 2);
}

bool FWindowsPlatformMisc::OsExecute(const TCHAR* CommandType, const TCHAR* Command, const TCHAR* CommandLine)
{
	HINSTANCE hApp = ShellExecute(NULL,
		CommandType,
		Command,
		CommandLine,
		NULL,
		SW_SHOWNORMAL);
	bool bSucceeded = hApp > (HINSTANCE)32;
	return bSucceeded;
}

struct FGetMainWindowHandleData
{
	HWND Handle;
	uint32 ProcessId;
};

int32 CALLBACK GetMainWindowHandleCallback(HWND Handle, LPARAM lParam)
{
	FGetMainWindowHandleData& Data = *(FGetMainWindowHandleData*)lParam;
	
	unsigned long ProcessId = 0;
	{
		::GetWindowThreadProcessId(Handle, &ProcessId);
	}
	
	if ((Data.ProcessId != ProcessId) || (::GetWindow(Handle, GW_OWNER) != (HWND)0) || !::IsWindowVisible(Handle))
	{
		return 1;
	}

	Data.Handle = Handle;

	return 0;
}

HWND FWindowsPlatformMisc::GetTopLevelWindowHandle(uint32 ProcessId)
{
	FGetMainWindowHandleData Data;
	{
		Data.Handle = 0;
		Data.ProcessId = ProcessId;
	}

	::EnumWindows(GetMainWindowHandleCallback, (LPARAM)&Data);

	return Data.Handle;
}

FORCENOINLINE void FWindowsPlatformMisc::RaiseException( uint32 ExceptionCode )
{
	/** This is the last place to gather memory stats before exception. */
	FGenericCrashContext::SetMemoryStats(FPlatformMemory::GetStats());

	::RaiseException( ExceptionCode, 0, 0, NULL );
}

bool FWindowsPlatformMisc::SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue)
{
	check(!InStoreId.IsEmpty());
	check(!InSectionName.IsEmpty());
	check(!InKeyName.IsEmpty());

	FString FullRegistryKey = FString(TEXT("Software")) / InStoreId / InSectionName;
	FullRegistryKey = FullRegistryKey.Replace(TEXT("/"), TEXT("\\")); // we use forward slashes, but the registry needs back slashes

	HKEY hKey;
	HRESULT Result = ::RegCreateKeyEx(HKEY_CURRENT_USER, *FullRegistryKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
	if(Result == ERROR_SUCCESS)
	{
		Result = ::RegSetValueEx(hKey, *InKeyName, 0, REG_SZ, (const BYTE*)*InValue, (InValue.Len() + 1) * sizeof(TCHAR));
		::RegCloseKey(hKey);
	}
	
	if(Result != ERROR_SUCCESS)
	{
		TCHAR ErrorBuffer[1024];
		::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, Result, 0, ErrorBuffer, 1024, nullptr);
		GWarn->Logf(TEXT("FWindowsPlatformMisc::SetStoredValue: ERROR: Could not store value for '%s'. Error Code %u: %s"), *InKeyName, Result, ErrorBuffer);
		return false;
	}

	return true;
}

bool FWindowsPlatformMisc::GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue)
{
	check(!InStoreId.IsEmpty());
	check(!InSectionName.IsEmpty());
	check(!InKeyName.IsEmpty());

	FString FullRegistryKey = FString(TEXT("Software")) / InStoreId / InSectionName;
	FullRegistryKey = FullRegistryKey.Replace(TEXT("/"), TEXT("\\")); // we use forward slashes, but the registry needs back slashes

	return QueryRegKey(HKEY_CURRENT_USER, *FullRegistryKey, *InKeyName, OutValue);
}

bool FWindowsPlatformMisc::DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName)
{
	// Deletes values in reg keys and also deletes the owning key if it becomes empty

	check(!InStoreId.IsEmpty());
	check(!InSectionName.IsEmpty());
	check(!InKeyName.IsEmpty());

	FString FullRegistryKey = FString(TEXT("Software")) / InStoreId / InSectionName;
	FullRegistryKey = FullRegistryKey.Replace(TEXT("/"), TEXT("\\")); // we use forward slashes, but the registry needs back slashes

	HKEY hKey;
	HRESULT Result = ::RegOpenKeyEx(HKEY_CURRENT_USER, *FullRegistryKey, 0, KEY_WRITE | KEY_READ, &hKey);
	if (Result == ERROR_SUCCESS)
	{
		Result = ::RegDeleteValue(hKey, *InKeyName);

		// Query for sub-keys in the open key
		TCHAR CheckKeyName[256];
		::DWORD CheckKeyNameLength = sizeof(CheckKeyName) / sizeof(CheckKeyName[0]);
		HRESULT EnumResult = RegEnumKeyEx(hKey, 0, CheckKeyName, &CheckKeyNameLength, NULL, NULL, NULL, NULL);
		bool bZeroSubKeys = EnumResult != ERROR_SUCCESS;

		// Query for a remaining value in the open key
		wchar_t CheckValueName[256];
		::DWORD CheckValueNameLength = sizeof(CheckValueName) / sizeof(CheckValueName[0]);
		EnumResult = RegEnumValue(hKey, 0, CheckValueName, &CheckValueNameLength, NULL, NULL, NULL, NULL);
		bool bZeroValues = EnumResult != ERROR_SUCCESS;

		::RegCloseKey(hKey);

		if (bZeroSubKeys && bZeroValues)
		{
			// No more values - delete the section
			::RegDeleteKey(HKEY_CURRENT_USER, *FullRegistryKey);
		}
	}

	return Result == ERROR_SUCCESS;
}

bool FWindowsPlatformMisc::DeleteStoredSection(const FString& InStoreId, const FString& InSectionName)
{
	check(!InStoreId.IsEmpty());
	check(!InSectionName.IsEmpty());

	FString FullRegistryKey = FString(TEXT("Software")) / InStoreId / InSectionName;
	FullRegistryKey = FullRegistryKey.Replace(TEXT("/"), TEXT("\\")); // we use forward slashes, but the registry needs back slashes

	return ::RegDeleteTree(HKEY_CURRENT_USER, *FullRegistryKey) == ERROR_SUCCESS;
}

FString FWindowsPlatformMisc::GetDefaultLanguage()
{
	// Only use GetUserPreferredUILanguages on Windows 8+ as older versions didn't always have language packs available
	if (FPlatformMisc::VerifyWindowsVersion(6, 2))
	{
		ULONG NumLanguages = 0;
		ULONG LangBufferSize = 0;
		if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &NumLanguages, nullptr, &LangBufferSize))
		{
			TArray<WCHAR> LangBuffer;
			LangBuffer.SetNumZeroed(LangBufferSize);
		
			if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &NumLanguages, LangBuffer.GetData(), &LangBufferSize))
			{
				// GetUserPreferredUILanguages returns a list where each item is null terminated, so this produces a string containing only the first item
				return FString(LangBuffer.GetData());
			}
		}
	}
	
	return GetDefaultLocale();
}

FString FWindowsPlatformMisc::GetDefaultLocale()
{
	WCHAR LocaleName[LOCALE_NAME_MAX_LENGTH];
	if (::GetUserDefaultLocaleName(LocaleName, LOCALE_NAME_MAX_LENGTH))
	{
		return FString(LocaleName);
	}

	return FGenericPlatformMisc::GetDefaultLocale();
}

uint32 FWindowsPlatformMisc::GetLastError()
{
	return (uint32)::GetLastError();
}

void FWindowsPlatformMisc::SetLastError(uint32 ErrorCode)
{
	::SetLastError((DWORD)ErrorCode);
}

bool FWindowsPlatformMisc::CoInitialize(ECOMModel Model)
{
	HRESULT hr = ::CoInitializeEx(NULL, (Model == ECOMModel::Singlethreaded) ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
	return hr == S_OK || hr == S_FALSE;
}

void FWindowsPlatformMisc::CoUninitialize()
{
	::CoUninitialize();
}

#if !UE_BUILD_SHIPPING
static TCHAR GErrorRemoteDebugPromptMessage[MAX_SPRINTF];

void FWindowsPlatformMisc::PromptForRemoteDebugging(bool bIsEnsure)
{
	if (bShouldPromptForRemoteDebugging)
	{
		if (bIsEnsure && !bPromptForRemoteDebugOnEnsure)
		{
			// Don't prompt on ensures unless overridden
			return;
		}

		if (FApp::IsUnattended())
		{
			// Do not ask if there is no one to show a message
			return;
		}

		if (GIsCriticalError && !GIsGuarded)
		{
			// A fatal error occurred.
			// We have not ability to debug, this does not make sense to ask.
			return;
		}

		// Upload locally compiled files for remote debugging
		FPlatformStackWalk::UploadLocalSymbols();

		FCString::Sprintf(GErrorRemoteDebugPromptMessage, 
			TEXT("Have a programmer remote debug this crash?\n"
			     "Hit NO to exit and submit error report as normal.\n"
			     "Otherwise, contact a programmer for remote debugging,\n"
			     "giving them the changelist number below.\n"
			     "Once they have confirmed they are connected to the machine,\n"
			     "hit YES to allow them to debug the crash.\n"
			     "[Changelist = %d]"),
			FEngineVersion::Current().GetChangelist());
		FSlowHeartBeatScope SuspendHeartBeat;
		if (MessageBox(0, GErrorRemoteDebugPromptMessage, TEXT("CRASHED"), MB_YESNO|MB_SYSTEMMODAL) == IDYES)
		{
			::DebugBreak();
		}
	}
}
#endif	//#if !UE_BUILD_SHIPPING

/**
* Class that caches __cpuid queried data.
*/
class FCPUIDQueriedData
{
public:
	FCPUIDQueriedData()
		: bHasCPUIDInstruction(CheckForCPUIDInstruction()), Vendor(), CPUInfo(0), CacheLineSize(PLATFORM_CACHE_LINE_SIZE)
	{
		if(bHasCPUIDInstruction)
		{
			GetCPUVendor(Vendor);
			GetCPUBrand(Brand);
			int Info[4];
			QueryCPUInfo(Info);
			CPUInfo = Info[0];
			CPUInfo2 = Info[2];
			CacheLineSize = QueryCacheLineSize();
			int ExtendedFeatures[4];
			QueryCPUExtendedFeatures(ExtendedFeatures);
			CPUExtendedFeatures2 = ExtendedFeatures[2];
		}
	}

	/** 
	 * Checks if this CPU supports __cpuid instruction.
	 *
	 * @returns True if this CPU supports __cpuid instruction. False otherwise.
	 */
	static bool HasCPUIDInstruction()
	{
		return CPUIDStaticCache.bHasCPUIDInstruction;
	}

	/**
	 * Gets pre-cached CPU vendor name.
	 *
	 * @returns CPU vendor name.
	 */
	static const ANSICHAR (&GetVendor())[12 + 1]
	{
		return CPUIDStaticCache.Vendor;
	}

	/**
	* Gets pre-cached CPU brand string.
	*
	* @returns CPU brand string.
	*/
	static const ANSICHAR (&GetBrand())[0x40]
	{
		return CPUIDStaticCache.Brand;
	}

	/**
	 * Gets __cpuid CPU info.
	 *
	 * @returns CPU info unsigned int queried using __cpuid.
	 */
	static uint32 GetCPUInfo()
	{
		return CPUIDStaticCache.CPUInfo;
	}

	/**
	* Gets __cpuid CPU info.
	*
	* @returns CPU info unsigned int queried using __cpuid.
	*/
	static uint32 GetCPUInfo2()
	{
		return CPUIDStaticCache.CPUInfo2;
	}

	/**
	* Gets __cpuidex CPU features.
	*
	* @returns CPU info unsigned int queried using __cpuidex.
	*/
	static uint32 GetCPUExtendedFeatures2()
	{
		return CPUIDStaticCache.CPUExtendedFeatures2;
	}

	/**
	 * Gets cache line size.
	 *
	 * @returns Cache line size.
	 */
	static int32 GetCacheLineSize()
	{
		return CPUIDStaticCache.CacheLineSize;
	}

private:
	/**
	 * Checks if __cpuid instruction is present on current machine.
	 *
	 * @returns True if this CPU supports __cpuid instruction. False otherwise.
	 */
	static bool CheckForCPUIDInstruction()
	{
		// all x86 64-bit CPUs support CPUID, no check required
#if PLATFORM_HAS_CPUID && PLATFORM_64BITS
		return true;
#else
	#if PLATFORM_SEH_EXCEPTIONS_DISABLED
		return false;
	#else
		__try
		{
			int Args[4];
			__cpuid(Args, 0);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		return true;
	#endif
#endif
	}

	/**
	 * Queries Vendor name using __cpuid instruction.
	 *
	 * @returns CPU vendor name.
	 */
	static void GetCPUVendor(ANSICHAR (&OutBuffer)[12 + 1])
	{
		union
		{
			char Buffer[12 + 1];
			struct
			{
				int dw0;
				int dw1;
				int dw2;
			} Dw;
		} VendorResult;

		int Args[4];
		__cpuid(Args, 0);

		VendorResult.Dw.dw0 = Args[1];
		VendorResult.Dw.dw1 = Args[3];
		VendorResult.Dw.dw2 = Args[2];
		VendorResult.Buffer[12] = 0;

		FMemory::Memcpy(OutBuffer, VendorResult.Buffer);
	}

	/**
	 * Queries brand string using __cpuid instruction.
	 *
	 * @returns CPU brand string.
	 */
	static void GetCPUBrand(ANSICHAR (&OutBrandString)[0x40])
	{
		// @see for more information http://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
		ANSICHAR BrandString[0x40] = {0};
		int32 CPUInfo[4] = {-1};
		const SIZE_T CPUInfoSize = sizeof( CPUInfo );

		__cpuid( CPUInfo, 0x80000000 );
		const uint32 MaxExtIDs = CPUInfo[0];

		if( MaxExtIDs >= 0x80000004 )
		{
			const uint32 FirstBrandString = 0x80000002;
			const uint32 NumBrandStrings = 3;
			for( uint32 Index = 0; Index < NumBrandStrings; Index++ )
			{
				__cpuid( CPUInfo, FirstBrandString + Index );
				FPlatformMemory::Memcpy( BrandString + CPUInfoSize * Index, CPUInfo, CPUInfoSize );
			}
		}

		FMemory::Memcpy(OutBrandString, BrandString);
	}

	/**
	 * Queries CPU info using __cpuid instruction.
	 *
	 * @returns CPU info unsigned int queried using __cpuid.
	 */
	static void QueryCPUInfo(int Args[4])
	{
		__cpuid(Args, 1);
	}

	/**
	 * Queries CPU info using __cpuid instruction.
	 *
	 * @returns CPU info unsigned int queried using __cpuidex.
	 */
	static void QueryCPUExtendedFeatures(int Args[4])
	{
		__cpuidex(Args, 7, 0);
	}

	/**
	 * Queries cache line size using __cpuid instruction.
	 *
	 * @returns Cache line size.
	 */
	static int32 QueryCacheLineSize()
	{
		int32 Result = 1;

		int Args[4];
		__cpuid(Args, 0x80000006);

		Result = Args[2] & 0xFF;
		check(Result && !(Result & (Result - 1))); // assumed to be a power of two

		return Result;
	}

	/** Static field with pre-cached __cpuid data. */
	static FCPUIDQueriedData CPUIDStaticCache;

	/** If machine has CPUID instruction. */
	bool bHasCPUIDInstruction;

	/** Vendor of the CPU. */
	ANSICHAR Vendor[12 + 1];

	/** CPU brand. */
	ANSICHAR Brand[0x40];

	/** CPU info from __cpuid. */
	uint32 CPUInfo;
	uint32 CPUInfo2;
	uint32 CPUExtendedFeatures2;

	/** CPU cache line size. */
	int32 CacheLineSize;
};

/** Static initialization of data to pre-cache __cpuid queries. */
FCPUIDQueriedData FCPUIDQueriedData::CPUIDStaticCache;

bool FWindowsPlatformMisc::HasCPUIDInstruction()
{
	return FCPUIDQueriedData::HasCPUIDInstruction();
}

FString FWindowsPlatformMisc::GetCPUVendor()
{
	return FCPUIDQueriedData::GetVendor();
}

FString FWindowsPlatformMisc::GetCPUBrand()
{
	return FCPUIDQueriedData::GetBrand();
}

bool FWindowsPlatformMisc::HasAVX2InstructionSupport()
{
	if (!HasCPUIDInstruction())
	{
		return false;
	}

	int flags[4];
	/* CPUID.(EAX=01H, ECX=0H):ECX.FMA[bit 12]==1   &&
	   CPUID.(EAX=01H, ECX=0H):ECX.MOVBE[bit 22]==1 &&
	   CPUID.(EAX=01H, ECX=0H):ECX.XSAVE[bit 26]==1 &&
	   CPUID.(EAX=01H, ECX=0H):ECX.OSXSAVE[bit 27]==1 &&
	   CPUID.(EAX=01H, ECX=0H):ECX.AVX[bit 28]==1 */
	const int FMA_MOVBE_XSAVE_OSXSAVE_AVX_BITS = (1 << 12) | (1 << 22) | (1 << 26) | (1 << 27) | (1 << 28);
	__cpuidex(flags, 1, 0);
	if ((flags[2] & FMA_MOVBE_XSAVE_OSXSAVE_AVX_BITS) != FMA_MOVBE_XSAVE_OSXSAVE_AVX_BITS)
	{
		return false;
	}

	/*  CPUID.(EAX=07H, ECX=0H):EBX.AVX2[bit 5]==1  &&
		CPUID.(EAX=07H, ECX=0H):EBX.BMI1[bit 3]==1  &&
		CPUID.(EAX=07H, ECX=0H):EBX.BMI2[bit 8]==1  */
	const int AVX2_BMI1_BMI2_BITS = (1 << 5) | (1 << 3) | (1 << 8);
	__cpuidex(flags, 7, 0);
	if ((flags[1] & AVX2_BMI1_BMI2_BITS) != AVX2_BMI1_BMI2_BITS)
	{
		return false;
	}

	/* CPUID.(EAX=80000001H):ECX.LZCNT[bit 5]==1 */
	const int LZCNT_BITS = (1 << 5);
	__cpuidex(flags, 0x80000001, 0);
	if ((flags[2] & LZCNT_BITS) != LZCNT_BITS)
	{
		return false;
	}

	// OS must save YMM registers between context switch
	if ((_xgetbv(0) & 6) != 6)
	{
		return false;
	}

	return true;
}

#include "Windows/AllowWindowsPlatformTypes.h"
FString FWindowsPlatformMisc::GetPrimaryGPUBrand()
{
	static FString PrimaryGPUBrand;
	if( PrimaryGPUBrand.IsEmpty() )
	{
		// Find primary display adapter and get the device name.
		PrimaryGPUBrand = FGenericPlatformMisc::GetPrimaryGPUBrand();

		DISPLAY_DEVICE DisplayDevice;
		DisplayDevice.cb = sizeof( DisplayDevice );
		DWORD DeviceIndex = 0;

		while( EnumDisplayDevices( 0, DeviceIndex, &DisplayDevice, 0 ) )
		{
			if( (DisplayDevice.StateFlags & (DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE)) > 0 )
			{
				PrimaryGPUBrand = DisplayDevice.DeviceString;
				break;
			}

			FMemory::Memzero( DisplayDevice );
			DisplayDevice.cb = sizeof( DisplayDevice );
			DeviceIndex++;
		}
	}

	return PrimaryGPUBrand;
}

#define USE_SP_ALTPLATFORM_INFO_V1 0
#define USE_SP_ALTPLATFORM_INFO_V3 1
#define USE_SP_DRVINFO_DATA_V1 0
#define USE_SP_BACKUP_QUEUE_PARAMS_V1 0
#define USE_SP_INF_SIGNER_INFO_V1 0
#include <SetupAPI.h>
#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#undef USE_SP_ALTPLATFORM_INFO_V1
#undef USE_SP_ALTPLATFORM_INFO_V3
#undef USE_SP_DRVINFO_DATA_V1
#undef USE_SP_BACKUP_QUEUE_PARAMS_V1
#undef USE_SP_INF_SIGNER_INFO_V1

static void GetVideoDriverDetailsFromSetup(const FString& DeviceName, FGPUDriverInfo& Out)
{
	
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_DISPLAY, NULL, NULL, DIGCF_PRESENT);

	FString RegistryKey = "";
	
	if (hDevInfo != INVALID_HANDLE_VALUE)
	{
		DWORD DataType = 0;
		
		const uint32 BufferSize = 512;
		TCHAR Buffer[BufferSize + 1] = { 0 };
		
		SP_DEVINFO_DATA DeviceInfoData;
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		bool bFound = false;
		for (int32 Idx = 0; SetupDiEnumDeviceInfo(hDevInfo, Idx, &DeviceInfoData); Idx++)
		{
			// Get the device description, check if it matches the queried device name
			if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_DriverDesc, &DataType,
				(PBYTE)Buffer, sizeof(Buffer), nullptr, 0))
			{
				if (DeviceName.Compare(Buffer) == 0)
				{
					Out.DeviceDescription = Buffer;
					bFound = true;
					ZeroMemory(Buffer, sizeof(Buffer));

					// Retrieve the registry key for this device for 3rd party data
					if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_Driver, &DataType,
						(PBYTE)Buffer, sizeof(Buffer), nullptr, 0))
					{
						RegistryKey = Buffer;
						ZeroMemory(Buffer, sizeof(Buffer));
					}
					else
					{
						UE_LOG(LogWindows, Log, TEXT("Failed to retrieve driver registry key for device %d"), Idx);
					}
					
					break;
				}
				ZeroMemory(Buffer, sizeof(Buffer));
			}
			else
			{
				UE_LOG(LogWindows, Log, TEXT("Failed to retrieve driver description for device %d"), Idx);
			}
		}

		if (bFound)
		{
			// Get the provider name
			if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_DriverProvider, &DataType,
				(PBYTE)Buffer, sizeof(Buffer), nullptr, 0))
			{
				Out.ProviderName = Buffer;
				ZeroMemory(Buffer, sizeof(Buffer));
			}
			else
			{
				UE_LOG(LogWindows, Log, TEXT("Failed to find provider name"));
			}
			// Get the internal driver version
			if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_DriverVersion, &DataType,
				(PBYTE)Buffer, sizeof(Buffer), nullptr, 0))
			{
				Out.InternalDriverVersion = Buffer;
				ZeroMemory(Buffer, sizeof(Buffer));
			}
			else
			{
				UE_LOG(LogWindows, Log, TEXT("Failed to find internal driver version"));
			}
			// Get the driver date
			FILETIME FileTime;
			if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_DriverDate, &DataType,
				(PBYTE)&FileTime, sizeof(FILETIME), nullptr, 0))
			{
				SYSTEMTIME SystemTime;
				FileTimeToSystemTime(&FileTime, &SystemTime);
				Out.DriverDate = FString::Printf(TEXT("%d-%d-%d"), SystemTime.wMonth, SystemTime.wDay, SystemTime.wYear);
			}
			else
			{
				UE_LOG(LogWindows, Log, TEXT("Failed to find driver date"));
			}
		}
		else
		{
			UE_LOG(LogWindows, Log, TEXT("Unable to find requested device '%s' using Setup API."), *DeviceName);
		}
		
		SetupDiDestroyDeviceInfoList(hDevInfo);
	}
	else
	{
		UE_LOG(LogWindows, Log, TEXT("Failed to initialize Setup API"));
	}

	if (!Out.ProviderName.IsEmpty())
	{
		if (Out.ProviderName.Find(TEXT("NVIDIA")) != INDEX_NONE)
		{
			Out.SetNVIDIA();
		}
		else if (Out.ProviderName.Find(TEXT("Advanced Micro Devices")) != INDEX_NONE)
		{
			Out.SetAMD();
		}
		else if (Out.ProviderName.Find(TEXT("Intel")) != INDEX_NONE)	// usually TEXT("Intel Corporation")
		{
			Out.SetIntel();
		}
	}

	Out.UserDriverVersion = Out.InternalDriverVersion;

	if(Out.IsNVIDIA())
	{
		Out.UserDriverVersion = Out.TrimNVIDIAInternalVersion(Out.InternalDriverVersion);
	}
	else if(Out.IsAMD() && !RegistryKey.IsEmpty())
	{
		// Get the AMD specific information directly from the registry
		// AMD AGS could be used instead, but retrieving the radeon software version cannot occur after a D3D Device
		// has been created, and this function could be called at any time
		
		const FString Key = FString::Printf(TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\%s"), *RegistryKey);
		
		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("Catalyst_Version"), Out.UserDriverVersion))
		{
			Out.UserDriverVersion = FString(TEXT("Catalyst ")) + Out.UserDriverVersion;
		}

		FString Edition;
		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("RadeonSoftwareEdition"), Edition))
		{
			FString Version;
			if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("RadeonSoftwareVersion"), Version))
			{
				// e.g. TEXT("Crimson 15.12") or TEXT("Catalyst 14.1")
				Out.UserDriverVersion = Edition + TEXT(" ") + Version;
			}
		}
	}

}

static void GetVideoDriverDetails(const FString& Key, FGPUDriverInfo& Out)
{
	// https://msdn.microsoft.com/en-us/library/windows/hardware/ff569240(v=vs.85).aspx

	const TCHAR* DeviceDescriptionValueName = TEXT("Device Description");

	bool bDevice = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, DeviceDescriptionValueName, Out.DeviceDescription); // AMD and NVIDIA
	if (!bDevice)
	{
		bDevice = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("DriverDesc"), Out.DeviceDescription);
	}

	if (!bDevice)
	{
		// in the case where this failed we also have:
		//  "DriverDesc"="NVIDIA GeForce GTX 670"
		
		// e.g. "GeForce GTX 680" (no NVIDIA prefix so no good for string comparison with DX)
		//	FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("HardwareInformation.AdapterString"), Out.DeviceDescription); // AMD and NVIDIA

		// Try again in Settings subfolder
		const FString SettingsSubKey = Key + TEXT("\\Settings");
		bDevice = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *SettingsSubKey, DeviceDescriptionValueName, Out.DeviceDescription); // AMD and NVIDIA

		if (!bDevice)
		{
			// Neither root nor Settings subfolder contained a "Device Description" value so this is probably not a device
			Out = FGPUDriverInfo();
			return;
		}
	}

	// some key/value pairs explained: http://www.helpdoc-online.com/SCDMS01EN1A330P306~Windows-NT-Workstation-3.51-Resource-Kit-Help-en~Video-Device-Driver-Entries.htm

	FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("ProviderName"), Out.ProviderName);

	if (!Out.ProviderName.IsEmpty())
	{
		if (Out.ProviderName.Find(TEXT("NVIDIA")) != INDEX_NONE)
		{
			Out.SetNVIDIA();
		}
		else if (Out.ProviderName.Find(TEXT("Advanced Micro Devices")) != INDEX_NONE)
		{
			Out.SetAMD();
		}
		else if (Out.ProviderName.Find(TEXT("Intel")) != INDEX_NONE)	// usually TEXT("Intel Corporation")
		{
			Out.SetIntel();
		}
	}

	// technical driver version, AMD and NVIDIA
	FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("DriverVersion"), Out.InternalDriverVersion);

	Out.UserDriverVersion = Out.InternalDriverVersion;

	if(Out.IsNVIDIA())
	{
		Out.UserDriverVersion = Out.TrimNVIDIAInternalVersion(Out.InternalDriverVersion);
	}
	else if(Out.IsAMD())
	{
		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("Catalyst_Version"), Out.UserDriverVersion))
		{
			Out.UserDriverVersion = FString(TEXT("Catalyst ")) + Out.UserDriverVersion;
		}

		FString Edition;
		if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("RadeonSoftwareEdition"), Edition))
		{
			FString Version;
			if(FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("RadeonSoftwareVersion"), Version))
			{
				// e.g. TEXT("Crimson 15.12") or TEXT("Catalyst 14.1")
				Out.UserDriverVersion = Edition + TEXT(" ") + Version;
			}
		}
	}

	// AMD and NVIDIA
	FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, *Key, TEXT("DriverDate"), Out.DriverDate);
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR Monitor, HDC MonitorDC, LPRECT Rect, LPARAM UserData)
{
	int* NumMonitors = (int*)UserData;
	*NumMonitors += 1;

	MONITORINFOEX MonitorInfoEx;
	MonitorInfoEx.cbSize = sizeof(MonitorInfoEx);
	GetMonitorInfo(Monitor, &MonitorInfoEx);

	UE_LOG(LogWindows, Log, TEXT("    resolution: %dx%d, work area: (%d, %d) -> (%d, %d), device: '%s'%s"),
		MonitorInfoEx.rcMonitor.right - MonitorInfoEx.rcMonitor.left, MonitorInfoEx.rcMonitor.bottom - MonitorInfoEx.rcMonitor.top,
		MonitorInfoEx.rcWork.left, MonitorInfoEx.rcWork.top, MonitorInfoEx.rcWork.right, MonitorInfoEx.rcWork.bottom,
		MonitorInfoEx.szDevice,
		MonitorInfoEx.dwFlags & MONITORINFOF_PRIMARY ? TEXT(" [PRIMARY]") : TEXT("")
	);

	return TRUE;
}

FGPUDriverInfo FWindowsPlatformMisc::GetGPUDriverInfo(const FString& DeviceDescription)
{
	// Also report monitor information here, for lack of a better place.
	UE_LOG(LogWindows, Log, TEXT("Attached monitors:"));
	int NumMonitors = 0;
	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&NumMonitors);
	UE_LOG(LogWindows, Log, TEXT("Found %d attached monitors."), NumMonitors);

	// to distinguish failed GetGPUDriverInfo() from call to GetGPUDriverInfo()
	FGPUDriverInfo Ret;

	Ret.InternalDriverVersion = TEXT("Unknown");
	Ret.UserDriverVersion = TEXT("Unknown");
	Ret.DriverDate = TEXT("Unknown");

	// for debugging, useful even in shipping to see what went wrong
	FString DebugString;

	uint32 FoundDriverCount = 0;

	int32 Method = CVarDriverDetectionMethod.GetValueOnGameThread();

	if (Method == 5)
	{
		UE_LOG(LogWindows, Log, TEXT("Gathering driver information using Windows Setup API"));
		FGPUDriverInfo Local;
		GetVideoDriverDetailsFromSetup(DeviceDescription, Local);

		if(Local.IsValid() && Local.DeviceDescription == DeviceDescription)
		{
			return Local;
		}

		UE_LOG(LogWindows, Log, TEXT("Failed to get driver data for device '%s' using Setup API. Switching to fallback method."), *DeviceDescription);
		Method = 4; // Switch to method 4 as a fallback if method 5 fails
	}
	
	if(Method == 3 || Method == 4)
	{
		UE_LOG(LogWindows, Log, TEXT("EnumDisplayDevices:"));

		for(uint32 i = 0; i < 256; ++i)
		{
			DISPLAY_DEVICE Device;
			
			ZeroMemory(&Device, sizeof(Device));
			Device.cb = sizeof(Device);
			
			// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd162609(v=vs.85).aspx
			if(EnumDisplayDevices(0, i, &Device, EDD_GET_DEVICE_INTERFACE_NAME) == 0)
			{
				// last device or error
				break;
			}

			UE_LOG(LogWindows, Log, TEXT("   %d. '%s' (P:%d D:%d), name: '%s'"),
				i,
				Device.DeviceString,
				(Device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0,
				(Device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0,
				Device.DeviceName
				);

			if(Method == 3)
			{
				if (!(Device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE))
				{
					// see http://www.vistaheads.com/forums/microsoft-public-windows-vista-hardware-devices/286017-find-out-active-graphics-device-programmatically-registry-key.html
					DebugString += TEXT("JumpOverNonPrimary ");
					// we want the primary device
					continue;
				}
			}

			FString DriverLocation = Device.DeviceKey;

			if(DriverLocation.Left(18) == TEXT("\\Registry\\Machine\\"))		// not case sensitive
			{
				DriverLocation = FString(TEXT("\\HKEY_LOCAL_MACHINE\\")) + DriverLocation.RightChop(18);
			}
			if(DriverLocation.Left(20) == TEXT("\\HKEY_LOCAL_MACHINE\\"))		// not case sensitive
			{
				FString DriverKey = DriverLocation.RightChop(20);
				
				FGPUDriverInfo Local;
				GetVideoDriverDetails(DriverKey, Local);

				if(!Local.IsValid())
				{
					DebugString += TEXT("GetVideoDriverDetailsInvalid ");
				}

				if((Method == 3) || (Local.DeviceDescription == DeviceDescription))
				{
					if(!FoundDriverCount)
					{
						Ret = Local;
					}
					++FoundDriverCount;
				}
				else
				{
					DebugString += TEXT("PrimaryIsNotTheChoosenAdapter ");
				}
			}
			else
			{
				DebugString += TEXT("PrimaryDriverLocationFailed ");
			}
		}
		
		if(FoundDriverCount != 1)
		{
			// We assume if multiple entries are found they are all the same driver. If that is correct - this is no error.
			DebugString += FString::Printf(TEXT("FoundDriverCount:%d "), FoundDriverCount);
		}

		if(!DebugString.IsEmpty())
		{
			UE_LOG(LogWindows, Log, TEXT("DebugString: %s"), *DebugString);
		}

		return Ret;
	}

	const bool bIterateAvailableAndChoose = Method == 0;

	if(bIterateAvailableAndChoose)
	{
		for(uint32 i = 0; i < 256; ++i)
		{
			// Iterate all installed display adapters
			const FString DriverNKey = FString::Printf(TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E968-E325-11CE-BFC1-08002BE10318}\\%04d"), i);
		
			FGPUDriverInfo Local;
			GetVideoDriverDetails(DriverNKey, Local);

			if(!Local.IsValid())
			{
				// last device or error
				DebugString += TEXT("GetVideoDriverDetailsInvalid ");
				break;
			}

			if(Local.DeviceDescription == DeviceDescription)
			{
				// found the one we are searching for
				Ret = Local;
				++FoundDriverCount;
				break;
			}
		}
	}

	// FoundDriverCount can be != 1, we take the primary adapter (can be from upgrading a machine to a new OS or old drivers) which also might be wrong
	// see: http://win7settings.blogspot.com/2014/10/how-to-extract-installed-drivers-from.html
	// https://support.microsoft.com/en-us/kb/200435
	// http://www.experts-exchange.com/questions/10198207/Windows-NT-Display-adapter-information.html
	// alternative: from https://support.microsoft.com/en-us/kb/200435
	if(FoundDriverCount != 1)
	{
		// we start again, this time we only look at the primary adapter
		Ret.InternalDriverVersion = TEXT("Unknown");
		Ret.UserDriverVersion = TEXT("Unknown");
		Ret.DriverDate = TEXT("Unknown");

		if(bIterateAvailableAndChoose)
		{
			DebugString += FString::Printf(TEXT("FoundDriverCount:%d FallbackToPrimary "), FoundDriverCount);
		}
	
		FString DriverLocation; // e.g. HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\<videodriver>\Device0
		// Video0 is the first logical one, not neccesarily the primary, would have to iterate multiple to get the right one (see https://support.microsoft.com/en-us/kb/102992)
		bool bOk = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DEVICEMAP\\VIDEO"), TEXT("\\Device\\Video0"), /*out*/ DriverLocation);

		if(bOk)
		{
			if(DriverLocation.Left(18) == TEXT("\\Registry\\Machine\\"))		// not case sensitive
			{
				DriverLocation = FString(TEXT("\\HKEY_LOCAL_MACHINE\\")) + DriverLocation.RightChop(18);
			}
			if(DriverLocation.Left(20) == TEXT("\\HKEY_LOCAL_MACHINE\\"))		// not case sensitive
			{
				FString DriverLocationKey = DriverLocation.RightChop(20);
				
				FGPUDriverInfo Local;
				GetVideoDriverDetails(DriverLocationKey, Local);

				if(!Local.IsValid())
				{
					DebugString += TEXT("GetVideoDriverDetailsInvalid ");
				}

				if(Local.DeviceDescription == DeviceDescription)
				{
					Ret = Local;
					FoundDriverCount = 1;
				}
				else
				{
					DebugString += TEXT("PrimaryIsNotTheChoosenAdapter ");
				}
			}
			else
			{
				DebugString += TEXT("PrimaryDriverLocationFailed ");
			}
		}
		else
		{
			DebugString += TEXT("QueryForPrimaryFailed ");
		}
	}

	if(!DebugString.IsEmpty())
	{
		UE_LOG(LogWindows, Log, TEXT("DebugString: %s"), *DebugString);
	}

	return Ret;
}

#include "Windows/HideWindowsPlatformTypes.h"

void FWindowsPlatformMisc::GetOSVersions( FString& OutOSVersionLabel, FString& OutOSSubVersionLabel )
{
	static struct FOSVersionsInitializer
	{
		FOSVersionsInitializer()
		{
			OSVersionLabel[0] = TEXT('\0');
			OSSubVersionLabel[0] = TEXT('\0');
			GetOSVersionsHelper( OSVersionLabel, UE_ARRAY_COUNT(OSVersionLabel), OSSubVersionLabel, UE_ARRAY_COUNT(OSSubVersionLabel) );
		}

		TCHAR OSVersionLabel[128];
		TCHAR OSSubVersionLabel[128];
	} OSVersionsInitializer;

	OutOSVersionLabel = OSVersionsInitializer.OSVersionLabel;
	OutOSSubVersionLabel = OSVersionsInitializer.OSSubVersionLabel;
}


FString FWindowsPlatformMisc::GetOSVersion()
{
	static struct FOSVersionInitializer
	{
		FOSVersionInitializer()
		{
			CachedOSVersion[0] = TEXT('\0');
			if (!GetOSVersionHelper(CachedOSVersion, UE_ARRAY_COUNT(CachedOSVersion)))
			{
				CachedOSVersion[0] = TEXT('\0');
			}
		}

		TCHAR CachedOSVersion[128];
	} OSVersionInitializer;
	return OSVersionInitializer.CachedOSVersion;
}

bool FWindowsPlatformMisc::GetDiskTotalAndFreeSpace( const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes )
{
	const FString ValidatedPath = FPaths::ConvertRelativePathToFull(InPath).Replace(TEXT("/"), TEXT("\\"));

	bool bSuccess = !!::GetDiskFreeSpaceEx( *ValidatedPath,
											nullptr,
											reinterpret_cast<ULARGE_INTEGER*>(&TotalNumberOfBytes),
											reinterpret_cast<ULARGE_INTEGER*>(&NumberOfFreeBytes));
	return bSuccess;
}

bool FWindowsPlatformMisc::GetPageFaultStats(FPageFaultStats& OutStats, EPageFaultFlags Flags/*=EPageFaultFlags::All*/)
{
	bool bSuccess = false;

	if (EnumHasAnyFlags(Flags, EPageFaultFlags::TotalPageFaults))
	{
		PROCESS_MEMORY_COUNTERS ProcessMemoryCounters;

		FPlatformMemory::Memzero(&ProcessMemoryCounters, sizeof(ProcessMemoryCounters));
		::GetProcessMemoryInfo(::GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters));

		OutStats.TotalPageFaults = ProcessMemoryCounters.PageFaultCount;

		bSuccess = true;
	}

	return bSuccess;
}

bool FWindowsPlatformMisc::GetBlockingIOStats(FProcessIOStats& OutStats, EInputOutputFlags Flags/*=EInputOutputFlags::All*/)
{
	bool bSuccess = false;
	IO_COUNTERS Counters;

	FPlatformMemory::Memzero(&Counters, sizeof(Counters));

	// Ignore flags as all values are grabbed at once
	if (::GetProcessIoCounters(::GetCurrentProcess(), &Counters) != 0)
	{
		OutStats.BlockingInput = Counters.ReadOperationCount;
		OutStats.BlockingOutput = Counters.WriteOperationCount;
		OutStats.BlockingOther = Counters.OtherOperationCount;
		OutStats.InputBytes = Counters.ReadTransferCount;
		OutStats.OutputBytes = Counters.WriteTransferCount;
		OutStats.OtherBytes = Counters.OtherTransferCount;

		bSuccess = true;
	}

	return bSuccess;
}



uint32 FWindowsPlatformMisc::GetCPUInfo()
{
	return FCPUIDQueriedData::GetCPUInfo();
}

bool FWindowsPlatformMisc::HasNonoptionalCPUFeatures()
{
	// Check for popcnt is bit 23
	return (FCPUIDQueriedData::GetCPUInfo2() & (1 << 23)) != 0;
}

bool FWindowsPlatformMisc::NeedsNonoptionalCPUFeaturesCheck()
{
	// popcnt is 64bit
	return PLATFORM_ENABLE_POPCNT_INTRINSIC;
}

bool FWindowsPlatformMisc::HasTimedPauseCPUFeature()
{
	return false;
}

int32 FWindowsPlatformMisc::GetCacheLineSize()
{
	return FCPUIDQueriedData::GetCacheLineSize();
}

bool FWindowsPlatformMisc::QueryRegKey( const Windows::HKEY InKey, const TCHAR* InSubKey, const TCHAR* InValueName, FString& OutData )
{
	bool bSuccess = false;

	// Redirect key depending on system
	for (int32 RegistryIndex = 0; RegistryIndex < 2 && !bSuccess; ++RegistryIndex)
	{
		HKEY Key = 0;
		const uint32 RegFlags = (RegistryIndex == 0) ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
		if (RegOpenKeyEx( InKey, InSubKey, 0, KEY_READ | RegFlags, &Key ) == ERROR_SUCCESS)
		{
			::DWORD Size = 0, ValueType = 0;
			// First, we'll call RegQueryValueEx to find out how large of a buffer we need
			if ((RegQueryValueEx( Key, InValueName, NULL, &ValueType, NULL, &Size ) == ERROR_SUCCESS) && Size)
			{
				switch (ValueType)
				{
					case REG_DWORD:
					{
						::DWORD Value;
						if (RegQueryValueEx(Key, InValueName, NULL, NULL, (LPBYTE)&Value, &Size) == ERROR_SUCCESS)
						{
							OutData = FString::Printf(TEXT("%d"), Value);
							bSuccess = true;
						}
						break;
					}

					case REG_QWORD:
					{
						int64 Value;
						if (RegQueryValueEx(Key, InValueName, NULL, NULL, (LPBYTE)&Value, &Size) == ERROR_SUCCESS)
						{
							OutData = FString::Printf(TEXT("%lld"), Value);
							bSuccess = true;
						}
						break;
					}

					case REG_SZ:
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
					{
						// Allocate a buffer to hold the value and call the function again to get the data
						char* Buffer = new char[Size];
						if (RegQueryValueEx(Key, InValueName, NULL, NULL, (LPBYTE)Buffer, &Size) == ERROR_SUCCESS)
						{
							const uint32 Length = (Size / sizeof(TCHAR)) - 1;
							OutData = FString(Length, (TCHAR*)Buffer);
							bSuccess = true;
						}
						delete[] Buffer;
						break;
					}
				}
			}
			RegCloseKey( Key );
		}
	}

	return bSuccess;
}

bool FWindowsPlatformMisc::GetVSComnTools(int32 Version, FString& OutData)
{
	checkf(12 <= Version && Version <= 15, L"Not supported Visual Studio version.");

	FString ValueName = FString::Printf(TEXT("%d.0"), Version);

	FString IDEPath;
	if (!QueryRegKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"), *ValueName, IDEPath))
	{
		if (!QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"), *ValueName, IDEPath))
		{
			if (!QueryRegKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7"), *ValueName, IDEPath))
			{
				if (!QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7"), *ValueName, IDEPath))
				{
					return false;
				}
			}
		}
	}

	OutData = FPaths::ConvertRelativePathToFull(FPaths::Combine(*IDEPath, L"Common7", L"Tools"));
	return true;
}

const TCHAR* FWindowsPlatformMisc::GetDefaultPathSeparator()
{
	return TEXT( "\\" );
}

FText FWindowsPlatformMisc::GetFileManagerName()
{
	return NSLOCTEXT("WindowsPlatform", "FileManagerName", "Explorer");
}

bool FWindowsPlatformMisc::IsRunningOnBattery()
{
	SYSTEM_POWER_STATUS status;
	GetSystemPowerStatus(&status);
	switch(status.BatteryFlag)
	{
	case 4://	"Critical-the battery capacity is at less than five percent"
	case 2://	"Low-the battery capacity is at less than 33 percent"
	case 1://	"High-the battery capacity is at more than 66 percent"
	case 8://	"Charging"
		return true;
	case 128://	"No system battery" - desktop, NB: UPS don't count as batteries under Windows
	case 255://	"Unknown status-unable to read the battery flag information"
	default:
		return false;
	}

	return false;
}

FString FWindowsPlatformMisc::GetOperatingSystemId()
{
	FString Result;
	// more info on this key can be found here: http://stackoverflow.com/questions/99880/generating-a-unique-machine-id
	QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Cryptography"), TEXT("MachineGuid"), Result);
	return Result;
}


EConvertibleLaptopMode FWindowsPlatformMisc::GetConvertibleLaptopMode()
{
	if (!VerifyWindowsVersion(6, 2))
	{
		return EConvertibleLaptopMode::NotSupported;
	}

	if (::GetSystemMetrics(SM_CONVERTIBLESLATEMODE) == 0)
	{
		return EConvertibleLaptopMode::Tablet;
	}
	
	return EConvertibleLaptopMode::Laptop;
}

IPlatformChunkInstall* FWindowsPlatformMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	static bool bIniChecked = false;
	if (!ChunkInstall || !bIniChecked)
	{
		IPlatformChunkInstallModule* PlatformChunkInstallModule = nullptr;
		if (!GEngineIni.IsEmpty())
		{
			FString InstallModule;
			GConfig->GetString(TEXT("StreamingInstall"), TEXT("DefaultProviderName"), InstallModule, GEngineIni);
			FModuleStatus Status;
			if (FModuleManager::Get().QueryModule(*InstallModule, Status))
			{
				PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>(*InstallModule);
				if (PlatformChunkInstallModule != nullptr)
				{
					// Attempt to grab the platform installer
					ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
				}
			}
			bIniChecked = true;
		}

		if (PlatformChunkInstallModule == nullptr)
		{
			// Placeholder instance
			ChunkInstall = FGenericPlatformMisc::GetPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

void FWindowsPlatformMisc::PumpMessagesOutsideMainLoop()
{
	TGuardValue<bool> PumpMessageGuard(GPumpingMessagesOutsideOfMainLoop, true);
	// Process pending windows messages, which is necessary to the rendering thread in some cases where D3D
	// sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
	MSG Msg;
	PeekMessage(&Msg, NULL, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
	return;
}

uint64 FWindowsPlatformMisc::GetFileVersion(const FString &FileName)
{
	::DWORD VersionInfoSize = GetFileVersionInfoSize(*FileName, NULL);
	if (VersionInfoSize != 0)
	{
		TArray<uint8> VersionInfo;
		VersionInfo.AddUninitialized(VersionInfoSize);
		if (GetFileVersionInfo(*FileName, NULL, VersionInfoSize, VersionInfo.GetData()))
		{
			VS_FIXEDFILEINFO *FileInfo;
			::UINT FileInfoLen;
			if (VerQueryValue(VersionInfo.GetData(), TEXT("\\"), (LPVOID*)&FileInfo, &FileInfoLen))
			{
				return (uint64(FileInfo->dwFileVersionMS) << 32) | uint64(FileInfo->dwFileVersionLS);
			}
		}
	}
	return 0;
}

int32 FWindowsPlatformMisc::GetMaxRefreshRate()
{
	int32 Result = FGenericPlatformMisc::GetMaxRefreshRate();

#if !UE_SERVER
	DEVMODE DeviceMode;
	FMemory::Memzero(DeviceMode);
	DeviceMode.dmSize = sizeof(DEVMODE);

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DeviceMode) != 0)
	{
		// dmDisplayFrequency isn't always useful, the Windows docs say it can
		// return 0 or 1 to indicate 'default refresh rate', so always assume we
		// can do at least the generic platform default of 60 Hz
		Result = FMath::Max(Result, (int32)DeviceMode.dmDisplayFrequency);
	}
#endif

	return Result;
}
