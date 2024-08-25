// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolRing.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeRingDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolRing::UAvaShapesEditorShapeToolRing()
{
	ShapeClass = UAvaShapeRingDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolRing::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolRing::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Ring,
		TEXT("Parametric Ring Tool"),
		5000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolRing>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeRingDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolRing::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeRingDynamicMesh* Ring = Cast<UAvaShapeRingDynamicMesh>(InShape);
	check(Ring);

	Ring->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
