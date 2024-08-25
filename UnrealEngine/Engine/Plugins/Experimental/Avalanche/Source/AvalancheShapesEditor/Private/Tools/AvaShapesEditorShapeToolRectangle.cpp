// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolRectangle.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeRectangleDynMesh.h"
#include "Tools/AvaInteractiveToolsToolPresetBase.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "AvaShapesEditorShapeToolRectangle"

class FAvaShapesEditorShapeToolRectanglePreset : public FAvaInteractiveToolsToolPresetBase
{
public:
	FAvaShapesEditorShapeToolRectanglePreset(const FText& InName, const FText& InDescription, const FVector2D& InSize)
	{
		Name = InName;
		Description = InDescription;
		Size = InSize;
	}

	virtual void ApplyPreset(AActor* InActor) const override
	{
		if (AAvaShapeActor* ShapeActor = Cast<AAvaShapeActor>(InActor))
		{
			if (UAvaShapeRectangleDynamicMesh* Rectangle = Cast<UAvaShapeRectangleDynamicMesh>(ShapeActor->GetDynamicMesh()))
			{
				Rectangle->SetSize2D(Size);
			}
		}
	}

protected:
	FVector2D Size;
};

UAvaShapesEditorShapeToolRectangle::UAvaShapesEditorShapeToolRectangle()
{
	ShapeClass = UAvaShapeRectangleDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolRectangle::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolRectangle::GetToolParameters() const
{
	static const TMap<FName, TSharedRef<FAvaInteractiveToolsToolPresetBase>> Presets = {
			{
				"Square100",
				MakeShared<FAvaShapesEditorShapeToolRectanglePreset>(
					LOCTEXT("100x100", "Square [100 x 100]"),
					LOCTEXT("100x100.ToolTip", "Create a 100 x 100 square shape."),
					FVector2D(100, 100)
				)
			},
			{
				"Rectangle16x9",
				MakeShared<FAvaShapesEditorShapeToolRectanglePreset>(
					LOCTEXT("160x90", "Rectangle [160 x 90]"),
					LOCTEXT("160x90.ToolTip", "Create a 160 x 90 rectangle shape."),
					FVector2D(160, 90)
				)
			}
	};

	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Rectangle,
		TEXT("Parametric Rectangle Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolRectangle>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeRectangleDynamicMesh>(),
		Presets
	};
}

void UAvaShapesEditorShapeToolRectangle::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeRectangleDynamicMesh* Rectangle = Cast<UAvaShapeRectangleDynamicMesh>(InShape);
	check(Rectangle);

	Rectangle->SetSize2D({DefaultDim, DefaultDim});

	Super::InitShape(InShape);
}

#undef LOCTEXT_NAMESPACE
