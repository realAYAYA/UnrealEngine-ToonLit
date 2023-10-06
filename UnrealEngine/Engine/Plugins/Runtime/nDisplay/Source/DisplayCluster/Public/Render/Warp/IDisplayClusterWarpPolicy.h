// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterViewport;
class IDisplayClusterViewportManager;

/**
 * Warp policy interface
 * Customize warp math for projection policies.
 * Multiple projections can use the same warp policy.
 */
class IDisplayClusterWarpPolicy
{
public:
	virtual ~IDisplayClusterWarpPolicy() = default;

public:
	/** Get TSharedPtr from self. */
	virtual TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> ToSharedPtr() = 0;
	virtual TSharedPtr<const IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> ToSharedPtr() const = 0;

	/**
	* Return warp policy instance ID
	*/
	virtual const FString& GetId() const = 0;

	/**
	* Return warp policy type
	*/
	virtual const FString& GetType() const = 0;


	/** Returns true if this policy supports ICVFX rendering
	 * 
	 * @param InViewport - a owner viewport
	 * 
	 * @return - true if this warp policy supports ICVFX
	 */
	virtual bool ShouldSupportICVFX(IDisplayClusterViewport* InViewport) const
	{
		return false;
	}

	/**
	* Handle new frame for viewports group.
	* This function is called once per frame at the beginning, immediately after the viewport setup is complete.
	* 
	* @param InViewports - a group of viewports from the entire cluster that use this WarpInterface
	*/
	virtual void HandleNewFrame(const TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports)
	{ }

	/**
	* Tick function
	* The positions of movable preview components may be updated on each frame.
	* Use this function to handle these updates.
	* 
	* @param InViewportManager - Viewport manager interface
	* @param DeltaSeconds      - delta time
	*/
	virtual void Tick(IDisplayClusterViewportManager* InViewportManager, float DeltaSeconds)
	{ }

	/** Override frustum for viewport context
	 * 
	 * @param InViewport - a owner viewport
	 * @param ContextNum - viewport eye context index
	 * 
	 * @return - true if frustum overrided.
	 */
	virtual bool CalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum)
	{
		return false;
	}
};
