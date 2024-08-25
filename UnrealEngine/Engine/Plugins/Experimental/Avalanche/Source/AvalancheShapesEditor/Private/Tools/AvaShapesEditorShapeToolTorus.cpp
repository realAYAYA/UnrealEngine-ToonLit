// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolTorus.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeTorusDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolTorus::UAvaShapesEditorShapeToolTorus()
{
	ShapeClass = UAvaShapeTorusDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolTorus::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName3D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolTorus::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Torus,
		TEXT("Parametric Torus Tool"),
		4000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolTorus>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeTorusDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolTorus::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeTorusDynamicMesh* Torus = Cast<UAvaShapeTorusDynamicMesh>(InShape);
	check(Torus);

	Torus->SetSize3D({DefaultDim, DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
