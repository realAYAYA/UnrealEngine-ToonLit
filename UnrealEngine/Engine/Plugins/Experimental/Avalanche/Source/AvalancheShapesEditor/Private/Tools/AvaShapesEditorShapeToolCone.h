// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeAreaToolBase.h"
#include "AvaShapesEditorShapeToolCone.generated.h"

UCLASS()
class UAvaShapesEditorShapeToolCone : public UAvaShapesEditorShapeAreaToolBase
{
	GENERATED_BODY()

public:
	UAvaShapesEditorShapeToolCone();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase

protected:
	//~ Begin UAvaShapesEditorShapeToolBase
	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const override;
	//~ End UAvaShapesEditorShapeToolBase
};
