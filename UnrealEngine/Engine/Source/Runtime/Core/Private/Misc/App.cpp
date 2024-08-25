// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/CompactBinary.h"
#include "HAL/LowLevelMemTracker.h"
#include "BuildSettings.h"
#include "UObject/DevObjectVersion.h"
#include "Misc/EngineVersion.h"
#include "Misc/NetworkVersion.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogApp, Log, All);

/* FApp static initialization
 *****************************************************************************/

#if UE_BUILD_DEVELOPMENT
bool FApp::bIsDebugGame = false;
#endif

FGuid FApp::SessionId = ToGuid(FApp::GetSessionObjectId());
FString FApp::SessionName = FString();
FString FApp::SessionOwner = FString();
FString FApp::GraphicsRHI = FString();
TArray<FString> FApp::SessionUsers = TArray<FString>();
bool FApp::Standalone = true;
bool FApp::bIsBenchmarking = false;
bool FApp::bUseFixedSeed = false;
bool FApp::bUseFixedTimeStep = false;
double FApp::FixedDeltaTime = 1 / 30.0;
double FApp::CurrentTime = 0.0;
double FApp::LastTime = 0.0;
double FApp::DeltaTime = 1 / 30.0;
double FApp::IdleTime = 0.0;
double FApp::IdleTimeOvershoot = 0.0;
double FApp::GameTime = 0.0;
TOptional<FQualifiedFrameTime> FApp::CurrentFrameTime;
float FApp::VolumeMultiplier = 1.0f;
float FApp::UnfocusedVolumeMultiplier = 0.0f;
bool FApp::bUseVRFocus = false;
bool FApp::bHasVRFocus = false;
bool (*FApp::HasFocusFunction)() = nullptr;


/* FApp static interface
 *****************************************************************************/

FString FApp::GetBranchName()
{
	return FString(BuildSettings::GetBranchName());
}

const TCHAR* FApp::GetBuildVersion()
{
	return BuildSettings::GetBuildVersion();
}

const TCHAR* FApp::GetBuildURL()
{
	if (FCoreDelegates::OnGetBuildURL.IsBound()) 
	{
		return FCoreDelegates::OnGetBuildURL.Execute();
	}	
	return BuildSettings::GetBuildURL();
}	

int32 FApp::GetEngineIsPromotedBuild()
{
	return BuildSettings::IsPromotedBuild()? 1 : 0;
}

bool FApp::GetIsWithDebugInfo()
{
	return BuildSettings::IsWithDebugInfo();
}

const TCHAR* FApp::GetExecutingJobURL()
{
	if (FCoreDelegates::OnGetExecutingJobURL.IsBound())
	{
		return FCoreDelegates::OnGetExecutingJobURL.Execute();
	}

	static const FString URL = []() -> FString {
		FString HordeUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_URL"));
		if (HordeUrl.IsEmpty())
		{
			return FString();
		}
		FString HordeJobId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_JOBID"));
		if (HordeJobId.IsEmpty())
		{
			return FString();
		}
		FString HordeStepId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_STEPID"));
		if (HordeStepId.IsEmpty())
		{
			return FString::Printf(TEXT("%s/job/%s"), *HordeUrl, *HordeJobId);
		}
		else 
		{
			return FString::Printf(TEXT("%s/job/%s?step=%s"), *HordeUrl, *HordeJobId, *HordeStepId);
		}
	}();
	return *URL;
}

FString FApp::GetEpicProductIdentifier()
{
	return FString(TEXT(EPIC_PRODUCT_IDENTIFIER));
}

EBuildConfiguration FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfiguration::Debug;

#elif UE_BUILD_DEVELOPMENT
	return bIsDebugGame ? EBuildConfiguration::DebugGame : EBuildConfiguration::Development;

#elif UE_BUILD_SHIPPING
	return EBuildConfiguration::Shipping;

#elif UE_BUILD_TEST
	return EBuildConfiguration::Test;

#else
	return EBuildConfiguration::Unknown;
#endif
}

EBuildTargetType FApp::GetBuildTargetType()
{
#if IS_CLIENT_TARGET
	return EBuildTargetType::Client;
#elif UE_GAME
	return EBuildTargetType::Game;
#elif UE_EDITOR
	return EBuildTargetType::Editor;
#elif UE_SERVER
	return EBuildTargetType::Server;
#elif IS_PROGRAM
	return EBuildTargetType::Program;
#else
	static_assert(false, "No host type is set.");
#endif
}

#if UE_BUILD_DEVELOPMENT
void FApp::SetDebugGame(bool bInIsDebugGame)
{
	bIsDebugGame = bInIsDebugGame;
}
#endif

FString FApp::GetBuildDate()
{
	return FString(BuildSettings::GetBuildDate());
}

FString FApp::GetGraphicsRHI()
{
	return GraphicsRHI;
}

void FApp::SetGraphicsRHI(FString RHIString)
{
	GraphicsRHI = RHIString;
}

void FApp::InitializeSession()
{
	// parse session details on command line
	FString InstanceIdString;
	FGuid InstanceId = GetInstanceId();

	if (FParse::Value(FCommandLine::Get(), TEXT("-InstanceId="), InstanceIdString))
	{
		if (!FGuid::Parse(InstanceIdString, InstanceId))
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
		}
	}

	FString SessionIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-SessionId="), SessionIdString))
	{
		if (FGuid::Parse(SessionIdString, SessionId))
		{
			Standalone = false;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid SessionId on command line: %s"), *SessionIdString);
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("-SessionName="), SessionName);

	if (!FParse::Value(FCommandLine::Get(), TEXT("-SessionOwner="), SessionOwner))
	{
		SessionOwner = FPlatformProcess::UserName(false);
	}
}


bool FApp::IsInstalled()
{
	static int32 InstalledState = -1;

	if (InstalledState == -1)
	{
#if UE_BUILD_SHIPPING && PLATFORM_DESKTOP && !UE_SERVER
		bool bIsInstalled = true;
#else
		bool bIsInstalled = false;
#endif

#if PLATFORM_DESKTOP
		FString InstalledProjectBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledProjectBuild.txt");
		FPaths::NormalizeFilename(InstalledProjectBuildFile);
		bIsInstalled |= IFileManager::Get().FileExists(*InstalledProjectBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalled)
		{
			bIsInstalled = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalled"));
		}
		else
		{
			bIsInstalled = FParse::Param(FCommandLine::Get(), TEXT("Installed"));
		}
		InstalledState = bIsInstalled ? 1 : 0;
	}
	return InstalledState == 1;
}


bool FApp::IsEngineInstalled()
{
	static int32 EngineInstalledState = -1;

	if (EngineInstalledState == -1)
	{
		bool bIsInstalledEngine = IsInstalled();

#if PLATFORM_DESKTOP
		FString InstalledBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledBuild.txt");
		FPaths::NormalizeFilename(InstalledBuildFile);
		bIsInstalledEngine |= IFileManager::Get().FileExists(*InstalledBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalledEngine)
		{
			bIsInstalledEngine = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalledEngine"));
		}
		else
		{
			bIsInstalledEngine = FParse::Param(FCommandLine::Get(), TEXT("InstalledEngine"));
		}
		EngineInstalledState = bIsInstalledEngine ? 1 : 0;
	}

	return EngineInstalledState == 1;
}

bool FApp::IsUnattended()
{
	// FCommandLine::Get() will assert that the command line has been set.
	// This function may not be used before FCommandLine::Set() is called.
	static bool bIsUnattended = FParse::Param(FCommandLine::Get(), TEXT("UNATTENDED"));
	return bIsUnattended || GIsAutomationTesting;
}

bool FApp::ShouldUseThreadingForPerformance()
{
	static bool OnlyOneThread = 
		FParse::Param(FCommandLine::Get(), TEXT("onethread")) ||
		FParse::Param(FCommandLine::Get(), TEXT("noperfthreads")) ||
		IsRunningDedicatedServer() ||
		!FPlatformProcess::SupportsMultithreading() ||
		FPlatformMisc::NumberOfCoresIncludingHyperthreads() == 1;

	// Enable at runtime for experimentation by passing "useperfthreads" as a command line arg.
	static bool bForceUsePerfThreads = FParse::Param(FCommandLine::Get(), TEXT("useperfthreads"));
	return !OnlyOneThread || bForceUsePerfThreads;
}

FTimecode FApp::GetTimecode()
{
	if (CurrentFrameTime.IsSet())
	{
		return FTimecode::FromFrameNumber(CurrentFrameTime->Time.GetFrame(), CurrentFrameTime->Rate);
	}
	return FTimecode();
}

FFrameRate FApp::GetTimecodeFrameRate()
{
	if (CurrentFrameTime.IsSet())
	{
		return CurrentFrameTime->Rate;
	}
	return FFrameRate();
}

static bool GUnfocusedVolumeMultiplierInitialised = false;
float FApp::GetUnfocusedVolumeMultiplier()
{
	if (!GUnfocusedVolumeMultiplierInitialised)
	{
		GUnfocusedVolumeMultiplierInitialised = true;
		GConfig->GetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	}
	return UnfocusedVolumeMultiplier;
}

void FApp::SetUnfocusedVolumeMultiplier(float InVolumeMultiplier)
{
	UnfocusedVolumeMultiplier = InVolumeMultiplier;
	GConfig->SetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	GUnfocusedVolumeMultiplierInitialised = true;
}

void FApp::SetUseVRFocus(bool bInUseVRFocus)
{
	UE_CLOG(bUseVRFocus != bInUseVRFocus, LogApp, Verbose, TEXT("UseVRFocus has changed to %d"), int(bInUseVRFocus));
	bUseVRFocus = bInUseVRFocus;
}

void FApp::SetHasVRFocus(bool bInHasVRFocus)
{
	UE_CLOG(bHasVRFocus != bInHasVRFocus, LogApp, Verbose, TEXT("HasVRFocus has changed to %d"), int(bInHasVRFocus));
	bHasVRFocus = bInHasVRFocus;
}

void FApp::SetHasFocusFunction(bool (*InHasFocusFunction)())
{
	HasFocusFunction = InHasFocusFunction;
}

bool FApp::HasFocus()
{
	if (FApp::IsBenchmarking())
	{
		return true;
	}

	if (FApp::UseVRFocus())
	{
		return FApp::HasVRFocus();
	}

	// by default we assume we have focus, it's a worse thing to encounter a bug where focus is locked off, vs. locked on
	bool bHasFocus = true;

	// desktop platforms are more or less why we have this abstraction, to dip into ApplicationCore's Platform implementation
#if PLATFORM_DESKTOP
	check(HasFocusFunction);
#endif

	// call the HasFocusFunction, if we have one. otherwise fall back to the default
	return HasFocusFunction ? HasFocusFunction() : bHasFocus;
}

void FApp::PrintStartupLogMessages()
{
	UE_LOG(LogInit, Log, TEXT("ExecutableName: %s"), FPlatformProcess::ExecutableName(false));
	UE_LOG(LogInit, Log, TEXT("Build: %s"), FApp::GetBuildVersion());

	UE_LOG(LogInit, Log, TEXT("Platform=%s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	UE_LOG(LogInit, Log, TEXT("MachineId=%s"), *FPlatformMisc::GetLoginId());
	UE_LOG(LogInit, Log, TEXT("DeviceId=%s"), *FPlatformMisc::GetDeviceId());

	UE_LOG(LogInit, Log, TEXT("Engine Version: %s"), *FEngineVersion::Current().ToString());
	UE_LOG(LogInit, Log, TEXT("Compatible Engine Version: %s"), *FEngineVersion::CompatibleWith().ToString());
	UE_LOG(LogInit, Log, TEXT("Net CL: %u"), FNetworkVersion::GetNetworkCompatibleChangelist());
	FString OSLabel, OSVersion;
	FPlatformMisc::GetOSVersions(OSLabel, OSVersion);
	UE_LOG(LogInit, Log, TEXT("OS: %s (%s), CPU: %s, GPU: %s"), *OSLabel, *OSVersion, *FPlatformMisc::GetCPUBrand(), *FPlatformMisc::GetPrimaryGPUBrand());

#if PLATFORM_64BITS
	UE_LOG(LogInit, Log, TEXT("Compiled (64-bit): %s %s"), BuildSettings::GetBuildDate(), BuildSettings::GetBuildTime());
#else
	UE_LOG(LogInit, Log, TEXT("Compiled (32-bit): %s %s"), BuildSettings::GetBuildDate(), BuildSettings::GetBuildTime());
#endif

#if PLATFORM_CPU_ARM_FAMILY
	UE_LOG(LogInit, Log, TEXT("Architecture: arm64"));
#elif PLATFORM_CPU_X86_FAMILY
	UE_LOG(LogInit, Log, TEXT("Architecture: x64"));
#else
#error No architecture defined!
#endif // x64/arm

	// Print compiler version info
#if defined(__clang__)
	UE_LOG(LogInit, Log, TEXT("Compiled with Clang: %s"), ANSI_TO_TCHAR(__clang_version__));
#elif defined(__INTEL_COMPILER)
	UE_LOG(LogInit, Log, TEXT("Compiled with ICL: %d"), __INTEL_COMPILER);
#elif defined( _MSC_VER )
#ifndef __INTELLISENSE__	// Intellisense compiler doesn't support _MSC_FULL_VER
	{
		const FString VisualCPPVersion(FString::Printf(TEXT("%d"), _MSC_FULL_VER));
		const FString VisualCPPRevisionNumber(FString::Printf(TEXT("%02d"), _MSC_BUILD));
		UE_LOG(LogInit, Log, TEXT("Compiled with Visual C++: %s.%s.%s.%s"),
			*VisualCPPVersion.Mid(0, 2), // Major version
			*VisualCPPVersion.Mid(2, 2), // Minor version
			*VisualCPPVersion.Mid(4),	// Build version
			*VisualCPPRevisionNumber	// Revision number
		);
	}
#endif
#else
	UE_LOG(LogInit, Log, TEXT("Compiled with unrecognized C++ compiler"));
#endif

	UE_LOG(LogInit, Log, TEXT("Build Configuration: %s"), LexToString(FApp::GetBuildConfiguration()));
	UE_LOG(LogInit, Log, TEXT("Branch Name: %s"), *FApp::GetBranchName());
	FString FilteredString = FCommandLine::IsCommandLineLoggingFiltered() ? TEXT("Filtered ") : TEXT("");
	UE_LOG(LogInit, Log, TEXT("%sCommand Line: %s"), *FilteredString, FCommandLine::GetForLogging());
	UE_LOG(LogInit, Log, TEXT("Base Directory: %s"), FPlatformProcess::BaseDir());
	//UE_LOG(LogInit, Log, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );
	UE_LOG(LogInit, Log, TEXT("Allocator: %s"), GMalloc->GetDescriptiveName());
	UE_LOG(LogInit, Log, TEXT("Installed Engine Build: %d"), FApp::IsEngineInstalled() ? 1 : 0);
	UE_LOG(LogInit, Log, TEXT("This binary is optimized with LTO: %s, PGO: %s, instrumented for PGO data collection: %s"),
		PLATFORM_COMPILER_OPTIMIZATION_LTCG ? TEXT("yes") : TEXT("no"),
		FPlatformMisc::IsPGOEnabled() ? TEXT("yes") : TEXT("no"),
		FPlatformMisc::IsPGICapableBinary() ? TEXT("yes") : TEXT("no")
	);

	FDevVersionRegistration::DumpVersionsToLog();
}

FString FApp::GetZenStoreProjectId()
{
	FString ProjectId;
	if (FParse::Value(FCommandLine::Get(), TEXT("-ZenStoreProject="), ProjectId))
	{
		return ProjectId;
	}

#if PLATFORM_DESKTOP
	if (FPaths::IsProjectFilePathSet())
	{
		FString ProjectFilePath = FPaths::GetProjectFilePath();
		FString AbsProjectFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ProjectFilePath);
		AbsProjectFilePath = FPaths::FindCorrectCase(AbsProjectFilePath);
		FTCHARToUTF8 AbsProjectFilePathUTF8(*AbsProjectFilePath);

		FString HashString = FMD5::HashBytes((unsigned char*)AbsProjectFilePathUTF8.Get(), AbsProjectFilePathUTF8.Length()).Left(8);
		return FString::Printf(TEXT("%s.%.8s"), FApp::GetProjectName(), *HashString);
	}
	UE_LOG(LogInit, Fatal, TEXT("GetZenStoreProjectId() called before having a valid project file path"));
#else
	UE_LOG(LogInit, Fatal, TEXT("-ZenStoreProject command line argument is required to run from Zen"));
#endif
	return FString();
}


FGuid FApp::GetInstanceId()
{
	static FGuid InstanceId = FGuid::NewGuid();
	return InstanceId;
}

const FCbObjectId& FApp::GetSessionObjectId()
{
	static const FCbObjectId SessionObjectId = FCbObjectId::NewObjectId();
	return SessionObjectId;
}
