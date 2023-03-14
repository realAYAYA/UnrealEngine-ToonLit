// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "MeshBatch.h"
#include "PrimitiveSceneProxy.h"
#include "PointCloudVertexFactory.h"
#include "PointCloudBuffers.h"

class UPointCloudComponent;

/** Point cloud rendering when vertex fetch is not supported, aka the slow way */
class FNoFetchPointCloudSceneProxy final :
	public FPrimitiveSceneProxy
{
public:
	FNoFetchPointCloudSceneProxy(UPointCloudComponent* Component);
	virtual ~FNoFetchPointCloudSceneProxy();
	
	SIZE_T GetTypeHash() const override;
	
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;
	
	virtual uint32 GetMemoryFootprint(void) const;
	uint32 GetAllocatedSize(void) const;
	
private:
	TArray<FVector> Points;
	FLinearColor Color;
	float Size;
	bool bIsVisible;
	UTexture* SpriteTexture = nullptr;
	FMaterialRelevance MaterialRelevance;
};


/** Point cloud rendering when manual vertex fetch is supported */
class FPointCloudSceneProxy :
	public FPrimitiveSceneProxy
{
public:
	FPointCloudSceneProxy(UPointCloudComponent* Component);
	virtual ~FPointCloudSceneProxy();

	SIZE_T GetTypeHash() const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() override;

private:
	bool bIsVisible;
	/** Temp array until the render resource gets created */
	TArray<FVector> Points;
	/** Temp array until the render resource gets created */
	TArray<FColor> Colors;
	/** The linear color to use to draw all points */
	FLinearColor PointColor;
	/** The point size to render at */
	float PointSize;
	/** The material from the component to render with */
	UMaterialInterface* Material;
	/** The index buffer to use when drawing */
	FPointCloudIndexBuffer PointCloudIndexBuffer;
	/** The vertex buffer of colors for each point */
	FPointCloudColorVertexBuffer PointCloudColorVertexBuffer;
	/** The vertex buffer of locations for each point */
	FPointCloudLocationVertexBuffer PointCloudLocationVertexBuffer;

	FMaterialRelevance MaterialRelevance;
	FPointCloudVertexFactory PointCloudVertexFactory;
};
