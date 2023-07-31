// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "ITextureShareCoreObject.h"

#include "Game/ViewExtension/TextureShareSceneViewExtension.h"
#include "Templates/SharedPointer.h"

class FViewport;
class FTextureShareObjectProxy;

/**
 * TextureShare object
 */
class FTextureShareObject
	: public ITextureShareObject
	, public TSharedFromThis<FTextureShareObject, ESPMode::ThreadSafe>
{
public:
	FTextureShareObject(const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject);
	virtual ~FTextureShareObject();
	
public:
	// ~ITextureShareObject
	virtual const FString& GetName() const override;
	virtual const FTextureShareCoreObjectDesc& GetObjectDesc() const override;

	virtual bool IsActive() const override;
	virtual bool IsFrameSyncActive() const override;

	virtual bool SetProcessId(const FString& InProcessId) override;
	
	virtual bool SetSyncSetting(const FTextureShareCoreSyncSettings& InSyncSetting) override;
	virtual const FTextureShareCoreSyncSettings& GetSyncSetting() const override;

	virtual FTextureShareCoreFrameSyncSettings GetFrameSyncSettings(const ETextureShareFrameSyncTemplate InType) const override;

	virtual bool BeginSession() override;
	virtual bool EndSession() override;
	virtual bool IsSessionActive() const override;

	virtual bool BeginFrameSync() override;
	virtual bool FrameSync(const ETextureShareSyncStep InSyncStep) override;
	virtual bool EndFrameSync(FViewport* InViewport) override;

	virtual const TArray<FTextureShareCoreObjectDesc>& GetConnectedInterprocessObjects() const override;

	virtual FTextureShareCoreData& GetCoreData() override;
	virtual const FTextureShareCoreData& GetCoreData() const override;

	virtual const TArray<FTextureShareCoreObjectData>& GetReceivedCoreObjectData() const override;
	
	virtual FTextureShareData& GetData() override;

	virtual TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() const override;

	virtual TSharedPtr<ITextureShareObjectProxy, ESPMode::ThreadSafe> GetProxy() const override;
	//~~ITextureShareObject

	const TSharedRef<FTextureShareObjectProxy, ESPMode::ThreadSafe>& GetObjectProxyRef() const
	{
		return ObjectProxy;
	}

private:
	void UpdateViewExtension(FViewport* InViewport);

protected:
	friend class FTextureShareObjectProxy;

	// TS Core lib coreobject
	const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe> CoreObject;

	// Render thread object proxy
	const TSharedRef<FTextureShareObjectProxy, ESPMode::ThreadSafe> ObjectProxy;

	// Object data from game thread
	TSharedRef<FTextureShareData, ESPMode::ThreadSafe> TextureShareData;

	bool bFrameSyncActive = false;
	bool bSessionActive = false;

	TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
