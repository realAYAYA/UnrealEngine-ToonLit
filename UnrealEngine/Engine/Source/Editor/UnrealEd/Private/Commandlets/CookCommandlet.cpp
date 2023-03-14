// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#include "Commandlets/CookCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/TaskGraphInterfaces.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Cooker/CookProfiling.h"
#include "Cooker/PackageBuildDependencyTracker.h"
#include "CookerSettings.h"
#include "DerivedDataBuildRemoteExecutor.h"
#include "Editor.h"
#include "EngineGlobals.h"
#include "GameDelegates.h"
#include "GlobalShader.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MemoryBase.h"
#include "HAL/MemoryMisc.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFileManager.h"
#include "INetworkFileSystemModule.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/RedirectCollector.h"
#include "Modules/ModuleManager.h"
#include "PackageHelperFunctions.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/ArrayWriter.h"
#include "Settings/ProjectPackagingSettings.h"
#include "ShaderCompiler.h"
#include "Stats/StatsMisc.h"
#include "StudioAnalytics.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
DEFINE_LOG_CATEGORY_STATIC(LogCookCommandlet, Log, All);

#if ENABLE_COOK_STATS
#include "AnalyticsET.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "StudioAnalytics.h"
#include "Virtualization/VirtualizationSystem.h"

namespace DetailedCookStats
{

FString CookProject;
FString CookCultures;
FString CookLabel;
FString TargetPlatforms;
double CookWallTimeSec = 0.0;
double StartupWallTimeSec = 0.0;
double CookByTheBookTimeSec = 0.0;
double StartCookByTheBookTimeSec = 0.0;
extern double TickCookOnTheSideTimeSec;
extern double TickCookOnTheSideLoadPackagesTimeSec;
extern double TickCookOnTheSideResolveRedirectorsTimeSec;
extern double TickCookOnTheSideSaveCookedPackageTimeSec;
extern double TickCookOnTheSidePrepareSaveTimeSec;
extern double BlockOnAssetRegistryTimeSec;
extern double GameCookModificationDelegateTimeSec;
double TickLoopGCTimeSec = 0.0;
double TickLoopRecompileShaderRequestsTimeSec = 0.0;
double TickLoopShaderProcessAsyncResultsTimeSec = 0.0;
double TickLoopProcessDeferredCommandsTimeSec = 0.0;
double TickLoopTickCommandletStatsTimeSec = 0.0;
double TickLoopFlushRenderingCommandsTimeSec = 0.0;
bool IsCookAll = false;
bool IsCookOnTheFly = false;
bool IsIterativeCook = false;
bool IsFastCook = false;
bool IsUnversioned = false;

FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
{
	const FString StatName(TEXT("Cook.Profile"));
	#define ADD_COOK_STAT_FLT(Path, Name) AddStat(StatName, FCookStatsManager::CreateKeyValueArray(TEXT("Path"), TEXT(Path), TEXT(#Name), Name))
	ADD_COOK_STAT_FLT(" 0", CookWallTimeSec);
	ADD_COOK_STAT_FLT(" 0. 0", StartupWallTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1", CookByTheBookTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 0", StartCookByTheBookTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 0. 0", BlockOnAssetRegistryTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 0. 1", GameCookModificationDelegateTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1", TickCookOnTheSideTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1. 0", TickCookOnTheSideLoadPackagesTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1. 1", TickCookOnTheSideSaveCookedPackageTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1. 1. 0", TickCookOnTheSideResolveRedirectorsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 1. 2", TickCookOnTheSidePrepareSaveTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 2", TickLoopGCTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 3", TickLoopRecompileShaderRequestsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 4", TickLoopShaderProcessAsyncResultsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 5", TickLoopProcessDeferredCommandsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 6", TickLoopTickCommandletStatsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 7", TickLoopFlushRenderingCommandsTimeSec);
	ADD_COOK_STAT_FLT(" 0. 1. 8", TargetPlatforms);
	ADD_COOK_STAT_FLT(" 0. 1. 9", CookProject);
	ADD_COOK_STAT_FLT(" 0. 1. 10", CookCultures);
	ADD_COOK_STAT_FLT(" 0. 1. 11", IsCookAll);
	ADD_COOK_STAT_FLT(" 0. 1. 12", IsCookOnTheFly);
	ADD_COOK_STAT_FLT(" 0. 1. 13", IsIterativeCook);
	ADD_COOK_STAT_FLT(" 0. 1. 14", IsUnversioned);
	ADD_COOK_STAT_FLT(" 0. 1. 15", CookLabel);
	ADD_COOK_STAT_FLT(" 0. 1. 16", IsFastCook);
		
	#undef ADD_COOK_STAT_FLT
});

static void LogCookStats(const FString& CookCmdLine)
{
	if (FStudioAnalytics::IsAvailable())
	{

		// convert filtered stats directly to an analytics event
		TArray<FAnalyticsEventAttribute> StatAttrs;

		// Sends each cook stat to the studio analytics system.
		auto SendCookStatsToAnalytics = [&StatAttrs](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
		{
			for (const auto& Attr : StatAttributes)
			{
				FString FormattedAttrName = StatName + "." + Attr.Key;

				StatAttrs.Emplace(FormattedAttrName, Attr.Value);
			}
		};

		// Now actually grab the stats 
		FCookStatsManager::LogCookStats(SendCookStatsToAnalytics);

		// Record them all under cooking event
		FStudioAnalytics::GetProvider().RecordEvent(TEXT("Core.Cooking"), StatAttrs);

		FStudioAnalytics::GetProvider().BlockUntilFlushed(60.0f);
	}

	/** Used for custom logging of DDC Resource usage stats. */
	struct FDDCResourceUsageStat
	{
	public:
		FDDCResourceUsageStat(FString InAssetType, double InTotalTimeSec, bool bIsGameThreadTime, double InSizeMB, int64 InAssetsBuilt) : AssetType(MoveTemp(InAssetType)), TotalTimeSec(InTotalTimeSec), GameThreadTimeSec(bIsGameThreadTime ? InTotalTimeSec : 0.0), SizeMB(InSizeMB), AssetsBuilt(InAssetsBuilt) {}
		void Accumulate(const FDDCResourceUsageStat& OtherStat)
		{
			TotalTimeSec += OtherStat.TotalTimeSec;
			GameThreadTimeSec += OtherStat.GameThreadTimeSec;
			SizeMB += OtherStat.SizeMB;
			AssetsBuilt += OtherStat.AssetsBuilt;
		}
		FString AssetType;
		double TotalTimeSec;
		double GameThreadTimeSec;
		double SizeMB;
		int64 AssetsBuilt;
	};

	/** Used for custom TSet comparison of DDC Resource usage stats. */
	struct FDDCResourceUsageStatKeyFuncs : BaseKeyFuncs<FDDCResourceUsageStat, FString, false>
	{
		static const FString& GetSetKey(const FDDCResourceUsageStat& Element) { return Element.AssetType; }
		static bool Matches(const FString& A, const FString& B) { return A == B; }
		static uint32 GetKeyHash(const FString& Key) { return GetTypeHash(Key); }
	};

	/** Used to store profile data for custom logging. */
	struct FCookProfileData
	{
	public:
		FCookProfileData(FString InPath, FString InKey, FString InValue) : Path(MoveTemp(InPath)), Key(MoveTemp(InKey)), Value(MoveTemp(InValue)) {}
		FString Path;
		FString Key;
		FString Value;
	};

	// instead of printing the usage stats generically, we capture them so we can log a subset of them in an easy-to-read way.
	TSet<FDDCResourceUsageStat, FDDCResourceUsageStatKeyFuncs> DDCResourceUsageStats;
	TArray<FCookStatsManager::StringKeyValue> DDCSummaryStats;
	TArray<FCookProfileData> CookProfileData;
	TArray<FString> StatCategories;
	TMap<FString, TArray<FCookStatsManager::StringKeyValue>> StatsInCategories;

	/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
	auto LogStatsFunc = [&DDCResourceUsageStats, &DDCSummaryStats, &CookProfileData, &StatCategories, &StatsInCategories]
	(const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
	{
		// Some stats will use custom formatting to make a visibly pleasing summary.
		bool bStatUsedCustomFormatting = false;

		if (StatName == TEXT("DDC.Usage"))
		{
			// Don't even log this detailed DDC data. It's mostly only consumable by ingestion into pivot tools.
			bStatUsedCustomFormatting = true;
		}
		else if (StatName.EndsWith(TEXT(".Usage"), ESearchCase::IgnoreCase))
		{
			// Anything that ends in .Usage is assumed to be an instance of FCookStats.FDDCResourceUsageStats. We'll log that using custom formatting.
			FString AssetType = StatName;
			AssetType.RemoveFromEnd(TEXT(".Usage"), ESearchCase::IgnoreCase);
			// See if the asset has a subtype (found via the "Node" parameter")
			const FCookStatsManager::StringKeyValue* AssetSubType = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Node"); });
			if (AssetSubType && AssetSubType->Value.Len() > 0)
			{
				AssetType += FString::Printf(TEXT(" (%s)"), *AssetSubType->Value);
			}
			// Pull the Time and Size attributes and AddOrAccumulate them into the set of stats. Ugly string/container manipulation code courtesy of UE/C++.
			const FCookStatsManager::StringKeyValue* AssetTimeSecAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("TimeSec"); });
			double AssetTimeSec = 0.0;
			if (AssetTimeSecAttr)
			{
				LexFromString(AssetTimeSec, *AssetTimeSecAttr->Value);
			}
			const FCookStatsManager::StringKeyValue* AssetSizeMBAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("MB"); });
			double AssetSizeMB = 0.0;
			if (AssetSizeMBAttr)
			{
				LexFromString(AssetSizeMB, *AssetSizeMBAttr->Value);
			}
			const FCookStatsManager::StringKeyValue* ThreadNameAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("ThreadName"); });
			bool bIsGameThreadTime = ThreadNameAttr != nullptr && ThreadNameAttr->Value == TEXT("GameThread");

			const FCookStatsManager::StringKeyValue* HitOrMissAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("HitOrMiss"); });
			bool bWasMiss = HitOrMissAttr != nullptr && HitOrMissAttr->Value == TEXT("Miss");
			int64 AssetsBuilt = 0;
			if (bWasMiss)
			{
				const FCookStatsManager::StringKeyValue* CountAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Count"); });
				if (CountAttr)
				{
					LexFromString(AssetsBuilt, *CountAttr->Value);
				}
			}


			FDDCResourceUsageStat Stat(AssetType, AssetTimeSec, bIsGameThreadTime, AssetSizeMB, AssetsBuilt);
			FDDCResourceUsageStat* ExistingStat = DDCResourceUsageStats.Find(Stat.AssetType);
			if (ExistingStat)
			{
				ExistingStat->Accumulate(Stat);
			}
			else
			{
				DDCResourceUsageStats.Add(Stat);
			}
			bStatUsedCustomFormatting = true;
		}
		else if (StatName == TEXT("DDC.Summary"))
		{
			DDCSummaryStats.Append(StatAttributes);
			bStatUsedCustomFormatting = true;
		}
		else if (StatName == TEXT("Cook.Profile"))
		{
			if (StatAttributes.Num() >= 2)
			{
				CookProfileData.Emplace(StatAttributes[0].Value, StatAttributes[1].Key, StatAttributes[1].Value);
			}
			bStatUsedCustomFormatting = true;
		}

		// if a stat doesn't use custom formatting, just spit out the raw info.
		if (!bStatUsedCustomFormatting)
		{
			TArray<FCookStatsManager::StringKeyValue>& StatsInCategory = StatsInCategories.FindOrAdd(StatName);
			if (StatsInCategory.Num() == 0)
			{
				StatCategories.Add(StatName);
			}
			StatsInCategory.Append(StatAttributes);
		}
	};

	FCookStatsManager::LogCookStats(LogStatsFunc);

	UE_LOG(LogCookCommandlet, Display, TEXT("Misc Cook Stats"));
	UE_LOG(LogCookCommandlet, Display, TEXT("==============="));
	for (FString& StatCategory : StatCategories)
	{
		UE_LOG(LogCookCommandlet, Display, TEXT("%s"), *StatCategory);
		TArray<FCookStatsManager::StringKeyValue>& StatsInCategory = StatsInCategories.FindOrAdd(StatCategory);

		// log each key/value pair, with the equal signs lined up.
		for (const FCookStatsManager::StringKeyValue& StatKeyValue : StatsInCategory)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("    %s=%s"), *StatKeyValue.Key, *StatKeyValue.Value);
		}
	}

	// DDC Usage stats are custom formatted, and the above code just accumulated them into a TSet. Now log it with our special formatting for readability.
	if (CookProfileData.Num() > 0)
	{
		UE_LOG(LogCookCommandlet, Display, TEXT(""));
		UE_LOG(LogCookCommandlet, Display, TEXT("Cook Profile"));
		UE_LOG(LogCookCommandlet, Display, TEXT("============"));
		for (const auto& ProfileEntry : CookProfileData)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("%s.%s=%s"), *ProfileEntry.Path, *ProfileEntry.Key, *ProfileEntry.Value);
		}
	}
	if (DDCSummaryStats.Num() > 0)
	{
		UE_LOG(LogCookCommandlet, Display, TEXT(""));
		UE_LOG(LogCookCommandlet, Display, TEXT("DDC Summary Stats"));
		UE_LOG(LogCookCommandlet, Display, TEXT("================="));
		for (const auto& Attr : DDCSummaryStats)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("%-16s=%10s"), *Attr.Key, *Attr.Value);
		}
	}

	DumpDerivedDataBuildRemoteExecutorStats();

	if (DDCResourceUsageStats.Num() > 0)
	{
		// sort the list
		TArray<FDDCResourceUsageStat> SortedDDCResourceUsageStats;
		SortedDDCResourceUsageStats.Empty(DDCResourceUsageStats.Num());
		for (const FDDCResourceUsageStat& Stat : DDCResourceUsageStats)
		{
			SortedDDCResourceUsageStats.Emplace(Stat);
		}
		SortedDDCResourceUsageStats.Sort([](const FDDCResourceUsageStat& LHS, const FDDCResourceUsageStat& RHS)
			{
				return LHS.TotalTimeSec > RHS.TotalTimeSec;
			});

		UE_LOG(LogCookCommandlet, Display, TEXT(""));
		UE_LOG(LogCookCommandlet, Display, TEXT("DDC Resource Stats"));
		UE_LOG(LogCookCommandlet, Display, TEXT("======================================================================================================="));
		UE_LOG(LogCookCommandlet, Display, TEXT("Asset Type                          Total Time (Sec)  GameThread Time (Sec)  Assets Built  MB Processed"));
		UE_LOG(LogCookCommandlet, Display, TEXT("----------------------------------  ----------------  ---------------------  ------------  ------------"));
		for (const FDDCResourceUsageStat& Stat : SortedDDCResourceUsageStats)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("%-34s  %16.2f  %21.2f  %12d  %12.2f"), *Stat.AssetType, Stat.TotalTimeSec, Stat.GameThreadTimeSec, Stat.AssetsBuilt, Stat.SizeMB);
		}
	}

	DumpBuildDependencyTrackerStats();

	if (UE::Virtualization::IVirtualizationSystem::IsInitialized())
	{
		UE::Virtualization::IVirtualizationSystem::Get().DumpStats();
	}
}

}
#endif

namespace UE::Cook
{

struct FScopeRootObject
{
	UObject* Object;
	FScopeRootObject(UObject* InObject) : Object(InObject)
	{
		Object->AddToRoot();
	}

	~FScopeRootObject()
	{
		Object->RemoveFromRoot();
	}
};

}

UCookCommandlet::UCookCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

	LogToConsole = false;
}

bool UCookCommandlet::CookOnTheFly( FGuid InstanceId, int32 Timeout, bool bForceClose, const TArray<ITargetPlatform*>& TargetPlatforms)
{
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	UE::Cook::FScopeRootObject S(CookOnTheFlyServer);

	UCookerSettings const* CookerSettings = GetDefault<UCookerSettings>();
	ECookInitializationFlags IterateFlags = ECookInitializationFlags::Iterative;

	ECookInitializationFlags CookFlags = ECookInitializationFlags::None;
	CookFlags |= bIterativeCooking ? IterateFlags : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookFlags |= bCookEditorOptional ? ECookInitializationFlags::CookEditorOptional : ECookInitializationFlags::None;
	CookFlags |= bIgnoreIniSettingsOutOfDate || CookerSettings->bIgnoreIniSettingsOutOfDateForIteration ? ECookInitializationFlags::IgnoreIniSettingsOutOfDate : ECookInitializationFlags::None;
	CookOnTheFlyServer->Initialize( ECookMode::CookOnTheFly, CookFlags );

	UCookOnTheFlyServer::FCookOnTheFlyStartupOptions CookOnTheFlyStartupOptions;
	CookOnTheFlyStartupOptions.bBindAnyPort = InstanceId.IsValid();
	CookOnTheFlyStartupOptions.bZenStore = Switches.Contains(TEXT("ZenStore"));
	CookOnTheFlyStartupOptions.bPlatformProtocol = Switches.Contains(TEXT("PlatformProtocol"));
	CookOnTheFlyStartupOptions.TargetPlatforms = TargetPlatforms;

	if (CookOnTheFlyServer->StartCookOnTheFly(MoveTemp(CookOnTheFlyStartupOptions)) == false)
	{
		return false;
	}

	if ( InstanceId.IsValid() )
	{
		if ( CookOnTheFlyServer->BroadcastFileserverPresence(InstanceId) == false )
		{
			return false;
		}
	}

	FDateTime LastConnectionTime = FDateTime::UtcNow();
	bool bHadConnection = false;

	while (!IsEngineExitRequested())
	{
		uint32 TickResults = CookOnTheFlyServer->TickCookOnTheFly(/*TimeSlice =*/MAX_flt,
			ShowProgress ? ECookTickFlags::None : ECookTickFlags::HideProgressDisplay);
		ConditionalCollectGarbage(TickResults, *CookOnTheFlyServer);

		if (!CookOnTheFlyServer->HasRemainingWork() && !IsEngineExitRequested())
		{
			// handle server timeout
			if (InstanceId.IsValid() || bForceClose)
			{
				if (CookOnTheFlyServer->NumConnections() > 0)
				{
					bHadConnection = true;
					LastConnectionTime = FDateTime::UtcNow();
				}

				if ((FDateTime::UtcNow() - LastConnectionTime) > FTimespan::FromSeconds(Timeout))
				{
					uint32 Result = FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "FileServerIdle", "The file server did not receive any connections in the past 3 minutes. Would you like to shut it down?"));

					if (Result == EAppReturnType::No && !bForceClose)
					{
						LastConnectionTime = FDateTime::UtcNow();
					}
					else
					{
						RequestEngineExit(TEXT("Cook file server idle"));
					}
				}
				else if (bHadConnection && (CookOnTheFlyServer->NumConnections() == 0) && bForceClose) // immediately shut down if we previously had a connection and now do not
				{
					RequestEngineExit(TEXT("Cook file server lost last connection"));
				}
			}

			CookOnTheFlyServer->WaitForRequests(100 /* timeoutMs */);
		}
	}

	CookOnTheFlyServer->ShutdownCookOnTheFly();
	return true;
}

/* UCommandlet interface
 *****************************************************************************/

int32 UCookCommandlet::Main(const FString& CmdLineParams)
{
	COOK_STAT(double CookStartTime = FPlatformTime::Seconds());
	Params = CmdLineParams;
	ParseCommandLine(*Params, Tokens, Switches);

	bCookOnTheFly = Switches.Contains(TEXT("COOKONTHEFLY"));   // Prototype cook-on-the-fly server
	bCookAll = Switches.Contains(TEXT("COOKALL"));   // Cook everything
	bUnversioned = Switches.Contains(TEXT("UNVERSIONED"));   // Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes.
	bCookEditorOptional = Switches.Contains(TEXT("EDITOROPTIONAL")); // Produce the optional editor package data alongside the cooked data.
	bGenerateStreamingInstallManifests = Switches.Contains(TEXT("MANIFESTS"));   // Generate manifests for building streaming install packages
	bIterativeCooking = Switches.Contains(TEXT("ITERATE"));
	bSkipEditorContent = Switches.Contains(TEXT("SKIPEDITORCONTENT")); // This won't save out any packages in Engine/Content/Editor*
	bErrorOnEngineContentUse = Switches.Contains(TEXT("ERRORONENGINECONTENTUSE"));
	bUseSerializationForGeneratingPackageDependencies = Switches.Contains(TEXT("UseSerializationForGeneratingPackageDependencies"));
	bCookSinglePackage = Switches.Contains(TEXT("cooksinglepackagenorefs"));
	bKeepSinglePackageRefs = Switches.Contains(TEXT("cooksinglepackage")); // This is a legacy parameter; it's a minor misnomer since singlepackage implies norefs, but we want to avoiding changing the behavior
	bCookSinglePackage = bCookSinglePackage || bKeepSinglePackageRefs;
	bVerboseCookerWarnings = Switches.Contains(TEXT("verbosecookerwarnings"));
	bPartialGC = Switches.Contains(TEXT("Partialgc"));
	ShowErrorCount = !Switches.Contains(TEXT("DIFFONLY"));
	ShowProgress = !Switches.Contains(TEXT("DIFFONLY"));
	bIgnoreIniSettingsOutOfDate = Switches.Contains(TEXT("IgnoreIniSettingsOutOfDate"));
	bFastCook = Switches.Contains(TEXT("FastCook"));

	COOK_STAT(DetailedCookStats::IsCookAll = bCookAll);
	COOK_STAT(DetailedCookStats::IsCookOnTheFly = bCookOnTheFly);
	COOK_STAT(DetailedCookStats::IsIterativeCook = bIterativeCooking);
	COOK_STAT(DetailedCookStats::IsFastCook = bFastCook);
	COOK_STAT(DetailedCookStats::IsUnversioned = bUnversioned);

	COOK_STAT(DetailedCookStats::CookProject = FApp::GetProjectName());

	COOK_STAT(FParse::Value(*Params, TEXT("CookCultures="), DetailedCookStats::CookCultures));
	COOK_STAT(FParse::Value(*Params, TEXT("CookLabel="), DetailedCookStats::CookLabel));
	
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	if ( bCookOnTheFly )
	{
		// In cook on the fly, if the user did not provide a targetplatform on the commandline, then we do not intialize any platforms up front; we wait for the first connection.
		// TPM.GetActiveTargetPlatforms defaults to the currently running platform (e.g. Windows, with editor) in the no-target case, so we need to only call GetActiveTargetPlatforms
		// if targetplatform was on the commandline
		FString Unused;
		TArray<ITargetPlatform*> TargetPlatforms;
		if (FParse::Value(FCommandLine::Get(), TEXT("TARGETPLATFORM="), Unused))
		{
			TargetPlatforms = TPM.GetActiveTargetPlatforms();
		}

		// parse instance identifier
		FString InstanceIdString;
		bool bForceClose = Switches.Contains(TEXT("FORCECLOSE"));

		FGuid InstanceId;
		if (FParse::Value(*Params, TEXT("InstanceId="), InstanceIdString))
		{
			if (!FGuid::Parse(InstanceIdString, InstanceId))
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
			}
		}

		int32 Timeout = 180;
		if (!FParse::Value(*Params, TEXT("timeout="), Timeout))
		{
			Timeout = 180;
		}

		CookOnTheFly( InstanceId, Timeout, bForceClose, TargetPlatforms);
	}
	else if (Switches.Contains(TEXT("COOKWORKER")))
	{
		CookAsCookWorker();
	}
	else
	{
		const TArray<ITargetPlatform*>& Platforms = TPM.GetActiveTargetPlatforms();

		CookByTheBook(Platforms);
		
		if(GShaderCompilerStats)
		{
			GShaderCompilerStats->WriteStats();
		}

		// Use -LogCookStats to log the results to the command line after the cook (happens automatically on a build machine)
		COOK_STAT(
		{
			double Now = FPlatformTime::Seconds();
			DetailedCookStats::CookWallTimeSec = Now - GStartTime;
			DetailedCookStats::StartupWallTimeSec = CookStartTime - GStartTime;
			DetailedCookStats::LogCookStats(CmdLineParams);

			FStudioAnalytics::FireEvent_Loading(TEXT("CookByTheBook"), DetailedCookStats::CookWallTimeSec);
		});
	}
	return 0;
}

bool UCookCommandlet::CookByTheBook( const TArray<ITargetPlatform*>& Platforms)
{
#if OUTPUT_COOKTIMING
	TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(CookByTheBook, CookChannel);
#endif // OUTPUT_COOKTIMING

	COOK_STAT(FScopedDurationTimer CookByTheBookTimer(DetailedCookStats::CookByTheBookTimeSec));
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();
	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	UE::Cook::FScopeRootObject S(CookOnTheFlyServer);

	UCookerSettings const* CookerSettings = GetDefault<UCookerSettings>();
	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	ECookInitializationFlags IterateFlags = ECookInitializationFlags::Iterative;

	if (Switches.Contains(TEXT("IterateSharedCookedbuild")))
	{
		// Add shared build flag to method flag, and enable iterative
		IterateFlags |= ECookInitializationFlags::IterateSharedBuild;
		
		bIterativeCooking = true;
	}
	
	ECookInitializationFlags CookFlags = ECookInitializationFlags::IncludeServerMaps;
	CookFlags |= bIterativeCooking ? IterateFlags : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;	
	CookFlags |= bUseSerializationForGeneratingPackageDependencies ? ECookInitializationFlags::UseSerializationForPackageDependencies : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookFlags |= bCookEditorOptional ? ECookInitializationFlags::CookEditorOptional : ECookInitializationFlags::None;
	CookFlags |= bVerboseCookerWarnings ? ECookInitializationFlags::OutputVerboseCookerWarnings : ECookInitializationFlags::None;
	CookFlags |= bPartialGC ? ECookInitializationFlags::EnablePartialGC : ECookInitializationFlags::None;
	bool bTestCook = Switches.Contains(TEXT("TestCook"));
	CookFlags |= bTestCook ? ECookInitializationFlags::TestCook : ECookInitializationFlags::None;
	CookFlags |= Switches.Contains(TEXT("LogDebugInfo")) ? ECookInitializationFlags::LogDebugInfo : ECookInitializationFlags::None;
	CookFlags |= bIgnoreIniSettingsOutOfDate || CookerSettings->bIgnoreIniSettingsOutOfDateForIteration ? ECookInitializationFlags::IgnoreIniSettingsOutOfDate : ECookInitializationFlags::None;
	CookFlags |= Switches.Contains(TEXT("IgnoreScriptPackagesOutOfDate")) || CookerSettings->bIgnoreScriptPackagesOutOfDateForIteration ? ECookInitializationFlags::IgnoreScriptPackagesOutOfDate : ECookInitializationFlags::None;

	//////////////////////////////////////////////////////////////////////////
	// parse commandline options 

	FString DLCName;
	FParse::Value( *Params, TEXT("DLCNAME="), DLCName);

	FString BasedOnReleaseVersion;
	FParse::Value( *Params, TEXT("BasedOnReleaseVersion="), BasedOnReleaseVersion);

	FString CreateReleaseVersion;
	FParse::Value( *Params, TEXT("CreateReleaseVersion="), CreateReleaseVersion);

	FString OutputDirectoryOverride;
	FParse::Value( *Params, TEXT("OutputDir="), OutputDirectoryOverride);

	TArray<FString> CmdLineMapEntries;
	TArray<FString> CmdLineDirEntries;
	TArray<FString> CmdLineCultEntries;
	TArray<FString> CmdLineNeverCookDirEntries;
	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		const FString& Switch = Switches[SwitchIdx];

		auto GetSwitchValueElements = [&Switch](const FString SwitchKey) -> TArray<FString>
		{
			TArray<FString> ValueElements;
			if (Switch.StartsWith(SwitchKey + TEXT("=")) == true)
			{
				FString ValuesList = Switch.Right(Switch.Len() - (SwitchKey + TEXT("=")).Len());

				// Allow support for -KEY=Value1+Value2+Value3 as well as -KEY=Value1 -KEY=Value2
				for (int32 PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive); PlusIdx != INDEX_NONE; PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive))
				{
					const FString ValueElement = ValuesList.Left(PlusIdx);
					ValueElements.Add(ValueElement);

					ValuesList.RightInline(ValuesList.Len() - (PlusIdx + 1), false);
				}
				ValueElements.Add(ValuesList);
			}
			return ValueElements;
		};

		// Check for -MAP=<name of map> entries
		CmdLineMapEntries += GetSwitchValueElements(TEXT("MAP"));
		CmdLineMapEntries += GetSwitchValueElements(TEXT("PACKAGE"));

		// Check for -COOKDIR=<path to directory> entries
		const FString CookDirPrefix = TEXT("COOKDIR=");
		if (Switch.StartsWith(CookDirPrefix))
		{
			FString Entry = Switch.Mid(CookDirPrefix.Len()).TrimQuotes();
			FPaths::NormalizeDirectoryName(Entry);
			CmdLineDirEntries.Add(Entry);
		}

		// Check for -NEVERCOOKDIR=<path to directory> entries
		for (FString& NeverCookDir : GetSwitchValueElements(TEXT("NEVERCOOKDIR")))
		{
			FPaths::NormalizeDirectoryName(NeverCookDir);
			CmdLineNeverCookDirEntries.Add(MoveTemp(NeverCookDir));
		}

		// Check for -COOKCULTURES=<culture name> entries
		CmdLineCultEntries += GetSwitchValueElements(TEXT("COOKCULTURES"));
	}

	CookOnTheFlyServer->Initialize(ECookMode::CookByTheBook, CookFlags, OutputDirectoryOverride);

	TArray<FString> MapIniSections;
	FString SectionStr;
	if (FParse::Value(*Params, TEXT("MAPINISECTION="), SectionStr))
	{
		if (SectionStr.Contains(TEXT("+")))
		{
			TArray<FString> Sections;
			SectionStr.ParseIntoArray(Sections, TEXT("+"), true);
			for (int32 Index = 0; Index < Sections.Num(); Index++)
			{
				MapIniSections.Add(Sections[Index]);
			}
		}
		else
		{
			MapIniSections.Add(SectionStr);
		}
	}

	// Set the list of cultures to cook as those on the commandline, if specified.
	// Otherwise, use the project packaging settings.
	TArray<FString> CookCultures;
	if (Switches.ContainsByPredicate([](const FString& Switch) -> bool
		{
			return Switch.StartsWith("COOKCULTURES=");
		}))
	{
		CookCultures = CmdLineCultEntries;
	}
	else
	{
		CookCultures = PackagingSettings->CulturesToStage;
	}

	const bool bUseZenStore =
		!Switches.Contains(TEXT("SkipZenStore")) &&
		(Switches.Contains(TEXT("ZenStore")) || PackagingSettings->bUseZenStore);

	//////////////////////////////////////////////////////////////////////////
	// start cook by the book 
	ECookByTheBookOptions CookOptions = ECookByTheBookOptions::None;

	CookOptions |= bCookAll ? ECookByTheBookOptions::CookAll : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("MAPSONLY")) ? ECookByTheBookOptions::MapsOnly : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("NODEV")) ? ECookByTheBookOptions::NoDevContent : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("FullLoadAndSave")) ? ECookByTheBookOptions::FullLoadAndSave : ECookByTheBookOptions::None;
	CookOptions |= bUseZenStore ? ECookByTheBookOptions::ZenStore : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("NoGameAlwaysCook")) ? ECookByTheBookOptions::NoGameAlwaysCookPackages : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("DisableUnsolicitedPackages")) ? (ECookByTheBookOptions::SkipHardReferences | ECookByTheBookOptions::SkipSoftReferences) : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("NoDefaultMaps")) ? ECookByTheBookOptions::NoDefaultMaps : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("SkipSoftReferences")) ? ECookByTheBookOptions::SkipSoftReferences : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("SkipHardReferences")) ? ECookByTheBookOptions::SkipHardReferences : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("CookAgainstFixedBase")) ? ECookByTheBookOptions::CookAgainstFixedBase : ECookByTheBookOptions::None;
	CookOptions |= (Switches.Contains(TEXT("DlcLoadMainAssetRegistry")) || !bErrorOnEngineContentUse) ? ECookByTheBookOptions::DlcLoadMainAssetRegistry : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("DlcReevaluateUncookedAssets")) ? ECookByTheBookOptions::DlcReevaluateUncookedAssets : ECookByTheBookOptions::None;

	if (bCookSinglePackage)
	{
		const ECookByTheBookOptions SinglePackageFlags = ECookByTheBookOptions::NoAlwaysCookMaps | ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages |
			ECookByTheBookOptions::NoInputPackages | ECookByTheBookOptions::SkipSoftReferences | ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
		CookOptions |= SinglePackageFlags;
		CookOptions |= bKeepSinglePackageRefs ? ECookByTheBookOptions::None : ECookByTheBookOptions::SkipHardReferences;
	}

	// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory or start with a / root
	if (!(CookOptions & ECookByTheBookOptions::NoGameAlwaysCookPackages))
	{
		for (const FDirectoryPath& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
		{
			FString LocalPath;
			if (FPackageName::TryConvertGameRelativePackagePathToLocalPath(DirToCook.Path, LocalPath))
			{
				CmdLineDirEntries.Add(LocalPath);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("'ProjectSettings -> PackagingSettings -> Directories to always cook' has invalid element '%s'"), *DirToCook.Path);
			}
		}
	}

	UCookOnTheFlyServer::FCookByTheBookStartupOptions StartupOptions;

	// Validate target platforms and add them to StartupOptions
	for (ITargetPlatform* TargetPlatform : Platforms)
	{
		if (TargetPlatform)
		{
			if (TargetPlatform->HasEditorOnlyData())
			{
				UE_LOG(LogCook, Warning, TEXT("Target platform \"%s\" is an editor platform and can not be a cook target"), *TargetPlatform->PlatformName());
			}
			else
			{
				StartupOptions.TargetPlatforms.Add(TargetPlatform);
			}
		}
	}
	if (!StartupOptions.TargetPlatforms.Num())
	{
		UE_LOG(LogCook, Error, TEXT("No target platforms specified or all target platforms are invalid"));
		return false;
	}

	Swap( StartupOptions.CookMaps, CmdLineMapEntries);
	Swap( StartupOptions.CookDirectories, CmdLineDirEntries );
	Swap( StartupOptions.NeverCookDirectories, CmdLineNeverCookDirEntries);
	Swap( StartupOptions.CookCultures, CookCultures );
	Swap( StartupOptions.DLCName, DLCName );
	Swap( StartupOptions.BasedOnReleaseVersion, BasedOnReleaseVersion );
	Swap( StartupOptions.CreateReleaseVersion, CreateReleaseVersion );
	Swap( StartupOptions.IniMapSections, MapIniSections);
	StartupOptions.CookOptions = CookOptions;
	StartupOptions.bErrorOnEngineContentUse = bErrorOnEngineContentUse;
	StartupOptions.bGenerateDependenciesForMaps = Switches.Contains(TEXT("GenerateDependenciesForMaps"));
	StartupOptions.bGenerateStreamingInstallManifests = bGenerateStreamingInstallManifests;

	COOK_STAT(
	{
		for (const auto& Platform : Platforms)
		{
			DetailedCookStats::TargetPlatforms += Platform->PlatformName() + TEXT("+");
		}
		if (!DetailedCookStats::TargetPlatforms.IsEmpty())
		{
			DetailedCookStats::TargetPlatforms.RemoveFromEnd(TEXT("+"));
		}
	});

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FLowLevelMemTracker::Get().UpdateStatsPerFrame();
#endif

	do
	{
		{
			COOK_STAT(FScopedDurationTimer StartCookByTheBookTimer(DetailedCookStats::StartCookByTheBookTimeSec));
			CookOnTheFlyServer->StartCookByTheBook(StartupOptions);
		}
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CookByTheBook.MainLoop"), STAT_CookByTheBook_MainLoop, STATGROUP_LoadTime);
		if (CookOnTheFlyServer->IsFullLoadAndSave())
		{
			CookOnTheFlyServer->CookFullLoadAndSave();
		}
		else
		{
			while (CookOnTheFlyServer->IsInSession())
			{
				uint32 TickResults = 0;
				uint32 UnusedVariable = 0;

				TickResults = CookOnTheFlyServer->TickCookByTheBook(MAX_flt,
					ShowProgress ? ECookTickFlags::None : ECookTickFlags::HideProgressDisplay);
				ConditionalCollectGarbage(TickResults, *CookOnTheFlyServer);
			}
		}
	} while (bTestCook);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FLowLevelMemTracker::Get().UpdateStatsPerFrame();
#endif

	if (StartupOptions.DLCName.IsEmpty())
	{
		bool bFullReferencesExpected = !(CookOptions & ECookByTheBookOptions::SkipHardReferences);
		UE::SavePackageUtilities::VerifyEDLCookInfo(bFullReferencesExpected);
	}

	return true;
}

bool UCookCommandlet::CookAsCookWorker()
{
#if OUTPUT_COOKTIMING
	TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(CookAsCookWorker, CookChannel);
#endif // OUTPUT_COOKTIMING

	UCookOnTheFlyServer* CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();
	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below
	UE::Cook::FScopeRootObject S(CookOnTheFlyServer);

	if (!CookOnTheFlyServer->TryInitializeCookWorker())
	{
		UE_LOG(LogCook, Display, TEXT("CookWorker initialization failed, aborting CookCommandlet."));
		return false;
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CookByTheBook.MainLoop"), STAT_CookByTheBook_MainLoop, STATGROUP_LoadTime);
		while (CookOnTheFlyServer->IsInSession())
		{
			uint32 TickResults = CookOnTheFlyServer->TickCookWorker();
			ConditionalCollectGarbage(TickResults, *CookOnTheFlyServer);
		}
	}
	CookOnTheFlyServer->ShutdownCookAsCookWorker();

	return true;
}

void UCookCommandlet::ConditionalCollectGarbage(uint32 TickResults, UCookOnTheFlyServer& COTFS)
{
	if ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC) == 0)
	{
		return;
	}

	FString GCReason;
	if ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC_PackageCount) != 0)
	{
		GCReason = TEXT("Exceeded packages per GC");
	}
	else if ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC_OOM) != 0)
	{
		// this can cause thrashing if the cooker loads the same stuff into memory next tick
		GCReason = TEXT("Exceeded Max Memory");

		int32 JobsToLogAt = GShaderCompilingManager->GetNumRemainingJobs();

		UE_SCOPED_COOKTIMER(CookByTheBook_ShaderJobFlush);
		UE_LOG(LogCookCommandlet, Display, TEXT("Detected max mem exceeded - forcing shader compilation flush"));
		while (true)
		{
			int32 NumRemainingJobs = GShaderCompilingManager->GetNumRemainingJobs();
			if (NumRemainingJobs < 1000)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Finished flushing shader jobs at %d"), NumRemainingJobs);
				break;
			}

			if (NumRemainingJobs < JobsToLogAt)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Flushing shader jobs, remaining jobs %d"), NumRemainingJobs);
			}

			GShaderCompilingManager->ProcessAsyncResults(false, false);

			FPlatformProcess::Sleep(0.05);

			// GShaderCompilingManager->FinishAllCompilation();
		}
	}
	else if (TickResults & UCookOnTheFlyServer::COSR_RequiresGC_IdleTimer)
	{
		GCReason = TEXT("Cooker has been idle for long time gc");
	}
	else
	{
		// cooker loaded some object which needs to be cleaned up before the cooker can proceed so force gc
		GCReason = TEXT("COSR_RequiresGC");
	}

	// Flush the asset registry before GC
	{
		UE_SCOPED_COOKTIMER(CookByTheBook_TickAssetRegistry);
		FAssetRegistryModule::TickAssetRegistry(-1.0f);
	}
#if OUTPUT_COOKTIMING
	TOptional<FScopedDurationTimer> CBTBScopedDurationTimer;
	if (!COTFS.IsCookOnTheFlyMode())
	{
		CBTBScopedDurationTimer.Emplace(DetailedCookStats::TickLoopGCTimeSec);
	}
#endif
	UE_SCOPED_COOKTIMER(CookCommandlet_GC);

	const FPlatformMemoryStats MemStatsBeforeGC = FPlatformMemory::GetStats();
	int32 NumObjectsBeforeGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
	int32 NumObjectsAvailableBeforeGC = GUObjectArray.GetObjectArrayEstimatedAvailable();
	UE_LOG(LogCookCommandlet, Display, TEXT("GarbageCollection...%s (%s)"), (bPartialGC ? TEXT(" partial gc") : TEXT("")), *GCReason);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FLowLevelMemTracker::Get().UpdateStatsPerFrame();
#endif
	auto DumpMemStats = []()
	{
		FGenericMemoryStats MemStats;
		GMalloc->GetAllocatorStats(MemStats);
		for (const auto& Item : MemStats.Data)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT("Item %s = %d"), *Item.Key, Item.Value);
		}
	};

	if (!COTFS.IsCookOnTheFlyMode())
	{
		DumpMemStats();
	}

	CollectGarbage(RF_NoFlags);

	int32 NumObjectsAfterGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
	int32 NumObjectsAvailableAfterGC = GUObjectArray.GetObjectArrayEstimatedAvailable();
	FPlatformMemoryStats MemStatsAfterGC = FPlatformMemory::GetStats();
	if (!COTFS.IsCookOnTheFlyMode())
	{
		int64 VirtualMemBeforeGC = MemStatsBeforeGC.UsedVirtual;
		int64 VirtualMemAfterGC = MemStatsAfterGC.UsedVirtual;
		int64 VirtualMemFreed = MemStatsBeforeGC.UsedVirtual - MemStatsAfterGC.UsedVirtual;
		constexpr int32 BytesPerMeg = 1000000;
		UE_LOG(LogCookCommandlet, Display, TEXT("GarbageCollection Results:\n")
			TEXT("\tType: %s\n")
			TEXT("\tNumObjects:\n")
			TEXT("\t\tBefore GC:        %10d\n")
			TEXT("\t\tAvailable Before: %10d\n")
			TEXT("\t\tAfter GC:         %10d\n")
			TEXT("\t\tAvailable After:  %10d\n")
			TEXT("\t\tFreed by GC:      %10d\n")
			TEXT("\tVirtual Memory:\n")
			TEXT("\t\tBefore GC:        %10" INT64_FMT " MB\n")
			TEXT("\t\tAfter GC:         %10" INT64_FMT " MB\n")
			TEXT("\t\tFreed by GC:      %10" INT64_FMT " MB\n"),
			(bPartialGC ? TEXT("Partial") : TEXT("Full")),
			NumObjectsBeforeGC, NumObjectsAvailableBeforeGC, NumObjectsAfterGC, NumObjectsAvailableAfterGC,
			NumObjectsBeforeGC - NumObjectsAfterGC,
			VirtualMemBeforeGC / BytesPerMeg, VirtualMemAfterGC / BytesPerMeg, VirtualMemFreed / BytesPerMeg
		);

		DumpMemStats();
	}

	if (TickResults & UCookOnTheFlyServer::COSR_RequiresGC_OOM)
	{
		COTFS.EvaluateGarbageCollectionResults(NumObjectsBeforeGC, MemStatsBeforeGC, NumObjectsAfterGC, MemStatsAfterGC);
	} 
}