// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphAnnotationComponent.generated.h"

class UZoneGraphAnnotationSubsystem;
class UZoneGraphAnnotationComponent;
class UCanvas;
class AZoneGraphData;
struct FInstancedStructStream;
struct FZoneGraphAnnotationTagLookup;
struct FZoneGraphAnnotationTagContainer;

#if UE_ENABLE_DEBUG_DRAWING
class ZONEGRAPHANNOTATIONS_API FZoneGraphAnnotationSceneProxy final : public FDebugRenderSceneProxy
{
public:
	FZoneGraphAnnotationSceneProxy(const UPrimitiveComponent& InComponent, const EDrawType InDrawType = EDrawType::WireMesh);
	
	virtual SIZE_T GetTypeHash() const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint(void) const override;

private:
	uint32 ViewFlagIndex = 0;
};
#endif


UCLASS(Abstract)
class ZONEGRAPHANNOTATIONS_API UZoneGraphAnnotationComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UZoneGraphAnnotationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Called during OnRegister(), or after all subsystems have been initialized. */
	virtual void PostSubsystemsInitialized();
	
	/** Ticks the Annotation and changes the tags in the container when needed. */
	virtual void TickAnnotation(const float DeltaTime, FZoneGraphAnnotationTagContainer& AnnotationTagContainer) {}

	/** Called when new events are ready to be processed */
	virtual void HandleEvents(TConstArrayView<const UScriptStruct*> AllEventStructs, const FInstancedStructStream& Events) {}

	/** @return Tags applied by the Annotation, used to lookup Annotations from tags. */
	virtual FZoneGraphTagMask GetAnnotationTags() const { return FZoneGraphTagMask::None; }

	/** Called when new ZoneGraph data is added. */ 
	virtual void PostZoneGraphDataAdded(const AZoneGraphData& ZoneGraphData) {}

	/** Called when new ZoneGraph data is removed. */ 
	virtual void PreZoneGraphDataRemoved(const AZoneGraphData& ZoneGraphData) {}
	
#if UE_ENABLE_DEBUG_DRAWING
	/** Returns first view point (player controller or debug camera) */
	void GetFirstViewPoint(FVector& ViewLocation, FRotator& ViewRotation) const;

	/** Returns ZoneGraph max debug draw distance. */
	float GetMaxDebugDrawDistance() const;

	/** Called when scene proxy is rebuilt. */
	virtual void DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy) {}

	/** Called when it's time to draw to canvas. */
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) {}
#endif

protected:

	void OnPostZoneGraphDataAdded(const AZoneGraphData* ZoneGraphData);
	void OnPreZoneGraphDataRemoved(const AZoneGraphData* ZoneGraphData);
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues);

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	FDelegateHandle OnPostZoneGraphDataAddedHandle;
	FDelegateHandle OnPreZoneGraphDataRemovedHandle;
	FDelegateHandle OnPostWorldInitDelegateHandle;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	FDelegateHandle CanvasDebugDrawDelegateHandle;
#endif
	
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawing = false;
};
