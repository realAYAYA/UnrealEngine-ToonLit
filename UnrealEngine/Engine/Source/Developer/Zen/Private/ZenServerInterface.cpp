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
#include "Serialization/CompactBinaryWriter.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include <shellapi.h>
#	include <synchapi.h>
#	include <Windows.h>
#	include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_UNIX || PLATFORM_MAC
#	include <sys/file.h>
#	include <sys/mman.h>
#	include <sys/sem.h>
#endif

#define ALLOW_SETTINGS_OVERRIDE_FROM_COMMANDLINE			(UE_SERVER || !(UE_BUILD_SHIPPING))

namespace UE::Zen
{

DEFINE_LOG_CATEGORY_STATIC(LogZenServiceInstance, Log, All);

class ZenServerState
{
public:
	ZenServerState(bool ReadOnly = true);
	~ZenServerState();

	struct ZenServerEntry
	{
		// This matches the structure found in the Zen server
		// https://github.com/EpicGames/zen/blob/main/zenutil/include/zenutil/zenserverprocess.h#L91
		//
		std::atomic<uint32> Pid;
		std::atomic<uint16> DesiredListenPort;
		std::atomic<uint16> Flags;
		uint8				  SessionId[12];
		std::atomic<uint32> SponsorPids[8];
		std::atomic<uint16> EffectiveListenPort;
		uint8				  Padding[10];

		enum class FlagsEnum : uint16
		{
			kShutdownPlease = 1 << 0,
			kIsReady = 1 << 1,
		};

		bool AddSponsorProcess(uint32_t PidToAdd);
	};
	static_assert(sizeof(ZenServerEntry) == 64);

	ZenServerEntry* LookupByDesiredListenPort(int DesiredListenPort);
	const ZenServerEntry* LookupByDesiredListenPort(int DesiredListenPort) const;
	const ZenServerEntry* LookupByEffectiveListenPort(int DesiredListenPort) const;
	const ZenServerEntry* LookupByPid(uint32 Pid) const;

private:
	void* m_hMapFile = nullptr;
	ZenServerEntry* m_Data = nullptr;
	int				m_MaxEntryCount = 65536 / sizeof(ZenServerEntry);
	bool			m_IsReadOnly = true;
};

ZenServerState::ZenServerState(bool ReadOnly)
	: m_hMapFile(nullptr)
	, m_Data(nullptr)
{
	size_t MapSize = m_MaxEntryCount * sizeof(ZenServerEntry);

#if PLATFORM_WINDOWS
	DWORD DesiredAccess = ReadOnly ? FILE_MAP_READ : (FILE_MAP_READ | FILE_MAP_WRITE);
	HANDLE hMap = OpenFileMapping(DesiredAccess, 0, L"Global\\ZenMap");
	if (hMap == NULL)
	{
		hMap = OpenFileMapping(DesiredAccess, 0, L"Local\\ZenMap");
	}

	if (hMap == NULL)
	{
		return;
	}

	void* pBuf = MapViewOfFile(hMap,		   // handle to map object
		DesiredAccess,  // read permission
		0,			   // offset high
		0,			   // offset low
		MapSize);

	if (pBuf == NULL)
	{
		CloseHandle(hMap);
		return;
	}
#elif PLATFORM_UNIX || PLATFORM_MAC
	int Fd = shm_open("/UnrealEngineZen", O_RDONLY | O_CLOEXEC, 0666);
	if (Fd < 0)
	{
		return;
	}
	void* hMap = (void*)intptr_t(Fd);

	int Prot = ReadOnly ? (PROT_WRITE | PROT_READ) : PROT_READ;
	void* pBuf = mmap(nullptr, MapSize, Prot, MAP_PRIVATE, Fd, 0);
	if (pBuf == MAP_FAILED)
	{
		close(Fd);
		return;
	}
#endif

#if PLATFORM_WINDOWS || PLATFORM_UNIX || PLATFORM_MAC
	m_hMapFile = hMap;
	m_Data = reinterpret_cast<ZenServerEntry*>(pBuf);
#endif
	m_IsReadOnly = ReadOnly;
}

ZenServerState::~ZenServerState()
{
#if PLATFORM_WINDOWS
	if (m_Data)
	{
		UnmapViewOfFile(m_Data);
	}

	if (m_hMapFile)
	{
		CloseHandle(m_hMapFile);
	}
#elif PLATFORM_UNIX || PLATFORM_MAC
	if (m_Data != nullptr)
	{
		munmap((void*)m_Data, m_MaxEntryCount * sizeof(ZenServerEntry));
	}

	int Fd = int(intptr_t(m_hMapFile));
	close(Fd);
#endif
	m_hMapFile = nullptr;
	m_Data = nullptr;
}

const ZenServerState::ZenServerEntry* ZenServerState::LookupByDesiredListenPort(int Port) const
{
	if (m_Data == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < m_MaxEntryCount; ++i)
	{
		if (m_Data[i].DesiredListenPort == Port)
		{
			return &m_Data[i];
		}
	}

	return nullptr;
}

ZenServerState::ZenServerEntry* ZenServerState::LookupByDesiredListenPort(int Port)
{
	check(!m_IsReadOnly);

	if (m_Data == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < m_MaxEntryCount; ++i)
	{
		if (m_Data[i].DesiredListenPort == Port)
		{
			return &m_Data[i];
		}
	}

	return nullptr;
}

const ZenServerState::ZenServerEntry* ZenServerState::LookupByEffectiveListenPort(int Port) const
{
	if (m_Data == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < m_MaxEntryCount; ++i)
	{
		if (m_Data[i].EffectiveListenPort == Port)
		{
			return &m_Data[i];
		}
	}

	return nullptr;
}

const ZenServerState::ZenServerEntry* ZenServerState::LookupByPid(uint32 Pid) const
{
	if (m_Data == nullptr)
	{
		return nullptr;
	}

	for (int i = 0; i < m_MaxEntryCount; ++i)
	{
		if (m_Data[i].Pid == Pid)
		{
			return &m_Data[i];
		}
	}

	return nullptr;
}

bool
ZenServerState::ZenServerEntry::AddSponsorProcess(uint32_t PidToAdd)
{
	for (std::atomic<uint32_t>& PidEntry : SponsorPids)
	{
		if (PidEntry.load(std::memory_order_relaxed) == 0)
		{
			uint32_t Expected = 0;
			if (PidEntry.compare_exchange_strong(Expected, PidToAdd))
			{
				// Success!
				return true;
			}
		}
		else if (PidEntry.load(std::memory_order_relaxed) == PidToAdd)
		{
			// Success, the because pid is already in the list
			return true;
		}
	}

	return false;
}

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
	bool VersionCacheIsOlderThanUtilityExecutable = VersionCacheModificationTime < UtilityExecutableModificationTime;
	bool VersionCacheIsOlderThanServerExecutable = VersionCacheModificationTime < ServiceExecutableModificationTime;
	bool VersionCacheIsOutOfDate = VersionCacheIsOlderThanUtilityExecutable || VersionCacheIsOlderThanServerExecutable;
	FString VersionFileContents;
	FZenVersion ComparableVersion;
	if (VersionCacheIsOutOfDate ||
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

static void
PromptUserToSyncInTreeVersion(const FString& ServerFilePath)
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer can not verify installation. Please make sure your source installation in properly synced at '%s'"), *FPaths::GetPath(ServerFilePath));
		return;
	}

	FText ZenSyncSourcePromptTitle = NSLOCTEXT("Zen", "Zen_SyncSourcePromptTitle", "Failed to launch");
	FText ZenSyncSourcePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_SyncSourcePromptText", "ZenServer can not verify installation. Please make sure your source installation in properly synced at '{0}'"), FText::FromString(FPaths::GetPath(ServerFilePath)));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenSyncSourcePromptText.ToString(), *ZenSyncSourcePromptTitle.ToString());
}

static bool
IsInstallVersionOutOfDate(const FString& InTreeUtilityPath, const FString& InstallUtilityPath, const FString& InTreeServicePath, const FString& InstallServicePath, FString& OutInTreeVersionCache, FString& OutInstallVersionCache)
{
	OutInTreeVersionCache = FPaths::Combine(FPaths::EngineSavedDir(), TEXT("Zen"), TEXT("zen.version"));
	OutInstallVersionCache = FPaths::SetExtension(InstallUtilityPath, TEXT("version"));

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*InTreeUtilityPath) || !FileManager.FileExists(*InTreeServicePath))
	{
		UE_LOG(LogZenServiceInstance, Warning, TEXT("InTree version at '%s' is invalid"), *InTreeServicePath);
		PromptUserToSyncInTreeVersion(InTreeServicePath);
		return false;
	}

	// Always get the InTree utility path so cached version information is up to date
	FZenVersion InTreeVersion = GetZenVersion(InTreeUtilityPath, InTreeServicePath, OutInTreeVersionCache);
	UE_LOG(LogZenServiceInstance, Log, TEXT("InTree version at '%s' is '%s'"), *InTreeServicePath, *InTreeVersion.ToString());

	if (!FileManager.FileExists(*InstallUtilityPath) || !FileManager.FileExists(*InstallServicePath))
	{
		UE_LOG(LogZenServiceInstance, Log, TEXT("No installation found at '%s'"), *InstallServicePath);
		return true;
	}
	FZenVersion InstallVersion = GetZenVersion(InstallUtilityPath, InstallServicePath, OutInstallVersionCache);
	UE_LOG(LogZenServiceInstance, Log, TEXT("Installed version at '%s' is '%s'"), *InstallServicePath, *InstallVersion.ToString());

	if (InstallVersion < InTreeVersion)
	{
		UE_LOG(LogZenServiceInstance, Log, TEXT("Installed version at '%s' (%s) is older than '%s' (%s)"), *InstallServicePath, *InstallVersion.ToString(), *InTreeServicePath, *InTreeVersion.ToString());
		return true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ForceZenInstall")))
	{
		UE_LOG(LogZenServiceInstance, Display, TEXT("Forcing install from '%s' (%s) over '%s' (%s)"), *InTreeServicePath, *InTreeVersion.ToString(), *InstallServicePath, *InstallVersion.ToString());
		return true;
	}
	return false;
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
	if (CopyResult == COPY_OK)
	{
		return true;
	}
	UE_LOG(LogZenServiceInstance, Warning, TEXT("copy from '%s' to '%s', '%s'"), Src, Dst, CopyResult == COPY_Fail ? TEXT("Failed to copy file") : TEXT("Cancelled file copy"));
	return false;
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

static bool
DetermineDataPath(const TCHAR* ConfigSection, FString& DataPath, bool& HasInvalidPathConfigurations)
{
	auto ValidateDataPath = [](const FString& InDataPath)
{
		if (InDataPath.IsEmpty())
	{
			return FString{};
		}
		IFileManager& FileManager = IFileManager::Get();
		FString FinalPath = FPaths::ConvertRelativePathToFull(InDataPath);
		FPaths::NormalizeDirectoryName(FinalPath);
		FFileStatData StatData = FileManager.GetStatData(*InDataPath);
		if (StatData.bIsValid && StatData.bIsDirectory)
		{
			FString TestFilePath = FinalPath / ".zen-startup-test-file";
			FArchive* TestFile = FileManager.CreateFileWriter(*TestFilePath, FILEWRITE_Silent);
			if (!TestFile)
			{
				return FString{};
			}
			TestFile->Close();
			delete TestFile;
			FileManager.Delete(*TestFilePath);
		return FinalPath;
		}
		if (FileManager.MakeDirectory(*InDataPath, true))
		{
			return FinalPath;
		}
		return FString{};
	};

	// Zen commandline
	FString CommandLineOverrideValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZenDataPath="), CommandLineOverrideValue) && !CommandLineOverrideValue.IsEmpty())
	{
		if (FString Path = ValidateDataPath(CommandLineOverrideValue); !Path.IsEmpty())
	{
			DataPath = Path;
		UE_LOG(LogZenServiceInstance, Log, TEXT("Found command line override ZenDataPath=%s"), *CommandLineOverrideValue);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping command line override ZenDataPath=%s due to an invalid path"), *CommandLineOverrideValue);
		HasInvalidPathConfigurations = true;
	}

	// Zen subprocess environment
	if (FString SubprocessDataPathEnvOverrideValue = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-ZenSubprocessDataPath")); !SubprocessDataPathEnvOverrideValue.IsEmpty())
	{
		if (FString Path = ValidateDataPath(SubprocessDataPathEnvOverrideValue); !Path.IsEmpty())
	{
			DataPath = Path;
		UE_LOG(LogZenServiceInstance, Log, TEXT("Found subprocess environment variable UE-ZenSubprocessDataPath=%s"), *SubprocessDataPathEnvOverrideValue);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping subprocess environment variable UE-ZenSubprocessDataPath=%s due to an invalid path"), *SubprocessDataPathEnvOverrideValue);
		HasInvalidPathConfigurations = true;
	}

	// Zen registry/stored
	FString DataPathEnvOverrideValue;
	if (FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("Zen"), TEXT("DataPath"), DataPathEnvOverrideValue) && !DataPathEnvOverrideValue.IsEmpty())
	{
		if (FString Path = ValidateDataPath(DataPathEnvOverrideValue); !Path.IsEmpty())
		{
			DataPath = Path;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found registry key Zen DataPath=%s"), *DataPathEnvOverrideValue);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping registry key Zen DataPath=%s due to an invalid path"), *DataPathEnvOverrideValue);
		HasInvalidPathConfigurations = true;
	}

	// Zen environment
	if (FString ZenDataPathEnvOverrideValue = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-ZenDataPath")); !ZenDataPathEnvOverrideValue.IsEmpty())
	{
		if (FString Path = ValidateDataPath(ZenDataPathEnvOverrideValue); !Path.IsEmpty())
	{
			DataPath = Path;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found environment variable UE-ZenDataPath=%s"), *ZenDataPathEnvOverrideValue);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping environment variable UE-ZenDataPath=%s due to an invalid path"), *ZenDataPathEnvOverrideValue);
		HasInvalidPathConfigurations = true;
	}

	// Follow local DDC (if outside workspace)
	FString LocalDataCachePath;
	DetermineLocalDataCachePath(ConfigSection, LocalDataCachePath);
	if (!LocalDataCachePath.IsEmpty() && (LocalDataCachePath != TEXT("None")) && !FPaths::IsUnderDirectory(LocalDataCachePath, FPaths::RootDir()))
	{
		FString ZenLocalDataCachePath = FPaths::Combine(LocalDataCachePath, TEXT("Zen"));
		if (FString Path = ValidateDataPath(ZenLocalDataCachePath); !Path.IsEmpty())
		{
			DataPath = Path;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found local data cache path=%s"), *LocalDataCachePath);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping local data cache path=%s due to an invalid path"), *LocalDataCachePath);
		HasInvalidPathConfigurations = true;
	}

	// Zen config default
	FString ConfigDefaultPath;
	GConfig->GetString(ConfigSection, TEXT("DataPath"), ConfigDefaultPath, GEngineIni);
	if (!ConfigDefaultPath.IsEmpty())
	{
		if (FString Path = ValidateDataPath(ConfigDefaultPath); !Path.IsEmpty())
		{
			DataPath = Path;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found Zen config default=%s"), *ConfigDefaultPath);
			return true;
		}
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Skipping Zen config default=%s due to an invalid path"), *ConfigDefaultPath);
		HasInvalidPathConfigurations = true;
	}
	UE_LOG(LogZenServiceInstance, Warning, TEXT("Unable to determine a valid Zen data path"));
	return false;
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

static void
ApplyProcessLifetimeOverride(bool& bLimitProcessLifetime)
{
	FString LimitProcessLifetime = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-ZenLimitProcessLifetime"));
	if (!LimitProcessLifetime.IsEmpty())
	{
		bLimitProcessLifetime = LimitProcessLifetime.ToBool();
	}
}

static void
PromptUserUnableToDetermineValidDataPath()
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer is unable to determine a valid data path"));
		return;
	}
	FString LogDirPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
	FText ZenInvalidDataPathPromptTitle = NSLOCTEXT("Zen", "Zen_InvalidDataPathPromptTitle", "No Valid Data Path Configuration");
	FText ZenInvalidDataPathPromptText = FText::Format(NSLOCTEXT("Zen", "Zen_InvalidDataPathPromptText", "ZenServer can not determine a valid data path.\nPlease check the log in '{0}' for details.\nUpdate your configuration and restart."), FText::FromString(LogDirPath));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenInvalidDataPathPromptText.ToString(), *ZenInvalidDataPathPromptTitle.ToString());
}

static void
PromptUserAboutInvalidValidDataPathConfiguration(const FString& UsedDataPath)
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer has detected invalid data path configuration. Falling back to '%s'"), *UsedDataPath);
		return;
	}

	FString LogDirPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir());
	FText ZenInvalidValidDataPathConfigurationPromptTitle = NSLOCTEXT("Zen", "Zen_InvalidValidDataPathConfigurationPromptTitle", "Invalid Data Paths");
	FText ZenInvalidValidDataPathConfigurationPromptText = FText::Format(NSLOCTEXT("Zen", "Zen_InvalidValidDataPathConfigurationPromptText", "ZenServer has detected invalid data path configuration.\nPlease check the log in '{0}' for details.\n\nFalling back to using '{1}' as data path."), FText::FromString(LogDirPath), FText::FromString(UsedDataPath));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenInvalidValidDataPathConfigurationPromptText.ToString(), *ZenInvalidValidDataPathConfigurationPromptTitle.ToString());
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

			bool HasInvalidPathConfigurations = false;
			if (!DetermineDataPath(AutoLaunchConfigSection, AutoLaunchSettings.DataPath, HasInvalidPathConfigurations))
			{
				PromptUserUnableToDetermineValidDataPath();
				FPlatformMisc::RequestExit(true);
				return;
			}
			else if (HasInvalidPathConfigurations)
			{
				PromptUserAboutInvalidValidDataPathConfiguration(AutoLaunchSettings.DataPath);
			}
			GConfig->GetString(AutoLaunchConfigSection, TEXT("ExtraArgs"), AutoLaunchSettings.ExtraArgs, GEngineIni);

			ReadUInt16FromConfig(AutoLaunchConfigSection, TEXT("DesiredPort"), AutoLaunchSettings.DesiredPort, GEngineIni);
			GConfig->GetBool(AutoLaunchConfigSection, TEXT("ShowConsole"), AutoLaunchSettings.bShowConsole, GEngineIni);
			GConfig->GetBool(AutoLaunchConfigSection, TEXT("LimitProcessLifetime"), AutoLaunchSettings.bLimitProcessLifetime, GEngineIni);
			ApplyProcessLifetimeOverride(AutoLaunchSettings.bLimitProcessLifetime);
			GConfig->GetBool(TEXT("/Script/UnrealEd.CrashReportsPrivacySettings"), TEXT("bSendUnattendedBugReports"), AutoLaunchSettings.bSendUnattendedBugReports, GEditorSettingsIni);
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
FServiceSettings::ReadFromCompactBinary(FCbFieldView Field)
{
	if (bool bAutoLaunchValue = Field["bAutoLaunch"].AsBool())
	{
		if (!TryApplyAutoLaunchOverride())
		{
			SettingsVariant.Emplace<FServiceAutoLaunchSettings>();
			FServiceAutoLaunchSettings& AutoLaunchSettings = SettingsVariant.Get<FServiceAutoLaunchSettings>();

			if (FCbObjectView AutoLaunchSettingsObject = Field["AutoLaunchSettings"].AsObjectView())
			{
				AutoLaunchSettings.DataPath = FString(AutoLaunchSettingsObject["DataPath"].AsString());
				AutoLaunchSettings.ExtraArgs = FString(AutoLaunchSettingsObject["ExtraArgs"].AsString());
				AutoLaunchSettings.DesiredPort = AutoLaunchSettingsObject["DesiredPort"].AsInt16();
				AutoLaunchSettings.bShowConsole = AutoLaunchSettingsObject["ShowConsole"].AsBool();
				AutoLaunchSettings.bLimitProcessLifetime = AutoLaunchSettingsObject["LimitProcessLifetime"].AsBool();
				ApplyProcessLifetimeOverride(AutoLaunchSettings.bLimitProcessLifetime);
				AutoLaunchSettings.bSendUnattendedBugReports = AutoLaunchSettingsObject["SendUnattendedBugReports"].AsBool();
				AutoLaunchSettings.bIsDefaultSharedRunContext = AutoLaunchSettingsObject["IsDefaultSharedRunContext"].AsBool(AutoLaunchSettings.bIsDefaultSharedRunContext);
			}
		}
	}
	else
	{
		SettingsVariant.Emplace<FServiceConnectSettings>();
		FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();

		if (FCbObjectView ConnectExistingSettingsObject = Field["ConnectExistingSettings"].AsObjectView())
		{
			ConnectExistingSettings.HostName = FString(ConnectExistingSettingsObject["HostName"].AsString());
			ConnectExistingSettings.Port = ConnectExistingSettingsObject["Port"].AsInt16();
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
FServiceSettings::WriteToCompactBinary(FCbWriter& Writer) const
{
	bool bAutoLaunch = IsAutoLaunch();
	Writer << "bAutoLaunch" << bAutoLaunch;
	if (bAutoLaunch)
	{
		const FServiceAutoLaunchSettings& AutoLaunchSettings = SettingsVariant.Get<FServiceAutoLaunchSettings>();
		Writer.BeginObject("AutoLaunchSettings");
		Writer << "DataPath" << AutoLaunchSettings.DataPath;
		Writer << "ExtraArgs" <<AutoLaunchSettings.ExtraArgs;
		Writer << "DesiredPort" << AutoLaunchSettings.DesiredPort;
		Writer << "ShowConsole" << AutoLaunchSettings.bShowConsole;
		Writer << "LimitProcessLifetime" << AutoLaunchSettings.bLimitProcessLifetime;
		Writer << "SendUnattendedBugReports" << AutoLaunchSettings.bSendUnattendedBugReports;
		Writer << "IsDefaultSharedRunContext" << AutoLaunchSettings.bIsDefaultSharedRunContext;
		Writer.EndObject();
	}
	else
	{
		const FServiceConnectSettings& ConnectExistingSettings = SettingsVariant.Get<FServiceConnectSettings>();
		Writer.BeginObject("ConnectExistingSettings");
		Writer << "HostName" << ConnectExistingSettings.HostName;
		Writer << "Port" << ConnectExistingSettings.Port;
		Writer.EndObject();
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

struct LockFileData
{
	uint32 ProcessId = 0;
	FString DataDir;
	uint16 EffectivePort = 0;
	FCbObjectId SessionId;
	bool IsReady = false;
	bool IsValid = false;
};

static LockFileData ReadLockData(FUniqueBuffer&& FileBytes)
{
	if (ValidateCompactBinary(FileBytes, ECbValidateMode::Default) == ECbValidateError::None)
	{
		FCbObject LockObject(FileBytes.MoveToShared());
		int32 ProcessId = LockObject["pid"].AsInt32();
		FUtf8StringView DataDir = LockObject["data"].AsString();
		int32 EffectivePort = LockObject["port"].AsInt32();
		FCbObjectId SessionId = LockObject["session_id"].AsObjectId();
		bool IsReady = LockObject["ready"].AsBool();
		bool IsValid = ProcessId > 0 && EffectivePort > 0 && EffectivePort <= 0xffff;
		return LockFileData{static_cast<uint32>(ProcessId), FString(DataDir), static_cast<uint16>(EffectivePort), SessionId, IsReady, IsValid};
	}
	return LockFileData{ 0, {}, 0, {}, false};
}

static LockFileData
ReadCbLockFile(FStringView FileName)
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
				return ReadLockData(std::move(FileBytes));
			}
		}
	}
	return {};
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<256> LockFilePath;
	LockFilePath << FileName;
	int32 Fd = open(LockFilePath.ToString(), O_RDONLY);
	if (Fd < 0)
	{
		return {};
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
		return {};
	}

	if (errno != EWOULDBLOCK && errno != EAGAIN)
	{
		return {};
	}

	struct stat Stat;
	fstat(Fd, &Stat);
	uint64 FileSize = uint64(Stat.st_size);

	FUniqueBuffer FileBytes = FUniqueBuffer::Alloc(FileSize);
	if (read(Fd, FileBytes.GetData(), FileSize) == FileSize)
	{
		return ReadLockData(std::move(FileBytes));
	}

	close(Fd);
	return {};
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

static bool
IsZenProcessUsingEffectivePort(uint16 EffectiveListenPort)
{
#if PLATFORM_WINDOWS
	HANDLE Handle = OpenEventW(READ_CONTROL, false, *WriteToWideString<64>(WIDETEXT("Zen_"), EffectiveListenPort, WIDETEXT("_Shutdown")));
	if (Handle != NULL)
	{
		ON_SCOPE_EXIT{ CloseHandle(Handle); };
		return true;
	}
	return false;
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<64> EventPath;
	EventPath << "/tmp/Zen_" << EffectiveListenPort << "_Shutdown";

	key_t IpcKey = ftok(EventPath.ToString(), 1);
	if (IpcKey < 0)
	{
		return false;
	}

	int Semaphore = semget(IpcKey, 1, 0400);
	if (Semaphore < 0)
	{
		return false;
	}
	return true;
#else
	static_assert(false, "Missing implementation for Zen named shutdown events");
	return false;
#endif
}

static bool
IsZenProcessActive(const TCHAR* ExecutablePath, uint32* OutPid)
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
			if (OutPid)
			{
				*OutPid = ProcInfo.GetPID();
			}
			return true;
		}
	}
	return false;
}

static bool
IsZenProcessUsingDataDir(const TCHAR* LockFilePath, LockFileData* OutLockFileData)
{
	if (IsLockFileLocked(LockFilePath, true))
	{
		if (OutLockFileData)
		{
			// If an instance is running with this data path, check if we can use it and what port it is on
			*OutLockFileData = ReadCbLockFile(LockFilePath);
		}
		return true;
	}
	return false;
}

static bool
RequestZenShutdownOnEffectivePort(uint16 EffectiveListenPort)
{
#if PLATFORM_WINDOWS
	HANDLE Handle = OpenEventW(EVENT_MODIFY_STATE, false, *WriteToWideString<64>(WIDETEXT("Zen_"), EffectiveListenPort, WIDETEXT("_Shutdown")));
	if (Handle != NULL)
	{
		ON_SCOPE_EXIT{ CloseHandle(Handle); };
		BOOL OK = SetEvent(Handle);
		return OK;
	}
	return false;
#elif PLATFORM_UNIX || PLATFORM_MAC
	TAnsiStringBuilder<64> EventPath;
	EventPath << "/tmp/Zen_" << EffectiveListenPort << "_Shutdown";

	key_t IpcKey = ftok(EventPath.ToString(), 1);
	if (IpcKey < 0)
	{
		return false;
	}

	int Semaphore = semget(IpcKey, 1, 0600);
	if (Semaphore < 0)
	{
		return false;
	}

	semctl(Semaphore, 0, SETVAL, 0);
	semctl(Semaphore, 0, IPC_RMID);
	return true;
#else
	static_assert(false, "Missing implementation for Zen named shutdown events");
	return false;
#endif
}

static bool
ShutdownRunningServiceUsingExecutablePath(const TCHAR* ExecutablePath, double MaximumWaitDurationSeconds = 5.0)
{
	uint32 ServicePid;
	if (!IsZenProcessActive(ExecutablePath, &ServicePid))
	{
		UE_LOG(LogZenServiceInstance, Log, TEXT("No running service found for executable '%s'"), ExecutablePath);
		return true;
	}
	UE_LOG(LogZenServiceInstance, Display, TEXT("Waiting for running instance using executable '%s' to shut down"), ExecutablePath);
	FProcHandle ProcessHandle = FPlatformProcess::OpenProcess(ServicePid);
	if (!ProcessHandle.IsValid() && IsZenProcessActive(ExecutablePath, nullptr))
	{
		UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed to open handle for running service for executable '%s' (Pid: %u)"), ExecutablePath, ServicePid);
		return false;
	}
	ON_SCOPE_EXIT{ FPlatformProcess::CloseProc(ProcessHandle); };

	const ZenServerState ServerState;
	const ZenServerState::ZenServerEntry* Entry = ServerState.LookupByPid(ServicePid);
	if (!Entry)
	{
		if (FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Can't find server state for running service for executable '%s' (Pid: %u)"), ExecutablePath, ServicePid);
			return false;
		}
		return true;
	}
	uint16 EffectiveListenPort = Entry->EffectiveListenPort;
	if (!RequestZenShutdownOnEffectivePort(EffectiveListenPort))
	{
		if (FPlatformProcess::IsProcRunning(ProcessHandle) || IsZenProcessActive(ExecutablePath, nullptr))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed to request shutdown for running service for executable '%s' (Pid: %u, Port: %u)"), ExecutablePath, ServicePid, EffectiveListenPort);
			return false;
		}
		return true;
	}
	uint64 ZenShutdownWaitStartTime = FPlatformTime::Cycles64();
	while (FPlatformProcess::IsProcRunning(ProcessHandle) || IsZenProcessActive(ExecutablePath, nullptr))
	{
		double ZenShutdownWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - ZenShutdownWaitStartTime);
		if (ZenShutdownWaitDuration < MaximumWaitDurationSeconds)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		else
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Timed out waiting for shutdown of running service for executable '%s' (Pid: %u, Port: %u)"), ExecutablePath, ServicePid, EffectiveListenPort);
			return false;
		}
	}
	return true;
}

static bool
ShutdownRunningServiceUsingEffectivePort(uint16 EffectiveListenPort, double MaximumWaitDurationSeconds = 5.0)
{
	if (!IsZenProcessUsingEffectivePort(EffectiveListenPort))
	{
		return true;
	}
	UE_LOG(LogZenServiceInstance, Display, TEXT("Waiting for running instance using port %u to shut down"), EffectiveListenPort);
	const ZenServerState ServerState;
	const ZenServerState::ZenServerEntry* Entry = ServerState.LookupByEffectiveListenPort(EffectiveListenPort);
	if (!Entry)
	{
		if (IsZenProcessUsingEffectivePort(EffectiveListenPort))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Can't find server state for running service using port %u"), EffectiveListenPort);
			return false;
		}
		return true;
	}

	uint32 ServicePid = Entry->Pid;
	FProcHandle ProcessHandle = FPlatformProcess::OpenProcess(ServicePid);
	if (!ProcessHandle.IsValid())
	{
		if (IsZenProcessUsingEffectivePort(EffectiveListenPort))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed to open handle for running service using port %u (Pid: %u)"), EffectiveListenPort, ServicePid);
			return false;
		}
		return true;
	}
	ON_SCOPE_EXIT{ FPlatformProcess::CloseProc(ProcessHandle); };

	if (!RequestZenShutdownOnEffectivePort(EffectiveListenPort))
	{
		if (FPlatformProcess::IsProcRunning(ProcessHandle) || IsZenProcessUsingEffectivePort(EffectiveListenPort))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed to request shutdown for running service using port %u (Pid: %u)"), EffectiveListenPort, ServicePid);
			return false;
		}
		return true;
	}
	uint64 ZenShutdownWaitStartTime = FPlatformTime::Cycles64();
	while (FPlatformProcess::IsProcRunning(ProcessHandle) || IsZenProcessUsingEffectivePort(EffectiveListenPort))
	{
		double ZenShutdownWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - ZenShutdownWaitStartTime);
		if (ZenShutdownWaitDuration < MaximumWaitDurationSeconds)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		else
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Timed out waiting for shutdown of running service using port %u (Pid: %u)"), EffectiveListenPort, ServicePid);
			return false;
		}
	}
	return true;
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

	if (!InSettings.bSendUnattendedBugReports)
	{
		Parms.Append(TEXT(" --no-sentry"));
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
IsLocalServiceRunning(const TCHAR* DataPath, uint16* OutEffectivePort)
{
	const FString LockFilePath = FPaths::Combine(DataPath, TEXT(".lock"));
	LockFileData LockFileState;
	if (IsZenProcessUsingDataDir(*LockFilePath, &LockFileState))
	{
		if (OutEffectivePort != nullptr && LockFileState.IsValid && LockFileState.IsReady)
		{
			*OutEffectivePort = LockFileState.EffectivePort;
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

	UE_LOG(LogZenServiceInstance, Display, TEXT("Launching executable '%s', working dir '%s', data dir '%s', args '%s'"), *Context.GetExecutable(), *Context.GetWorkingDirectory(), *Context.GetDataPath(), *Parms);

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
		if (CreateProcess(NULL, CommandLine.GetCharArray().GetData(), nullptr, nullptr, false, (::DWORD)(NORMAL_PRIORITY_CLASS | DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB), nullptr, PlatformWorkingDirectory.GetCharArray().GetData(), &StartupInfo, &ProcInfo))
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
	const FString LockFilePath = FPaths::Combine(DataPath, TEXT(".lock"));
	LockFileData LockFileState;
	if (IsZenProcessUsingDataDir(*LockFilePath, &LockFileState))
	{
		if (LockFileState.IsValid)
		{
			return ShutdownRunningServiceUsingEffectivePort(LockFileState.EffectivePort, MaximumWaitDurationSeconds);
		}
		return false;
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
			UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer HTTP service at %s status: %s."), ZenDomain.ToString(), *Request.GetResponseAsString());
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
				if (!ShutdownRunningServiceUsingExecutablePath(*RunContext.GetExecutable()))
				{
					return false;
				}
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
		FString ExecutableInstallPath = ConditionalUpdateLocalInstall();
		if (!ExecutableInstallPath.IsEmpty())
		{
			bHasLaunchedLocal = AutoLaunch(Settings.SettingsVariant.Get<FServiceAutoLaunchSettings>(), *ExecutableInstallPath, HostName, Port);
			if (bHasLaunchedLocal)
			{
				AutoLaunchedPort = Port;
				bIsRunningLocally = true;
			}
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

static void
PromptUserToStopRunningServerInstanceForUpdate(const FString& ServerFilePath)
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Display, TEXT("ZenServer needs to be updated to a new version. Please shut down any tools that are using the ZenServer at '%s'"), *ServerFilePath);
		return;
	}

	FText ZenUpdatePromptTitle = NSLOCTEXT("Zen", "Zen_UpdatePromptTitle", "Update required");
	FText ZenUpdatePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_UpdatePromptText", "ZenServer needs to be updated to a new version. Please shut down Unreal Editor and any tools that are using the ZenServer at '{0}'"), FText::FromString(ServerFilePath));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenUpdatePromptText.ToString(), *ZenUpdatePromptTitle.ToString());
}

static void
PromptUserOfLockedDataFolder(const FString& DataPath)
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Warning, TEXT("ZenServer Failed to auto launch, an unknown process is locking the data folder '%s'"), *DataPath);
		return;
	}

	FText ZenLaunchFailurePromptTitle = NSLOCTEXT("Zen", "Zen_NonLocalProcessUsesDataDirPromptTitle", "Failed to launch");
	FText ZenLaunchFailurePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_NonLocalProcessUsesDataDirPromptText", "ZenServer Failed to auto launch, an unknown process is locking the data folder '{0}'"), FText::FromString(DataPath));
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenLaunchFailurePromptText.ToString(), *ZenLaunchFailurePromptTitle.ToString());
}

static void
PromptUserOfFailedShutDownOfExistingProcess(uint16 Port)
{
	if (FApp::IsUnattended())
	{
		// Just log as there is no one to show a message
		UE_LOG(LogZenServiceInstance, Warning, TEXT("ZenServer Failed to auto launch, failed to shut down currently running service using port %u"), Port);
		FPlatformMisc::RequestExit(true);
		return;
	}

	FText ZenLaunchFailurePromptTitle = NSLOCTEXT("Zen", "Zen_ShutdownFailurePromptTitle", "Failed to launch");
	FText ZenLaunchFailurePromptText = FText::Format(NSLOCTEXT("Zen", "Zen_ShutdownFailurePromptText", "ZenServer Failed to auto launch, failed to shut down currently running service using port '{0}'"), Port);
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ZenLaunchFailurePromptText.ToString(), *ZenLaunchFailurePromptTitle.ToString());
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
		UE_LOG(LogZenServiceInstance, Display, TEXT("Installing service from '%s' to '%s'"), *InTreeServicePath, *InstallServicePath);
		if (!ShutdownRunningServiceUsingExecutablePath(*InstallServicePath))
		{
			PromptUserToStopRunningServerInstanceForUpdate(InstallServicePath);
			return FString();
		}

		// Even after waiting for the process to shut down we have a tolerance for failure when overwriting the target files
		if (!AttemptFileCopyWithRetries(*InstallServicePath, *InTreeServicePath, 5.0))
		{
			PromptUserToStopRunningServerInstanceForUpdate(InstallServicePath);
			return FString();
		}

		if (!AttemptFileCopyWithRetries(*InstallUtilityPath, *InTreeUtilityPath, 5.0))
		{
			PromptUserToStopRunningServerInstanceForUpdate(InstallServicePath);
			return FString();
		}

		AttemptFileCopyWithRetries(*InstallVersionCache, *InTreeVersionCache, 1.0);

		bMainExecutablesUpdated = true;
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
	IFileManager& FileManager = IFileManager::Get();
	const FString LockFilePath = FPaths::Combine(InSettings.DataPath, TEXT(".lock"));
	const FString ExecutionContextFilePath = FPaths::SetExtension(ExecutablePath, TEXT(".runcontext"));

	FString WorkingDirectory = FPaths::GetPath(ExecutablePath);

	LockFileData LockFileState;
	uint64 ZenWaitForRunningProcessReadyStartTime = FPlatformTime::Cycles64();
	while (IsZenProcessUsingDataDir(*LockFilePath, &LockFileState) && LockFileState.IsValid && !LockFileState.IsReady)
	{
		// Server is starting up, wait for it to get ready
		double ZenWaitDuration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - ZenWaitForRunningProcessReadyStartTime);
		if (ZenWaitDuration > 5.0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.1f);
		LockFileState = LockFileData();
	}

	uint16 ShutdownEffectivePort = 0;
	bool bLaunchNewInstance = true;

	if (LockFileState.IsReady)
	{
		const ZenServerState State;
		if (State.LookupByPid(LockFileState.ProcessId) == nullptr)
		{
			PromptUserOfLockedDataFolder(*InSettings.DataPath);
			FPlatformMisc::RequestExit(true);
			return false;
		}

		if (InSettings.bIsDefaultSharedRunContext)
		{
			FZenLocalServiceRunContext DesiredRunContext;
			DesiredRunContext.Executable = ExecutablePath;
			DesiredRunContext.CommandlineArguments = DetermineCmdLineWithoutTransientComponents(InSettings, InSettings.DesiredPort);
			DesiredRunContext.WorkingDirectory = WorkingDirectory;
			DesiredRunContext.DataPath = InSettings.DataPath;
			DesiredRunContext.bShowConsole = InSettings.bShowConsole;

			FZenLocalServiceRunContext CurrentRunContext;
			if (CurrentRunContext.ReadFromJsonFile(*ExecutionContextFilePath) && (DesiredRunContext == CurrentRunContext))
			{
				UE_LOG(LogZenServiceInstance, Log, TEXT("Found existing instance running on port %u matching our settings"), InSettings.DesiredPort);
				bLaunchNewInstance = false;
			}
			else
			{
				UE_LOG(LogZenServiceInstance, Log, TEXT("Found existing instance running on port %u with different run context, will attempt shut down"), InSettings.DesiredPort);
				ShutdownEffectivePort = LockFileState.EffectivePort;
			}
		}
		else
		{
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found existing instance running on port %u when not using shared context, will use it"), InSettings.DesiredPort);
		}
	}
	else
	{
		const ZenServerState State;
		const ZenServerState::ZenServerEntry* RunningEntry = State.LookupByDesiredListenPort(InSettings.DesiredPort);
		if (RunningEntry != nullptr)
		{
			ShutdownEffectivePort = RunningEntry->EffectiveListenPort;
			UE_LOG(LogZenServiceInstance, Log, TEXT("Found existing instance running on port %u with different data directory, will attempt shut down"), ShutdownEffectivePort);
		}
	}

	if (ShutdownEffectivePort != 0)
	{
		if (!ShutdownRunningServiceUsingEffectivePort(ShutdownEffectivePort))
		{
			PromptUserOfFailedShutDownOfExistingProcess(ShutdownEffectivePort);
			FPlatformMisc::RequestExit(true);
			return false;
		}
		checkf(!IsZenProcessUsingEffectivePort(ShutdownEffectivePort), TEXT("Service port is still in use %u"), ShutdownEffectivePort);

		if (IsLockFileLocked(*LockFilePath))
		{
			PromptUserOfLockedDataFolder(*InSettings.DataPath);
			FPlatformMisc::RequestExit(true);
			return false;
		}
	}

	// When limiting process lifetime, always re-launch to add sponsor process IDs.
	// When not limiting process lifetime, only launch if the process is not already live.
	if (bLaunchNewInstance)
	{
		FString ParmsWithoutTransients = DetermineCmdLineWithoutTransientComponents(InSettings, InSettings.DesiredPort);
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

		// Only write run context if we're using the default shared run context
		if (InSettings.bIsDefaultSharedRunContext)
		{
			EffectiveRunContext.WriteToJsonFile(*ExecutionContextFilePath);
		}

		if (!Proc.IsValid())
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed launch service using executable '%s' on port %u"), *ExecutablePath, InSettings.DesiredPort);
			return false;
		}
	}
	else if (InSettings.bLimitProcessLifetime)
	{
		ZenServerState State(/*ReadOnly*/ false);
		ZenServerState::ZenServerEntry* RunningEntry = State.LookupByDesiredListenPort(InSettings.DesiredPort);
		if (RunningEntry == nullptr)
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed attach as sponsor process to executable '%s' on port %u, can't find entry in shared state"), *ExecutablePath, InSettings.DesiredPort);
		}
		else if (!RunningEntry->AddSponsorProcess(FPlatformProcess::GetCurrentProcessId()))
		{
			UE_LOG(LogZenServiceInstance, Warning, TEXT("Failed attach as sponsor process to executable '%s' on port %u, to many sponsored processes attached already"), *ExecutablePath, InSettings.DesiredPort);
		}
	}

	OutHostName = TEXT("[::1]");
	// Default to assuming that we get to run on the port we want
	OutPort = InSettings.DesiredPort;

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
		LockFileData RunningLockFileState = ReadCbLockFile(LockFilePath);
		if (RunningLockFileState.IsValid)
		{
			bIsReady = RunningLockFileState.IsReady;
			if (bIsReady)
			{
				OutPort = RunningLockFileState.EffectivePort;
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
						// Just log as there is no one to show a message
						UE_LOG(LogZenServiceInstance, Warning, TEXT("ZenServer did not launch in the expected duration"));
						return false;
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
		Query << Separators[SeparatorIndex] << TEXT("smallobjects=") << LexToString(*OverrideCollectSmallObjects);
		SeparatorIndex = FMath::Min(SeparatorIndex + 1, (int32)UE_ARRAY_COUNT(Separators));
	}

	if (OverrideMaxCacheDuration)
	{
		Query << Separators[SeparatorIndex] << TEXT("maxcacheduration=") << LexToString(*OverrideMaxCacheDuration);
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

	const FString BaseName = TEXT("Zen_");

	{
		FString AttrName = BaseName + TEXT("Enabled");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.IsValid);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_HitRatio");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.HitRatio);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_Hits");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Hits);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_Misses");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Misses);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_Size_Disk");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Disk);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_Size_Memory");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Memory);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_UpstreamHits");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamHits);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_UpstreamRatio");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamRatio);
	}

	{
		FString AttrName = BaseName + TEXT("Cache_TotalUploadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
	}

	{
		FString AttrName = BaseName + TEXT("Upstream_TotalDownloadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalDownloadedMB);
	}

	{
		FString AttrName = BaseName + TEXT("Upstream_TotalUploadedMB");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
	}

	{
		FString AttrName = BaseName + TEXT("Cas_Size_Large");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Large);
	}

	{
		FString AttrName = BaseName + TEXT("Cas_Size_Small");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Small);
	}

	{
		FString AttrName = BaseName + TEXT("Cas_Size_Tiny");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Tiny);
	}

	{
		FString AttrName = BaseName + TEXT("Cas_Size_Total");
		Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Total);
	}

	return true;
}

#endif // UE_WITH_ZEN

}

