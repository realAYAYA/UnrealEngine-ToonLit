// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeAreaToolBase.h"
#include "AvaShapesEditorShapeToolCube.generated.h"

UCLASS()
class UAvaShapesEditorShapeToolCube : public UAvaShapesEditorShapeAreaToolBase
{
	GENERATED_BODY()

public:
	UAvaShapesEditorShapeToolCube();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase

protected:
	//~ Begin UAvaShapesEditorShapeToolBase
	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const override;
	//~ End UAvaShapesEditorShapeToolBase
};
