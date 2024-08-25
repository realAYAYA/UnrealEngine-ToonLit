// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/ImplicitObject.h"
#include "Components/ChaosVDSceneQueryDataComponent.h"

struct FChaosVDRecording;
class FChaosVDGeometryBuilder;
struct FChaosVDVisualizationContext;
struct FChaosVDQueryDataWrapper;

/** Visualization context structure specific for Scene Queries visualizations */
struct FChaosVDSceneQueryVisualizationDataContext : public FChaosVDVisualizationContext
{
	FChaosVDSceneQuerySelectionHandle DataSelectionHandle = FChaosVDSceneQuerySelectionHandle(nullptr, INDEX_NONE);

	/** Generates a random color based on the selection state and query ID, which will be used to debug draw the scene query */
	void GenerateColor(int32 QueryID, bool bIsSelected)
	{
		RandomSeededColor = bIsSelected ? FLinearColor::White : FLinearColor::MakeRandomSeededColor(QueryID);
		DebugDrawColor = RandomSeededColor.ToFColorSRGB();
		DebugDrawDarkerColor = (RandomSeededColor * 0.85f).ToFColorSRGB();
		HitColor = (RandomSeededColor * 1.2f).ToFColorSRGB();
	}

	FLinearColor RandomSeededColor = FLinearColor(EForceInit::ForceInit);
	FColor DebugDrawColor = FColor(EForceInit::ForceInit);
	FColor DebugDrawDarkerColor = FColor(EForceInit::ForceInit);
	FColor HitColor = FColor(EForceInit::ForceInit);

	Chaos::FConstImplicitObjectPtr InputGeometry = nullptr;
	TWeakPtr<FChaosVDGeometryBuilder> GeometryGenerator = nullptr;
};

/** Custom Hit Proxy for debug drawn scene queries */
struct HChaosVDSceneQueryProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDSceneQueryProxy(const UActorComponent* Component, const FChaosVDSceneQuerySelectionHandle& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDSceneQuerySelectionHandle DataSelectionHandle;
};

/** Set of visualization flags options for Scene Queries */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDSceneQueryVisualizationFlags: uint32
{
	None					= 0 UMETA(Hidden),
	EnableDraw				= 1 << 0,
	DrawLineTraceQueries	= 1 << 1,
	DrawSweepQueries		= 1 << 2,
	DrawOverlapQueries		= 1 << 3,
	DrawHits				= 1 << 4,
	OnlyDrawSelectedQuery	= 1 << 5,
	HideEmptyQueries		= 1 << 6,
	HideSubQueries			= 1 << 7,
};
ENUM_CLASS_FLAGS(EChaosVDSceneQueryVisualizationFlags);

/**
 * Component visualizer in charge of generating debug draw visualizations for scene queries in a ChaosVDSceneQueryDataComponent
 */
class FChaosVDSceneQueryDataComponentVisualizer final : public FComponentVisualizer
{

public:
	FChaosVDSceneQueryDataComponentVisualizer()
	{
	}

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

private:

	void DrawLineTraceQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawOverlapQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawSweepQuery(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
	void DrawHits(const UActorComponent* Component, const FChaosVDQueryDataWrapper& SceneQueryData, FPrimitiveDrawInterface* PDI, const FColor& InColor, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext);

	void DrawSceneQuery(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI, const TSharedPtr<FChaosVDScene>& CVDScene, const TSharedPtr<FChaosVDRecording>& CVDRecording, FChaosVDSceneQueryVisualizationDataContext& VisualizationContext, const TSharedPtr<FChaosVDQueryDataWrapper>& Query);

	bool HasEndLocation(const FChaosVDQueryDataWrapper& SceneQueryData) const;
};
