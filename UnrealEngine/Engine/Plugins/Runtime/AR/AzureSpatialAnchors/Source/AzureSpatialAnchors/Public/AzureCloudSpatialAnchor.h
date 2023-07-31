// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARPin.h"

#include "AzureCloudSpatialAnchor.generated.h"


UCLASS(BlueprintType, Experimental, Category="AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
class AZURESPATIALANCHORS_API UAzureCloudSpatialAnchor : public UObject
{
	GENERATED_BODY()
	
public:
	/**
	 * The UE Cloud Anchor ID.
	 * This maps to a CloudSpatialAnchor within the microsoft api.  It is unique per UE app instance.
	 * It is different from the CloudSpatialAnchorIdentifier, which uniquely identifies an anchor in the Azure cloud.
	 * This is not exposed to blueprint, blueprint should use the UAzureCloudSpatialAnchor object as a whole.
	 */
	typedef int AzureCloudAnchorID;
	static const AzureCloudAnchorID AzureCloudAnchorID_Invalid = -1;
	AzureCloudAnchorID CloudAnchorID  = AzureCloudAnchorID_Invalid;

	
	/**
	 * The ARPin with which this cloud anchor is associated, or null.  Null could mean we are still trying to load the anchor or have not yet located it.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	TObjectPtr<UARPin> ARPin;

	/**
	 * The Azure Cloud identifier of the spatial anchor is the persistent identifier by which a cloud anchor can be requested from the azure cloud service.  Empty if the anchor has not been saved or loaded yet.
	 */
	UFUNCTION(BlueprintPure, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	FString GetAzureCloudIdentifier() const;

	/**
	 * The AppProperties dictionary of the cloud anchor.
	 */
	UFUNCTION(BlueprintPure, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	TMap<FString, FString> GetAppProperties() const;
	/**
	 * Set the AppProperties dictionary of the cloud anchor.  You must call SaveCloudAnchor or UpdateCloudAnchorProperties before these will be persisted on azure.
	 */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	void SetAppProperties(const TMap<FString, FString>& InAppProperties);


	/**
	 * The Expiration time of the cloud anchor as seconds into the future.
	 */
	UFUNCTION(BlueprintPure, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	float GetExpiration() const;
	/**
	 * Set the Expiration time of the cloud anchor as seconds into the future.  You must call SaveCloudAnchor or UpdateCloudAnchorProperties before this will be persisted on azure.
	 */
	UFUNCTION(BlueprintCallable, Category = "AzureSpatialAnchors", meta = (Keywords = "azure spatial anchor hololens wmr pin ar all"))
	void SetExpiration(const float Lifetime);
};
