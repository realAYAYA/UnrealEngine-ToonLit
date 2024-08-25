// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Synchronization/DisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBase.h"

#include "IRivermaxOutputStream.h"

#include "MediaOutputSynchronizationPolicyRivermax.generated.h"


/*
 * Synchronization logic handler class for UMediaOutputSynchronizationPolicyRivermax.
 */
class RIVERMAXSYNC_API FMediaOutputSynchronizationPolicyRivermaxHandler
	: public FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler
{
	using Super = FDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBaseHandler;
public:

	FMediaOutputSynchronizationPolicyRivermaxHandler(UMediaOutputSynchronizationPolicyRivermax* InPolicyObject);

	//~ Begin IDisplayClusterMediaOutputSynchronizationPolicyHandler interface
	virtual TSubclassOf<UDisplayClusterMediaOutputSynchronizationPolicy> GetPolicyClass() const override;
	//~ End IDisplayClusterMediaOutputSynchronizationPolicyHandler interface

public:
	/** We do our own synchronization by looking at distance to alignment point. */
	virtual void Synchronize() override;

	/** Returns true if specified media capture type can be synchonized by the policy implementation */
	virtual bool IsCaptureTypeSupported(UMediaCapture* MediaCapture) const override;

protected:
	/** Initializes dynamic barrier on the primary node. */
	virtual bool InitializeBarrier(const FString& SyncInstanceId) override;

	/** Barrier callback containing data from each node to detect if cluster is out of sync. */
	void HandleBarrierSync(FGenericBarrierSynchronizationDelegateData& BarrierSyncData);

	/** Returns true if frame time from each node are close to each other (limit controlled by cvar) */
	bool ValidateNodesFrameTime(const TMap<FString, TArray<uint8>>& NodeRequestData) const;

	/** Returns amount of time before next synchronization point. */
	double GetTimeBeforeNextSyncPoint() const;

protected:

	/** Holds data provided to server by each node when joining the barrier */
	struct FMediaSyncBarrierData
	{
		FMediaSyncBarrierData()
		{
		}

		/** For now, we build it directly based on presentation info from the stream */
		FMediaSyncBarrierData(const UE::RivermaxCore::FPresentedFrameInfo& FrameInfo)
			: PresentedFrameBoundaryNumber(FrameInfo.PresentedFrameBoundaryNumber)
			, LastRenderedFrameNumber(FrameInfo.RenderedFrameNumber)
		{
		}

		/** Frame boundary number at which the last frame was presented.  */
		uint64 PresentedFrameBoundaryNumber = 0;
		
		/** Last engine frame number that was presented */
		uint32 LastRenderedFrameNumber = 0;
	};

	/** Synchronization margin (ms) */
	float MarginMs = 5.0f;

	/** Memory buffer used to contain data exchanged in the barrier */
	TArray<uint8> BarrierData;

	/** On first barrier sync, we will verify frame time range to detect if clocks are too far appart. Even when desynchronized, frame time shouldn't be further than 1-2 frame away */
	bool bCanUseSelfRepair = true;

	/** Verify clock between nodes only once */
	bool bHasVerifiedClocks = false;
};


/*
 * Rivermax media synchronization policy implementation
 */
UCLASS(editinlinenew, Blueprintable, meta = (DisplayName = "Rivermax (PTP)"))
class RIVERMAXSYNC_API UMediaOutputSynchronizationPolicyRivermax
	: public UDisplayClusterMediaOutputSynchronizationPolicyEthernetBarrierBase
{
	GENERATED_BODY()


public:
	virtual TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> GetHandler() override;

protected:
	TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> Handler;

public:
	/** Synchronization margin (ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (DisplayName = "Margin (ms)", ClampMin = "1", ClampMax = "20", UIMin = "1", UIMax = "20"))
	float MarginMs = 5.0f;
};
