// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeToolBase.h"
#include "AvaShapesEditorShapeToolLine.generated.h"

UCLASS()
class UAvaShapesEditorShapeToolLine : public UAvaShapesEditorShapeToolBase
{
	GENERATED_BODY()

public:
	UAvaShapesEditorShapeToolLine();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	virtual void OnViewportPlannerUpdate() override;
	virtual void OnViewportPlannerComplete() override {} // Does nothing
	//~ End UAvaInteractiveToolsToolBase

protected:
	// The minimum dimension
	static constexpr float MinDim = 5;

	//~ Begin UAvaShapesEditorShapeToolBase
	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const override;
	virtual void SetShapeSize(AAvaShapeActor* InShapeActor, const FVector2D& InShapeSize) const override;
	//~ End UAvaShapesEditorShapeToolBase

	void SetLineEnds(AAvaShapeActor* InShapeActor, const FVector2f& Start, const FVector2f& End);
};
