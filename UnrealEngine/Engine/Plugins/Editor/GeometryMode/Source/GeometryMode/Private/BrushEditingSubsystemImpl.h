// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/BrushEditingSubsystem.h"

#include "BrushEditingSubsystemImpl.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBrushEditing, Log, All);

struct HGeomPolyProxy;
struct HGeomEdgeProxy;
struct HGeomVertexProxy;

UCLASS()
class UBrushEditingSubsystemImpl : public UBrushEditingSubsystem
{
	GENERATED_BODY()

public:
	UBrushEditingSubsystemImpl();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual bool ProcessClickOnBrushGeometry(FLevelEditorViewportClient* ViewportClient, HHitProxy* InHitProxy, const FViewportClick& Click) override;

	virtual void UpdateGeometryFromSelectedBrushes() override;
	virtual void UpdateGeometryFromBrush(ABrush* Brush) override;

	virtual bool IsGeometryEditorModeActive() const override;

	virtual void DeselectAllEditingGeometry() override;

	virtual bool HandleActorDelete() override;
private:

	bool ProcessClickOnGeomPoly(FLevelEditorViewportClient* ViewportClient, HGeomPolyProxy* GeomHitProxy, const FViewportClick& Click);
	bool ProcessClickOnGeomEdge(FLevelEditorViewportClient* ViewportClient, HGeomEdgeProxy* GeomHitProxy, const FViewportClick& Click);
	bool ProcessClickOnGeomVertex(FLevelEditorViewportClient* ViewportClient, HGeomVertexProxy* GeomHitProxy, const FViewportClick& Click);

};
