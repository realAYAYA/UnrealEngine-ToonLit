// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterMediaBase.h"
#include "UObject/GCObject.h"

#include "RHI.h"
#include "RHIResources.h"

class FRDGBuilder;
class UMediaCapture;
class UMediaOutput;
class UDisplayClusterMediaOutputSynchronizationPolicy;
class IDisplayClusterMediaOutputSynchronizationPolicyHandler;

/**
 * Base media capture class
 */
class FDisplayClusterMediaCaptureBase
	: public FDisplayClusterMediaBase
	, public FGCObject
{
public:
	FDisplayClusterMediaCaptureBase(const FString& MediaId, const FString& ClusterNodeId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);
	virtual ~FDisplayClusterMediaCaptureBase();

public:
	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FDisplayClusterMediaCaptureBase");
	}
	//~ End FGCObject interface

public:
	virtual bool StartCapture();
	virtual void StopCapture();

	UMediaCapture* GetMediaCapture() const
	{
		return MediaCapture;
	}

protected:
	void ExportMediaData(FRDGBuilder& GraphBuilder, const FMediaTextureInfo& TextureInfo);
	void OnPostClusterTick();
	bool StartMediaCapture();

	virtual FIntPoint GetCaptureSize() const = 0;

private:

	// Trivial version of FIntPoint so that it can be std::atomic
	struct FIntSize
	{
		int32 X = 0;
		int32 Y = 0;

		FIntSize(int32 InX, int32 InY) : X(InX), Y(InY) {}
		FIntSize(const FIntPoint& IntPoint) : X(IntPoint.X), Y(IntPoint.Y) {}

		FIntPoint ToIntPoint()
		{
			return FIntPoint(X, Y);
		}
	};

private:
	//~ Begin GC by AddReferencedObjects
	TObjectPtr<UMediaOutput>  MediaOutput;
	TObjectPtr<UMediaCapture> MediaCapture;
	TObjectPtr<UDisplayClusterMediaOutputSynchronizationPolicy> SyncPolicy;
	//~ End GC by AddReferencedObjects

	// Used to restart media capture in the case it falls in error
	bool bWasCaptureStarted = false;

	// Used to control the rate at which we try to restart the capture
	double LastRestartTimestamp = 0;

	// Last region size of the texture being exported. Used to restart the capture when in error.
	std::atomic<FIntSize> LastSrcRegionSize { FIntSize(0,0) };

	/** Sync policy handler to deal with synchronization logic */
	TSharedPtr<IDisplayClusterMediaOutputSynchronizationPolicyHandler> SyncPolicyHandler;
};
