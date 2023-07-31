// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstallBundleManagerUtil.h"

#include "DefaultInstallBundleManagerPrivate.h"
#include "Misc/App.h"

#include "JsonObjectConverter.h"
#include "BuildPatchSettings.h"

#include "InstallBundleSourceInterface.h"
#include "InstallBundleCache.h"
#include "InstallBundleSourceBulk.h"

#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"

#if !defined(WITH_PLATFORM_INSTALL_BUNDLE_SOURCE)
	#define WITH_PLATFORM_INSTALL_BUNDLE_SOURCE 0
#endif

#if WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
	#include "PlatformInstallBundleSource.h"
#endif

#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"

DEFAULTINSTALLBUNDLEMANAGER_API FString GInstallBundleManagerIni;

DEFINE_LOG_CATEGORY(LogDefaultInstallBundleManager);

// Json writer subclass to allow us to avoid using a SharedPtr to write basic Json.
typedef TCondensedJsonPrintPolicy<TCHAR> FPrintPolicy;
class FInstallBundleManagerUtilJsonWriter : public TJsonStringWriter<FPrintPolicy>
{
public:
	explicit FInstallBundleManagerUtilJsonWriter(FString* Out) : TJsonStringWriter<FPrintPolicy>(Out, 0)
	{
	}
};

namespace InstallBundleManagerUtil
{
	TSharedPtr<IInstallBundleSource> MakeBundleSource(EInstallBundleSourceType Type)
	{
		switch (Type)
		{
		case EInstallBundleSourceType::Bulk:
			return MakeShared<FInstallBundleSourceBulk>();
			
#if WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
		case EInstallBundleSourceType::Platform:
			return MakeShared<FPlatformInstallBundleSource>();
#endif
		}

		LOG_INSTALL_BUNDLE_MAN(Fatal, TEXT("Can't make EInstallBundleSourceType %s"), LexToString(Type));
		return nullptr;
	}

	TSharedPtr<FQueuedThreadPool, ESPMode::ThreadSafe> GetJournalThreadPool()
	{
		static TWeakPtr<FQueuedThreadPool, ESPMode::ThreadSafe> WeakJournalThreadPool;

		TSharedPtr<FQueuedThreadPool, ESPMode::ThreadSafe> PinnedJournalThreadPool = WeakJournalThreadPool.Pin();
		if (FPlatformProcess::SupportsMultithreading() && PinnedJournalThreadPool == nullptr)
		{
			FQueuedThreadPool* JournalThreadPool = FQueuedThreadPool::Allocate();
			const int32 NumThreadsInThreadPool = 1;  // Need the journal tasks to be executed in-order so only use one thread.
			verify(JournalThreadPool->Create(NumThreadsInThreadPool, 96 * 1024, TPri_AboveNormal));

			PinnedJournalThreadPool = MakeShareable(JournalThreadPool,
				[](FQueuedThreadPool* JournalThreadPool)
				{
					JournalThreadPool->Destroy();
				});
			WeakJournalThreadPool = PinnedJournalThreadPool;
		}
		
		return PinnedJournalThreadPool;
	}

	bool LoadBundleSourceBundleInfoFromConfig(EInstallBundleSourceType SourceType, const FConfigFile& InstallBundleConfig, const FString& Section, FInstallBundleSourcePersistentBundleInfo& OutInfo)
	{
		if (!Section.StartsWith(InstallBundleUtil::GetInstallBundleSectionPrefix()))
			return false;

		const FString BundleName = Section.RightChop(InstallBundleUtil::GetInstallBundleSectionPrefix().Len());
		OutInfo.BundleName = FName(*BundleName);
		OutInfo.BundleNameString = BundleName;

		if (!InstallBundleConfig.GetBool(*Section, TEXT("IsStartup"), OutInfo.bIsStartup))
		{
			OutInfo.bIsStartup = false;
		}

		TArray<FString> BundlePrereqs;
		InstallBundleConfig.GetArray(*Section, TEXT("Prereqs"), BundlePrereqs);
		for (const FString& Prereq : BundlePrereqs)
		{
			if (Prereq == TEXT("RequiresLatestClient"))
			{
				OutInfo.bDoPatchCheck = true;
			}
			else
			{
				LOG_INSTALL_BUNDLE_MAN(Warning, TEXT("Unknown Bundle Prereq %s, ignoring"), *Prereq);
			}
		}

		FString PriorityString;
		if (InstallBundleConfig.GetString(*Section, TEXT("Priority"), PriorityString))
		{
			EInstallBundlePriority Priority = EInstallBundlePriority::Normal;
			if (LexTryParseString(Priority, *PriorityString))
			{
				OutInfo.Priority = Priority;
			}
			else
			{
				LOG_INSTALL_BUNDLE_MAN(Warning, TEXT("Unknown Bundle Priority %s, ignoring"), *PriorityString);
			}
		}

		TArray<FString> CachedBySources;
		if (InstallBundleConfig.GetArray(*Section, TEXT("CachedBySource"), CachedBySources))
		{
			if (CachedBySources.Contains(LexToString(SourceType)))
			{
				OutInfo.bIsCached = true;
			}
		}

		if (!InstallBundleConfig.GetBool(*Section, TEXT("UseChunkDBs"), OutInfo.bUseChunkDBs))
		{
			OutInfo.bUseChunkDBs = false;
		}

		return true;
	}

	TSet<FName> GetBundleDependenciesFromConfig(FName InBundleName, TSet<FName>* SkippedUnknownBundles /*= nullptr*/)
	{
		TSet<FName> BundlesToLoad;
		TArray<FString> BundleDependencies;
		BundleDependencies.Add(InBundleName.ToString());

		if (SkippedUnknownBundles)
		{
			SkippedUnknownBundles->Empty();
		}

		while (BundleDependencies.Num() > 0)
		{
			TArray<FString> BundleDependenciesNext;
			for (const FString& Dep : BundleDependencies)
			{
				const FString DepSection = InstallBundleUtil::GetInstallBundleSectionPrefix() + Dep;

				FName DepName(*Dep);
				if (GConfig->DoesSectionExist(*DepSection, GInstallBundleIni))
				{
					BundlesToLoad.Add(DepName);
					TArray<FString> DepsFromConfig;
					GConfig->GetArray(*DepSection, TEXT("Dependencies"), DepsFromConfig, GInstallBundleIni);
					BundleDependenciesNext.Append(DepsFromConfig);
				}
				else if (SkippedUnknownBundles)
				{
					SkippedUnknownBundles->Add(DepName);
				}
			}
			BundleDependencies = MoveTemp(BundleDependenciesNext);
		}		

		return BundlesToLoad;
	}

	void LogBundleRequestStats(const TCHAR* BundleName, const InstallBundleUtil::FContentRequestStats& RequestStats, ELogVerbosity::Type LogVerbosityOverride)
	{
		LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("------------------------------------------------------"));
		LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("Bundle Request Stats - %s"), BundleName);
		LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("Total Time %f"), RequestStats.GetElapsedTime());

		if (ensureAlways(!RequestStats.bOpen))
		{
			for (const TPair<FString, InstallBundleUtil::FContentRequestStateStats>& StatePair : RequestStats.StateStats)
			{
				if (StatePair.Value.bOpen)
				{
					LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("\tState %s did not finish, possibly canceled"), *StatePair.Key);
				}
				else
				{
					LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("\tState %s: Time - %f"), *StatePair.Key, StatePair.Value.GetElapsedTime());
					LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("\tState %s: Size - %" UINT64_FMT), *StatePair.Key, StatePair.Value.DataSize);
				}
			}
		}
		else
		{
			LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("Bundle Request %s did not finish"), BundleName);
		}

		LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("End Bundle Request Stats - %s"), BundleName);
		LOG_INSTALL_BUNDLE_MAN_OVERRIDE(LogVerbosityOverride, Display, TEXT("------------------------------------------------------"));
	}

	FPersistentStatContainer::FPersistentStatContainer()
		: SessionAnalyticsDataMap()
	{
	}

	FPersistentStatContainer::~FPersistentStatContainer()
	{
	}

	void FPersistentStatContainer::StartSessionPersistentStatTracking(const FString& SessionName, const TArray<FName>& RequiredBundles /* = TArray<FName>() */, const FString& ExpectedAnalyticsID /* = FString() */, bool bForceResetStatData /* = false */)
	{
		//First do our base behavior to populate all data
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::StartSessionPersistentStatTracking(SessionName, RequiredBundles, ExpectedAnalyticsID, bForceResetStatData);
		
		//Send our StartPatching analytic for this session
		TSharedPtr<IInstallBundleManager> InstallBundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		TSharedPtr<IAnalyticsProviderET> BundleAnalyticsPtr = (nullptr != InstallBundleManager) ? InstallBundleManager->GetAnalyticsProvider() : nullptr;
		if (BundleAnalyticsPtr.IsValid())
		{
			InstallBundleManagerAnalytics::FireEvent_PersistentPatchStats_StartPatching(BundleAnalyticsPtr.Get(), CalculatePersistentStatsInformationForSession(SessionName));
		}
		
		//Reset our tracking of BG analytics, as we might have previously stopped it and need to restart it
		FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(SessionName);
		FoundData.ResetShouldSendBGAnalytics();
	}

	void FPersistentStatContainer::StopSessionPersistentStatTracking(const FString& SessionName, bool bStopAllActiveTimers /* = true */)
	{
		//First do our base behavior to correctly handle all data
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::StopSessionPersistentStatTracking(SessionName, bStopAllActiveTimers);

		//Send our EndPatching analytic for this session
		TSharedPtr<IInstallBundleManager> InstallBundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		TSharedPtr<IAnalyticsProviderET> BundleAnalyticsPtr = (nullptr != InstallBundleManager) ? InstallBundleManager->GetAnalyticsProvider() : nullptr;
		if (BundleAnalyticsPtr.IsValid())
		{
			InstallBundleManagerAnalytics::FireEvent_PersistentPatchStats_EndPatching(BundleAnalyticsPtr.Get(), CalculatePersistentStatsInformationForSession(SessionName));
		}
		
		//Don't send BG Analytics now that we have stopped stat tracking for this session
		FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(SessionName);
		FoundData.bShouldSendBGAnalyticsSessionMap = false;
		
		//Clean up our data for this session and it's required bundles
		{
			//only have stuff to clean up if we actually have a session under this identifier
			InstallBundleUtil::PersistentStats::FSessionPersistentStats* FoundSessionStats = SessionPersistentStatMap.Find(SessionName);
			if (nullptr != FoundSessionStats)
			{
				//Before cleaning up, lets save out all dirty stats to make sure we have the most up-to-date stuff on disk.
				SaveAllDirtyStatsToDisk();
			
				//remove data for our bundles
				TArray<FString> RequiredBundlesForSession;
				FoundSessionStats->GetRequiredBundles(RequiredBundlesForSession);
				for (const FString& BundleName : RequiredBundlesForSession)
				{
					RemoveBundleStats(*BundleName);
				}
				
				//Now remove Session data now that we have removed the data for all bundles
				RemoveSessionStats(SessionName);
			}
		}
	}

	void FPersistentStatContainer::RemoveSessionStats(const FString& SessionName)
	{
		LOG_INSTALL_BUNDLE_MAN(Display, TEXT("Cleaning Up PersistentStatTracking -- Session %s"), *SessionName);
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::RemoveSessionStats(SessionName);
	}
	
	void FPersistentStatContainer::RemoveBundleStats(FName BundleName)
	{
		LOG_INSTALL_BUNDLE_MAN(Display, TEXT("Cleaning Up PersistentStatTracking -- Bundle %s"), *BundleName.ToString());
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::RemoveBundleStats(BundleName);
	}

	void FPersistentStatContainer::AddRequiredBundlesForSession(const FString& SessionName, const TArray<FName>& BundleNames)
	{
		InstallBundleUtil::PersistentStats::FSessionPersistentStats* FoundSessionStats = SessionPersistentStatMap.Find(SessionName);
		if (ensureAlwaysMsgf((nullptr != FoundSessionStats), TEXT("Call to AddRequiredBundlesForSession without having created a session for %s!"), *SessionName))
		{
			FoundSessionStats->AddRequiredBundles(BundleNames);
			
			//After adding required bundles, lets go ahead and also try and load the required bundle's data
			LoadRequiredBundleDataFromDiskForSession(SessionName);
		}
	}

	void FPersistentStatContainer::UpdateForContentState(const FInstallBundleCombinedContentState& ContentState, const FString& SessionName)
	{
		FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(SessionName);
		FoundData.bRequiresDownload = ContentState.DownloadSize > 0;
		FoundData.bRequiresInstall = ContentState.InstallSize > 0;
		
		//Reset so that it can react to the above values being changed
		FoundData.ResetShouldSendBGAnalytics();
	}

	void FPersistentStatContainer::UpdateForBundleSource(const FInstallBundleSourceUpdateContentResultInfo& BundleSourceResult, EInstallBundleSourceType SourceType, const FString& BundleName)
	{
		FBundleAnalyticsData& BundleAnalyticsData = BundleAnalyticsDataMap.FindOrAdd(BundleName);
		
		if (BundleSourceResult.DidBundleSourceDoWork())
		{
			BundleAnalyticsData.BundleSourcesThatDidWorkMap.Add(SourceType);
		}
	}

	void FPersistentStatContainer::OnApp_EnteringBackground()
	{
		//First call super class to update data
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::OnApp_EnteringBackground();
		
		SendEnteringBackgroundAnalytic();
	}

	void FPersistentStatContainer::OnApp_EnteringForeground()
	{
		//First call super class to update data
		InstallBundleUtil::PersistentStats::FPersistentStatContainerBase::OnApp_EnteringForeground();
		
		SendEnteringForegroundAnalytic();
	}

	void FPersistentStatContainer::SendEnteringBackgroundAnalytic()
	{
		TSharedPtr<IInstallBundleManager> InstallBundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		TSharedPtr<IAnalyticsProviderET> BundleAnalyticsPtr = (nullptr != InstallBundleManager) ? InstallBundleManager->GetAnalyticsProvider() : nullptr;
		if (BundleAnalyticsPtr.IsValid())
		{
			//Send Background analytics for all tracked sessions
			TArray<FString> SessionNames;
			SessionPersistentStatMap.GetKeys(SessionNames);
			for (const FString& Session : SessionNames)
			{
				FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(Session);
				if (FoundData.bShouldSendBGAnalyticsSessionMap)
				{
					InstallBundleManagerAnalytics::FireEvent_PersistentPatchStats_Background(BundleAnalyticsPtr.Get(), CalculatePersistentStatsInformationForSession(Session));
				}
			}
		}
	}

	void FPersistentStatContainer::SendEnteringForegroundAnalytic()
	{
		TSharedPtr<IInstallBundleManager> InstallBundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		TSharedPtr<IAnalyticsProviderET> BundleAnalyticsPtr = (nullptr != InstallBundleManager) ? InstallBundleManager->GetAnalyticsProvider() : nullptr;
		if (BundleAnalyticsPtr.IsValid())
		{
			//Send Foreground analytics for all tracked sessions
			TArray<FString> SessionNames;
			SessionPersistentStatMap.GetKeys(SessionNames);
			for (const FString& Session : SessionNames)
			{
				FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(Session);
				if (FoundData.bShouldSendBGAnalyticsSessionMap)
				{
					InstallBundleManagerAnalytics::FireEvent_PersistentPatchStats_Foreground(BundleAnalyticsPtr.Get(), CalculatePersistentStatsInformationForSession(Session));
				}
			}
		}
	}

	FPersistentStatContainer::FPersistentStatsInformation FPersistentStatContainer::CalculatePersistentStatsInformationForSession(const FString& SessionName)
	{
		FPersistentStatsInformation NewStatsInfo;
		NewStatsInfo.SessionName = SessionName;
		
		TSet<EInstallBundleSourceType> SourcesThatDidWork;
		
		InstallBundleUtil::PersistentStats::FSessionPersistentStats* SessionStats = SessionPersistentStatMap.Find(SessionName);
		if (nullptr != SessionStats)
		{
			//RealTotalTime = Session's TotalTime_Real
			{
				const InstallBundleUtil::PersistentStats::FPersistentTimerData* RealTotalTimeData = SessionStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::TotalTime_Real);
				if (nullptr != RealTotalTimeData)
				{
					NewStatsInfo.RealTotalTime = RealTotalTimeData->CurrentValue;
				}
			}
			
			//ActiveTotalTime = Session's TotalTime_FG
			{
				const InstallBundleUtil::PersistentStats::FPersistentTimerData* ActiveTotalTimeData = SessionStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::TotalTime_FG);
				if (nullptr != ActiveTotalTimeData)
				{
					NewStatsInfo.ActiveTotalTime = ActiveTotalTimeData->CurrentValue;
				}
			}
			
			//EstimatedTotalBGTime = Session's TotalTime_BG
			{
				const InstallBundleUtil::PersistentStats::FPersistentTimerData* EstimatedBGData = SessionStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::TotalTime_BG);
				if (nullptr != EstimatedBGData)
				{
					NewStatsInfo.EstimatedTotalBGTime = EstimatedBGData->CurrentValue;
				}
			}
			
			//NumResumedFromBackground = Session's NumResumedFromBackground
			{
				const int* NumResumedFromBackground = SessionStats->GetCountStatData(InstallBundleUtil::PersistentStats::ECountStatNames::NumResumedFromBackground);
				if (nullptr != NumResumedFromBackground)
				{
					NewStatsInfo.NumResumedFromBackground = *NumResumedFromBackground;
				}
			}
			
			//NumResumedFromLaunch = Session's NumResumedFromLaunch
			{
				const int* NumResumedFromLaunch = SessionStats->GetCountStatData(InstallBundleUtil::PersistentStats::ECountStatNames::NumResumedFromLaunch);
				if (nullptr != NumResumedFromLaunch)
				{
					NewStatsInfo.NumResumedFromLaunch = *NumResumedFromLaunch;
				}
			}
			
			//NumBackgrounded = Session's NumBackgrounded
			{
				const int* NumBackgrounded = SessionStats->GetCountStatData(InstallBundleUtil::PersistentStats::ECountStatNames::NumBackgrounded);
				if (nullptr != NumBackgrounded)
				{
					NewStatsInfo.NumBackgrounded = *NumBackgrounded;
				}
			}
			
			//Go through all the required bundles and combine their stats for our total style stats
			TArray<FString> BundleNames;
			SessionStats->GetRequiredBundles(BundleNames);
			
			TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> > BundleStatsJsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&NewStatsInfo.BundleStats);
			
			//Write start of array as we are going to go through all bundles and add something to the BundleStats JSON array
			BundleStatsJsonWriter->WriteArrayStart();
			{
				for (const FString& BundleName : BundleNames)
				{
					if (NewStatsInfo.RequiredBundleNames.IsEmpty())
					{
						NewStatsInfo.RequiredBundleNames = BundleName;
					}
					else
					{
						NewStatsInfo.RequiredBundleNames.Append(TEXT(", ") + BundleName);
					}
					
					FName BundleNameAsFName = FName(*BundleName);
					InstallBundleUtil::PersistentStats::FBundlePersistentStats* BundleStats = PerBundlePersistentStatMap.Find(BundleNameAsFName);
					if (nullptr != BundleStats)
					{
						//JSON output added to BundleStats for the Session for each bundle
						BundleStatsJsonWriter->WriteObjectStart();
						{
							BundleStats->ToJson(BundleStatsJsonWriter, true);
						}
						BundleStatsJsonWriter->WriteObjectEnd();
						
						//RealChunkDBDownloadTime = Highest ChunkDBDownloadTime_Real in all required bundles as these are all started at the start of the BG downloads and thus the highest one was started during all others
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData*  RealChunkDBDownloadTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::ChunkDBDownloadTime_Real);
							if (nullptr != RealChunkDBDownloadTimeStat)
							{
								if (RealChunkDBDownloadTimeStat->CurrentValue > NewStatsInfo.RealChunkDBDownloadTime)
								{
									NewStatsInfo.RealChunkDBDownloadTime = RealChunkDBDownloadTimeStat->CurrentValue;
								}
							}
						}
						
						//ActivePSOTime = Highest PSOTIme_FG in all required bundles as only 1 bundle should have any shaders to optimize, and so we can just use the highest one
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData*  ActivePSOTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::PSOTime_FG);
							if (nullptr != ActivePSOTimeStat)
							{
								if (ActivePSOTimeStat->CurrentValue > NewStatsInfo.ActivePSOTime)
								{
									NewStatsInfo.ActivePSOTime = ActivePSOTimeStat->CurrentValue;
								}
							}
						}
						
						//ActiveChunkDBDownloadTime = Bundle ChunkDBDownloadTime_FG Sum
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData*  ActiveChunkDBDownloadTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::ChunkDBDownloadTime_FG);
							if (nullptr != ActiveChunkDBDownloadTimeStat)
							{
								NewStatsInfo.ActiveChunkDBDownloadTime += ActiveChunkDBDownloadTimeStat->CurrentValue;
							}
						}
						
						//EstimatedBackgroundChunkDBDownloadTime = Bundle ChunkDBDownloadTime_BG Sum
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData* EstimatedBackgroundChunkDBDownloadTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::ChunkDBDownloadTime_BG);
							if (nullptr != EstimatedBackgroundChunkDBDownloadTimeStat)
							{
								NewStatsInfo.EstimatedBackgroundChunkDBDownloadTime += EstimatedBackgroundChunkDBDownloadTimeStat->CurrentValue;
							}
						}
						
						//ActiveInstallTime = Bundle InstallTime_FG Sum
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData* ActiveInstallTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::InstallTime_FG);
							if (nullptr != ActiveInstallTimeStat)
							{
								NewStatsInfo.ActiveInstallTime += ActiveInstallTimeStat->CurrentValue;
							}
						}
						
						//EstimatedBGInstallTime = Bundle InstallTime_BG Sum
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData* EstimatedBGInstallTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::InstallTime_BG);
							if (nullptr != EstimatedBGInstallTimeStat)
							{
								NewStatsInfo.EstimatedBGInstallTime += EstimatedBGInstallTimeStat->CurrentValue;
							}
						}
						
						//EstimatedBGPSOTime = Bundle PSOTime_BG Sum
						{
							const InstallBundleUtil::PersistentStats::FPersistentTimerData* EstimatedBGPSOTimeStat = BundleStats->GetTimingStatData(InstallBundleUtil::PersistentStats::ETimingStatNames::PSOTime_BG);
							if (nullptr != EstimatedBGPSOTimeStat)
							{
								NewStatsInfo.EstimatedBGPSOTime += EstimatedBGPSOTimeStat->CurrentValue;
							}
						}
						
						//Pull info from this bundle's BundleAnalyticsDataMap entries
						{
							FBundleAnalyticsData& BundleData = BundleAnalyticsDataMap.FindOrAdd(BundleName);
							
							//Fill out SourcesThatDidWork array with this bundle's sources that did work
							SourcesThatDidWork.Append(BundleData.BundleSourcesThatDidWorkMap);
						}
					}
				}
			}
			
			//We are finished going through our bundles, so close out the JSON array
			BundleStatsJsonWriter->WriteArrayEnd();
			BundleStatsJsonWriter->Close();
		}
		
		//Create BundleSourcesThatDidWork string from SourcesThatDidWork map
		for (EInstallBundleSourceType BundleSourceType : SourcesThatDidWork)
		{
			if (NewStatsInfo.BundleSourcesThatDidWork.IsEmpty())
			{
				NewStatsInfo.BundleSourcesThatDidWork = LexToString(BundleSourceType);
			}
			else
			{
				NewStatsInfo.BundleSourcesThatDidWork.Append(TEXT(", "));
				NewStatsInfo.BundleSourcesThatDidWork.Append(LexToString(BundleSourceType));
			}
		}
		
		//bRequiresDownload and bRequiresInstall copied from our session analytics data
		FSessionAnalyticsData& FoundData = SessionAnalyticsDataMap.FindOrAdd(SessionName);
		NewStatsInfo.bRequiresDownload = FoundData.bRequiresDownload;
		NewStatsInfo.bRequiresInstall = FoundData.bRequiresInstall;
		
		return MoveTemp(NewStatsInfo);
	}
	
	void FPersistentStatContainer::FPersistentStatsInformation::FillOutAnalyticsArrayWithData(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
	{
		const bool bRequiresUpdate = (bRequiresDownload || bRequiresInstall);

		AppendAnalyticsEventAttributeArray(OutAnalyticsArray,
			TEXT("SessionName"), SessionName,
			TEXT("RequiredBundleNames"), RequiredBundleNames,
			TEXT("BundleStats"), BundleStats,

			TEXT("NumBackgrounded"), NumBackgrounded,
			TEXT("NumResumedFromBackground"), NumResumedFromBackground,
			TEXT("NumResumedFromLaunch"), NumResumedFromLaunch,

			TEXT("RealTotalTime"), RealTotalTime,
			TEXT("ActiveTotalTime"), ActiveTotalTime,
			TEXT("EstimatedTotalBGTime"), EstimatedTotalBGTime,
			TEXT("RealChunkDBDownloadTime"), RealChunkDBDownloadTime,
			TEXT("ActiveChunkDBDownloadTime"), ActiveChunkDBDownloadTime,
			TEXT("EstimatedBackgroundChunkDBDownloadTime"), EstimatedBackgroundChunkDBDownloadTime,
			TEXT("ActiveInstallTime"), ActiveInstallTime,
			TEXT("EstimatedBGInstallTime"), EstimatedBGInstallTime,
			TEXT("ActivePSOTime"), ActivePSOTime,
			TEXT("EstimatedBGPSOTime"), EstimatedBGPSOTime,

			TEXT("bRequiresDownload"), bRequiresDownload,
			TEXT("bRequiresInstall"), bRequiresInstall,
			TEXT("bRequiresUpdate"), bRequiresUpdate,

			TEXT("BundleSourcesThatDidWork"), BundleSourcesThatDidWork);
	}
}

namespace InstallBundleManagerAnalytics
{
	//////////////////////////////////////////////////////////////////////////
	/// TODO Move these into the IBuildInstaller.h
	//////////////////////////////////////////////////////////////////////////
	FString GetBuildPatchInstallErrorString(EBuildPatchInstallError Value)
	{
		// Using an assignment to local allows for RVO
#define CASE_ENUM_SET(State) case State: ReturnValue = TEXT(#State); break;
		FString ReturnValue;

		switch (Value)
		{
			CASE_ENUM_SET(EBuildPatchInstallError::NoError)
			CASE_ENUM_SET(EBuildPatchInstallError::DownloadError)
			CASE_ENUM_SET(EBuildPatchInstallError::FileConstructionFail)
			CASE_ENUM_SET(EBuildPatchInstallError::MoveFileToInstall)
			CASE_ENUM_SET(EBuildPatchInstallError::BuildVerifyFail)
			CASE_ENUM_SET(EBuildPatchInstallError::ApplicationClosing)
			CASE_ENUM_SET(EBuildPatchInstallError::ApplicationError)
			CASE_ENUM_SET(EBuildPatchInstallError::UserCanceled)
			CASE_ENUM_SET(EBuildPatchInstallError::PrerequisiteError)
			CASE_ENUM_SET(EBuildPatchInstallError::InitializationError)
			CASE_ENUM_SET(EBuildPatchInstallError::PathLengthExceeded)
			CASE_ENUM_SET(EBuildPatchInstallError::OutOfDiskSpace)
		}
		return ReturnValue;
#undef CASE_ENUM_SET
	}

	FString GetInstallRetryTypesString(const TArray<EBuildPatchInstallError>& Value)
	{
		FString ReturnValue;
		bool bFirst = true;
		for (EBuildPatchInstallError InstallRetryType : Value)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				ReturnValue += TEXT(",");
			}
			ReturnValue += GetBuildPatchInstallErrorString(InstallRetryType);
		}
		return ReturnValue;
	}


	void FireEvent_InitBundleManagerComplete(IAnalyticsProviderET* AnalyticsProvider, 
		const bool bCanRetry, 
		const FString InitResultString)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InitComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("CanRetry"), bCanRetry,
			TEXT("InitResultString"), InitResultString));
	}

	void FireEvent_BundleManagerCacheStats(IAnalyticsProviderET* AnalyticsProvider, const FInstallBundleCacheStats& Stats)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.CacheStats"), MakeAnalyticsEventAttributeArray(
			TEXT("CacheName"), Stats.CacheName,
			TEXT("MaxSize"), Stats.MaxSize,
			TEXT("UsedSize"), Stats.UsedSize,
			TEXT("ReservedSize"), Stats.ReservedSize,
			TEXT("FreeSize"), Stats.FreeSize));
	}

	void FireEvent_InitBundleSourceBPSComplete(IAnalyticsProviderET* AnalyticsProvider,
		const FString& InstallManifestFilename, 
		const FString& InstallManifestVersion,
		const bool bRetrievedManifestFromBuildInfoService, 
		const bool bDownloadedManifest, 
		const bool bDownloadedBackgroundDownloadIni, 
		const FString InitResultString)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InitBundleSourceBPSComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("CurrentVersion"), InstallManifestVersion,
			TEXT("CurrentManifest"), InstallManifestFilename,
			TEXT("RetrievedManifestFromBuildInfoService"), bRetrievedManifestFromBuildInfoService,
			TEXT("DownloadedManifest"), bDownloadedManifest,
			TEXT("DownloadedBackgroundDownloadIni"), bDownloadedBackgroundDownloadIni,
			TEXT("InitResultString"), InitResultString));
	}

	void FireEvent_InitBundleSourceBulkComplete(IAnalyticsProviderET* AnalyticsProvider, const FString InitResultString)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InitBundleSourceBulkComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("InitResultString"), InitResultString));
	}

	void FireEvent_InitBundleSourcePlayGoComplete(IAnalyticsProviderET* AnalyticsProvider, const FString InitResultString)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InitBundleSourcePlayGoComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("InitResultString"), InitResultString));
	}

	void FireEvent_InitBundleSourceIntelligentDeliveryComplete(IAnalyticsProviderET* AnalyticsProvider, const FString InitResultString)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InitBundleSourceIntelligentDeliveryComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("InitResultString"), InitResultString));
	}

	void FireEvent_BeginInstall(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, bool bIsPatching, const FString& OldManifestVersion, const FString& InstallManifestVersion)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BeginInstall"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("OldManifestVersion"), OldManifestVersion,
			TEXT("InstallManifestVersion"), InstallManifestVersion,
			TEXT("IsOnCellular"), (FPlatformMisc::GetNetworkConnectionType() == ENetworkConnectionType::Cell),
			TEXT("IsPatching"), bIsPatching));
	}

	void FireEvent_BeginInstall_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, bool bIsPatching, const FString& OldManifestVersion, const FString& InstallManifestVersion)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BeginInstall"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("OldManifestVersion"), OldManifestVersion,
			TEXT("InstallManifestVersion"), InstallManifestVersion,
			TEXT("IsOnCellular"), (FPlatformMisc::GetNetworkConnectionType() == ENetworkConnectionType::Cell),
			TEXT("IsPatching"), bIsPatching));
	}

	void FireEvent_EndInstall(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, IBuildInstallerPtr ContentBuildInstaller, int32 BundleInstallRetryCount)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		if (ContentBuildInstaller.IsValid())
		{
			BuildPatchServices::FBuildInstallerConfiguration BuildConfig = ContentBuildInstaller->GetConfiguration();
			FBuildInstallStats BuildStats = ContentBuildInstaller->GetBuildStatistics();
			FString AppName = TEXT("Unknown");
			FString CurrentManifestVersion = TEXT("NONE");
			FString InstallManifestVersion = TEXT("NONE");
			if (BuildConfig.InstallerActions.Num() > 0)
			{
				// Currently only one action is ever used for FN installers.
				const BuildPatchServices::FInstallerAction& InstallerAction = BuildConfig.InstallerActions[0];
				AppName = InstallerAction.GetInstallOrCurrentManifest()->GetAppName();
				IBuildManifestPtr CurrentManifest = InstallerAction.TryGetCurrentManifest();
				IBuildManifestPtr InstallManifest = InstallerAction.TryGetInstallManifest();
				if (CurrentManifest.IsValid())
				{
					CurrentManifestVersion = CurrentManifest->GetVersionString();
				}
				if (InstallManifest.IsValid())
				{
					InstallManifestVersion = InstallManifest->GetVersionString();
				}
			}

			// NB: Only add newer stats to the end of the list to avoid data confusion server-side
			AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.EndInstall"), MakeAnalyticsEventAttributeArray(
				TEXT("BundleName"), BundleName,
				TEXT("AppName"), AppName,
				TEXT("AppInstalledVersion"), CurrentManifestVersion,
				TEXT("AppPatchVersion"), InstallManifestVersion,
				TEXT("TotalDownloadedData"), BuildStats.TotalDownloadedData,
				TEXT("AverageDownloadSpeed"), BuildStats.AverageDownloadSpeed,
				TEXT("InitializeTime"), BuildStats.InitializeTime,
				TEXT("ConstructTime"), BuildStats.ConstructTime,
				TEXT("MoveFromStageTime"), BuildStats.MoveFromStageTime,
				TEXT("FileAttributesTime"), BuildStats.FileAttributesTime,
				TEXT("VerifyTime"), BuildStats.VerifyTime,
				TEXT("CleanUpTime"), BuildStats.CleanUpTime,
				TEXT("PrereqTime"), BuildStats.PrereqTime,
				TEXT("ProcessPausedTime"), BuildStats.ProcessPausedTime,
				TEXT("ProcessActiveTime"), BuildStats.ProcessActiveTime,
				TEXT("ProcessExecuteTime"), BuildStats.ProcessExecuteTime,
				TEXT("ProcessSuccess"), BuildStats.ProcessSuccess,
				TEXT("FailureType"), GetBuildPatchInstallErrorString(BuildStats.FailureType),
				TEXT("NumInstallRetries"), BuildStats.NumInstallRetries,
				TEXT("InstallRetryTypes"), GetInstallRetryTypesString(BuildStats.RetryFailureTypes),
				TEXT("InstallRetryCodes"), FString::Join(BuildStats.RetryErrorCodes, TEXT(",")),
				TEXT("ErrorCode"), BuildStats.ErrorCode,
				TEXT("FinalProgress"), BuildStats.FinalProgress,
				TEXT("FinalDownloadSpeed"), BuildStats.FinalDownloadSpeed,
				TEXT("BundleInstallRetryCount"), BundleInstallRetryCount));
		}
	}

	void FireEvent_EndInstall_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, IBuildInstallerPtr ContentBuildInstaller, int32 BundleInstallRetryCount)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		if (ContentBuildInstaller.IsValid())
		{
			BuildPatchServices::FBuildInstallerConfiguration BuildConfig = ContentBuildInstaller->GetConfiguration();
			FBuildInstallStats BuildStats = ContentBuildInstaller->GetBuildStatistics();
			FString AppName = TEXT("Unknown");
			FString CurrentManifestVersion = TEXT("NONE");
			FString InstallManifestVersion = TEXT("NONE");
			if (BuildConfig.InstallerActions.Num() > 0)
			{
				// Currently only one action is ever used for FN installers.
				const BuildPatchServices::FInstallerAction& InstallerAction = BuildConfig.InstallerActions[0];
				AppName = InstallerAction.GetInstallOrCurrentManifest()->GetAppName();
				IBuildManifestPtr CurrentManifest = InstallerAction.TryGetCurrentManifest();
				IBuildManifestPtr InstallManifest = InstallerAction.TryGetInstallManifest();
				if (CurrentManifest.IsValid())
				{
					CurrentManifestVersion = CurrentManifest->GetVersionString();
				}
				if (InstallManifest.IsValid())
				{
					InstallManifestVersion = InstallManifest->GetVersionString();
				}
			}

			// NB: Only add newer stats to the end of the list to avoid data confusion server-side
			AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.EndInstall"), MakeAnalyticsEventAttributeArray(
				TEXT("BundleName"), BundleName,
				TEXT("AppName"), AppName,
				TEXT("AppInstalledVersion"), CurrentManifestVersion,
				TEXT("AppPatchVersion"), InstallManifestVersion,
				TEXT("TotalDownloadedData"), BuildStats.TotalDownloadedData,
				TEXT("AverageDownloadSpeed"), BuildStats.AverageDownloadSpeed,
				TEXT("InitializeTime"), BuildStats.InitializeTime,
				TEXT("ConstructTime"), BuildStats.ConstructTime,
				TEXT("MoveFromStageTime"), BuildStats.MoveFromStageTime,
				TEXT("FileAttributesTime"), BuildStats.FileAttributesTime,
				TEXT("VerifyTime"), BuildStats.VerifyTime,
				TEXT("CleanUpTime"), BuildStats.CleanUpTime,
				TEXT("PrereqTime"), BuildStats.PrereqTime,
				TEXT("ProcessPausedTime"), BuildStats.ProcessPausedTime,
				TEXT("ProcessActiveTime"), BuildStats.ProcessActiveTime,
				TEXT("ProcessExecuteTime"), BuildStats.ProcessExecuteTime,
				TEXT("ProcessSuccess"), BuildStats.ProcessSuccess,
				TEXT("FailureType"), GetBuildPatchInstallErrorString(BuildStats.FailureType),
				TEXT("NumInstallRetries"), BuildStats.NumInstallRetries,
				TEXT("InstallRetryTypes"), GetInstallRetryTypesString(BuildStats.RetryFailureTypes),
				TEXT("InstallRetryCodes"), FString::Join(BuildStats.RetryErrorCodes, TEXT(",")),
				TEXT("ErrorCode"), BuildStats.ErrorCode,
				TEXT("FinalProgress"), BuildStats.FinalProgress,
				TEXT("FinalDownloadSpeed"), BuildStats.FinalDownloadSpeed,
				TEXT("BundleInstallRetryCount"), BundleInstallRetryCount));
		}
	}

	void FireEvent_BackgroundDownloadStats(IAnalyticsProviderET* AnalyticsProvider, 
		const FString& BundleName, 
		const FString& PreviousManifest, const FString& CurrentManifest, 
		const FString& OldManifestVersion, const FString& InstallManifestVersion,
		int32 NumFailedDownloads, int32 NumSuccessfulDownloads)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BackgroundDownloadStats"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("PreviousManfest"), PreviousManifest,
			TEXT("CurrentManifest"), CurrentManifest,
			TEXT("PreviousVersion"), OldManifestVersion,
			TEXT("CurrentVersion"), InstallManifestVersion,
			TEXT("NumFailedDownloads"), NumFailedDownloads,
			TEXT("NumSucceessfulDownloads"), NumSuccessfulDownloads));
	}

	void FireEvent_BackgroundDownloadStats_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider,
		const FString& BundleName,
		const FString& PreviousManifest, const FString& CurrentManifest,
		const FString& OldManifestVersion, const FString& InstallManifestVersion,
		int32 NumFailedDownloads, int32 NumSuccessfulDownloads)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BackgroundDownloadStats"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("PreviousManfest"), PreviousManifest,
			TEXT("CurrentManifest"), CurrentManifest,
			TEXT("PreviousVersion"), OldManifestVersion,
			TEXT("CurrentVersion"), InstallManifestVersion,
			TEXT("NumFailedDownloads"), NumFailedDownloads,
			TEXT("NumSucceessfulDownloads"), NumSuccessfulDownloads));
	}

	void FireEvent_BundleLatestClientCheckComplete(IAnalyticsProviderET* AnalyticsProvider, 
		const FString& BundleName, 
		bool bSkippedCheck, 
		FString SkipReason, 
		bool bShouldPatch, 
		bool bRequestFailed)
	{
	// WRH 2021-04-08 - As part of FORT-363174, disabling this unused, higher frequency telmetry event.
	// Disabling in the implementation as opposed to the callsites because there is more than one callsite,
	// so it's safer to disable it here just in case another callsite shows up down the road.
	// Not deleting the code as usual because we want to maybe enable it later. Since we can't hotfix this code,
	// we are forced to compile it out, but want a quick pathway to restore it if it comes back up that it is needed.
#if 0
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		const FString PlatformName = ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName());
		const FString BuildVersionWithPlatform = FString::Printf(TEXT("%s-%s"), FApp::GetBuildVersion(), *PlatformName);
		
		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleLatestClientCheckComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("AppVersion"), BuildVersionWithPlatform,
			TEXT("SkippedCheck"), bSkippedCheck,
			TEXT("SkipReason"), SkipReason,
			TEXT("ShouldPatch"), bShouldPatch,
			TEXT("RequestFailed"), bRequestFailed));
#endif
	}

	void FireEvent_BundleRequestStarted(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleRequestStarted"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName));
	}

	void FireEvent_BundleRequestStarted_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, 
		const FString& BundleName, 
		const FString& PreviousManifest, const FString& CurrentManifest, 
		const FString& InstallManifestVersion)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BundleRequestStarted"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("PreviousManfest"), PreviousManifest,
			TEXT("CurrentManifest"), CurrentManifest,
			TEXT("CurrentVersion"), InstallManifestVersion));
	}

	void FireEvent_InstallHeartbeat(IAnalyticsProviderET* AnalyticsProvider, const TArray<FBundleHeartbeatStats>& BundleStats)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		bool bIsAnyBundleInError = false;

		FString AllBundlesJSonString;
		FInstallBundleManagerUtilJsonWriter JsonWriter(&AllBundlesJSonString);
		
		//Build version of bundle json payload and look if any bundles are in error while going through
		JsonWriter.WriteArrayStart();
		{
			for (const FBundleHeartbeatStats& BundleStat : BundleStats)
			{
				JsonWriter.WriteObjectStart();
				{
					JsonWriter.WriteValue(TEXT("BundleName"), BundleStat.BundleName.ToString());
					JsonWriter.WriteValue(TEXT("LastStatusText"), BundleStat.LastStatusText);

					JsonWriter.WriteValue(TEXT("Finishing_Percent"), BundleStat.Finishing_Percent);
					JsonWriter.WriteValue(TEXT("Install_Percent"), BundleStat.Install_Percent);

					JsonWriter.WriteValue(TEXT("LastErrorResult"), LexToString(BundleStat.LastErrorResult));

					JsonWriter.WriteValue(TEXT("bIsPausedFromCellular"), EnumHasAnyFlags(BundleStat.PauseFlags, EInstallBundlePauseFlags::OnCellularNetwork));
					JsonWriter.WriteValue(TEXT("bIsPausedFromNoInternet"), EnumHasAnyFlags(BundleStat.PauseFlags, EInstallBundlePauseFlags::NoInternetConnection));
					JsonWriter.WriteValue(TEXT("bIsPausedFromUser"), EnumHasAnyFlags(BundleStat.PauseFlags, EInstallBundlePauseFlags::UserPaused));

					JsonWriter.WriteValue(TEXT("bIsComplete"), BundleStat.bIsComplete);
				}
				JsonWriter.WriteObjectEnd();


				bIsAnyBundleInError = bIsAnyBundleInError && (BundleStat.LastErrorResult == EInstallBundleResult::OK);
			}
		}
		JsonWriter.WriteArrayEnd();
		JsonWriter.Close();
		
		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InstallHeartbeat"), MakeAnalyticsEventAttributeArray(
			TEXT("IsAnyBundleInError"), bIsAnyBundleInError,
			TEXT("BundleStatus"), AllBundlesJSonString));
	}

	void FireEvent_BundleRequestComplete(IAnalyticsProviderET* AnalyticsProvider, 
		const FString& BundleName, 
		bool bDidInstall, 
		const FString& Result, 
		const InstallBundleUtil::FContentRequestStats& TimingStats)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("DidInstall"), bDidInstall,
			TEXT("Result"), Result,
			TEXT("Total_Time"), TimingStats.GetElapsedTime());

		Attributes.Reserve(Attributes.Num() + TimingStats.StateStats.Num());

		for (const TPair<FString, InstallBundleUtil::FContentRequestStateStats>& Pair : TimingStats.StateStats)
		{
			Attributes.Add(FAnalyticsEventAttribute(Pair.Key + TEXT("_Time"), Pair.Value.GetElapsedTime()));
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleRequestComplete"), MoveTemp(Attributes));
	}

	void FireEvent_BundleRequestComplete_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider,
		const FString& BundleName,
		const FString& PreviousManifest, const FString& CurrentManifest,
		const FString& OldManifestVersion, const FString& InstallManifestVersion,
		const FString& OldVersionTimeStamp,
		uint64 TotalDownloadedBytes,
		uint64 EstimatedFullDownloadBytes,
		bool bDidInstall,
		const FString& Result,
		const FString& BPTErrorCode)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BundleRequestComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("PreviousManfest"), PreviousManifest,
			TEXT("CurrentManifest"), CurrentManifest,
			TEXT("PreviousVersion"), OldManifestVersion,
			TEXT("CurrentVersion"), InstallManifestVersion,
			TEXT("PreviousVersionTimeStamp"), OldVersionTimeStamp,
			TEXT("TotalDownloadedBytes"), TotalDownloadedBytes,
			TEXT("EstimatedFullDownloadBytes"), EstimatedFullDownloadBytes,
			TEXT("DidInstall"), bDidInstall,
			TEXT("Result"), Result,
			TEXT("BPTErrorCode"), BPTErrorCode));
	}

	void FireEvent_BundleReleaseRequestStarted(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, bool bRemoveFilesIfPossible)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleReleaseRequestStarted"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("RemoveFilesIfPossible"), bRemoveFilesIfPossible));
	}

	void FireEvent_BundleRemoveRequestStarted_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BundleRemoveRequestStarted"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName));
	}

	void FireEvent_BundleReleaseRequestComplete(IAnalyticsProviderET* AnalyticsProvider, 
		const FString& BundleName, 
		bool bRemoveFilesIfPossible, 
		const FString& Result)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleReleaseRequestComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("RemoveFilesIfPossible"), bRemoveFilesIfPossible,
			TEXT("Result"), Result));
	}

	void FireEvent_BundleRemoveRequestComplete_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, const FString& Result)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BundleRemoveRequestComplete"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("Result"), Result));
	}

	void FireEvent_BundleEvictedFromCache(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, const FString& BundleSource, FDateTime LastAccessTime, const FString& Result)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		FTimespan BundleAge = FDateTime::UtcNow() - LastAccessTime;

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleEvictedFromCache"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("BundleSource"), BundleSource,
			TEXT("LastAccessTime"), LastAccessTime.ToString(),
			TEXT("BundleAgeHours"), BundleAge.GetTotalHours(),
			TEXT("Result"), Result));
	}

	void FireEvent_BundleCacheHit(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, const FString& BundleSource)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleCacheHit"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("BundleSource"), BundleSource));
	}

	void FireEvent_BundleCacheMiss(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, const FString& BundleSource, bool bPatchRequired)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleCacheMiss"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("BundleSource"), BundleSource,
			TEXT("PatchRequired"), bPatchRequired));
	}

	void FireEvent_EarlyStartupPatcherStall(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, IBuildInstallerPtr ContentBuildInstaller)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		const float ProgressPercentReached = ContentBuildInstaller.IsValid() ? ContentBuildInstaller->GetUpdateProgress() : 0.f;

		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("ProgressPercent"), ProgressPercentReached);

		if (ContentBuildInstaller.IsValid())
		{
			BuildPatchServices::FBuildInstallerConfiguration BuildConfig = ContentBuildInstaller->GetConfiguration();
			FBuildInstallStats BuildStats = ContentBuildInstaller->GetBuildStatistics();
			FString AppName = TEXT("Unknown");
			FString CurrentManifestVersion = TEXT("NONE");
			FString InstallManifestVersion = TEXT("NONE");
			if (BuildConfig.InstallerActions.Num() > 0)
			{
				// Currently only one action is ever used for FN installers.
				const BuildPatchServices::FInstallerAction& InstallerAction = BuildConfig.InstallerActions[0];
				AppName = InstallerAction.GetInstallOrCurrentManifest()->GetAppName();
				IBuildManifestPtr CurrentManifest = InstallerAction.TryGetCurrentManifest();
				IBuildManifestPtr InstallManifest = InstallerAction.TryGetInstallManifest();
				if (CurrentManifest.IsValid())
				{
					CurrentManifestVersion = CurrentManifest->GetVersionString();
				}
				if (InstallManifest.IsValid())
				{
					InstallManifestVersion = InstallManifest->GetVersionString();
				}
			}

			AppendAnalyticsEventAttributeArray(Attributes,
				TEXT("AppName"), AppName,
				TEXT("AppInstalledVersion"), CurrentManifestVersion,
				TEXT("AppPatchVersion"), InstallManifestVersion,
				TEXT("NumInstallRetries"), BuildStats.NumInstallRetries,
				TEXT("InstallRetryTypes"), GetInstallRetryTypesString(BuildStats.RetryFailureTypes),
				TEXT("InstallRetryCodes"), FString::Join(BuildStats.RetryErrorCodes, TEXT(",")));
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.InstallerStall"), MoveTemp(Attributes));
	}
	
	void FireEvent_BackgroundDownloadStall_BundleSourceBPS(IAnalyticsProviderET* AnalyticsProvider, const FString& BundleName, float ProgressPercentReached)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}

		AnalyticsProvider->RecordEvent(TEXT("InstallBundleManager.BundleSourceBPS.BackgroundDownloadStall"), MakeAnalyticsEventAttributeArray(
			TEXT("BundleName"), BundleName,
			TEXT("ProgressPercent"), ProgressPercentReached));
	}

	void FireEvent_PersistentPatchStats_StartPatching(IAnalyticsProviderET* AnalyticsProvider, const InstallBundleManagerUtil::FPersistentStatContainer::FPersistentStatsInformation& PersistentStatInformation)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}
		
		TArray<FAnalyticsEventAttribute> Attributes;
		PersistentStatInformation.FillOutAnalyticsArrayWithData(Attributes);
		
		AnalyticsProvider->RecordEvent(TEXT("PersistentPatchStats.StartPatching"), MoveTemp(Attributes));
	}

	void FireEvent_PersistentPatchStats_EndPatching(IAnalyticsProviderET* AnalyticsProvider, const InstallBundleManagerUtil::FPersistentStatContainer::FPersistentStatsInformation& PersistentStatInformation)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}
		
		TArray<FAnalyticsEventAttribute> Attributes;
		PersistentStatInformation.FillOutAnalyticsArrayWithData(Attributes);
		
		AnalyticsProvider->RecordEvent(TEXT("PersistentPatchStats.EndPatching"), MoveTemp(Attributes));
	}

	void FireEvent_PersistentPatchStats_Background(IAnalyticsProviderET* AnalyticsProvider, const InstallBundleManagerUtil::FPersistentStatContainer::FPersistentStatsInformation& PersistentStatInformation)
	{
		if (AnalyticsProvider == nullptr)
		{
			return;
		}
		
		TArray<FAnalyticsEventAttribute> Attributes;
		PersistentStatInformation.FillOutAnalyticsArrayWithData(Attributes);
		
		AnalyticsProvider->RecordEvent(TEXT("PersistentPatchStats.Background"), MoveTemp(Attributes));
	}

	void FireEvent_PersistentPatchStats_Foreground(IAnalyticsProviderET* AnalyticsProvider, const InstallBundleManagerUtil::FPersistentStatContainer::FPersistentStatsInformation& PersistentStatInformation)
	{	
		if (AnalyticsProvider == nullptr)
		{
			return;
		}
		
		TArray<FAnalyticsEventAttribute> Attributes;
		PersistentStatInformation.FillOutAnalyticsArrayWithData(Attributes);
		
		AnalyticsProvider->RecordEvent(TEXT("PersistentPatchStats.Foreground"), MoveTemp(Attributes));
	}
}

