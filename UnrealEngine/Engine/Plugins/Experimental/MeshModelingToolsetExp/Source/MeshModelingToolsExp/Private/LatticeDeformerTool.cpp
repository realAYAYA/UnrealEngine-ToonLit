// Copyright Epic Games, Inc. All Rights Reserved.

#include "LatticeDeformerTool.h"

#include "Mechanics/LatticeControlPointsMechanic.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "DeformationOps/LatticeDeformerOp.h"
#include "Properties/MeshMaterialProperties.h"
#include "Selection/ToolSelectionUtil.h"
#include "MeshOpPreviewHelpers.h" //FDynamicMeshOpResult
#include "ToolSceneQueriesUtil.h"
#include "ToolBuilderUtil.h"
#include "ToolSetupUtil.h"
#include "DynamicMeshToMeshDescription.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Algo/ForEach.h"
#include "Operations/FFDLattice.h"

#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ModelingToolTargetUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LatticeDeformerTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "ULatticeDeformerTool"


namespace
{
	void MakeLatticeGraph(const FFFDLattice& Lattice, FDynamicGraph3d& Graph)
	{
		const FVector3i& Dims = Lattice.GetDimensions();
		const FVector3d& CellSize = Lattice.GetCellSize();
		const FAxisAlignedBox3d& InitialBounds = Lattice.GetInitialBounds();

		// Add cell corners as vertices

		for (int i = 0; i < Dims.X; ++i)
		{
			const double X = CellSize.X * i;
			for (int j = 0; j < Dims.Y; ++j)
			{
				const double Y = CellSize.Y * j;
				for (int k = 0; k < Dims.Z; ++k)
				{
					const double Z = CellSize.Z * k;

					const FVector3d Position = InitialBounds.Min + FVector3d{ X,Y,Z };
					const int P = Lattice.ControlPointIndexFromCoordinates(i, j, k);
					const int VID = Graph.AppendVertex(Position);
					ensure(VID == P);
				}
			}
		}

		// Connect cell corners with edges

		for (int i = 0; i < Dims.X; ++i)
		{
			for (int j = 0; j < Dims.Y; ++j)
			{
				for (int k = 0; k < Dims.Z; ++k)
				{
					const int P = Lattice.ControlPointIndexFromCoordinates(i, j, k);
					if (i + 1 < Dims.X)
					{
						const int Pi = Lattice.ControlPointIndexFromCoordinates(i + 1, j, k);
						Graph.AppendEdge(P, Pi);
					}

					if (j + 1 < Dims.Y)
					{
						const int Pj = Lattice.ControlPointIndexFromCoordinates(i, j + 1, k);
						Graph.AppendEdge(P, Pj);
					}

					if (k + 1 < Dims.Z)
					{
						const int Pk = Lattice.ControlPointIndexFromCoordinates(i, j, k + 1);
						Graph.AppendEdge(P, Pk);
					}
				}
			}
		}
	}
}

// Tool properties/actions

void ULatticeDeformerToolProperties::PostAction(ELatticeDeformerToolAction Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}


// Tool builder

USingleSelectionMeshEditingTool* ULatticeDeformerToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<ULatticeDeformerTool>(SceneState.ToolManager);
}


// Operator factory

TUniquePtr<FDynamicMeshOperator> ULatticeDeformerOperatorFactory::MakeNewOperator()
{
	ELatticeInterpolation OpInterpolationType =
		(LatticeDeformerTool->Settings->InterpolationType == ELatticeInterpolationType::Cubic) ?
		ELatticeInterpolation::Cubic :
		ELatticeInterpolation::Linear;

	TUniquePtr<FLatticeDeformerOp> LatticeDeformOp = MakeUnique<FLatticeDeformerOp>(
		LatticeDeformerTool->OriginalMesh,
		LatticeDeformerTool->Lattice,
		LatticeDeformerTool->ControlPointsMechanic->GetControlPoints(),
		OpInterpolationType,
		LatticeDeformerTool->Settings->bDeformNormals);

	return LatticeDeformOp;
}


// Tool itself

FVector3i ULatticeDeformerTool::GetLatticeResolution() const
{
	return FVector3i{ Settings->XAxisResolution, Settings->YAxisResolution, Settings->ZAxisResolution };
}

void ULatticeDeformerTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	ControlPointsMechanic->DrawHUD(Canvas, RenderAPI);
}

bool ULatticeDeformerTool::CanAccept() const
{
	return Preview != nullptr && Preview->HaveValidResult();
}

void ULatticeDeformerTool::InitializeLattice(TArray<FVector3d>& OutLatticePoints, TArray<FVector2i>& OutLatticeEdges)
{
	Lattice = MakeShared<FFFDLattice, ESPMode::ThreadSafe>(GetLatticeResolution(), *OriginalMesh, Settings->Padding);

	Lattice->GenerateInitialLatticePositions(OutLatticePoints);

	// Put the lattice in world space
	FTransform3d LocalToWorld(Cast<IPrimitiveComponentBackedTarget>(Target)->GetWorldTransform());
	Algo::ForEach(OutLatticePoints, [&LocalToWorld](FVector3d& Point) {
		Point = LocalToWorld.TransformPosition(Point);
	});

	Lattice->GenerateLatticeEdges(OutLatticeEdges);
}

void ULatticeDeformerTool::Setup()
{
	UInteractiveTool::Setup();

	SetToolDisplayName(LOCTEXT("ToolName", "Lattice Deform"));
	GetToolManager()->DisplayMessage(LOCTEXT("LatticeDeformerToolMessage", 
		"Drag the lattice control points to deform the mesh"), EToolMessageLevel::UserNotification);

	OriginalMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(UE::ToolTarget::GetMeshDescription(Target), *OriginalMesh);

	// Note: Mesh will be implicitly transformed to world space by transforming the lattice; we account for whether that would invert the mesh here
	MeshTransforms::ReverseOrientationIfNeeded(*OriginalMesh, (Cast<IPrimitiveComponentBackedTarget>(Target)->GetWorldTransform()));

	Settings = NewObject<ULatticeDeformerToolProperties>(this, TEXT("Lattice Deformer Tool Settings"));
	Settings->Initialize(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	// Watch for property changes
	Settings->WatchProperty(Settings->XAxisResolution, [this](int) { bShouldRebuild = true; });
	Settings->WatchProperty(Settings->YAxisResolution, [this](int) { bShouldRebuild = true; });
	Settings->WatchProperty(Settings->ZAxisResolution, [this](int) { bShouldRebuild = true; });
	Settings->WatchProperty(Settings->Padding, [this](float) { bShouldRebuild = true; });
	Settings->WatchProperty(Settings->InterpolationType, [this](ELatticeInterpolationType)
	{
		Preview->InvalidateResult();
	});
	Settings->WatchProperty(Settings->bDeformNormals, [this](bool)
	{
		Preview->InvalidateResult();
	});
	Settings->WatchProperty(Settings->GizmoCoordinateSystem, [this](EToolContextCoordinateSystem)
	{
		ControlPointsMechanic->SetCoordinateSystem(Settings->GizmoCoordinateSystem);
	});
	Settings->WatchProperty(Settings->bSetPivotMode, [this](bool)
	{
		ControlPointsMechanic->UpdateSetPivotMode(Settings->bSetPivotMode);
	});
	Settings->WatchProperty(Settings->bSoftDeformation, [this](bool)
	{
		if (Settings->bSoftDeformation)
		{
			RebuildDeformer();
		}
	});


	TArray<FVector3d> LatticePoints;
	TArray<FVector2i> LatticeEdges;
	InitializeLattice(LatticePoints, LatticeEdges);

	// Set up control points mechanic
	ControlPointsMechanic = NewObject<ULatticeControlPointsMechanic>(this);
	ControlPointsMechanic->Setup(this);
	ControlPointsMechanic->SetWorld(GetTargetWorld());
	FTransform3d LocalToWorld(Cast<IPrimitiveComponentBackedTarget>(Target)->GetWorldTransform());
	ControlPointsMechanic->Initialize(LatticePoints, LatticeEdges, LocalToWorld);

	auto OnPointsChangedLambda = [this]()
	{
		if (Settings->bSoftDeformation)
		{
			SoftDeformLattice();
		}
		ResetConstrainedPoints();
		Preview->InvalidateResult();
		Settings->bCanChangeResolution = !ControlPointsMechanic->bHasChanged;
	};
	ControlPointsMechanic->OnPointsChanged.AddLambda(OnPointsChangedLambda);

	ControlPointsMechanic->OnSelectionChanged.AddLambda([this]()
	{
		if (Settings->bSoftDeformation)
		{
			RebuildDeformer();
		}
	});

	ControlPointsMechanic->SetCoordinateSystem(Settings->GizmoCoordinateSystem);
	ControlPointsMechanic->UpdateSetPivotMode(Settings->bSetPivotMode);


	ControlPointsMechanic->ShouldHideGizmo = ULatticeControlPointsMechanic::FShouldHideGizmo::CreateLambda([this]()->bool
	{
		for (int32 VID : ControlPointsMechanic->GetSelectedPointIDs())
		{
			if (!ConstrainedLatticePoints.Contains(VID))
			{
				return false;	// found a selected point that is not constrained
			}
		}
		return true;
	});


	StartPreview();
}


void ULatticeDeformerTool::RebuildDeformer()
{
	LatticeGraph = MakePimpl<UE::Geometry::FDynamicGraph3d>();
	MakeLatticeGraph(*Lattice, *LatticeGraph);

	const TArray<FVector3d>& CurrentLatticePoints = ControlPointsMechanic->GetControlPoints();
	check(LatticeGraph->VertexCount() == CurrentLatticePoints.Num());

	for (int VID : LatticeGraph->VertexIndicesItr())
	{
		LatticeGraph->SetVertex(VID, CurrentLatticePoints[VID]);
	}

	DeformationSolver = UE::MeshDeformation::ConstructUniformConstrainedMeshDeformer(*LatticeGraph);

	for (int LatticePointIndex = 0; LatticePointIndex < CurrentLatticePoints.Num(); ++LatticePointIndex)
	{
		if(ConstrainedLatticePoints.Contains(LatticePointIndex))
		{
			// Pin constraint
			DeformationSolver->AddConstraint(LatticePointIndex, 1.0, ConstrainedLatticePoints[LatticePointIndex], true);
		}
		else 
		{
			if (ControlPointsMechanic->ControlPointIsSelected(LatticePointIndex))
			{
				const FVector3d& MovePosition = CurrentLatticePoints[LatticePointIndex];
				DeformationSolver->AddConstraint(LatticePointIndex, 1.0, MovePosition, true);
			}
		}
	}
}


void ULatticeDeformerTool::ResetConstrainedPoints()
{
	ControlPointsMechanic->UpdatePointLocations(ConstrainedLatticePoints);
}

void ULatticeDeformerTool::SoftDeformLattice()
{
	if (!ensure(Lattice))
	{
		return;
	}

	if (!ensure(ControlPointsMechanic))
	{
		return;
	}

	if (!ensure(DeformationSolver))
	{
		return;
	}
	
	const TArray<FVector3d>& CurrentLatticePoints = ControlPointsMechanic->GetControlPoints();

	if (!ensure(LatticeGraph->VertexCount() == CurrentLatticePoints.Num()))
	{
		return;
	}

	for (int LatticePointIndex = 0; LatticePointIndex < CurrentLatticePoints.Num(); ++LatticePointIndex)
	{
		if (ControlPointsMechanic->ControlPointIsSelected(LatticePointIndex))
		{
			// Don't move pinned points
			if (ConstrainedLatticePoints.Contains(LatticePointIndex))
			{
				continue;
			}

			if (!ensure(DeformationSolver->IsConstrained(LatticePointIndex)))
			{
				continue;
			}

			const FVector3d& MovePosition = CurrentLatticePoints[LatticePointIndex];
			DeformationSolver->UpdateConstraintPosition(LatticePointIndex, MovePosition, true);
		}
	}

	TArray<FVector3d> DeformedLatticePoints;
	DeformationSolver->Deform(DeformedLatticePoints);

	ControlPointsMechanic->UpdateControlPointPositions(DeformedLatticePoints);
}


void ULatticeDeformerTool::OnShutdown(EToolShutdownType ShutdownType)
{
	Settings->SaveProperties(this);
	ControlPointsMechanic->Shutdown();

	IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	TargetComponent->SetOwnerVisibility(true);

	if (Preview)
	{
		FDynamicMeshOpResult Result = Preview->Shutdown();

		if (ShutdownType == EToolShutdownType::Accept)
		{
			GetToolManager()->BeginUndoTransaction(LOCTEXT("LatticeDeformerTool", "Lattice Deformer"));

			FDynamicMesh3* DynamicMeshResult = Result.Mesh.Get();
			check(DynamicMeshResult != nullptr);

			// The lattice and its output mesh are in world space, so get them in local space.
			// TODO: Would it make more sense to do all the lattice computation in local space?
			FTransform3d LocalToWorld(TargetComponent->GetWorldTransform());
			MeshTransforms::ApplyTransformInverse(*DynamicMeshResult, LocalToWorld, true);

			UE::ToolTarget::CommitMeshDescriptionUpdateViaDynamicMesh(Target, *DynamicMeshResult, true);

			GetToolManager()->EndUndoTransaction();
		}
	}
}


void ULatticeDeformerTool::StartPreview()
{
	ULatticeDeformerOperatorFactory* LatticeDeformOpCreator = NewObject<ULatticeDeformerOperatorFactory>();
	LatticeDeformOpCreator->LatticeDeformerTool = this;

	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>(LatticeDeformOpCreator);
	Preview->Setup(GetTargetWorld(), LatticeDeformOpCreator);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, Target);

	Preview->SetIsMeshTopologyConstant(true, EMeshRenderAttributeFlags::Positions | EMeshRenderAttributeFlags::VertexNormals);

	FComponentMaterialSet MaterialSet;
	Cast<IMaterialProvider>(Target)->GetMaterialSet(MaterialSet);
	Preview->ConfigureMaterials(MaterialSet.Materials,
								ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager())
	);

	// configure secondary render material
	UMaterialInterface* SelectionMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor(0.8f, 0.75f, 0.0f), GetToolManager());
	if (SelectionMaterial != nullptr)
	{
		Preview->PreviewMesh->SetSecondaryRenderMaterial(SelectionMaterial);
	}

	Preview->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::NoTangents);
	Preview->SetVisibility(true);
	Preview->InvalidateResult();

	Cast<IPrimitiveComponentBackedTarget>(Target)->SetOwnerVisibility(false);
}


void ULatticeDeformerTool::ApplyAction(ELatticeDeformerToolAction Action)
{
	switch (Action)
	{
	case ELatticeDeformerToolAction::ClearConstraints:
		ClearConstrainedPoints();
		break;
	case ELatticeDeformerToolAction::Constrain:
		ConstrainSelectedPoints();
		break;
	default:
		break;
	}
}


void ULatticeDeformerTool::OnTick(float DeltaTime)
{
	if (PendingAction != ELatticeDeformerToolAction::NoAction)
	{
		ApplyAction(PendingAction);
		PendingAction = ELatticeDeformerToolAction::NoAction;
	}

	if (Preview)
	{
		if (bShouldRebuild)
		{
			ClearConstrainedPoints();
			TArray<FVector3d> LatticePoints;
			TArray<FVector2i> LatticeEdges;
			InitializeLattice(LatticePoints, LatticeEdges);
			FTransform3d LocalToWorld(Cast<IPrimitiveComponentBackedTarget>(Target)->GetWorldTransform());
			ControlPointsMechanic->Initialize(LatticePoints, LatticeEdges, LocalToWorld);
			Preview->InvalidateResult();
			bShouldRebuild = false;
		}

		Preview->Tick(DeltaTime);
	}
}


void ULatticeDeformerTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (ControlPointsMechanic != nullptr)
	{
		ControlPointsMechanic->Render(RenderAPI);
	}
}

void ULatticeDeformerTool::RequestAction(ELatticeDeformerToolAction Action)
{
	if (PendingAction == ELatticeDeformerToolAction::NoAction)
	{
		PendingAction = Action;
	}
}


static const FText LatticeConstraintChangeTransactionText = LOCTEXT("LatticeConstraintChange", "Lattice Constraint Change");

void ULatticeDeformerTool::ConstrainSelectedPoints()
{
	TMap<int, FVector3d> PrevConstrainedLatticePoints = ConstrainedLatticePoints;
	const TArray<FVector3d>& CurrentControlPointPositions = ControlPointsMechanic->GetControlPoints();
	for (int32 VID : ControlPointsMechanic->GetSelectedPointIDs())
	{
		ConstrainedLatticePoints.FindOrAdd(VID) = CurrentControlPointPositions[VID];
	}
	UpdateMechanicColorOverrides();

	GetToolManager()->EmitObjectChange(this, MakeUnique<FLatticeDeformerToolConstrainedPointsChange>(PrevConstrainedLatticePoints,
																									 ConstrainedLatticePoints, 
																									 CurrentChangeStamp), 
									   LatticeConstraintChangeTransactionText);
}

void ULatticeDeformerTool::ClearConstrainedPoints()
{
	TMap<int, FVector3d> PrevConstrainedLatticePoints = ConstrainedLatticePoints;
	ConstrainedLatticePoints.Reset();
	UpdateMechanicColorOverrides();

	GetToolManager()->EmitObjectChange(this, MakeUnique<FLatticeDeformerToolConstrainedPointsChange>(PrevConstrainedLatticePoints,
																									 ConstrainedLatticePoints,
																									 CurrentChangeStamp),
									   LatticeConstraintChangeTransactionText);
}


void ULatticeDeformerTool::UpdateMechanicColorOverrides()
{
	ControlPointsMechanic->ClearAllPointColorOverrides();
	for ( const TPair<int32,FVector3d>& Constraint : ConstrainedLatticePoints)
	{
		ControlPointsMechanic->SetPointColorOverride(Constraint.Key, FColor::Cyan);
	}
	RebuildDeformer();
	ControlPointsMechanic->UpdateDrawables();
}



void FLatticeDeformerToolConstrainedPointsChange::Apply(UObject* Object)
{
	ULatticeDeformerTool* Tool = Cast<ULatticeDeformerTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}

	Tool->ConstrainedLatticePoints = NewConstrainedLatticePoints;
	Tool->UpdateMechanicColorOverrides();
}

void FLatticeDeformerToolConstrainedPointsChange::Revert(UObject* Object)
{
	ULatticeDeformerTool* Tool = Cast<ULatticeDeformerTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}

	Tool->ConstrainedLatticePoints = PrevConstrainedLatticePoints;
	Tool->UpdateMechanicColorOverrides();
}

FString FLatticeDeformerToolConstrainedPointsChange::ToString() const
{
	return TEXT("FLatticeDeformerToolConstrainedPointsChange");
}


#undef LOCTEXT_NAMESPACE

