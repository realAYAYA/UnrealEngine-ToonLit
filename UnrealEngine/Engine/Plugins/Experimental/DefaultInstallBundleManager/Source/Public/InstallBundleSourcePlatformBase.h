// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstallBundleSourceInterface.h"
#if WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
#include "Internationalization/Regex.h"


class DEFAULTINSTALLBUNDLEMANAGER_API FInstallBundleSourcePlatformBase : public IInstallBundleSource
{
private:
public:
	FInstallBundleSourcePlatformBase();
	FInstallBundleSourcePlatformBase(const FInstallBundleSourcePlatformBase& Other) = delete;
	FInstallBundleSourcePlatformBase& operator=(const FInstallBundleSourcePlatformBase& Other) = delete;
	virtual ~FInstallBundleSourcePlatformBase();


	// IInstallBundleSource Interface
public:
	virtual EInstallBundleSourceType GetSourceType() const override;

	virtual FInstallBundleSourceInitInfo Init(
		TSharedRef<InstallBundleUtil::FContentRequestStatsMap> InRequestStats,
		TSharedPtr<IAnalyticsProviderET> AnalyticsProvider,
		TSharedPtr<InstallBundleUtil::PersistentStats::FPersistentStatContainerBase> InPersistentStatsContainer) override;


	virtual void AsyncInit_QueryBundleInfo(FInstallBundleSourceQueryBundleInfoDelegate Callback) override;

	virtual EInstallBundleManagerInitState GetInitState() const override;

	virtual FString GetContentVersion() const override;

	virtual TSet<FName> GetBundleDependencies(FName InBundleName, TSet<FName>* SkippedUnknownBundles /*= nullptr*/) const override;


protected:
	virtual bool IsPlatformInstallBundleConfig(const FConfigFile* InstallBundleConfig, const FString& Section) const = 0;
	virtual bool QueryPersistentBundleInfo(FInstallBundleSourcePersistentBundleInfo& SourceBundleInfo) const = 0;

	template<typename T>
	inline void StatsBegin(FName BundleName, T State)
	{
		RequestStats->StatsBegin(BundleName, LexToString(State));
	}
	
	template<typename T>
	inline void StatsEnd(FName BundleName, T State, uint64 DataSize = 0)
	{
		RequestStats->StatsEnd(BundleName, LexToString(State), DataSize);
	}

	TSharedPtr<IAnalyticsProviderET> AnalyticsProvider;

	TSharedPtr<InstallBundleUtil::FContentRequestStatsMap> RequestStats;

	TSharedPtr<InstallBundleUtil::PersistentStats::FPersistentStatContainerBase> PersistentStatsContainer;

	EInstallBundleManagerInitState InitState = EInstallBundleManagerInitState::NotInitialized;

	TArray<TPair<FString, TArray<FRegexPattern>>> BundleRegexList; // BundleName -> FileRegex
};
#endif //WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
