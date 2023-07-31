// Copyright Epic Games, Inc. All Rights Reserved.

#include "AzureSpatialAnchorsBase.h"

void FAzureSpatialAnchorsBase::Startup()
{
	CacheSessionHandle = IAzureSpatialAnchors::ASASessionUpdatedDelegate.AddRaw(this, &FAzureSpatialAnchorsBase::CacheSessionStatus);
}

void FAzureSpatialAnchorsBase::Shutdown()
{
	IAzureSpatialAnchors::ASASessionUpdatedDelegate.Remove(CacheSessionHandle);
}

void FAzureSpatialAnchorsBase::DestroySession()
{
	CloudAnchors.Reset();
}

const FAzureSpatialAnchorsSessionStatus& FAzureSpatialAnchorsBase::GetSessionStatus()
{
	check(IsInGameThread());
	return CachedSessionStatus;
}

bool FAzureSpatialAnchorsBase::GetCloudAnchor(UARPin*& InARPin, UAzureCloudSpatialAnchor*& OutCloudAnchor)
{
	if (!InARPin)
	{
		UE_LOG(LogAzureSpatialAnchors, Warning, TEXT("GetCloudAnchor called with null ARPin.  Ignoring."));
		OutCloudAnchor = nullptr;
		return false;
	}

	for (UAzureCloudSpatialAnchor* Anchor : CloudAnchors)
	{
		if (Anchor->ARPin == InARPin)
		{
			OutCloudAnchor = Anchor;
			return true;
		}
	}

	OutCloudAnchor = nullptr;
	return false;
}

void FAzureSpatialAnchorsBase::GetCloudAnchors(TArray<UAzureCloudSpatialAnchor*>& OutCloudAnchors)
{
	OutCloudAnchors = CloudAnchors;
}

bool FAzureSpatialAnchorsBase::ConstructCloudAnchor(UARPin*& InARPin, UAzureCloudSpatialAnchor*& OutCloudAnchor)
{
	// Don't construct UObjects on other threads.
	check(IsInGameThread());

	CloudAnchorID CloudAnchorID = CloudAnchorID_Invalid;
	bool bSuccess = ConstructAnchor(InARPin, CloudAnchorID) == EAzureSpatialAnchorsResult::Success;

	if (bSuccess)
	{
		check(CloudAnchorID != CloudAnchorID_Invalid);
		UAzureCloudSpatialAnchor* NewCloudAnchor = NewObject<UAzureCloudSpatialAnchor>();
		NewCloudAnchor->ARPin = InARPin;
		NewCloudAnchor->CloudAnchorID = CloudAnchorID;
		CloudAnchors.Add(NewCloudAnchor);
		OutCloudAnchor = NewCloudAnchor;
	}

	return bSuccess;
}

UAzureCloudSpatialAnchor* FAzureSpatialAnchorsBase::GetCloudAnchor(CloudAnchorID CloudAnchorID) const
{
	for (auto& Itr : CloudAnchors)
	{
		if (Itr->CloudAnchorID == CloudAnchorID)
		{
			return Itr;
		}
	}

	return nullptr;
}


UAzureCloudSpatialAnchor* FAzureSpatialAnchorsBase::GetOrConstructCloudAnchor(CloudAnchorID CloudAnchorID)
{
	// Don't construct UObjects on other threads.
	check(IsInGameThread());

	UAzureCloudSpatialAnchor* CloudAnchor = nullptr;
	if (CloudAnchorID != UAzureCloudSpatialAnchor::AzureCloudAnchorID_Invalid)
	{
		CloudAnchor = GetCloudAnchor(CloudAnchorID);
		if (CloudAnchor == nullptr)
		{
			CloudAnchor = NewObject<UAzureCloudSpatialAnchor>();
			CloudAnchor->CloudAnchorID = CloudAnchorID;
			CloudAnchors.Add(CloudAnchor);
		}
	}
	return CloudAnchor;
}

// Called from an ASA worker thread.
void FAzureSpatialAnchorsBase::AnchorLocatedCallback(int32 InWatcherIdentifier, int32 InLocateAnchorStatus, IAzureSpatialAnchors::CloudAnchorID InCloudAnchorID)
{
	const EAzureSpatialAnchorsLocateAnchorStatus Status = static_cast<EAzureSpatialAnchorsLocateAnchorStatus>(InLocateAnchorStatus);
	TGraphTask< FASAAnchorLocatedTask >::CreateTask().ConstructAndDispatchWhenReady(InWatcherIdentifier, Status, InCloudAnchorID, *this);
}

// Called from an ASA worker thread.
void FAzureSpatialAnchorsBase::LocateAnchorsCompletedCallback(int32 InWatcherIdentifier, bool InWasCanceled)
{
	TGraphTask< IAzureSpatialAnchors::FASALocateAnchorsCompletedTask >::CreateTask().ConstructAndDispatchWhenReady(InWatcherIdentifier, InWasCanceled);
}

// Called from an ASA worker thread.
void FAzureSpatialAnchorsBase::SessionUpdatedCallback(float InReadyForCreateProgress, float InRecommendedForCreateProgress, int InSessionCreateHash, int InSessionLocateHash, int32 InSessionUserFeedback)
{
	const EAzureSpatialAnchorsSessionUserFeedback SessionUserFeedback = static_cast<EAzureSpatialAnchorsSessionUserFeedback>(InSessionUserFeedback);
	TGraphTask< IAzureSpatialAnchors::FASASessionUpdatedTask >::CreateTask().ConstructAndDispatchWhenReady(InReadyForCreateProgress, InRecommendedForCreateProgress, InSessionCreateHash, InSessionLocateHash, SessionUserFeedback);
}

void FAzureSpatialAnchorsBase::CacheSessionStatus(float ReadyForCreateProgress, float ReccomendedForCreateProgress, int CreateHash, int LocateHash, EAzureSpatialAnchorsSessionUserFeedback Feedback)
{
	check(IsInGameThread());
	CachedSessionStatus.ReadyForCreateProgress = ReadyForCreateProgress;
	CachedSessionStatus.RecommendedForCreateProgress = ReccomendedForCreateProgress;
	CachedSessionStatus.SessionCreateHash = CreateHash;
	CachedSessionStatus.SessionLocateHash = LocateHash;
	CachedSessionStatus.feedback = Feedback;
}
