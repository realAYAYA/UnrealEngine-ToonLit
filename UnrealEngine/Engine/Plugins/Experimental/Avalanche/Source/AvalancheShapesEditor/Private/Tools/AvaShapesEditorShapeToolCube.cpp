// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolCube.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeCubeDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolCube::UAvaShapesEditorShapeToolCube()
{
	ShapeClass = UAvaShapeCubeDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolCube::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName3D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolCube::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Cube,
		TEXT("Parametric Cube Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolCube>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeCubeDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolCube::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeCubeDynamicMesh* Cube = Cast<UAvaShapeCubeDynamicMesh>(InShape);
	check(Cube);

	Cube->SetSize3D({DefaultDim, DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
