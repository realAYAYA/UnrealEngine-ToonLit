// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevolveSplineTool.h"

#include "CompGeom/PolygonTriangulation.h"
#include "CompositionOps/CurveSweepOp.h"
#include "DynamicMesh/MeshTransforms.h"
#include "GameFramework/Actor.h"
#include "InteractiveToolManager.h"
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute
#include "Properties/MeshMaterialProperties.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "Selection/ToolSelectionUtil.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"

#define LOCTEXT_NAMESPACE "URevolveSplineTool"

using namespace UE::Geometry;



void URevolveSplineTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<URevolveSplineToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	ToolActions = NewObject<URevolveSplineToolActionPropertySet>(this);
	ToolActions->Initialize(this);
	AddToolPropertySource(ToolActions);

	MaterialProperties = NewObject<UNewMeshMaterialProperties>(this);
	AddToolPropertySource(MaterialProperties);
	MaterialProperties->RestoreProperties(this);

	OutputTypeProperties = NewObject<UCreateMeshObjectTypeProperties>(this);
	OutputTypeProperties->InitializeDefault();
	OutputTypeProperties->RestoreProperties(this);
	OutputTypeProperties->WatchProperty(OutputTypeProperties->OutputType, [this](FString) { OutputTypeProperties->UpdatePropertyVisibility(); });
	AddToolPropertySource(OutputTypeProperties);

	SetToolDisplayName(LOCTEXT("RevolveSplineToolName", "Revolve Spline"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("RevolveSplineToolDescription", "Revolve the selected spline to create a mesh."),
		EToolMessageLevel::UserNotification);

	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>(this);
	Preview->Setup(GetTargetWorld(), this);
	Preview->PreviewMesh->EnableWireframe(MaterialProperties->bShowWireframe);
	Preview->ConfigureMaterials(MaterialProperties->Material.Get(),
		ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager()));
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, nullptr);

	Preview->OnMeshUpdated.AddLambda(
		[this](const UMeshOpPreviewWithBackgroundCompute* UpdatedPreview)
		{
			UpdateAcceptWarnings(UpdatedPreview->HaveEmptyResult() ? EAcceptWarning::EmptyForbidden : EAcceptWarning::NoWarning);
		}
	);

	// TODO: We'll probably want a click behavior someday for clicking on the spline to align to a tangent at a point

	// The plane mechanic is used for the revolution axis.
	// Note: The only thing we really end up using from it is the gizmo and the control+click. We
	// could use our own gizmo directly.
	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->Initialize(GetTargetWorld(), FFrame3d(Settings->AxisOrigin, 
		FRotator(Settings->AxisOrientation.X, Settings->AxisOrientation.Y, 0).Quaternion()));
	PlaneMechanic->bShowGrid = false;
	PlaneMechanic->OnPlaneChanged.AddLambda([this]() {
		Settings->AxisOrigin = (FVector)PlaneMechanic->Plane.Origin;
		FRotator AxisOrientation = ((FQuat)PlaneMechanic->Plane.Rotation).Rotator();
		Settings->AxisOrientation.X = AxisOrientation.Pitch;
		Settings->AxisOrientation.Y = AxisOrientation.Yaw;
		NotifyOfPropertyChangeByTool(Settings);
		UpdateRevolutionAxis();
		});
	// Add if we get our own click behavior:
	//PlaneMechanic->UpdateClickPriority(ClickBehavior->GetPriority().MakeLower());

	// TODO: It would be nice to have a drag alignment mechanic for the above gizmo, but we currently
	// don't have a way to pass in a custom alignment raycast, which we would want in order to snap
	// and align to spline points.

	PollSplineUpdates();

	if (Settings->bResetAxisOnStart)
	{
		ResetAxis();
	}
	else
	{
		UpdateRevolutionAxis();
	}
}

void URevolveSplineTool::ResetAxis()
{
	if (!Spline.IsValid())
	{
		return;
	}
	int32 NumSplinePoints = Spline->GetNumberOfSplinePoints();
	if (NumSplinePoints == 0)
	{
		return;
	}
	Settings->AxisOrigin = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);

	// Our axis is the X axis of the frame, and we align it to Last-First
	FVector3d PlaneX = Spline->GetLocationAtSplinePoint(NumSplinePoints-1, ESplineCoordinateSpace::World) - Settings->AxisOrigin;
	PlaneX.Normalize();
	FFrame3d PlaneFrame(Settings->AxisOrigin, SplineFitPlaneNormal);
	if (!PlaneX.IsZero())
	{
		FVector3d PlaneY = SplineFitPlaneNormal.Cross(PlaneX);
		FVector3d PlaneZ = PlaneX.Cross(PlaneY);
		PlaneFrame = FFrame3d(Settings->AxisOrigin, PlaneX, PlaneY, PlaneZ);
	}
	FRotator AxisOrientation = ((FQuat)PlaneFrame.Rotation).Rotator();
	Settings->AxisOrientation.X = AxisOrientation.Pitch;
	Settings->AxisOrientation.Y = AxisOrientation.Yaw;

	NotifyOfPropertyChangeByTool(Settings);
	UpdateRevolutionAxis();
}

void URevolveSplineTool::PollSplineUpdates()
{
	if (bLostInputSpline)
	{
		return;
	}

	if (!Spline.IsValid())
	{
		// Attempt to recapture the spline.
		TArray<USplineComponent*> Splines;
		if (SplineOwningActor.IsValid())
		{
			SplineOwningActor->GetComponents<USplineComponent>(Splines);
		}
		if (SplineComponentIndex < Splines.Num() && ensure(SplineComponentIndex >= 0))
		{
			Spline = Splines[SplineComponentIndex];
		}
		else
		{
			GetToolManager()->DisplayMessage(
				LOCTEXT("LostSpline", "Tool lost reference to the input spline. The spline input will no longer be updated."),
				EToolMessageLevel::UserWarning);
			bLostInputSpline = true;
			return;
		}
	}

	if (Spline->SplineCurves.Version == LastSplineVersion && !bForceSplineUpdate)
	{
		// No update necessary
		return;
	}

	UpdatePointsFromSpline();

	LastSplineVersion = Spline->SplineCurves.Version;
	bForceSplineUpdate = false;
}

void URevolveSplineTool::UpdatePointsFromSpline()
{
	if (!Spline.IsValid())
	{
		return;
	}

	bProfileCurveIsClosed = Spline->IsClosedLoop();

	// Update the curve plane
	TArray<FVector> SplineControlPoints;
	int32 NumSplinePoints = Spline->GetNumberOfSplinePoints();
	for (int32 i = 0; i < NumSplinePoints; ++i)
	{
		SplineControlPoints.Add(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	}
	PolygonTriangulation::ComputePolygonPlane<double>(SplineControlPoints, SplineFitPlaneNormal, SplineFitPlaneOrigin);

	// Update the points we actually revolve
	ProfileCurve.Reset();
	switch (Settings->SampleMode)
	{
	case ERevolveSplineSampleMode::ControlPointsOnly:
	{
		ProfileCurve = SplineControlPoints;
	}
	break;
	case ERevolveSplineSampleMode::PolyLineMaxError:
	{
		Spline->ConvertSplineToPolyLine(ESplineCoordinateSpace::World,
			Settings->ErrorTolerance * Settings->ErrorTolerance,
			ProfileCurve);
	};
	break;
	case ERevolveSplineSampleMode::UniformSpacingAlongCurve:
	{
		double Length = Spline->GetSplineLength();
		int32 NumSegments = FMath::RoundFromZero(Length / FMath::Max(0.01, Settings->MaxSampleDistance));
		double SegmentLengthToUse = Length / NumSegments;
		for (int32 i = 0; i <= NumSegments; ++i)
		{
			double DistanceToSampleAt = Length * ((double)i / NumSegments);
			ProfileCurve.Add(Spline->GetLocationAtDistanceAlongSpline(DistanceToSampleAt, 
				ESplineCoordinateSpace::World));
		}
	}
	break;
	}

	Preview->InvalidateResult();
}

void URevolveSplineTool::OnTick(float DeltaTime)
{
	PollSplineUpdates();

	if (PlaneMechanic)
	{
		PlaneMechanic->Tick(DeltaTime);
	}

	if (Preview)
	{
		Preview->Tick(DeltaTime);
	}
}

void URevolveSplineTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	FViewCameraState CameraState;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(CameraState);

	if (PlaneMechanic)
	{
		PlaneMechanic->Render(RenderAPI);

		// Draw the axis of rotation
		float PdiScale = CameraState.GetPDIScalingFactor();
		FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

		FColor AxisColor(240, 16, 240);
		double AxisThickness = 1 * PdiScale;
		double AxisHalfLength = ToolSceneQueriesUtil::CalculateDimensionFromVisualAngleD(CameraState, RevolutionAxisOrigin, 90);

		FVector3d StartPoint = RevolutionAxisOrigin - (RevolutionAxisDirection * (AxisHalfLength * PdiScale));
		FVector3d EndPoint = RevolutionAxisOrigin + (RevolutionAxisDirection * (AxisHalfLength * PdiScale));

		PDI->DrawLine((FVector)StartPoint, (FVector)EndPoint, AxisColor, SDPG_Foreground,
			AxisThickness, 0.0f, true);
	}
}

void URevolveSplineTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == OutputTypeProperties)
	{
		return;
	}

	if (Property)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNewMeshMaterialProperties, Material))
		{
			Preview->ConfigureMaterials(MaterialProperties->Material.Get(),
				ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager()));
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URevolveSplineToolProperties, SampleMode)
			|| Property->GetFName() == GET_MEMBER_NAME_CHECKED(URevolveSplineToolProperties, ErrorTolerance)
			|| Property->GetFName() == GET_MEMBER_NAME_CHECKED(URevolveSplineToolProperties, MaxSampleDistance))
		{
			UpdatePointsFromSpline();
		}

		// Checking the name for these settings doesn't work, since the reported names are the low level components, like "X" or "Y"
		// So we'll simply update the axis whenever any property changes. It's overkill but probably not too bad.
		PlaneMechanic->SetPlaneWithoutBroadcast(FFrame3d(Settings->AxisOrigin,
			FRotator(Settings->AxisOrientation.X, Settings->AxisOrientation.Y, 0).Quaternion()));
		UpdateRevolutionAxis();		
	}

	Preview->PreviewMesh->EnableWireframe(MaterialProperties->bShowWireframe);

	Preview->InvalidateResult();
}

void URevolveSplineTool::Shutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	OutputTypeProperties->SaveProperties(this);
	MaterialProperties->SaveProperties(this);

	FDynamicMeshOpResult Result = Preview->Shutdown();

	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("RevolveSplineAction", "Revolve Spline"));

		// Generate the result asset
		GenerateAsset(Result);

		GetToolManager()->EndUndoTransaction();
	}

	PlaneMechanic->Shutdown();

	Preview = nullptr;
	Settings = nullptr;
	MaterialProperties = nullptr;
	OutputTypeProperties = nullptr;
	ToolActions = nullptr;
	PlaneMechanic = nullptr;

	Super::Shutdown(ShutdownType);
}

void URevolveSplineTool::GenerateAsset(const FDynamicMeshOpResult& OpResult)
{
	if (OpResult.Mesh.Get() == nullptr) return;

	if (!ensure(OpResult.Mesh.Get() && OpResult.Mesh->TriangleCount() > 0))
	{
		return;
	}

	GetToolManager()->BeginUndoTransaction(LOCTEXT("RevolveSplineToolTransactionName", "Revolve Spline"));

	FCreateMeshObjectParams NewMeshObjectParams;
	NewMeshObjectParams.TargetWorld = GetTargetWorld();
	NewMeshObjectParams.Transform = (FTransform)OpResult.Transform;
	NewMeshObjectParams.BaseName = TEXT("RevolveSpline");
	NewMeshObjectParams.Materials.Add(MaterialProperties->Material.Get());
	NewMeshObjectParams.SetMesh(OpResult.Mesh.Get());
	OutputTypeProperties->ConfigureCreateMeshObjectParams(NewMeshObjectParams);
	FCreateMeshObjectResult Result = UE::Modeling::CreateMeshObject(GetToolManager(), MoveTemp(NewMeshObjectParams));
	if (Result.IsOK() && Result.NewActor != nullptr)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), Result.NewActor);
	}

	GetToolManager()->EndUndoTransaction();
}

bool URevolveSplineTool::CanAccept() const
{
	return Preview->HaveValidNonEmptyResult();
}

TUniquePtr<FDynamicMeshOperator> URevolveSplineTool::MakeNewOperator()
{
	TUniquePtr<FCurveSweepOp> CurveSweepOp = MakeUnique<FCurveSweepOp>();

	// Assemble profile curve
	CurveSweepOp->ProfileCurve = ProfileCurve;
	CurveSweepOp->bProfileCurveIsClosed = bProfileCurveIsClosed;

	// If we are capping the top and bottom, we just add a couple extra vertices and mark the curve as being closed
	if (!bProfileCurveIsClosed && Settings->bClosePathToAxis && ProfileCurve.Num() > 1)
	{
		// Project first and last points onto the revolution axis.
		FVector3d FirstPoint = CurveSweepOp->ProfileCurve[0];
		FVector3d LastPoint = CurveSweepOp->ProfileCurve.Last();
		double DistanceAlongAxis = RevolutionAxisDirection.Dot(LastPoint - RevolutionAxisOrigin);
		FVector3d ProjectedPoint = RevolutionAxisOrigin + (RevolutionAxisDirection * DistanceAlongAxis);

		CurveSweepOp->ProfileCurve.Add(ProjectedPoint);

		DistanceAlongAxis = RevolutionAxisDirection.Dot(FirstPoint - RevolutionAxisOrigin);
		ProjectedPoint = RevolutionAxisOrigin + (RevolutionAxisDirection * DistanceAlongAxis);

		CurveSweepOp->ProfileCurve.Add(ProjectedPoint);
		CurveSweepOp->bProfileCurveIsClosed = true;
	}

	Settings->ApplyToCurveSweepOp(*MaterialProperties,
		RevolutionAxisOrigin, RevolutionAxisDirection, *CurveSweepOp);

	return CurveSweepOp;
}


//Uses the settings stored in the properties object to update the revolution axis
void URevolveSplineTool::UpdateRevolutionAxis()
{
	RevolutionAxisOrigin = (FVector3d)Settings->AxisOrigin;
	
	FRotator Rotator(Settings->AxisOrientation.X, Settings->AxisOrientation.Y, 0);
	RevolutionAxisDirection = (FVector3d)Rotator.RotateVector(FVector(1, 0, 0));

	PlaneMechanic->SetPlaneWithoutBroadcast(FFrame3d(Settings->AxisOrigin, Rotator.Quaternion()));

	if (Preview)
	{
		Preview->InvalidateResult();
	}
}

// To be called by builder
void URevolveSplineTool::SetSpline(USplineComponent* SplineComponent)
{
	Spline = SplineComponent;
	SplineOwningActor = Spline->GetOwner();
	bForceSplineUpdate = true;

	TArray<USplineComponent*> Splines;
	SplineOwningActor->GetComponents<USplineComponent>(Splines);

	SplineComponentIndex = Splines.IndexOfByKey(Spline.Get());
}

void URevolveSplineTool::RequestAction(ERevolveSplineToolAction Action)
{
	if (Action == ERevolveSplineToolAction::ResetAxis)
	{
		ResetAxis();
	}
}


/// Tool builder:

bool URevolveSplineToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	int32 NumSplines = ToolBuilderUtil::CountComponents(SceneState, [&](UActorComponent* Object) -> bool
	{
		return Object->IsA<USplineComponent>();
	});
	return NumSplines == 1;
}

UInteractiveTool* URevolveSplineToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URevolveSplineTool* NewTool = NewObject<URevolveSplineTool>(SceneState.ToolManager);
	USplineComponent* Spline = Cast<USplineComponent>(ToolBuilderUtil::FindFirstComponent(SceneState, [&](UActorComponent* Object)
	{
		return Object->IsA<USplineComponent>();
	}));

	ensure(Spline);
	NewTool->SetSpline(Spline);

	NewTool->SetWorld(SceneState.World);

	return NewTool;
}

// Action set:

void URevolveSplineToolActionPropertySet::PostAction(ERevolveSplineToolAction Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}


#undef LOCTEXT_NAMESPACE
