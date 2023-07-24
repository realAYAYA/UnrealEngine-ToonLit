// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IAzureSpatialAnchors.h"
#include "ARPin.h"
#include "ARSystem.h"
#include "ARBlueprintLibrary.h"
#include <functional>

#include "AzureSpatialAnchors/AzureSpatialAnchors.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAzureSpatialAnchorsARKitInterop, Log, All);

@class AzureSpatialAnchorsSessionCallbackDelegate;

struct FAzureSpatialAnchorsARKitInterop
{
public:
	typedef std::function<void(int32 WatcherIdentifier, int32 LocateAnchorStatus, IAzureSpatialAnchors::CloudAnchorID CloudAnchorID)> AnchorLocatedCallbackPtr;
	typedef std::function<void(int32 InWatcherIdentifier, bool InWasCanceled)> LocateAnchorsCompletedCallbackPtr;
	typedef std::function<void(float InReadyForCreateProgress, float InRecommendedForCreateProgress, int InSessionCreateHash, int InSessionLocateHash, int32 InSessionUserFeedback)> SessionUpdatedCallbackPtr;

	static TSharedPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> Create(
		AnchorLocatedCallbackPtr AnchorLocatedCallback,
		LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback,
		SessionUpdatedCallbackPtr SessionUpdatedCallback);
    ~FAzureSpatialAnchorsARKitInterop();
	
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
	void SetWeakThis(TWeakPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> WeakSelf) { WeakThis = WeakSelf; }
	void SetCallbacks(AnchorLocatedCallbackPtr AnchorLocatedCallback, LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback, SessionUpdatedCallbackPtr SessionUpdatedCallback);
	FAzureSpatialAnchorsARKitInterop();
	IAzureSpatialAnchors::CloudAnchorID GetNextID();
	void ClearAnchorData(IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID);
    void OnAnchorLocated(ASACloudSpatialAnchorSession* session, ASAAnchorLocatedEventArgs* args);
    void OnLocateAnchorsCompleted(ASACloudSpatialAnchorSession* session, ASALocateAnchorsCompletedEventArgs* args);
    void OnSessionUpdated(ASACloudSpatialAnchorSession* session, ASASessionUpdatedEventArgs* args);
    void OnError(ASACloudSpatialAnchorSession* session, ASASessionErrorEventArgs* args);
    void OnLogDebugEvent(ASACloudSpatialAnchorSession* session, ASAOnLogDebugEventArgs* args);
    NSString* FStringToNSString(const FString& fstring);
    FString NSStringToFString(const NSString* nsstring);
    
    ASACloudSpatialAnchorSession* Session;
    AzureSpatialAnchorsSessionCallbackDelegate* CallbackHelper;
	TMap<IAzureSpatialAnchors::CloudAnchorID, UAzureCloudSpatialAnchor*> LocalAnchorMap;
    TMap<IAzureSpatialAnchors::CloudAnchorID, ASACloudSpatialAnchor*> CloudAnchorMap;
	FCriticalSection CloudAnchorMapMutex;
    TSet<IAzureSpatialAnchors::CloudAnchorID> SavedAnchors;
    FCriticalSection SavedAnchorsMutex;
	const int32_t InvalidWatcherIdentifier = -1;
    ASACloudSpatialAnchorWatcher* CurrentWatcher;

	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARSystem;
	bool bSessionRunning = false;
	bool bLocationProviderStarted = false;

	TWeakPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> WeakThis;

	FCriticalSection CurrentIDMutex;
	IAzureSpatialAnchors::CloudAnchorID CurrentID = 0;

	AnchorLocatedCallbackPtr AnchorLocatedCallback;
	LocateAnchorsCompletedCallbackPtr LocateAnchorsCompletedCallback;
	SessionUpdatedCallbackPtr SessionUpdatedCallback;
};
