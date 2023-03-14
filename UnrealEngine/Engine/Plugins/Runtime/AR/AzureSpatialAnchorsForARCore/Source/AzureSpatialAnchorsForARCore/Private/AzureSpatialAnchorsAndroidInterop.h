// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAzureSpatialAnchors.h"
#include "ARPin.h"
#include "ARSystem.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#endif

#include "AzureSpatialAnchorsNDK.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogAzureSpatialAnchorsAndroidInterop, Log, All);

struct FAzureSpatialAnchorsAndroidInterop
{
public:
	typedef std::function<void(int32 WatcherIdentifier, int32 LocateAnchorStatus, IAzureSpatialAnchors::CloudAnchorID CloudAnchorID)> AnchorLocatedCallbackPtr;
	typedef std::function<void(int32 InWatcherIdentifier, bool InWasCanceled)> LocateAnchorsCompletedCallbackPtr;
	typedef std::function<void(float InReadyForCreateProgress, float InRecommendedForCreateProgress, int InSessionCreateHash, int InSessionLocateHash, int32 InSessionUserFeedback)> SessionUpdatedCallbackPtr;

	static TSharedPtr<FAzureSpatialAnchorsAndroidInterop, ESPMode::ThreadSafe> Create(
		AnchorLocatedCallbackPtr AnchorLocatedCallback,
		LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback,
		SessionUpdatedCallbackPtr SessionUpdatedCallback);
    ~FAzureSpatialAnchorsAndroidInterop();
	
	bool CreateSession();
	void DestroySession();
	
	void GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, IAzureSpatialAnchors::Callback_Result_String Callback);
	void GetAccessTokenWithAuthenticationTokenAsync(const FString& AuthenticationToken, IAzureSpatialAnchors::Callback_Result_String Callback);
	EAzureSpatialAnchorsResult StartSession();
	void UpdateSession();
	void StopSession();
	EAzureSpatialAnchorsResult ResetSession();
	void DisposeSession();
	void GetSessionStatusAsync(IAzureSpatialAnchors::Callback_Result_SessionStatus Callback);
	EAzureSpatialAnchorsResult ConstructAnchor(UARPin* InARPin, IAzureSpatialAnchors::CloudAnchorID& OutCloudAnchorID);
	void CreateAnchorAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback);
	void DeleteAnchorAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback);
	EAzureSpatialAnchorsResult CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, IAzureSpatialAnchors::WatcherID& OutWatcherID, FString& OutErrorString);
	EAzureSpatialAnchorsResult GetActiveWatchers(TArray<IAzureSpatialAnchors::WatcherID>& OutWatcherIDs);
	void GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, IAzureSpatialAnchors::Callback_Result_CloudAnchorID Callback);
	void RefreshAnchorPropertiesAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback);
	void UpdateAnchorPropertiesAsync(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, IAzureSpatialAnchors::Callback_Result Callback);
	EAzureSpatialAnchorsResult GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig);
	EAzureSpatialAnchorsResult SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig);
	EAzureSpatialAnchorsResult SetLocationProvider(const FCoarseLocalizationSettings& InConfig);
	EAzureSpatialAnchorsResult GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity);
	EAzureSpatialAnchorsResult SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity);
	EAzureSpatialAnchorsResult GetSessionId(FString& OutSessionID);

	EAzureSpatialAnchorsResult StopWatcher(IAzureSpatialAnchors::WatcherID InWatcherIdentifier);

	EAzureSpatialAnchorsResult GetCloudSpatialAnchorIdentifier(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, FString& OutCloudAnchorIdentifier);
	EAzureSpatialAnchorsResult SetCloudAnchorExpiration(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds);
	EAzureSpatialAnchorsResult GetCloudAnchorExpiration(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds);
	EAzureSpatialAnchorsResult SetCloudAnchorAppProperties(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties);
	EAzureSpatialAnchorsResult GetCloudAnchorAppProperties(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties);

	EAzureSpatialAnchorsResult SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig);
	void CreateDiagnosticsManifestAsync(const FString& Description, IAzureSpatialAnchors::Callback_Result_String Callback);
	void SubmitDiagnosticsManifestAsync(const FString& ManifestPath, IAzureSpatialAnchors::Callback_Result Callback);

	void CreateNamedARPinAroundAnchor(const FString& InLocalAnchorId, UARPin*& OutARPin);
	bool CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin);

private:
	void SetWeakThis(TWeakPtr<FAzureSpatialAnchorsAndroidInterop, ESPMode::ThreadSafe> WeakSelf) { WeakThis = WeakSelf; }
	void SetCallbacks(AnchorLocatedCallbackPtr AnchorLocatedCallback, LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback, SessionUpdatedCallbackPtr SessionUpdatedCallback);
	FAzureSpatialAnchorsAndroidInterop();

	static FString SessionUserFeedbackToString(Microsoft::Azure::SpatialAnchors::SessionUserFeedback userFeedback);
	IAzureSpatialAnchors::CloudAnchorID GetNextID();
	void OnAnchorLocated(const std::shared_ptr<Microsoft::Azure::SpatialAnchors::AnchorLocatedEventArgs>& Args);
	void ClearAnchorData(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID);

	TMap<IAzureSpatialAnchors::CloudAnchorID, UAzureCloudSpatialAnchor*> LocalAnchorMap;
	TMap<IAzureSpatialAnchors::CloudAnchorID, std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>> CloudAnchorMap;
	FCriticalSection CloudAnchorMapMutex;
	const int32_t InvalidWatcherIdentifier = -1;
	std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher> CurrentWatcher;

    float CreateSufficiency = 0;
	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARSystem;
    std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSession> Session;
	bool bSessionRunning = false;
	bool bLocationProviderStarted = false;
    Microsoft::Azure::SpatialAnchors::event_token SessionUpdatedToken;
    Microsoft::Azure::SpatialAnchors::event_token ErrorToken;
	Microsoft::Azure::SpatialAnchors::event_token AnchorLocatedToken;
	Microsoft::Azure::SpatialAnchors::event_token LocateAnchorsCompletedToken;
    Microsoft::Azure::SpatialAnchors::event_token OnLogDebugToken;

	TWeakPtr<FAzureSpatialAnchorsAndroidInterop, ESPMode::ThreadSafe> WeakThis;

	FCriticalSection CurrentIDMutex;
	IAzureSpatialAnchors::CloudAnchorID CurrentID = 0;

	AnchorLocatedCallbackPtr AnchorLocatedCallback;
	LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback;
	SessionUpdatedCallbackPtr SessionUpdatedCallback;
};
