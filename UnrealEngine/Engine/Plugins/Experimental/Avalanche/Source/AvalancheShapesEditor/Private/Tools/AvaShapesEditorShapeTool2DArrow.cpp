// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeTool2DArrow.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShape2DArrowDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeTool2DArrow::UAvaShapesEditorShapeTool2DArrow()
{
	ShapeClass = UAvaShape2DArrowDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeTool2DArrow::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeTool2DArrow::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_2DArrow,
		TEXT("Parametric 2D Arrow Tool"),
		7000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeTool2DArrow>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShape2DArrowDynamicMesh>()
	};
}

void UAvaShapesEditorShapeTool2DArrow::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShape2DArrowDynamicMesh* Arrow2D = Cast<UAvaShape2DArrowDynamicMesh>(InShape);
	check(Arrow2D);

	Arrow2D->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
