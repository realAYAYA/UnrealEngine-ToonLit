// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstallBundleManagerInterface.h"
#include "InstallBundleManagerUtil.h"
#include "InstallBundleCache.h"
#include "InstallBundleUtils.h"

#include "Delegates/IDelegateInstance.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Templates/Function.h"
#include "Containers/Ticker.h"
#include "Containers/Array.h"
#include "Containers/StaticArray.h"
#include "Containers/ArrayView.h"
#include "PatchCheck.h"

#ifndef INSTALL_BUNDLE_ENABLE_ANALYTICS
	#define INSTALL_BUNDLE_ENABLE_ANALYTICS (!WITH_EDITOR)
#endif

class IInstallBundleSource;
class IAnalyticsProviderET;

class DEFAULTINSTALLBUNDLEMANAGER_API FDefaultInstallBundleManager : public IInstallBundleManager
{
protected:
	// Strongly Typed enums do not work well for this thing's use case
	struct FContentRequestBatchNS
	{
		enum Enum : int
		{
			Requested,
			Cache,
			Install,
			Count,
		};
	};
	using EContentRequestBatch = FContentRequestBatchNS::Enum;
	friend struct NEnumRangePrivate::TEnumRangeTraits<EContentRequestBatch>;
	friend DEFAULTINSTALLBUNDLEMANAGER_API const TCHAR* LexToString(FDefaultInstallBundleManager::EContentRequestBatch Val);

	// Strongly Typed enums do not work well for this thing's use case
	struct FContentReleaseRequestBatchNS
	{
		enum Enum : int
		{
			Requested,
			Release,
			Count,
		};
	};
	using EContentReleaseRequestBatch = FContentReleaseRequestBatchNS::Enum;
	friend struct NEnumRangePrivate::TEnumRangeTraits<EContentReleaseRequestBatch>;
	friend DEFAULTINSTALLBUNDLEMANAGER_API const TCHAR* LexToString(FDefaultInstallBundleManager::EContentReleaseRequestBatch Val);
	
	enum class EBundleState : int
	{
		NotInstalled,
		NeedsUpdate,
		NeedsMount,
		Mounted,
		Count,
	};
	friend const TCHAR* LexToString(FDefaultInstallBundleManager::EBundleState Val)
	{
		static const TCHAR* Strings[] =
		{
			TEXT("NotInstalled"),
			TEXT("NeedsUpdate"),
			TEXT("NeedsMount"),
			TEXT("Mounted"),
		};

		return InstallBundleUtil::TLexToString(Val, Strings);
	}
	bool StateSignifiesNeedsInstall(EBundleState StateIn)
	{
		return (StateIn == EBundleState::NotInstalled || StateIn == EBundleState::NeedsUpdate);
	}

	enum class EAsyncInitStep : int
	{
		None,
		InitBundleSources,
		InitBundleCaches,
		QueryBundleInfo,
		SetUpdateBundleInfoCallback,
		CreateAnalyticsSession,
		Finishing,
		Count,
	};

	friend const TCHAR* LexToString(FDefaultInstallBundleManager::EAsyncInitStep Val)
	{
		static const TCHAR* Strings[] =
		{
			TEXT("None"),
			TEXT("InitBundleSources"),
			TEXT("InitBundleCaches"),
			TEXT("QueryBundleInfo"),
			TEXT("SetUpdateBundleInfoCallback"),
			TEXT("CreateAnalyticsSession"),
			TEXT("Finishing"),
		};

		return InstallBundleUtil::TLexToString(Val, Strings);
	}

	enum class EAsyncInitStepResult : int
	{
		Waiting,
		Done,
	};
	
	enum class EBundlePrereqs : int
	{
		CacheHintRequested,
		RequiresLatestClient,
		HasNoPendingCancels,
		HasNoPendingReleaseRequests,
		HasNoPendingUpdateRequests,
		DetermineSteps,
		Count
	};

	friend const TCHAR* LexToString(EBundlePrereqs Val)
	{
		static const TCHAR* Strings[] =
		{
			TEXT("CacheHintRequested"),
			TEXT("RequiresLatestClient"),
			TEXT("HasNoPendingCancels"),
			TEXT("HasNoPendingReleaseRequests"),
			TEXT("HasNoPendingUpdateRequests"),
			TEXT("DetermineSteps"),
		};

		return InstallBundleUtil::TLexToString(Val, Strings);
	}

	struct FBundleSourceRelevance
	{
		EInstallBundleSourceType SourceType;
		bool bIsRelevant = true;

		bool operator==(const FBundleSourceRelevance& Other) const { return SourceType == Other.SourceType; }
	};

	struct FBundleContentPaths
	{
		TArray<FString> ContentPaths;
		TArray<FString> AdditionalRootDirs;
		TSet<FString> NonUFSShaderLibPaths;
	};

	friend struct FBundleInfo;
	struct FBundleInfo
	{
	public:
		EBundleState GetBundleStatus(const FDefaultInstallBundleManager& BundleMan) const
		{
			check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
			check(BundleMan.InitState == EInstallBundleManagerInitState::Succeeded || BundleMan.bIsCurrentlyInAsyncInit);
			return BundleStatus;
		}
		void SetBundleStatus(const FDefaultInstallBundleManager& BundleMan, EBundleState InBundleState)
		{
			check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
			check(BundleMan.InitState == EInstallBundleManagerInitState::Succeeded || BundleMan.bIsCurrentlyInAsyncInit);
			BundleStatus = InBundleState;
		}
		bool GetMustWaitForPSOCache(const FDefaultInstallBundleManager& BundleMan) const
		{
			check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
			check(BundleMan.InitState == EInstallBundleManagerInitState::Succeeded);
			return bWaitForPSOCache;
		}
		uint32 GetInitialShaderPrecompiles(const FDefaultInstallBundleManager& BundleMan) const
		{
			check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
			check(BundleMan.InitState == EInstallBundleManagerInitState::Succeeded);
			return InitialShaderPrecompiles;
		}
		void SetMustWaitForPSOCache(const FDefaultInstallBundleManager& BundleMan, uint32 InNumPSOPrecompilesRemaining)
		{
			check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
			check(BundleMan.InitState == EInstallBundleManagerInitState::Succeeded);
			bWaitForPSOCache = InNumPSOPrecompilesRemaining > 0;
			if (InNumPSOPrecompilesRemaining > InitialShaderPrecompiles)
			{
				InitialShaderPrecompiles = InNumPSOPrecompilesRemaining;
			}
		}
	private:
		EBundleState BundleStatus = EBundleState::NotInstalled;
		bool bWaitForPSOCache = false;
		uint32 InitialShaderPrecompiles = 0;
	public:
		FString BundleNameString; // Since FNames do not preserve casing
		TArray<EBundlePrereqs> Prereqs;
		bool bIsStartup = false;
		TArray<FBundleSourceRelevance> ContributingSources; // Sources contributing to this bundle info
		EInstallBundlePriority Priority = EInstallBundlePriority::Low;
		FBundleContentPaths ContentPaths; // Only valid if BundleStatus >= NeedsMount
	};

	// GetBundleStatus protects erroneous accesses of the bundle status before initialization is complete by throwing an assert 
	EBundleState GetBundleStatus(const FBundleInfo& BundleInfo) const;
	void SetBundleStatus(FBundleInfo& BundleInfo, EBundleState InBundleState);
	bool GetMustWaitForPSOCache(const FBundleInfo& BundleInfo) const;
	uint32 GetInitialShaderPrecompiles(const FBundleInfo& BundleInfo) const;
	void SetMustWaitForPSOCache(FBundleInfo& BundleInfo, uint32 InNumPSOPrecompilesRemaining);

	struct FGetContentStateRequest
	{
		TMap<EInstallBundleSourceType, FInstallBundleCombinedContentState> BundleSourceContentStates;

		TArray<FName> BundleNames;

		EInstallBundleGetContentStateFlags Flags = EInstallBundleGetContentStateFlags::None;

		bool Started = false;
		bool bCancelled = false;

		//Used to track an individual request so that it can be canceled
		FName RequestTag;

		void SetCallback(FInstallBundleGetContentStateDelegate NewCallback)
		{
			Callback = MoveTemp(NewCallback);
		}

		void ExecCallbackIfValid(FInstallBundleCombinedContentState BundleState)
		{
			if (!bCancelled)
			{
				Callback.ExecuteIfBound(MoveTemp(BundleState));
			}
		}

		const FDelegateHandle GetCallbackDelegateHandle()
		{
			return Callback.GetHandle();
		}

	private:
		FInstallBundleGetContentStateDelegate Callback;
	};
	using FGetContentStateRequestRef = TSharedRef<FGetContentStateRequest>;
	using FGetContentStateRequestPtr = TSharedPtr<FGetContentStateRequest>;

	struct FGetInstallStateRequest
	{
		TArray<FName> BundleNames;

		bool bCancelled = false;

		//Used to track an individual request so that it can be canceled
		FName RequestTag;

		void SetCallback(FInstallBundleGetInstallStateDelegate NewCallback)
		{
			Callback = MoveTemp(NewCallback);
		}

		void ExecCallbackIfValid(FInstallBundleCombinedInstallState BundleState)
		{
			if (!bCancelled)
			{
				Callback.ExecuteIfBound(MoveTemp(BundleState));
			}
		}

		const FDelegateHandle GetCallbackDelegateHandle()
		{
			return Callback.GetHandle();
		}

	private:
		FInstallBundleGetInstallStateDelegate Callback;
	};
	using FGetInstallStateRequestRef = TSharedRef<FGetInstallStateRequest>;
	using FGetInstallStateRequestPtr = TSharedPtr<FGetInstallStateRequest>;

	enum class EContentRequestStepResult : int
	{
		Waiting,
		Done,
	};

	enum class EContentRequestState : int
	{
		ReservingCache,
		FinishingCache,
		UpdatingBundleSources,
		Mounting,
		WaitingForShaderCache,
		Finishing,
		CleaningUp,
		Count,
	};

	friend const TCHAR* LexToString(EContentRequestState Val)
	{
		static const TCHAR* Strings[] =
		{
			TEXT("ReservingCache"),
			TEXT("FinishingCache"),
			TEXT("UpdatingBundleSources"),
			TEXT("Mounting"),
			TEXT("WaitingForShaderCache"),
			TEXT("Finishing"),
			TEXT("CleaningUp"),
		};

		return InstallBundleUtil::TLexToString(Val, Strings);
	}

	enum class ECacheEvictionRequestorType : int
	{
		CacheFlush,
		ContentRequest
	};

	struct FCacheEvictionRequestor
	{
		TMap<FName, TArray<EInstallBundleSourceType>> BundlesToEvictFromSourcesMap;

		virtual ~FCacheEvictionRequestor() {}

		virtual FString GetEvictionRequestorName() const = 0;
		virtual ECacheEvictionRequestorType GetEvictionRequestorType() const = 0;
		virtual ELogVerbosity::Type GetLogVerbosityOverride() const = 0;
	};
	using FCacheEvictionRequestorRef = TSharedRef<FCacheEvictionRequestor>;
	using FCacheEvictionRequestorPtr = TSharedPtr<FCacheEvictionRequestor>;
	using FCacheEvictionRequestorWeakPtr = TWeakPtr<FCacheEvictionRequestor>;

	struct FCacheFlushRequest : FCacheEvictionRequestor
	{
		FInstallBundleSourceOrCache SourceOrCache; // Bundles are evicted from all caches, but we gather them from only this one if set
		ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging;

		FInstallBundleManagerFlushCacheCompleteDelegate Callback;

		virtual FString GetEvictionRequestorName() const override 
		{
			if (SourceOrCache.HasSubtype<FName>())
			{
				return FString::Printf(TEXT("CacheFlush(%s)"), *SourceOrCache.GetSubtype<FName>().ToString());
			}
			else if (SourceOrCache.HasSubtype<EInstallBundleSourceType>())
			{
				return FString::Printf(TEXT("CacheFlush(%s)"), LexToString(SourceOrCache.GetSubtype<EInstallBundleSourceType>()));
			}
			else
			{
				return TEXT("CacheFlush(All)");
			}
		}
		virtual ECacheEvictionRequestorType GetEvictionRequestorType() const override { return ECacheEvictionRequestorType::CacheFlush; }
		virtual ELogVerbosity::Type GetLogVerbosityOverride() const override { return LogVerbosityOverride; }
	};
	using FCacheFlushRequestRef = TSharedRef<FCacheFlushRequest>;
	using FCacheFlushRequestPtr = TSharedPtr<FCacheFlushRequest>;
	using FCacheFlushRequestWeakPtr = TWeakPtr<FCacheFlushRequest>;

	struct FContentRequest : FCacheEvictionRequestor
	{
		EContentRequestStepResult StepResult = EContentRequestStepResult::Done;
		TArray<EContentRequestState> Steps;
		int32 iStep = INDEX_NONE;
		TStaticArray<int32, EContentRequestBatch::Count> iOnCanceledStep{ InPlace, INDEX_NONE };

		TArray<EBundlePrereqs> Prereqs;
		int32 iPrereq = INDEX_NONE;
		FDelegateHandle CheckLastestClientDelegateHandle;

		EInstallBundleRequestFlags Flags = EInstallBundleRequestFlags::None;

		ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging;

		bool bShouldSendAnalytics = true;
		bool bIsCanceled = false;
		bool bFinishWhenCanceled = true;  // Whether to run cleanup and callback when canceled
		bool bDidCacheHintRequested = false; // Whether this request hinted to the bundle caches that the bundle was requested
		bool bContentWasInstalled = false;
		EInstallBundleResult Result = EInstallBundleResult::OK;

		FName BundleName;

		TMap<EInstallBundleSourceType, EInstallBundlePauseFlags> SourcePauseFlags;
		EInstallBundlePauseFlags LastSentPauseFlags = EInstallBundlePauseFlags::None;
		bool bForcePauseCallback = false;

		EInstallBundleCacheReserveResult LastCacheReserveResult = EInstallBundleCacheReserveResult::Success;

		TMap<EInstallBundleSourceType, FInstallBundleSourceUpdateContentResultInfo> SourceRequestResults;
		FText OptionalErrorText;
		FString OptionalErrorCode;

		TMap<EInstallBundleSourceType, FInstallBundleSourceProgress> CachedSourceProgress;

		InstallBundleUtil::FContentRequestSharedContextPtr RequestSharedContext;

		// If needed, Keep the engine awake while process requests
		TOptional<InstallBundleUtil::FInstallBundleManagerKeepAwake> KeepAwake;

		// If needed, Banish screen savers
		TOptional<InstallBundleUtil::FInstallBundleManagerScreenSaverControl> ScreenSaveControl;

		virtual FString GetEvictionRequestorName() const override { return BundleName.ToString(); }
		virtual ECacheEvictionRequestorType GetEvictionRequestorType() const override { return ECacheEvictionRequestorType::ContentRequest; }
		virtual ELogVerbosity::Type GetLogVerbosityOverride() const override { return LogVerbosityOverride;  }
	};
	using FContentRequestRef = TSharedRef<FContentRequest>;
	using FContentRequestPtr = TSharedPtr<FContentRequest>;
	using FContentRequestWeakPtr = TWeakPtr<FContentRequest>;

	enum class EContentReleaseRequestState : int
	{
		Unmounting,
		UpdatingBundleSources,
		Finishing,
		CleaningUp,
		Count
	};
	
	friend const TCHAR* LexToString(EContentReleaseRequestState Val)
	{
		static const TCHAR* Strings[] =
		{
			TEXT("Unmounting"),
			TEXT("UpdatingBundleSources"),
			TEXT("Finishing"),
			TEXT("CleaningUp"),
		};

		return InstallBundleUtil::TLexToString(Val, Strings);
	}

	struct FContentReleaseRequest
	{
		EContentRequestStepResult StepResult = EContentRequestStepResult::Done;
		TArray<EContentReleaseRequestState> Steps;
		int32 iStep = INDEX_NONE;
		TStaticArray<int32, EContentReleaseRequestBatch::Count> iOnCanceledStep{ InPlace, INDEX_NONE };

		TArray<EBundlePrereqs> Prereqs;
		int32 iPrereq = INDEX_NONE;
		
		EInstallBundleReleaseRequestFlags Flags = EInstallBundleReleaseRequestFlags::None;

		EInstallBundleReleaseResult Result = EInstallBundleReleaseResult::OK;
		
		FName BundleName;

		TMap<EInstallBundleSourceType, TOptional<FInstallBundleSourceReleaseContentResultInfo>> SourceReleaseRequestResults;
		TMap<EInstallBundleSourceType, TOptional<FInstallBundleSourceReleaseContentResultInfo>> SourceRemoveRequestResults;

		ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging;

		bool bIsCanceled = false;
		bool bFinishWhenCanceled = true;  // Whether to run cleanup and callback when canceled
	};
	using FContentReleaseRequestRef = TSharedRef<FContentReleaseRequest>;
	using FContentReleaseRequestPtr = TSharedPtr<FContentReleaseRequest>;
	using FContentReleaseRequestWeakPtr = TWeakPtr<FContentReleaseRequest>;

	struct FContentPatchCheckSharedContext
	{
		TMap<EInstallBundleSourceType, bool> Results;
	};
	using FContentPatchCheckSharedContextRef = TSharedRef<FContentPatchCheckSharedContext>;

public:
	typedef TFunction<TSharedPtr<IInstallBundleSource>(EInstallBundleSourceType)> FInstallBundleSourceFactoryFunction;

	FDefaultInstallBundleManager(const TCHAR* InConfigBaseName = nullptr, FInstallBundleSourceFactoryFunction InBundleSourceFactory = nullptr);
	FDefaultInstallBundleManager(const FDefaultInstallBundleManager& Other) = delete;
	FDefaultInstallBundleManager& operator=(const FDefaultInstallBundleManager& Other) = delete;

	virtual ~FDefaultInstallBundleManager();

	virtual void Initialize() override;

	//Tick
protected:
	bool Tick(float dt);

	EInstallBundleManagerInitErrorHandlerResult HandleAsyncInitError(EInstallBundleManagerInitResult InitResultError);

	void TickInit();

	void TickGetContentState();

	void TickGetInstallState();

	FInstallBundleCombinedInstallState GetInstallStateInternal(TArrayView<const FName> BundleNames) const;

	void CacheHintRequested(FContentRequestRef Request, bool bRequested);

	void CheckPrereqHasNoPendingCancels(FContentRequestRef Request);

	void CheckPrereqHasNoPendingCancels(FContentReleaseRequestRef Request);

	void CheckPrereqHasNoPendingReleaseRequests(FContentRequestRef Request);

	void CheckPrereqHasNoPendingUpdateRequests(FContentReleaseRequestRef Request);

	void CheckPrereqLatestClient(FContentRequestRef Request);

	void HandlePatchInformationReceived(EInstallBundleManagerPatchCheckResult Result, FContentRequestRef Request);

	void DetermineSteps(FContentRequestRef Request);

	void DetermineSteps(FContentReleaseRequestRef Request);

	void AddRequestToInitialBatch(FContentRequestRef Request);

	void AddRequestToInitialBatch(FContentReleaseRequestRef Request);

	void ReserveCache(FContentRequestRef Request);

	void TryReserveCache(FContentRequestRef Request);

	void RequestEviction(FCacheEvictionRequestorRef Requestor);

	void CacheEvictionComplete(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceReleaseContentResultInfo InResultInfo);
	void CacheEvictionComplete(TSharedRef<IInstallBundleSource> Source, const FInstallBundleSourceReleaseContentResultInfo& InResultInfo, FCacheEvictionRequestorRef Requestor);

	void UpdateBundleSources(FContentRequestRef Request);

	void UpdateBundleSourceComplete(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceUpdateContentResultInfo InResultInfo, FContentRequestRef Request);

	void UpdateBundleSourcePause(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourcePauseInfo InPauseInfo, FContentRequestRef Request);

	void UpdateBundleSources(FContentReleaseRequestRef Request);

	void UpdateBundleSourceReleaseComplete(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceReleaseContentResultInfo InResultInfo, FContentReleaseRequestRef Request);

	void MountPaks(FContentRequestRef Request);

	static bool MountPaksInList(TArrayView<FString> Paths, ELogVerbosity::Type LogVerbosityOverride);

	static bool UnmountPaksInList(TArrayView<FString> Paths, ELogVerbosity::Type LogVerbosityOverride);

	void UnmountPaks(FContentReleaseRequestRef Request);

	virtual void OnPaksMountedInternal(FContentRequestRef Request, FBundleInfo& BundleInfo) {}
	virtual void OnPaksUnmountedInternal(FContentReleaseRequestRef Request, FBundleInfo& BundleInfo) {};

	void WaitForShaderCache(FContentRequestRef Request);

	void FinishRequest(FContentRequestRef Request);

	void FinishRequest(FContentReleaseRequestRef Request);

	void TickUpdatePrereqs();

	void TickReleasePrereqs();

	void TickContentRequests();

	void TickReserveCache();

	void TickCacheFlush();

	void TickWaitForShaderCache();

	void TickPauseStatus(bool bForceCallback);

	void TickAsyncMountTasks();

	void TickReleaseRequests();

	void TickPruneBundleInfo();

	void IterateContentRequests(TFunctionRef<bool(const FContentRequestRef& QueuedRequest)> OnFound);
	void IterateReleaseRequests(TFunctionRef<bool(const FContentReleaseRequestRef& QueuedRequest)> OnFound);

	void IterateContentRequestsForBundle(FName BundleName, TFunctionRef<bool(const FContentRequestRef& QueuedRequest)> OnFound);
	void IterateReleaseRequestsForBundle(FName BundleName, TFunctionRef<bool(const FContentReleaseRequestRef& QueuedRequest)> OnFound);

	TSet<FName> GetBundleDependencies(FName InBundleName, bool* bSkippedUnknownBundles = nullptr) const;

	TSet<FName> GetBundleDependencies(TArrayView<const FName> InBundleNames, bool* bSkippedUnknownBundles = nullptr) const;

	TSet<FName> GatherBundlesForRequest(TArrayView<const FName> InBundleNames, EInstallBundleRequestInfoFlags& OutFlags);

	TSet<FName> GatherBundlesForRequest(TArrayView<const FName> InBundleNames);

	EInstallBundleSourceType GetBundleSourceFallback(EInstallBundleSourceType Type) const;

	EInstallBundleSourceUpdateBundleInfoResult OnUpdateBundleInfoFromSource(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceUpdateBundleInfoResult Result);
	void OnBundleLostRelevanceForSource(TSharedRef<IInstallBundleSource> Source, TSet<FName> BundleNames);

	void StartClientPatchCheck();
	void StartContentPatchCheck();
	void HandleClientPatchCheck(EPatchCheckResult Result);
	void HandleBundleSourceContentPatchCheck(TSharedRef<IInstallBundleSource> Source, bool bContentPatchRequired, FContentPatchCheckSharedContextRef Context);
	void HandleContentPatchCheck(FContentPatchCheckSharedContextRef Context);

	bool CancelUpdateContentInternal(TArrayView<const FName> BundleNames);
	bool CancelReleaseContentInternal(TArrayView<const FName> BundleNames);

	// IInstallBundleManager interface
public:
	virtual bool HasBundleSource(EInstallBundleSourceType SourceType) const override;

	virtual FDelegateHandle PushInitErrorCallback(FInstallBundleManagerInitErrorHandler Callback) override;
	virtual void PopInitErrorCallback() override;
	virtual void PopInitErrorCallback(FDelegateHandle Handle) override;
	virtual void PopInitErrorCallback(const void* InUserObject) override;

	virtual EInstallBundleManagerInitState GetInitState() const override;

	virtual TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> RequestUpdateContent(TArrayView<const FName> InBundleNames, EInstallBundleRequestFlags Flags, ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging) override;

	virtual FDelegateHandle GetContentState(TArrayView<const FName> InBundleNames, EInstallBundleGetContentStateFlags Flags, bool bAddDependencies, FInstallBundleGetContentStateDelegate Callback, FName RequestTag = TEXT("None")) override;

	virtual void CancelAllGetContentStateRequestsForTag(FName RequestTag) override;
	virtual void CancelAllGetContentStateRequests(FDelegateHandle Handle) override;

	virtual FDelegateHandle GetInstallState(TArrayView<const FName> BundleNames, bool bAddDependencies, FInstallBundleGetInstallStateDelegate Callback, FName RequestTag = NAME_None) override;

	virtual TValueOrError<FInstallBundleCombinedInstallState, EInstallBundleResult> GetInstallStateSynchronous(TArrayView<const FName> BundleNames, bool bAddDependencies) const override;

	virtual void CancelAllGetInstallStateRequestsForTag(FName RequestTag) override;
	virtual void CancelAllGetInstallStateRequests(FDelegateHandle Handle) override;

	virtual TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> RequestReleaseContent(TArrayView<const FName> ReleaseNames, EInstallBundleReleaseRequestFlags Flags, TArrayView<const FName> KeepNames = TArrayView<const FName>(), ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging) override;
	
	virtual EInstallBundleResult FlushCache(FInstallBundleSourceOrCache SourceOrCache, FInstallBundleManagerFlushCacheCompleteDelegate Callback, ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging) override;

	virtual TArray<FInstallBundleCacheStats> GetCacheStats(EInstallBundleCacheDumpToLog DumpToLog = EInstallBundleCacheDumpToLog::None, ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging) override;
	virtual TOptional<FInstallBundleCacheStats> GetCacheStats(FInstallBundleSourceOrCache SourceOrCache, EInstallBundleCacheDumpToLog DumpToLog = EInstallBundleCacheDumpToLog::None, ELogVerbosity::Type LogVerbosityOverride = ELogVerbosity::NoLogging) override;

	virtual void RequestRemoveContentOnNextInit(TArrayView<const FName> RemoveNames, TArrayView<const FName> KeepNames = TArrayView<const FName>()) override;

	virtual void CancelRequestRemoveContentOnNextInit(TArrayView<const FName> BundleName) override;

	virtual void CancelUpdateContent(TArrayView<const FName> BundleNames) override;

	virtual void PauseUpdateContent(TArrayView<const FName> BundleNames) override;

	virtual void ResumeUpdateContent(TArrayView<const FName> BundleNames) override;

	virtual void RequestPausedBundleCallback() override;

	virtual TOptional<FInstallBundleProgress> GetBundleProgress(FName BundleName) const override;

	virtual EInstallBundleRequestFlags GetModifyableContentRequestFlags() const override;
	virtual void UpdateContentRequestFlags(TArrayView<const FName> BundleNames, EInstallBundleRequestFlags AddFlags, EInstallBundleRequestFlags RemoveFlags) override;

	virtual void StartPatchCheck() override;
	virtual void AddEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag, FInstallBundleManagerEnvironmentWantsPatchCheck Delegate) override;
	virtual void RemoveEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag) override;
	virtual bool SupportsEarlyStartupPatching() const override;

	virtual bool IsNullInterface() const override;

	virtual void SetErrorSimulationCommands(const FString& CommandLine) override;

	//For overrides that we need to handle even when in a shipping build
	void SetCommandLineOverrides(const FString& CommandLine);

	virtual TSharedPtr<IAnalyticsProviderET> GetAnalyticsProvider() const override;

	virtual void StartSessionPersistentStatTracking(const FString& SessionName, const TArray<FName>& RequiredBundles = TArray<FName>(), const FString& ExpectedAnalyticsID = FString(), bool bForceResetStatData = false, const FInstallBundleCombinedContentState* State = nullptr) override;
	virtual void StopSessionPersistentStatTracking(const FString& SessionName) override;
	
protected:
	//Special version of these to wrap our calls to PersistentStats
	void StartBundlePersistentStatTracking(TSharedRef<FContentRequest> ContentRequest, const FString& ExpectedAnalyticsID = FString(), bool bForceResetStatData = false);
	void StopBundlePersistentStatTracking(TSharedRef<FContentRequest> ContentRequest);
	void PersistentTimingStatsBegin(TSharedRef<FContentRequest> ContentRequest, InstallBundleUtil::PersistentStats::ETimingStatNames TimerStatName);
	void PersistentTimingStatsEnd(TSharedRef<FContentRequest> ContentRequest, InstallBundleUtil::PersistentStats::ETimingStatNames TimerStatName);
	
	// Initialization state machine
protected:
	EInstallBundleManagerInitResult Init_DefaultBundleSources();
	EInstallBundleManagerInitResult Init_TryCreateBundleSources(TArray<EInstallBundleSourceType> SourcesToCreate, TArray<TSharedPtr<IInstallBundleSource>>* OutNewSources = nullptr);

	EInstallBundleSourceType FindFallbackSource(EInstallBundleSourceType SourceType);
	void AsyncInit_InitBundleSources();
	void AsyncInit_OnBundleSourceInitComplete(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceAsyncInitInfo InInitInfo);
	void AsyncInit_InitBundleCaches();
	void AsyncInit_QueryBundleInfo();
	void AsyncInit_OnQueryBundleInfoComplete(TSharedRef<IInstallBundleSource> Source, FInstallBundleSourceBundleInfoQueryResult Result);
	void AsyncInit_OnQueryBundleInfoComplete_HandleClientPatchCheck(EPatchCheckResult Result);
	void AsyncInit_SetUpdateBundleInfoCallback();
	void AsyncInit_CreateAnalyticsSession();
	void AsyncInit_FireInitAnlaytic(bool bCanRetry);
	void StatsBegin(FName BundleName);
	void StatsEnd(FName BundleName);
	void StatsBegin(FName BundleName, EContentRequestState State);
	void StatsEnd(FName BundleName, EContentRequestState State, uint64 DataSize = 0);
	void LogStats(FName BundleName, ELogVerbosity::Type LogVerbosityOverride);

protected:
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle AsyncInit_PatchCheckHandle;
	FDelegateHandle PatchCheckHandle;
	FInstallBundleSourceFactoryFunction InstallBundleSourceFactory;

	TMap<FName, FBundleInfo> BundleInfoMap;
	TSet<FName> BundlesInfosToPrune;

	TMap<EInstallBundleSourceType, TSharedPtr<IInstallBundleSource>> BundleSources;
	TMap<EInstallBundleSourceType, EInstallBundleSourceType> BundleSourceFallbacks;

	TMap<FName, TSharedRef<FInstallBundleCache>> BundleCaches;
	TMap<EInstallBundleSourceType, FName> BundleSourceCaches;

	TMap<TTuple<EInstallBundleSourceType, FName>, TArray<FCacheEvictionRequestorRef>> PendingCacheEvictions; // (Source, Bundle) -> List of requestors
	TMap<TTuple<FName, FName>, TArray<EInstallBundleSourceType>> CachesPendingEvictToSources; // (Cache, Bundle) -> List of Sources

	// Only used during Init
	TMap<EInstallBundleSourceType, TOptional<FInstallBundleSourceAsyncInitInfo>> BundleSourceInitResults;
	TMap<EInstallBundleSourceType, FInstallBundleSourceBundleInfoQueryResult> BundleSourceBundleInfoQueryResults;

	// Init
	EInstallBundleManagerInitState InitState = EInstallBundleManagerInitState::NotInitialized;
	EInstallBundleManagerInitResult InitResult = EInstallBundleManagerInitResult::OK;
	TArray<FInstallBundleManagerInitErrorHandler> InitErrorHandlerStack;
	TArray<TSharedPtr<IInstallBundleSource>> BundleSourcesToDelete;
	EAsyncInitStep InitStep = EAsyncInitStep::None;
	EAsyncInitStep LastInitStep = EAsyncInitStep::None;
	EAsyncInitStepResult InitStepResult = EAsyncInitStepResult::Done;
	bool bUnrecoverableInitError = false;
	bool bIsCurrentlyInAsyncInit = false;
	double LastInitRetryTimeSeconds = 0.0;
	double InitRetryTimeDeltaSeconds = 0.0;

	// Content State Requests
	TArray<FGetContentStateRequestRef> GetContentStateRequests;
	TArray<FGetInstallStateRequestRef> GetInstallStateRequests;

	// Content Requests
	TArray<FContentRequestRef> ContentRequests[EContentRequestBatch::Count];

	// Content Release Requests
	TArray<FContentReleaseRequestRef> ContentReleaseRequests[EContentReleaseRequestBatch::Count];
	
	// Cache Flush Requests
	TArray<FCacheFlushRequestRef> CacheFlushRequests;

	TSharedRef<InstallBundleManagerUtil::FPersistentStatContainer> PersistentStats;

	TArray<TUniquePtr<InstallBundleUtil::FInstallBundleTask>> AsyncMountTasks;

	
	bool bIsCheckingForPatch = false;
	bool bDelayCheckingForContentPatch = false;

#if INSTALL_BUNDLE_ALLOW_ERROR_SIMULATION
	// Error Simulation
	bool bSimulateClientNotLatest = false;
	bool bSimulateContentNotLatest = false;
#endif // INSTALL_BUNDLE_ALLOW_ERROR_SIMULATION

	//Not included in INSTALL_BUNDLE_ALLOW_ERROR_SIMULATION as we want to provide
	//this functionality even on ship builds
	bool bOverrideCommand_SkipPatchCheck = false;

protected:
	// Analytics
	TSharedPtr<IAnalyticsProviderET> AnalyticsProvider;
	TSharedRef<InstallBundleUtil::FContentRequestStatsMap> StatsMap;
};


ENUM_RANGE_BY_COUNT(FDefaultInstallBundleManager::EContentRequestBatch, FDefaultInstallBundleManager::EContentRequestBatch::Count);

DEFAULTINSTALLBUNDLEMANAGER_API const TCHAR* LexToString(FDefaultInstallBundleManager::EContentRequestBatch Val);


ENUM_RANGE_BY_COUNT(FDefaultInstallBundleManager::EContentReleaseRequestBatch, FDefaultInstallBundleManager::EContentReleaseRequestBatch::Count);

DEFAULTINSTALLBUNDLEMANAGER_API const TCHAR* LexToString(FDefaultInstallBundleManager::EContentReleaseRequestBatch Val);
