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


/**
 * Base media capture class
 */
class FDisplayClusterMediaCaptureBase
	: public FDisplayClusterMediaBase
	, public FGCObject
{
public:
	FDisplayClusterMediaCaptureBase(const FString& MediaId, const FString& ClusterNodeId, UMediaOutput* MediaOutput);
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

protected:
	void ExportMediaData(FRDGBuilder& GraphBuilder, const FMediaTextureInfo& TextureInfo);
	void OnPostClusterTick();
	bool StartMediaCapture();

	virtual FIntPoint GetCaptureSize() const = 0;

private:
	//~ Begin GC by AddReferencedObjects
	UMediaOutput*           MediaOutput  = nullptr;
	UMediaCapture*          MediaCapture = nullptr;
	//~ End GC by AddReferencedObjects

	// Used to restart media capture in the case it falls in error
	bool bWasCaptureStarted = false;

	// Used to control the rate at which we try to restart the capture
	double LastRestartTimestamp = 0;
};
