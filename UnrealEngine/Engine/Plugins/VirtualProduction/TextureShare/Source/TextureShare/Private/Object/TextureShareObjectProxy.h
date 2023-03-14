// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObjectProxy.h"
#include "ITextureShareCoreObject.h"
#include "Containers/TextureShareContainers.h"
#include "Templates/SharedPointer.h"

class FTextureShareResourcesProxy;
class FTextureShareSceneViewExtension;

/**
 * TextureShare proxy object
 */
class FTextureShareObjectProxy
	: public ITextureShareObjectProxy
	, public TSharedFromThis<FTextureShareObjectProxy, ESPMode::ThreadSafe>
{
	friend class FTextureShareObject;

public:
	FTextureShareObjectProxy(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject);
	virtual ~FTextureShareObjectProxy();

public:
	// ITextureShareObjectProxy
	virtual const FString& GetName_RenderThread() const override;
	virtual const FTextureShareCoreObjectDesc& GetObjectDesc_RenderThread() const override;

	virtual bool IsActive_RenderThread() const override;
	virtual bool IsFrameSyncActive_RenderThread() const override;

	virtual bool BeginFrameSync_RenderThread(FRHICommandListImmediate& RHICmdList) const override;
	virtual bool FrameSync_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep) const override;
	virtual bool EndFrameSync_RenderThread(FRHICommandListImmediate& RHICmdList) const override;

	virtual FTextureShareCoreProxyData& GetCoreProxyData_RenderThread() override;
	virtual const FTextureShareCoreProxyData& GetCoreProxyData_RenderThread() const override;

	virtual const TArray<FTextureShareCoreObjectProxyData>& GetReceivedCoreObjectProxyData_RenderThread() const override;

	virtual const FTextureShareData& GetData_RenderThread() const override;
	virtual const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& GetViewExtension_RenderThread() const override;

	virtual bool ShareResource_RenderThread(FRHICommandListImmediate& RHICmdList, const FTextureShareCoreResourceDesc& InResourceDesc, FRHITexture* InTexture, const int32 InTextureGPUIndex, const FIntRect* InTextureRect = nullptr) const override;
	virtual bool ShareResource_RenderThread(FRDGBuilder& GraphBuilder, const FTextureShareCoreResourceDesc& InResourceDesc, const FRDGTextureRef& InTextureRef, const int32 InTextureGPUIndex, const FIntRect* InTextureRect = nullptr) const override;
	//~ITextureShareObjectProxy

protected:
	bool BeginSession_RenderThread();
	bool EndSession_RenderThread();
	void HandleNewFrame_RenderThread(const TSharedRef<FTextureShareData, ESPMode::ThreadSafe>& InTextureShareData, const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe>& InViewExtension);

	static void BeginSession_GameThread(const FTextureShareObject& In);
	static void EndSession_GameThread(const FTextureShareObject& In);
	static void UpdateProxy_GameThread(const FTextureShareObject& In);

private:
	// TS Core lib object
	const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe> CoreObject;

	// Object data from game thread
	TSharedRef<FTextureShareData, ESPMode::ThreadSafe> TextureShareData;

	// Frame proxy sync valid
	mutable bool bFrameProxySyncActive = false;

	bool bSessionStarted = false;

	// All RHI resources and interfaces
	TUniquePtr<FTextureShareResourcesProxy> ResourcesProxy;

	// Scene view extension
	TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
