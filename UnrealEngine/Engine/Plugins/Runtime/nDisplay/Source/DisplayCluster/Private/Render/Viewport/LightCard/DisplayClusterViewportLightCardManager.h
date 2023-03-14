// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/IDisplayClusterViewportLightCardManager.h"
#include "UObject/GCObject.h"

#include "RenderResource.h"
#include "UnrealClient.h"

class ADisplayClusterLightCardActor;
class FDisplayClusterViewportManager;
class FPreviewScene;
class FSceneInterface;
class UWorld;

/** A render targetable texture resource used to render the UV light cards to */
class FDisplayClusterLightCardMap : public FTexture, public FRenderTarget
{
public:
	FDisplayClusterLightCardMap(uint32 InSize)
		: Size(InSize)
	{ }

	virtual ~FDisplayClusterLightCardMap() = default;

	virtual uint32 GetSizeX() const override { return Size; }
	virtual uint32 GetSizeY() const override { return Size; }
	virtual FIntPoint GetSizeXY() const override { return FIntPoint(Size, Size); }

	virtual void InitDynamicRHI() override;

	virtual FString GetFriendlyName() const override { return TEXT("DisplayClusterLightCardMap"); }

private:
	uint32 Size;
};

/** Manages the rendering of UV light cards for the viewport manager */
class FDisplayClusterViewportLightCardManager : public IDisplayClusterViewportLightCardManager, public FGCObject
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
	//~ Begin IDisplayClusterViewportLightCardManager interface
	virtual FRHITexture* GetUVLightCardMap_RenderThread() const override;
	//~ End IDisplayClusterViewportLightCardManager interface

public:
	void UpdateConfiguration();

	void HandleStartScene();
	void HandleEndScene();

	void RenderFrame();

private:
	/** Initializes the preview world used to render the UV light cards*/
	void InitializePreviewWorld();

	/** Destroys the preview world used to render the UV light cards */
	void DestroyPreviewWorld();

	/** Initializes the UV light card map texture */
	void InitializeUVLightCardMap();

	/** Releases the UV light card map texture */
	void ReleaseUVLightCardMap();

private:
	/** A reference to the owning viewport manager */
	FDisplayClusterViewportManager& ViewportManager;

	/** The preview world the UV light card proxies live in for rendering */
	UWorld* PreviewWorld = nullptr;

	/** The list of UV light card actors that are referenced by the root actor */
	TArray<ADisplayClusterLightCardActor*> UVLightCards;

	/** The render target to which the UV light card map is rendered */
	FDisplayClusterLightCardMap* UVLightCardMap = nullptr;

	struct FProxyData
	{
		~FProxyData();

		void InitializeUVLightCardMap_RenderThread(FDisplayClusterLightCardMap* InUVLightCardMap);
		void ReleaseUVLightCardMap_RenderThread();

		void RenderLightCardMap_RenderThread(FRHICommandListImmediate& RHICmdList, const bool bLoadedPrimitives, FSceneInterface* InSceneInterface);

		FRHITexture* GetUVLightCardMap_RenderThread() const;

	private:
		/** The render thread copy of the pointer to the UV ligth card map */
		FDisplayClusterLightCardMap* UVLightCardMap = nullptr;

		/** A render thread flag that indicates the light card manager has UV light cards to render */
		bool bHasUVLightCards = false;
	};

	TSharedPtr<FProxyData, ESPMode::ThreadSafe> ProxyData;
};
