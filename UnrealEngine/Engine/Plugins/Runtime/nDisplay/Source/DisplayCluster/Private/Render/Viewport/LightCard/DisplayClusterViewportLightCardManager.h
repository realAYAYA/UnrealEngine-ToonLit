// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterViewportLightCardManagerProxy.h"
#include "DisplayClusterViewportLightCardResource.h"

#include "UObject/GCObject.h"

class FDisplayClusterViewportConfiguration;
class ADisplayClusterLightCardActor;
class UWorld;

/**
 * Manages the rendering of UV light cards for the viewport manager (Game Thread object)
 */
class FDisplayClusterViewportLightCardManager
	: public FGCObject
{
public:
	FDisplayClusterViewportLightCardManager(const TSharedRef<const FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration);
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

public:
	/** Handle StartScene event: created and update internal resources. */
	void OnHandleStartScene();

	/** Handle EndScene event: release internal resources. */
	void OnHandleEndScene();

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

public:
	// Configuration of the current cluster node
	const TSharedRef<const FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe> Configuration;

	/** RenderThread Proxy object*/
	const TSharedRef<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManagerProxy;

private:
	/** A list of primitive components that have been added to the preview scene for rendering in the current frame */
	TArray<UPrimitiveComponent*> UVLightCardPrimitiveComponents;

	/** The render target to which the UV light card map is rendered */
	TSharedPtr<FDisplayClusterViewportLightCardResource, ESPMode::ThreadSafe> UVLightCardResource;

};
