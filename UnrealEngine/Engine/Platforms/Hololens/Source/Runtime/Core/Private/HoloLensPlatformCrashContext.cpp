// Copyright Epic Games, Inc. All Rights Reserved.


#include "HoloLensPlatformCrashContext.h"

#if WER_CUSTOM_REPORTS

#include "HAL/PlatformMallocCrash.h"
#include "HAL/ExceptionHandling.h"
#include "HoloLensPlatformCrashContext.h"
#include "../../Launch/Resources/Version.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"

	#include <strsafe.h>
	#include <werapi.h>
	#pragma comment( lib, "wer.lib" )
	#include <dbghelp.h>
	#include <Shlwapi.h>

#pragma comment( lib, "version.lib" )
#pragma comment( lib, "Shlwapi.lib" )


namespace
{
static int32 ReportCrashCallCount = 0;

/**
 * Write a Windows minidump to disk
 * @param Path Full path of file to write (normally a .dmp file)
 * @param ExceptionInfo Pointer to structure containing the exception information
 * @return Success or failure
 */

// @TODO yrx 2014-10-08 Move to FHoloLensPlatformCrashContext
bool WriteMinidump(const TCHAR* Path, LPEXCEPTION_POINTERS ExceptionInfo)
{
	// Try to create file for minidump.
	HANDLE FileHandle = CreateFile2(Path, GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
	
	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Initialise structure required by MiniDumpWriteDumps
	MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo = {};

	DumpExceptionInfo.ThreadId			= GetCurrentThreadId();
	DumpExceptionInfo.ExceptionPointers	= ExceptionInfo;
	DumpExceptionInfo.ClientPointers	= FALSE;

	// CrashContext.runtime-xml is now a part of the minidump file.
	FHoloLensPlatformCrashContext CrashContext;
	CrashContext.SerializeContentToBuffer();
	
	MINIDUMP_USER_STREAM CrashContextStream ={0};
	CrashContextStream.Type = FHoloLensPlatformCrashContext::UE4_MINIDUMP_CRASHCONTEXT;
	CrashContextStream.BufferSize = CrashContext.GetBuffer().GetAllocatedSize();
	CrashContextStream.Buffer = (void*)*CrashContext.GetBuffer();

	MINIDUMP_USER_STREAM_INFORMATION CrashContextStreamInformation = {0};
	CrashContextStreamInformation.UserStreamCount = 1;
	CrashContextStreamInformation.UserStreamArray = &CrashContextStream;

	const BOOL Result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), FileHandle, MiniDumpNormal, &DumpExceptionInfo, &CrashContextStreamInformation, NULL);
	CloseHandle(FileHandle);

	return Result == TRUE;
}

/** 
 * Get one line of text to describe the crash
 */
void GetCrashDescription(WER_REPORT_INFORMATION& ReportInformation)
{
	StringCchCopy( ReportInformation.wzDescription, ARRAYSIZE( ReportInformation.wzDescription ), TEXT( "The application crashed while running " ) );

	const TCHAR* Description = IsRunningCommandlet() ? TEXT( "a commandlet" ) :
		GIsEditor ? TEXT( "the editor" ) :
		IsRunningDedicatedServer() ? TEXT( "a server" ) :
		TEXT( "the game" );

	StringCchCat( ReportInformation.wzDescription, ARRAYSIZE( ReportInformation.wzDescription ), Description );
}

//--------------------
// @HEMI : GetFileVersionInfo apis aren't part of the "APP" api set, but if a developer specifically changes
//                   to a non-shippable API set for compilation, we'll still collect and attach the additional module info
//-----
#if (WINAPI_FAMILY==WINAPI_FAMILY_ONECORE_APP)

/**
 * Get the 4 element version number of the module
 */
void GetModuleVersion( TCHAR* ModuleName, TCHAR* StringBuffer, DWORD MaxSize )
{
	StringCchCopy( StringBuffer, MaxSize, TEXT( "0.0.0.0" ) );
	
	DWORD Handle = 0;
	DWORD InfoSize = GetFileVersionInfoSize( ModuleName, &Handle );
	if( InfoSize > 0 )
	{
		TArray<char> VersionInfo;
		VersionInfo.SetNum(InfoSize);

		if( GetFileVersionInfo( ModuleName, 0, InfoSize, VersionInfo.GetData() ) )
		{
			VS_FIXEDFILEINFO* FixedFileInfo;

			UINT InfoLength = 0;
			if( VerQueryValue( VersionInfo.GetData(), TEXT( "\\" ), ( void** )&FixedFileInfo, &InfoLength ) )
			{
				StringCchPrintf( StringBuffer, MaxSize, TEXT( "%u.%u.%u.%u" ), 
					HIWORD( FixedFileInfo->dwProductVersionMS ), LOWORD( FixedFileInfo->dwProductVersionMS ), HIWORD( FixedFileInfo->dwProductVersionLS ), LOWORD( FixedFileInfo->dwProductVersionLS ) );
			}
		}
	}
}

#endif
//-----
// @HEMI : end
//--------------------

/** 
 * Set the ten Windows Error Reporting parameters
 *
 * Parameters 0 through 7 are predefined for Windows
 * Parameters 8 and 9 are user defined
 */
void SetReportParameters( HREPORT ReportHandle, EXCEPTION_POINTERS* ExceptionInfo, const TCHAR* ErrorMessage )
{
	HRESULT Result;
	TCHAR StringBuffer[MAX_SPRINTF] = {0};
	TCHAR LocalBuffer[MAX_SPRINTF] = {0};

	// Set the parameters for the standard problem signature
	HMODULE ModuleHandle = GetModuleHandle( NULL );

	StringCchPrintf( StringBuffer, MAX_SPRINTF, TEXT( "UE4-%s" ), FApp::GetGameName() );
	Result = WerReportSetParameter( ReportHandle, WER_P0, TEXT( "Application Name" ), StringBuffer );

	StringCchPrintf(StringBuffer, MAX_SPRINTF, TEXT("%08x"), GetTimestampForLoadedLibrary(ModuleHandle));
	Result = WerReportSetParameter(ReportHandle, WER_P2, TEXT("Application Timestamp"), StringBuffer);

//--------------------
// @HEMI : GetFileVersionInfo apis aren't part of the "APP" api set, but if a developer specifically changes
//                   to a non-shippable API set for compilation, we'll still collect and attach the additional module info
//-----
#if (WINAPI_FAMILY==WINAPI_FAMILY_ONECORE_APP)
	GetModuleFileName( ModuleHandle, LocalBuffer, MAX_SPRINTF );
	PathStripPath( LocalBuffer );
	GetModuleVersion( LocalBuffer, StringBuffer, MAX_SPRINTF );
	Result = WerReportSetParameter( ReportHandle, WER_P1, TEXT( "Application Version" ), StringBuffer );

	HMODULE FaultModuleHandle = NULL;
	GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ( LPCTSTR )ExceptionInfo->ExceptionRecord->ExceptionAddress, &FaultModuleHandle );

	GetModuleFileName( FaultModuleHandle, LocalBuffer, MAX_SPRINTF );
	PathStripPath( LocalBuffer );
	Result = WerReportSetParameter( ReportHandle, WER_P3, TEXT( "Fault Module Name" ), LocalBuffer );

	GetModuleVersion( LocalBuffer, StringBuffer, MAX_SPRINTF );
	Result = WerReportSetParameter( ReportHandle, WER_P4, TEXT( "Fault Module Version" ), StringBuffer );

	StringCchPrintf(StringBuffer, MAX_SPRINTF, TEXT("%08x"), GetTimestampForLoadedLibrary(FaultModuleHandle));
	Result = WerReportSetParameter(ReportHandle, WER_P5, TEXT("Fault Module Timestamp"), StringBuffer);

	INT_PTR ExceptionOffset = (char*)(ExceptionInfo->ExceptionRecord->ExceptionAddress) - (char*)FaultModuleHandle;
	CA_SUPPRESS(6066) // The format specifier should probably be something like %tX, but VS 2013 doesn't support 't'.
		StringCchPrintf(StringBuffer, MAX_SPRINTF, TEXT("%p"), ExceptionOffset);
	Result = WerReportSetParameter(ReportHandle, WER_P7, TEXT("Exception Offset"), StringBuffer);
#endif
//-----
// @HEMI : end
//--------------------

	StringCchPrintf( StringBuffer, MAX_SPRINTF, TEXT( "%08x" ), ExceptionInfo->ExceptionRecord->ExceptionCode );
	Result = WerReportSetParameter( ReportHandle, WER_P6, TEXT( "Exception Code" ), StringBuffer );

	// Use LocalBuffer to store the error message.
	FCString::Strncpy( LocalBuffer, ErrorMessage, MAX_SPRINTF );

	// Replace " with ' and replace \n with #
	for (TCHAR& Char: LocalBuffer)
	{
		if (Char == 0)
		{
			break;
		}

		switch (Char)
		{
			default: break;
			case '"':	Char = '\'';	break;
			case '\r':	Char = '#';		break;
			case '\n':	Char = '#';		break;
		}
	}

	// AssertLog should be ErrorMessage, but this require crash server changes, so don't change this.
	StringCchPrintf( StringBuffer, MAX_SPRINTF, TEXT( "!%s!AssertLog=\"%s\"" ), FCommandLine::Get(), LocalBuffer );
	Result = WerReportSetParameter( ReportHandle, WER_P8, TEXT( "Commandline" ), StringBuffer );

	StringCchPrintf( StringBuffer, MAX_SPRINTF, TEXT( "%s!%s!%s!%d" ), TEXT( BRANCH_NAME ), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), BUILT_FROM_CHANGELIST );
	Result = WerReportSetParameter( ReportHandle, WER_P9, TEXT( "BranchBaseDir" ), StringBuffer );
}

/**
 * Add a minidump to the Windows Error Report
 *
 * Note this has to be a minidump (and not a microdump) for the dbgeng functions to work correctly
 */
void AddMiniDump(HREPORT ReportHandle, EXCEPTION_POINTERS* ExceptionInfo)
{
	// Add GetMinidumpFile
	const FString MinidumpFileName = FString::Printf(TEXT("%sDump%d.dmp"), *FPaths::GameLogDir(), FDateTime::UtcNow().GetTicks());
	
	if (WriteMinidump(*MinidumpFileName, ExceptionInfo))
	{
		WerReportAddFile(ReportHandle, *MinidumpFileName, WerFileTypeMinidump, WER_FILE_ANONYMOUS_DATA);
	}
}

/** 
 * Add miscellaneous files to the report. Currently the log and the video file
 */
void AddMiscFiles( HREPORT ReportHandle )
{
	FString LogFileName = FPaths::GameLogDir() / FApp::GetGameName() + TEXT( ".log" );
	WerReportAddFile( ReportHandle, *LogFileName, WerFileTypeOther, WER_FILE_ANONYMOUS_DATA ); 

	FString CrashVideoPath = FPaths::GameLogDir() / TEXT( "CrashVideo.avi" );
	WerReportAddFile( ReportHandle, *CrashVideoPath, WerFileTypeOther, WER_FILE_ANONYMOUS_DATA );
}

/**
 * Enum indicating whether to run the crash reporter UI
 */
enum class EErrorReportUI
{
	/** Ask the user for a description */
	ShowDialog,

	/** Silently uploaded the report */
	ReportInUnattendedMode	
};

/** 
 * Create a Windows Error Report, add the user log and video, and add it to the WER queue
 */
int32 ReportCrashUsingWindowsErrorReporting(EXCEPTION_POINTERS* ExceptionInfo, const TCHAR* ErrorMessage, EErrorReportUI ReportUI)
{
	// Flush out the log
	GLog->Panic();

	// Construct the report details
	WER_REPORT_INFORMATION ReportInformation = {sizeof( WER_REPORT_INFORMATION )};

	StringCchCopy( ReportInformation.wzConsentKey, ARRAYSIZE( ReportInformation.wzConsentKey ), TEXT( "" ) );

	StringCchCopy( ReportInformation.wzApplicationName, ARRAYSIZE( ReportInformation.wzApplicationName ), TEXT("UE4-") );
	StringCchCat( ReportInformation.wzApplicationName, ARRAYSIZE( ReportInformation.wzApplicationName ), FApp::GetGameName() );

	StringCchCopy( ReportInformation.wzApplicationPath, ARRAYSIZE( ReportInformation.wzApplicationPath ), FPlatformProcess::BaseDir() );
	StringCchCat( ReportInformation.wzApplicationPath, ARRAYSIZE( ReportInformation.wzApplicationPath ), FPlatformProcess::ExecutableName() );
	StringCchCat( ReportInformation.wzApplicationPath, ARRAYSIZE( ReportInformation.wzApplicationPath ), TEXT( ".exe" ) );

	GetCrashDescription( ReportInformation );

	// Create a crash event report
	HREPORT ReportHandle = NULL;
	if( WerReportCreate( APPCRASH_EVENT, WerReportApplicationCrash, &ReportInformation, &ReportHandle ) == S_OK )
	{
		// Set the standard set of a crash parameters
		SetReportParameters( ReportHandle, ExceptionInfo, ErrorMessage );

		// Add a manually generated minidump
		AddMiniDump( ReportHandle, ExceptionInfo );

		// Add the log and video
		AddMiscFiles( ReportHandle );

		// Submit
		WER_SUBMIT_RESULT SubmitResult;
		WerReportSubmit( ReportHandle, WerConsentAlwaysPrompt, WER_SUBMIT_QUEUE | WER_SUBMIT_BYPASS_DATA_THROTTLING, &SubmitResult );

		// Cleanup
		WerReportCloseHandle( ReportHandle );
	}

	// Let the system take back over (return value only used by NewReportEnsure)
	return EXCEPTION_CONTINUE_EXECUTION;
}


} // end anonymous namespace
#include "Microsoft/HideMicrosoftPlatformTypes.h"

#endif

#include "Microsoft/AllowMicrosoftPlatformTypes.h"

/** 
 * Report an ensure to the crash reporting system
 */
void NewReportEnsure( const TCHAR* ErrorMessage )
{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && !PLATFORM_HOLOLENS
	__try
#endif
	{
		FPlatformMisc::RaiseException( 1 );
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && !PLATFORM_HOLOLENS
	__except(ReportCrashUsingWindowsErrorReporting(GetExceptionInformation(), ErrorMessage, EErrorReportUI::ReportInUnattendedMode))
	CA_SUPPRESS(6322)
	{
	}
#endif
}


#include "Microsoft/HideMicrosoftPlatformTypes.h"
#if WER_CUSTOM_REPORTS

// Original code below

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	#include <ErrorRep.h>
	#include <DbgHelp.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"

#pragma comment(lib, "Faultrep.lib")

/** 
 * Creates an info string describing the given exception record.
 * See MSDN docs on EXCEPTION_RECORD.
 */
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
void CreateExceptionInfoString(EXCEPTION_RECORD* ExceptionRecord)
{
	// @TODO yrx 2014-08-18 Fix FString usage?
	FString ErrorString = TEXT("Unhandled Exception: ");

#define HANDLE_CASE(x) case x: ErrorString += TEXT(#x); break;

	switch (ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		ErrorString += TEXT("EXCEPTION_ACCESS_VIOLATION ");
		if (ExceptionRecord->ExceptionInformation[0] == 0)
		{
			ErrorString += TEXT("reading address ");
		}
		else if (ExceptionRecord->ExceptionInformation[0] == 1)
		{
			ErrorString += TEXT("writing address ");
		}
		ErrorString += FString::Printf(TEXT("0x%016llx"), ExceptionRecord->ExceptionInformation[1]);
		break;
	HANDLE_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
	HANDLE_CASE(EXCEPTION_DATATYPE_MISALIGNMENT)
	HANDLE_CASE(EXCEPTION_FLT_DENORMAL_OPERAND)
	HANDLE_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO)
	HANDLE_CASE(EXCEPTION_FLT_INVALID_OPERATION)
	HANDLE_CASE(EXCEPTION_ILLEGAL_INSTRUCTION)
	HANDLE_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO)
	HANDLE_CASE(EXCEPTION_PRIV_INSTRUCTION)
	HANDLE_CASE(EXCEPTION_STACK_OVERFLOW)
	default:
		ErrorString += FString::Printf(TEXT("0x%08x"), (uint32)ExceptionRecord->ExceptionCode);
	}

#if WITH_EDITORONLY_DATA
	FCString::Strncpy(GErrorExceptionDescription, *ErrorString, UE_ARRAY_COUNT(GErrorExceptionDescription));
#endif
#undef HANDLE_CASE
}
#include "Microsoft/HideMicrosoftPlatformTypes.h"

int32 ReportCrash( LPEXCEPTION_POINTERS ExceptionInfo )
{
	// Switch to malloc crash.
	FMallocCrash::Get().SetAsGMalloc();

	// Only create a minidump the first time this function is called.
	// (Can be called the first time from the RenderThread, then a second time from the MainThread.)
	if( FPlatformAtomics::InterlockedIncrement( &ReportCrashCallCount ) != 1 )
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

	FCoreDelegates::OnHandleSystemError.Broadcast();

	// Note: ReportCrashUsingWindowsErrorReporting reads the callstack from GErrorHist
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*) GMalloc->Malloc( StackTraceSize );
	StackTrace[0] = 0;
	// Walk the stack and dump it to the allocated memory.
	FPlatformStackWalk::StackWalkAndDump( StackTrace, StackTraceSize, 0, ExceptionInfo->ContextRecord );

	// @TODO yrx 2014-08-20 Make this constant.
	if( ExceptionInfo->ExceptionRecord->ExceptionCode != 1 )
	{
		CreateExceptionInfoString( ExceptionInfo->ExceptionRecord );
		FCString::Strncat( GErrorHist, GErrorExceptionDescription, UE_ARRAY_COUNT( GErrorHist ) );
		FCString::Strncat( GErrorHist, TEXT( "\r\n\r\n" ), UE_ARRAY_COUNT( GErrorHist ) );
	}

	FCString::Strncat( GErrorHist, ANSI_TO_TCHAR(StackTrace), UE_ARRAY_COUNT(GErrorHist) );

	GMalloc->Free( StackTrace );

	ReportCrashUsingWindowsErrorReporting(ExceptionInfo, GErrorMessage, EErrorReportUI::ShowDialog);
	WriteMinidump(MiniDumpFilenameW, ExceptionInfo);

#if UE_BUILD_SHIPPING && WITH_EDITOR
	uint32 dwOpt = 0;
	EFaultRepRetVal repret = ReportFault( ExceptionInfo, dwOpt);
#endif

	return EXCEPTION_EXECUTE_HANDLER;
}

#else
int32 ReportCrash(LPEXCEPTION_POINTERS ExceptionInfo)
{
	throw *ExceptionInfo;
}
#endif

#if WER_CUSTOM_REPORTS
void ReportHang(const TCHAR* ErrorMessage, const uint64* StackFrames, int32 NumStackFrames, uint32 HungThreadId)
{
	if (ReportCrashCallCount > 0 || FDebug::HasAsserted())
	{
		// Don't report ensures after we've crashed/asserted, they simply may be a result of the crash as
		// the engine is already in a bad state.
		return;
	}

	FHoloLensPlatformCrashContext CrashContext(ECrashContextType::Ensure, ErrorMessage);
	CrashContext.SetPortableCallStack(StackFrames, NumStackFrames);
	CrashContext.SetCrashedThreadId(HungThreadId);
	CrashContext.CaptureAllThreadContexts();

	EErrorReportUI ReportUI = IsInteractiveEnsureMode() ? EErrorReportUI::ShowDialog : EErrorReportUI::ReportInUnattendedMode;
	ReportCrashUsingCrashReportClient(CrashContext, nullptr, ReportUI);
}
#else
void ReportHang(const TCHAR* ErrorMessage, const uint64* StackFrames, int32 NumStackFrames, uint32 HungThreadId)
{

}
#endif

/** Implement platform specific static cleanup function */
void FGenericCrashContext::CleanupPlatformSpecificFiles()
{
#if WER_CUSTOM_REPORTS
	FString CrashVideoPath = FPaths::ProjectLogDir() / TEXT("CrashVideo.avi");
	IFileManager::Get().Delete(*CrashVideoPath);
#endif
}

#if WER_CUSTOM_REPORTS

static FCriticalSection EnsureLock;
static bool bReentranceGuard = false;

void ReportEnsure(const TCHAR* ErrorMessage, int NumStackFramesToIgnore)
{
	if (ReportCrashCallCount > 0 || FDebug::HasAsserted())
	{
		// Don't report ensures after we've crashed/asserted, they simply may be a result of the crash as
		// the engine is already in a bad state.
		return;
	}

	// Simple re-entrance guard.
	EnsureLock.Lock();

	if (bReentranceGuard)
	{
		EnsureLock.Unlock();
		return;
	}

	// Stop checking heartbeat for this thread (and stop the gamethread hitch detector if we're the game thread).
	// Ensure can take a lot of time (when stackwalking), so we don't want hitches/hangs firing.
	// These are no-ops on threads that didn't already have a heartbeat etc.
	FSlowHeartBeatScope SuspendHeartBeat(true);
	FDisableHitchDetectorScope SuspendGameThreadHitch;

	bReentranceGuard = true;

	NewReportEnsure(ErrorMessage);

	bReentranceGuard = false;
	EnsureLock.Unlock();
}
#else
void ReportEnsure(const TCHAR* ErrorMessage, int NumStackFramesToIgnore)
{

}
#endif
