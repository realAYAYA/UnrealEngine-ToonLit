// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaterBodyMeshComponent.h"
#include "StaticMeshSceneProxy.h"

#include "WaterBodyInfoMeshComponent.generated.h"

class UWaterBodyComponent;

/**
 * WaterBodyMeshComponent used to render the water body into the water info texture.
 * Utilizes a custom scene proxy to allow hiding the mesh outside of all other passes besides the water info passes.
 */
UCLASS(MinimalAPI)
class UWaterBodyInfoMeshComponent : public UWaterBodyMeshComponent
{
	GENERATED_BODY()
public:
	UWaterBodyInfoMeshComponent(const FObjectInitializer& ObjectInitializer);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	
	virtual bool IsHLODRelevant() const { return false; }
};

struct FWaterBodyInfoMeshSceneProxy : public FStaticMeshSceneProxy
{
public:
	FWaterBodyInfoMeshSceneProxy(UWaterBodyInfoMeshComponent* Component);

	SIZE_T GetTypeHash() const
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	void SetEnabled(bool bInEnabled);
};

