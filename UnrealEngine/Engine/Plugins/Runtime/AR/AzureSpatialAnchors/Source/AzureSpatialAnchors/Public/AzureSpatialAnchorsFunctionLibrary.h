// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARPin.h"
#include "AzureSpatialAnchorsTypes.h"
#include "AzureCloudSpatialAnchor.h"

#include "AzureSpatialAnchorsFunctionLibrary.generated.h"


/** A function library that provides static/Blueprint functions for AzureSpatialAnchors.*/
UCLASS()
class AZURESPATIALANCHORS_API UAzureSpatialAnchorsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	///**
	// * Create an ASA session.  
	// * It is not yet active.
	// *
	// * @return (Boolean)  True if a session has been created (even if it already existed).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool CreateSession();

	///**
	// * Configure the ASA session.  
	// * This will take effect when the next session is started.
	// * This version is deprecated.  Please use ConfigSession2 instead.
	// * 
	// * @param AccountId      The Azure Spatial Anchor Account ID.
	// * @param AccountKey		The Azure Spatial Anchor Account Key.
	// * @param CoarseLocalizationSettings	Settings related to locating the device in the world (eg GPS).
	// * @param LogVerbosity	Logging verbosity for the Azure Spatial Anchor api.
	// *
	// * @return (Boolean)  True if the session configuration was set.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (DeprecatedFunction, DeprecationMessage = "ConfigSession is deprecated, use ConfigSession2 instead."))
	static bool ConfigSession(const FString& AccountId, const FString& AccountKey, const FCoarseLocalizationSettings CoarseLocalizationSettings, EAzureSpatialAnchorsLogVerbosity LogVerbosity);

	///**
	// * Configure the ASA session.  
	// * This will take effect when the next session is started.
	// * 
	// * @param SessionConfiguration		Azure cloud sign in related configuration.
	// * @param CoarseLocalizationSettings	Settings related to locating the device in the world (eg GPS).
	// * @param LogVerbosity				Logging verbosity for the Azure Spatial Anchor api.
	// *
	// * @return (Boolean)  True if the session configuration was set.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool ConfigSession2(const FAzureSpatialAnchorsSessionConfiguration& SessionConfiguration, const FCoarseLocalizationSettings CoarseLocalizationSettings, EAzureSpatialAnchorsLogVerbosity LogVerbosity);

	///**
	// * Start a Session running.  
	// * ASA will start collecting tracking data.
	// *
	// * @return (Boolean)  True if a session has been started (even if it was already started).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool StartSession();

	///**
	// * The session will stop, it can be started again.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool StopSession();

	///**
	// * The session will be destroyed.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool DestroySession();

	///**
	// * Get the azure spatial anchors session status struct.
	// *
	// * @param OutStatus	The retrieved status struct.
	// * @return (Boolean&)  True if is an AzureSpatialAnchors plugin running.  False probably means that this platform does not support ASA or the plugin for this platform is not enabled.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool GetCachedSessionStatus(FAzureSpatialAnchorsSessionStatus& OutStatus);

	///**
	// * Get the AzureSpatialAnchors Session's Status.
	// * This will start a Latent Action to get the Session Status.
	// *
	// * @param OutStatus	The retrieved status struct.
	// * @param OutResult	Result of the Save attempt.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void GetSessionStatus(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FAzureSpatialAnchorsSessionStatus& OutStatus, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Get the cloud anchor associated with a particular ARPin.
	// *
	// * @param ARPin      The ARPin who's cloud anchor we hope to get.
	// * @param OutAzureCloudSpatialAnchor	The cloud spatial anchor, or null.
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void GetCloudAnchor(UARPin* ARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor);

	///**
	// * Get list of all CloudAnchors.
	// *
	// * @param OutCloudAnchors 	The cloud spatial anchors
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))	
	static void GetCloudAnchors(TArray<UAzureCloudSpatialAnchor*>& OutCloudAnchors);

	///**
	// * Save the pin to the cloud.
	// * This will start a Latent Action to save the ARPin to the Azure Spatial Anchors cloud service.
	// *	
	// * @param ARPin						The ARPin to save.
	// * @param Lifetime					The lifetime time of the cloud pin in the cloud in seconds.  <= 0 means no expiration.  I would not expect expiration to be accurate to the second.
	// * @param OutAzureCloudSpatialAnchor  The Cloud anchor handle.
	// * @param OutResult					Result of the Save attempt.
	// * @param OutErrorString				Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void SavePinToCloud(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UARPin* ARPin, float Lifetime, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);
	
	///**
	// * Save the pin to the cloud.
	// * This will start a Latent Action to save the ARPin to the Azure Spatial Anchors cloud service.
	// *	
	// * @param ARPin						The ARPin to save.
	// * @param Lifetime					The lifetime time of the cloud pin in the cloud in seconds.  <= 0 means no expiration.  I would not expect expiration to be accurate to the second.
	// * @param InAppProperties				Key-Value pairs of strings that will be stored to the cloud with the anchor.  Use them to attach app-specific information to an anchor.
	// * @param OutAzureCloudSpatialAnchor  The Cloud anchor handle.
	// * @param OutResult					Result of the Save attempt.
	// * @param OutErrorString				Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void SavePinToCloudWithAppProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UARPin* ARPin, float Lifetime, const TMap<FString, FString>& InAppProperties, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Delete the cloud anchor in the cloud.
	// * This will start a Latent Action to delete the cloud anchor from the cloud service.
	// *
	// * @param CloudSpatialAnchor      The Cloud anchor to delete.
	// * @param OutResult	Result of the Delete attempt.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void DeleteCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* CloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Load a pin from the cloud..
	// * This will start a Latent Action to load a cloud anchor and create a pin for it.
	// *
	// * @param CloudIdentifier				The Azure Cloud Spatial Anchor Identifier of the cloud anchor we will try to load.
	// * @param PinId						Specify the name of the Pin to load into.  Will fail if a pin of this name already exists.  If empty an auto-generated id will be used.
	// * @param OutARPin					Filled in with the pin created, if successful.
	// * @param OutAzureCloudSpatialAnchor	Filled in with the UE representation of the cloud spatial anchor created, if successful.
	// * @param OutResult					The Result enumeration.
	// * @param OutErrorString				Additional informatiuon about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AR|AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void LoadCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FString CloudIdentifier, FString PinId, UARPin*& OutARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);


	// Advanced CloudAnchor api.  More flexibility, more steps to go through, more ways to go wrong.

	///**
	// * Construct a cloud anchor for the pin.  This is not yet stored in the cloud.
	// *
	// * @param ARPin      The ARPin to create an anchor for.
	// * @param OutAzureCloudSpatialAnchor  The Cloud anchor handle. (null if fails)
	// * @param OutResult					The Result enumeration.
	// * @param OutErrorString				Additional informatiuon about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void ConstructCloudAnchor(UARPin* ARPin, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Save the cloud anchor to the cloud.
	// * This will start a Latent Action to save the AzureCloudSpatialAnchor to the Azure Spatial Anchors cloud service.
	// *
	// * @param InAzureCloudSpatialAnchor      The AzureCloudSpatialAnchor to save.
	// * @param OutResult	The Result enumeration.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void SaveCloudAnchor(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Save the cloud anchor's properties to the cloud.
	// * This will start a Latent Action to save the AzureCloudSpatialAnchor properties to the Azure Spatial Anchors cloud service.
	// * This can fail if another client updates the anchor.  If that happens you will have to call RefreshCloudAnchorProperties to get the updated values before you might UpdateCloudAnchorProperties sucessfully.
	// *
	// * @param InAzureCloudSpatialAnchor      The AzureCloudSpatialAnchor to update.
	// * @param OutResult	The Result enumeration.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void UpdateCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	///**
	// * Get the latest cloud anchor properties from the cloud.
	// * This will start a Latent Action to fetch the AzureCloudSpatialAnchor's propertiesfrom the Azure Spatial Anchors cloud service.
	// *
	// * @param InAzureCloudSpatialAnchor      The AzureCloudSpatialAnchor to refresh.
	// * @param OutResult	The Result enumeration.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void RefreshCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);

	/////**
	//// * Get the AzureCloudSpatialAnchor with properties by ID, even if it is not yet located.
	//// * This will start a Latent Action to fetch the AzureCloudSpatialAnchor's properties from the Azure Spatial Anchors cloud service even if it has not been located yet.
	//// *
	//// * @param CloudIdentifier      The cloud identifier who's anchor we are trying to get.
	//// * @param InAzureCloudSpatialAnchor      The AzureCloudSpatialAnchor to refresh.
	//// * @param OutResult	The Result enumeration.
	//// * @param OutErrorString	Additional information about the OutResult (often empty).
	//// */
	//UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	//static void GetCloudAnchorProperties(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, FString CloudIdentifier, UAzureCloudSpatialAnchor*& OutAzureCloudSpatialAnchor, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);


	///**
	// * Create and start a 'Watcher' searching for azure cloud spatial anchors as specified in the locate criteria.  Use an AzureSpatialAnchorsEventComponent's events to get
	// * notifications of found anchors and watcher completion.
	// *
	// * @param InLocateCriteria      Structure describing the watcher we wish to start.
	// * @param OutWatcherIdentifier   The ID of the created watcher (can be used to stop the watcher).
	// * @param OutResult	The Result enumeration.
	// * @param OutErrorString	Additional information about the OutResult (often empty).
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (WorldContext = "WorldContextObject", Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static void CreateWatcher(UObject* WorldContextObject, const FAzureSpatialAnchorsLocateCriteria& InLocateCriteria, int32& OutWatcherIdentifier, EAzureSpatialAnchorsResult& OutResult, FString& OutErrorString);


	///**
	// * Stop the specified Watcher looking for anchors, if it still exists.  
	// *
	// * @param InWatcherIdentifier      The identifier of the watcher we are trying to stop.
	// *
	// * @return (Boolean)  True if the watcher existed.  False if it did not.
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool StopWatcher(int32 InWatcherIdentifier);

	///**
	// * Create an ARPin around an already existing cloud anchor. 
	// *
	// * @param PinId      The name of the pin we want created.
	// * @param InAzureCloudSpatialAnchor  The cloud anchor we will create the pin around.
	// * @param OutARPin The pin that was created, or null.
	// *
	// * @return (Boolean)  True if we were able to create.
	// */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	static bool CreateARPinAroundAzureCloudSpatialAnchor(FString PinId, UAzureCloudSpatialAnchor* InAzureCloudSpatialAnchor, UARPin*& OutARPin);
};