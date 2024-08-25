// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolSphere.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeSphereDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolSphere::UAvaShapesEditorShapeToolSphere()
{
	ShapeClass = UAvaShapeSphereDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolSphere::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName3D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolSphere::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Sphere,
		TEXT("Parametric Sphere Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolSphere>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeSphereDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolSphere::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeSphereDynamicMesh* Sphere = Cast<UAvaShapeSphereDynamicMesh>(InShape);
	check(Sphere);

	Sphere->SetSize3D({DefaultDim, DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
