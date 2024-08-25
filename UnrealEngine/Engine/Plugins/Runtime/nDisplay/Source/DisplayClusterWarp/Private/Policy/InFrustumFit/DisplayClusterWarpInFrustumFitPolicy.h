// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterWarpPolicyBase.h"
#include "Render/Viewport/IDisplayClusterViewportConfiguration.h"
#include "Blueprints/DisplayClusterWarpBlueprint_Enums.h"

#include "Containers/DisplayClusterWarpContext.h"

class ADisplayClusterRootActor;
class UDisplayClusterInFrustumFitCameraComponent;
class FDisplayClusterWarpEye;

/**
 * InFrustumFit warp policy
 */
class FDisplayClusterWarpInFrustumFitPolicy
	: public FDisplayClusterWarpPolicyBase
{
public:
	FDisplayClusterWarpInFrustumFitPolicy(const FString& InWarpPolicyName);

public:
	//~ Begin IDisplayClusterWarpPolicy
	virtual const FString& GetType() const override;
	virtual void HandleNewFrame(const TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports) override;
	virtual void Tick(IDisplayClusterViewportManager* InViewportManager, float DeltaSeconds) override;

	virtual void BeginCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum) override;
	virtual void EndCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum) override;

	virtual bool HasPreviewEditableMesh(IDisplayClusterViewport* InViewport) override;

	virtual void OnUpdateDisplayDeviceMeshAndMaterialInstance(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UMeshComponent* InMeshComponent, UMaterialInstanceDynamic* InMeshMaterialInstance) const override;
	//~~ End IDisplayClusterWarpPolicy

private:
	/** Return camera component that used for configuration.. */
	const UDisplayClusterInFrustumFitCameraComponent* GetConfigurationInFrustumFitCameraComponent(IDisplayClusterViewport* InViewport) const;

	/** Converts an asymmetric group frustum to a symmetric group frustum and computes the correction rotator to the view forward vector needed for a symmetric frustum  */
	void MakeGroupFrustumSymmetrical(bool bFixedViewTarget);

	/** Apply frustum fit to the specified warp projection */
	FDisplayClusterWarpProjection ApplyInFrustumFit(IDisplayClusterViewport* InViewport, const FTransform& World2OriginTransform, const FDisplayClusterWarpProjection& InWarpProjection);

	/** Find final projection scale.*/
	FVector2D FindFrustumFit(const EDisplayClusterWarpCameraProjectionMode InProjectionMode, const FVector2D& InCameraFOV, const FVector2D& InGeometryFOV);

#if WITH_EDITOR
	/** Renders a debug visualization for the group bounding box */
	void DrawDebugGroupBoundingBox(ADisplayClusterRootActor* RootActor, const FLinearColor& Color);

	/** Renders a debug visualization for the group frustum */
	void DrawDebugGroupFrustum(ADisplayClusterRootActor* RootActor, UDisplayClusterInFrustumFitCameraComponent* CameraComponent, const FLinearColor& Color);
#endif

private:
	// Warp projection data
	FDisplayClusterWarpProjection GroupGeometryWarpProjection;

	/** The AABB of the group computed in DCRA space */
	FDisplayClusterWarpAABB GroupAABBox;

	/** If set, stores the correction rotator to apply to the view direction to make the group frustum symmetrical */
	TOptional<FRotator> SymmetricForwardCorrection;

	bool bValidGroupFrustum = false;
	bool bGeometryContextsUpdated = false;
};
