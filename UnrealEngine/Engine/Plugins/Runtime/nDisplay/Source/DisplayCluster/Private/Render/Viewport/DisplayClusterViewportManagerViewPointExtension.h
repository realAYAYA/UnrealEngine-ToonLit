// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SceneViewExtension.h"

class FDisplayClusterViewportManager;

#define DISPLAYCLUSTER_SCENE_VIEWPOINT_EXTENSION_PRIORITY -1

/**
 * View extension applying an DC Viewport features
 */
class FDisplayClusterViewportManagerViewPointExtension : public FSceneViewExtensionBase
{
public:
	FDisplayClusterViewportManagerViewPointExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager);
	virtual ~FDisplayClusterViewportManagerViewPointExtension();

public:
	//~ Begin ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override { }
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override { }
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override { }

	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InOutViewInfo) override;

	virtual int32 GetPriority() const override { return DISPLAYCLUSTER_SCENE_VIEWPOINT_EXTENSION_PRIORITY; }

	/** Set current view index for this VE:
	 * This function must be used because LocalPlayer::GetViewPoint() calls the ISceneViewExtension::SetupViewPoint() function from this VE.
	 * And at this point the VE must know the current StereoViewIndex value in order to understand which viewport will be used for this SetupViewPoint() call.
	 * Set INDEX_NONE when this VE no longer needs to be used.
	 */
	void SetCurrentStereoViewIndex(const int32 InStereoViewIndex)
	{
		CurrentStereoViewIndex = InStereoViewIndex;
	}

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	/** Get viewport manager ptr. */
	inline const FDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

	/** True, if VE can be used at the moment. */
	bool IsActive() const
	{
		return GetViewportManager() != nullptr && CurrentStereoViewIndex != INDEX_NONE;
	}

private:
	TWeakPtr<const FDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;

	// Current StereoViewIndex for rendered viewport
	int32 CurrentStereoViewIndex = INDEX_NONE;
};
