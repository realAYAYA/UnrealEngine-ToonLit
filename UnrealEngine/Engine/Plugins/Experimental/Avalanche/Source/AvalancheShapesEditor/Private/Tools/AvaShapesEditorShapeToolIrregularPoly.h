// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeToolBase.h"
#include "AvaShapesEditorShapeToolIrregularPoly.generated.h"

UCLASS()
class UAvaShapesEditorShapeToolIrregularPoly : public UAvaShapesEditorShapeToolBase
{
	GENERATED_BODY()

public:
	UAvaShapesEditorShapeToolIrregularPoly();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	virtual void DrawHUD(FCanvas* InCanvas, IToolsContextRenderAPI* InRenderAPI) override;
	virtual void OnViewportPlannerUpdate() override;
	virtual void OnViewportPlannerComplete() override {} // Does nothing
	//~ End UAvaInteractiveToolsToolBase

protected:
	// The minimum dimension
	static constexpr float MinDim = 5;

	//~ Begin UAvaShapesEditorShapeToolBase
	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const override;
	virtual void SetShapeSize(AAvaShapeActor* InShapeActor, const FVector2D& InShapeSize) const override {}
	//~ End UAvaShapesEditorShapeToolBase

	int32 FindOverlappingPointIndex(const FVector2f& InPosition) const;

	void OnNewPointAdded();
	void UpdateTransientLineColor();
	void AddFinishedShape();
};
