// Copyright Epic Games, Inc. All Rights Reserved.

// This base class implements two things;

// It owns all UAzureCloudSpatialAnchor objects, each of which represents the microsoft api side object for a cloud anchor.

#pragma once

#include "IAzureSpatialAnchors.h"
#include "UObject/GCObject.h"

class AZURESPATIALANCHORS_API FAzureSpatialAnchorsBase : public IAzureSpatialAnchors, public FGCObject
{
public:
	void Startup();
	void Shutdown();

	virtual void DestroySession() override;

	virtual const FAzureSpatialAnchorsSessionStatus& GetSessionStatus() override;
	virtual bool GetCloudAnchor(class UARPin*& InARPin, class UAzureCloudSpatialAnchor*& OutCloudAnchor) override;
	virtual void GetCloudAnchors(TArray<class UAzureCloudSpatialAnchor*>& OutCloudAnchors) override;
	virtual bool ConstructCloudAnchor(class UARPin*& InARPin, class UAzureCloudSpatialAnchor*& OutCloudAnchor) override;
	virtual UAzureCloudSpatialAnchor* GetOrConstructCloudAnchor(CloudAnchorID CloudAnchorID) override;

protected:
	UAzureCloudSpatialAnchor* GetCloudAnchor(CloudAnchorID CloudAnchorID) const;
	TArray<UAzureCloudSpatialAnchor*> CloudAnchors;
	FAzureSpatialAnchorsSessionStatus CachedSessionStatus;
	FDelegateHandle CacheSessionHandle;

	// FGCObject Implementation
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (auto anchor : CloudAnchors)
		{
			Collector.AddReferencedObject(anchor);
		}
	}
	virtual FString GetReferencerName() const override
	{
		return "FAzureSpatialAnchorsBase";
	}

	void AnchorLocatedCallback(int32 WatcherIdentifier, int32 LocateAnchorStatus, IAzureSpatialAnchors::CloudAnchorID CloudAnchorID);
	void LocateAnchorsCompletedCallback(int32 WatcherIdentifier, bool WasCanceled);
	void SessionUpdatedCallback(float ReadyForCreateProgress, float RecommendedForCreateProgress, int SessionCreateHash, int SessionLocateHash, int32 SessionUserFeedback);
	void CacheSessionStatus(float, float, int, int, EAzureSpatialAnchorsSessionUserFeedback);
};