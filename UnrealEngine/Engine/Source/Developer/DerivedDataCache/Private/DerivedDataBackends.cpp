// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/StringView.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCacheMethod.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheReplay.h"
#include "DerivedDataCacheStore.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataRequestOwner.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "Internationalization/FastDecimalFormat.h"
#include "Math/BasicMathExpressionEvaluator.h"
#include "Math/UnitConversion.h"
#include "MemoryCacheStore.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineBuildSettings.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "PakFileCacheStore.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/CompactBinaryPackage.h"
#include "String/Find.h"
#include <atomic>

DEFINE_LOG_CATEGORY(LogDerivedDataCache);
LLM_DEFINE_TAG(UntaggedDDCResult);
LLM_DEFINE_TAG(DDCBackend);

#define MAX_BACKEND_KEY_LENGTH (120)
#define LOCTEXT_NAMESPACE "DerivedDataBackendGraph"

static TAutoConsoleVariable<FString> GDerivedDataCacheGraphName(
	TEXT("DDC.Graph"),
	TEXT("Default"),
	TEXT("Name of the graph to use for the Derived Data Cache."),
	ECVF_ReadOnly);

namespace UE::DerivedData::Private
{

static std::atomic<int32> GAsyncTaskCounter;

int32 AddToAsyncTaskCounter(int32 Addend)
{
	return GAsyncTaskCounter.fetch_add(Addend);
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

ILegacyCacheStore* CreateCacheStoreAsync(ILegacyCacheStore* InnerBackend, IMemoryCacheStore* MemoryCache, bool bDeleteInnerCache);
ILegacyCacheStore* CreateCacheStoreHierarchy(ICacheStoreOwner*& OutOwner, TFunctionRef<void (IMemoryCacheStore*&)> MemoryCacheCreator);
ILegacyCacheStore* CreateCacheStoreThrottle(ILegacyCacheStore* InnerCache, uint32 LatencyMS, uint32 MaxBytesPerSecond);
ILegacyCacheStore* CreateCacheStoreVerify(ILegacyCacheStore* InnerCache, bool bPutOnError);
ILegacyCacheStore* CreateFileSystemCacheStore(const TCHAR* Name, const TCHAR* Config, ICacheStoreOwner& Owner, ICacheStoreGraph* Graph, FString& OutPath, bool& OutRedirected);
ILegacyCacheStore* CreateHttpCacheStore(const TCHAR* Name, const TCHAR* Config, ICacheStoreOwner* Owner);
void CreateMemoryCacheStore(IMemoryCacheStore*& OutCache, const TCHAR* Name, const TCHAR* Config, ICacheStoreOwner* Owner);
IPakFileCacheStore* CreatePakFileCacheStore(const TCHAR* Name, const TCHAR* Filename, bool bWriting, bool bCompressed, ICacheStoreOwner* Owner);
ILegacyCacheStore* CreateS3CacheStore(const TCHAR* Name, const TCHAR* Config, ICacheStoreOwner& Owner);
ILegacyCacheStore* CreateZenCacheStore(const TCHAR* Name, const TCHAR* Config, ICacheStoreOwner* Owner);
ILegacyCacheStore* TryCreateCacheStoreReplay(ILegacyCacheStore* InnerCache);

/**
 * This class is used to create a singleton that represents the derived data cache hierarchy and all of the wrappers necessary
 * ideally this would be data driven and the backends would be plugins...
 */
class FDerivedDataBackendGraph final : public FDerivedDataBackend, ICacheStoreOwner, ICacheStoreGraph
{
public:
	using FParsedNode = ILegacyCacheStore*;
	using FParsedNodeMap = TMap<FString, FParsedNode>;

	/**
	 * constructor, builds the cache tree
	 */
	FDerivedDataBackendGraph()
		: RootCache(nullptr)
		, MemoryCache(nullptr)
		, BootCache(nullptr)
		, WritePakCache(nullptr)
		, Hierarchy(nullptr)
		, bUsingSharedDDC(false)
		, bIsShuttingDown(false)
		, MountPakCommand(
			TEXT("DDC.MountPak"),
			*LOCTEXT("CommandText_DDCMountPak", "Mounts read-only pak file").ToString(),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FDerivedDataBackendGraph::MountPakCommandHandler))
		, UnmountPakCommand(
			TEXT("DDC.UnmountPak"),
			*LOCTEXT("CommandText_DDCUnmountPak", "Unmounts read-only pak file").ToString(),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FDerivedDataBackendGraph::UnmountPakCommandHandler))
		, LoadReplayCommand(
			TEXT("DDC.LoadReplay"),
			*LOCTEXT("CommandText_DDCLoadReplay", "Loads a cache replay file created by -DDC-ReplaySave=<Path>").ToString(),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FDerivedDataBackendGraph::LoadReplayCommandHandler))
	{
		check(!StaticGraph);
		StaticGraph = this;

		check(IsInGameThread()); // we pretty much need this to be initialized from the main thread...it uses GConfig, etc
		check(GConfig && GConfig->IsReadyForUse());

		bHasDefaultDebugOptions = FBackendDebugOptions::ParseFromTokens(DefaultDebugOptions, TEXT("All"), FCommandLine::Get());

		RootCache = nullptr;
		FParsedNodeMap ParsedNodes;
		TGuardValue ParsedNodeMapGuard(ActiveParsedNodeMap, &ParsedNodes);

		ILegacyCacheStore* RootNode = nullptr;

		// Create the graph using ini settings. The string "default" forwards creation to use the default graph.

		if (!FParse::Value(FCommandLine::Get(), TEXT("-DDC="), GraphName))
		{
			GraphName = GDerivedDataCacheGraphName.GetValueOnGameThread();
		}

		// A DDC graph of "None" is used by build worker programs that use the DDC build code paths but avoid use of the DDC cache
		// code paths. Unfortunately the cache must currently be instantiated as part of initializing the build code, so we have
		// to disable the cache portion by injecting "-DDC=None" to the commandline args during process startup. This mode is not
		// compatible with use in the editor/commandlets which is not written to operate with a non-functional cache layer. This
		// can lead to confusion when people attempt to use "-DDC=None" in the editor expecting it to behave like "-DDC=Cold".
		// To avoid this confusion (and until we can use the build without the cache) it is restricted to use in programs only
		// and not the editor.
#if IS_PROGRAM
		if (GraphName == TEXT("None"))
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("Requested cache graph of 'None'. Every cache operation will fail."));
		}
		else
#endif
		{
			const TCHAR* const RootName = TEXT("Root");

			if (!GraphName.IsEmpty() && GraphName != TEXT("Default"))
			{
				RootNode = CreateCacheStoreHierarchy(Hierarchy, [this](IMemoryCacheStore*& OutCache) { GetMemoryCache(OutCache); });

				if (!ParseNode(RootName, GEngineIni, *GraphName, ParsedNodes) || !Hierarchy->HasAllFlags(ECacheStoreFlags::Query | ECacheStoreFlags::Store))
				{
					// Destroy any cache stores that have been created.
					delete RootNode;
					RootNode = nullptr;
					ParsedNodes.Empty();
					MemoryCache = nullptr;
					BootCache = nullptr;
					WritePakCache = nullptr;
					Hierarchy = nullptr;
					ReadPakCache.Empty();
					Directories.Empty();
					bAsyncFound = false;
					bBootFound = false;
					bHierarchyFound = false;
					bVerifyFound = false;
					bVerifyFix = false;
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("Unable to create or use cache graph '%s'. Reverting to the default graph."), *GraphName);
				}
			}

			if (!Hierarchy)
			{
				// Try to use the default graph.
				GraphName = FApp::IsEngineInstalled() ? TEXT("InstalledDerivedDataBackendGraph") : TEXT("DerivedDataBackendGraph");
				RootNode = CreateCacheStoreHierarchy(Hierarchy, [this](IMemoryCacheStore*& OutCache) { GetMemoryCache(OutCache); });

				if (!ParseNode(RootName, GEngineIni, *GraphName, ParsedNodes) || !Hierarchy->HasAllFlags(ECacheStoreFlags::Query | ECacheStoreFlags::Store))
				{
					FString Entry;
					if (!GConfig->DoesSectionExist(*GraphName, GEngineIni))
					{
						UE_LOG(LogDerivedDataCache, Fatal,
							TEXT("Unable to create default cache graph '%s' because its config section is missing in the '%s' config."),
							*GraphName, *GEngineIni);
					}
					else if (!GConfig->GetString(*GraphName, RootName, Entry, GEngineIni) || !Entry.Len())
					{
						UE_LOG(LogDerivedDataCache, Fatal,
							TEXT("Unable to create default cache graph '%s' because the root node '%s' is missing."),
							*GraphName, RootName);
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Fatal,
							TEXT("Unable to use default cache graph '%s' because there are no %s nodes available."),
							*GraphName,
							Hierarchy->HasAllFlags(ECacheStoreFlags::Query) ? TEXT("writable") :
							Hierarchy->HasAllFlags(ECacheStoreFlags::Store) ? TEXT("readable") : TEXT("readable or writable"));
					}
				}
			}
		}

		// Async must exist in the graph.
		RootNode = CreateCacheStoreAsync(RootNode, MemoryCache, /*bDeleteInnerCache*/ true);

		// Create a Verify node when using -DDC-Verify[=Type1[@Rate2][+Type2[@Rate2]...]] or the graph has a verify node.
		if (FString VerifyArg;
			FParse::Value(FCommandLine::Get(), TEXT("-DDC-Verify="), VerifyArg) ||
			FParse::Param(FCommandLine::Get(), TEXT("DDC-Verify")) ||
			bVerifyFound)
		{
			IFileManager::Get().DeleteDirectory(*(FPaths::ProjectSavedDir() / TEXT("VerifyDDC/")), /*bRequireExists*/ false, /*bTree*/ true);
			RootNode = CreateCacheStoreVerify(RootNode, /*bPutOnError*/ bVerifyFix);
		}

		// Create a Replay node when requested on the command line.
		if (ILegacyCacheStore* ReplayNode = TryCreateCacheStoreReplay(RootNode))
		{
			RootNode = ReplayNode;
		}

		if (MaxKeyLength == 0)
		{
			MaxKeyLength = MAX_BACKEND_KEY_LENGTH;
		}

		RootCache = RootNode;
	}

	/**
	 * Helper function to get the value of parsed bool as the return value
	 **/
	bool GetParsedBool( const TCHAR* Stream, const TCHAR* Match ) const
	{
		bool bValue = 0;
		FParse::Bool( Stream, Match, bValue );
		return bValue;
	}

	/**
	 * Parses backend graph node from ini settings
	 *
	 * @param NodeName Name of the node to parse
	 * @param IniFilename Ini filename
	 * @param IniSection Section in the ini file containing the graph definition
	 * @param InParsedNodes Map of parsed nodes and their names to be able to find already parsed nodes
	 * @return Derived data backend interface instance created from ini settings
	 */
	bool ParseNode(const FString& NodeName, const FString& IniFilename, const TCHAR* IniSection, FParsedNodeMap& InParsedNodes)
	{
		if (const FParsedNode* ParsedNode = InParsedNodes.Find(NodeName))
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("Node %s was referenced more than once in the graph. Nodes may not be shared."), *NodeName);
			return false;
		}

		ICacheStoreOwner* NodeOwner = this;
		FString Entry;
		if (GConfig->GetString(IniSection, *NodeName, Entry, IniFilename))
		{
			Entry.TrimStartInline();
			Entry.RemoveFromStart(TEXT("("));
			Entry.RemoveFromEnd(TEXT(")"));

			TGuardValue NodeNameGuard(ActiveNodeName, NodeName);
			TGuardValue NodeConfigGuard(ActiveNodeConfig, Entry);

			FString	NodeType;
			if (FParse::Value(*Entry, TEXT("Type="), NodeType))
			{
				if (NodeType == TEXT("FileSystem"))
				{
					return ParseFileSystemCache(*NodeName, *Entry, InParsedNodes);
				}
				else if (NodeType == TEXT("Boot"))
				{
					return ParseBootCache(*NodeName, *Entry, InParsedNodes);
				}
				else if (NodeType == TEXT("Memory"))
				{
					return ParseMemoryCache(*NodeName, *Entry, InParsedNodes);
				}
				else if (NodeType == TEXT("Hierarchical"))
				{
					return ParseHierarchyNode(*NodeName, *Entry, IniFilename, IniSection, InParsedNodes);
				}
				else if (NodeType == TEXT("KeyLength"))
				{
					return ParseKeyLength(*NodeName, *Entry, IniFilename, IniSection, InParsedNodes);
				}
				else if (NodeType == TEXT("AsyncPut"))
				{
					return ParseAsyncNode(*NodeName, *Entry, IniFilename, IniSection, InParsedNodes);
				}
				else if (NodeType == TEXT("Verify"))
				{
					return ParseVerify(*NodeName, *Entry, IniFilename, IniSection, InParsedNodes);
				}
				else if (NodeType == TEXT("ReadPak"))
				{
					return ParsePak(*NodeName, *Entry, /*bWriting*/ false, InParsedNodes);
				}
				else if (NodeType == TEXT("WritePak"))
				{
					return ParsePak(*NodeName, *Entry, /*bWriting*/ true, InParsedNodes);
				}
				else if (NodeType == TEXT("S3"))
				{
					if (ILegacyCacheStore* CacheStore = CreateS3CacheStore(*NodeName, *Entry, *this))
					{
						InParsedNodes.Add(NodeName, CacheStore);
						return true;
					}
				}
				else if (NodeType == TEXT("Cloud") || NodeType == TEXT("Http"))
				{
					if (ILegacyCacheStore* CacheStore = CreateHttpCacheStore(*NodeName, *Entry, this))
					{
						InParsedNodes.Add(NodeName, CacheStore);
						return true;
					}
				}
				else if (NodeType == TEXT("Zen"))
				{
					if (ILegacyCacheStore* CacheStore = CreateZenCacheStore(*NodeName, *Entry, this))
					{
						InParsedNodes.Add(NodeName,CacheStore);
						return true;
					}
				}
			}
		}

		return false;
	}

	/**
	 * Creates Read/write Pak file interface from ini settings
	 *
	 * @param NodeName Node name
	 * @param Entry Node definition
	 * @param bWriting true to create pak interface for writing
	 * @return Pak file data backend interface instance or nullptr if unsuccessful
	 */
	bool ParsePak(const TCHAR* NodeName, const TCHAR* Entry, const bool bWriting, FParsedNodeMap& InParsedNodes)
	{
		ILegacyCacheStore* PakNode = nullptr;
		FString PakFilename;
		FParse::Value( Entry, TEXT("Filename="), PakFilename );
		bool bCompressed = GetParsedBool(Entry, TEXT("Compressed="));

		if (PakFilename.IsEmpty())
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("FDerivedDataBackendGraph: %s pak cache Filename not found in *engine.ini, will not use a pak cache."), NodeName);
			return false;
		}

		if (bWriting)
		{
			if (WritePakCache)
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to create %s pak cache because only one pak cache write node is supported."), NodeName);
				return false;
			}

			FGuid Temp = FGuid::NewGuid();
			ReadPakFilename = PakFilename;
			WritePakFilename = PakFilename + TEXT(".") + Temp.ToString();
			WritePakCache = CreatePakFileCacheStore(NodeName, *WritePakFilename, /*bWriting*/ true, bCompressed, this);
			PakNode = WritePakCache;
			InParsedNodes.Add(NodeName, WritePakCache);
			return true;
		}
		else
		{
			bool bReadPak = FPlatformFileManager::Get().GetPlatformFile().FileExists(*PakFilename);
			if (!bReadPak)
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("FDerivedDataBackendGraph: %s pak cache file %s not found, will not use a pak cache."), NodeName, *PakFilename);
				return false;
			}

			IPakFileCacheStore* ReadPak = CreatePakFileCacheStore(NodeName, *PakFilename, /*bWriting*/ false, bCompressed, this);
			ReadPakFilename = PakFilename;
			PakNode = ReadPak;
			ReadPakCache.Add(ReadPak);
			InParsedNodes.Add(NodeName, ReadPak);
			return true;
		}
	}

	/**
	 * Creates Verify wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return Verify wrapper backend interface instance or nullptr if unsuccessful
	 */
	bool ParseVerify(const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, FParsedNodeMap& InParsedNodes)
	{
		if (bVerifyFound)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to create %s Verify because only one Verify node is supported."), NodeName);
			return false;
		}

		bVerifyFound = true;
		bVerifyFix = GetParsedBool(Entry, TEXT("Fix="));

		FString InnerName;
		if (FParse::Value(Entry, TEXT("Inner="), InnerName) && ParseNode(InnerName, IniFilename, IniSection, InParsedNodes))
		{
			return true;
		}

		UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for Verify node %s. Verify node will not be created."), *InnerName, NodeName);
		return false;
	}

	/**
	 * Creates AsyncPut wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return AsyncPut wrapper backend interface instance or nullptr if unsuccessful
	 */
	bool ParseAsyncNode(const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, FParsedNodeMap& InParsedNodes)
	{
		if (bAsyncFound)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to create %s AsyncPut because only one AsyncPut node is supported."), NodeName);
			return false;
		}

		bAsyncFound = true;
		FString InnerName;
		if (FParse::Value(Entry, TEXT("Inner="), InnerName) && ParseNode(InnerName, IniFilename, IniSection, InParsedNodes))
		{
			return true;
		}

		UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for AsyncPut node %s. AsyncPut node will not be created."), *InnerName, NodeName);
		return false;
	}

	/**
	 * Creates KeyLength wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return KeyLength wrapper backend interface instance or nullptr if unsuccessful
	 */
	bool ParseKeyLength(const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, FParsedNodeMap& InParsedNodes)
	{
		if (MaxKeyLength)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Node %s is disabled because there may be only one key length node."), NodeName);
			return false;
		}

		FString InnerName;
		if (FParse::Value(Entry, TEXT("Inner="), InnerName) && ParseNode(InnerName, IniFilename, IniSection, InParsedNodes))
		{
			int32 KeyLength = MAX_BACKEND_KEY_LENGTH;
			FParse::Value(Entry, TEXT("Length="), KeyLength);
			MaxKeyLength = FMath::Clamp(KeyLength, 0, MAX_BACKEND_KEY_LENGTH);
			return true;
		}

		UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for KeyLength node %s. KeyLength node will not be created."), *InnerName, NodeName);
		return false;
	}

	/**
	 * Creates Hierarchical interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return Hierarchical backend interface instance or nullptr if unsuccessful
	 */
	bool ParseHierarchyNode(const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, FParsedNodeMap& InParsedNodes)
	{
		if (bHierarchyFound)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Node %s is disabled because there may be only one hierarchy node. "
				"Confirm there is only one hierarchy in the cache graph and that it is inside of any async node."), NodeName);
			return false;
		}

		const TCHAR* InnerMatch = TEXT("Inner=");
		const int32 InnerMatchLength = FCString::Strlen(InnerMatch);

		bHierarchyFound = true;
		bool bParsed = false;
		FString InnerName;
		while (FParse::Value(Entry, InnerMatch, InnerName))
		{
			if (ParseNode(InnerName, IniFilename, IniSection, InParsedNodes))
			{
				bParsed = true;
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("Unable to find inner node %s for hierarchy %s."), *InnerName, NodeName);
			}

			// Move the Entry pointer forward so that we can find more children
			Entry = FCString::Strifind(Entry, InnerMatch);
			Entry += InnerMatchLength;
		}

		if (!bParsed)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Hierarchical cache %s has no inner backends and will not be created."), NodeName);
			return false;
		}

		return true;
	}

	/**
	 * Creates Filesystem data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @return Filesystem data cache backend interface instance or nullptr if unsuccessful
	 */
	bool ParseFileSystemCache(const TCHAR* NodeName, const TCHAR* Config, FParsedNodeMap& InParsedNodes)
	{
		bool bRedirected = false;
		FString Path;
		if (ILegacyCacheStore* Store = CreateFileSystemCacheStore(NodeName, Config, *this, this, Path, bRedirected))
		{
			if (!bRedirected)
			{
				bUsingSharedDDC |= NodeName == TEXTVIEW("Shared");
				Directories.AddUnique(Path);
			}
			InParsedNodes.Add(NodeName, Store);
			return true;
		}

		return false;
	}

	/**
	 * Creates Boot data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param OutFilename filename specified for the cache
	 * @return Boot data cache backend interface instance or nullptr if unsuccessful
	 */
	bool ParseBootCache(const TCHAR* NodeName, const TCHAR* Entry, FParsedNodeMap& InParsedNodes)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("Boot nodes are deprecated. Please remove the Boot node from the cache graph."));
		if (bBootFound)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Unable to create %s Boot cache because only one Boot node is supported."), NodeName);
			return false;
		}

		bBootFound = true;

	#if WITH_EDITOR
		// Only allow boot cache with the editor. We don't want other tools and utilities (eg. SCW) writing to the same file.
		CreateMemoryCacheStore(BootCache, TEXT("Boot"), *WriteToString<128>(TEXT("-Boot "), Entry), this);
	#endif

		if (!!BootCache)
		{
			InParsedNodes.Add(NodeName, BootCache);
		}

		return !!BootCache;
	}

	/**
	 * Creates Memory data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @return Memory data cache backend interface instance or nullptr if unsuccessful
	 */
	bool ParseMemoryCache(const TCHAR* NodeName, const TCHAR* Entry, FParsedNodeMap& InParsedNodes)
	{
		FString Filename;
		FParse::Value(Entry, TEXT("Filename="), Filename);
		IMemoryCacheStore* Cache;
		CreateMemoryCacheStore(Cache, NodeName, TEXT(""), this);
		check(Cache);
		InParsedNodes.Add(NodeName, Cache);
		if (Filename.Len())
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("Memory nodes that load from a file are deprecated. Please remove the filename from the cache configuration."));
		}
		return true;
	}

	void GetMemoryCache(IMemoryCacheStore*& OutCache)
	{
		if (MemoryCache)
		{
			OutCache = MemoryCache;
		}
		else
		{
			// This is unconditionally added to the hierarchy and will be deleted by the hierarchy.
			CreateMemoryCacheStore(OutCache, TEXT("Memory"), TEXT("-ReadOnly -StopGetStore -NoStats"), this);
			MemoryCache = OutCache;
		}
	}

	virtual ~FDerivedDataBackendGraph()
	{
		check(StaticGraph == this);
		Replays.Empty();
		delete RootCache;
		StaticGraph = nullptr;
	}

	ILegacyCacheStore& GetRoot() override
	{
		check(RootCache);
		return *RootCache;
	}

	virtual int32 GetMaxKeyLength() const override
	{
		return MaxKeyLength;
	}

	virtual void NotifyBootComplete() override
	{
		check(RootCache);
		if (BootCache)
		{
			BootCache->Disable();
		}
	}

	virtual void WaitForQuiescence(bool bShutdown) override
	{
		double StartTime = FPlatformTime::Seconds();
		double LastPrint = StartTime;

		if (bShutdown)
		{
			bIsShuttingDown.store(true, std::memory_order_relaxed);
		}

		while (const int32 AsyncCompletionCounter = Private::AddToAsyncTaskCounter(0))
		{
			check(AsyncCompletionCounter > 0);
			FPlatformProcess::Sleep(0.1f);
			if (FPlatformTime::Seconds() - LastPrint > 5.0)
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("Waited %ds for derived data cache to finish..."), int32(FPlatformTime::Seconds() - StartTime));
				LastPrint = FPlatformTime::Seconds();
			}
		}
		if (bShutdown)
		{
			FString MergePaks;
			if(WritePakCache && WritePakCache->IsWritable() && FParse::Value(FCommandLine::Get(), TEXT("MergePaks="), MergePaks))
			{
				TArray<FString> MergePakList;
				MergePaks.FString::ParseIntoArray(MergePakList, TEXT("+"));

				for(const FString& MergePakName : MergePakList)
				{
					TUniquePtr<IPakFileCacheStore> ReadPak(
						CreatePakFileCacheStore(TEXT("Merge"), *FPaths::Combine(*FPaths::GetPath(WritePakFilename), *MergePakName), /*bWriting*/ false, /*bCompressed*/ false, /*Owner*/ nullptr));
					WritePakCache->MergeCache(ReadPak.Get());
				}
			}
			for (int32 ReadPakIndex = 0; ReadPakIndex < ReadPakCache.Num(); ReadPakIndex++)
			{
				ReadPakCache[ReadPakIndex]->Close();
			}
			if (WritePakCache && WritePakCache->IsWritable())
			{
				WritePakCache->Close();
				if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*WritePakFilename))
				{
					UE_LOG(LogDerivedDataCache, Error, TEXT("Pak file %s was not produced?"), *WritePakFilename);
				}
				else
				{
					if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ReadPakFilename))
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadPakFilename, false);
						if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ReadPakFilename))
						{
							UE_LOG(LogDerivedDataCache, Error, TEXT("Could not delete the pak file %s to overwrite it with a new one."), *ReadPakFilename);
						}
					}
					if (!IPakFileCacheStore::SortAndCopy(WritePakFilename, ReadPakFilename))
					{
						UE_LOG(LogDerivedDataCache, Error, TEXT("Couldn't sort pak file (%s)"), *WritePakFilename);
					}
					else if (!IFileManager::Get().Delete(*WritePakFilename))
					{
						UE_LOG(LogDerivedDataCache, Error, TEXT("Couldn't delete pak file (%s)"), *WritePakFilename);
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Display, TEXT("Sucessfully wrote %s."), *ReadPakFilename);
					}
				}
			}
		}
	}

	/** Get whether a shared cache is in use */
	virtual bool GetUsingSharedDDC() const override
	{
		return bUsingSharedDDC;
	}

	virtual const TCHAR* GetGraphName() const override
	{
		return *GraphName;
	}

	virtual const TCHAR* GetDefaultGraphName() const override
	{
		return FApp::IsEngineInstalled() ? TEXT("InstalledDerivedDataBackendGraph") : TEXT("DerivedDataBackendGraph");
	}

	virtual void AddToAsyncCompletionCounter(int32 Addend) override
	{
		verify(Private::AddToAsyncTaskCounter(Addend) + Addend >= 0);
	}

	virtual bool AnyAsyncRequestsRemaining() override
	{
		return Private::AddToAsyncTaskCounter(0) > 0;
	}

	virtual bool IsShuttingDown() override
	{
		return bIsShuttingDown.load(std::memory_order_relaxed);
	}

	virtual void GetDirectories(TArray<FString>& OutResults) override
	{
		OutResults = Directories;
	}

	static FORCEINLINE FDerivedDataBackendGraph& Get()
	{
		check(StaticGraph);
		return *StaticGraph;
	}

	virtual ILegacyCacheStore* MountPakFile(const TCHAR* PakFilename) override
	{
		// Assumptions: there's at least one read-only pak backend in the hierarchy
		// and its parent is a hierarchical backend.
		IPakFileCacheStore* ReadPak = nullptr;
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(PakFilename))
		{
			ReadPak = CreatePakFileCacheStore(TEXT("Mount"), PakFilename, /*bWriting*/ false, /*bCompressed*/ false, this);
			ReadPakCache.Add(ReadPak);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Failed to add %s read-only pak DDC backend. Make sure it exists and there's at least one hierarchical backend in the cache tree."), PakFilename);
		}

		return ReadPak;
	}

	virtual bool UnmountPakFile(const TCHAR* PakFilename) override
	{
		for (int PakIndex = 0; PakIndex < ReadPakCache.Num(); ++PakIndex)
		{
			IPakFileCacheStore* ReadPak = ReadPakCache[PakIndex];
			if (ReadPak->GetFilename() == PakFilename)
			{
				check(Hierarchy);

				// Wait until all async requests are complete.
				WaitForQuiescence(false);

				ReadPakCache.RemoveAt(PakIndex);
				ReadPak->Close();
				delete ReadPak;
				return true;
			}
		}
		return false;
	}

	virtual TSharedRef<FDerivedDataCacheStatsNode> GatherUsageStats() const override
	{
		TSharedRef<FDerivedDataCacheStatsNode> Stats = MakeShared<FDerivedDataCacheStatsNode>();
		if (RootCache)
		{
			RootCache->LegacyStats(Stats.Get());
		}
		return Stats;
	}

	virtual void GatherResourceStats(TArray<FDerivedDataCacheResourceStat>& DDCResourceStats) const override
	{
		if (Hierarchy)
		{
			Hierarchy->LegacyResourceStats(DDCResourceStats);
		}
	}

private:
	void Add(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) final
	{
		check(Hierarchy);

		// Parse any debug options for this node. E.g. -DDC-<Name>-MissRate
		FBackendDebugOptions DebugOptions;
		if (FBackendDebugOptions::ParseFromTokens(DebugOptions, *ActiveNodeName, FCommandLine::Get()))
		{
			if (!CacheStore->LegacyDebugOptions(DebugOptions))
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Node is ignoring one or more -DDC-%s-<Option> debug options."), *ActiveNodeName, *ActiveNodeName);
			}
		}
		else if (bHasDefaultDebugOptions)
		{
			if (!CacheStore->LegacyDebugOptions(DefaultDebugOptions))
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Node is ignoring one or more -DDC-All-<Option> debug options."), *ActiveNodeName);
			}
		}

		// Add a throttling layer if parameters are found
		{
			uint32 LatencyMS = 0;
			FParse::Value(*ActiveNodeConfig, TEXT("LatencyMS="), LatencyMS);

			uint32 MaxBytesPerSecond = 0;
			FParse::Value(*ActiveNodeConfig, TEXT("MaxBytesPerSecond="), MaxBytesPerSecond);

			if (LatencyMS != 0 || MaxBytesPerSecond != 0)
			{
				ILegacyCacheStore* ThrottleNode = CreateCacheStoreThrottle(CacheStore, LatencyMS, MaxBytesPerSecond);
				ThrottleNodes.Add(CacheStore, ThrottleNode);
				CacheStore = ThrottleNode;
			}
		}

		Hierarchy->Add(CacheStore, Flags);
	}

	void SetFlags(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) final
	{
		check(Hierarchy);
		if (ILegacyCacheStore* ThrottleNode = ThrottleNodes.FindRef(CacheStore))
		{
			Hierarchy->SetFlags(ThrottleNode, Flags);
		}
		else
		{
			Hierarchy->SetFlags(CacheStore, Flags);
		}
	}

	void RemoveNotSafe(ILegacyCacheStore* CacheStore) final
	{
		check(Hierarchy);
		if (ILegacyCacheStore* ThrottleNode = ThrottleNodes.FindRef(CacheStore))
		{
			Hierarchy->RemoveNotSafe(ThrottleNode);
			ThrottleNodes.Remove(CacheStore);
			delete ThrottleNode;
		}
		else
		{
			Hierarchy->RemoveNotSafe(CacheStore);
		}
	}

	bool HasAllFlags(ECacheStoreFlags Flags) const final
	{
		check(Hierarchy);
		return Hierarchy->HasAllFlags(Flags);
	}

	ICacheStoreStats* CreateStats(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags, FStringView Type, FStringView Name, FStringView Path) final
	{
		check(Hierarchy);
		if (ILegacyCacheStore* ThrottleNode = ThrottleNodes.FindRef(CacheStore))
		{
			CacheStore = ThrottleNode;
		}
		return Hierarchy->CreateStats(CacheStore, Flags, Type, Name, Path);
	}

	void DestroyStats(ICacheStoreStats* Stats) final
	{
		check(Hierarchy);
		Hierarchy->DestroyStats(Stats);
	}

	void LegacyResourceStats(TArray<FDerivedDataCacheResourceStat>& OutStats) const final
	{
		check(Hierarchy);
		Hierarchy->LegacyResourceStats(OutStats);
	}

	ILegacyCacheStore* FindOrCreate(const TCHAR* Name) final
	{
		if (!ActiveParsedNodeMap)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Requesting creation of node %s during an unsupported time for node creation."), Name);
			return nullptr;
		}

		if (const FParsedNode* ParsedNode = ActiveParsedNodeMap->Find(Name))
		{
			return *ParsedNode;
		}

		if (!ParseNode(Name, GEngineIni, *GraphName, *ActiveParsedNodeMap))
		{
			return nullptr;
		}

		const FParsedNode* ParsedNode = ActiveParsedNodeMap->Find(Name);
		if (ParsedNode)
		{
			return *ParsedNode;
		}
		return nullptr;
	}

	/** MountPak console command handler. */
	void UnmountPakCommandHandler(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Usage: DDC.MountPak PakFilename"));
			return;
		}
		UnmountPakFile(*Args[0]);
	}

	/** UnmountPak console command handler. */
	void MountPakCommandHandler(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Usage: DDC.UnmountPak PakFilename"));
			return;
		}
		MountPakFile(*Args[0]);
	}

	void LoadReplayCommandHandler(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Usage: DDC.LoadReplay ReplayPath"
				" [Methods=Get+GetValue+GetChunks]"
				" [Rate=<0-100>]"
				" [Types=Type1[@Rate1][+Type2[@Rate2]]..."
				" [Salt=PositiveInt32]"
				" [Priority=<Lowest-Blocking>]"
				" [AddPolicy=Query,SkipData]"
				" [RemovePolicy=SkipMeta]"));
			return;
		}

		TStringBuilder<512> JoinedArgs;
		JoinedArgs.Join(MakeArrayView(Args).RightChop(1), TEXT(' '));

		FCacheReplayReader Replay(RootCache);

		// Parse Key Filter
		const bool bDefaultMatch = String::FindFirst(*JoinedArgs, TEXT("Types="), ESearchCase::IgnoreCase) == INDEX_NONE;
		float DefaultRate = bDefaultMatch ? 100.0f : 0.0f;
		FParse::Value(*JoinedArgs, TEXT("Rate="), DefaultRate);

		FCacheKeyFilter KeyFilter = FCacheKeyFilter::Parse(*JoinedArgs, TEXT("Types="), DefaultRate);

		if (KeyFilter)
		{
			uint32 Salt;
			if (FParse::Value(*JoinedArgs, TEXT("Salt="), Salt))
			{
				if (Salt == 0)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("Replay: Ignoring salt of 0. The salt must be a positive integer."));
				}
				else
				{
					KeyFilter.SetSalt(Salt);
				}
			}

			UE_LOG(LogDerivedDataCache, Display,
				TEXT("Replay: Using salt %u to filter cache keys to replay."), KeyFilter.GetSalt());
		}

		Replay.SetKeyFilter(MoveTemp(KeyFilter));

		// Parse Method Filter
		FString MethodNames;
		if (FParse::Value(*JoinedArgs, TEXT("Methods="), MethodNames))
		{
			Replay.SetMethodFilter(FCacheMethodFilter::Parse(MethodNames));
		}

		// Parse Policy Transform
		ECachePolicy FlagsToAdd = ECachePolicy::None;
		FString FlagNamesToAdd;
		if (FParse::Value(*JoinedArgs, TEXT("AddPolicy="), FlagNamesToAdd))
		{
			TryLexFromString(FlagsToAdd, FlagNamesToAdd);
		}
		ECachePolicy FlagsToRemove = ECachePolicy::None;
		FString FlagNamesToRemove;
		if (FParse::Value(*JoinedArgs, TEXT("RemovePolicy="), FlagNamesToRemove))
		{
			TryLexFromString(FlagsToRemove, FlagNamesToRemove);
		}
		Replay.SetPolicyTransform(FlagsToAdd, FlagsToRemove);

		// Parse Priority Override
		EPriority Priority{};
		FString PriorityName;
		if (FParse::Value(*JoinedArgs, TEXT("Priority="), PriorityName) &&
			TryLexFromString(Priority, PriorityName))
		{
			Replay.SetPriorityOverride(Priority);
		}

		Replay.ReadFromFileAsync(*Args[0]);
		Replays.Add(MoveTemp(Replay));
	}

	static inline FDerivedDataBackendGraph*			StaticGraph;

	FString											GraphName;
	FString											ReadPakFilename;
	FString											WritePakFilename;

	/** Root of the graph */
	ILegacyCacheStore*					RootCache;

	TMap<ILegacyCacheStore*, ILegacyCacheStore*> ThrottleNodes;

	/** Instances of backend interfaces which exist in only one copy */
	IMemoryCacheStore* MemoryCache;
	IMemoryCacheStore* BootCache;
	IPakFileCacheStore* WritePakCache;
	ICacheStoreOwner* Hierarchy;
	/** Support for multiple read only pak files. */
	TArray<IPakFileCacheStore*> ReadPakCache;

	/** List of directories used by the DDC */
	TArray<FString> Directories;

	FBackendDebugOptions DefaultDebugOptions;

	int32 MaxKeyLength = 0;

	bool bHasDefaultDebugOptions = false;

	/** Whether a shared cache is in use */
	bool bUsingSharedDDC;

	/** Whether a shutdown is pending */
	std::atomic<bool> bIsShuttingDown;

	/** MountPak console command */
	FAutoConsoleCommand MountPakCommand;
	/** UnmountPak console command */
	FAutoConsoleCommand UnmountPakCommand;

	FAutoConsoleCommand LoadReplayCommand;
	TArray<FCacheReplayReader> Replays;

	FParsedNodeMap* ActiveParsedNodeMap = nullptr;
	FString ActiveNodeName;
	FString ActiveNodeConfig;

	bool bAsyncFound = false;
	bool bBootFound = false;
	bool bHierarchyFound = false;
	bool bVerifyFound = false;
	bool bVerifyFix = false;
};

} // UE::DerivedData

namespace UE::DerivedData
{

FDerivedDataBackend* FDerivedDataBackend::Create()
{
	return new FDerivedDataBackendGraph();
}

FDerivedDataBackend& FDerivedDataBackend::Get()
{
	return FDerivedDataBackendGraph::Get();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EBackendDebugKeyState
{
	None,
	HitGet,
	MissGet,
};

struct Private::FBackendDebugMissState
{
	FCriticalSection Lock;
	TMap<FCacheKey, EBackendDebugKeyState> Keys;
};

FBackendDebugOptions::FBackendDebugOptions()
{
	SimulateMissState.Get() = MakePimpl<Private::FBackendDebugMissState>();
}

/**
 * Parse debug options for the provided node name. Returns true if any options were specified
 */
bool FBackendDebugOptions::ParseFromTokens(FBackendDebugOptions& OutOptions, const TCHAR* InNodeName, const TCHAR* InInputTokens)
{
	// Check if the input stream has any DDC options for this node.
	TStringBuilder<64> Prefix;
	Prefix.Append(TEXTVIEW("-DDC-")).Append(InNodeName).Append(TEXTVIEW("-"));
	if (UE::String::FindFirst(InInputTokens, Prefix, ESearchCase::IgnoreCase) == INDEX_NONE)
	{
		return false;
	}

	const int32 PrefixLen = Prefix.Len();

	// Look for -DDC-Local-MissRate=, -DDC-Shared-MissRate=, etc.
	float MissRate = 0.0f;
	Prefix.Append(TEXTVIEW("MissRate="));
	FParse::Value(InInputTokens, *Prefix, MissRate);
	Prefix.RemoveAt(PrefixLen, Prefix.Len() - PrefixLen);

	// Look for -DDC-Local-MissTypes=AnimSeq+Audio, -DDC-Shared-MissType=AnimSeq+Audio, etc.
	Prefix.Append(TEXTVIEW("MissTypes="));
	OutOptions.SimulateMissFilter = FCacheKeyFilter::Parse(InInputTokens, *Prefix, MissRate);
	Prefix.RemoveAt(PrefixLen, Prefix.Len() - PrefixLen);

	// Look for -DDC-Local-MissSalt=, -DDC-Shared-MissSalt=, etc.
	uint32 Salt = 0;
	Prefix.Append(TEXTVIEW("MissSalt="));
	if (FParse::Value(InInputTokens, *Prefix, Salt))
	{
		OutOptions.SimulateMissFilter.SetSalt(Salt);
	}
	if (OutOptions.SimulateMissFilter)
	{
		UE_LOG(LogDerivedDataCache, Display,
			TEXT("%s: Using salt %s%u to filter cache keys to simulate misses on."),
			InNodeName, *Prefix, OutOptions.SimulateMissFilter.GetSalt());
	}
	Prefix.RemoveAt(PrefixLen, Prefix.Len() - PrefixLen);

	return true;
}

bool FBackendDebugOptions::ShouldSimulatePutMiss(const FCacheKey& Key)
{
	if (!SimulateMissFilter.IsMatch(Key))
	{
		return false;
	}

	Private::FBackendDebugMissState& State = *SimulateMissState.Get();
	const uint32 KeyHash = GetTypeHash(Key);

	FScopeLock Lock(&State.Lock);
	State.Keys.AddByHash(KeyHash, Key, EBackendDebugKeyState::HitGet);
	return false;
}

bool FBackendDebugOptions::ShouldSimulateGetMiss(const FCacheKey& Key)
{
	if (!SimulateMissFilter.IsMatch(Key))
	{
		return false;
	}

	Private::FBackendDebugMissState& State = *SimulateMissState.Get();
	const uint32 KeyHash = GetTypeHash(Key);

	FScopeLock Lock(&State.Lock);
	return State.Keys.FindOrAddByHash(KeyHash, Key, EBackendDebugKeyState::MissGet) == EBackendDebugKeyState::MissGet;
}

} // UE::DerivedData

#undef LOCTEXT_NAMESPACE
