// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/DisplayClusterObjectRef.h"
#include "Policy/DisplayClusterProjectionPolicyBase.h"


/**
 * Simple projection policy
 */
class FDisplayClusterProjectionSimplePolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionSimplePolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual const FString& GetType() const override;

	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport) override;
	virtual void HandleEndScene(IDisplayClusterViewport* InViewport) override;

	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) override;

	virtual bool HasPreviewMesh(IDisplayClusterViewport* InViewport) override
	{
		return true;
	}

	virtual UMeshComponent* GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent) override;

protected:
	bool InitializeMeshData(IDisplayClusterViewport* InViewport);
	void ReleaseMeshData(IDisplayClusterViewport* InViewport);

protected:
	// Custom projection screen geometry (hw - half-width, hh - half-height of projection screen)
	// Left bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLBC(const float& hw, const float& hh) const
	{
		return FVector(0.f, -hw, -hh);
	}
	
	// Right bottom corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryRBC(const float& hw, const float& hh) const
	{
		return FVector(0.f, hw, -hh);
	}

	// Left top corner (from camera point view)
	virtual FVector GetProjectionScreenGeometryLTC(const float& hw, const float& hh) const
	{
		return FVector(0.f, -hw, hh);
	}

private:
	// Screen ID taken from the nDisplay config file
	FString ScreenId;

	// Weak ptr screen component
	FDisplayClusterSceneComponentRef ScreenCompRef;

	// Weak ptr to preview screen component
	FDisplayClusterSceneComponentRef PreviewScreenCompRef;

	struct FViewData
	{
		FVector  ViewLoc;
		float    NCP;
		float    FCP;
		float    WorldToMeters;
	};

	TArray<FViewData> ViewData;
};
