// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Synchronization/DisplayClusterMediaOutputSynchronizationPolicyThresholdBase.h"

#include "MediaOutputSynchronizationPolicyRivermax.generated.h"

/*
 * Synchronization logic handler class for UMediaOutputSynchronizationPolicyRivermax.
 */
class RIVERMAXSYNC_API FMediaOutputSynchronizationPolicyRivermaxHandler
	: public FDisplayClusterMediaOutputSynchronizationPolicyThresholdBaseHandler
{
	using Super = FDisplayClusterMediaOutputSynchronizationPolicyThresholdBaseHandler;
public:

	FMediaOutputSynchronizationPolicyRivermaxHandler(UMediaOutputSynchronizationPolicyRivermax* InPolicyObject);

	//~ Begin IDisplayClusterMediaOutputSynchronizationPolicyHandler interface
	virtual TSubclassOf<UDisplayClusterMediaOutputSynchronizationPolicy> GetPolicyClass() const override;
	//~ End IDisplayClusterMediaOutputSynchronizationPolicyHandler interface

public:
	/** Returns true if specified media capture type can be synchonized by the policy implementation */
	virtual bool IsCaptureTypeSupported(UMediaCapture* MediaCapture) const override;

protected:
	/** Returns amount of time before next synchronization point. */
	virtual double GetTimeBeforeNextSyncPoint() override;
};


/*
 * Rivermax media synchronization policy implementation
 */
UCLASS(editinlinenew, Blueprintable, meta = (DisplayName = "Rivermax (PTP)"))
class RIVERMAXSYNC_API UMediaOutputSynchronizationPolicyRivermax
	: public UDisplayClusterMediaOutputSynchronizationPolicyThresholdBase
{
	GENERATED_BODY()


public:
	virtual TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> GetHandler() override;

protected:
	TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> Handler;
};
