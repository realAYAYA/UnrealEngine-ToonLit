// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolStar.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeStarDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolStar::UAvaShapesEditorShapeToolStar()
{
	ShapeClass = UAvaShapeStarDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolStar::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolStar::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Star,
		TEXT("Parametric Star Tool"),
		6000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolStar>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeStarDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolStar::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeStarDynamicMesh* Star = Cast<UAvaShapeStarDynamicMesh>(InShape);
	check(Star);

	Star->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
