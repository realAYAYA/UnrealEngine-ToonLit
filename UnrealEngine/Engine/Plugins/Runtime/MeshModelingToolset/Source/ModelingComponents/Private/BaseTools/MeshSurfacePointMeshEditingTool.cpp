// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "ToolTargetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSurfacePointMeshEditingTool)


/*
 * ToolBuilder
 */

const FToolTargetTypeRequirements& UMeshSurfacePointMeshEditingToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMaterialProvider::StaticClass(),
		UMeshDescriptionProvider::StaticClass(),
		UMeshDescriptionCommitter::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

UMeshSurfacePointTool* UMeshSurfacePointMeshEditingToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UMeshSurfacePointTool>(SceneState.ToolManager);
}



