// Copyright Epic Games, Inc. All Rights Reserved.

/*
IAzureSpatialAnchors.h provides the C++ interface for using AzureSpatialAnchors (ASA) in Unreal.

Most of its functions are simple wrappers for ASA api's from Microsoft.
A few relate to UAzureCloudSpatialAnchor objects.
A few communicate with the platform specific local anchor api's, where that is necessary.

It is extended by AzureSpatialAnchorsBase, which owns the UAzureCloudSpatialAnchor objects which are UE's representation of the ASA CloudAnchor object.
That is then extended by AzureSpatialAnchorsFor<Platform> classes in their own plugins (so one can choose to enable the feature per platform).  
The ForPlatform classes implement the api wrapper functions and do whatever type conversions and async translation is required.
AzureSpatialAnchorsForARKit is an example.
Other implementations may be somewhat simpler without this additional layer.

AzureSpatialAnchorsFunctionLibrary provides the blueprint api for AzureSpatialAnchors.
It is also an example of how to use the C++ api.

UAzureSpatialAnchorsLibrary::StartSession() is a super simple case.  It just calls through to one of the non-async ForPlatform api functions.

UAzureSpatialAnchorsLibrary::DeleteCloudAnchor is a latent blueprint function wrapping a simple async api function.  Most of the logic is actually in FAzureSpatialAnchorsDeleteCloudAnchorAction.

UAzureSpatialAnchorsLibrary::LoadCloudAnchor / FAzureSpatialAnchorsLoadCloudAnchorAction is the most complicated.  It creates a watcher then registers for the delegates so it can handle AnchorLocated and AnchorLocateCompleted events.
*/

#pragma once

#include "CoreMinimal.h"

#include "AzureSpatialAnchorsTypes.h"
#include "AzureSpatialAnchors.h"
#include "AzureCloudSpatialAnchor.h"

#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Async/TaskGraphInterfaces.h"

class AZURESPATIALANCHORS_API IAzureSpatialAnchors : public IModularFeature
{
public:
	virtual ~IAzureSpatialAnchors() {}

	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("AzureSpatialAnchors"));
		return FeatureName;
	}

	static inline bool IsAvailable()
	{
		return IModularFeatures::Get().IsModularFeatureAvailable(GetModularFeatureName());
	}

	static inline IAzureSpatialAnchors* Get()
	{
		TArray<IAzureSpatialAnchors*> Impls = IModularFeatures::Get().GetModularFeatureImplementations<IAzureSpatialAnchors>(GetModularFeatureName());

		// There can be only one!  Or zero.  The implementations are platform specific and we are not currently supporting 'overlapping' platforms.
		check(Impls.Num() <= 1);

		if (Impls.Num() > 0)
		{
			check(Impls[0]);
			return Impls[0];
		}
		return nullptr;
	}

	static void OnLog(const wchar_t* LogMsg)
	{
		UE_LOG(LogAzureSpatialAnchors, Log, TEXT("%s"), LogMsg);
	}

public:
	typedef void(*LogFunctionPtr)(const wchar_t* LogMsg);
	typedef int32_t CloudAnchorID;
	static const CloudAnchorID CloudAnchorID_Invalid = -1;
	typedef int32_t WatcherID;
	static const WatcherID WatcherID_Invalid =-1;

	// These functions alias the AzureSpatialAnchors API.  They must be implemented for each platform.
	virtual bool CreateSession() = 0;
	virtual void DestroySession() = 0;

	typedef TFunction<void(EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString)> Callback_Result;
	typedef TFunction<void(EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString, FAzureSpatialAnchorsSessionStatus SessionStatus)> Callback_Result_SessionStatus;
	typedef TFunction<void(EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString, CloudAnchorID InCloudAnchorID)> Callback_Result_CloudAnchorID;
	typedef TFunction<void(EAzureSpatialAnchorsResult Result, const wchar_t* ErrorString, const wchar_t* InString)> Callback_Result_String;
	virtual void GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, Callback_Result_String Callback) = 0;
	virtual void GetAccessTokenWithAuthenticationTokenAsync(const FString& AccountKey, Callback_Result_String Callback) = 0;
	virtual EAzureSpatialAnchorsResult StartSession() = 0;
	virtual void StopSession() = 0;
	virtual EAzureSpatialAnchorsResult ResetSession() = 0;
	virtual void DisposeSession() = 0;
	virtual void GetSessionStatusAsync(Callback_Result_SessionStatus Callback) = 0;
	virtual EAzureSpatialAnchorsResult ConstructAnchor(UARPin* InARPin, CloudAnchorID& OutCloudAnchorID) = 0; // Construct a local cloud anchor object in the microsoft api (does not store it to azure).  This version does not construct a UAzureCloudSpatialAnchor for you.  ConstructCloudAnchor does do that, maybe use it instead?
	virtual void CreateAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) = 0;  // 'creates' the anchor in the azure cloud, aka saves it to the cloud.
	virtual void DeleteAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) = 0;  // 'deletes' the anchor in the azure cloud.
	virtual EAzureSpatialAnchorsResult CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, WatcherID& OutWatcherID, FString& OutErrorString) = 0;
	virtual EAzureSpatialAnchorsResult GetActiveWatchers(TArray<WatcherID>& OutWatcherIDs) = 0;
	virtual void GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, Callback_Result_CloudAnchorID Callback) = 0;  // Get a cloud anchor, even if it's not localized yet, so we can look at it's properties.
	virtual void RefreshAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) = 0; // Get properties from the cloud.
	virtual void UpdateAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) = 0; // Send properties to the cloud.  If we do not have the latest it will fail and we need to refresh.
	virtual EAzureSpatialAnchorsResult GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig) = 0;
	virtual EAzureSpatialAnchorsResult SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig) = 0;
	virtual EAzureSpatialAnchorsResult SetLocationProvider(const FCoarseLocalizationSettings& InConfig) = 0;
	virtual EAzureSpatialAnchorsResult GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity) = 0;
	virtual EAzureSpatialAnchorsResult SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity) = 0;
	//virtual EAzureSpatialAnchorsResult GetSession() = 0;
	//virtual EAzureSpatialAnchorsResult SetSession() = 0;
	virtual EAzureSpatialAnchorsResult GetSessionId(FString& OutSessionID) = 0;

	virtual EAzureSpatialAnchorsResult StopWatcher(WatcherID WatcherID) = 0;

	virtual EAzureSpatialAnchorsResult GetCloudSpatialAnchorIdentifier(CloudAnchorID InCloudAnchorID, FString& OutCloudAnchorIdentifier) = 0;
	virtual EAzureSpatialAnchorsResult SetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds) = 0;
	virtual EAzureSpatialAnchorsResult GetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds) = 0;
	virtual EAzureSpatialAnchorsResult SetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties) = 0;
	virtual EAzureSpatialAnchorsResult GetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties) = 0;

	virtual EAzureSpatialAnchorsResult SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig) = 0;
	virtual void CreateDiagnosticsManifestAsync(const FString& Description, Callback_Result_String Callback) = 0;
	virtual void SubmitDiagnosticsManifestAsync(const FString& ManifestPath, Callback_Result Callback) = 0;


	// Functions that must be implemented per-platform
	virtual void CreateNamedARPinAroundAnchor(const FString& LocalAnchorId, UARPin*& OutARPin) = 0;
	virtual bool CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin) = 0;

	// These functions provide basic housekeeping for AzureSpatialAnchors.  The AzureSpatialAnchorsBase layer implements them.
	virtual const FAzureSpatialAnchorsSessionStatus& GetSessionStatus() = 0;
	virtual bool GetCloudAnchor(class UARPin*& InARPin, class UAzureCloudSpatialAnchor*& OutCloudAnchor) = 0;
	virtual void GetCloudAnchors(TArray<class UAzureCloudSpatialAnchor*>& OutCloudAnchors) = 0;
	virtual bool ConstructCloudAnchor(class UARPin*& InARPin, class UAzureCloudSpatialAnchor*& OutCloudAnchor) = 0;
	virtual UAzureCloudSpatialAnchor* GetOrConstructCloudAnchor(CloudAnchorID CloudAnchorID) = 0;

	/** Delegates that will be cast by the ASA platform implementations. */
	/** These delegates should only be fired from the game thread */
	// WatcherIdentifier, status, anchor
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FASAAnchorLocatedDelegate, int32, EAzureSpatialAnchorsLocateAnchorStatus, UAzureCloudSpatialAnchor*);
	static FASAAnchorLocatedDelegate ASAAnchorLocatedDelegate;
	// WatcherIdentifier, canceled
	DECLARE_MULTICAST_DELEGATE_TwoParams(FASALocateAnchorsCompletedDelegate, int32, bool);
	static FASALocateAnchorsCompletedDelegate ASALocateAnchorsCompletedDelegate;
	// ReadyForCreateProgress, RecommendedForCreateProgress, SessionCreateHash, SessionLocateHash, feedback
	DECLARE_MULTICAST_DELEGATE_FiveParams(FASASessionUpdatedDelegate, float, float, int, int, EAzureSpatialAnchorsSessionUserFeedback);
	static FASASessionUpdatedDelegate ASASessionUpdatedDelegate;

	// If an implementation generates events from a thread other than the game thread it should launch these tasks that will fire the delegates on the game
	// thread

protected:

	class FASAAnchorLocatedTask
	{
	public:
		FASAAnchorLocatedTask(int32 InWatcherIdentifier, EAzureSpatialAnchorsLocateAnchorStatus InStatus, CloudAnchorID InCloudAnchorID, class IAzureSpatialAnchors& InIASA)
			: WatcherIdentifier(InWatcherIdentifier), Status(InStatus), CloudAnchorID(InCloudAnchorID), IASA(InIASA)
		{
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			UAzureCloudSpatialAnchor* CloudSpatialAnchor = IASA.GetOrConstructCloudAnchor(CloudAnchorID);
			IAzureSpatialAnchors::ASAAnchorLocatedDelegate.Broadcast(WatcherIdentifier, Status, CloudSpatialAnchor);
		}

		static FORCEINLINE TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(ASAAnchorLocatedTask, STATGROUP_TaskGraphTasks);
		}

		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::GameThread;
		}

		static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::FireAndForget;
		}

	private:
		const int32 WatcherIdentifier;
		const EAzureSpatialAnchorsLocateAnchorStatus Status;
		const CloudAnchorID CloudAnchorID;
		IAzureSpatialAnchors& IASA;
	};

	class FASALocateAnchorsCompletedTask
	{
	public:
		FASALocateAnchorsCompletedTask(int32 InWatcherIdentifier, bool InWasCanceled)
			: WatcherIdentifier(InWatcherIdentifier), WasCanceled(InWasCanceled)
		{
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			IAzureSpatialAnchors::ASALocateAnchorsCompletedDelegate.Broadcast(WatcherIdentifier, WasCanceled);
		}

		static FORCEINLINE TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(ASALocateAnchorsCompletedTask, STATGROUP_TaskGraphTasks);
		}

		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::GameThread;
		}

		static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::FireAndForget;
		}

	private:
		const int32 WatcherIdentifier;
		const bool WasCanceled;
	};

	class FASASessionUpdatedTask
	{
	public:
		FASASessionUpdatedTask(float InReadyForCreateProgress, float InRecommendedForCreateProgress, int InSessionCreateHash, int InSessionLocateHash, EAzureSpatialAnchorsSessionUserFeedback InFeedback)
			: ReadyForCreateProgress(InReadyForCreateProgress), RecommendedForCreateProgress(InRecommendedForCreateProgress), SessionCreateHash(InSessionCreateHash), SessionLocateHash(InSessionLocateHash), Feedback(InFeedback)
		{
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			checkf(CurrentThread == ENamedThreads::GameThread, TEXT("This task can only safely be run on the game thread"));
			IAzureSpatialAnchors::ASASessionUpdatedDelegate.Broadcast(ReadyForCreateProgress, RecommendedForCreateProgress, SessionCreateHash, SessionLocateHash, Feedback);
		}

		static FORCEINLINE TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(ASASessionUpdatedTask, STATGROUP_TaskGraphTasks);
		}

		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::GameThread;
		}

		static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::FireAndForget;
		}

	private:
		const float ReadyForCreateProgress;
		const float RecommendedForCreateProgress;
		const int SessionCreateHash;
		const int SessionLocateHash;
		const EAzureSpatialAnchorsSessionUserFeedback Feedback;
	};
};
