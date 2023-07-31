// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRSystemAssets.h"
#include "UObject/SoftObjectPtr.h"

class  FSteamVRAsyncMeshLoader;
struct FSteamVRMeshData;
class  UProceduralMeshComponent;
class  UMaterial;
class  UTexture2D;

/**
 *
 */
class FSteamVRAssetManager : public IXRSystemAssets
{
public:
	FSteamVRAssetManager();
	virtual ~FSteamVRAssetManager();

public:
	//~ IXRSystemAssets interface 

	virtual bool EnumerateRenderableDevices(TArray<int32>& DeviceListOut) override;
	virtual int32 GetDeviceId(EControllerHand ControllerHand) override;
	virtual UPrimitiveComponent* CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags, const bool bForceSynchronous, const FXRComponentLoadComplete& OnLoadComplete) override;

protected:
	struct FAsyncLoadData
	{
		TWeakObjectPtr<UProceduralMeshComponent> ComponentPtr;
		FString LoadedModelName;
	};
	void OnMeshLoaded(int32 SubMeshIndex, const FSteamVRMeshData& MeshData, UTexture2D* DiffuseTex, FAsyncLoadData LoadData);
	void OnComponentLoadComplete(TWeakObjectPtr<UProceduralMeshComponent> ComponentPtr, FXRComponentLoadComplete LoadCompleteCallback);
	void OnModelFullyLoaded(FString ModelName);

private:
	TArray< TSharedPtr<FSteamVRAsyncMeshLoader> > AsyncMeshLoaders;
	TSoftObjectPtr<UMaterial> DefaultDeviceMat;
	TMap< FString, TSharedPtr<FSteamVRAsyncMeshLoader> > ActiveMeshLoaders;
};
