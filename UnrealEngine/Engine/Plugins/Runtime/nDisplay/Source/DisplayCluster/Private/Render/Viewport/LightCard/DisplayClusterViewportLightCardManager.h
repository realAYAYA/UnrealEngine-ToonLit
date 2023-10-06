// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterViewportLightCardManagerProxy.h"
#include "DisplayClusterViewportLightCardResource.h"

#include "UObject/GCObject.h"

class ADisplayClusterLightCardActor;
class FDisplayClusterViewportManager;
class UWorld;

/**
 * Manages the rendering of UV light cards for the viewport manager (Game Thread object)
 */
class FDisplayClusterViewportLightCardManager
	: public FGCObject
{
public:
	FDisplayClusterViewportLightCardManager(FDisplayClusterViewportManager& InViewportManager);
	virtual ~FDisplayClusterViewportLightCardManager();

	void Release();

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDisplayClusterViewportLightCardManager"); }
	//~ End FGCObject interface

public:
	/** Return true if UV LightCard is used in this frame. */
	bool IsUVLightCardEnabled() const;

	/** Get UV LightCard texture size. */
	FIntPoint GetUVLightCardResourceSize() const;

	/** Get proxy object. */
	TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> GetLightCardManagerProxy() const
	{ return LightCardManagerProxy; }

public:
	/** Update internals for the new frame. */
	void UpdateConfiguration();

	/** Handle StartScene event: created and update internal resources. */
	void HandleStartScene();

	/** Handle EndScene event: release internal resources. */
	void HandleEndScene();

	/** Render internal resoures for current frame. */
	void RenderFrame();

private:
	/** Render UVLightCard */
	void RenderUVLightCard();

	/** Update the UV light card map texture */
	void UpdateUVLightCardResource();

	/** Releases the UV light card map texture */
	void ReleaseUVLightCardResource();

	/** Create the UV light card map texture */
	void CreateUVLightCardResource(const FIntPoint& InResourceSize);

	/** Update UVLightCard data game thread*/
	void UpdateUVLightCardData();

	/** Release UVLightCard data game thread*/
	void ReleaseUVLightCardData();

	/** Get a pointer to the DC ViewportManager if it still exists. */
	FDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

private:
	/** A reference to the owning viewport manager */
	const TWeakPtr<FDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;

	/** A list of primitive components that have been added to the preview scene for rendering in the current frame */
	TArray<UPrimitiveComponent*> UVLightCardPrimitiveComponents;

	/** The render target to which the UV light card map is rendered */
	TSharedPtr<FDisplayClusterViewportLightCardResource, ESPMode::ThreadSafe> UVLightCardResource;

private:
	/** RenderThread Proxy object*/
	TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManagerProxy;
};
