// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaShapesEditorShapeAreaToolBase.h"
#include "AvaShapesEditorShapeToolTorus.generated.h"

UCLASS()
class UAvaShapesEditorShapeToolTorus : public UAvaShapesEditorShapeAreaToolBase
{
	GENERATED_BODY()

public:
	UAvaShapesEditorShapeToolTorus();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual FName GetCategoryName() override;
	virtual FAvaInteractiveToolsToolParameters GetToolParameters() const override;
	//~ End UAvaInteractiveToolsToolBase

protected:
	//~ Begin UAvaShapesEditorShapeToolBase
	virtual void InitShape(UAvaShapeDynamicMeshBase* InShape) const override;
	//~ End UAvaShapesEditorShapeToolBase
};
