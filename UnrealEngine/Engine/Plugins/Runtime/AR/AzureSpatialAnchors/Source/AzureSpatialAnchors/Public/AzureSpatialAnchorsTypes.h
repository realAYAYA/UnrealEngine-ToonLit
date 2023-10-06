// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AzureSpatialAnchorsTypes.generated.h"

AZURESPATIALANCHORS_API DECLARE_LOG_CATEGORY_EXTERN(LogAzureSpatialAnchors, Log, All);

// Note: this must match winrt::Microsoft::Azure::SpatialAnchors::SessionLogLevel
UENUM(BlueprintType, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
enum class EAzureSpatialAnchorsLogVerbosity : uint8
{
	None = 0,
	Error = 1,
	Warning = 2,
	Information = 3,
	Debug = 4,
	All = 5,
};

// Note: this Result enum must match AzureSpatialAnchorsInterop::AsyncResult in MixedRealityInterop.h
UENUM(BlueprintType, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
enum class EAzureSpatialAnchorsResult : uint8
{
	Success,
	NotStarted,
	Started,
	FailAlreadyStarted,
	FailNoARPin,
	FailBadLocalAnchorID,
	FailBadCloudAnchorIdentifier,
	FailAnchorIdAlreadyUsed,
	FailAnchorDoesNotExist,
	FailAnchorAlreadyTracked,
	FailNoAnchor,
	FailNoCloudAnchor,
	FailNoLocalAnchor,
	FailNoSession,
	FailNoWatcher,
	FailNotEnoughData,
	FailBadLifetime,
	FailSeeErrorString,
	NotLocated,
	Canceled,
	FailUnknown
};

// Note: this must match winrt::Microsoft::Azure::SpatialAnchors::AnchorDataCategory
UENUM(BlueprintType, Category = "AzureSpatialAnchors")
enum class EAzureSpatialAnchorDataCategory : uint8
{
	None = 0,		// No data is returned.
	Properties = 1,	// Get only Anchor metadata properties including AppProperties.
	Spatial = 2		// Returns spatial information about an Anchor including a local anchor.
};

// Note: this must match winrt::Microsoft::Azure::SpatialAnchors::LocateStrategy
UENUM(BlueprintType, Category = "AzureSpatialAnchors")
enum class EAzureSpatialAnchorsLocateStrategy : uint8
{
	AnyStrategy = 0,		// Indicates that any method is acceptable.
	VisualInformation = 1,	// Indicates that anchors will be located primarily by visual information.
	Relationship = 2		// Indicates that anchors will be located primarily by relationship to other anchors.
};

// Note: this must match winrt::Microsoft::Azure::SpatialAnchors::LocateAnchorStatus
UENUM(BlueprintType, Category = "AzureSpatialAnchors")
enum class EAzureSpatialAnchorsLocateAnchorStatus : uint8
{
	AlreadyTracked = 0,				// The anchor was already being tracked.
	Located = 1,						// The anchor was found.
	NotLocated = 2,						// The anchor was not found.
	NotLocatedAnchorDoesNotExist = 3	// The anchor cannot be found - it was deleted or the identifier queried for was incorrect.
};

// Note: this must match winrt::Microsoft::Azure::SpatialAnchors::SessionUserFeedback
UENUM(BlueprintType, Category = "AzureSpatialAnchors")
enum class EAzureSpatialAnchorsSessionUserFeedback : uint8
{
	None = 0,					// No specific feedback is available.
	NotEnoughMotion = 1,		// Device is not moving enough to create a neighborhood of key-frames.
	MotionTooQuick = 2,			// Device is moving too quickly for stable tracking.
	// Note: skipped 3  - presumably these values are used as bit flags somewhere?
	NotEnoughFeatures = 4		// The environment doesn't have enough feature points for stable tracking.
};

USTRUCT(BlueprintType, Category = "AzureSpatialAnchors")
struct FAzureSpatialAnchorsSessionConfiguration
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	FString  AccessToken;

	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	FString AccountId;

	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	FString AccountKey;

	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	FString  AccountDomain;

	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	FString  AuthenticationToken;
};

USTRUCT(BlueprintType, Category = "AzureSpatialAnchors")
struct FCoarseLocalizationSettings
{
	GENERATED_BODY()

	/**
	* If true coarse localization will be active
	*/
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	bool bEnable = false;

	/**
	* If true GPS can be used for localization ("location" must also be enabled in Project Settings->Platforms->Hololens->Capabilities)
	*/
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	bool bEnableGPS = false;

	/**
	* If true WiFi  can be used for localization ("wiFiControl" must also be enabled in Project Settings->Platforms->Hololens->Capabilities)
	*/
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	bool bEnableWifi = false;

	/**
	 * List of bluetooth beacon uuids that can be used for localization ("bluetooth" must also be enabled in Project Settings->Platforms->Hololens->Capabilities)
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	TArray<FString> BLEBeaconUUIDs;
};

USTRUCT(BlueprintType, Category = "AzureSpatialAnchors")
struct FAzureSpatialAnchorsLocateCriteria
{
	GENERATED_BODY()

	/**
	 * If true the device local cache of anchors is ignored.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	bool bBypassCache = false;

	/**
	 * List of specific anchor identifiers to locate.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	TArray<FString> Identifiers;

	/**
	 * Specify (optionally) an anchor around which to locate anchors.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearAnchor")
	TObjectPtr<class UAzureCloudSpatialAnchor> NearAnchor = nullptr;

	/**
	 * Specify the distance at which to locate anchors near the NearAnchor, in cm.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearAnchor")
	float NearAnchorDistance = 500.0f;

	/**
	 * Specify the maximum number of anchors around the NearAnchor to locate.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearAnchor")
	int NearAnchorMaxResultCount = 20;

	/**
	 * Specify whether to search near the device location.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearDevice")
	bool bSearchNearDevice = false;

	/**
	 * Specify the distance at which to locate anchors near the device, in cm.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearDevice")
	float NearDeviceDistance = 500.0f;

	/**
	 * Specify the maximum number of anchors around the device to locate.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors | NearDevice")
	int NearDeviceMaxResultCount = 20;

	/**
	 * Specify what data to retrieve.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	EAzureSpatialAnchorDataCategory RequestedCategories = EAzureSpatialAnchorDataCategory::Spatial;

	/**
	 * Specify the method by which anchors will be located.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "AzureSpatialAnchors")
	EAzureSpatialAnchorsLocateStrategy Strategy = EAzureSpatialAnchorsLocateStrategy::AnyStrategy;
};

USTRUCT(BlueprintType, Category = "AzureSpatialAnchors")
struct FAzureSpatialAnchorsSessionStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	float ReadyForCreateProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	float RecommendedForCreateProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	int SessionCreateHash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	int SessionLocateHash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")	
	EAzureSpatialAnchorsSessionUserFeedback feedback = EAzureSpatialAnchorsSessionUserFeedback::None;
};

USTRUCT(BlueprintType, Category = "AzureSpatialAnchors")
struct FAzureSpatialAnchorsDiagnosticsConfig
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	bool bImagesEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	FString LogDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	EAzureSpatialAnchorsLogVerbosity LogLevel = EAzureSpatialAnchorsLogVerbosity::Debug;

	UPROPERTY(BlueprintReadOnly, Category = "AzureSpatialAnchors")
	int MaxDiskSizeInMB = 0;
};