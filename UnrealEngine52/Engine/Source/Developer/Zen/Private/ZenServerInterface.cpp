// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/ZenServerInterface.h"

#include "ZenBackendUtils.h"
#include "ZenSerialization.h"
#include "ZenServerHttp.h"

#include "AnalyticsEventAttribute.h"
#include "Async/Async.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryValidation.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "String/LexFromString.h"
#include "ZenVersion.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include <shellapi.h>
#	include <synchapi.h>
#	include <Windows.h>
#	include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_UNIX || PLATFORM_MAC
#	include <sys/file.h>
#	include <sys/sem.h>
#endif

#define ALLOW_SETTINGS_OVERRIDE_FROM_COMMANDLINE			(UE_SERVER || !(UE_BUILD_SHIPPING))

namespace UE::Zen
{

DEFINE_LOG_CATEGORY_STATIC(LogZenServiceInstance, Log, All);

static FZenVersion
GetZenVersion(const FString& UtilityPath, const FString& ServicePath, const FString& VersionCachePath)
{
	IFileManager& FileManager = IFileManager::Get();
	FDateTime UtilityExecutableModificationTime = FileManager.GetTimeStamp(*UtilityPath);
	FDateTime ServiceExecutableModificationTime = FileManager.GetTimeStamp(*ServicePath);

	auto GetFallbackVersion = [&UtilityExecutableModificationTime, &ServiceExecutableModificationTime]()
	{
		FZenVersion FallbackVersion;
		if (UtilityExecutableModificationTime > ServiceExecutableModificationTime)
		{
			FallbackVersion.Details = UtilityExecutableModificationTime.ToString();
			return FallbackVersion;
		}
		FallbackVersion.Details = ServiceExecutableModificationTime.ToString();
		return FallbackVersion;
	};

	FDateTime VersionCacheModificationTime = FileManager.GetTimeStamp(*VersionCachePath);
	FString VersionFileContents;
	FZenVersion ComparableVersion;
	if ((VersionCacheModificationTime < UtilityExecutableModificationTime) ||
		(VersionCacheModificationTime < ServiceExecutableModificationTime) ||
		!FFileHelper::LoadFileToString(VersionFileContents, *VersionCachePath) ||
		!ComparableVersion.TryParse(*VersionFileContents))
	{
		FString AbsoluteUtilityPath = FPaths::ConvertRelativePathToFull(UtilityPath);
		FMonitoredProcess MonitoredUtilityProcess(AbsoluteUtilityPath, TEXT("version --detailed"), FPaths::GetPath(UtilityPath), true);

		bool bLaunched = MonitoredUtilityProcess.Launch();
		checkf(bLaunched, TEXT("Failed to launch zen utility to gather version data: '%s'."), *UtilityPath);
		if (!bLaunched)
		{
			return GetFallbackVersion();
		}

		while (MonitoredUtilityProcess.Update())
		{
			FPlatformProcess::Sleep(0.1f);
			if (MonitoredUtilityProcess.GetDuration().GetTotalSeconds() > 10)
			{
				MonitoredUtilityProcess.Cancel(true);
				checkf(false, TEXT("Cancelled launch of zen utility for gathering version data: '%s'."), *UtilityPath);
				return GetFallbackVersion();
			}
		}

		FString OutputString = MonitoredUtilityProcess.GetFullOutputWithoutDelegate();
		if (MonitoredUtilityProcess.GetReturnCode() != 0)
		{
			checkf(false, TEXT("Unexpected return code after launch of zen utility for gathering version data: '%s' (%d). Output: '%s'"), *UtilityPath, MonitoredUtilityProcess.GetReturnCode(), *OutputString);
			return GetFallbackVersion();
		}

		FString VersionOutputString = OutputString.TrimStartAndEnd();
		if (!ComparableVersion.TryParse(*VersionOutputString))
		{
			checkf(false, TEXT("Invalid version information after launch of zen utility for gathering version data: '%s' (`%s`)"), *UtilityPath, *VersionOutputString);
			return GetFallbackVersion();
		}

		FFileHelper::SaveStringToFile(ComparableVersion.ToString(), *VersionCachePath);
	}

	return ComparableVersion;
}

static bool
IsInstallVersionOutOfDate(const FString& InTreeUtilityPath, const FString& InstallUtilityPath, const FString& InTreeServicePath, const FString& InstallServicePath, FString& OutInTreeVersionCache, FString& OutInstallVersionCache)
{
	OutInTreeVersionCache = FPaths::Combine(FPaths::EngineSavedDir(), TEXT("Zen") TEXT("zen.version"));
	OutInstallVersionCache = FPaths::SetExtension(InstallUtilityPath, TEXT("version"));

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*InstallUtilityPath) || !FileManager.FileExists(*InstallServicePath))
	{
		return true;
	}

	FZenVersion InTreeVersion = GetZenVersion(InTreeUtilityPath, InTreeServicePath, OutInTreeVersionCache);
	FZenVersion InstallVersion = GetZenVersion(InstallUtilityPath, InstallServicePath, OutInstallVersionCache);

	return InstallVersion < InTreeVersion;
}

static bool
AttemptFileCopyWithRetries(const TCHAR* Dst, const TCHAR* Src, double RetryDurationSeconds)
{
	uint32 CopyResult = IFileManager::Get().Copy(Dst, Src, true, true, false);
	uint64 CopyWaitStartTime = FPlatformTime::Cycles64();
	while (CopyResult != COPY_OK)
	{
		double CopyWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - CopyWaitStartTime);
		if (CopyWaitDuration < RetryDurationSeconds)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		else
		{
			break;
		}
		CopyResult = IFileManager::Get().Copy(Dst, Src, true, true, false);
	}

	return CopyResult == COPY_OK;
}

static void
DetermineLocalDataCachePath(const TCHAR* ConfigSection, FString& DataPath)
{
	FString DataPathEnvOverride;
	if (GConfig->GetString(ConfigSection, TEXT("LocalDataCachePathEnvOverride"), DataPathEnvOverride, GEngineIni))
	{
		FString DataPathEnvOverrideValue = FPlatformMisc::GetEnvironmentVariable(*DataPathEnvOverride);
		if (!DataPathEnvOverrideValue.IsEmpty())
		{
			DataPath = DataPathEnvOverrideValue;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found environment variable %s=%s"), *DataPathEnvOverride, *DataPathEnvOverrideValue);
		}

		if (FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("GlobalDataCachePath"), *DataPathEnvOverride, DataPathEnvOverrideValue))
		{
			if (!DataPathEnvOverrideValue.IsEmpty())
			{
				DataPath = DataPathEnvOverrideValue;
				UE_LOG(LogZenServiceInstance, Log, TEXT("Found registry key GlobalDataCachePath %s=%s"), *DataPathEnvOverride, *DataPath);
			}
		}
	}

	FString DataPathCommandLineOverride;
	if (GConfig->GetString(ConfigSection, TEXT("LocalDataCachePathCommandLineOverride"), DataPathCommandLineOverride, GEngineIni))
	{
		FString DataPathCommandLineOverrideValue;
		if (FParse::Value(FCommandLine::Get(), *(DataPathCommandLineOverride + TEXT("=")), DataPathCommandLineOverrideValue))
		{
			DataPath = DataPathCommandLineOverrideValue;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found command line override %s=%s"), *DataPathCommandLineOverride, *DataPath);
		}
	}

	// Paths starting with a '?' are looked up from config
	if (DataPath.StartsWith(TEXT("?")) && !GConfig->GetString(TEXT("DerivedDataCacheSettings"), *DataPath + 1, DataPath, GEngineIni))
	{
		DataPath.Empty();
	}

	FString DataPathEditorOverrideSetting;
	if (GConfig->GetString(ConfigSection, TEXT("LocalDataCachePathEditorOverrideSetting"), DataPathEditorOverrideSetting, GEngineIni))
	{
		FString Setting = GConfig->GetStr(TEXT("/Script/UnrealEd.EditorSettings"), *DataPathEditorOverrideSetting, GEditorSettingsIni);
		if (!Setting.IsEmpty())
		{
			FString SettingPath;
			if (FParse::Value(*Setting, TEXT("Path="), SettingPath))
			{
				SettingPath.TrimQuotesInline();
				SettingPath.ReplaceEscapedCharWithCharInline();
				if (!SettingPath.IsEmpty())
				{
					DataPath = SettingPath;
					UE_LOG(LogZenServiceInstance, Log, TEXT("Found editor setting /Script/UnrealEd.EditorSettings.Path=%s"), *DataPath);
				}
			}
		}
	}
}

static void
DetermineDataPath(const TCHAR* ConfigSection, FString& DataPath)
{
	auto NormalizeDataPath = [](const FString& InDataPath)
	{
		FString FinalPath = FPaths::ConvertRelativePathToFull(InDataPath);
		FPaths::NormalizeDirectoryName(FinalPath);
		return FinalPath;
	};

	// Zen commandline
	FString CommandLineOverrideValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZenDataPath="), CommandLineOverrideValue))
	{
		DataPath = NormalizeDataPath(CommandLineOverrideValue);
		UE_LOG(LogZenServiceInstance, Log, TEXT("Found command line override ZenDataPath=%s"), *CommandLineOverrideValue);
		return;
	}

	// Zen subprocess environment
	FString SubprocessDataPathEnvOverrideValue;
	SubprocessDataPathEnvOverrideValue = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-ZenSubprocessDataPath"));
	if (!SubprocessDataPathEnvOverrideValue.IsEmpty())
	{
		DataPath = NormalizeDataPath(SubprocessDataPathEnvOverrideValue);
		UE_LOG(LogZenServiceInstance, Log, TEXT("Found subprocess environment variable UE-ZenSubprocessDataPath=%s"), *SubprocessDataPathEnvOverrideValue);
		return;
	}

	// Zen registry/stored
	FString DataPathEnvOverrideValue;
	if (FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("Zen"), TEXT("DataPath"), DataPathEnvOverrideValue))
	{
		if (!DataPathEnvOverrideValue.IsEmpty())
		{
			DataPath = NormalizeDataPath(DataPathEnvOverrideValue);
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found registry key Zen DataPath=%s"), *DataPath);
			return;
		}
	}

	// Zen environment
	DataPathEnvOverrideValue = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-ZenDataPath"));
	if (!DataPathEnvOverrideValue.IsEmpty())
	{
		DataPath = NormalizeDataPath(DataPathEnvOverrideValue);
		UE_LOG(LogZenServiceInstance, Log, TEXT("Found environment variable UE-ZenDataPath=%s"), *DataPathEnvOverrideValue);
		return;
	}

	// Follow local DDC (if outside workspace)
	FString LocalDataCachePath;
	DetermineLocalDataCachePath(ConfigSection, LocalDataCachePath);
	if (!LocalDataCachePath.IsEmpty() && (LocalDataCachePath != TEXT("None")) && !FPaths::IsUnderDirectory(LocalDataCachePath, FPaths::RootDir()))
	{
		DataPath = NormalizeDataPath(FPaths::Combine(LocalDataCachePath, TEXT("Zen")));
		return;
	}

	// Zen config default
	GConfig->GetString(ConfigSection, TEXT("DataPath"), DataPath, GEngineIni);
	DataPath = NormalizeDataPath(DataPath);

	check(!DataPath.IsEmpty())
}

static void
ReadUInt16FromConfig(const TCHAR* Section, const TCHAR* Key, uint16& Value, const FString& ConfigFile)
{
	int32 ValueInt32 = Value;
	GConfig->GetInt(Section, Key, ValueInt32, ConfigFile);
	Value = (uint16)ValueInt32;
}

static bool
IsLocalHost(const FString& Host)
{
	if (Host.Compare(FString(TEXT("localhost")), ESearchCase::IgnoreCase) == 0)
	{
		return true;
	}

	if (Host.Compare(FString(TEXT("127.0.0.1"))) == 0)
	{
		return true;
	}

	if (Host.Compare(FString(TEXT("[::1]"))) == 0)
	{
		return true;
	}

	return false;
}

void
FServiceSettings::ReadFromConfig()
{
	check(GConfig && GConfig->IsReadyForUse());
	const TCHAR* ConfigSection = TEXT("Zen");
	bool bAutoLaunch = true;
	GConfig->GetBool(ConfigSection, TEXT("AutoLaunch"), bAutoLaunch, GEngineIni);

	if (bAutoLaunch)
	{
		if (!TryApplyAutoLaunchOverride())
		{
			// AutoLaunch settings
			const TCHAR* AutoLaunchConfigSection = TEXT("Zen.AutoLaunch");
			SettingsVariant.Emplace<FServiceAutoLaunchSettings>();
			FServiceAutoLaunchSettings& AutoLaunchSettings = SettingsVariant.Get<FServiceAutoLaunchSettings>();

			DetermineDataPath(AutoLaunchConfigSection, AutoLaunchSettings.DataPath);
			GConfig->GetString(AutoLaunchConfigSection, TEXT("ExtraArgs"), AutoLaunchSettings.ExtraArgs, GEngineIni);

			ReadUInt16FromConfig(AutoLaunchConfigSection, TEXT("DesiredPort"), AutoLaunchSettings.DesiredPort, GEngineIni);
			GConfig->GetBool(AutoLaunchConfigSection, TEXT("ShowConsole"), AutoLaunchSettings.bShowConsole, GEngineIni);
			GConfig->GetBool(AutoLaunchConfigSection, TEXT("LimitProcessLifetime"), AutoLaunchSettings.bLimitProcessLifetime, GEngineIni);
		}
	}
	else
	{
		// ConnectExisting settings
		const TCHAR* ConnectExistingConfigSection = TEXT("Zen.ConnectExisting");
		SettingsVariant.Emplace<FServiceConnectSettings>();
		FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();

		GConfig->GetString(ConnectExistingConfigSection, TEXT("HostName"), ConnectExistingSettings.HostName, GEngineIni);
		ReadUInt16FromConfig(ConnectExistingConfigSection, TEXT("Port"), ConnectExistingSettings.Port, GEngineIni);
	}
}

void
FServiceSettings::ReadFromJson(FJsonObject& JsonObject)
{
	if (TSharedPtr<FJsonValue> bAutoLaunchValue = JsonObject.Values.FindRef(TEXT("bAutoLaunch")))
	{
		if (bAutoLaunchValue->AsBool())
		{
			if (!TryApplyAutoLaunchOverride())
			{
				SettingsVariant.Emplace<FServiceAutoLaunchSettings>();
				FServiceAutoLaunchSettings& AutoLaunchSettings = SettingsVariant.Get<FServiceAutoLaunchSettings>();

				TSharedPtr<FJsonValue> AutoLaunchSettingsValue = JsonObject.Values.FindRef(TEXT("AutoLaunchSettings"));
				if (AutoLaunchSettingsValue)
				{
					TSharedPtr<FJsonObject> AutoLaunchSettingsObject = AutoLaunchSettingsValue->AsObject();
					AutoLaunchSettings.DataPath = AutoLaunchSettingsObject->Values.FindRef(TEXT("DataPath"))->AsString();
					AutoLaunchSettings.ExtraArgs = AutoLaunchSettingsObject->Values.FindRef(TEXT("ExtraArgs"))->AsString();
					AutoLaunchSettingsObject->Values.FindRef(TEXT("DesiredPort"))->TryGetNumber(AutoLaunchSettings.DesiredPort);
					AutoLaunchSettingsObject->Values.FindRef(TEXT("ShowConsole"))->TryGetBool(AutoLaunchSettings.bShowConsole);
					AutoLaunchSettingsObject->Values.FindRef(TEXT("LimitProcessLifetime"))->TryGetBool(AutoLaunchSettings.bLimitProcessLifetime);
				}
			}
		}
		else
		{
			SettingsVariant.Emplace<FServiceConnectSettings>();
			FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();

			TSharedPtr<FJsonValue> ConnectExistingSettingsValue = JsonObject.Values.FindRef(TEXT("ConnectExistingSettings"));
			if (ConnectExistingSettingsValue)
			{
				TSharedPtr<FJsonObject> ConnectExistingSettingsObject = ConnectExistingSettingsValue->AsObject();
				ConnectExistingSettings.HostName = ConnectExistingSettingsObject->Values.FindRef(TEXT("HostName"))->AsString();
				ConnectExistingSettingsObject->Values.FindRef(TEXT("Port"))->TryGetNumber(ConnectExistingSettings.Port);
			}
		}

	}
}

void
FServiceSettings::ReadFromURL(FStringView InstanceURL)
{
	SettingsVariant.Emplace<FServiceConnectSettings>();
	FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();

	if (InstanceURL.StartsWith(TEXT("http://")))
	{
		InstanceURL.RightChopInline(7);
	}

	int32 PortDelimIndex = INDEX_NONE;
	InstanceURL.FindLastChar(TEXT(':'), PortDelimIndex);
	if (PortDelimIndex != INDEX_NONE)
	{
		ConnectExistingSettings.HostName = InstanceURL.Left(PortDelimIndex);
		LexFromString(ConnectExistingSettings.Port, InstanceURL.RightChop(PortDelimIndex + 1));
	}
	else
	{
		ConnectExistingSettings.HostName = InstanceURL;
		ConnectExistingSettings.Port = 1337;
	}
}

void
FServiceSettings::WriteToJson(TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>& Writer) const
{
	bool bAutoLaunch = IsAutoLaunch();
	Writer.WriteValue(TEXT("bAutoLaunch"), bAutoLaunch);
	if (bAutoLaunch)
	{
		const FServiceAutoLaunchSettings& AutoLaunchSettings = SettingsVariant.Get<FServiceAutoLaunchSettings>();
		Writer.WriteObjectStart(TEXT("AutoLaunchSettings"));
		Writer.WriteValue(TEXT("DataPath"), AutoLaunchSettings.DataPath);
		Writer.WriteValue(TEXT("ExtraArgs"), AutoLaunchSettings.ExtraArgs);
		Writer.WriteValue(TEXT("DesiredPort"), AutoLaunchSettings.DesiredPort);
		Writer.WriteValue(TEXT("ShowConsole"), AutoLaunchSettings.bShowConsole);
		Writer.WriteValue(TEXT("LimitProcessLifetime"), AutoLaunchSettings.bLimitProcessLifetime);
		Writer.WriteObjectEnd();
	}
	else
	{
		const FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();
		Writer.WriteObjectStart(TEXT("ConnectExistingSettings"));
		Writer.WriteValue(TEXT("HostName"), ConnectExistingSettings.HostName);
		Writer.WriteValue(TEXT("Port"), ConnectExistingSettings.Port);
		Writer.WriteObjectEnd();
	}
}

bool
FServiceSettings::TryApplyAutoLaunchOverride()
{
#if ALLOW_SETTINGS_OVERRIDE_FROM_COMMANDLINE
	if (FParse::Param(FCommandLine::Get(), TEXT("NoZenAutoLaunch")))
	{
		SettingsVariant.Emplace<FServiceConnectSettings>();
		FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();
		ConnectExistingSettings.HostName = TEXT("[::1]");
		ConnectExistingSettings.Port = 1337;
		return true;
	}

	FString Host;
	if  (FParse::Value(FCommandLine::Get(), TEXT("-NoZenAutoLaunch="), Host))
	{
		SettingsVariant.Emplace<FServiceConnectSettings>();
		FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();

		int32 PortDelimIndex = INDEX_NONE;
		if (Host.FindChar(TEXT(':'), PortDelimIndex))
		{
			ConnectExistingSettings.HostName = Host.Left(PortDelimIndex);
			LexFromString(ConnectExistingSettings.Port, Host.RightChop(PortDelimIndex + 1));
		}
		else
		{
			ConnectExistingSettings.HostName = Host;
			ConnectExistingSettings.Port = 1337;
		}

		return true;
	}
#endif
	return false;
}

#if UE_WITH_ZEN

uint16 FZenServiceInstance::AutoLaunchedPort = 0;

static bool
ReadCbLockFile(FStringView FileName, FCbObject& OutLockObject)
{
#if PLATFORM_WINDOWS
	// Windows specific lock reading path
	// Uses share flags that are unique to windows to allow us to read file contents while the file may be open for write AND delete by another process (zenserver).

	uint32 Access = GENERIC_READ;
	uint32 WinFlags = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	uint32 Create = OPEN_EXISTING;

	TStringBuilder<MAX_PATH> FullFileNameBuilder;
	FPathViews::ToAbsolutePath(FileName, FullFileNameBuilder);
	for (TCHAR& Char : MakeArrayView(FullFileNameBuilder))
	{
		if (Char == TEXT('/'))
		{
			Char = TEXT('\\');
		}
	}
	if (FullFileNameBuilder.Len() >= MAX_PATH)
	{
		FullFileNameBuilder.Prepend(TEXTVIEW("\\\\?\\"));
	}
	HANDLE Handle = CreateFileW(FullFileNameBuilder.ToString(), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL);
	if (Handle != INVALID_HANDLE_VALUE)
	{
		ON_SCOPE_EXIT { CloseHandle(Handle); };
		LARGE_INTEGER LI;
		if (GetFileSizeEx(Handle, &LI))
		{
			checkf(LI.QuadPart == LI.u.LowPart, TEXT("Lock file exceeds supported 2GB limit."));
			int32 FileSize32 = LI.u.LowPart;
			FUniqueBuffer FileBytes = FUniqueBuffer::Alloc(FileSize32);
			DWORD ReadBytes = 0;
			if (ReadFile(Handle, FileBytes.GetData(), FileSize32, &ReadBytes, NULL) && (ReadBytes == FileSize32))
			{
				if (ValidateCompactBinary(FileBytes, ECbValidateMode::Default) == ECbValidateError::None)
				{
					OutLockObject = FCbObject(FileBytes.MoveToShared());
					return true;
				}
			}
		}
	}
	return false;
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<256> LockFilePath;
	LockFilePath << FileName;
	int32 Fd = open(LockFilePath.ToString(), O_RDONLY);
	if (Fd < 0)
	{
		return false;
	}

	// If we can claim the lock then it's an orphaned lock file and should be
	// ignored. Not ideal as there's a period of time when the lock can be
	// held unncessarily.
	int32 LockRet = flock(Fd, LOCK_EX | LOCK_NB);
	if (LockRet >= 0)
	{
		unlink(LockFilePath.ToString());
		flock(Fd, LOCK_UN);
		close(Fd);
		return false;
	}

	if (errno != EWOULDBLOCK && errno != EAGAIN)
	{
		return false;
	}

	struct stat Stat;
	fstat(Fd, &Stat);
	uint64 FileSize = uint64(Stat.st_size);

	bool bSuccess = false;
	FUniqueBuffer FileBytes = FUniqueBuffer::Alloc(FileSize);
	if (read(Fd, FileBytes.GetData(), FileSize) == FileSize)
	{
		if (ValidateCompactBinary(FileBytes, ECbValidateMode::Default) == ECbValidateError::None)
		{
			OutLockObject = FCbObject(FileBytes.MoveToShared());
			bSuccess = true;
		}
	}

	close(Fd);
	return bSuccess;
#endif
}

static bool
IsLockFileLocked(const TCHAR* FileName, bool bAttemptCleanUp=false)
{
#if PLATFORM_WINDOWS
	if (bAttemptCleanUp)
	{
		IFileManager::Get().Delete(FileName, false, false, true);
	}
	return IFileManager::Get().FileExists(FileName);
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<256> LockFilePath;
	LockFilePath << FileName;
	int32 Fd = open(LockFilePath.ToString(), O_RDONLY);
	if (Fd < 0)
	{
		return false;
	}

	int32 LockRet = flock(Fd, LOCK_EX | LOCK_NB);
	if (LockRet < 0)
	{
		close(Fd);
		return errno == EWOULDBLOCK || errno == EAGAIN;
	}

	// Consider the lock file as orphaned if we we managed to claim the lock for
	// it. Might as well delete it while we own it.
	unlink(LockFilePath.ToString());

	flock(Fd, LOCK_UN);
	close(Fd);

	return true;
#endif
}

static void
RequestZenShutdownOnPort(uint16 Port)
{
#if PLATFORM_WINDOWS
	HANDLE Handle = OpenEventW(EVENT_MODIFY_STATE, false, *WriteToWideString<64>(WIDETEXT("Zen_"), Port, WIDETEXT("_Shutdown")));
	if (Handle != INVALID_HANDLE_VALUE)
	{
		ON_SCOPE_EXIT{ CloseHandle(Handle); };
		SetEvent(Handle);
	}
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<64> EventPath;
	EventPath << "/tmp/Zen_" << Port << "_Shutdown";

	key_t IpcKey = ftok(EventPath.ToString(), 1);
	if (IpcKey < 0)
	{
		return;
	}

	int Semaphore = semget(IpcKey, 1, 0600);
	if (Semaphore < 0)
	{
		return;
	}

	semctl(Semaphore, 0, SETVAL, 0);
	semctl(Semaphore, 0, IPC_RMID);
#else
	static_assert(false, "Missing implementation for Zen named shutdown events");
#endif
}

static bool
WaitForZenShutdown(const TCHAR* LockFilePath, double MaximumWaitDurationSeconds)
{
	uint64 ZenShutdownWaitStartTime = FPlatformTime::Cycles64();
	while (IsLockFileLocked(LockFilePath))
	{
		double ZenShutdownWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - ZenShutdownWaitStartTime);
		if (ZenShutdownWaitDuration < MaximumWaitDurationSeconds)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		else
		{
			return false;
		}
	}
	return true;
}

static bool IsProcessActive(const TCHAR* ExecutablePath)
{
	FString NormalizedExecutablePath(ExecutablePath);
	FPaths::NormalizeFilename(NormalizedExecutablePath);
	FPlatformProcess::FProcEnumerator ProcIter;
	while (ProcIter.MoveNext())
	{
		FPlatformProcess::FProcEnumInfo ProcInfo = ProcIter.GetCurrent();
		FString Candidate = ProcInfo.GetFullPath();
		FPaths::NormalizeFilename(Candidate);
		if (Candidate == NormalizedExecutablePath)
		{
			return true;
		}
	}
	return false;
}

static FString
DetermineCmdLineWithoutTransientComponents(const FServiceAutoLaunchSettings& InSettings, int16 OverrideDesiredPort)
{
	FString PlatformDataPath(InSettings.DataPath);
	FPaths::MakePlatformFilename(PlatformDataPath);

	FString Parms;
	Parms.Appendf(TEXT("--port %d --data-dir \"%s\""),
		OverrideDesiredPort,
		*PlatformDataPath);

	if (!InSettings.ExtraArgs.IsEmpty())
	{
		Parms.AppendChar(TEXT(' '));
		Parms.Append(InSettings.ExtraArgs);
	}

	FString LogCommandLineOverrideValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZenLogPath="), LogCommandLineOverrideValue))
	{
		if (!LogCommandLineOverrideValue.IsEmpty())
		{
			Parms.Appendf(TEXT(" --abslog \"%s\""),
				*FPaths::ConvertRelativePathToFull(LogCommandLineOverrideValue));
		}
	}

	FString CfgCommandLineOverrideValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZenCfgPath="), CfgCommandLineOverrideValue))
	{
		if (!CfgCommandLineOverrideValue.IsEmpty())
		{
			Parms.Appendf(TEXT(" --config \"%s\""),
				*FPaths::ConvertRelativePathToFull(CfgCommandLineOverrideValue));
		}
	}

	return Parms;
}

bool
Private::IsLocalAutoLaunched(FStringView InstanceURL)
{
	if (!InstanceURL.IsEmpty() && !InstanceURL.Equals(TEXT("<DefaultInstance>")))
	{
		FString TempURL(InstanceURL);
		return IsLocalHost(TempURL);
	}
	return true;
}

bool
Private::GetLocalDataCachePathOverride(FString& OutDataPath)
{
	const TCHAR* AutoLaunchConfigSection = TEXT("Zen.AutoLaunch");
	FString DataPath;
	DetermineLocalDataCachePath(AutoLaunchConfigSection, DataPath);
	if (DataPath.IsEmpty())
	{
		return false;
	}
	OutDataPath = DataPath;
	return true;
}

bool
TryGetLocalServiceRunContext(FZenLocalServiceRunContext& OutContext)
{
	FString LocalServiceRunContextPath = FPaths::SetExtension(GetLocalServiceInstallPath(), TEXT(".runcontext"));
	return OutContext.ReadFromJsonFile(*LocalServiceRunContextPath);
}

bool
FZenLocalServiceRunContext::ReadFromJson(FJsonObject& JsonObject)
{
	Executable = JsonObject.Values.FindRef(TEXT("Executable"))->AsString();
	CommandlineArguments = JsonObject.Values.FindRef(TEXT("CommandlineArguments"))->AsString();
	WorkingDirectory = JsonObject.Values.FindRef(TEXT("WorkingDirectory"))->AsString();
	DataPath = JsonObject.Values.FindRef(TEXT("DataPath"))->AsString();
	bShowConsole = JsonObject.Values.FindRef(TEXT("ShowConsole"))->AsBool();
	return true;
}

void
FZenLocalServiceRunContext::WriteToJson(TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>& Writer) const
{
	Writer.WriteValue(TEXT("Executable"), Executable);
	Writer.WriteValue(TEXT("CommandlineArguments"), CommandlineArguments);
	Writer.WriteValue(TEXT("WorkingDirectory"), WorkingDirectory);
	Writer.WriteValue(TEXT("DataPath"), DataPath);
	Writer.WriteValue(TEXT("ShowConsole"), bShowConsole);
}

bool
FZenLocalServiceRunContext::ReadFromJsonFile(const TCHAR* Filename)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, Filename))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	return ReadFromJson(*JsonObject);
}

bool
FZenLocalServiceRunContext::WriteToJsonFile(const TCHAR* Filename) const
{
	FString JsonTcharText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonTcharText);
	Writer->WriteObjectStart();
	WriteToJson(*Writer);
	Writer->WriteObjectEnd();
	Writer->Close();

	if (!FFileHelper::SaveStringToFile(JsonTcharText, Filename))
	{
		return false;
	}

	return true;
}

bool
IsLocalServiceRunning(const TCHAR* DataPath, uint16* OutPort)
{
	// TODO: Instead of using the lock file, this could use the shared memory system state named "Global\ZenMap" (see zenserverprocess.{h,cpp} in zen codebase)
	const FString LockFilePath = FPaths::Combine(DataPath, TEXT(".lock"));
	if (IsLockFileLocked(*LockFilePath, true))
	{
		if (OutPort)
		{
			// If an instance is running with this data path, check if we can use it and what port it is on
			*OutPort = 0;
			FCbObject LockObject;
			if (ReadCbLockFile(LockFilePath, LockObject))
			{
				bool bIsReady = LockObject["ready"].AsBool();
				if (bIsReady)
				{
					*OutPort = LockObject["port"].AsUInt16();
				}
			}
		}

		return true;
	}
	return false;
}

FProcHandle
StartLocalService(const FZenLocalServiceRunContext& Context, const TCHAR* TransientArgs)
{
	FString Parms = Context.GetCommandlineArguments();
	if (TransientArgs)
	{
		Parms.Appendf(TEXT(" %s"), TransientArgs);
	}

	FProcHandle Proc;
#if PLATFORM_WINDOWS
	FString PlatformExecutable = Context.GetExecutable();
	FPaths::MakePlatformFilename(PlatformExecutable);
	FString PlatformWorkingDirectory = Context.GetWorkingDirectory();
	FPaths::MakePlatformFilename(PlatformWorkingDirectory);
	{
		// Attempt non-elevated launch
		STARTUPINFO StartupInfo = {
			sizeof(STARTUPINFO),
			NULL, NULL, NULL,
			(::DWORD)CW_USEDEFAULT,
			(::DWORD)CW_USEDEFAULT,
			(::DWORD)CW_USEDEFAULT,
			(::DWORD)CW_USEDEFAULT,
			(::DWORD)0, (::DWORD)0, (::DWORD)0,
			(::DWORD)STARTF_USESHOWWINDOW,
			(::WORD)(Context.GetShowConsole() ? SW_SHOWMINNOACTIVE : SW_HIDE),
			0, NULL,
			HANDLE(nullptr),
			HANDLE(nullptr),
			HANDLE(nullptr)
		};

		FString CommandLine = FString::Printf(TEXT("\"%s\" %s"), *PlatformExecutable, *Parms);
		PROCESS_INFORMATION ProcInfo;
		if (CreateProcess(NULL, CommandLine.GetCharArray().GetData(), nullptr, nullptr, false, (::DWORD)(NORMAL_PRIORITY_CLASS | DETACHED_PROCESS), nullptr, PlatformWorkingDirectory.GetCharArray().GetData(), &StartupInfo, &ProcInfo))
		{
			::CloseHandle(ProcInfo.hThread);
			Proc = FProcHandle(ProcInfo.hProcess);
		}

	}
	if (!Proc.IsValid())
	{
		// Fall back to elevated launch
		SHELLEXECUTEINFO ShellExecuteInfo;
		ZeroMemory(&ShellExecuteInfo, sizeof(ShellExecuteInfo));
		ShellExecuteInfo.cbSize = sizeof(ShellExecuteInfo);
		ShellExecuteInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOCLOSEPROCESS;
		ShellExecuteInfo.lpFile = *PlatformExecutable;
		ShellExecuteInfo.lpDirectory = *PlatformWorkingDirectory;
		ShellExecuteInfo.lpVerb = TEXT("runas");
		ShellExecuteInfo.nShow = Context.GetShowConsole() ? SW_SHOWMINNOACTIVE : SW_HIDE;
		ShellExecuteInfo.lpParameters = *Parms;

		if (ShellExecuteEx(&ShellExecuteInfo))
		{
			Proc = FProcHandle(ShellExecuteInfo.hProcess);
		}
	}
#else
	{
		bool bLaunchDetached = true;
		bool bLaunchHidden = true;
		bool bLaunchReallyHidden = !Context.GetShowConsole();
		uint32* OutProcessID = nullptr;
		int32 PriorityModifier = 0;
		void* PipeWriteChild = nullptr;
		void* PipeReadChild = nullptr;
		Proc = FPlatformProcess::CreateProc(
			*Context.GetExecutable(),
			*Parms,
			bLaunchDetached,
			bLaunchHidden,
			bLaunchReallyHidden,
			OutProcessID,
			PriorityModifier,
			*Context.GetWorkingDirectory(),
			PipeWriteChild,
			PipeReadChild);
	}
#endif
	return Proc;
}

bool
StopLocalService(const TCHAR* DataPath, double MaximumWaitDurationSeconds)
{
	uint16 CurrentPort = 0;
	if (IsLocalServiceRunning(DataPath, &CurrentPort))
	{
		if (CurrentPort == 0)
		{
			return false;
		}

		RequestZenShutdownOnPort(CurrentPort);
		return WaitForZenShutdown(*FPaths::Combine(DataPath, TEXT(".lock")), MaximumWaitDurationSeconds);
	}
	return true;
}

FString
GetLocalServiceInstallPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPlatformProcess::ApplicationSettingsDir(), TEXT("Zen\\Install"),
#if PLATFORM_WINDOWS
		TEXT("zenserver.exe")
#else
		TEXT("zenserver")
#endif
		));
}

FString
GetLocalServiceInstallVersion(bool bDetailed)
{
	FString InstallUtilityPath = GetLocalInstallUtilityPath();
	FString InstallVersionCache = FPaths::SetExtension(InstallUtilityPath, TEXT("version"));

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*InstallUtilityPath))
	{
		return FZenVersion().ToString(bDetailed);
	}

	FZenVersion InstallVersion = GetZenVersion(InstallUtilityPath, GetLocalServiceInstallPath(), InstallVersionCache);

	return InstallVersion.ToString(bDetailed);
}

FString
GetLocalInstallUtilityPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPlatformProcess::ApplicationSettingsDir(), TEXT("Zen\\Install"),
#if PLATFORM_WINDOWS
		TEXT("zen.exe")
#else
		TEXT("zen")
#endif
		));
}

static bool GIsDefaultServicePresent = false;

FZenServiceInstance& GetDefaultServiceInstance()
{
	static FZenServiceInstance DefaultServiceInstance;
	GIsDefaultServicePresent = true;
	return DefaultServiceInstance;
}

bool IsDefaultServicePresent()
{
	return GIsDefaultServicePresent;
}

FScopeZenService::FScopeZenService()
	: FScopeZenService(FStringView())
{
}

FScopeZenService::FScopeZenService(FStringView InstanceURL)
{
	if (!InstanceURL.IsEmpty() && !InstanceURL.Equals(TEXT("<DefaultInstance>")))
	{
		UniqueNonDefaultInstance = MakeUnique<FZenServiceInstance>(InstanceURL);
		ServiceInstance = UniqueNonDefaultInstance.Get();
	}
	else
	{
		ServiceInstance = &GetDefaultServiceInstance();
	}
}

FScopeZenService::FScopeZenService(FServiceSettings&& InSettings)
{
	UniqueNonDefaultInstance = MakeUnique<FZenServiceInstance>(MoveTemp(InSettings));
	ServiceInstance = UniqueNonDefaultInstance.Get();
}

FScopeZenService::~FScopeZenService()
{}

FZenServiceInstance::FZenServiceInstance()
: FZenServiceInstance(FStringView())
{
}

FZenServiceInstance::FZenServiceInstance(FStringView InstanceURL)
{
	if (InstanceURL.IsEmpty())
	{
		Settings.ReadFromConfig();
		if (Settings.IsAutoLaunch())
		{
			// Ensure that the zen data path is inherited by subprocesses
			FPlatformMisc::SetEnvironmentVar(TEXT("UE-ZenSubprocessDataPath"), *Settings.SettingsVariant.Get<FServiceAutoLaunchSettings>().DataPath);
		}
	}
	else
	{
		Settings.ReadFromURL(InstanceURL);
	}

	Initialize();
}

FZenServiceInstance::FZenServiceInstance(FServiceSettings&& InSettings)
: Settings(MoveTemp(InSettings))
{
	Initialize();
}

FZenServiceInstance::~FZenServiceInstance()
{
}

bool 
FZenServiceInstance::IsServiceRunning()
{
	return !Settings.IsAutoLaunch() || bHasLaunchedLocal;
}

bool 
FZenServiceInstance::IsServiceReady()
{
	if (IsServiceRunning())
	{
		TStringBuilder<128> ZenDomain;
		ZenDomain << HostName << TEXT(":") << Port;
		Zen::FZenHttpRequest Request(ZenDomain.ToString(), false);
		Zen::FZenHttpRequest::Result Result = Request.PerformBlockingDownload(TEXTVIEW("health/ready"), nullptr, Zen::EContentType::Text);
		
		if (Result == Zen::FZenHttpRequest::Result::Success && Zen::IsSuccessCode(Request.GetResponseCode()))
		{
			UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer HTTP service status: %s."), *Request.GetResponseAsString());
			return true;
		}
		else
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Unable to reach ZenServer HTTP service at %s. Status: %d . Response: %s"), ZenDomain.ToString(), Request.GetResponseCode(), *Request.GetResponseAsString());
		}
	}
	return false;
}

bool 
FZenServiceInstance::TryRecovery()
{
	if (!bHasLaunchedLocal)
	{
		return false;
	}

	static FCriticalSection RecoveryCriticalSection;
	static std::atomic<int64> LastRecoveryTicks;
	static bool bLastRecoveryResult = false;
	const FTimespan MaximumWaitForHealth = FTimespan::FromSeconds(30);
	const FTimespan MinimumDurationSinceLastRecovery = FTimespan::FromMinutes(2);

	FTimespan TimespanSinceLastRecovery = FDateTime::UtcNow() - FDateTime(LastRecoveryTicks.load(std::memory_order_relaxed));

	if (TimespanSinceLastRecovery > MinimumDurationSinceLastRecovery)
	{
		FScopeLock Lock(&RecoveryCriticalSection);
		// Update timespan since it may have changed since we waited to enter the crit section
		TimespanSinceLastRecovery = FDateTime::UtcNow() - FDateTime(LastRecoveryTicks.load(std::memory_order_relaxed));
		if (TimespanSinceLastRecovery > MinimumDurationSinceLastRecovery)
		{
			UE_LOG(LogZenServiceInstance, Display, TEXT("Local ZenServer recovery being attempted..."));
			FZenLocalServiceRunContext RunContext;
			if (TryGetLocalServiceRunContext(RunContext))
			{
				StopLocalService(*RunContext.GetDataPath());
				StartLocalService(RunContext);
				UE_LOG(LogZenServiceInstance, Display, TEXT("Local ZenServer recovery finished."));
			}
			else
			{
				UE_LOG(LogZenServiceInstance, Warning, TEXT("Local ZenServer recovery failed due to lack of run context."));
			}
			
			FDateTime StartedWaitingForHealth = FDateTime::UtcNow();
			bLastRecoveryResult = IsServiceReady();
			while (!bLastRecoveryResult)
			{
				FTimespan WaitForHealth = StartedWaitingForHealth - FDateTime::UtcNow();
				if (WaitForHealth > MaximumWaitForHealth)
				{
					UE_LOG(LogZenServiceInstance, Warning, TEXT("Local ZenServer recovery timed out waiting for service to become healthy"));
					break;
				}

				FPlatformProcess::Sleep(0.5f);
				bLastRecoveryResult = IsServiceReady();
			}
			LastRecoveryTicks.store(FDateTime::UtcNow().GetTicks(), std::memory_order_relaxed);
			
			if (bLastRecoveryResult)
			{
				UE_LOG(LogZenServiceInstance, Display, TEXT("Local ZenServer post recovery status: Healthy"));
			}
			else
			{
				UE_LOG(LogZenServiceInstance, Display, TEXT("Local ZenServer post recovery status: NOT healthy"));
			}
		}
	}

	return bLastRecoveryResult;
}

uint16
FZenServiceInstance::GetAutoLaunchedPort()
{
	return AutoLaunchedPort;
}

void
FZenServiceInstance::Initialize()
{
	if (Settings.IsAutoLaunch())
	{
		bHasLaunchedLocal = AutoLaunch(Settings.SettingsVariant.Get<FServiceAutoLaunchSettings>(), ConditionalUpdateLocalInstall(), HostName, Port);
		if (bHasLaunchedLocal)
		{
			AutoLaunchedPort = Port;
			bIsRunningLocally = true;
		}
	}
	else
	{
		const FServiceConnectSettings& ConnectExistingSettings = Settings.SettingsVariant.Get<FServiceConnectSettings>();
		HostName = ConnectExistingSettings.HostName;
		Port = ConnectExistingSettings.Port;
		bIsRunningLocally = IsLocalHost(HostName);
	}
	URL = WriteToString<64>(TEXT("http://"), HostName, TEXT(":"), Port, TEXT("/"));
}

void
FZenServiceInstance::PromptUserToStopRunningServerInstance(const FString& ServerFilePath)
{
	if (FApp::IsUnattended())
	{
		// Do not ask if there is no one to show a message
		return;
	}

	FText ZenUpdatePromptTitle = NSLOCTEXT("Zen", "Zen_UpdatePromptTitle", "Update required");
	FText ZenUpdatePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_UpdatePromptText", "ZenServer needs to be updated to a new version. Please shut down Unreal Editor and any tools that are using the ZenServer at '{0}'"), FText::FromString(ServerFilePath));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenUpdatePromptText.ToString(), *ZenUpdatePromptTitle.ToString());
}

FString
FZenServiceInstance::ConditionalUpdateLocalInstall()
{
	FString InTreeUtilityPath = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("zen"), EBuildConfiguration::Development));
	FString InstallUtilityPath = GetLocalInstallUtilityPath();

	FString InTreeServicePath = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("zenserver"), EBuildConfiguration::Development));
	FString InstallServicePath = GetLocalServiceInstallPath();

	IFileManager& FileManager = IFileManager::Get();

	bool bMainExecutablesUpdated = false;
	FString InTreeVersionCache, InstallVersionCache;
	if (IsInstallVersionOutOfDate(InTreeUtilityPath, InstallUtilityPath, InTreeServicePath, InstallServicePath, InTreeVersionCache, InstallVersionCache))
	{
		if (IsProcessActive(*InstallServicePath))
		{
			FString DataPath = Settings.SettingsVariant.Get<FServiceAutoLaunchSettings>().DataPath;
			StopLocalService(*DataPath);

			if (IsLocalServiceRunning(*DataPath))
			{
				PromptUserToStopRunningServerInstance(InstallServicePath);
			}
		}

		// Even after waiting for the lock file to be removed, the executable may have a period where it can't be overwritten as the process shuts down
		// so any attempt to overwrite it should have some tolerance for retrying.
		bool bServiceExecutableUpdated = AttemptFileCopyWithRetries(*InstallServicePath, *InTreeServicePath, 5.0);
		checkf(bServiceExecutableUpdated, TEXT("Failed to copy zenserver to install location '%s'."), *InstallServicePath);

		bool bUtilityExecutableUpdated = AttemptFileCopyWithRetries(*InstallUtilityPath, *InTreeUtilityPath, 5.0);
		checkf(bUtilityExecutableUpdated, TEXT("Failed to copy zen to install location '%s'."), *InstallUtilityPath);

		bMainExecutablesUpdated = (bServiceExecutableUpdated && bUtilityExecutableUpdated);
		if (bMainExecutablesUpdated)
		{
			AttemptFileCopyWithRetries(*InstallVersionCache, *InTreeVersionCache, 1.0);
		}
	}

#if PLATFORM_WINDOWS
	FString InTreeSymbolFilePath = FPaths::SetExtension(InTreeServicePath, TEXT("pdb"));
	FString InstallSymbolFilePath = FPaths::SetExtension(InstallServicePath, TEXT("pdb"));

	if (FileManager.FileExists(*InTreeSymbolFilePath) && (bMainExecutablesUpdated || !FileManager.FileExists(*InstallSymbolFilePath)))
	{
		AttemptFileCopyWithRetries(*InstallSymbolFilePath, *InTreeSymbolFilePath, 1.0);
	}
#endif

	FString InTreeCrashpadHandlerFilePath = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("crashpad_handler"), EBuildConfiguration::Development));
	FString InstallCrashpadHandlerFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPlatformProcess::ApplicationSettingsDir(), TEXT("Zen\\Install"), FString(FPathViews::GetCleanFilename(InTreeCrashpadHandlerFilePath))));

	if (FileManager.FileExists(*InTreeCrashpadHandlerFilePath) && (bMainExecutablesUpdated || !FileManager.FileExists(*InstallCrashpadHandlerFilePath)))
	{
		AttemptFileCopyWithRetries(*InstallCrashpadHandlerFilePath, *InTreeCrashpadHandlerFilePath, 1.0);
	}

	return InstallServicePath;
}

bool
FZenServiceInstance::AutoLaunch(const FServiceAutoLaunchSettings& InSettings, FString&& ExecutablePath, FString& OutHostName, uint16& OutPort)
{
	int16 DesiredPort = InSettings.DesiredPort;
	IFileManager& FileManager = IFileManager::Get();
	const FString LockFilePath = FPaths::Combine(InSettings.DataPath, TEXT(".lock"));
	const FString ExecutionContextFilePath = FPaths::SetExtension(ExecutablePath, TEXT(".runcontext"));

	FString WorkingDirectory = FPaths::GetPath(ExecutablePath);

	bool bReUsingExistingInstance = false;

	if (IsLockFileLocked(*LockFilePath, true))
	{
		// If an instance is running with this data path, check if we can use it and what port it is on
		uint16 CurrentPort = 0;
		FCbObject LockObject;
		if (ReadCbLockFile(LockFilePath, LockObject))
		{
			bool bIsReady = LockObject["ready"].AsBool();
			if (bIsReady)
			{
				CurrentPort = LockObject["port"].AsUInt16();
			}
		}

		bool bCurrentInstanceUsable = false;

		FZenLocalServiceRunContext DesiredRunContext;
		DesiredRunContext.Executable = ExecutablePath;
		DesiredRunContext.CommandlineArguments = DetermineCmdLineWithoutTransientComponents(InSettings, CurrentPort);
		DesiredRunContext.WorkingDirectory = WorkingDirectory;
		DesiredRunContext.DataPath = InSettings.DataPath;
		DesiredRunContext.bShowConsole = InSettings.bShowConsole;

		FZenLocalServiceRunContext CurrentRunContext;
		if (CurrentRunContext.ReadFromJsonFile(*ExecutionContextFilePath) && (DesiredRunContext == CurrentRunContext))
		{
			DesiredPort = CurrentPort;
			bReUsingExistingInstance = true;
		}
		else
		{
			RequestZenShutdownOnPort(CurrentPort);
			WaitForZenShutdown(*LockFilePath, 5.0);
		}
	}

	if (!bReUsingExistingInstance)
	{
		RequestZenShutdownOnPort(DesiredPort);
	}


	bool bProcessIsLive = IsLockFileLocked(*LockFilePath);

	// When limiting process lifetime, always re-launch to add sponsor process IDs.
	// When not limiting process lifetime, only launch if the process is not already live.
	if (InSettings.bLimitProcessLifetime || !bProcessIsLive)
	{
		FString ParmsWithoutTransients = DetermineCmdLineWithoutTransientComponents(InSettings, DesiredPort);
		FString TransientParms;

		if (InSettings.bLimitProcessLifetime)
		{
			TransientParms.Appendf(TEXT("--owner-pid %d"), FPlatformProcess::GetCurrentProcessId());
		}

		FZenLocalServiceRunContext EffectiveRunContext;
		EffectiveRunContext.Executable = ExecutablePath;
		EffectiveRunContext.CommandlineArguments = ParmsWithoutTransients;
		EffectiveRunContext.WorkingDirectory = WorkingDirectory;
		EffectiveRunContext.DataPath = InSettings.DataPath;
		EffectiveRunContext.bShowConsole = InSettings.bShowConsole;

		FProcHandle Proc = StartLocalService(EffectiveRunContext, TransientParms.IsEmpty() ? nullptr : *TransientParms);
		if (!bProcessIsLive)
		{
			EffectiveRunContext.WriteToJsonFile(*ExecutionContextFilePath);
		}

		bProcessIsLive = Proc.IsValid();
	}


	OutHostName = TEXT("[::1]");
	// Default to assuming that we get to run on the port we want
	OutPort = DesiredPort;

	if (bProcessIsLive)
	{

		FScopedSlowTask WaitForZenReadySlowTask(0, NSLOCTEXT("Zen", "Zen_WaitingForReady", "Waiting for ZenServer to be ready"));
		uint64 ZenWaitStartTime = FPlatformTime::Cycles64();
		enum class EWaitDurationPhase
		{
			Short,
			Medium,
			Long
		} DurationPhase = EWaitDurationPhase::Short;
		bool bIsReady = false;
		while (!bIsReady)
		{
			FCbObject LockObject;
			if (ReadCbLockFile(LockFilePath, LockObject))
			{
				bIsReady = LockObject["ready"].AsBool();
				if (bIsReady)
				{
					OutPort = LockObject["port"].AsUInt16(DesiredPort);
					break;
				}
			}

			double ZenWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - ZenWaitStartTime);
			if (ZenWaitDuration < 10.0)
			{
				// Initial 10 second window of higher frequency checks
				FPlatformProcess::Sleep(0.01f);
			}
			else
			{
				if (DurationPhase == EWaitDurationPhase::Short)
				{
					if (!IsLockFileLocked(*LockFilePath))
					{
						if (FApp::IsUnattended())
						{
							checkf(false, TEXT("ZenServer did not launch in the expected duration."));
						}
						else
						{
							FText ZenLaunchFailurePromptTitle = NSLOCTEXT("Zen", "Zen_LaunchFailurePromptTitle", "Failed to launch");

							FFormatNamedArguments FormatArguments;
							FString LogFilePath = FPaths::Combine(InSettings.DataPath, TEXT("logs"), TEXT("zenserver.log"));
							FPaths::MakePlatformFilename(LogFilePath);
							FormatArguments.Add(TEXT("LogFilePath"), FText::FromString(LogFilePath));
							FText ZenLaunchFailurePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_LaunchFailurePromptText", "ZenServer failed to launch. This process will now exit. Please check the ZenServer log file for details:\n{LogFilePath}"), FormatArguments);
							FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenLaunchFailurePromptText.ToString(), *ZenLaunchFailurePromptTitle.ToString());
							FPlatformMisc::RequestExit(true);
							return false;
						}
					}
					// Note that the dialog may not show up when zenserver is needed early in the launch cycle, but this will at least ensure
					// the splash screen is refreshed with the appropriate text status message.
					WaitForZenReadySlowTask.MakeDialog(true, false);
					UE_LOG(LogZenServiceInstance, Display, TEXT("Waiting for ZenServer to be ready..."));
					DurationPhase = EWaitDurationPhase::Medium;
				}
				else if (!FApp::IsUnattended() && ZenWaitDuration > 20.0 && (DurationPhase == EWaitDurationPhase::Medium))
				{
					FText ZenLongWaitPromptTitle = NSLOCTEXT("Zen", "Zen_LongWaitPromptTitle", "Wait for ZenServer?");
					FText ZenLongWaitPromptText = NSLOCTEXT("Zen", "Zen_LongWaitPromptText", "ZenServer is taking a long time to launch. It may be performing maintenance. Keep waiting?");
					if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *ZenLongWaitPromptText.ToString(), *ZenLongWaitPromptTitle.ToString()) == EAppReturnType::No)
					{
						FPlatformMisc::RequestExit(true);
						return false;
					}
					DurationPhase = EWaitDurationPhase::Long;
				}

				if (WaitForZenReadySlowTask.ShouldCancel())
				{
					FPlatformMisc::RequestExit(true);
					return false;
				}
				FPlatformProcess::Sleep(0.1f);
			}
		}

		return bIsReady;
	}
	else
	{
		return false;
	}
	return true;
}

bool 
FZenServiceInstance::GetStats(FZenStats& Stats)
{
	check(IsInGameThread());

	// If we've already requested a stats and they are ready then grab them
	if ( StatsRequest.IsReady() == true )
	{
		LastStats		= StatsRequest.Get();
		LastStatsTime	= FPlatformTime::Cycles64();

		StatsRequest.Reset();
	}
	
	// Make a copy of the last updated stats
	Stats = LastStats;

	const uint64 CurrentTime = FPlatformTime::Cycles64();
	constexpr double MinTimeBetweenRequestsInSeconds = 0.5;
	const double DeltaTimeInSeconds = FPlatformTime::ToSeconds64(CurrentTime - LastStatsTime);

	if (!StatsRequest.IsValid() && DeltaTimeInSeconds > MinTimeBetweenRequestsInSeconds)
	{
#if WITH_EDITOR
		EAsyncExecution ThreadPool = EAsyncExecution::LargeThreadPool;
#else
		EAsyncExecution ThreadPool = EAsyncExecution::ThreadPool;
#endif
		if (!StatsHttpRequest.IsValid())
		{
			TStringBuilder<128> ZenDomain;
			ZenDomain << HostName << TEXT(":") << Port;
			StatsHttpRequest = MakePimpl<FZenHttpRequest>(ZenDomain.ToString(), false);
		}

		// We've not got any requests in flight and we've met a given time requirement for requests
		StatsRequest = Async(ThreadPool, [this]
			{
				UE::Zen::FZenHttpRequest& Request = *StatsHttpRequest.Get();
				Request.Reset();

				TArray64<uint8> GetBuffer;
				FZenHttpRequest::Result Result = Request.PerformBlockingDownload(TEXTVIEW("/stats/z$"), &GetBuffer, Zen::EContentType::CbObject);

				FZenStats Stats;

				if (Result == Zen::FZenHttpRequest::Result::Success && Request.GetResponseCode() == 200)
				{
					FCbObjectView RootObjectView(GetBuffer.GetData());

					FCbObjectView RequestsObjectView = RootObjectView["requests"].AsObjectView();
					FZenRequestStats& RequestStats = Stats.RequestStats;

					RequestStats.Count = RequestsObjectView["count"].AsInt64();
					RequestStats.RateMean = RequestsObjectView["rate_mean"].AsDouble();
					RequestStats.TAverage = RequestsObjectView["t_avg"].AsDouble();
					RequestStats.TMin = RequestsObjectView["t_min"].AsDouble();
					RequestStats.TMax = RequestsObjectView["t_max"].AsDouble();

					FCbObjectView CacheObjectView = RootObjectView["cache"].AsObjectView();
					FZenCacheStats& CacheStats = Stats.CacheStats;

					CacheStats.Hits = CacheObjectView["hits"].AsInt64();
					CacheStats.Misses = CacheObjectView["misses"].AsInt64();
					CacheStats.HitRatio = CacheObjectView["hit_ratio"].AsDouble();
					CacheStats.UpstreamHits = CacheObjectView["upstream_hits"].AsInt64();
					CacheStats.UpstreamRatio = CacheObjectView["upstream_ratio"].AsDouble();

					FCbObjectView CacheSizeObjectView = CacheObjectView["size"].AsObjectView();
					FZenCacheSizeStats& CacheSizeStats = CacheStats.Size;
					CacheSizeStats.Disk = CacheSizeObjectView["disk"].AsDouble();
					CacheSizeStats.Memory = CacheSizeObjectView["memory"].AsDouble();

					FCbObjectView UpstreamObjectView = RootObjectView["upstream"].AsObjectView();
					FZenUpstreamStats& UpstreamStats = Stats.UpstreamStats;

					UpstreamStats.Reading = UpstreamObjectView["reading"].AsBool();
					UpstreamStats.Writing = UpstreamObjectView["writing"].AsBool();
					UpstreamStats.WorkerThreads = UpstreamObjectView["worker_threads"].AsInt64();
					UpstreamStats.QueueCount = UpstreamObjectView["queue_count"].AsInt64();
					UpstreamStats.TotalUploadedMB = 0.0;
					UpstreamStats.TotalDownloadedMB = 0.0;

					FCbObjectView UpstreamRequestObjectView = RootObjectView["upstream_gets"].AsObjectView();
					FZenRequestStats& UpstreamRequestStats = Stats.UpstreamRequestStats;

					UpstreamRequestStats.Count = UpstreamRequestObjectView["count"].AsInt64();
					UpstreamRequestStats.RateMean = UpstreamRequestObjectView["rate_mean"].AsDouble();
					UpstreamRequestStats.TAverage = UpstreamRequestObjectView["t_avg"].AsDouble();
					UpstreamRequestStats.TMin = UpstreamRequestObjectView["t_min"].AsDouble();
					UpstreamRequestStats.TMax = UpstreamRequestObjectView["t_max"].AsDouble();

					FCbArrayView EndpPointArrayView = UpstreamObjectView["endpoints"].AsArrayView();

					for (FCbFieldView FieldView : EndpPointArrayView)
					{
						FCbObjectView EndPointView = FieldView.AsObjectView();
						FZenEndPointStats EndPointStats;

						EndPointStats.Name = FString(EndPointView["name"].AsString());
						EndPointStats.Url = FString(EndPointView["url"].AsString());
						EndPointStats.Health = FString(EndPointView["state"].AsString());

						if (FCbObjectView Cache = EndPointView["cache"].AsObjectView())
						{
							EndPointStats.HitRatio = Cache["hit_ratio"].AsDouble();
							EndPointStats.UploadedMB = Cache["put_bytes"].AsDouble() / 1024.0 / 1024.0;
							EndPointStats.DownloadedMB = Cache["get_bytes"].AsDouble() / 1024.0 / 1024.0;
							EndPointStats.ErrorCount = Cache["error_count"].AsInt64();
						}

						UpstreamStats.TotalUploadedMB += EndPointStats.UploadedMB;
						UpstreamStats.TotalDownloadedMB += EndPointStats.DownloadedMB;

						UpstreamStats.EndPointStats.Push(EndPointStats);
					}

					FCbObjectView CASObjectView = RootObjectView["cid"].AsObjectView();
					FCbObjectView CASSizeObjectView = CASObjectView["size"].AsObjectView();

					FZenCASSizeStats& CASSizeStats = Stats.CASStats.Size;

					CASSizeStats.Tiny = CASSizeObjectView["tiny"].AsInt64();
					CASSizeStats.Small = CASSizeObjectView["small"].AsInt64();
					CASSizeStats.Large = CASSizeObjectView["large"].AsInt64();
					CASSizeStats.Total = CASSizeObjectView["total"].AsInt64();

					Stats.IsValid = true;
				}

				return Stats;
			});
	}

	return Stats.IsValid;
}

bool 
FZenServiceInstance::GetGCStatus(FGCStatus& Status)
{
	check(IsInGameThread());

	// If we've already requested status and it is ready then grab it
	if (GCStatusRequest.IsReady() == true )
	{
		LastGCStatus	 = GCStatusRequest.Get();
		LastGCStatusTime = FPlatformTime::Cycles64();

		GCStatusRequest.Reset();
	}
	
	// Make a copy of the last updated status
	if (LastGCStatus.IsSet())
	{
		Status = LastGCStatus.GetValue();
	}

	const uint64 CurrentTime = FPlatformTime::Cycles64();
	constexpr double MinTimeBetweenRequestsInSeconds = 0.5;
	const double DeltaTimeInSeconds = FPlatformTime::ToSeconds64(CurrentTime - LastGCStatusTime);

	if (!GCStatusRequest.IsValid() && DeltaTimeInSeconds > MinTimeBetweenRequestsInSeconds)
	{
#if WITH_EDITOR
		EAsyncExecution ThreadPool = EAsyncExecution::LargeThreadPool;
#else
		EAsyncExecution ThreadPool = EAsyncExecution::ThreadPool;
#endif
		if (!GCStatusHttpRequest.IsValid())
		{
			TStringBuilder<128> ZenDomain;
			ZenDomain << HostName << TEXT(":") << Port;
			GCStatusHttpRequest = MakePimpl<FZenHttpRequest>(ZenDomain.ToString(), false);
		}

		// We've not got any requests in flight and we've met a given time requirement for requests
		GCStatusRequest = Async(ThreadPool, [this]
			{
				UE::Zen::FZenHttpRequest& Request = *GCStatusHttpRequest.Get();
				Request.Reset();

				TArray64<uint8> GetBuffer;
				FZenHttpRequest::Result Result = Request.PerformBlockingDownload(TEXTVIEW("/admin/gc"), &GetBuffer, Zen::EContentType::CbObject);

				TOptional<FGCStatus> GCStatus;

				if (Result == Zen::FZenHttpRequest::Result::Success && Request.GetResponseCode() == 200)
				{
					FCbObjectView RootObjectView(GetBuffer.GetData());

					GCStatus.Emplace();
					GCStatus->Description = FString(RootObjectView["Status"].AsString());
				}

				return GCStatus;
			});
	}

	return LastGCStatus.IsSet();
}

bool 
FZenServiceInstance::RequestGC(const bool* OverrideCollectSmallObjects, const uint32* OverrideMaxCacheDuration)
{
	TStringBuilder<128> ZenDomain;
	ZenDomain << HostName << TEXT(":") << Port;
	UE::Zen::FZenHttpRequest Request(ZenDomain.ToString(), false);

	TCHAR Separators[] = {TEXT('?'), TEXT('&')};
	int32 SeparatorIndex = 0;
	TStringBuilder<128> Query;
	Query << TEXTVIEW("/admin/gc");

	if (OverrideCollectSmallObjects)
	{
		Query << Separators[SeparatorIndex] << LexToString(*OverrideCollectSmallObjects);
		SeparatorIndex = FMath::Min(SeparatorIndex + 1, (int32)UE_ARRAY_COUNT(Separators));
	}

	if (OverrideMaxCacheDuration)
	{
		Query << Separators[SeparatorIndex] << LexToString(*OverrideMaxCacheDuration);
		SeparatorIndex = FMath::Min(SeparatorIndex + 1, (int32)UE_ARRAY_COUNT(Separators));
	}

	FZenHttpRequest::Result Result = Request.PerformBlockingPost(Query.ToString(), FMemoryView());

	if (Result == Zen::FZenHttpRequest::Result::Success && Request.GetResponseCode() == 200)
	{
		FCbObjectView ResponseObject = FCbObjectView(Request.GetResponseBuffer().GetData());
		FUtf8StringView ResponseStatus = ResponseObject["status"].AsString();

		return (ResponseStatus == "Started") || (ResponseStatus == "Running");
	}
	return false;
}

bool 
FZenServiceInstance::GatherAnalytics(TArray<FAnalyticsEventAttribute>& Attributes)
{
	FZenStats ZenStats;

	if (GetStats(ZenStats) == false)
		return false;

	const FString BaseName = TEXT("Zen");

	{
		FString AttrName = BaseName + TEXT(".Enabled");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.IsValid);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.HitRatio");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.HitRatio);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.Hits");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Hits);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.Misses");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Misses);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.Size.Disk");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Disk);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.Size.Memory");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Memory);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.UpstreamHits");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamHits);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.UpstreamRatio");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamRatio);
	}

	{
		FString AttrName = BaseName + TEXT(".Cache.TotalUploadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
	}

	{
		FString AttrName = BaseName + TEXT(".Upstream.TotalDownloadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalDownloadedMB);
	}

	{
		FString AttrName = BaseName + TEXT(".Upstream.TotalUploadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
	}

	{
		FString AttrName = BaseName + TEXT(".Cas.Size.Large");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Large);
	}

	{
		FString AttrName = BaseName + TEXT(".Cas.Size.Small");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Small);
	}

	{
		FString AttrName = BaseName + TEXT(".Cas.Size.Tiny");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Tiny);
	}

	{
		FString AttrName = BaseName + TEXT(".Cas.Size.Total");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Total);
	}

	return true;
}

#endif // UE_WITH_ZEN

}

