// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaShapesEditorShapeToolLine.h"
#include "AvaShapeActor.h"
#include "AvaShapesEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "DynamicMeshes/AvaShapeLineDynMesh.h"
#include "Planners/AvaInteractiveToolsToolViewportPointListPlanner.h"

UAvaShapesEditorShapeToolLine::UAvaShapesEditorShapeToolLine()
{
	ViewportPlannerClass = UAvaInteractiveToolsToolViewportPointListPlanner::StaticClass();
	ShapeClass = UAvaShapeLineDynamicMesh::StaticClass();
}

FName UAvaShapesEditorShapeToolLine::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryName2D;
}

FAvaInteractiveToolsToolParameters UAvaShapesEditorShapeToolLine::GetToolParameters() const
{
	return {
		FAvaShapesEditorCommands::Get().Tool_Shape_Line,
		TEXT("Parametric Line Tool"),
		9000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaShapesEditorShapeToolLine>(InEdMode);
			}),
		nullptr,
		CreateFactory<UAvaShapeLineDynamicMesh>()
	};
}

void UAvaShapesEditorShapeToolLine::OnViewportPlannerUpdate()
{
	Super::OnViewportPlannerUpdate();

	if (UAvaInteractiveToolsToolViewportPointListPlanner* PointListPlanner = Cast<UAvaInteractiveToolsToolViewportPointListPlanner>(ViewportPlanner))
	{
		const TArray<FVector2f>& Positions = PointListPlanner->GetViewportPositions();

		switch (Positions.Num())
		{
			case 0:
				return;

			case 1:
				if (!PreviewActor)
				{
					PreviewActor = SpawnActor(ActorClass, true);
				}

				if (PreviewActor)
				{
					SetLineEnds(Cast<AAvaShapeActor>(PreviewActor), Positions[0], PointListPlanner->GetCurrentViewportPosition());
				}
				break;
			
			case 2:
			{
				if (PreviewActor)
				{
					PreviewActor->Destroy();
				}

				const FVector2f& Center = Positions[0] * 0.5f + Positions[1] * 0.5f;
				SpawnedActor = SpawnActor(ActorClass, EAvaViewportStatus::Focused, Center, /* Preview */ false);
				SetLineEnds(Cast<AAvaShapeActor>(SpawnedActor), Positions[0], Positions[1]);
				RequestShutdown(EToolShutdownType::Completed);
				break;
			}
			default:
				checkNoEntry();
		}
	}
}

void UAvaShapesEditorShapeToolLine::InitShape(UAvaShapeDynamicMeshBase* InShape) const
{
	UAvaShapeLineDynamicMesh* Line = Cast<UAvaShapeLineDynamicMesh>(InShape);
	check(Line);

	Line->SetLineWidth(3.f);

	Super::InitShape(InShape);
}

void UAvaShapesEditorShapeToolLine::SetShapeSize(AAvaShapeActor* InShapeActor, const FVector2D& InShapeSize) const
{
	Super::SetShapeSize(InShapeActor, InShapeSize);

	if (InShapeActor)
	{
		if (UAvaShapeLineDynamicMesh* LineMesh = Cast<UAvaShapeLineDynamicMesh>(InShapeActor->GetDynamicMesh()))
		{
			LineMesh->SetVector({InShapeSize.X, 0});
		}
	}
}

void UAvaShapesEditorShapeToolLine::SetLineEnds(AAvaShapeActor* InActor, const FVector2f& Start, const FVector2f& End)
{
	if (!InActor)
	{
		return;
	}

	FVector2f ActualEnd = End;

	if ((FMath::Abs(Start.X - End.X) + FMath::Abs(Start.Y - End.Y)) < (MinDim * 2))
	{
		for (int32 Component = 0; Component < 2; ++Component)
		{
			if (FMath::Abs(Start[Component] - End[Component]) < MinDim)
			{
				if (Start[Component] < End[Component])
				{
					ActualEnd[Component] = Start[Component] + MinDim;
				}
				else
				{
					ActualEnd[Component] = Start[Component] - MinDim;
				}
			}
			else
			{
				ActualEnd[Component] = End[Component];
			}
		}
	}

	const FVector2f& Center = Start * 0.5 + ActualEnd * 0.5;
	UWorld* TempWorld;
	FVector TempVector;
	FRotator TempRotator;
	bool bValid = true;

	bValid &= ViewportPositionToWorldPositionAndOrientation(EAvaViewportStatus::Focused, Center, TempWorld, TempVector, TempRotator);
	FVector CenterWorld = TempVector;

	bValid &= ViewportPositionToWorldPositionAndOrientation(EAvaViewportStatus::Focused, Start, TempWorld, TempVector, TempRotator);
	FVector StartWorld = TempVector;

	bValid &= ViewportPositionToWorldPositionAndOrientation(EAvaViewportStatus::Focused, ActualEnd, TempWorld, TempVector, TempRotator);
	FVector EndWorld = TempVector;

	if (bValid)
	{
		if (UAvaShapeLineDynamicMesh* LineMesh = Cast<UAvaShapeLineDynamicMesh>(InActor->GetDynamicMesh()))
		{
			FVector Vector = EndWorld - StartWorld;
			Vector = TempRotator.UnrotateVector(Vector);

			LineMesh->SetMeshRegenWorldLocation(CenterWorld);
			LineMesh->SetVector({Vector.Y, Vector.Z});
		}
	}
}
