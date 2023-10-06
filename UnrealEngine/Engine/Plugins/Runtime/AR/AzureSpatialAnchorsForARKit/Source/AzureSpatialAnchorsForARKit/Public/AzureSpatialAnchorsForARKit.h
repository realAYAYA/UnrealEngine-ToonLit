// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AzureSpatialAnchorsBase.h"
#include "Engine/EngineBaseTypes.h"

struct FAzureSpatialAnchorsARKitInterop;

class FAzureSpatialAnchorsForARKit : public FAzureSpatialAnchorsBase, public IModuleInterface
{
public:
	/* IModuleInterface */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/* IAzureSpatialAnchors */
	virtual bool CreateSession() override;
	virtual void DestroySession() override;

	virtual void GetAccessTokenWithAccountKeyAsync(const FString& AccountKey, Callback_Result_String Callback) override;
	virtual void GetAccessTokenWithAuthenticationTokenAsync(const FString& AuthenticationToken, Callback_Result_String Callback) override;
	virtual EAzureSpatialAnchorsResult StartSession() override;
	virtual void StopSession() override;
	virtual EAzureSpatialAnchorsResult ResetSession() override;
	virtual void DisposeSession() override;
	virtual void GetSessionStatusAsync(Callback_Result_SessionStatus Callback) override;
	virtual EAzureSpatialAnchorsResult ConstructAnchor(UARPin* InARPin, CloudAnchorID& OutCloudAnchorID) override;
	virtual void CreateAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) override;  // note this 'creates' the anchor in the azure cloud, aka saves it to the cloud.
	virtual void DeleteAnchorAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) override;
	virtual EAzureSpatialAnchorsResult CreateWatcher(const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, float InWorldToMetersScale, WatcherID& OutWatcherID, FString& OutErrorString) override;
	virtual EAzureSpatialAnchorsResult GetActiveWatchers(TArray<WatcherID>& OutWatcherIDs) override;
	virtual void GetAnchorPropertiesAsync(const FString& InCloudAnchorIdentifier, Callback_Result_CloudAnchorID Callback) override;
	virtual void RefreshAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) override;
	virtual void UpdateAnchorPropertiesAsync(CloudAnchorID InCloudAnchorID, Callback_Result Callback) override;
	virtual EAzureSpatialAnchorsResult GetConfiguration(FAzureSpatialAnchorsSessionConfiguration& OutConfig) override;
	virtual EAzureSpatialAnchorsResult SetConfiguration(const FAzureSpatialAnchorsSessionConfiguration& InConfig) override;
	virtual EAzureSpatialAnchorsResult SetLocationProvider(const FCoarseLocalizationSettings& InConfig) override;
	virtual EAzureSpatialAnchorsResult GetLogLevel(EAzureSpatialAnchorsLogVerbosity& OutLogVerbosity) override;
	virtual EAzureSpatialAnchorsResult SetLogLevel(EAzureSpatialAnchorsLogVerbosity InLogVerbosity) override;
	virtual EAzureSpatialAnchorsResult GetSessionId(FString& OutSessionID) override;

	virtual EAzureSpatialAnchorsResult StopWatcher(WatcherID InWatcherIdentifier) override;
		
	virtual EAzureSpatialAnchorsResult GetCloudSpatialAnchorIdentifier(CloudAnchorID InCloudAnchorID, FString& OutCloudAnchorIdentifier) override;
	virtual EAzureSpatialAnchorsResult SetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float InLifetimeInSeconds) override;
	virtual EAzureSpatialAnchorsResult GetCloudAnchorExpiration(CloudAnchorID InCloudAnchorID, float& OutLifetimeInSeconds) override;
	virtual EAzureSpatialAnchorsResult SetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, const TMap<FString, FString>& InAppProperties) override;
	virtual EAzureSpatialAnchorsResult GetCloudAnchorAppProperties(CloudAnchorID InCloudAnchorID, TMap<FString, FString>& OutAppProperties) override;

	virtual EAzureSpatialAnchorsResult SetDiagnosticsConfig(FAzureSpatialAnchorsDiagnosticsConfig& InConfig) override;
	virtual void CreateDiagnosticsManifestAsync(const FString& Description, Callback_Result_String Callback) override;
	virtual void SubmitDiagnosticsManifestAsync(const FString& ManifestPath, Callback_Result Callback) override;

	virtual void CreateNamedARPinAroundAnchor(const FString& InLocalAnchorId, UARPin*& OutARPin) override;
	virtual bool CreateARPinAroundAzureCloudSpatialAnchor(const FString& PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin) override;

private:
	void OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime);
	void OnASAAnchorLocated(int32 WatcherIdentifier, EAzureSpatialAnchorsLocateAnchorStatus Status, UAzureCloudSpatialAnchor* Anchor);
	void OnLocateAnchorsCompleted(int32 WatcherIdentifier, bool Cancelled);
	void OnASASessionUpdated(float ReadyForCreateProgress, float RecommendedForCreateProgress, int SessionCreateHash, int SessionLocateHash, EAzureSpatialAnchorsSessionUserFeedback Feedback);

	TSharedPtr<FAzureSpatialAnchorsARKitInterop, ESPMode::ThreadSafe> ARKitInterop;
};
