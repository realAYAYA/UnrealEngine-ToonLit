// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialShared.h"
#include "DynamicMeshBuilder.h"
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawComponent.h"
#include "MeshBatch.h"
#include "LocalVertexFactory.h"
#include "Math/GenericOctree.h"
#include "StaticMeshResources.h"
#include "NavigationSystemTypes.h"
#include "Templates/UnrealTemplate.h"
#include "NavMeshRenderingComponent.generated.h"

class APlayerController;
class ARecastNavMesh;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class UCanvas;
class UNavMeshRenderingComponent;

enum class ENavMeshDetailFlags : uint8
{
	TriangleEdges,
	PolyEdges,
	BoundaryEdges,
	FilledPolys,
	TileBounds,
	PathCollidingGeometry,
	TileLabels,
	PolygonLabels,
	PolygonCost,
	PolygonFlags,
	PathLabels,
	NavLinks,
	FailedNavLinks,
	Clusters,
	NavOctree,
	NavOctreeDetails,
	MarkForbiddenPolys,
};

// exported to API for GameplayDebugger module
struct NAVIGATIONSYSTEM_API FNavMeshSceneProxyData : public TSharedFromThis<FNavMeshSceneProxyData, ESPMode::ThreadSafe>
{
	struct FDebugMeshData
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
		FColor ClusterColor;
	};
	TArray<FDebugMeshData> MeshBuilders;

	struct FDebugPoint
	{
		FDebugPoint(const FVector& InPosition, const FColor& InColor, const float InSize) : Position(InPosition), Color(InColor), Size(InSize) {}
		FVector Position;
		FColor Color;
		float Size = 0.f;
	};

	TArray<FDebugRenderSceneProxy::FDebugLine> ThickLineItems;
	TArray<FDebugRenderSceneProxy::FDebugLine> TileEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavMeshEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavLinkLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> ClusterLinkLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> AuxLines;
	TArray<FDebugPoint> AuxPoints;
	TArray<FDebugRenderSceneProxy::FDebugBox> AuxBoxes;
	TArray<FDebugRenderSceneProxy::FMesh> Meshes;

	struct FDebugText
	{
		FVector Location;
		FString Text;

		FDebugText() {}
		FDebugText(const FVector& InLocation, const FString& InText) : Location(InLocation), Text(InText) {}
	};
	TArray<FDebugText> DebugLabels;
	
	TArray<FBoxCenterAndExtent>	OctreeBounds;

	FBox Bounds;
	FVector NavMeshDrawOffset;
	uint32 bDataGathered : 1;
	uint32 bNeedsNewData : 1;
	int32 NavDetailFlags;

	FNavMeshSceneProxyData() : NavMeshDrawOffset(0, 0, 10.f),
		bDataGathered(false), bNeedsNewData(true), NavDetailFlags(0) {}

	void Reset();
	void Serialize(FArchive& Ar);
	uint32 GetAllocatedSize() const;

#if WITH_RECAST
	void GatherData(const ARecastNavMesh* NavMesh, int32 InNavDetailFlags, const TArray<int32>& TileSet);

#if RECAST_INTERNAL_DEBUG_DATA
	void AddMeshForInternalData(const struct FRecastInternalDebugData& InInternalData);
#endif //RECAST_INTERNAL_DEBUG_DATA

#endif
};

// exported to API for GameplayDebugger module
class NAVIGATIONSYSTEM_API FNavMeshSceneProxy final : public FDebugRenderSceneProxy, public FNoncopyable
{
	friend class FNavMeshDebugDrawDelegateHelper;
public:
	virtual SIZE_T GetTypeHash() const override;

	FNavMeshSceneProxy(const UPrimitiveComponent* InComponent, FNavMeshSceneProxyData* InProxyData, bool ForceToRender = false);
	virtual ~FNavMeshSceneProxy() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

protected:
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSizeInternal(); }
	uint32 GetAllocatedSizeInternal(void) const;

private:			
	FNavMeshSceneProxyData ProxyData;

	FDynamicMeshIndexBuffer32 IndexBuffer;
	FStaticMeshVertexBuffers VertexBuffers;
	FLocalVertexFactory VertexFactory;

	TArray<TUniquePtr<FColoredMaterialRenderProxy>> MeshColors;
	TArray<FMeshBatchElement> MeshBatchElements;

	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	TWeakObjectPtr<UNavMeshRenderingComponent> RenderingComponent;
	uint32 bForceRendering : 1;
	uint32 bSkipDistanceCheck : 1;
	uint32 bUseThickLines : 1;
};

#if WITH_RECAST && UE_ENABLE_DEBUG_DRAWING
class FNavMeshDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FNavMeshDebugDrawDelegateHelper()
		: bForceRendering(false)
		, bNeedsNewData(false)
	{
	}

	void SetupFromProxy(const FNavMeshSceneProxy* InSceneProxy)
	{
		DebugLabels.Reset();
		DebugLabels.Append(InSceneProxy->ProxyData.DebugLabels);
		bForceRendering = InSceneProxy->bForceRendering;
		bNeedsNewData = InSceneProxy->ProxyData.bNeedsNewData;
	}

protected:
	NAVIGATIONSYSTEM_API virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;

private:
	TArray<FNavMeshSceneProxyData::FDebugText> DebugLabels;
	uint32 bForceRendering : 1;
	uint32 bNeedsNewData : 1;
};
#endif

UCLASS(editinlinenew, ClassGroup = Debug)
class NAVIGATIONSYSTEM_API UNavMeshRenderingComponent : public UDebugDrawComponent
{
	GENERATED_UCLASS_BODY()

public:
	void ForceUpdate() { bForceUpdate = true; }
	bool IsForcingUpdate() const { return bForceUpdate; }

	static bool IsNavigationShowFlagSet(const UWorld* World);

protected:
	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;

#if UE_ENABLE_DEBUG_DRAWING
  	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return NavMeshDebugDrawDelegateManager; }
#endif

	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

	/** Gathers drawable information from NavMesh and puts it in OutProxyData. 
	 *	Override to add additional information to OutProxyData.*/
	virtual void GatherData(const ARecastNavMesh& NavMesh, FNavMeshSceneProxyData& OutProxyData) const;

	void TimerFunction();

protected:
	uint32 bCollectNavigationData : 1;
	uint32 bForceUpdate : 1;
	FTimerHandle TimerHandle;

protected:
#if UE_ENABLE_DEBUG_DRAWING
	FNavMeshDebugDrawDelegateHelper NavMeshDebugDrawDelegateManager;
#endif
};

namespace FNavMeshRenderingHelpers
{
	NAVIGATIONSYSTEM_API void AddVertex(FNavMeshSceneProxyData::FDebugMeshData& MeshData, const FVector& Pos, const FColor Color = FColor::White);

	NAVIGATIONSYSTEM_API void AddTriangleIndices(FNavMeshSceneProxyData::FDebugMeshData& MeshData, int32 V0, int32 V1, int32 V2);
}
