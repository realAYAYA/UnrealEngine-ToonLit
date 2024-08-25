// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolEllipse.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeEllipseDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolEllipse::UAvaShapesEditorShapeToolEllipse()
{
	ShapeClass = UAvaShapeEllipseDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolEllipse::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolEllipse::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Ellipse,
		TEXT("Parametric Ellipse Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolEllipse>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeEllipseDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolEllipse::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeEllipseDynamicMesh* Ellipse = Cast<UAvaShapeEllipseDynamicMesh>(InShape);
	check(Ellipse);

	Ellipse->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
