// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimitiveViewRelevance.h"
#include "Debug/DebugDrawComponent.h"
#include "DynamicMeshBuilder.h"
#include "DebugRenderSceneProxy.h"

class ANavigationTestingActor;
class APlayerController;
class FMeshElementCollector;
class UCanvas;
class UNavTestRenderingComponent;
struct FNodeDebugData;

class FNavTestSceneProxy final : public FDebugRenderSceneProxy
{
	friend class FNavTestDebugDrawDelegateHelper;

public:
	virtual SIZE_T GetTypeHash() const override;

	struct FNodeDebugData
	{
		NavNodeRef PolyRef;
		FVector Position;
		FString Desc;
		FSetElementId ParentId;
		uint32 bClosedSet : 1;
		uint32 bBestPath : 1;
		uint32 bModified : 1;
		uint32 bOffMeshLink : 1;

		FORCEINLINE bool operator==(const FNodeDebugData& Other) const
		{
			return PolyRef == Other.PolyRef;
		}
		FORCEINLINE friend uint32 GetTypeHash(const FNodeDebugData& Other)
		{
			return Other.PolyRef;
		}
	};

	FNavTestSceneProxy(const UNavTestRenderingComponent* InComponent);

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	void GatherPathPoints();

	void GatherPathStep();

	FORCEINLINE static bool LocationInView(const FVector& Location, const FSceneView* View)
	{
		return View->ViewFrustum.IntersectBox(Location, FVector::ZeroVector);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSizeInternal(); }

private:
	uint32 GetAllocatedSizeInternal() const;

	FVector3f NavMeshDrawOffset;

	uint32 bShowBestPath : 1;
	uint32 bShowNodePool : 1;
	uint32 bShowDiff : 1;

	ANavigationTestingActor* NavTestActor;
	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	TArray<FVector> PathPoints;
	TArray<FString> PathPointFlags;

	TArray<FDynamicMeshVertex> OpenSetVerts;
	TArray<uint32> OpenSetIndices;
	TArray<FDynamicMeshVertex> ClosedSetVerts;
	TArray<uint32> ClosedSetIndices;
	TSet<FNodeDebugData> NodeDebug;
	FSetElementId BestNodeId;

	FVector ClosestWallLocation;
};

#if WITH_RECAST && WITH_EDITOR
class FNavTestDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FNavTestDebugDrawDelegateHelper(): bShowBestPath(false), bShowDiff(false)
	{
	}

	void SetupFromProxy(const FNavTestSceneProxy* InSceneProxy);

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;

private:
	TSet<FNavTestSceneProxy::FNodeDebugData> NodeDebug;
	ANavigationTestingActor* NavTestActor = nullptr;
	TArray<FVector> PathPoints;
	TArray<FString> PathPointFlags;
	FSetElementId BestNodeId;
	uint32 bShowBestPath : 1;
	uint32 bShowDiff : 1;
};
#endif //WITH_RECAST && WITH_EDITOR

#include "NavTestRenderingComponent.generated.h"

class FPrimitiveSceneProxy;

UCLASS(ClassGroup = Debug)
class UNavTestRenderingComponent: public UDebugDrawComponent
{
	GENERATED_UCLASS_BODY()

protected:

	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;

#if WITH_RECAST && WITH_EDITOR
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return NavTestDebugDrawDelegateHelper; }
private:
	FNavTestDebugDrawDelegateHelper NavTestDebugDrawDelegateHelper;
#endif // WITH_RECAST && WITH_EDITOR
#endif // UE_ENABLE_DEBUG_DRAWING
};
