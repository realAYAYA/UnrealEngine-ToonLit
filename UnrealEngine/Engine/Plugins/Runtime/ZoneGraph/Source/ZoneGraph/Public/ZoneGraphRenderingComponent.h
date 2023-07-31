// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "Components/PrimitiveComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ZoneGraphRenderingComponent.generated.h"

class UZoneGraphRenderingComponent;
class AZoneGraphData;

// exported to API for GameplayDebugger module
class ZONEGRAPH_API FZoneGraphSceneProxy : public FDebugRenderSceneProxy
{
public:
	virtual SIZE_T GetTypeHash() const override;

	FZoneGraphSceneProxy(const UPrimitiveComponent& InComponent, const AZoneGraphData& ZoneGraph);
	virtual ~FZoneGraphSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	struct FZoneVisibility
	{
		bool bVisible = false;
		bool bDetailsVisible = false;
		float Alpha = 1.0f;
	};
	struct FDrawDistances
	{
		float MinDrawDistanceSqr = 0.f;
		float MaxDrawDistanceSqr = FLT_MAX;
		float FadeDrawDistanceSqr = FLT_MAX;
		float DetailDrawDistanceSqr = FLT_MAX;
	};
	static FDrawDistances GetDrawDistances(const float MinDrawDistance, const float MaxDrawDistance);
	static FZoneVisibility CalculateZoneVisibility(const FDrawDistances& Distances, const FVector Origin, const FVector Position);
	static bool ShouldRenderZoneGraph(const FSceneView& View);

protected:

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }

	uint32 GetAllocatedSize(void) const
	{
		return FDebugRenderSceneProxy::GetAllocatedSize();
	}

private:

	TWeakObjectPtr<UZoneGraphRenderingComponent> WeakRenderingComponent;
	bool bSkipDistanceCheck;
};

UCLASS(hidecategories = Object, editinlinenew)
class ZONEGRAPH_API UZoneGraphRenderingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UZoneGraphRenderingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

	void ForceUpdate() { bForceUpdate = true; }
	bool IsForcingUpdate() const { return bForceUpdate; }

	static bool IsNavigationShowFlagSet(const UWorld* World);

protected:

	void CheckDrawFlagTimerFunction();

protected:
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FDelegateHandle DebugTextDrawingDelegateHandle;
#endif

	bool bPreviousShowNavigation;
	bool bForceUpdate;
	FTimerHandle TimerHandle;
};
