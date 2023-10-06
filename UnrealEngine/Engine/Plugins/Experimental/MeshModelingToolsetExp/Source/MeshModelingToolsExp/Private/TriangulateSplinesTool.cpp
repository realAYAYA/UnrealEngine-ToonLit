// Copyright Epic Games, Inc. All Rights Reserved.

#include "TriangulateSplinesTool.h"

#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "Selection/ToolSelectionUtil.h"
#include "DynamicMesh/MeshTransforms.h"

#include "Engine/World.h"

#include "Components/SplineComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TriangulateSplinesTool)

#define LOCTEXT_NAMESPACE "UTriangulateSplinesTool"

using namespace UE::Geometry;



void UTriangulateSplinesTool::Setup()
{
	UInteractiveTool::Setup();

	// initialize our properties

	TriangulateProperties = NewObject<UTriangulateSplinesToolProperties>(this);
	TriangulateProperties->RestoreProperties(this);
	AddToolPropertySource(TriangulateProperties);

	OutputTypeProperties = NewObject<UCreateMeshObjectTypeProperties>(this);
	OutputTypeProperties->InitializeDefault();
	OutputTypeProperties->RestoreProperties(this);
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	AddToolPropertySource(OutputTypeProperties);

	SetToolDisplayName(LOCTEXT("TriangulateSplinesToolName", "Triangulate Splines"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("TriangulateSplinesToolToolDescription", "Triangulate the shapes of the selected splines."),
		EToolMessageLevel::UserNotification);

	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	Preview->Setup(GetTargetWorld(), this);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, nullptr);

	Preview->OnMeshUpdated.AddLambda(
		[this](const UMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			UpdateAcceptWarnings(UpdatedPreview->HaveEmptyResult() ? EAcceptWarning::EmptyForbidden : EAcceptWarning::NoWarning);
		}
	);

	PollSplineUpdates();
}

void UTriangulateSplinesTool::PollSplineUpdates()
{
	bool bSplinesUpdated = false;
	int32 SplineIdx = 0;
	TArray<TObjectPtr<USplineComponent>, TInlineAllocator<8>> SplineComponents;
	for (int32 ActorIdx = 0; ActorIdx < ActorsWithSplines.Num(); ++ActorIdx)
	{
		if (AActor* Actor = ActorsWithSplines[ActorIdx].Get())
		{
			SplineComponents.Reset();
			Actor->GetComponents<USplineComponent, TInlineAllocator<8>>(SplineComponents);
			for (USplineComponent* SplineComponent : SplineComponents)
			{
				int32 Version = SplineComponent->SplineCurves.Version;
				FTransform Transform = SplineComponent->GetComponentTransform();
				if (SplineIdx >= LastSplineVersions.Num())
				{
					bSplinesUpdated = true;
					LastSplineVersions.Add(Version);
					LastSplineTransforms.Add(Transform);
				}
				else if (LastSplineVersions[SplineIdx] != Version || !LastSplineTransforms[SplineIdx].Equals(Transform))
				{
					bSplinesUpdated = true;
				}
				LastSplineVersions[SplineIdx] = Version;
				LastSplineTransforms[SplineIdx] = Transform;
				++SplineIdx;
			}
		}
	}
	if (LastSplineVersions.Num() != SplineIdx)
	{
		LastSplineVersions.SetNum(SplineIdx);
		LastSplineTransforms.SetNum(SplineIdx);
		bSplinesUpdated = true;
	}
	if (bSplinesUpdated)
	{
		Preview->InvalidateResult();
	}
}

void UTriangulateSplinesTool::OnTick(float DeltaTime)
{
	PollSplineUpdates();

	Preview->Tick(DeltaTime);
}

void UTriangulateSplinesTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

UWorld* UTriangulateSplinesTool::GetTargetWorld()
{
	return TargetWorld.Get();
}

void UTriangulateSplinesTool::GenerateAsset(const FDynamicMeshOpResult& OpResult)
{
	if (OpResult.Mesh.Get() == nullptr) return;

	FTransform3d NewTransform;
	if (ActorsWithSplines.Num() == 1 && ActorsWithSplines[0].IsValid()) // in the single-actor case, shove the result back into the original actor transform space
	{
		FTransform3d ActorToWorld = (FTransform3d)ActorsWithSplines[0]->GetTransform();
		MeshTransforms::ApplyTransform(*OpResult.Mesh, OpResult.Transform, true);
		MeshTransforms::ApplyTransformInverse(*OpResult.Mesh, ActorToWorld, true);
		NewTransform = ActorToWorld;
	}
	else // in the multi-selection case, center the pivot for the combined result
	{
		FVector3d Center = OpResult.Mesh->GetBounds().Center();
		double Rescale = OpResult.Transform.GetScale().X;
		FTransform3d LocalTransform(-Center * Rescale);
		LocalTransform.SetScale3D(FVector3d(Rescale, Rescale, Rescale));
		MeshTransforms::ApplyTransform(*OpResult.Mesh, LocalTransform, true);
		NewTransform = OpResult.Transform;
		NewTransform.SetScale3D(FVector3d::One());
		NewTransform.SetTranslation(NewTransform.GetTranslation() + NewTransform.TransformVector(Center * Rescale));
	}

	FString BaseName = TEXT("Triangulation");

	FCreateMeshObjectParams NewMeshObjectParams;
	NewMeshObjectParams.TargetWorld = GetTargetWorld();
	NewMeshObjectParams.Transform = (FTransform)NewTransform;
	NewMeshObjectParams.BaseName = BaseName;
	NewMeshObjectParams.Materials = Preview->StandardMaterials;
	NewMeshObjectParams.SetMesh(OpResult.Mesh.Get());
	OutputTypeProperties->ConfigureCreateMeshObjectParams(NewMeshObjectParams);
	FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
	if (Result.IsOK() && Result.NewActor != nullptr)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), Result.NewActor);
	}
}

void UTriangulateSplinesTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (Property &&
		(PropertySet == OutputTypeProperties
			))
	{
		// nothing
	}
	else
	{
		Preview->InvalidateResult();
	}
}

void UTriangulateSplinesTool::Shutdown(EToolShutdownType ShutdownType)
{
	OutputTypeProperties->SaveProperties(this);
	TriangulateProperties->SaveProperties(this);

	FDynamicMeshOpResult Result = Preview->Shutdown();

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("TriangulateSplinesAction", "Spline Triangulation"));

		// Generate the result asset
		GenerateAsset(Result);

		GetToolManager()->EndUndoTransaction();
	}

	TargetWorld = nullptr;
	Super::Shutdown(ShutdownType);
}

bool UTriangulateSplinesTool::CanAccept() const
{
	return Preview->HaveValidNonEmptyResult();
}

TUniquePtr<FDynamicMeshOperator> UTriangulateSplinesTool::MakeNewOperator()
{
	TUniquePtr<FTriangulateCurvesOp> Op = MakeUnique<FTriangulateCurvesOp>();

	Op->Thickness = TriangulateProperties->Thickness;
	Op->bFlipResult = TriangulateProperties->bFlipResult;
	Op->CombineMethod = TriangulateProperties->CombineMethod;
	Op->FlattenMethod = TriangulateProperties->FlattenMethod;
	Op->CurveOffset = TriangulateProperties->CurveOffset;
	if (TriangulateProperties->CurveOffset == 0.0)
	{
		Op->OffsetClosedMethod = EOffsetClosedCurvesMethod::DoNotOffset;
	}
	else
	{
		Op->OffsetClosedMethod = TriangulateProperties->OffsetClosedCurves;
	}
	Op->OffsetOpenMethod = TriangulateProperties->OpenCurves;
	Op->OffsetJoinMethod = TriangulateProperties->JoinMethod;
	Op->OpenEndShape = TriangulateProperties->EndShapes;
	Op->MiterLimit = TriangulateProperties->MiterLimit;
	
	Op->bFlipResult = TriangulateProperties->bFlipResult;

	TArray<TObjectPtr<USplineComponent>, TInlineAllocator<8>> SplineComponents;
	for (int32 ActorIdx = 0; ActorIdx < ActorsWithSplines.Num(); ++ActorIdx)
	{
		if (AActor* Actor = ActorsWithSplines[ActorIdx].Get())
		{
			SplineComponents.Reset();
			Actor->GetComponents<USplineComponent, TInlineAllocator<8>>(SplineComponents);
			for (USplineComponent* SplineComponent : SplineComponents)
			{
				Op->AddSpline(SplineComponent, TriangulateProperties->ErrorTolerance);
			}
		}
	}

	return Op;
}

/// Tool builder

bool UTriangulateSplinesToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	int32 NumSplines = ToolBuilderUtil::CountComponents(SceneState, [&](UActorComponent* Object) -> bool
	{
		return Object->IsA<USplineComponent>();
	});
	return NumSplines > 0;
}

UInteractiveTool* UTriangulateSplinesToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UTriangulateSplinesTool* NewTool = NewObject<UTriangulateSplinesTool>(SceneState.ToolManager);
	InitializeNewTool(NewTool, SceneState);
	return NewTool;
}

void UTriangulateSplinesToolBuilder::InitializeNewTool(UTriangulateSplinesTool* NewTool, const FToolBuilderState& SceneState) const
{
	TArray<UActorComponent*> Components = ToolBuilderUtil::FindAllComponents(SceneState, [&](UActorComponent* Object)
	{
		return Object->IsA<USplineComponent>();
	});
	TArray<TWeakObjectPtr<AActor>> ActorsWithSplines;
	TSet<AActor*> FoundActors;
	for (UActorComponent* Component : Components)
	{
		AActor* ActorWithSpline = Component->GetOwner();
		if (!FoundActors.Contains(ActorWithSpline))
		{
			FoundActors.Add(ActorWithSpline);
			ActorsWithSplines.Add(ActorWithSpline);
		}
	}
	NewTool->SetSplineActors(MoveTemp(ActorsWithSplines));
	NewTool->SetWorld(SceneState.World);
}



#undef LOCTEXT_NAMESPACE

