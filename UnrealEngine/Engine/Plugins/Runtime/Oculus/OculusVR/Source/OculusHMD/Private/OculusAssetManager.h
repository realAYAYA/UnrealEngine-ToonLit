// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRSystemAssets.h"
#include "OculusResourceHolder.h"
#include "UObject/SoftObjectPtr.h"

/**
 *
 */
class FOculusAssetManager : public IXRSystemAssets
{
public:
	FOculusAssetManager();
	virtual ~FOculusAssetManager();

public:
	UOculusResourceHolder* GetResourceHolder() { return ResourceHolder; }

	//~ IXRSystemAssets interface 

	virtual bool EnumerateRenderableDevices(TArray<int32>& DeviceListOut) override;
	virtual int32 GetDeviceId(EControllerHand ControllerHand) override;
	virtual UPrimitiveComponent* CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags, const bool bForceSynchronous, const FXRComponentLoadComplete& OnLoadComplete) override;

protected:
	UOculusResourceHolder* ResourceHolder;
};