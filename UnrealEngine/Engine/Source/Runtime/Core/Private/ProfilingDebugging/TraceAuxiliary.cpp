// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/TraceAuxiliary.h"
#include "ProfilingDebugging/StringsTrace.h"
#include "Trace/Trace.h"
#include "CoreGlobals.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include <Windows.h>
#	include "Windows/HideWindowsPlatformTypes.h"
#endif

#if !defined(WITH_UNREAL_TRACE_LAUNCH)
#	define WITH_UNREAL_TRACE_LAUNCH (PLATFORM_DESKTOP && !UE_BUILD_SHIPPING && !IS_PROGRAM)
#endif

#if !defined(UE_TRACE_AUTOSTART)
	#define UE_TRACE_AUTOSTART 1
#endif

#if WITH_UNREAL_TRACE_LAUNCH
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#endif

#if UE_TRACE_ENABLED

#include <atomic>
#include "BuildSettings.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CString.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/Fork.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CallstackTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "ProfilingDebugging/PlatformEvents.h"
#include "String/ParseTokens.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Trace.inl"

////////////////////////////////////////////////////////////////////////////////
const TCHAR* GDefaultChannels = TEXT("cpu,gpu,frame,log,bookmark");
const TCHAR* GMemoryChannels = TEXT("memtag,memalloc,callstack,module");
const TCHAR* GTraceConfigSection = TEXT("Trace.Config");

////////////////////////////////////////////////////////////////////////////////
CSV_DEFINE_CATEGORY(Trace, true);

////////////////////////////////////////////////////////////////////////////////
DECLARE_STATS_GROUP(TEXT("TraceLog"), STATGROUP_Trace, STATCAT_Advanced);
DECLARE_MEMORY_STAT(TEXT("Memory used"), STAT_TraceMemoryUsed, STATGROUP_Trace);
DECLARE_MEMORY_STAT(TEXT("Important event cache"), STAT_TraceCacheAllocated, STATGROUP_Trace);
DECLARE_MEMORY_STAT(TEXT("Important event cache used"), STAT_TraceCacheUsed, STATGROUP_Trace);
DECLARE_MEMORY_STAT(TEXT("Important event cache waste"), STAT_TraceCacheWaste, STATGROUP_Trace);
DECLARE_MEMORY_STAT(TEXT("Sent"), STAT_TraceSent, STATGROUP_Trace);

////////////////////////////////////////////////////////////////////////////////
enum class ETraceConnectType
{
	Network,
	File
};

////////////////////////////////////////////////////////////////////////////////
class FTraceAuxiliaryImpl
{
public:
	const TCHAR*			GetDest() const;
	bool					IsConnected() const;
	void					GetActiveChannelsString(FStringBuilderBase& String) const;
	void					AddCommandlineChannels(const TCHAR* ChannelList);
	void					ResetCommandlineChannels();
	bool					HasCommandlineChannels() const { return !CommandlineChannels.IsEmpty(); }
	void					EnableChannels(const TCHAR* ChannelList);
	void					DisableChannels(const TCHAR* ChannelList);
	bool					Connect(ETraceConnectType Type, const TCHAR* Parameter, const FTraceAuxiliary::FLogCategoryAlias& LogCategory);
	bool					Stop();
	void					ResumeChannels();
	void					PauseChannels();
	void					EnableCommandlineChannels();
	void					EnableCommandlineChannelsPostInitialize();
	void					SetTruncateFile(bool bTruncateFile);
	void					UpdateCsvStats() const;
	void					StartWorkerThread();
	void					StartEndFramePump();
	bool					WriteSnapshot(const TCHAR* InFilePath, const FTraceAuxiliary::FLogCategoryAlias& LogCategory);

	// True if this is parent process with forking requested before forking.
	bool					IsParentProcessAndPreFork();

private:
	enum class EState : uint8
	{
		Stopped,
		Tracing,
	};

	struct FChannelEntry
	{
		FString				Name;
		bool				bActive = false;
	};

	void					AddChannel(const TCHAR* Name);
	void					RemoveChannel(const TCHAR* Name);
	template <class T> void ForEachChannel(const TCHAR* ChannelList, bool bResolvePresets, T Callable);
	static uint32			HashChannelName(const TCHAR* Name);
	bool					EnableChannel(const TCHAR* Channel);
	void					DisableChannel(const TCHAR* Channel);
	bool					SendToHost(const TCHAR* Host, const FTraceAuxiliary::FLogCategoryAlias& LogCategory);
	bool					WriteToFile(const TCHAR* Path, const FTraceAuxiliary::FLogCategoryAlias& LogCategory);
	bool					FinalizeFilePath(const TCHAR* InPath, FString& OutPath, const FTraceAuxiliary::FLogCategoryAlias& LogCategory);

	typedef TMap<uint32, FChannelEntry, TInlineSetAllocator<128>> ChannelSet;
	ChannelSet				CommandlineChannels;
	FString					TraceDest;
	FTraceAuxiliary::EConnectionType TraceType = FTraceAuxiliary::EConnectionType::None;
	EState					State = EState::Stopped;
	bool					bTruncateFile = false;
	bool					bWorkerThreadStarted = false;
	FString					PausedPreset;
};

static FTraceAuxiliaryImpl GTraceAuxiliary;
static FDelegateHandle GEndFrameDelegateHandle;
static FDelegateHandle GEndFrameStatDelegateHandle;
static FDelegateHandle GOnPostForkHandle;

// Whether to start tracing automatically at start or wait to initiate via Console Command.
// This value can also be set by passing '-traceautostart=[0|1]' on command line.
static bool GTraceAutoStart = UE_TRACE_AUTOSTART ? true : false;

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::IsParentProcessAndPreFork()
{
	return FForkProcessHelper::IsForkRequested() && !FForkProcessHelper::IsForkedChildProcess();
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::AddCommandlineChannels(const TCHAR* ChannelList)
{
	ForEachChannel(ChannelList, true, &FTraceAuxiliaryImpl::AddChannel);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::ResetCommandlineChannels()
{
	CommandlineChannels.Reset();
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::EnableChannels(const TCHAR* ChannelList)
{
	if (ChannelList)
	{
		ForEachChannel(ChannelList, true, &FTraceAuxiliaryImpl::EnableChannel);
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::DisableChannels(const TCHAR* ChannelList)
{
	if (ChannelList)
	{
		ForEachChannel(ChannelList, true, &FTraceAuxiliaryImpl::DisableChannel);
	}
	else
	{
		// Disable all channels.
		TStringBuilder<128> EnabledChannels;
		GetActiveChannelsString(EnabledChannels);
		ForEachChannel(EnabledChannels.ToString(), true, &FTraceAuxiliaryImpl::DisableChannel);
	}
}

////////////////////////////////////////////////////////////////////////////////
template<typename T>
void FTraceAuxiliaryImpl::ForEachChannel(const TCHAR* ChannelList, bool bResolvePresets, T Callable)
{
	check(ChannelList);
	UE::String::ParseTokens(ChannelList, TEXT(","), [this, bResolvePresets, Callable] (const FStringView& Token)
	{
		TCHAR Name[80];
		const size_t ChannelNameSize = Token.CopyString(Name, UE_ARRAY_COUNT(Name) - 1);
		Name[ChannelNameSize] = '\0';

		if (bResolvePresets)
		{
			FString Value;
			// Check against hard coded presets
			if(FCString::Stricmp(Name, TEXT("default")) == 0)
			{
				ForEachChannel(GDefaultChannels, false, Callable);
			}
			else if(FCString::Stricmp(Name, TEXT("memory"))== 0)
			{
				ForEachChannel(GMemoryChannels, false, Callable);
			}
			// Check against data driven presets (if available)
			else if (GConfig && GConfig->GetString(TEXT("Trace.ChannelPresets"), Name, Value, GEngineIni))
			{
				ForEachChannel(*Value, false, Callable);
				return;
			}
		}

		Invoke(Callable, this, Name);
	});
}

////////////////////////////////////////////////////////////////////////////////
uint32 FTraceAuxiliaryImpl::HashChannelName(const TCHAR* Name)
{
	uint32 Hash = 5381;
	for (const TCHAR* c = Name; *c; ++c)
	{
		uint32 LowerC = *c | 0x20;
		Hash = ((Hash << 5) + Hash) + LowerC;
	}
	return Hash;
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::AddChannel(const TCHAR* Name)
{
	uint32 Hash = HashChannelName(Name);

	if (CommandlineChannels.Find(Hash) != nullptr)
	{
		return;
	}

	FChannelEntry& Value = CommandlineChannels.Add(Hash, {});
	Value.Name = Name;

	if (State >= EState::Tracing && !Value.bActive)
	{
		Value.bActive = EnableChannel(*Value.Name);
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::RemoveChannel(const TCHAR* Name)
{
	uint32 Hash = HashChannelName(Name);

	FChannelEntry Channel;
	if (!CommandlineChannels.RemoveAndCopyValue(Hash, Channel))
	{
		return;
	}

	if (State >= EState::Tracing && Channel.bActive)
	{
		DisableChannel(*Channel.Name);
		Channel.bActive = false;
	}
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::Connect(ETraceConnectType Type, const TCHAR* Parameter, const FTraceAuxiliary::FLogCategoryAlias& LogCategory)
{
	// Connect/write to file, but only if we're not already sending/writing.
	bool bConnected = UE::Trace::IsTracing();
	if (!bConnected)
	{
		if (Type == ETraceConnectType::Network)
		{
			bConnected = SendToHost(Parameter, LogCategory);
			if (bConnected)
			{
				UE_LOG_REF(LogCategory, Display, TEXT("Trace started (connected to trace server %s)."), GetDest());
			}
			else
			{
				UE_LOG_REF(LogCategory, Error, TEXT("Trace failed to connect (trace server: %s)!"), Parameter ? Parameter : TEXT(""));
			}

			TraceType = FTraceAuxiliary::EConnectionType::Network;
		}

		else if (Type == ETraceConnectType::File)
		{
			bConnected = WriteToFile(Parameter, LogCategory);
			if (bConnected)
			{
				UE_LOG_REF(LogCategory, Display, TEXT("Trace started (writing to file \"%s\")."), GetDest());
			}
			else
			{
				UE_LOG_REF(LogCategory, Error, TEXT("Trace failed to connect (file: \"%s\")!"), Parameter ? Parameter : TEXT(""));
			}

			TraceType = FTraceAuxiliary::EConnectionType::File;
		}

		if (bConnected)
		{
			FTraceAuxiliary::OnTraceStarted.Broadcast(TraceType, TraceDest);
		}
	}

	if (!bConnected)
	{
		return false;
	}

	State = EState::Tracing;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::Stop()
{
	if (IsParentProcessAndPreFork())
	{
		return false;
	}

	if (!UE::Trace::Stop())
	{
		return false;
	}

	FTraceAuxiliary::OnTraceStopped.Broadcast(TraceType, TraceDest);

	State = EState::Stopped;
	TraceType = FTraceAuxiliary::EConnectionType::None;
	TraceDest.Reset();
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::EnableChannel(const TCHAR* Channel)
{
	// Channel names have been provided by the user and may not exist yet. As
	// we want to maintain bActive accurately (channels toggles are reference
	// counted), we will first check Trace knows of the channel.
	if (!UE::Trace::IsChannel(Channel))
	{
		return false;
	}

	EPlatformEvent Event = PlatformEvents_GetEvent(Channel);
	if (Event != EPlatformEvent::None)
	{
		PlatformEvents_Enable(Event);
	}

	return UE::Trace::ToggleChannel(Channel, true);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::DisableChannel(const TCHAR* Channel)
{
	// Channel names have been provided by the user and may not exist yet. As
	// we want to maintain bActive accurately (channels toggles are reference
	// counted), we will first check Trace knows of the channel.
	if (!UE::Trace::IsChannel(Channel))
	{
		return;
	}

	EPlatformEvent Event = PlatformEvents_GetEvent(Channel);
	if (Event != EPlatformEvent::None)
	{
		PlatformEvents_Disable(Event);
	}

	UE::Trace::ToggleChannel(Channel, false);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::ResumeChannels()
{
	// Enable channels from the "paused" preset.
	ForEachChannel(*PausedPreset, false, &FTraceAuxiliaryImpl::EnableChannel);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::PauseChannels()
{
	TStringBuilder<128> EnabledChannels;
	GetActiveChannelsString(EnabledChannels);

	// Save the list of enabled channels as the current "paused" preset.
	// The "paused" preset can only be used in the Trace.Resume command / API.
	PausedPreset = EnabledChannels.ToString();

	// Disable all "paused" channels.
	ForEachChannel(*PausedPreset, true, &FTraceAuxiliaryImpl::DisableChannel);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::EnableCommandlineChannels()
{
	if (IsParentProcessAndPreFork())
	{
		return;
	}

	for (auto& ChannelPair : CommandlineChannels)
	{
		if (!ChannelPair.Value.bActive)
		{
			ChannelPair.Value.bActive = EnableChannel(*ChannelPair.Value.Name);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::EnableCommandlineChannelsPostInitialize()
{
	for (auto& ChannelPair : CommandlineChannels)
	{
		// Intentionally enable channel without checking current state.
		ChannelPair.Value.bActive = EnableChannel(*ChannelPair.Value.Name);
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::SetTruncateFile(bool bNewTruncateFileState)
{
	bTruncateFile = bNewTruncateFileState;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::SendToHost(const TCHAR* Host, const FTraceAuxiliary::FLogCategoryAlias& LogCategory)
{
	if (!UE::Trace::SendTo(Host))
	{
		UE_LOG_REF(LogCategory, Warning, TEXT("Unable to trace to host '%s'"), Host);
		return false;
	}

	TraceDest = Host;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::FinalizeFilePath(const TCHAR* InPath, FString& OutPath, const FTraceAuxiliary::FLogCategoryAlias& LogCategory)
{
	const FStringView Path(InPath);

	// Default file name functor
	auto GetDefaultName = [] { return FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S.utrace")); };

	if (Path.IsEmpty())
	{
		const FString Name = GetDefaultName();
		return FinalizeFilePath(*Name, OutPath, LogCategory);
	}

	FString WritePath;
	// Relative paths go to the profiling directory
	if (FPathViews::IsRelativePath(Path))
	{
		WritePath = FPaths::Combine(FPaths::ProfilingDir(), InPath);
	}
#if PLATFORM_WINDOWS
	// On windows we treat paths starting with '/' as relative, except double slash which is a network path
	else if (FPathViews::IsSeparator(Path[0]) && !(Path.Len() > 1 && FPathViews::IsSeparator(Path[1])))
	{
		WritePath = FPaths::Combine(FPaths::ProfilingDir(), InPath);
	}
#endif
	else
	{
		WritePath = InPath;
	}

	// If a directory is specified, add the default trace file name
	if (FPathViews::GetCleanFilename(WritePath).IsEmpty())
	{
		WritePath = FPaths::Combine(WritePath, GetDefaultName());
	}

	// The user may not have provided a suitable extension
	if (FPathViews::GetExtension(WritePath) != TEXT("utrace"))
	{
		WritePath = FPaths::SetExtension(WritePath, TEXT(".utrace"));
	}

	// Finally make sure the path is platform friendly
	IFileManager& FileManager = IFileManager::Get();
	FString NativePath = FileManager.ConvertToAbsolutePathForExternalAppForWrite(*WritePath);

	// Ensure we can write the trace file appropriately
	const FString WriteDir = FPaths::GetPath(NativePath);
	if (!FPaths::IsDrive(WriteDir))
	{
		if (!FileManager.MakeDirectory(*WriteDir, true))
		{
			UE_LOG_REF(LogCategory, Warning, TEXT("Failed to create directory '%s'"), *WriteDir);
			return false;
		}
	}

	if (!bTruncateFile && FileManager.FileExists(*NativePath))
	{
		UE_LOG_REF(LogCategory, Warning, TEXT("Trace file '%s' already exists"), *NativePath);
		return false;
	}

	OutPath = MoveTemp(NativePath);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::WriteToFile(const TCHAR* Path, const FTraceAuxiliary::FLogCategoryAlias& LogCategory)
{
	FString NativePath;
	if (!FinalizeFilePath(Path, NativePath, LogCategory))
	{
		return false;
	}

	if (!UE::Trace::WriteTo(*NativePath))
	{
		if (FPathViews::Equals(NativePath, FStringView(Path)))
		{
			UE_LOG_REF(LogCategory, Warning, TEXT("Unable to trace to file '%s'"), *NativePath);
		}
		else
		{
			UE_LOG_REF(LogCategory, Warning, TEXT("Unable to trace to file '%s' (transformed from '%s')"), *NativePath, Path ? Path : TEXT("null"));
		}
		return false;
	}

	TraceDest = MoveTemp(NativePath);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::WriteSnapshot(const TCHAR* InFilePath, const FTraceAuxiliary::FLogCategoryAlias& LogCategory)
{
	double StartTime = FPlatformTime::Seconds();

	FString NativePath;
	if (!FinalizeFilePath(InFilePath, NativePath, LogCategory))
	{
		return false;
	}

	UE_LOG_REF(LogCategory, Log, TEXT("Writing trace snapshot to '%s'..."), *NativePath);

	const bool bResult = UE::Trace::WriteSnapshotTo(*NativePath);

	if (bResult)
	{
		UE_LOG_REF(LogCategory, Display, TEXT("Trace snapshot generated in %.3f seconds to \"%s\"."), FPlatformTime::Seconds() - StartTime, *NativePath);
	}
	else
	{
		UE_LOG_REF(LogCategory, Error, TEXT("Failed to trace snapshot to \"%s\"."), *NativePath);
	}

	return bResult;
}

////////////////////////////////////////////////////////////////////////////////
const TCHAR* FTraceAuxiliaryImpl::GetDest() const
{
	return *TraceDest;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliaryImpl::IsConnected() const
{
	return State == EState::Tracing;
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::GetActiveChannelsString(FStringBuilderBase& String) const
{
	UE::Trace::EnumerateChannels([](const ANSICHAR* Name, bool bEnabled, void* User)
	{
		FStringBuilderBase& EnabledChannelsStr = *static_cast<FStringBuilderBase*>(User);
		if (bEnabled)
		{
			FAnsiStringView NameView = FAnsiStringView(Name).LeftChop(7); // Remove "Channel" suffix
			EnabledChannelsStr << NameView << TEXT(",");
		}
	}, &String);
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::UpdateCsvStats() const
{
#if TRACE_PRIVATE_STATISTICS
	// Only publish CSV stats if we have ever run tracing in order to reduce overhead in most runs.
	static bool bDoStats = false;
	if (UE::Trace::IsTracing() || bDoStats)
	{
		bDoStats = true;

		UE::Trace::FStatistics Stats;
		UE::Trace::GetStatistics(Stats);

		CSV_CUSTOM_STAT(Trace, MemoryUsedMb,	double(Stats.MemoryUsed) / 1024.0 / 1024.0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Trace, CacheUsedMb,		double(Stats.CacheUsed) / 1024.0 / 1024.0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(Trace, CacheWasteMb,	double(Stats.CacheWaste) / 1024.0 / 1024.0, ECsvCustomStatOp::Set);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::StartWorkerThread()
{
	if (!bWorkerThreadStarted)
	{
		UE::Trace::StartWorkerThread();
		bWorkerThreadStarted = true;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliaryImpl::StartEndFramePump()
{
	if (!GEndFrameDelegateHandle.IsValid())
	{
		// If the worker thread is disabled, pump the update from end frame
		GEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddStatic(UE::Trace::Update);
	}
	if (!GEndFrameStatDelegateHandle.IsValid())
	{
		// Update stats every frame
		GEndFrameStatDelegateHandle = FCoreDelegates::OnEndFrame.AddLambda([]()
		{
			UE::Trace::FStatistics Stats;
			UE::Trace::GetStatistics(Stats);
			SET_MEMORY_STAT(STAT_TraceMemoryUsed, Stats.MemoryUsed);
			SET_MEMORY_STAT(STAT_TraceCacheAllocated, Stats.CacheAllocated);
			SET_MEMORY_STAT(STAT_TraceCacheUsed, Stats.CacheUsed);
			SET_MEMORY_STAT(STAT_TraceCacheWaste, Stats.CacheWaste);
			SET_MEMORY_STAT(STAT_TraceSent, Stats.BytesSent);
		});
	}
}

////////////////////////////////////////////////////////////////////////////////
void OnConnectionCallback()
{
	FTraceAuxiliary::OnConnection.Broadcast();
}

////////////////////////////////////////////////////////////////////////////////
static void SetupInitFromConfig(UE::Trace::FInitializeDesc& OutDesc)
{
	if (!GConfig)
	{
		return;
	}

	int32 SleepTimeConfig = 0;
	if (GConfig->GetInt(GTraceConfigSection, TEXT("SleepTimeInMS"), SleepTimeConfig, GEngineIni))
	{
		if (SleepTimeConfig > 0)
		{
			OutDesc.ThreadSleepTimeInMS = SleepTimeConfig;
		}
	}

	int32 TailSizeBytesConfig = 0;
	if (GConfig->GetInt(GTraceConfigSection, TEXT("TailSizeBytes"), TailSizeBytesConfig, GEngineIni))
	{
		OutDesc.TailSizeBytes = TailSizeBytesConfig;
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryConnectEpilogue()
{
	// It is possible that something outside of TraceAux's world view has called
	// UE::Trace::SendTo/WriteTo(). A plugin that has created its own store for
	// example. There's not really much that can be done about that here (tracing
	// is singular within a process. We can at least detect the obvious case and
	// inform the user.
	const TCHAR* TraceDest = GTraceAuxiliary.GetDest();
	if (TraceDest[0] == '\0')
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Trace system already in use by a plugin or -trace*=... argument. Use 'Trace.Stop' first."));
		return;
	}

	// Give the user some feedback that everything's underway.
	TStringBuilder<128> Channels;
	GTraceAuxiliary.GetActiveChannelsString(Channels);

	UE_LOG(LogConsoleResponse, Display, TEXT("Enabled channels: %s"), Channels.ToString());
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliarySend(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("No host name given. Usage: Trace.Send <Host> [ChannelSet]"));
		return;
	}

	const TCHAR* Target = *Args[0];
	const TCHAR* Channels = Args.Num() > 1 ? *Args[1] : nullptr;
	if (FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, Target, Channels, nullptr, LogConsoleResponse))
	{
		TraceAuxiliaryConnectEpilogue();
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryFile(const TArray<FString>& Args)
{
	const TCHAR* Filepath = nullptr;
	const TCHAR* Channels = nullptr;

	if (Args.Num() == 2)
	{
		Filepath = *Args[0];
		Channels = *Args[1];
	}
	else if (Args.Num() == 1)
	{
		// Try to detect if the first argument is a file path.
		if (FCString::Strchr(*Args[0], TEXT('/')) ||
			FCString::Strchr(*Args[0], TEXT('\\')) ||
			FCString::Strchr(*Args[0], TEXT('.')) ||
			FCString::Strchr(*Args[0], TEXT(':')))
		{
			Filepath = *Args[0];
		}
		else
		{
			Channels = *Args[0];
		}
	}
	else if (Args.Num() > 2)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Invalid arguments. Usage: Trace.File [Path] [ChannelSet]"));
		return;
	}

	if (FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, Filepath, Channels, nullptr, LogConsoleResponse))
	{
		TraceAuxiliaryConnectEpilogue();
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryStart(const TArray<FString>& Args)
{
	UE_LOG(LogConsoleResponse, Warning, TEXT("'Trace.Start' is being deprecated in favor of 'Trace.File'."));
	TraceAuxiliaryFile(Args);
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryStop()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Tracing stopped."));
	GTraceAuxiliary.Stop();
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryPause()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Tracing paused."));
	GTraceAuxiliary.PauseChannels();
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryResume()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Tracing resumed."));
	GTraceAuxiliary.ResumeChannels();
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryStatus()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Trace status ----------------------------------------------------------"));

	// Status of data connection
	TStringBuilder<256> ConnectionStr;
	if (UE::Trace::IsTracing())
	{
		const TCHAR* Dest = GTraceAuxiliary.GetDest();
		if (Dest && FCString::Strlen(Dest) > 0)
		{
			ConnectionStr.Appendf(TEXT("Tracing to '%s'"), Dest);
		}
		else
		{
			// If GTraceAux doesn't know about the target but we are still tracing this is an externally initiated connection
			// (e.g. connection command from Insights).
			ConnectionStr = TEXT("Tracing to unknown target (externally set)");
		}
	}
	else
	{
		ConnectionStr = TEXT("Not tracing");
	}
	UE_LOG(LogConsoleResponse, Display, TEXT("- Connection: %s"), ConnectionStr.ToString());

	// Stats
	UE::Trace::FStatistics Stats;
	UE::Trace::GetStatistics(Stats);
	constexpr double MiB = 1.0 / (1024.0 * 1024.0);
	UE_LOG(LogConsoleResponse, Display, TEXT("- Memory used: %.02f MiB"),
		double(Stats.MemoryUsed) * MiB);
	UE_LOG(LogConsoleResponse, Display, TEXT("- Important Events cache: %.02f MiB (%.02f MiB used + %0.02f MiB unused | %0.02f MiB waste)"),
		double(Stats.CacheAllocated) * MiB,
		double(Stats.CacheUsed) * MiB,
		double(Stats.CacheAllocated - Stats.CacheUsed) * MiB,
		double(Stats.CacheWaste) * MiB);
	UE_LOG(LogConsoleResponse, Display, TEXT("- Sent: %.02f MiB"),
		double(Stats.BytesSent) * MiB);

	// Channels
	struct EnumerateType
	{
		TStringBuilder<512> ChannelsStr;
		uint32 Count = 0;
#if WITH_EDITOR
		uint32 LineLen = 50;
#else
		uint32 LineLen = 20;
#endif

		void AddChannel(const FAnsiStringView& NameView)
		{
			if (Count++ > 0)
			{
				ChannelsStr << TEXT(", ");
				LineLen += 2;
			}
			if (LineLen + NameView.Len() > 100)
			{
				ChannelsStr << TEXT("\n    ");
				LineLen = 4;
			}
			ChannelsStr << NameView;
			LineLen += NameView.Len();
		}
	} EnumerateUserData[2];
	UE::Trace::EnumerateChannels([](const ANSICHAR* Name, bool bEnabled, void* User)
	{
		EnumerateType* EnumerateUser = static_cast<EnumerateType*>(User);
		FAnsiStringView NameView = FAnsiStringView(Name).LeftChop(7); // Remove "Channel" suffix
		EnumerateUser[bEnabled ? 0 : 1].AddChannel(NameView);
	}, EnumerateUserData);
	UE_LOG(LogConsoleResponse, Display, TEXT("- Enabled channels: %s"), EnumerateUserData[0].Count == 0 ? TEXT("<none>") : EnumerateUserData[0].ChannelsStr.ToString());
	UE_LOG(LogConsoleResponse, Display, TEXT("- Available channels: %s"), EnumerateUserData[1].ChannelsStr.ToString());

	UE_LOG(LogConsoleResponse, Display, TEXT("-----------------------------------------------------------------------"));
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryEnableChannels(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Need to provide at least one channel."));
		return;
	}
	GTraceAuxiliary.EnableChannels(*Args[0]);

	TStringBuilder<128> EnabledChannels;
	GTraceAuxiliary.GetActiveChannelsString(EnabledChannels);
	UE_LOG(LogConsoleResponse, Display, TEXT("Enabled channels: %s"), EnabledChannels.ToString());
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliaryDisableChannels(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		GTraceAuxiliary.DisableChannels(nullptr);
	}
	else
	{
		GTraceAuxiliary.DisableChannels(*Args[0]);
	}

	TStringBuilder<128> EnabledChannels;
	GTraceAuxiliary.GetActiveChannelsString(EnabledChannels);
	UE_LOG(LogConsoleResponse, Display, TEXT("Enabled channels: %s"), EnabledChannels.ToString());
}

////////////////////////////////////////////////////////////////////////////////
static void TraceAuxiliarySnapshotFile(const TArray<FString>& Args)
{
	const TCHAR* FilePath = nullptr;

	if (Args.Num() == 1)
	{
		FilePath = *Args[0];
	}
	else if (Args.Num() > 1)
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Invalid arguments. Usage: Trace.SnapshotFile [Path]"));
		return;
	}

	GTraceAuxiliary.WriteSnapshot(FilePath, LogConsoleResponse);
}

////////////////////////////////////////////////////////////////////////////////
static void TraceBookmark(const TArray<FString>& Args)
{
	TRACE_BOOKMARK(TEXT("%s"), Args.Num() ? *Args[0] : TEXT(""));
}

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliarySendCmd(
	TEXT("Trace.Send"),
	TEXT("<Host> [ChannelSet] - Starts tracing to a trace store."
		" <Host> is the IP address or hostname of the trace store."
		" ChannelSet is comma-separated list of trace channels/presets to be enabled."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliarySend)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryStartCmd(
	TEXT("Trace.Start"),
	TEXT("[ChannelSet] - (Deprecated: Use Trace.File instead.) Starts tracing to a file."
		" ChannelSet is comma-separated list of trace channels/presets to be enabled."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliaryStart)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryFileCmd(
	TEXT("Trace.File"),
	TEXT("[Path] [ChannelSet] - Starts tracing to a file."
		" ChannelSet is comma-separated list of trace channels/presets to be enabled."
		" Either Path or ChannelSet can be excluded."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliaryFile)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryStopCmd(
	TEXT("Trace.Stop"),
	TEXT("Stops tracing profiling events."),
	FConsoleCommandDelegate::CreateStatic(TraceAuxiliaryStop)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryPauseCmd(
	TEXT("Trace.Pause"),
	TEXT("Pauses all trace channels currently sending events."),
	FConsoleCommandDelegate::CreateStatic(TraceAuxiliaryPause)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryResumeCmd(
	TEXT("Trace.Resume"),
	TEXT("Resumes tracing that was previously paused (re-enables the paused channels)."),
	FConsoleCommandDelegate::CreateStatic(TraceAuxiliaryResume)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryStatusCmd(
	TEXT("Trace.Status"),
	TEXT("Prints Trace status to console."),
	FConsoleCommandDelegate::CreateStatic(TraceAuxiliaryStatus)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryChannelEnableCmd(
	TEXT("Trace.Enable"),
	TEXT("[ChannelSet] - Enables a set of channels."
		" ChannelSet is comma-separated list of trace channels/presets to be enabled."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliaryEnableChannels)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliaryChannelDisableCmd(
	TEXT("Trace.Disable"),
	TEXT("[ChannelSet] - Disables a set of channels."
		" ChannelSet is comma-separated list of trace channels/presets to be disabled."
		" If no channel is specified, it disables all channels."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliaryDisableChannels)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceAuxiliarySnapshotFileCmd(
	TEXT("Trace.SnapshotFile"),
	TEXT("[Path] - Writes a snapshot of the current in-memory trace buffer to a file."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceAuxiliarySnapshotFile)
);

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand TraceBookmarkCmd(
	TEXT("Trace.Bookmark"),
	TEXT("[Name] - Emits a TRACE_BOOKMARK() event with the given string name."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceBookmark)
);

#endif // UE_TRACE_ENABLED



#if WITH_UNREAL_TRACE_LAUNCH
////////////////////////////////////////////////////////////////////////////////
static std::atomic<int32> GUnrealTraceLaunched; // = 0;

////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_WINDOWS
static void LaunchUnrealTraceInternal(const TCHAR* CommandLine)
{
	if (GUnrealTraceLaunched.load(std::memory_order_relaxed))
	{
		UE_LOG(LogCore, Log, TEXT("UnrealTraceServer: Trace store already started"));
		return;
	}

	TWideStringBuilder<MAX_PATH + 32> CreateProcArgs;
	CreateProcArgs << "\"";
	CreateProcArgs << FPaths::EngineDir();
	CreateProcArgs << TEXT("/Binaries/Win64/UnrealTraceServer.exe\"");
	CreateProcArgs << TEXT(" fork");

	uint32 CreateProcFlags = CREATE_BREAKAWAY_FROM_JOB;
	if (FParse::Param(CommandLine, TEXT("traceshowstore")))
	{
		CreateProcFlags |= CREATE_NEW_CONSOLE;
	}
	else
	{
		CreateProcFlags |= CREATE_NO_WINDOW;
	}
	STARTUPINFOW StartupInfo = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION ProcessInfo = {};
	BOOL bOk = CreateProcessW(nullptr, LPWSTR(*CreateProcArgs), nullptr, nullptr,
		false, CreateProcFlags, nullptr, nullptr, &StartupInfo, &ProcessInfo);

	if (!bOk)
	{
		UE_LOG(LogCore, Display, TEXT("UnrealTraceServer: Unable to launch the trace store with '%s' (%08x)"), *CreateProcArgs, GetLastError());
		return;
	}

	if (WaitForSingleObject(ProcessInfo.hProcess, 5000) == WAIT_TIMEOUT)
	{
		UE_LOG(LogCore, Warning, TEXT("UnrealTraceServer: Timed out waiting for the trace store to start"));
	}
	else
	{
		DWORD ExitCode = 0x0000'a9e0;
		GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
		if (ExitCode)
		{
			UE_LOG(LogCore, Warning, TEXT("UnrealTraceServer: Trace store returned an error (0x%08x)"), ExitCode);
		}
		else
		{
			UE_LOG(LogCore, Log, TEXT("UnrealTraceServer: Trace store launch successful"));
			GUnrealTraceLaunched.fetch_add(1, std::memory_order_relaxed);
		}
	}

	CloseHandle(ProcessInfo.hProcess);
	CloseHandle(ProcessInfo.hThread);
}
#endif // PLATFORM_WINDOWS

////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_UNIX || PLATFORM_MAC
static void LaunchUnrealTraceInternal(const TCHAR* CommandLine)
{
	/* nop */

#if 0
	if (GUnrealTraceLaunched.load(std::memory_order_relaxed))
	{
		UE_LOG(LogCore, Log, TEXT("UnrealTraceServer: Trace store already started"));
		return;
	}

	TAnsiStringBuilder<320> BinPath;
	BinPath << TCHAR_TO_UTF8(*FPaths::EngineDir());
#if PLATFORM_UNIX
	BinPath << "Binaries/Linux/UnrealTraceServer";
#elif PLATFORM_MAC
	BinPath << "Binaries/Mac/UnrealTraceServer";
#endif

	if (access(*BinPath, F_OK) < 0)
	{
		UE_LOG(LogCore, Display, TEXT("UnrealTraceServer: Binary not found (%s)"), ANSI_TO_TCHAR(*BinPath));
		return;
	}

	TAnsiStringBuilder<64> ForkArg;
	ForkArg << "fork";

	pid_t UtsPid = vfork();
	if (UtsPid < 0)
	{
		UE_LOG(LogCore, Display, TEXT("UnrealTraceServer: Unable to fork (errno: %d)"), errno);
		return;
	}
	else if (UtsPid == 0)
	{
		char* Args[] = { BinPath.GetData(), ForkArg.GetData(), nullptr };
		extern char** environ;
		execve(*BinPath, Args, environ);
		_exit(0x80 | (errno & 0x7f));
	}

	int32 WaitStatus = 0;
	do
	{
		int32 WaitRet = waitpid(UtsPid, &WaitStatus, 0);
		if (WaitRet < 0)
		{
			UE_LOG(LogCore, Display, TEXT("UnrealTraceServer: waitpid() error; (errno: %d)"), errno);
			return;
		}
	}
	while (!WIFEXITED(WaitStatus));

	int32 UtsRet = WEXITSTATUS(WaitStatus);
	if (UtsRet)
	{
		UE_LOG(LogCore, Display, TEXT("UnrealTraceServer: Trace store returned an error (0x%08x)"), UtsRet);
	}
	else
	{
		UE_LOG(LogCore, Log, TEXT("UnrealTraceServer: Trace store launch successful"));
		GUnrealTraceLaunched.fetch_add(1, std::memory_order_relaxed);
	}
#endif // 0
}
#endif // PLATFORM_UNIX/MAC
#endif // WITH_UNREAL_TRACE_LAUNCH



////////////////////////////////////////////////////////////////////////////////
UE_TRACE_EVENT_BEGIN(Diagnostics, Session2, NoSync|Important)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Platform)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, AppName)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, CommandLine)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Branch)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, BuildVersion)
	UE_TRACE_EVENT_FIELD(uint32, Changelist)
	UE_TRACE_EVENT_FIELD(uint8, ConfigurationType)
	UE_TRACE_EVENT_FIELD(uint8, TargetType)
UE_TRACE_EVENT_END()

////////////////////////////////////////////////////////////////////////////////
static bool StartFromCommandlineArguments(const TCHAR* CommandLine, bool& bOutStartWorkerThread)
{
#if UE_TRACE_ENABLED

	// Get active channels
	FString Channels;
	if (FParse::Value(CommandLine, TEXT("-trace="), Channels, false))
	{
	}
	else if (FParse::Param(CommandLine, TEXT("trace")))
	{
		Channels = GDefaultChannels;
	}
#if WITH_EDITOR
	else
	{
		Channels = GDefaultChannels;
	}
#endif

	// By default, if any channels are enabled we trace to memory.
	FTraceAuxiliary::EConnectionType Type = FTraceAuxiliary::EConnectionType::None;

	// Setup options
	FTraceAuxiliary::Options Opts;
	Opts.bTruncateFile = FParse::Param(CommandLine, TEXT("tracefiletrunc"));

	const bool bWorkerThreadAllowed = FGenericPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance();

	if (!bWorkerThreadAllowed || FParse::Param(FCommandLine::Get(), TEXT("notracethreading")))
	{
		Opts.bNoWorkerThread = true;
	}

	// Find if a connection type is specified
	FString Parameter;
	const TCHAR* Target = nullptr;
	if (FParse::Value(CommandLine, TEXT("-tracehost="), Parameter))
	{
		Type = FTraceAuxiliary::EConnectionType::Network;
		Target = *Parameter;
	}
	else if (FParse::Value(CommandLine, TEXT("-tracefile="), Parameter))
	{
		Type = FTraceAuxiliary::EConnectionType::File;
		if (Parameter.IsEmpty())
		{
			UE_LOG(LogCore, Warning, TEXT("Empty parameter to 'tracefile' argument. Using default filename."));
			Target = nullptr;
		}
		else
		{
			Target = *Parameter;
		}
	}
	else if (FParse::Param(CommandLine, TEXT("tracefile")))
	{
		Type = FTraceAuxiliary::EConnectionType::File;
		Target = nullptr;
	}

	// If user has defined a connection type but not specified channels, use the default channel set.
	if (Type != FTraceAuxiliary::EConnectionType::None && Channels.IsEmpty())
	{
		Channels = GDefaultChannels;
	}

	if (Channels.IsEmpty())
	{
		return false;
	}

	if (!GTraceAutoStart)
	{
		GTraceAuxiliary.AddCommandlineChannels(*Channels);
		return false;
	}

	// Trace's worker thread should really only be started by Trace itself as
	// order is important. At the very least it must be done after Trace is
	// initialised. It isn't yet here so we defer it.
	bOutStartWorkerThread = !Opts.bNoWorkerThread;
	Opts.bNoWorkerThread = true;

	// Finally start tracing to the requested connection
	return FTraceAuxiliary::Start(Type, Target, *Channels, &Opts);
#endif
	return false;
}

////////////////////////////////////////////////////////////////////////////////
FTraceAuxiliary::FOnConnection FTraceAuxiliary::OnConnection;
FTraceAuxiliary::FOnTraceStarted FTraceAuxiliary::OnTraceStarted;
FTraceAuxiliary::FOnTraceStopped FTraceAuxiliary::OnTraceStopped;

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliary::Start(EConnectionType Type, const TCHAR* Target, const TCHAR* Channels, Options* Options, const FLogCategoryAlias& LogCategory)
{
#if UE_TRACE_ENABLED
	if (GTraceAuxiliary.IsParentProcessAndPreFork())
	{
		return false;
	}

	if (GTraceAuxiliary.IsConnected())
	{
		UE_LOG_REF(LogCategory, Error, TEXT("Unable to start trace, already tracing to %s"), GTraceAuxiliary.GetDest());
		return false;
	}

	// Make sure the worker thread is started unless explicitly opt out.
	if (!Options || !Options->bNoWorkerThread)
	{
		if (FGenericPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance())
		{
			GTraceAuxiliary.StartWorkerThread();
		}
	}

	if (Channels)
	{
		UE_LOG_REF(LogCategory, Display, TEXT("Requested channels: '%s'"), Channels);
		GTraceAuxiliary.ResetCommandlineChannels();
		GTraceAuxiliary.AddCommandlineChannels(Channels);
		GTraceAuxiliary.EnableCommandlineChannels();
	}

	if (Options)
	{
		// Truncation is only valid when tracing to file and filename is set.
		if (Options->bTruncateFile && Type == EConnectionType::File && Target != nullptr)
		{
			GTraceAuxiliary.SetTruncateFile(Options->bTruncateFile);
		}
	}

	if (Type == EConnectionType::File)
	{
		return GTraceAuxiliary.Connect(ETraceConnectType::File, Target, LogCategory);
	}
	else if(Type == EConnectionType::Network)
	{
		return GTraceAuxiliary.Connect(ETraceConnectType::Network, Target, LogCategory);
	}
#endif
	return false;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliary::Stop()
{
#if UE_TRACE_ENABLED
	return GTraceAuxiliary.Stop();
#else
	return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliary::Pause()
{
#if UE_TRACE_ENABLED
	GTraceAuxiliary.PauseChannels();
#endif
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliary::Resume()
{
#if UE_TRACE_ENABLED
	GTraceAuxiliary.ResumeChannels();
#endif
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FTraceAuxiliary::WriteSnapshot(const TCHAR* InFilePath)
{
#if UE_TRACE_ENABLED
	return GTraceAuxiliary.WriteSnapshot(InFilePath, LogCore);
#else
	return true;
#endif
}

////////////////////////////////////////////////////////////////////////////////
#if UE_TRACE_ENABLED
static void TraceAuxiliaryAddPostForkCallback(const TCHAR* CommandLine)
{
	if (GTraceAutoStart)
	{
		UE_LOG(LogCore, Display, TEXT("Trace not started in parent because forking is expected. Use -NoFakeForking to trace parent."));
	}

	checkf(!GOnPostForkHandle.IsValid(), TEXT("TraceAuxiliaryAddPostForkCallback should only be called once."));

	GOnPostForkHandle = FCoreDelegates::OnPostFork.AddLambda([](EForkProcessRole Role)
	{
		if (Role == EForkProcessRole::Child)
		{
			FString CmdLine = FCommandLine::Get();

			FTraceAuxiliary::Initialize(*CmdLine);
			FTraceAuxiliary::TryAutoConnect();

			// InitializePresets is needed in the regular startup phase since dynamically loaded modules can define
			// presets and channels and we need to enable those after modules have been loaded. In the case of forked
			// process all modules should already have been loaded.
			// FTraceAuxiliary::InitializePresets(*CmdLine);
		}
	});
}
#endif // UE_TRACE_ENABLED

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliary::Initialize(const TCHAR* CommandLine)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTraceAux_Init);

#if WITH_UNREAL_TRACE_LAUNCH
	if (!(FParse::Param(CommandLine, TEXT("notraceserver")) || FParse::Param(CommandLine, TEXT("buildmachine"))))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FTraceAux_LaunchUnrealTrace);
		LaunchUnrealTraceInternal(CommandLine);
	}
#endif

#if UE_TRACE_ENABLED
	UE_LOG(LogCore, Log, TEXT("Initializing trace..."));

	FParse::Bool(CommandLine, TEXT("-traceautostart="), GTraceAutoStart);
	UE_LOG(LogCore, Verbose, TEXT("Trace auto start = %d."), GTraceAutoStart);

	if (GTraceAuxiliary.IsParentProcessAndPreFork())
	{
		UE_LOG(LogCore, Log, TEXT("Trace initialization skipped for parent process (pre fork)."));

		GTraceAuxiliary.DisableChannels(nullptr);

		// Set our post fork callback up and return - children will pass through and Initialize when they're created.
		TraceAuxiliaryAddPostForkCallback(CommandLine);
		return;
	}

	const TCHAR* AppName = TEXT(UE_APP_NAME);
#if IS_MONOLITHIC && !IS_PROGRAM
	extern TCHAR GInternalProjectName[];
	if (GInternalProjectName[0] != '\0')
	{
		AppName = GInternalProjectName;
	}
#endif

	// Trace out information about this session. This is done before initialisation,
	// so that it is always sent (all channels are enabled prior to initialisation).
	const TCHAR* BranchName = BuildSettings::GetBranchName();
	const TCHAR* BuildVersion = BuildSettings::GetBuildVersion();
	constexpr uint32 PlatformLen = UE_ARRAY_COUNT(PREPROCESSOR_TO_STRING(UBT_COMPILED_PLATFORM)) - 1;
	const uint32 AppNameLen = FCString::Strlen(AppName);
	const uint32 CommandLineLen = FCString::Strlen(CommandLine);
	const uint32 BranchNameLen = FCString::Strlen(BranchName);
	const uint32 BuildVersionLen = FCString::Strlen(BuildVersion);
	uint32 DataSize =
		(PlatformLen * sizeof(ANSICHAR)) +
		(AppNameLen * sizeof(ANSICHAR)) +
		(CommandLineLen * sizeof(TCHAR)) +
		(BranchNameLen * sizeof(TCHAR)) +
		(BuildVersionLen * sizeof(TCHAR));
	UE_TRACE_LOG(Diagnostics, Session2, UE::Trace::TraceLogChannel, DataSize)
		<< Session2.Platform(PREPROCESSOR_TO_STRING(UBT_COMPILED_PLATFORM), PlatformLen)
		<< Session2.AppName(AppName, AppNameLen)
		<< Session2.CommandLine(CommandLine, CommandLineLen)
		<< Session2.Branch(BranchName, BranchNameLen)
		<< Session2.BuildVersion(BuildVersion, BuildVersionLen)
		<< Session2.Changelist(BuildSettings::GetCurrentChangelist())
		<< Session2.ConfigurationType(uint8(FApp::GetBuildConfiguration()))
		<< Session2.TargetType(uint8(FApp::GetBuildTargetType()));

	// Attempt to send trace data somewhere from the command line. It perhaps
	// seems odd to do this before initialising Trace, but it is done this way
	// to support disabling the "important" cache without losing any events.
	bool bShouldStartWorkerThread = false;
	StartFromCommandlineArguments(CommandLine, bShouldStartWorkerThread);

	// Initialize Trace
	UE::Trace::FInitializeDesc Desc;
	SetupInitFromConfig(Desc);
	
	Desc.bUseWorkerThread = bShouldStartWorkerThread;
	Desc.bUseImportantCache = (FParse::Param(CommandLine, TEXT("tracenocache")) == false);
	Desc.OnConnectionFunc = &OnConnectionCallback;
	if (FParse::Value(CommandLine, TEXT("-tracetailmb="), Desc.TailSizeBytes))
	{
		Desc.TailSizeBytes <<= 20;
	}

	// Memory tracing is very chatty. To reduce load on trace we'll speed up the
	// worker thread so it can clear events faster.
	extern bool MemoryTrace_IsActive();
	if (MemoryTrace_IsActive())
	{
		int32 SleepTimeMs = 5;
		if (GConfig)
		{
			GConfig->GetInt(GTraceConfigSection, TEXT("SleepTimeWhenMemoryTracingInMS"), SleepTimeMs, GEngineIni);
		}

		if (Desc.ThreadSleepTimeInMS)
		{
			SleepTimeMs = FMath::Min<uint32>(Desc.ThreadSleepTimeInMS, SleepTimeMs);
		}

		Desc.ThreadSleepTimeInMS = SleepTimeMs;
	}

	UE::Trace::Initialize(Desc);

	// Workaround for the fact that even if StartFromCommandlineArguments will enable channels
	// specified by the commandline, UE::Trace::Initialize will reset all channels.
	GTraceAuxiliary.EnableCommandlineChannelsPostInitialize();

	// Setup known on connection callbacks
	OnConnection.AddStatic(FStringTrace::OnConnection);

	// Always register end frame updates. This path is short circuited if a worker thread exists.
	GTraceAuxiliary.StartEndFramePump();

	// Initialize callstack tracing with the regular malloc (it might have already been initialized by memory tracing).
	CallstackTrace_Create(GMalloc);
	CallstackTrace_Initialize();

	// By default use 1 msec for stack sampling interval.
	uint32 Microseconds = 1000;
	FParse::Value(CommandLine, TEXT("-samplinginterval="), Microseconds);
	PlatformEvents_Init(Microseconds);
	PlatformEvents_PostInit();

#if CSV_PROFILER
	FCoreDelegates::OnEndFrame.AddRaw(&GTraceAuxiliary, &FTraceAuxiliaryImpl::UpdateCsvStats);
#endif

	if (GTraceAutoStart)
	{
		FModuleManager::Get().OnModulesChanged().AddLambda([](FName Name, EModuleChangeReason Reason)
		{
			if (Reason == EModuleChangeReason::ModuleLoaded)
			{
				GTraceAuxiliary.EnableCommandlineChannels();
			}
		});
	}

	UE::Trace::ThreadRegister(TEXT("GameThread"), FPlatformTLS::GetCurrentThreadId(), -1);

	UE_LOG(LogCore, Log, TEXT("Finished trace initialization."));
#endif //UE_TRACE_ENABLED
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliary::InitializePresets(const TCHAR* CommandLine)
{
#if UE_TRACE_ENABLED
	if (GTraceAuxiliary.IsParentProcessAndPreFork() || !GTraceAutoStart)
	{
		return;
	}

	// Second pass over trace arguments, this time to allow config defined presets
	// to be applied.
	FString Parameter;
	if (FParse::Value(CommandLine, TEXT("-trace="), Parameter, false))
	{
		GTraceAuxiliary.AddCommandlineChannels(*Parameter);
		GTraceAuxiliary.EnableCommandlineChannels();
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliary::Shutdown()
{
#if UE_TRACE_ENABLED
	if (GTraceAuxiliary.IsParentProcessAndPreFork())
	{
		return;
	}

	// Make sure all platform event functionality has shut down as on some
	// platforms it impacts whole system, even if application has terminated.
	PlatformEvents_Stop();
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliary::EnableChannels()
{
#if UE_TRACE_ENABLED
	GTraceAuxiliary.EnableCommandlineChannels();
#endif
}

void FTraceAuxiliary::DisableChannels(const TCHAR* Channels)
{
#if UE_TRACE_ENABLED
	GTraceAuxiliary.DisableChannels(Channels);
#endif
}

const TCHAR* FTraceAuxiliary::GetTraceDestination()
{
#if UE_TRACE_ENABLED
	return GTraceAuxiliary.GetDest();
#endif
	return nullptr;
}

bool FTraceAuxiliary::IsConnected()
{
#if UE_TRACE_ENABLED
	return GTraceAuxiliary.IsConnected();
#endif
	return false;
}

void FTraceAuxiliary::GetActiveChannelsString(FStringBuilderBase& String)
{
#if UE_TRACE_ENABLED
	GTraceAuxiliary.GetActiveChannelsString(String);
#endif
}

////////////////////////////////////////////////////////////////////////////////
void FTraceAuxiliary::TryAutoConnect()
{
#if UE_TRACE_ENABLED
#if PLATFORM_WINDOWS
	if (GTraceAutoStart)
	{
		// If we can detect a named event it means UnrealInsights (Browser Mode) is running.
		// In this case, we try to auto-connect with the Trace Server.
		HANDLE KnownEvent = ::OpenEvent(EVENT_ALL_ACCESS, false, TEXT("Local\\UnrealInsightsBrowser"));
		if (KnownEvent != nullptr)
		{
			UE_LOG(LogCore, Display, TEXT("Unreal Insights instance detected, auto-connecting to local trace server..."));
			Start(EConnectionType::Network, TEXT("127.0.0.1"), GTraceAuxiliary.HasCommandlineChannels() ? nullptr : TEXT("default"), nullptr);
			::CloseHandle(KnownEvent);
		}
	}
#endif // PLATFORM_WINDOWS
#endif // UE_TRACE_ENABLED
}
