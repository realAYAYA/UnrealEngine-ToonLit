// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolChevron.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeChevronDynMesh.h"
#include "UObject/ConstructorHelpers.h"

UAvaShapesEditorShapeToolChevron::UAvaShapesEditorShapeToolChevron()
{
	ShapeClass = UAvaShapeChevronDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolChevron::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolChevron::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Chevron,
		TEXT("Parametric Chevron Tool"),
		8000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolChevron>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeChevronDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolChevron::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeChevronDynamicMesh* Chevron = Cast<UAvaShapeChevronDynamicMesh>(InShape);
	check(Chevron);

	Chevron->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}
