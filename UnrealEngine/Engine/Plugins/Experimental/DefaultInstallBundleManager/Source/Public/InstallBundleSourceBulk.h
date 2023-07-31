// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstallBundleSourceInterface.h"
#include "InstallBundleUtils.h"
#include "InstallBundleManagerUtil.h"
#include "Containers/Ticker.h"

class DEFAULTINSTALLBUNDLEMANAGER_API FInstallBundleSourceBulk : public IInstallBundleSource
{
public:
	FInstallBundleSourceBulk();
	FInstallBundleSourceBulk(const FInstallBundleSourceBulk& Other) = delete;
	FInstallBundleSourceBulk& operator=(const FInstallBundleSourceBulk& Other) = delete;
	virtual ~FInstallBundleSourceBulk();

private:
	bool Tick(float dt);

	void TickInit();

	// Init
	void AsyncInit_FireInitAnlaytic();
	void AsyncInit_MakeBundlesForBulkBuild();

	virtual EInstallBundleInstallState GetBundleInstallState(FName BundleName);

	// IInstallBundleSource Interface
public:
	virtual EInstallBundleSourceType GetSourceType() const override { return EInstallBundleSourceType::Bulk; }
	virtual float GetSourceWeight() const override { return 0.1f; }  // Low weight since all this source does in mount

	virtual FInstallBundleSourceInitInfo Init(
		TSharedRef<InstallBundleUtil::FContentRequestStatsMap> InRequestStats,
		TSharedPtr<IAnalyticsProviderET> AnalyticsProvider,
		TSharedPtr<InstallBundleUtil::PersistentStats::FPersistentStatContainerBase> PersistentStatsContainer) override;
	virtual void AsyncInit(FInstallBundleSourceInitDelegate Callback) override;

	virtual void AsyncInit_QueryBundleInfo(FInstallBundleSourceQueryBundleInfoDelegate Callback) override;

	virtual EInstallBundleManagerInitState GetInitState() const override;

	virtual FString GetContentVersion() const override;

	virtual TSet<FName> GetBundleDependencies(FName InBundleName, TSet<FName>* SkippedUnknownBundles /*= nullptr*/) const override;

	virtual void GetContentState(TArrayView<const FName> BundleNames, EInstallBundleGetContentStateFlags Flags, FInstallBundleGetContentStateDelegate Callback) override;

	virtual void RequestUpdateContent(FRequestUpdateContentBundleContext BundleContext) override;

	virtual void SetErrorSimulationCommands(const FString& CommandLine) override;

protected:
	FTSTicker::FDelegateHandle TickHandle;

	enum class EAsyncInitStep : int
	{
		None,
		MakeBundlesForBulkBuild,
		Finishing,
		Count,
	};
	friend const TCHAR* LexToString(FInstallBundleSourceBulk::EAsyncInitStep Val);

	enum class EAsyncInitStepResult : int
	{
		Waiting,
		Done,
	};

	EInstallBundleManagerInitState InitState = EInstallBundleManagerInitState::NotInitialized;
	EInstallBundleManagerInitResult InitResult = EInstallBundleManagerInitResult::OK;
	EAsyncInitStep InitStep = EAsyncInitStep::None;
	EAsyncInitStep LastInitStep = EAsyncInitStep::None;
	EAsyncInitStepResult InitStepResult = EAsyncInitStepResult::Done;
	bool bRetryInit = false;
	FInstallBundleSourceInitDelegate OnInitCompleteCallback;
	TArray<TUniquePtr<InstallBundleUtil::FInstallBundleTask>> InitAsyncTasks;

	TMap<FName, TArray<FString>> BulkBuildBundles; // BundleName -> Files

	TSharedPtr<IAnalyticsProviderET> AnalyticsProvider;
};
