// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolCone.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeConeDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolCone::UAvaShapesEditorShapeToolCone()
{
	ShapeClass = UAvaShapeConeDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolCone::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName3D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolCone::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Cone,
		TEXT("Parametric Cone Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolCone>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeConeDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolCone::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeConeDynamicMesh* Cone = Cast<UAvaShapeConeDynamicMesh>(InShape);
	check(Cone);

	Cone->SetSize3D({DefaultDim, DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
