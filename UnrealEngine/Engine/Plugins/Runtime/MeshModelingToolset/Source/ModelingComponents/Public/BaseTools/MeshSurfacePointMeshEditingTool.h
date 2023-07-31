// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/MeshSurfacePointTool.h"
#include "MeshSurfacePointMeshEditingTool.generated.h"

/**
 * Base tool builder class for UMeshSurfacePointTools with mesh editing requirements.
 */
UCLASS()
class MODELINGCOMPONENTS_API UMeshSurfacePointMeshEditingToolBuilder : public UMeshSurfacePointToolBuilder
{
	GENERATED_BODY()
public:
	/** @return new Tool instance. Override this in subclasses to build a different Tool class type */
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};

