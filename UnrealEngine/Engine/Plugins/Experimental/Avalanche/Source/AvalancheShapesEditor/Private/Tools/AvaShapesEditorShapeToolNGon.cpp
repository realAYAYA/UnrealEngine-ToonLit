// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolNGon.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeNGonDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolNGon::UAvaShapesEditorShapeToolNGon()
{
	ShapeClass = UAvaShapeNGonDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolNGon::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolNGon::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_NGon,
		TEXT("Parametric Regular Polygon Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolNGon>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeNGonDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolNGon::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeNGonDynamicMesh* NGon = Cast<UAvaShapeNGonDynamicMesh>(InShape);
	check(NGon);

	NGon->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
