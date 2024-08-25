// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixPlatformProcess.h"
#include "Unix/UnixPlatformCrashContext.h"
#include "Unix/UnixPlatformRealTimeSignals.h"
#include "Unix/UnixForkPageProtector.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Unix/UnixPlatformRunnableThread.h"
#include "Misc/EngineVersion.h"
#include <spawn.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <asm/ioctls.h>
#include <sys/prctl.h>
#include "Unix/UnixPlatformOutputDevices.h"
#include "Unix/UnixPlatformTLS.h"
#include "Containers/CircularQueue.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Misc/Fork.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"

namespace PlatformProcessLimits
{
	enum
	{
		MaxUserHomeDirLength = UNIX_MAX_PATH + 1
	};
};

#if IS_MONOLITHIC
__thread uint32 FUnixTLS::ThreadIdTLS = 0;
#else
uint32 FUnixTLS::ThreadIdTLSKey = FUnixTLS::AllocTlsSlot();
#endif

#if UE_CHECK_LARGE_ALLOCATIONS
static TAutoConsoleVariable<int32> CVarEnableLargeAllocationChecksAfterFork(
	TEXT("memory.EnableLargeAllocationChecksAfterFork"),
	false,
	TEXT("After forking, Turn on ensure which checks no single allocation is greater than 'LargeAllocationThreshold'"),
	ECVF_Default);
#endif

void* FUnixPlatformProcess::GetDllHandle( const TCHAR* Filename )
{
	check( Filename );
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(Filename);

	// first of all open the lib in LOCAL mode (we will eventually move to GLOBAL if required)
	int DlOpenMode = RTLD_LAZY;
	void *Handle = dlopen( TCHAR_TO_UTF8(*AbsolutePath), DlOpenMode | RTLD_LOCAL );
	if (Handle)
	{
		bool UpgradeToGlobal = false;
		// check for the "ue4_module_options" symbol
		const char **ue4_module_options = (const char **)dlsym(Handle, "ue4_module_options");
		if (ue4_module_options)
		{
			// split by ','
			TArray<FString> Options;
			FString UE4ModuleOptions = FString(ANSI_TO_TCHAR(*ue4_module_options));
			int32 OptionsNum = UE4ModuleOptions.ParseIntoArray(Options, ANSI_TO_TCHAR(","), true);
			for(FString Option : Options)
			{
				if (Option.Equals(FString(ANSI_TO_TCHAR("linux_global_symbols")), ESearchCase::IgnoreCase))
				{
					UpgradeToGlobal = true;
				}
			}
		}
		else
		{
			// is it ia ue4 module ? if not, move it to GLOBAL
			void *IsUE4Module = dlsym(Handle, "InitializeModule");
			if (!IsUE4Module)
			{
				UpgradeToGlobal = true;
			}
		}

		if (UpgradeToGlobal)
		{
			Handle = dlopen( TCHAR_TO_UTF8(*AbsolutePath), DlOpenMode | RTLD_NOLOAD | RTLD_GLOBAL );
		}
	} 
	else if (!FString(Filename).Contains(TEXT("/")))
	{
		// if not found and the filename did not contain a path we search for it in the global path
		Handle = dlopen( TCHAR_TO_UTF8(Filename), DlOpenMode | RTLD_GLOBAL );
	}

	if (!Handle)
	{
		UE_LOG(LogCore, Warning, TEXT("dlopen failed: %s"), UTF8_TO_TCHAR(dlerror()) );
	}

	return Handle;
}

void FUnixPlatformProcess::FreeDllHandle( void* DllHandle )
{
	check( DllHandle );
	dlclose( DllHandle );
}

void* FUnixPlatformProcess::GetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	check(DllHandle);
	check(ProcName);
	return dlsym( DllHandle, TCHAR_TO_ANSI(ProcName) );
}

const TCHAR* FUnixPlatformProcess::GetModulePrefix()
{
	return TEXT("lib");
}

const TCHAR* FUnixPlatformProcess::GetModuleExtension()
{
	return TEXT("so");
}

namespace PlatformProcessLimits
{
	enum
	{
		MaxComputerName	= 128,
		MaxBaseDirLength= UNIX_MAX_PATH + 1,
		MaxArgvParameters = 256,
		MaxUserName = LOGIN_NAME_MAX
	};
};

const TCHAR* FUnixPlatformProcess::ComputerName()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxComputerName ];
	if (!bHaveResult)
	{
		struct utsname name;
		const char * SysName = name.nodename;
		if(uname(&name))
		{
			SysName = "Unix Computer";
		}

		FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, UTF8_TO_TCHAR(SysName));
		CachedResult[UE_ARRAY_COUNT(CachedResult) - 1] = 0;
		bHaveResult = true;
	}
	return CachedResult;
}

const TCHAR* FUnixPlatformProcess::UserName(bool bOnlyAlphaNumeric)
{
	static TCHAR Name[PlatformProcessLimits::MaxUserName] = { 0 };
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
		struct passwd * UserInfo = getpwuid(geteuid());
		if (nullptr != UserInfo && nullptr != UserInfo->pw_name)
		{
			FString TempName(UTF8_TO_TCHAR(UserInfo->pw_name));
			if (bOnlyAlphaNumeric)
			{
				const TCHAR *Src = *TempName;
				TCHAR * Dst = Name;
				for (; *Src != 0 && (Dst - Name) < UE_ARRAY_COUNT(Name) - 1; ++Src)
				{
					if (FChar::IsAlnum(*Src))
					{
						*Dst++ = *Src;
					}
				}
				*Dst++ = 0;
			}
			else
			{
				FCString::Strncpy(Name, *TempName, UE_ARRAY_COUNT(Name) - 1);
			}
		}
		else
		{
			FCString::Sprintf(Name, TEXT("euid%d"), geteuid());
		}
		bHaveResult = true;
	}

	return Name;
}

const TCHAR* FUnixPlatformProcess::UserTempDir()
{
	// Use $TMPDIR if its set otherwise fallback to /var/tmp as Windows defaults to %TEMP% which does not get cleared on reboot.
	static bool bHaveTemp = false;
	static TCHAR CachedResult[PlatformProcessLimits::MaxUserHomeDirLength] = { 0 };

	if (!bHaveTemp)
	{
		const char* TmpDirValue = secure_getenv("TMPDIR");
		if (TmpDirValue)
		{
			FCString::Strcpy(CachedResult, UTF8_TO_TCHAR(TmpDirValue));
		}
		else
		{
			FCString::Strcpy(CachedResult, TEXT("/var/tmp"));
		}

		bHaveTemp = true;
	}

	return CachedResult;
}

const TCHAR* FUnixPlatformProcess::UserDir()
{
	// The UserDir is where user visible files (such as game projects) live.
	// On Unix (just like on Mac) this corresponds to $HOME/Documents.
	// To accomodate localization requirement we use xdg-user-dir command,
	// and fall back to $HOME/Documents if setting not found.
	static TCHAR Result[UNIX_MAX_PATH] = {0};

	if (!Result[0])
	{
		FILE* FilePtr = popen("xdg-user-dir DOCUMENTS", "r");
		if (FilePtr)
		{
			char DocPath[UNIX_MAX_PATH];
			if (fgets(DocPath, UNIX_MAX_PATH, FilePtr) != nullptr)
			{
				size_t DocLen = strlen(DocPath) - 1;
				if (DocLen > 0)
				{
					DocPath[DocLen] = '\0';
					FCString::Strncpy(Result, UTF8_TO_TCHAR(DocPath), UE_ARRAY_COUNT(Result));
					FCString::Strncat(Result, TEXT("/"), UE_ARRAY_COUNT(Result));
				}
			}
			pclose(FilePtr);
		}

		// if xdg-user-dir did not work, use $HOME
		if (!Result[0])
		{
			FCString::Strncpy(Result, FPlatformProcess::UserHomeDir(), UE_ARRAY_COUNT(Result));
			FCString::Strncat(Result, TEXT("/Documents/"), UE_ARRAY_COUNT(Result));
		}
	}
	return Result;
}

const TCHAR* FUnixPlatformProcess::UserHomeDir()
{
	static bool bHaveHome = false;
	static TCHAR CachedResult[PlatformProcessLimits::MaxUserHomeDirLength] = { 0 };

	if (!bHaveHome)
	{
		bHaveHome = true;
		//  get user $HOME var first
		const char * VarValue = secure_getenv("HOME");
		if (VarValue && VarValue[0] != '\0')
		{
			FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, UTF8_TO_TCHAR(VarValue));
		}
		else
		{
			struct passwd * UserInfo = getpwuid(geteuid());
			if (NULL != UserInfo && NULL != UserInfo->pw_dir && UserInfo->pw_dir[0] != '\0')
			{
				FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, UTF8_TO_TCHAR(UserInfo->pw_dir));
			}
			else
			{
				FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, FUnixPlatformProcess::UserTempDir());
				UE_LOG(LogInit, Warning, TEXT("Could get determine user home directory.  Using temporary directory: %s"), CachedResult);
			}
		}
	}

	return CachedResult;
}

const TCHAR* FUnixPlatformProcess::UserSettingsDir()
{
	// Like on Mac we use the same folder for UserSettingsDir and ApplicationSettingsDir
	// $HOME/.config/Epic/
	return ApplicationSettingsDir();
}

const TCHAR* FUnixPlatformProcess::ApplicationSettingsDir()
{
	// The ApplicationSettingsDir is where the engine stores settings and configuration
	// data.  On linux this corresponds to $HOME/.config/Epic
	static TCHAR Result[UNIX_MAX_PATH] = TEXT("");
	if (!Result[0])
	{
		FCString::Strncpy(Result, FPlatformProcess::UserHomeDir(), UE_ARRAY_COUNT(Result));
		FCString::Strncat(Result, TEXT("/.config/Epic/"), UE_ARRAY_COUNT(Result));
	}
	return Result;
}

FString FUnixPlatformProcess::GetApplicationSettingsDir(const ApplicationSettingsContext& Settings)
{
	// The ApplicationSettingsDir is where the engine stores settings and configuration
	// data.  On linux this corresponds to $HOME/.config/Epic
	TCHAR Result[UNIX_MAX_PATH] = TEXT("");
	FCString::Strncpy(Result, FPlatformProcess::UserHomeDir(), UE_ARRAY_COUNT(Result));
	if (Settings.bIsEpic)
	{
		FCString::Strncat(Result, TEXT("/.config/Epic/"), UE_ARRAY_COUNT(Result));
	}
	else
	{
		FCString::Strncat(Result, TEXT("/.config/"), UE_ARRAY_COUNT(Result));
	}
	return FString(Result);
}

bool FUnixPlatformProcess::SetProcessLimits(EProcessResource::Type Resource, uint64 Limit)
{
	rlimit NativeLimit;

	static_assert(sizeof(long) == sizeof(NativeLimit.rlim_cur), "Platform has atypical rlimit type.");

	// 32-bit platforms set limits as long
	if (sizeof(NativeLimit.rlim_cur) < sizeof(Limit))
	{
		long Limit32 = static_cast<long>(FMath::Min(Limit, static_cast<uint64>(INT_MAX)));
		NativeLimit.rlim_cur = Limit32;
		NativeLimit.rlim_max = Limit32;
	}
	else
	{
		NativeLimit.rlim_cur = Limit;
		NativeLimit.rlim_max = Limit;
	}

	int NativeResource = RLIMIT_AS;

	switch(Resource)
	{
		case EProcessResource::VirtualMemory:
			NativeResource = RLIMIT_AS;
			break;

		default:
			UE_LOG(LogHAL, Warning, TEXT("Unknown resource type %d"), Resource);
			return false;
	}

	if (setrlimit(NativeResource, &NativeLimit) != 0)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("setrlimit(%d, limit_cur=%d, limit_max=%d) failed with error %d (%s)\n"), NativeResource, NativeLimit.rlim_cur, NativeLimit.rlim_max, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
		return false;
	}

	return true;
}

const FString FUnixPlatformProcess::GetModulesDirectory()
{
	static FString CachedModulePath;
	if (CachedModulePath.IsEmpty())
	{
		CachedModulePath = FPaths::GetPath(FString(ExecutablePath()));
	}

	return CachedModulePath;
}

const TCHAR* FUnixPlatformProcess::ExecutablePath()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxBaseDirLength ];
	if (!bHaveResult)
	{
		char SelfPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
		if (readlink( "/proc/self/exe", SelfPath, UE_ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			return CachedResult;
		}
		SelfPath[UE_ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, UTF8_TO_TCHAR(SelfPath));
		CachedResult[UE_ARRAY_COUNT(CachedResult) - 1] = 0;
		bHaveResult = true;
	}
	return CachedResult;
}

const TCHAR* FUnixPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxBaseDirLength ];
	if (!bHaveResult)
	{
		char SelfPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
		if (readlink( "/proc/self/exe", SelfPath, UE_ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			return CachedResult;
		}
		SelfPath[UE_ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strcpy(CachedResult, UE_ARRAY_COUNT(CachedResult) - 1, UTF8_TO_TCHAR(basename(SelfPath)));
		CachedResult[UE_ARRAY_COUNT(CachedResult) - 1] = 0;
		bHaveResult = true;
	}
	return CachedResult;
}


FString FUnixPlatformProcess::GenerateApplicationPath( const FString& AppName, EBuildConfiguration BuildConfiguration)
{
	FString PlatformName = FPlatformProcess::GetBinariesSubdirectory();
	FString ExecutablePath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/%s/%s"), *PlatformName, *AppName);
	
	if (BuildConfiguration != EBuildConfiguration::Development)
	{
		ExecutablePath += FString::Printf(TEXT("-%s-%s"), *PlatformName, LexToString(BuildConfiguration));
	}
	return ExecutablePath;
}


FString FUnixPlatformProcess::GetApplicationName( uint32 ProcessId )
{
	FString Output = TEXT("");

	const int32 ReadLinkSize = 1024;	
	char ReadLinkCmd[ReadLinkSize] = {0};
	FCStringAnsi::Sprintf(ReadLinkCmd, "/proc/%d/exe", ProcessId);
	
	char ProcessPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
	int32 Ret = readlink(ReadLinkCmd, ProcessPath, UE_ARRAY_COUNT(ProcessPath) - 1);
	if (Ret != -1)
	{
		Output = UTF8_TO_TCHAR(ProcessPath);
	}
	return Output;
}

FPipeHandle::~FPipeHandle()
{
	close(PipeDesc);
}

FString FPipeHandle::Read()
{
	const int kBufferSize = 4096;
	ANSICHAR Buffer[kBufferSize];
	FString Output;

	int BytesAvailable = 0;
	if (ioctl(PipeDesc, FIONREAD, &BytesAvailable) == 0)
	{
		if (BytesAvailable > 0)
		{
			int BytesRead = read(PipeDesc, Buffer, kBufferSize - 1);
			if (BytesRead > 0)
			{
				Buffer[BytesRead] = 0;
				Output += StringCast< TCHAR >(Buffer).Get();
			}
		}
	}
	else
	{
		UE_LOG(LogHAL, Fatal, TEXT("ioctl(..., FIONREAD, ...) failed with errno=%d (%s)"), errno, StringCast< TCHAR >(strerror(errno)).Get());
	}

	return Output;
}

bool FPipeHandle::ReadToArray(TArray<uint8> & Output)
{
	int BytesAvailable = 0;
	if (ioctl(PipeDesc, FIONREAD, &BytesAvailable) == 0)
	{
		if (BytesAvailable > 0)
		{
			Output.SetNumUninitialized(BytesAvailable);
			int BytesRead = read(PipeDesc, Output.GetData(), BytesAvailable);
			if (BytesRead > 0)
			{
				if (BytesRead < BytesAvailable)
				{
					Output.SetNum(BytesRead);
				}

				return true;
			}
			else
			{
				Output.Empty();
			}
		}
	}

	return false;
}


void FUnixPlatformProcess::ClosePipe( void* ReadPipe, void* WritePipe )
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(ReadPipe);
		delete PipeHandle;
	}

	if (WritePipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(WritePipe);
		delete PipeHandle;
	}
}

bool FUnixPlatformProcess::CreatePipe(void*& ReadPipe, void*& WritePipe, bool bWritePipeLocal)
{
	int PipeFd[2];
	if (-1 == pipe(PipeFd))
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("pipe() failed with errno = %d (%s)"), ErrNo, 
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	ReadPipe = new FPipeHandle(PipeFd[ 0 ], PipeFd[ 1 ]);
	WritePipe = new FPipeHandle(PipeFd[ 1 ], PipeFd[ 0 ]);

	return true;
}

FString FUnixPlatformProcess::ReadPipe( void* ReadPipe )
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(ReadPipe);
		return PipeHandle->Read();
	}

	return FString();
}

bool FUnixPlatformProcess::ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output)
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast<FPipeHandle*>(ReadPipe);
		return PipeHandle->ReadToArray(Output);
	}

	return false;
}

bool FUnixPlatformProcess::WritePipe(void* WritePipe, const FString& Message, FString* OutWritten)
{
	// if there is not a message or WritePipe is null
	int32 MessageLen = Message.Len();
	if ((MessageLen == 0) || (WritePipe == nullptr))
	{
		return false;
	}

	// Convert input to UTF8CHAR
	const TCHAR* MessagePtr = *Message;
	int32 BytesAvailable = FPlatformString::ConvertedLength<UTF8CHAR>(MessagePtr, MessageLen);
	UTF8CHAR* Buffer = new UTF8CHAR[BytesAvailable + 2];
	*FPlatformString::Convert(Buffer, BytesAvailable, MessagePtr, MessageLen) = (UTF8CHAR)'\n';

	// write to pipe
	uint32 BytesWritten = write(*(int*)WritePipe, Buffer, BytesAvailable + 1);

	// Get written message
	if (OutWritten)
	{
		*OutWritten = StringCast<TCHAR>(Buffer, BytesWritten).Get();
	}

	delete[] Buffer;
	return (BytesWritten == BytesAvailable);
}

bool FUnixPlatformProcess::WritePipe(void* WritePipe, const uint8* Data, const int32 DataLength, int32* OutDataLength)
{
	// if there is not a message or WritePipe is null
	if ((DataLength == 0) || (WritePipe == nullptr))
	{
		return false;
	}

	// write to pipe
	uint32 BytesWritten = write(*(int*)WritePipe, Data, DataLength);

	// Get written Data Length
	if (OutDataLength)
	{
		*OutDataLength = (int32)BytesWritten;
	}

	return (BytesWritten == DataLength);
}

FRunnableThread* FUnixPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadUnix();
}

bool FUnixPlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return URL != nullptr;
}

void FUnixPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (FCoreDelegates::ShouldLaunchUrl.IsBound() && !FCoreDelegates::ShouldLaunchUrl.Execute(URL))
	{
		if (Error)
		{
			*Error = TEXT("LaunchURL cancelled by delegate");
		}
		return;
	}

	// @todo This ignores params and error; mostly a stub
	pid_t pid = fork();
	UE_LOG(LogHAL, Verbose, TEXT("FUnixPlatformProcess::LaunchURL: '%s'"), URL);
	if (pid == 0)
	{
		exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_UTF8(URL), (char *)0));
	}
}

/**
 * This class exists as an imperfect workaround to allow both "fire and forget" children and children about whose return code we actually care.
 * (maybe we could fork and daemonize ourselves for the first case instead?)
 */
struct FChildWaiterThread : public FRunnable
{
	/** Global table of all waiter threads */
	static TArray<FChildWaiterThread *>		ChildWaiterThreadsArray;

	/** Lock guarding the acess to child waiter threads */
	static FCriticalSection					ChildWaiterThreadsArrayGuard;

	/** Pid of child to wait for */
	int ChildPid;

	FChildWaiterThread(pid_t InChildPid)
		:	ChildPid(InChildPid)
	{
		// add ourselves to thread array
		ChildWaiterThreadsArrayGuard.Lock();
		ChildWaiterThreadsArray.Add(this);
		ChildWaiterThreadsArrayGuard.Unlock();
	}

	virtual ~FChildWaiterThread()
	{
		// remove
		ChildWaiterThreadsArrayGuard.Lock();
		ChildWaiterThreadsArray.RemoveSingle(this);
		ChildWaiterThreadsArrayGuard.Unlock();
	}

	virtual uint32 Run()
	{
		for(;;)	// infinite loop in case we get EINTR and have to repeat
		{
			siginfo_t SignalInfo;
			if (waitid(P_PID, ChildPid, &SignalInfo, WEXITED))
			{
				if (errno != EINTR)
				{
					int ErrNo = errno;
					UE_LOG(LogHAL, Fatal, TEXT("FChildWaiterThread::Run(): waitid for pid %d failed (errno=%d, %s)"), 
							 static_cast< int32 >(ChildPid), ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
					break;	// exit the loop if for some reason Fatal log (above) returns
				}
			}
			else
			{
				check(SignalInfo.si_pid == ChildPid);
				break;
			}
		}

		return 0;
	}

	virtual void Exit()
	{
		// unregister from the array
		delete this;
	}
};

/** See FChildWaiterThread */
TArray<FChildWaiterThread *> FChildWaiterThread::ChildWaiterThreadsArray;
/** See FChildWaiterThread */
FCriticalSection FChildWaiterThread::ChildWaiterThreadsArrayGuard;

namespace UnixPlatformProcess
{
	/**
	 * This function tries to set exec permissions on the file (if it is missing them).
	 * It exists because files copied manually from foreign filesystems (e.g. CrashReportClient) or unzipped from
	 * certain arhcive types may lack +x, yet we still want to execute them.
	 *
	 * @param AbsoluteFilename absolute filename to the file in question
	 *
	 * @return true if we should attempt to execute the file, false if it is not worth even trying
	 */	
	bool AttemptToMakeExecIfNotAlready(const FString & AbsoluteFilename)
	{
		bool bWorthTryingToExecute = true;	// be conservative and let the OS decide in most cases

		FTCHARToUTF8 AbsoluteFilenameUTF8Buffer(*AbsoluteFilename);
		const char* AbsoluteFilenameUTF8 = AbsoluteFilenameUTF8Buffer.Get();

		struct stat FilePerms;
		if (UNLIKELY(stat(AbsoluteFilenameUTF8, &FilePerms) == -1))
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("UnixPlatformProcess::AttemptToMakeExecIfNotAlready: could not stat '%s', errno=%d (%s)"),
				*AbsoluteFilename,
				ErrNo,
				UTF8_TO_TCHAR(strerror(ErrNo))
				);
		}
		else
		{
			// Try to make a guess if we can execute the file. We are not trying to do the exact check,
			// so if any of executable bits are set, assume it's executable
			if ((FilePerms.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
			{
				// if no executable bits at all, try setting permissions
				if (chmod(AbsoluteFilenameUTF8, FilePerms.st_mode | S_IXUSR) == -1)
				{
					int ErrNo = errno;
					UE_LOG(LogHAL, Warning, TEXT("UnixPlatformProcess::AttemptToMakeExecIfNotAlready: could not chmod +x '%s', errno=%d (%s)"),
						*AbsoluteFilename,
						ErrNo,
						UTF8_TO_TCHAR(strerror(ErrNo))
						);

					// at this point, assume that execution will fail
					bWorthTryingToExecute = false;
				}
			}
		}

		return bWorthTryingToExecute;
	}
}

FProcHandle FUnixPlatformProcess::CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void* PipeReadChild)
{
	// CreateProc used to only have a single "write" pipe argument, which Windows and Mac would pipe both stdout and stderr into.
	// On Unix though, only stdout was piped to it, and stderr wasn't available at all, so we'll preserve that behaviour in this overload for compatibility with existing code
	return CreateProc(URL, Parms, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, OutProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild, PipeWriteChild);
}

static char* MallocedUtf8FromString(const FString& Str)
{
	FTCHARToUTF8 AnsiBuffer(*Str);
	const char* Ansi = AnsiBuffer.Get();
	size_t AnsiSize = FCStringAnsi::Strlen(Ansi) + 1;	// will work correctly with UTF-8
	check(AnsiSize);

	char* Ret = reinterpret_cast<char*>(FMemory::Malloc(AnsiSize));
	check(Ret);

	FCStringAnsi::Strncpy(Ret, Ansi, AnsiSize);	// will work correctly with UTF-8
	return Ret;
}

static bool AddCmdLineArgumentTo(char** Argv, int& Argc, const FString& CurArg)
{
	if (Argc == PlatformProcessLimits::MaxArgvParameters)
	{
		UE_LOG(LogHAL, Warning, TEXT("FUnixPlatformProcess::CreateProc: too many (%d) commandline arguments passed, will only pass %d"),
			Argc, PlatformProcessLimits::MaxArgvParameters);
		return false;
	}
	Argv[Argc] = MallocedUtf8FromString(CurArg);
	UE_LOG(LogHAL, Verbose, TEXT("FUnixPlatformProcess::CreateProc: Argv[%d] = '%s'"), Argc, *CurArg);
	Argc++;
	return true;
}

// returns true if the token completes the current argument
static bool ParseCmdLineToken(const TCHAR* token, bool& OutIsInString, bool& OutHasArg, FString& OutCurArg, bool& OutEOL)
{
	if (*token == TEXT('\0'))
	{
		OutEOL = true;
		return OutHasArg;
	}

	if (*token == TEXT('"'))
	{
		// need to make sure this isn't a double-double quoted path
		// if we're currently in a string, then a double quote will only end a string if the next character is not a whitespace character
		if (OutIsInString)
		{
			// peek ahead to see if this looks like the start or end of a double-double quoted string
			FString temp(token + 1);
			bool StartDoubleDoubleQuote = !temp.IsEmpty() && !temp.StartsWith(TEXT(" ")) && !temp.StartsWith(TEXT("\n")) && !temp.StartsWith(TEXT("\r"));
			bool EndDoubleDoubleQuote = OutCurArg.StartsWith(TEXT("\"")) && temp.StartsWith(TEXT("\""));

			if (StartDoubleDoubleQuote || EndDoubleDoubleQuote)
			{
				// need to capture this quote into the argument
				OutCurArg += *token;
				OutHasArg = true;

				return false;
			}
		}

		OutIsInString = !OutIsInString;
		OutHasArg = true;
		return false;
	}

	if (*token == TEXT(' ') && !OutIsInString)
	{
		return OutHasArg;
	}

	// if we've made it this far, the token should be added to the argument
	OutCurArg += *token;
	OutHasArg = true;

	return false;
}

FProcHandle FUnixPlatformProcess::CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void* PipeReadChild, void* PipeStdErrChild)
{
	// @TODO bLaunchHidden bLaunchReallyHidden are not handled

	FString ProcessPath = URL;

	// - If the first character is /, we use the provided absolute path as-is.
	// - If no leading slash, prefer the result of FPaths::ConvertRelativePathToFull if it exists. This is roughly
	//   morally equivalent to prepending the basedir to $PATH (and in turn, Windows' cwd (==basedir) priority).
	// - If there was a (non-leading) slash, and ConvertRelativePathToFull missed, return failure.
	// - If there were no path separators in the input string, allow posix_spawnp to search the $PATH.

	int32 PathSepIdx = INDEX_NONE;
	const bool bInputHasPathSep = ProcessPath.FindChar(TEXT('/'), PathSepIdx);
	bool bAbsolutePath = PathSepIdx == 0;

	if (!bAbsolutePath)
	{
		const FString CandidatePath = FPaths::ConvertRelativePathToFull(ProcessPath);
		if (bInputHasPathSep || FPaths::FileExists(CandidatePath))
		{
			ProcessPath = CandidatePath;
			bAbsolutePath = true;
		}
	}

	// Even if we weren't passed an absolute path, we may have expanded to one above.
	if (bAbsolutePath)
	{
		if (!FPaths::FileExists(ProcessPath))
		{
			UE_LOG(LogHAL, Error, TEXT("FUnixPlatformProcess::CreateProc: File does not exist (%s)"), *ProcessPath);
			return FProcHandle();
		}

		if (!UnixPlatformProcess::AttemptToMakeExecIfNotAlready(ProcessPath))
		{
			UE_LOG(LogHAL, Error, TEXT("FUnixPlatformProcess::CreateProc: File not executable (%s)"), *ProcessPath);
			return FProcHandle();
		}
	}

	if (Parms == nullptr)
	{
		Parms = TEXT("");
	}

	const FString Commandline = FString::Printf(TEXT("\"%s\" %s"), *ProcessPath, Parms);
	UE_LOG(LogHAL, Verbose, TEXT("FUnixPlatformProcess::CreateProc: '%s'"), *Commandline);

	int Argc = 1;
	char* Argv[PlatformProcessLimits::MaxArgvParameters + 1] = { NULL };	// last argument is NULL, hence +1
	Argv[0] = MallocedUtf8FromString(ProcessPath);
	struct CleanupArgvOnExit
	{
		int Argc;
		char** Argv;	// relying on it being long enough to hold Argc elements

		CleanupArgvOnExit(int InArgc, char* InArgv[])
			: Argc(InArgc)
			, Argv(InArgv)
		{}

		~CleanupArgvOnExit()
		{
			for (int Idx = 0; Idx < Argc; ++Idx)
			{
				FMemory::Free(Argv[Idx]);
			}
		}
	} CleanupGuard(Argc, Argv);

	UE_LOG(LogHAL, Verbose, TEXT("FUnixPlatformProcess::CreateProc: ProcessPath = '%s' Parms = '%s'"), *ProcessPath, Parms);
	FString CurArg; // current argument, during parsing new chars will be appended, will be reused, once the argument is complete
	const TCHAR* CurChar = Parms; // pointer to the current char
	bool IsInString = false; // are we in a string?  if yes, spaces are treated as normal chars
	bool HasArg = false; // do we have a partial argument? CurArg might be empty if Parms contains "". Parms might contain two or more spaces in a row, so not every space indicates the end of an argument
	bool EOL = false;

	// parse Parms and fill Argv
	while (!EOL)
	{
		if (ParseCmdLineToken(CurChar, IsInString, HasArg, CurArg, EOL))
		{
			// if we're still in a string, then quit parsing because we've found a mismatched quote
			// if we can't add the argument, then we've exceeded the maximum number of allowed arguments
			if (!IsInString && !AddCmdLineArgumentTo(Argv, Argc, CurArg))
			{
				break;
			}

			HasArg = false;
			CurArg.Reset(0);
		}

		CurChar++;
	}

	if (IsInString)
	{
		UE_LOG(LogHAL, Warning, TEXT("FUnixPlatformProcess::CreateProc: mismatched quotes in command line (%s %s)"), *ProcessPath, Parms);
	}

	// we assume PlatformProcessLimits::MaxArgvParameters is >= 1. Since Argc starts at 1 and can never grow larger than PlatformProcessLimits::MaxArgvParameters,
	// we are within the Array
	Argv[Argc] = NULL;

	extern char ** environ;	// provided by libc
	pid_t ChildPid = -1;

	posix_spawnattr_t SpawnAttr;
	posix_spawnattr_init(&SpawnAttr);
	short int SpawnFlags = 0;

	// unmask all signals and set realtime signals to default for children
	// the latter is particularly important for mono, which otherwise will crash attempting to find usable signals
	// (NOTE: setting all signals to default fails)
	sigset_t EmptySignalSet;
	sigemptyset(&EmptySignalSet);
	posix_spawnattr_setsigmask(&SpawnAttr, &EmptySignalSet);
	SpawnFlags |= POSIX_SPAWN_SETSIGMASK;

	sigset_t SetToDefaultSignalSet;
	sigemptyset(&SetToDefaultSignalSet);
	for (int SigNum = SIGRTMIN; SigNum <= SIGRTMAX; ++SigNum)
	{
		sigaddset(&SetToDefaultSignalSet, SigNum);
	}
	posix_spawnattr_setsigdefault(&SpawnAttr, &SetToDefaultSignalSet);
	SpawnFlags |= POSIX_SPAWN_SETSIGDEF;

	// Makes spawned processes have its own unique group id so we can kill the entire group with out killing the parent
	SpawnFlags |= POSIX_SPAWN_SETPGROUP;

	int PosixSpawnErrNo = -1;
	if (PipeWriteChild || PipeReadChild || PipeStdErrChild)
	{
		posix_spawn_file_actions_t FileActions;
		posix_spawn_file_actions_init(&FileActions);

		if (PipeWriteChild)
		{
			const FPipeHandle* PipeReadHandle = reinterpret_cast<const FPipeHandle*>(PipeReadChild);
			const FPipeHandle* PipeWriteHandle = reinterpret_cast<const FPipeHandle*>(PipeWriteChild);

			// If using unique read and write pipes, close the other end of the write pipe
			if (PipeReadChild && PipeWriteHandle->GetPairHandle() != PipeReadHandle->GetHandle())
			{
				posix_spawn_file_actions_addclose(&FileActions, PipeWriteHandle->GetPairHandle());
			}
			posix_spawn_file_actions_adddup2(&FileActions, PipeWriteHandle->GetHandle(), STDOUT_FILENO);
		}

		if (PipeReadChild)
		{
			const FPipeHandle* PipeReadHandle = reinterpret_cast<const FPipeHandle*>(PipeReadChild);
			const FPipeHandle* PipeWriteHandle = reinterpret_cast<const FPipeHandle*>(PipeWriteChild);

			// If using unique read and write pipes, close the other end of the read pipe
			if (PipeWriteChild && PipeReadHandle->GetPairHandle() != PipeWriteHandle->GetHandle())
			{
				posix_spawn_file_actions_addclose(&FileActions, PipeReadHandle->GetPairHandle());
			}
			posix_spawn_file_actions_adddup2(&FileActions, PipeReadHandle->GetHandle(), STDIN_FILENO);
		}

		if (PipeStdErrChild)
		{
			const FPipeHandle* PipeStdErrorHandle = reinterpret_cast<const FPipeHandle*>(PipeStdErrChild);
			posix_spawn_file_actions_adddup2(&FileActions, PipeStdErrorHandle->GetHandle(), STDERR_FILENO);
		}

		posix_spawnattr_setflags(&SpawnAttr, SpawnFlags);
		PosixSpawnErrNo = posix_spawnp(&ChildPid, TCHAR_TO_UTF8(*ProcessPath), &FileActions, &SpawnAttr, Argv, environ);
		posix_spawn_file_actions_destroy(&FileActions);
	}
	else
	{
		// if we don't have any actions to do, use a faster route that will use vfork() instead.
		// This is not just faster, it is crucial when spawning a crash reporter to report a crash due to stack overflow in a thread
		// since otherwise atfork handlers will get called and posix_spawn() will crash (in glibc's __reclaim_stacks()).
		// However, it has its problems, see:
		//		http://ewontfix.com/7/
		//		https://sourceware.org/bugzilla/show_bug.cgi?id=14750
		//		https://sourceware.org/bugzilla/show_bug.cgi?id=14749
		SpawnFlags |= POSIX_SPAWN_USEVFORK;

		posix_spawnattr_setflags(&SpawnAttr, SpawnFlags);
		PosixSpawnErrNo = posix_spawnp(&ChildPid, TCHAR_TO_UTF8(*ProcessPath), nullptr, &SpawnAttr, Argv, environ);
	}
	posix_spawnattr_destroy(&SpawnAttr);

	if (PosixSpawnErrNo != 0)
	{
		UE_LOG(LogHAL, Error, TEXT("FUnixPlatformProcess::CreateProc: posix_spawnp() failed (%d, %s)"), PosixSpawnErrNo, UTF8_TO_TCHAR(strerror(PosixSpawnErrNo)));
		return FProcHandle();
	}

	// renice the child (subject to race condition).
	// Why this instead of posix_spawn_setschedparam()? 
	// Because posix_spawnattr priority is unusable under Unix due to min/max priority range being [0;0] for the default scheduler
	if (PriorityModifier != 0)
	{
		errno = 0;
		int TheirCurrentPrio = getpriority(PRIO_PROCESS, ChildPid);

		if (errno != 0)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FUnixPlatformProcess::CreateProc: could not get child's priority, errno=%d (%s)"),
				ErrNo,
				UTF8_TO_TCHAR(strerror(ErrNo))
			);
			
			// proceed anyway...
			TheirCurrentPrio = 0;
		}

		rlimit PrioLimits;
		int MaxPrio = 0;
		if (getrlimit(RLIMIT_NICE, &PrioLimits) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FUnixPlatformProcess::CreateProc: could not get priority limits (RLIMIT_NICE), errno=%d (%s)"),
				ErrNo,
				UTF8_TO_TCHAR(strerror(ErrNo))
			);

			// proceed anyway...
		}
		else
		{
			MaxPrio = 20 - PrioLimits.rlim_cur;
		}

		int NewPrio = TheirCurrentPrio;
		// This should be in sync with Mac/Windows and Also SetThreadPriority in Unix thread.
		// The single most important use of that is setting "below normal" priority to ShaderCompileWorker (PrioModifier == -1).
		// If SCW is run with too low a priority, shader compilation will be longer than needed.
		int PrioChange = 0;
		if (PriorityModifier > 0)
		{
			// decrease the nice value - will perhaps fail, it's up to the user to run with proper permissions
			PrioChange = (PriorityModifier == 1) ? -10 : -15;
		}
		else if (PriorityModifier < 0)
		{
			PrioChange = (PriorityModifier == -1) ? 5 : 10;
		}
		NewPrio += PrioChange;

		// cap to [RLIMIT_NICE, 19]
		NewPrio = FMath::Min(19, NewPrio);
		NewPrio = FMath::Max(MaxPrio, NewPrio);	// MaxPrio is actually the _lowest_ numerically priority

		if (setpriority(PRIO_PROCESS, ChildPid, NewPrio) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FUnixPlatformProcess::CreateProc: could not change child's priority (nice value) from %d to %d, errno=%d (%s)"),
				TheirCurrentPrio, NewPrio,
				ErrNo,
				UTF8_TO_TCHAR(strerror(ErrNo))
			);
		}
		else
		{
			UE_LOG(LogHAL, Verbose, TEXT("Changed child's priority (nice value) to %d (change from %d)"), NewPrio, TheirCurrentPrio);
		}
	}

	else
	{
		UE_LOG(LogHAL, Verbose, TEXT("FUnixPlatformProcess::CreateProc: spawned child %d"), ChildPid);
	}

	if (OutProcessID)
	{
		*OutProcessID = ChildPid;
	}

	// [RCL] 2015-03-11 @FIXME: is bLaunchDetached usable when determining whether we're in 'fire and forget' mode? This doesn't exactly match what bLaunchDetached is used for.
	return FProcHandle(new FProcState(ChildPid, bLaunchDetached));
}

/*
 * Return a limited use FProcHandle from a PID. Currently can only use w/ IsProcRunning().
 *
 * WARNING (from Arciel): PIDs can and will be reused. We have had that issue
 * before: the editor was tracking ShaderCompileWorker by their PIDs, and a
 * long-running process (something from PS4 SDK) got a reused SCW PID,
 * resulting in compilation never ending.
 */
FProcHandle FUnixPlatformProcess::OpenProcess(uint32 ProcessID)
{
	pid_t Pid = static_cast< pid_t >(ProcessID);

	// check if actually running
	int KillResult = kill(Pid, 0);	// no actual signal is sent
	check(KillResult != -1 || errno != EINVAL);

	// errno == EPERM: don't have permissions to send signal
	// errno == ESRCH: proc doesn't exist
	bool bIsRunning = (KillResult == 0);
	return FProcHandle(bIsRunning ? Pid : -1);
}

/** Initialization constructor. */
FProcState::FProcState(pid_t InProcessId, bool bInFireAndForget)
	:	ProcessId(InProcessId)
	,	bIsRunning(true)  // assume it is
	,	bHasBeenWaitedFor(false)
	,	ReturnCode(-1)
	,	bFireAndForget(bInFireAndForget)
{
}

FProcState::~FProcState()
{
	if (!bFireAndForget)
	{
		// If not in 'fire and forget' mode, try to catch the common problems that leave zombies:
		// - We don't want to close the handle of a running process as with our current scheme this will certainly leak a zombie.
		// - Nor we want to leave the handle unwait()ed for.
		
		if (bIsRunning)
		{
			// Warn the users before going into what may be a very long block
			UE_LOG(LogHAL, Warning, TEXT("Closing a process handle while the process (pid=%d) is still running - we will block until it exits to prevent a zombie"),
				GetProcessId()
			);
		}
		else if (!bHasBeenWaitedFor)	// if child is not running, but has not been waited for, still communicate a problem, but we shouldn't be blocked for long in this case.
		{
			UE_LOG(LogHAL, Warning, TEXT("Closing a process handle of a process (pid=%d) that has not been wait()ed for - will wait() now to reap a zombie"),
				GetProcessId()
			);
		}

		Wait();	// will exit immediately if everything is Ok
	}
	else if (IsRunning())
	{
		// warn about leaking a thread ;/
		UE_LOG(LogHAL, Verbose, TEXT("Process (pid=%d) is still running - we will reap it in a waiter thread, but the thread handle is going to be leaked."),
				 GetProcessId()
			);

		FChildWaiterThread * WaiterRunnable = new FChildWaiterThread(GetProcessId());
		// [RCL] 2015-03-11 @FIXME: do not leak
		FRunnableThread * WaiterThread = FRunnableThread::Create(WaiterRunnable, *FString::Printf(TEXT("waitpid(%d)"), GetProcessId()), 32768 /* needs just a small stack */, TPri_BelowNormal);
	}
}

bool FProcState::IsRunning()
{
	if (bIsRunning)
	{
		check(!bHasBeenWaitedFor);	// check for the sake of internal consistency

		// check if actually running
		int KillResult = kill(GetProcessId(), 0);	// no actual signal is sent
		check(KillResult != -1 || errno != EINVAL);

		bIsRunning = (KillResult == 0 || (KillResult == -1 && errno == EPERM));

		// additional check if it's a zombie
		if (bIsRunning)
		{
			for(;;)	// infinite loop in case we get EINTR and have to repeat
			{
				siginfo_t SignalInfo;
				SignalInfo.si_pid = 0;	// if remains 0, treat as child was not waitable (i.e. was running)
				if (waitid(P_PID, GetProcessId(), &SignalInfo, WEXITED | WNOHANG | WNOWAIT))
				{
					if (errno != EINTR)
					{
						int ErrNo = errno;
						UE_LOG(LogHAL, Fatal, TEXT("FUnixPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
							static_cast< int32 >(GetProcessId()), ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
						break;	// exit the loop if for some reason Fatal log (above) returns
					}
				}
				else
				{
					bIsRunning = ( SignalInfo.si_pid != GetProcessId() );
					break;
				}
			}
		}

		// If child is a zombie, wait() immediately to free up kernel resources. Higher level code
		// (e.g. shader compiling manager) can hold on to handle of no longer running process for longer,
		// which is a dubious, but valid behavior. We don't want to keep zombie around though.
		if (!bIsRunning)
		{
			UE_LOG(LogHAL, Verbose, TEXT("Child %d is no longer running (zombie), Wait()ing immediately."), GetProcessId() );
			Wait();
		}
	}

	return bIsRunning;
}

bool FProcState::GetReturnCode(int32* ReturnCodePtr)
{
	check(!bIsRunning || !"You cannot get a return code of a running process");
	if (!bHasBeenWaitedFor)
	{
		Wait();
	}

	if (ReturnCode != -1)
	{
		if (ReturnCodePtr != NULL)
		{
			*ReturnCodePtr = ReturnCode;
		}
		return true;
	}

	return false;
}

void FProcState::Wait()
{
	if (bHasBeenWaitedFor)
	{
		return;	// we could try waitpid() another time, but why
	}

	for(;;)	// infinite loop in case we get EINTR and have to repeat
	{
		siginfo_t SignalInfo;
		if (waitid(P_PID, GetProcessId(), &SignalInfo, WEXITED))
		{
			if (errno != EINTR)
			{
				int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("FUnixPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
					static_cast< int32 >(GetProcessId()), ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
				break;	// exit the loop if for some reason Fatal log (above) returns
			}
		}
		else
		{
			check(SignalInfo.si_pid == GetProcessId());

			ReturnCode = (SignalInfo.si_code == CLD_EXITED) ? SignalInfo.si_status : -1;
			bHasBeenWaitedFor = true;
			bIsRunning = false;	// set in advance
			UE_LOG(LogHAL, Verbose, TEXT("Child %d's return code is %d."), GetProcessId(), ReturnCode);
			break;
		}
	}
}

bool FUnixPlatformProcess::IsProcRunning( FProcHandle & ProcessHandle )
{
	bool bIsRunning = false;
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();

	if (ProcInfo)
	{
		bIsRunning = ProcInfo->IsRunning();
	}
	else if (ProcessHandle.Get() != -1)
	{
		// Process opened with OpenProcess() call (we only have pid)
		int KillResult = kill(ProcessHandle.Get(), 0);	// no actual signal is sent
		check(KillResult != -1 || errno != EINVAL);

		// errno == EPERM: don't have permissions to send signal
		// errno == ESRCH: proc doesn't exist
		bIsRunning = (KillResult == 0);
	}

	return bIsRunning;
}

void FUnixPlatformProcess::WaitForProc( FProcHandle & ProcessHandle )
{
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	if (ProcInfo)
	{
		ProcInfo->Wait();
	}
	else if (ProcessHandle.Get() != -1)
	{
		STUBBED("FUnixPlatformProcess::WaitForProc() : Waiting on OpenProcess() handle not implemented yet");
	}
}

void FUnixPlatformProcess::CloseProc(FProcHandle & ProcessHandle)
{
	// dispose of both handle and process info
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	ProcessHandle.Reset();

	delete ProcInfo;
}

void FUnixPlatformProcess::TerminateProc( FProcHandle & ProcessHandle, bool KillTree )
{
	if (KillTree)
	{
		// TODO: enumerate the children
		STUBBED("FUnixPlatformProcess::TerminateProc() : Killing a subtree is not implemented yet");
	}

	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	if (ProcInfo)
	{
		int KillResult = kill(ProcInfo->GetProcessId(), SIGTERM);	// graceful
		check(KillResult != -1 || errno != EINVAL);
	}
	else if (ProcessHandle.Get() != -1)
	{
		STUBBED("FUnixPlatformProcess::TerminateProc() : Terminating OpenProcess() handle not implemented");
	}
}

static FDelegateHandle OnEndFrameHandle;

/*
 * WaitAndFork on Unix
 *
 * This is a function that halts execution and waits for signals to cause forked processes to be created and continue execution.
 * The parent process will return when IsEngineExitRequested() is true. SIGRTMIN+1 is used to cause a fork to happen.
 * Optionally, the parent process will also return if any of the children close with an exit code equal to WAIT_AND_FORK_PARENT_SHUTDOWN_EXIT_CODE if it is set to non-zero.
 * If sigqueue is used, the payload int will be split into the upper and lower uint16 values. The upper value is a "cookie" and the
 *     lower value is an "index". These two values will be used to name the process using the pattern DS-<cookie>-<index>. This name
 *     can be used to uniquely discover the process that was spawned.
 * If -NumForks=x is suppled on the command line, x forks will be made when the function is called, and if any forked processes close for any reason, they will be reopened
 * If -WaitAndForkCmdLinePath=Foo is suppled, the command line parameters of the child processes will be filled out with the contents
 *     of files found in the directory referred to by Foo, where the child's "index" is the name of the file to be read in the directory.
 * If -WaitAndForkRequireResponse is on the command line, child processes will not proceed after being spawned until a SIGRTMIN+2 signal is sent to them.
 */
FGenericPlatformProcess::EWaitAndForkResult FUnixPlatformProcess::WaitAndFork()
{
#define WAIT_AND_FORK_QUEUE_LENGTH 4096
#define WAIT_AND_FORK_PARENT_SLEEP_DURATION 10
#define WAIT_AND_FORK_CHILD_SPAWN_DELAY 0.125
#ifndef WAIT_AND_FORK_PARENT_SHUTDOWN_EXIT_CODE
	#define WAIT_AND_FORK_PARENT_SHUTDOWN_EXIT_CODE 0
#endif
#ifndef WAIT_AND_FORK_RESPONSE_TIMEOUT_EXIT_CODE
	#define WAIT_AND_FORK_RESPONSE_TIMEOUT_EXIT_CODE 1
#endif
	
	// Only works in -nothreading mode for now (probably best this way)
	if (FPlatformProcess::SupportsMultithreading())
	{
		return EWaitAndForkResult::Error;
	}

	struct FForkSignalData
	{
		FForkSignalData() = default;
		FForkSignalData(int32 InSignal, double InTimeSeconds) : SignalValue(InSignal), TimeSeconds(InTimeSeconds) {}

		int32 SignalValue = 0;
		double TimeSeconds = 0.0;
	};

	static TCircularQueue<FForkSignalData> WaitAndForkSignalQueue(WAIT_AND_FORK_QUEUE_LENGTH);

	// If we asked to fork up front without the need to send signals, just push the fork requests on the queue and we will refork them if they close
	// This is mostly used in cases where there is no external process sending signals to this process to create forks and is a simple way to start or test
	int32 NumForks = 0;
	FParse::Value(FCommandLine::Get(), TEXT("-NumForks="), NumForks);
	if (NumForks > 0)
	{
		for (int32 ForkIdx = 0; ForkIdx < NumForks; ++ForkIdx)
		{
			WaitAndForkSignalQueue.Enqueue(FForkSignalData(ForkIdx + 1, FPlatformTime::Seconds()));
		}
	}

	// If we asked to fill out command line parameters from files on disk, read the folder that contains the parameters
	FString ChildParametersPath;
	FParse::Value(FCommandLine::Get(), TEXT("-WaitAndForkCmdLinePath="), ChildParametersPath);
	if (!ChildParametersPath.IsEmpty())
	{
		bool bDirExists = IFileManager::Get().DirectoryExists(*ChildParametersPath);
		if (!bDirExists)
		{
			UE_LOG(LogHAL, Fatal, TEXT("Path referred to by -WaitAndForkCmdLinePath does not exist: %s"), *ChildParametersPath);
		}
	}

	// If we are asked to wait for a response signal, keep track of that here so we can behave differently in children.
	const bool bRequireResponseSignal = FParse::Param(FCommandLine::Get(), TEXT("WaitAndForkRequireResponse"));

	double WaitAndForkResponseTimeout = -1.0;
	FParse::Value(FCommandLine::Get(), TEXT("-WaitAndForkResponseTimeout="), WaitAndForkResponseTimeout);
	if (WaitAndForkResponseTimeout > 0.0)
	{
		UE_LOG(LogHAL, Log, TEXT("WaitAndFork setting WaitAndForkResponseTimeout to %0.2f seconds."), WaitAndForkResponseTimeout);
	}

	FCoreDelegates::OnParentBeginFork.Broadcast();

	// Set up a signal handler for the signal to fork()
	{
		struct sigaction Action;
		FMemory::Memzero(Action);
		sigfillset(&Action.sa_mask);
		Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
		Action.sa_sigaction = [](int32 Signal, siginfo_t* Info, void* Context) {
			if (Signal == WAIT_AND_FORK_QUEUE_SIGNAL && Info)
			{
				WaitAndForkSignalQueue.Enqueue(FForkSignalData(Info->si_value.sival_int, FPlatformTime::Seconds()));
			}
		};
		sigaction(WAIT_AND_FORK_QUEUE_SIGNAL, &Action, nullptr);
	}

	UE_LOG(LogHAL, Log, TEXT("   *** WaitAndFork awaiting signal %d to process pid %d create child processes... ***"), WAIT_AND_FORK_QUEUE_SIGNAL, FPlatformProcess::GetCurrentProcessId());
	GLog->Flush();

	struct FMemoryStatsHolder
	{
		double AvailablePhysical;
		double PeakUsedPhysical;
		double PeakUsedVirtual;

		constexpr double ByteToMiB(uint64 InBytes) { return static_cast<double>(InBytes) / (1024.0 * 1024.0); }

		FMemoryStatsHolder(const FPlatformMemoryStats& PlatformStats)
			: AvailablePhysical(ByteToMiB(PlatformStats.AvailablePhysical))
			, PeakUsedPhysical(ByteToMiB(PlatformStats.PeakUsedPhysical))
			, PeakUsedVirtual(ByteToMiB(PlatformStats.PeakUsedVirtual))
		{ }
	};

	FMemoryStatsHolder PreviousMasterMemStats(FPlatformMemory::GetStats());

	EWaitAndForkResult RetVal = EWaitAndForkResult::Parent;
	struct FPidAndSignal
	{
		pid_t Pid;
		int32 SignalValue;

		FPidAndSignal() : SignalValue(0) {}
		FPidAndSignal(pid_t InPid, int32 InSignalValue) : Pid(InPid), SignalValue(InSignalValue) {}
	};
	TArray<FPidAndSignal> AllChildren;
	AllChildren.Reserve(1024); // Sized to be big enough that it probably wont reallocate, but its not the end of the world if it does.
	while (!IsEngineExitRequested())
	{
		BeginExitIfRequested();

		FForkSignalData SignalData;
		if (WaitAndForkSignalQueue.Dequeue(SignalData))
		{
			// Sleep for a short while to avoid spamming new processes to the OS all at once
			FPlatformProcess::Sleep(WAIT_AND_FORK_CHILD_SPAWN_DELAY);

			uint16 Cookie = (SignalData.SignalValue >> 16) & 0xffff;
			uint16 ChildIdx = SignalData.SignalValue & 0xffff;

			FDateTime SignalReceived = FDateTime::FromUnixTimestamp(FMath::FloorToInt64(SignalData.TimeSeconds));

			FCoreDelegates::OnParentPreFork.Broadcast();

			UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork processing child request %04hx-%04hx received at: %s"), Cookie, ChildIdx, *SignalReceived.ToString());

			FMemoryStatsHolder CurrentMasterMemStats(FPlatformMemory::GetStats());
			UE_LOG(LogHAL, Log, TEXT("MemoryStats PreFork: AvailablePhysical: %.02fMiB (%+.02fMiB), PeakPhysical: %.02fMiB, PeakVirtual: %.02fMiB"),
				CurrentMasterMemStats.AvailablePhysical, (CurrentMasterMemStats.AvailablePhysical - PreviousMasterMemStats.AvailablePhysical),
				CurrentMasterMemStats.PeakUsedPhysical,
				CurrentMasterMemStats.PeakUsedVirtual				
			);
			PreviousMasterMemStats = CurrentMasterMemStats;
			
			// Make sure there are no pending messages in the log.
			GLog->Flush();

			// This should be the very last thing we do before forking for optimal interaction with GMalloc
			FForkProcessHelper::LowLevelPreFork();
			// ******** The fork happens here! ********
			pid_t ChildPID = fork();
			// ******** The fork happened! This is now either the parent process or the new child process ********

			if (ChildPID == -1)
			{
				// Error handling
				// We could return with an error code here, but instead it is somewhat better to log out an error and continue since this loop is supposed to be stable.
				// Fork errors may include hitting process limits or other environmental factors so we will just report the issue since the environmental factor can be
				// fixed while the process is still running.
				int ErrNo = errno;
				UE_LOG(LogHAL, Error, TEXT("WaitAndFork failed to fork! fork() error:%d"), ErrNo);
			}
			else if (ChildPID == 0)
			{
				// This should be the very first thing we do after forking for optimal interaction with GMalloc
				FForkProcessHelper::LowLevelPostForkChild(ChildIdx);

				if (FPlatformMemory::HasForkPageProtectorEnabled())
				{
					UE::FForkPageProtector::OverrideGMalloc();
					UE::FForkPageProtector::Get().ProtectMemoryRegions();
				}

				// Close the log state we inherited from our parent
				GLog->TearDown();

				// Update GGameThreadId
				FUnixTLS::ClearThreadIdTLS();
				GGameThreadId = FUnixTLS::GetCurrentThreadId();

				// Fix the command line, if a path to command line parameters was specified
				if (!ChildParametersPath.IsEmpty() && ChildIdx > 0)
				{
					FString NewCmdLine;
					const FString CmdLineFilename = ChildParametersPath / FString::Printf(TEXT("%d"), ChildIdx);
					FFileHelper::LoadFileToString(NewCmdLine, *CmdLineFilename);
					if (!NewCmdLine.IsEmpty())
					{
						FCommandLine::Set(*NewCmdLine);
					}
					else
					{
						UE_LOG(LogHAL, Error, TEXT("[Child] WaitAndFork child %04hx-%04hx failed to set command line from: %s"), Cookie, ChildIdx, *CmdLineFilename);
					}
				}

				// Start up the log again
				FPlatformOutputDevices::SetupOutputDevices();
				GLog->SetCurrentThreadAsPrimaryThread();

				// Set the process name, if specified
				if (ChildIdx > 0)
				{
					if (prctl(PR_SET_NAME, TCHAR_TO_UTF8(*FString::Printf(TEXT("DS-%04hx-%04hx"), Cookie, ChildIdx))) != 0)
					{
						int ErrNo = errno;
						UE_LOG(LogHAL, Fatal, TEXT("[Child] WaitAndFork failed to set process name with prctl! error:%d"), ErrNo);
					}
				}

				// Need to remove the child process from the responsibility of keeping track of a valid sibling process.
				UnixCrashReporterTracker::RemoveValidCrashReportTickerForChildProcess();

				// If requested, now wait for a SIGRTMIN+2 signal before continuing execution.
				if (bRequireResponseSignal && (ChildIdx == 0 || ChildIdx > NumForks))
				{
					static bool bResponseReceived = false;
					struct sigaction Action;
					FMemory::Memzero(Action);
					sigfillset(&Action.sa_mask);
					Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
					Action.sa_sigaction = [](int32 Signal, siginfo_t* Info, void* Context) {
						if (Signal == WAIT_AND_FORK_RESPONSE_SIGNAL)
						{
							bResponseReceived = true;
						}
					};
					sigaction(WAIT_AND_FORK_RESPONSE_SIGNAL, &Action, nullptr);

					const double StartChildWaitSeconds = FPlatformTime::Seconds();

					UE_LOG(LogHAL, Log, TEXT("[Child] WaitAndFork child %04hx-%04hx waiting for signal %d to proceed."), Cookie, ChildIdx, WAIT_AND_FORK_RESPONSE_SIGNAL);
					while (!IsEngineExitRequested() && !bResponseReceived)
					{
						FPlatformProcess::Sleep(1);

						// Check to see how long we've been waiting and if we should time out.
						if ((WaitAndForkResponseTimeout > 0.0) && ((FPlatformTime::Seconds() - StartChildWaitSeconds) > WaitAndForkResponseTimeout))
						{
							UE_LOG(LogHAL, Error, TEXT("[Child] WaitAndFork child %04hx-%04hx has exceeded WAIT_AND_FORK_RESPONSE_SIGNAL timeout"), Cookie, ChildIdx);
							FPlatformMisc::RequestExitWithStatus(true, WAIT_AND_FORK_RESPONSE_TIMEOUT_EXIT_CODE);
							break;
						}
					}

					FMemory::Memzero(Action);
					sigaction(WAIT_AND_FORK_RESPONSE_SIGNAL, &Action, nullptr);
				}

				UE_LOG(LogHAL, Log, TEXT("[Child] WaitAndFork child process %04hx-%04hx has started with pid %d."), Cookie, ChildIdx, GetCurrentProcessId());
				FApp::PrintStartupLogMessages();

				OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddStatic(FUnixPlatformProcess::OnChildEndFramePostFork);
				FCoreDelegates::OnPostFork.Broadcast(EForkProcessRole::Child);

#if UE_CHECK_LARGE_ALLOCATIONS
				if (CVarEnableLargeAllocationChecksAfterFork.GetValueOnAnyThread())
				{
					UE::Memory::Private::CVarEnableLargeAllocationChecks->Set(true);
				}
#endif

				// Children break out of the loop and return
				RetVal = EWaitAndForkResult::Child;
				break;
			}
			else
			{
				// This should be the very first thing we do after forking for optimal interaction with GMalloc
				FForkProcessHelper::LowLevelPostForkParent();

				// Parent
				AllChildren.Emplace(ChildPID, SignalData.SignalValue);

				FCoreDelegates::OnPostFork.Broadcast(EForkProcessRole::Parent);

				UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork Successfully processed request %04hx-%04hx, made a child with pid %d! Total number of children: %d."), Cookie, ChildIdx, ChildPID, AllChildren.Num());
			}
		}
		else
		{
			// No signal to process. Sleep for a bit and do some bookkeeping.
			FPlatformProcess::Sleep(WAIT_AND_FORK_PARENT_SLEEP_DURATION);

			// Trim terminated children
			for (int32 ChildIdx = AllChildren.Num() - 1; ChildIdx >= 0; --ChildIdx)
			{
				const FPidAndSignal& ChildPidAndSignal = AllChildren[ChildIdx];

				int32 Status = 0;
				pid_t WaitResult = waitpid(ChildPidAndSignal.Pid, &Status, WNOHANG);
				if (WaitResult == -1)
				{
					int32 ErrNo = errno;
					UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork unknown error while querying existance of child %d. Error:%d"), ChildPidAndSignal.Pid, ErrNo);
				}
				else if (WaitResult != 0)
				{
					int32 ExitCode = WIFEXITED(Status) ? WEXITSTATUS(Status) : 0;
					if (ExitCode != 0 && ExitCode == WAIT_AND_FORK_PARENT_SHUTDOWN_EXIT_CODE)
					{
						UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork child %d exited with return code %d, indicating that the parent process should shut down. Shutting down..."), ChildPidAndSignal.Pid, WAIT_AND_FORK_PARENT_SHUTDOWN_EXIT_CODE);
						RequestEngineExit(TEXT("Unix Child has exited"));
					}
					else if (NumForks > 0 && ChildPidAndSignal.SignalValue > 0 && ChildPidAndSignal.SignalValue <= NumForks)
					{
						UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork child %d missing. This was NumForks child %d. Relaunching..."), ChildPidAndSignal.Pid, ChildPidAndSignal.SignalValue);
						WaitAndForkSignalQueue.Enqueue(FForkSignalData(ChildPidAndSignal.SignalValue, FPlatformTime::Seconds()));
					}
					else
					{
						UE_LOG(LogHAL, Log, TEXT("[Parent] WaitAndFork child %d missing. Removing from children list..."), ChildPidAndSignal.Pid);
					}

					AllChildren.RemoveAt(ChildIdx, 1, EAllowShrinking::No);
				}
			}
		}
	}

	// Clean up the queue signal handler from earlier.
	{
		struct sigaction Action;
		FMemory::Memzero(Action);
		sigaction(WAIT_AND_FORK_QUEUE_SIGNAL, &Action, nullptr);
	}

	return RetVal;
}

uint32 FUnixPlatformProcess::GetCurrentProcessId()
{
	return getpid();
}

uint32 FUnixPlatformProcess::GetCurrentCoreNumber()
{
	return sched_getcpu();
}

void FUnixPlatformProcess::SetCurrentWorkingDirectoryToBaseDir()
{
#if defined(DISABLE_CWD_CHANGES) && DISABLE_CWD_CHANGES != 0
	check(false);
#else
	FPlatformMisc::CacheLaunchDir();
	chdir(TCHAR_TO_ANSI(FPlatformProcess::BaseDir()));
#endif
}

FString FUnixPlatformProcess::GetCurrentWorkingDirectory()
{
	// get the current directory
	ANSICHAR CurrentDir[UNIX_MAX_PATH] = { 0 };
	(void)getcwd(CurrentDir, sizeof(CurrentDir));
	return UTF8_TO_TCHAR(CurrentDir);
}

bool FUnixPlatformProcess::GetProcReturnCode(FProcHandle& ProcHandle, int32* ReturnCode)
{
	if (IsProcRunning(ProcHandle))
	{
		return false;
	}

	FProcState * ProcInfo = ProcHandle.GetProcessInfo();
	if (ProcInfo)
	{
		return ProcInfo->GetReturnCode(ReturnCode);
	}
	else if (ProcHandle.Get() != -1)
	{
		STUBBED("FUnixPlatformProcess::GetProcReturnCode() : Return code of OpenProcess() handle not implemented yet");
	}

	return false;
}

bool FUnixPlatformProcess::Daemonize()
{
	if (daemon(1, 1) == -1)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("daemon(1, 1) failed with errno = %d (%s)"), ErrNo,
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	return true;
}

bool FUnixPlatformProcess::IsApplicationRunning( uint32 ProcessId )
{
	// PID 0 is not a valid user application so lets ignore it as valid
	if (ProcessId == 0)
	{
		return false;
	}

	errno = 0;
	getpriority(PRIO_PROCESS, ProcessId);
	return errno == 0;
}

bool FUnixPlatformProcess::IsApplicationRunning( const TCHAR* ProcName )
{
	FString Commandline = TEXT("pidof '");
	Commandline += ProcName;
	Commandline += TEXT("'  > /dev/null");
	return !system(TCHAR_TO_UTF8(*Commandline));
}

static bool ReadPipeToStr(void *PipeRead, FString *OutStr)
{
	if (PipeRead)
	{
		FString NewLine = FPlatformProcess::ReadPipe(PipeRead);

		if (NewLine.Len() > 0)
		{
			if (OutStr != nullptr)
			{
				*OutStr += NewLine;
			}

			return true;
		}
	}

	return false;
}

bool FUnixPlatformProcess::ExecProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr, const TCHAR* OptionalWorkingDirectory, bool bShouldEndWithParentProcess)
{
	FString CmdLineParams = Params;
	FString ExecutableFileName = URL;
	int32 ReturnCode = -1;

	void* PipeReadStdOut = nullptr;
	void* PipeWriteStdOut = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeReadStdOut, PipeWriteStdOut));

	void* PipeReadStdErr = nullptr;
	void* PipeWriteStdErr = nullptr;
	if (OutStdErr)
	{
		verify(FPlatformProcess::CreatePipe(PipeReadStdErr, PipeWriteStdErr));
	}

	bool bInvoked = false;

	const bool bLaunchDetached = true;
	const bool bLaunchHidden = false;
	const bool bLaunchReallyHidden = bLaunchHidden;

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, NULL, 0, OptionalWorkingDirectory, PipeWriteStdOut, nullptr, PipeWriteStdErr);

	if (ProcHandle.IsValid())
	{
		while (FPlatformProcess::IsProcRunning(ProcHandle))
		{
			ReadPipeToStr(PipeReadStdOut, OutStdOut);
			ReadPipeToStr(PipeReadStdErr, OutStdErr);
			FPlatformProcess::Sleep(0.5);
		}

		// Read the remainder
		bool bReadingStdOut = true;
		bool bReadingStdErr = true;
		while (bReadingStdOut || bReadingStdErr)
		{
			if (bReadingStdOut && !ReadPipeToStr(PipeReadStdOut, OutStdOut))
			{
				bReadingStdOut = false;
			}

			if (bReadingStdErr && !ReadPipeToStr(PipeReadStdErr, OutStdErr))
			{
				bReadingStdErr = false;
			}
		}

		FPlatformProcess::Sleep(0.5);

		bInvoked = true;
		bool bGotReturnCode = FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
		check(bGotReturnCode)
		if (OutReturnCode != nullptr)
		{
			*OutReturnCode = ReturnCode;
		}

		FPlatformProcess::CloseProc(ProcHandle);
	}
	else
	{
		bInvoked = false;
		if (OutReturnCode != nullptr)
		{
			*OutReturnCode = -1;
		}
		if (OutStdOut != nullptr)
		{
			*OutStdOut = "";
		}
		if (OutStdErr != nullptr)
		{
			*OutStdErr = "";
		}
		UE_LOG(LogHAL, Warning, TEXT("Failed to launch Tool. (%s)"), *ExecutableFileName);
	}

	FPlatformProcess::ClosePipe(PipeReadStdOut, PipeWriteStdOut);
	FPlatformProcess::ClosePipe(PipeReadStdErr, PipeWriteStdErr);
	return bInvoked;
}

bool FUnixPlatformProcess::LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms, ELaunchVerb::Type Verb, bool bPromptToOpenOnFailure)
{
	// TODO This ignores parms and verb
	pid_t pid = fork();
	if (pid == 0)
	{
		exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_UTF8(FileName), (char *)0));
	}

	return pid != -1;
}

void FUnixPlatformProcess::ExploreFolder( const TCHAR* FilePath )
{
	struct stat st;
	TCHAR TruncatedPath[UNIX_MAX_PATH] = TEXT("");
	FCString::Strcpy(TruncatedPath, FilePath);

	if (stat(TCHAR_TO_UTF8(FilePath), &st) == 0)
	{
		// we just want the directory portion of the path
		if (!S_ISDIR(st.st_mode))
		{
			for (int i=FCString::Strlen(TruncatedPath)-1; i > 0; i--)
			{
				if (TruncatedPath[i] == TCHAR('/'))
				{
					TruncatedPath[i] = 0;
					break;
				}
			}
		}

		// launch file manager
		pid_t pid = fork();
		if (pid == 0)
		{
			exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_UTF8(TruncatedPath), (char *)0));
		}
	}
}

/**
 * Private struct to store implementation specific data.
 */
struct FProcEnumData
{
	// Array of processes.
	TArray<FUnixPlatformProcess::FProcEnumInfo> Processes;

	// Current process id.
	uint32 CurrentProcIndex;
};

FUnixPlatformProcess::FProcEnumerator::FProcEnumerator()
{
		Data = new FProcEnumData;
	Data->CurrentProcIndex = -1;
	
	TArray<uint32> PIDs;
	
	class FPIDsCollector : public IPlatformFile::FDirectoryVisitor
	{
	public:
		FPIDsCollector(TArray<uint32>& InPIDsToCollect)
			: PIDsToCollect(InPIDsToCollect)
		{ }

		bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			FString StrPID = FPaths::GetBaseFilename(FilenameOrDirectory);
			
			if (bIsDirectory && FCString::IsNumeric(*StrPID))
			{
				PIDsToCollect.Add(FCString::Atoi(*StrPID));
			}
			
			return true;
		}

	private:
		TArray<uint32>& PIDsToCollect;
	} PIDsCollector(PIDs);

	IPlatformFile::GetPlatformPhysical().IterateDirectory(TEXT("/proc"), PIDsCollector);
	
	for (auto PID : PIDs)
	{
		Data->Processes.Add(FProcEnumInfo(PID));
	}
}

FUnixPlatformProcess::FProcEnumerator::~FProcEnumerator()
{
	delete Data;
}

FUnixPlatformProcess::FProcEnumInfo FUnixPlatformProcess::FProcEnumerator::GetCurrent() const
{
	return Data->Processes[Data->CurrentProcIndex];
}

bool FUnixPlatformProcess::FProcEnumerator::MoveNext()
{
	if (Data->CurrentProcIndex + 1 == Data->Processes.Num())
	{
		return false;
	}

	++Data->CurrentProcIndex;

	return true;
}

FUnixPlatformProcess::FProcEnumInfo::FProcEnumInfo(uint32 InPID)
	: PID(InPID)
{

}

uint32 FUnixPlatformProcess::FProcEnumInfo::GetPID() const
{
	return PID;
}

uint32 FUnixPlatformProcess::FProcEnumInfo::GetParentPID() const
{
	char Buf[256];
	uint32 DummyNumber;
	char DummyChar;
	uint32 ParentPID;
	
	sprintf(Buf, "/proc/%d/stat", GetPID());
	
	FILE* FilePtr = fopen(Buf, "r");
	if (fscanf(FilePtr, "%d %s %c %d", &DummyNumber, Buf, &DummyChar, &ParentPID) != 4)
	{
		ParentPID = 1;
	}
	fclose(FilePtr);

	return ParentPID;
}

FString FUnixPlatformProcess::FProcEnumInfo::GetFullPath() const
{
	return GetApplicationName(GetPID());
}

FString FUnixPlatformProcess::FProcEnumInfo::GetName() const
{
	return FPaths::GetCleanFilename(GetFullPath());
}

static int GFileLockDescriptor = -1;

bool FUnixPlatformProcess::IsFirstInstance()
{
	// set default return if we are unable to access lock file.
	static bool bIsFirstInstance = false;
	static bool bNeverFirst = FParse::Param(FCommandLine::Get(), TEXT("neverfirst"));

	if (!bIsFirstInstance && !bNeverFirst)	// once we determined that we're first, this can never change until we exit; otherwise, we re-check each time
	{
		// create the file if it doesn't exist
		if (GFileLockDescriptor == -1)
		{
			FString LockFileName(TEXT("/tmp/"));
			FString ExecPath(FPlatformProcess::ExecutableName());
			ExecPath.ReplaceInline(TEXT("/"), TEXT("-"), ESearchCase::CaseSensitive);
			// [RCL] 2015-09-20: can run out of filename limits (256 bytes) due to a long path, be conservative and assume 4-char UTF-8 name like e.g. Japanese
			ExecPath.RightInline(80, EAllowShrinking::No);

			LockFileName += ExecPath;

			GFileLockDescriptor = open(TCHAR_TO_UTF8(*LockFileName), O_RDWR | O_CREAT, 0666);
		}

		if (GFileLockDescriptor != -1)
		{
			if (flock(GFileLockDescriptor, LOCK_EX | LOCK_NB) == 0)
			{
				// lock file successfully locked by this process - no more checking if we're first!
				bIsFirstInstance = true;
			}
			else
			{
				// we were unable to lock file. so some other process beat us to lock file.
				bIsFirstInstance = false;
			}
		}
	}

	return bIsFirstInstance;
}

void FUnixPlatformProcess::CeaseBeingFirstInstance()
{
	if (GFileLockDescriptor != -1)
	{
		// may fail if we didn't have the lock
		flock(GFileLockDescriptor, LOCK_UN | LOCK_NB);
		close(GFileLockDescriptor);
		GFileLockDescriptor = -1;
	}
}

void FUnixPlatformProcess::OnChildEndFramePostFork()
{
	FCoreDelegates::OnEndFrame.Remove(OnEndFrameHandle);
	OnEndFrameHandle.Reset();

	FCoreDelegates::OnChildEndFramePostFork.Broadcast();
}

int32 FUnixPlatformProcess::TranslateThreadPriority(EThreadPriority Priority)
{
	// In general, the range is -20 to 19 (negative is highest, positive is lowest)
	int32 NiceLevel = 0;
	switch (Priority)
	{
		case TPri_TimeCritical:
			NiceLevel = -20;
			break;

		case TPri_Highest:
			NiceLevel = -15;
			break;

		case TPri_AboveNormal:
			NiceLevel = -10;
			break;

		case TPri_Normal:
			NiceLevel = 0;
			break;

		case TPri_SlightlyBelowNormal:
			NiceLevel = 3;
			break;

		case TPri_BelowNormal:
			NiceLevel = 5;
			break;

		case TPri_Lowest:
			NiceLevel = 10;		// 19 is a total starvation
			break;

		default:
			UE_LOG(LogHAL, Fatal, TEXT("Unknown Priority passed to FRunnableThreadPThread::TranslateThreadPriority()"));
			return 0;
	}

	// note: a non-privileged process can only go as low as RLIMIT_NICE
	return NiceLevel;
}

void FUnixPlatformProcess::SetThreadNiceValue(uint32_t ThreadId, int32 NiceValue)
{
	// We still try to set priority, but failure is not considered as an error
	if (setpriority(PRIO_PROCESS, ThreadId, NiceValue) != 0 && WITH_PROCESS_PRIORITY_CONTROL)
	{
		static bool bIsLogged = false;
		if (!bIsLogged)
		{
			bIsLogged = true;
			// Unfortunately this is going to be a frequent occurence given that by default Unix doesn't allow raising priorities.
			// NOTE: In WSL run "sudo prlimit --nice=40 --pid $$" to promote current shell to change nice values.
			int ErrNo = errno;
			UE_LOG(LogHAL, Error, TEXT("Can't set nice to %d. Reason = %s. Do you have CAP_SYS_NICE capability?"), NiceValue, ANSI_TO_TCHAR(strerror(ErrNo)));
		}
	}
}

void FUnixPlatformProcess::SetThreadPriority(EThreadPriority NewPriority)
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	int32 NiceValue = FUnixPlatformProcess::TranslateThreadPriority(NewPriority);

	FUnixPlatformProcess::SetThreadNiceValue(ThreadId, NiceValue);
}
