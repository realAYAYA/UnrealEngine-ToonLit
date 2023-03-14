// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstallBundleSourceBulk.h"

#include "DefaultInstallBundleManagerPrivate.h"
#include "IPlatformFilePak.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Stats/Stats.h"

#define LOG_SOURCE_BULK(Verbosity, Format, ...) LOG_INSTALL_BUNDLE_MAN(Verbosity, TEXT("InstallBundleSourceBulk: ") Format, ##__VA_ARGS__)

#define LOG_SOURCE_BULK_OVERRIDE(VerbosityOverride, Verbosity, Format, ...) LOG_INSTALL_BUNDLE_MAN_OVERRIDE(VerbosityOverride, Verbosity, TEXT("InstallBundleSourceBulk: ") Format, ##__VA_ARGS__)

FInstallBundleSourceBulk::FInstallBundleSourceBulk()
{
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FInstallBundleSourceBulk::Tick));
}

FInstallBundleSourceBulk::~FInstallBundleSourceBulk()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	InstallBundleUtil::CleanupInstallBundleAsyncIOTasks(InitAsyncTasks);
}

bool FInstallBundleSourceBulk::Tick(float dt)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FInstallBundleSourceBulk_Tick);

	TickInit();

	// Check for Init Task Completion
	InstallBundleUtil::FinishInstallBundleAsyncIOTasks(InitAsyncTasks);

	return true;
}

void FInstallBundleSourceBulk::TickInit()
{
	if (!OnInitCompleteCallback.IsBound())
		return;

	while (InitState == EInstallBundleManagerInitState::NotInitialized && InitStepResult == EAsyncInitStepResult::Done)
	{
		if (InitResult != EInstallBundleManagerInitResult::OK)
		{
			if (LastInitStep != InitStep)
			{
				// Only fire init analytic for failures the first time we retry
				AsyncInit_FireInitAnlaytic();
				LastInitStep = InitStep;
			}

			if(bRetryInit)
			{
				LOG_SOURCE_BULK(Warning, TEXT("Retrying initialization after %s"), LexToString(InitResult));
				InitResult = EInstallBundleManagerInitResult::OK;
				bRetryInit = false;
			}
			else
			{
				LOG_SOURCE_BULK(Warning, TEXT("Initialization Failed - %s"), LexToString(InitResult));
				InitState = EInstallBundleManagerInitState::Failed;
				break;
			}
		}
		else
		{
			LastInitStep = InitStep;
			++InstallBundleUtil::CastAsUnderlying(InitStep);
		}

		switch (InitStep)
		{
		case EAsyncInitStep::None:
			LOG_SOURCE_BULK(Fatal, TEXT("Trying to use init state None"));
			break;
		case EAsyncInitStep::MakeBundlesForBulkBuild:
			AsyncInit_MakeBundlesForBulkBuild();
			break;
		case EAsyncInitStep::Finishing:
			AsyncInit_FireInitAnlaytic();
			InitState = EInstallBundleManagerInitState::Succeeded;
			break;
		default:
			LOG_SOURCE_BULK(Fatal, TEXT("Unknown Init Step %s"), LexToString(InitStep));
			break;
		}
	}

	if (InitState == EInstallBundleManagerInitState::Succeeded || InitState == EInstallBundleManagerInitState::Failed)
	{
		FInstallBundleSourceAsyncInitInfo InitInfo;
		InitInfo.Result = InitResult;
		InitInfo.bShouldUseFallbackSource = false;

		OnInitCompleteCallback.Execute(AsShared(), MoveTemp(InitInfo));
		OnInitCompleteCallback = nullptr;
	}
}

void FInstallBundleSourceBulk::AsyncInit_FireInitAnlaytic()
{
	LOG_SOURCE_BULK(Display, TEXT("Fire Init Analytic: %s"), LexToString(InitResult));

	InstallBundleManagerAnalytics::FireEvent_InitBundleSourceBulkComplete(AnalyticsProvider.Get(), LexToString(InitResult));

	InitStepResult = EAsyncInitStepResult::Done;
}

void FInstallBundleSourceBulk::AsyncInit_MakeBundlesForBulkBuild()
{
	LOG_SOURCE_BULK(Display, TEXT("Making Bundles for Bulk Build"));

	InitStepResult = EAsyncInitStepResult::Waiting;

	TArray<FString> PakSearchDirs;
	FPakPlatformFile::GetPakFolders(FCommandLine::Get(), PakSearchDirs);

	TSharedPtr<TArray<FString>, ESPMode::ThreadSafe> FoundFiles = MakeShared<TArray<FString>, ESPMode::ThreadSafe>();
	InstallBundleUtil::StartInstallBundleAsyncIOTask(InitAsyncTasks,
	[FoundFiles, PakSearchDirs=MoveTemp(PakSearchDirs), ContentDir=FPaths::ProjectContentDir()]()
	{
		for (const FString& SearchDir : PakSearchDirs)
		{
			IPlatformFile::GetPlatformPhysical().FindFilesRecursively(*FoundFiles, *SearchDir, nullptr);
		}

#if PLATFORM_IOS
		// Only scan the root folder on IOS for shaderlibs.  Running this on windows is very expensive
		// if the content dir contains loose assets which is common during development.
		IPlatformFile::GetPlatformPhysical().FindFiles(*FoundFiles, *ContentDir, TEXT(".metallib"));
#endif // PLATFORM_IOS
	},
	[this, FoundFiles]()
	{
		// Prune out any paks that were mounted at startup
		for (int i = 0; i < FoundFiles->Num();)
		{
			const FString& File = (*FoundFiles)[i];
			if (File.MatchesWildcard(FPakPlatformFile::GetMountStartupPaksWildCard()))
			{
				FoundFiles->RemoveAtSwap(i);
			}
			else
			{
				++i;
			}
		}

		TArray<FString> SectionNames;
		const FConfigFile* InstallBundleConfig = GConfig->FindConfigFile(GInstallBundleIni);
		if (InstallBundleConfig)
		{
			for (const TPair<FString, FConfigSection>& Pair : *InstallBundleConfig)
			{
				if (Pair.Key.StartsWith(InstallBundleUtil::GetInstallBundleSectionPrefix()))
				{
					SectionNames.Add(Pair.Key);
				}
			}

			// Bundle regex need to be applied in order
			SectionNames.StableSort([this](const FString& SectionA, const FString& SectionB) -> bool
			{
				int BundleAOrder = INT_MAX;
				int BundleBOrder = INT_MAX;

				if (!GConfig->GetInt(*SectionA, TEXT("Order"), BundleAOrder, GInstallBundleIni))
				{
					LOG_SOURCE_BULK(Warning, TEXT("Bundle Section %s doesn't have an order"), *SectionA);
				}

				if (!GConfig->GetInt(*SectionB, TEXT("Order"), BundleBOrder, GInstallBundleIni))
				{
					LOG_SOURCE_BULK(Warning, TEXT("Bundle Section %s doesn't have an order"), *SectionB);
				}

				return BundleAOrder < BundleBOrder;
			});

			// Sort remaining files into bundles
			for (const FString& Section : SectionNames)
			{
				const FString BundleName = Section.RightChop(InstallBundleUtil::GetInstallBundleSectionPrefix().Len());
				TArray<FString>& BundleFileList = BulkBuildBundles.FindOrAdd(FName(*BundleName));

				TArray<FString> StrSearchRegexPatterns;
				if (!InstallBundleConfig->GetArray(*Section, TEXT("FileRegex"), StrSearchRegexPatterns))
					continue;

				TArray<FRegexPattern> SearchRegexPatterns;
				SearchRegexPatterns.Reserve(StrSearchRegexPatterns.Num());
				for (const FString& Str : StrSearchRegexPatterns)
				{
					SearchRegexPatterns.Emplace(Str, ERegexPatternFlags::CaseInsensitive);
				}
				
				for (int i = 0; i < FoundFiles->Num();)
				{
					const FString& File = (*FoundFiles)[i];
					bool bMatches = false;
					for (const FRegexPattern& Pattern : SearchRegexPatterns)
					{
						if (FRegexMatcher(Pattern, File).FindNext())
						{
							bMatches = true;
							break;
						}
					}

					if (bMatches)
					{
						LOG_SOURCE_BULK(Verbose, TEXT("Adding %s to Bundle %s"), *File, *BundleName);
						
						BundleFileList.AddUnique(File);
						FoundFiles->RemoveAtSwap(i);
					}
					else
					{
						++i;
					}
				}
			}
		}

		InitStepResult = EAsyncInitStepResult::Done;
	});
}

EInstallBundleInstallState FInstallBundleSourceBulk::GetBundleInstallState(FName BundleName)
{
	return EInstallBundleInstallState::UpToDate;
}

FInstallBundleSourceInitInfo FInstallBundleSourceBulk::Init(
	TSharedRef<InstallBundleUtil::FContentRequestStatsMap> InRequestStats,
	TSharedPtr<IAnalyticsProviderET> InAnalyticsProvider,
	TSharedPtr<InstallBundleUtil::PersistentStats::FPersistentStatContainerBase> PersistentStatsContainer)
{
	AnalyticsProvider = MoveTemp(InAnalyticsProvider);

	//Ignoring PersistentStatsContainer as we currently don't care about any stats for this bulk source

	FInstallBundleSourceInitInfo InitInfo;
	return InitInfo;
}

void FInstallBundleSourceBulk::AsyncInit(FInstallBundleSourceInitDelegate Callback)
{
	check(OnInitCompleteCallback.IsBound() == false);
	OnInitCompleteCallback = MoveTemp(Callback);

	if (InitState == EInstallBundleManagerInitState::Failed)
	{
		InitState = EInstallBundleManagerInitState::NotInitialized;
		bRetryInit = true;
	}
}

void FInstallBundleSourceBulk::AsyncInit_QueryBundleInfo(FInstallBundleSourceQueryBundleInfoDelegate Callback)
{
	check(InitState == EInstallBundleManagerInitState::Succeeded);

	FInstallBundleSourceBundleInfoQueryResult ResultInfo;

	const FConfigFile* InstallBundleConfig = GConfig->FindConfigFile(GInstallBundleIni);
	if (InstallBundleConfig)
	{
		for (const TPair<FString, FConfigSection>& Pair : *InstallBundleConfig)
		{
			const FString& Section = Pair.Key;
			FInstallBundleSourcePersistentBundleInfo BundleInfo;
			if(!InstallBundleManagerUtil::LoadBundleSourceBundleInfoFromConfig(GetSourceType(), *InstallBundleConfig, Section, BundleInfo))
				continue;

			BundleInfo.BundleContentState = GetBundleInstallState(BundleInfo.BundleName);

			FName BundleName = BundleInfo.BundleName;
			ResultInfo.SourceBundleInfoMap.Add(BundleName, MoveTemp(BundleInfo));
		}
	}

	Callback.ExecuteIfBound(AsShared(), MoveTemp(ResultInfo));
}

EInstallBundleManagerInitState FInstallBundleSourceBulk::GetInitState() const
{
	return InitState;
}

FString FInstallBundleSourceBulk::GetContentVersion() const
{
	return InstallBundleUtil::GetAppVersion();
}

TSet<FName> FInstallBundleSourceBulk::GetBundleDependencies(FName InBundleName, TSet<FName>* SkippedUnknownBundles /*= nullptr*/) const
{
	return InstallBundleManagerUtil::GetBundleDependenciesFromConfig(InBundleName, SkippedUnknownBundles);
}

void FInstallBundleSourceBulk::GetContentState(TArrayView<const FName> BundleNames, EInstallBundleGetContentStateFlags Flags, FInstallBundleGetContentStateDelegate Callback)
{
	FInstallBundleCombinedContentState State;
	State.CurrentVersion.Add(GetSourceType(), InstallBundleUtil::GetAppVersion());
	for (const FName& BundleName : BundleNames)
	{
		if(!BulkBuildBundles.Contains(BundleName))
			continue;

		LOG_SOURCE_BULK(Verbose, TEXT("Requesting Content State for %s"), *BundleName.ToString());

		FInstallBundleContentState& IndividualBundleState = State.IndividualBundleStates.Add(BundleName);
		IndividualBundleState.State = GetBundleInstallState(BundleName);
		IndividualBundleState.Version.Add(GetSourceType(), InstallBundleUtil::GetAppVersion());
	}

	for (TPair<FName, FInstallBundleContentState>& Pair : State.IndividualBundleStates)
	{
		Pair.Value.Weight = 1.0f / State.IndividualBundleStates.Num();
	}

	Callback.ExecuteIfBound(MoveTemp(State));
}

void FInstallBundleSourceBulk::RequestUpdateContent(FRequestUpdateContentBundleContext Context)
{
	LOG_SOURCE_BULK_OVERRIDE(Context.LogVerbosityOverride, Display, TEXT("Requesting Bundle %s"), *Context.BundleName.ToString());

	FInstallBundleSourceUpdateContentResultInfo ResultInfo;
	ResultInfo.BundleName = Context.BundleName;
	ResultInfo.Result = EInstallBundleResult::OK;
	TArray<FString>* BundleFileList = BulkBuildBundles.Find(Context.BundleName);
	if (BundleFileList)
	{
		ResultInfo.ContentPaths = *BundleFileList;
	}

#if PLATFORM_IOS
	for (const FString& Path : ResultInfo.ContentPaths)
	{
		if (Path.EndsWith(TEXT(".metallib")))
		{
			LOG_SOURCE_BULK_OVERRIDE(Context.LogVerbosityOverride, Display, TEXT("Found metallib %s for Bundle %s"), *Path, *Context.BundleName.ToString());
			
			ResultInfo.NonUFSShaderLibPaths.Add(FPaths::GetPath(Path));
		}
	}
#endif // PLATFORM_IOS

	Context.CompleteCallback.ExecuteIfBound(AsShared(), MoveTemp(ResultInfo));
}

void FInstallBundleSourceBulk::SetErrorSimulationCommands(const FString& CommandLine)
{
#if INSTALL_BUNDLE_ALLOW_ERROR_SIMULATION
#endif // INSTALL_BUNDLE_ALLOW_ERROR_SIMULATION
}

const TCHAR* LexToString(FInstallBundleSourceBulk::EAsyncInitStep Val)
{
	static const TCHAR* Strings[] =
	{
		TEXT("InstallBundleSourceBulk:None"),
		TEXT("InstallBundleSourceBulk:MakeBundlesForBulkBuild"),
		TEXT("InstallBundleSourceBulk:Finishing"),
	};
	static_assert(InstallBundleUtil::CastToUnderlying(FInstallBundleSourceBulk::EAsyncInitStep::Count) == UE_ARRAY_COUNT(Strings), "");

	return Strings[InstallBundleUtil::CastToUnderlying(Val)];
}
