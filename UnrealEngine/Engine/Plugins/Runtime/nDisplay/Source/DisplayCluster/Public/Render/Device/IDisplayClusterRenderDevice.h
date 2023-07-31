// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StereoRendering.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"

class IDisplayClusterPresentation;
struct FDisplayClusterRenderViewContext;
class FDisplayClusterRenderFrame;
class IDisplayClusterProjectionPolicy;
class UWorld;
class FViewport;


/**
 * nDisplay render device interface
 */
class IDisplayClusterRenderDevice : public IStereoRendering
{
public:
	virtual ~IDisplayClusterRenderDevice() = default;

public:

	/**
	* Device initialization
	*
	* @return - true if success
	*/
	virtual bool Initialize()
	{ return true; }

	/**
	* Called on a scene start to allow a rendering device to initialize any world related content
	*/
	virtual void StartScene(UWorld* World)
	{ }

	/**
	* Called before scene Tick
	*/
	virtual void PreTick(float DeltaSeconds)
	{ }

	/**
	* Called before unload current level
	*/
	virtual void EndScene()
	{ }

	// update settings from root actor config data, and build new frame structure
	virtual bool BeginNewFrame(FViewport* InViewport, UWorld* InWorld, FDisplayClusterRenderFrame& OutRenderFrame) = 0;
	virtual void FinalizeNewFrame() = 0;

	/**
	* Returns current presentation handler
	*
	* @return - nullptr if failed
	*/
	virtual IDisplayClusterPresentation* GetPresentation() const = 0;

};
