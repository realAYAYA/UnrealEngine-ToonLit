// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothWeightMapPaintTool.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "InteractiveGizmoManager.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"
#include "MeshWeights.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "Util/BufferUtil.h"
#include "Util/ColorConstants.h"
#include "Selections/MeshConnectedComponents.h"
#include "Selections/MeshFaceSelection.h"
#include "Selections/MeshVertexSelection.h"
#include "Polygon2.h"
#include "Intersection/IntrLine2Line2.h"
#include "Intersection/IntrSegment2Segment2.h"

#include "Changes/MeshVertexChange.h"
#include "Changes/BasicChanges.h"

#include "ChaosClothAsset/ClothWeightMapPaintBrushOps.h"
#include "Sculpting/StampFalloffs.h"
#include "Sculpting/MeshSculptUtil.h"

#include "CanvasTypes.h"
#include "CanvasItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothWeightMapPaintTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UClothEditorWeightMapPaintTool"

namespace
{
	// probably should be something defined for the whole tool framework...
#if WITH_EDITOR
	static EAsyncExecution WeightPaintToolAsyncExecTarget = EAsyncExecution::LargeThreadPool;
#else
	static EAsyncExecution WeightPaintToolAsyncExecTarget = EAsyncExecution::ThreadPool;
#endif
}


/*
 * ToolBuilder
 */
UMeshSurfacePointTool* UClothEditorWeightMapPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UClothEditorWeightMapPaintTool* SculptTool = NewObject<UClothEditorWeightMapPaintTool>(SceneState.ToolManager);
	SculptTool->SetWorld(SceneState.World);
	return SculptTool;
}


/*
 * Properties
 */
void UClothEditorWeightMapPaintToolActionPropertySet::PostAction(EClothEditorWeightMapPaintToolActions Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}





/*
 * Tool
 */

void UClothEditorWeightMapPaintTool::Setup()
{
	UMeshSculptToolBase::Setup();

	SetToolDisplayName(LOCTEXT("ToolName", "Paint Weight Maps"));

	// create dynamic mesh component to use for live preview
	FActorSpawnParameters SpawnInfo;
	PreviewMeshActor = TargetWorld->SpawnActor<AInternalToolFrameworkActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	DynamicMeshComponent = NewObject<UDynamicMeshComponent>(PreviewMeshActor);
	InitializeSculptMeshComponent(DynamicMeshComponent, PreviewMeshActor);

	// assign materials
	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	for (int k = 0; k < MaterialSet.Materials.Num(); ++k)
	{
		DynamicMeshComponent->SetMaterial(k, MaterialSet.Materials[k]);
	}

	DynamicMeshComponent->SetInvalidateProxyOnChangeEnabled(false);
	OnDynamicMeshComponentChangedHandle = DynamicMeshComponent->OnMeshVerticesChanged.AddUObject(this, &UClothEditorWeightMapPaintTool::OnDynamicMeshComponentChanged);

	FDynamicMesh3* Mesh = GetSculptMesh();
	Mesh->EnableVertexColors(FVector3f::One());
	FAxisAlignedBox3d Bounds = Mesh->GetBounds(true);

	TFuture<void> PrecomputeFuture = Async(WeightPaintToolAsyncExecTarget, [&]()
	{
		PrecomputeFilterData();
	});

	TFuture<void> OctreeFuture = Async(WeightPaintToolAsyncExecTarget, [&]()
	{
		// initialize dynamic octree
		if (Mesh->TriangleCount() > 100000)
		{
			Octree.RootDimension = Bounds.MaxDim() / 10.0;
			Octree.SetMaxTreeDepth(4);
		}
		else
		{
			Octree.RootDimension = Bounds.MaxDim();
			Octree.SetMaxTreeDepth(8);
		}
		Octree.Initialize(Mesh);
	});

	// initialize render decomposition
	TUniquePtr<FMeshRenderDecomposition> Decomp = MakeUnique<FMeshRenderDecomposition>();
	FMeshRenderDecomposition::BuildChunkedDecomposition(Mesh, &MaterialSet, *Decomp);
	Decomp->BuildAssociations(Mesh);
	DynamicMeshComponent->SetExternalDecomposition(MoveTemp(Decomp));

	// initialize brush radius range interval, brush properties
	UMeshSculptToolBase::InitializeBrushSizeRange(Bounds);

	// Set up control points mechanic
	PolyLassoMechanic = NewObject<UPolyLassoMarqueeMechanic>(this);
	PolyLassoMechanic->Setup(this);
	PolyLassoMechanic->SetIsEnabled(false);
	PolyLassoMechanic->SpacingTolerance = 10.0f;
	PolyLassoMechanic->OnDrawPolyLassoFinished.AddUObject(this, &UClothEditorWeightMapPaintTool::OnPolyLassoFinished);

	WeightMapSetProperties = NewObject<UWeightMapSetProperties>(this);
	WeightMapSetProperties->RestoreProperties(this, TEXT("ClothEditorWeightMapPaintTool"));

	InitializeWeightMapNames();

	WeightMapSetProperties->WatchProperty(WeightMapSetProperties->WeightMap, [&](FName) { OnSelectedWeightMapChanged(); });
	UpdateActiveWeightMap();
	AddToolPropertySource(WeightMapSetProperties);

	ClothEditorWeightMapActions = NewObject<UClothEditorWeightMapActions>(this);
	ClothEditorWeightMapActions->Initialize(this);
	AddToolPropertySource(ClothEditorWeightMapActions);

	// initialize other properties
	FilterProperties = NewObject<UClothEditorWeightMapPaintBrushFilterProperties>(this);
	FilterProperties->WatchProperty(FilterProperties->SubToolType,
		[this](EClothEditorWeightMapPaintInteractionType NewType) { UpdateSubToolType(NewType); });
	FilterProperties->WatchProperty(FilterProperties->BrushSize,
		[this](float NewSize) { UMeshSculptToolBase::BrushProperties->BrushSize.AdaptiveSize = NewSize; });
	FilterProperties->RestoreProperties(this);
	FilterProperties->BrushSize = UMeshSculptToolBase::BrushProperties->BrushSize.AdaptiveSize;
	FilterProperties->StrengthValue = 1.0;
	AddToolPropertySource(FilterProperties);

	InitializeIndicator();

	// initialize our properties
	AddToolPropertySource(UMeshSculptToolBase::BrushProperties);
	UMeshSculptToolBase::BrushProperties->bShowPerBrushProps = false;
	UMeshSculptToolBase::BrushProperties->bShowFalloff = true;
	UMeshSculptToolBase::BrushProperties->bShowLazyness = false;
	CalculateBrushRadius();

	PaintBrushOpProperties = NewObject<UWeightMapPaintBrushOpProps>(this);
	RegisterBrushType((int32)EClothEditorWeightMapPaintBrushType::Paint, LOCTEXT("Paint", "Paint"),
		MakeUnique<FLambdaMeshSculptBrushOpFactory>([this]() { return MakeUnique<FWeightMapPaintBrushOp>(); }),
		PaintBrushOpProperties);

	// secondary brushes
	EraseBrushOpProperties = NewObject<UWeightMapEraseBrushOpProps>(this);

	RegisterSecondaryBrushType((int32)EClothEditorWeightMapPaintBrushType::Erase, LOCTEXT("Erase", "Erase"),
		MakeUnique<TBasicMeshSculptBrushOpFactory<FWeightMapEraseBrushOp>>(),
		EraseBrushOpProperties);

	AddToolPropertySource(UMeshSculptToolBase::ViewProperties);

	AddToolPropertySource(UMeshSculptToolBase::GizmoProperties);
	SetToolPropertySourceEnabled(UMeshSculptToolBase::GizmoProperties, false);


	// register watchers
	FilterProperties->WatchProperty( FilterProperties->PrimaryBrushType,
		[this](EClothEditorWeightMapPaintBrushType NewType) { UpdateBrushType(NewType); });

	// must call before updating brush type so that we register all brush properties?
	UMeshSculptToolBase::OnCompleteSetup();

	UpdateBrushType(FilterProperties->PrimaryBrushType);
	SetActiveSecondaryBrushType((int32)EClothEditorWeightMapPaintBrushType::Erase);
	
	ActionsProps = NewObject<UMeshWeightMapPaintToolActions>(this);
	ActionsProps->Initialize(this);
	AddToolPropertySource(ActionsProps);

	MeshElementsDisplay = NewObject<UMeshElementsVisualizer>(this);
	MeshElementsDisplay->CreateInWorld(DynamicMeshComponent->GetWorld(), DynamicMeshComponent->GetComponentTransform());
	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->RestoreProperties(this, TEXT("ClothEditorWeightMapPaintTool"));
		AddToolPropertySource(MeshElementsDisplay->Settings);
	}
	MeshElementsDisplay->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) {
		ProcessFunc(*GetSculptMesh());
	});

	// force colors update... ?
	DynamicMeshComponent->SetTriangleColorFunction([this](const FDynamicMesh3* Mesh, int TriangleID)
	{
		if (ActiveWeightMap)
		{
			FIndex3i Vertices = Mesh->GetTriangle(TriangleID);
			FVector3f WeightPerVertex;
			ActiveWeightMap->GetValue(Vertices[0], &WeightPerVertex[0]);
			ActiveWeightMap->GetValue(Vertices[1], &WeightPerVertex[1]);
			ActiveWeightMap->GetValue(Vertices[2], &WeightPerVertex[2]);
			return GetColorForWeightValue(WeightPerVertex[0] / 3.0 + WeightPerVertex[1] / 3.0 + WeightPerVertex[2] / 3.0);
		}
		else
		{
			return LinearColors::Black3b();
		}
	});

	// disable view properties
	SetViewPropertiesEnabled(false);
	UpdateMaterialMode(EMeshEditingMaterialModes::VertexColor);
	UpdateWireframeVisibility(false);
	UpdateFlatShadingSetting(true);

	// configure panels
	UpdateSubToolType(FilterProperties->SubToolType);

	PrecomputeFuture.Wait();
	OctreeFuture.Wait();
}

void UClothEditorWeightMapPaintTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (DynamicMeshComponent != nullptr)
	{
		DynamicMeshComponent->OnMeshChanged.Remove(OnDynamicMeshComponentChangedHandle);
	}

	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->SaveProperties(this, TEXT("ClothEditorWeightMapPaintTool"));
	}
	MeshElementsDisplay->Disconnect();

	FilterProperties->SaveProperties(this);
	WeightMapSetProperties->SaveProperties(this, TEXT("ClothEditorWeightMapPaintTool"));

	if (PreviewMeshActor != nullptr)
	{
		PreviewMeshActor->Destroy();
		PreviewMeshActor = nullptr;
	}

	UMeshSculptToolBase::Shutdown(ShutdownType);
}


void UClothEditorWeightMapPaintTool::CommitResult(UBaseDynamicMeshComponent* Component, bool bModifiedTopology)
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("WeightPaintToolTransactionName", "Paint Weights"));
	Component->ProcessMesh([&](const FDynamicMesh3& CurMesh)
	{
		UE::ToolTarget::CommitDynamicMeshUpdate(Target, CurMesh, true);
	});
	GetToolManager()->EndUndoTransaction();
}


void UClothEditorWeightMapPaintTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	UMeshSculptToolBase::RegisterActions(ActionSet);
	
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 500,
		TEXT("PickWeightValueUnderCursor"),
		LOCTEXT("PickWeightValueUnderCursor", "Pick Weight Value"),
		LOCTEXT("PickWeightValueUnderCursorTooltip", "Set the active weight painting value to that currently under the cursor"),
		EModifierKey::Shift, EKeys::G,
		[this]() { bPendingPickWeight = true; });
};


TUniquePtr<FMeshSculptBrushOp>& UClothEditorWeightMapPaintTool::GetActiveBrushOp()
{
	if (GetInEraseStroke())
	{
		return SecondaryBrushOp;
	}
	else
	{
		return PrimaryBrushOp;
	}
}


void UClothEditorWeightMapPaintTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	CalculateBrushRadius();
}


void UClothEditorWeightMapPaintTool::IncreaseBrushRadiusAction()
{
	Super::IncreaseBrushRadiusAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UClothEditorWeightMapPaintTool::DecreaseBrushRadiusAction()
{
	Super::DecreaseBrushRadiusAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UClothEditorWeightMapPaintTool::IncreaseBrushRadiusSmallStepAction()
{
	Super::IncreaseBrushRadiusSmallStepAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UClothEditorWeightMapPaintTool::DecreaseBrushRadiusSmallStepAction()
{
	Super::DecreaseBrushRadiusSmallStepAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}


bool UClothEditorWeightMapPaintTool::IsInBrushSubMode() const
{
	return FilterProperties->SubToolType == EClothEditorWeightMapPaintInteractionType::Brush
		|| FilterProperties->SubToolType == EClothEditorWeightMapPaintInteractionType::Fill;
}


void UClothEditorWeightMapPaintTool::OnBeginStroke(const FRay& WorldRay)
{
	if (!ActiveWeightMap)
	{
		return;
	}

	UpdateBrushPosition(WorldRay);

	if (PaintBrushOpProperties)
	{
		PaintBrushOpProperties->AttributeValue = FilterProperties->StrengthValue;
	}
	if (EraseBrushOpProperties)
	{
		EraseBrushOpProperties->AttributeValue = FilterProperties->StrengthValue;
	}

	// initialize first "Last Stamp", so that we can assume all stamps in stroke have a valid previous stamp
	LastStamp.WorldFrame = GetBrushFrameWorld();
	LastStamp.LocalFrame = GetBrushFrameLocal();
	LastStamp.Radius = GetCurrentBrushRadius();
	LastStamp.Falloff = GetCurrentBrushFalloff();
	LastStamp.Direction = GetInInvertStroke() ? -1.0 : 1.0;
	LastStamp.Depth = GetCurrentBrushDepth();
	LastStamp.Power = GetActivePressure() * GetCurrentBrushStrength();
	LastStamp.TimeStamp = FDateTime::Now();

	FSculptBrushOptions SculptOptions;
	SculptOptions.ConstantReferencePlane = GetCurrentStrokeReferencePlane();

	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();
	UseBrushOp->ConfigureOptions(SculptOptions);
	UseBrushOp->BeginStroke(GetSculptMesh(), LastStamp, VertexROI);

	AccumulatedTriangleROI.Reset();

	// begin change here? or wait for first stamp?
	BeginChange();
}

void UClothEditorWeightMapPaintTool::OnEndStroke()
{
	if (!ActiveWeightMap)
	{
		return;
	}

	GetActiveBrushOp()->EndStroke(GetSculptMesh(), LastStamp, VertexROI);

	// close change record
	EndChange();
}




void UClothEditorWeightMapPaintTool::UpdateROI(const FSculptBrushStamp& BrushStamp)
{
	SCOPE_CYCLE_COUNTER(WeightMapPaintTool_UpdateROI);

	const FVector3d& BrushPos = BrushStamp.LocalFrame.Origin;
	const FDynamicMesh3* Mesh = GetSculptMesh();
	float RadiusSqr = GetCurrentBrushRadius() * GetCurrentBrushRadius();
	FAxisAlignedBox3d BrushBox(
		BrushPos - GetCurrentBrushRadius() * FVector3d::One(),
		BrushPos + GetCurrentBrushRadius() * FVector3d::One());

	TriangleROI.Reset();

	int32 CenterTID = GetBrushTriangleID();
	if (Mesh->IsTriangle(CenterTID))
	{
		TriangleROI.Add(CenterTID);
	}

	FVector3d CenterNormal = Mesh->IsTriangle(CenterTID) ? TriNormals[CenterTID] : FVector3d::One();		// One so that normal check always passes
	
	bool bUseAngleThreshold = FilterProperties->AngleThreshold < 180.0f;
	double DotAngleThreshold = FMathd::Cos(FilterProperties->AngleThreshold * FMathd::DegToRad);
	bool bStopAtUVSeams = FilterProperties->bUVSeams;
	bool bStopAtNormalSeams = FilterProperties->bNormalSeams;

	auto CheckEdgeCriteria = [&](int32 t1, int32 t2) -> bool
	{
		if (bUseAngleThreshold == false || CenterNormal.Dot(TriNormals[t2]) > DotAngleThreshold)
		{
			int32 eid = Mesh->FindEdgeFromTriPair(t1, t2);
			if (bStopAtUVSeams == false || UVSeamEdges[eid] == false)
			{
				if (bStopAtNormalSeams == false || NormalSeamEdges[eid] == false)
				{
					return true;
				}
			}
		}
		return false;
	};

	bool bFill = (FilterProperties->SubToolType == EClothEditorWeightMapPaintInteractionType::Fill);

	if (Mesh->IsTriangle(CenterTID))
	{
		TArray<int32> StartROI;
		StartROI.Add(CenterTID);
		FMeshConnectedComponents::GrowToConnectedTriangles(Mesh, StartROI, TriangleROI, &TempROIBuffer,
			[&](int t1, int t2) 
		{ 
			if ((Mesh->GetTriCentroid(t2) - BrushPos).SquaredLength() < RadiusSqr)
			{
				return CheckEdgeCriteria(t1, t2);
			}
			return false;
		});
	}

	if (bFill)
	{
		TArray<int32> StartROI;
		for (int32 tid : TriangleROI)
		{
			StartROI.Add(tid);
		}
		FMeshConnectedComponents::GrowToConnectedTriangles(Mesh, StartROI, TriangleROI, &TempROIBuffer,
														   [&](int t1, int t2)
		{
			return CheckEdgeCriteria(t1, t2);
		});
	}

	// construct ROI vertex set
	VertexSetBuffer.Reset();
	for (int32 tid : TriangleROI)
	{
		FIndex3i Tri = Mesh->GetTriangle(tid);
		VertexSetBuffer.Add(Tri.A);  VertexSetBuffer.Add(Tri.B);  VertexSetBuffer.Add(Tri.C);
	}
	
	// apply visibility filter
	if (FilterProperties->VisibilityFilter != EClothEditorWeightMapPaintVisibilityType::None)
	{
		TArray<int32> ResultBuffer;
		ApplyVisibilityFilter(VertexSetBuffer, TempROIBuffer, ResultBuffer);
	}

	VertexROI.SetNum(0, false);
	//TODO: If we paint a 2D projection of UVs, these will need to be the 2D vertices not the 3D original mesh vertices
	BufferUtil::AppendElements(VertexROI, VertexSetBuffer);

	// construct ROI triangle and weight buffers
	ROITriangleBuffer.Reserve(TriangleROI.Num());
	ROITriangleBuffer.SetNum(0, false);
	for (int32 tid : TriangleROI)
	{
		ROITriangleBuffer.Add(tid);
	}
	ROIWeightValueBuffer.SetNum(VertexROI.Num(), false);
	SyncWeightBufferWithMesh(Mesh);
}

bool UClothEditorWeightMapPaintTool::UpdateStampPosition(const FRay& WorldRay)
{
	CalculateBrushRadius();

	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();

	ESculptBrushOpTargetType TargetType = UseBrushOp->GetBrushTargetType();
	switch (TargetType)
	{
	case ESculptBrushOpTargetType::SculptMesh:
	case ESculptBrushOpTargetType::TargetMesh:
		UpdateBrushPositionOnSculptMesh(WorldRay, true);
		break;
	case ESculptBrushOpTargetType::ActivePlane:
		check(false);
		UpdateBrushPositionOnActivePlane(WorldRay);
		break;
	}

	if (UseBrushOp->GetAlignStampToView())
	{
		AlignBrushToView();
	}

	CurrentStamp = LastStamp;
	CurrentStamp.DeltaTime = FMathd::Min((FDateTime::Now() - LastStamp.TimeStamp).GetTotalSeconds(), 1.0);
	CurrentStamp.WorldFrame = GetBrushFrameWorld();
	CurrentStamp.LocalFrame = GetBrushFrameLocal();
	CurrentStamp.Power = GetActivePressure() * GetCurrentBrushStrength();

	CurrentStamp.PrevLocalFrame = LastStamp.LocalFrame;
	CurrentStamp.PrevWorldFrame = LastStamp.WorldFrame;

	FVector3d MoveDelta = CurrentStamp.LocalFrame.Origin - CurrentStamp.PrevLocalFrame.Origin;
	if (UseBrushOp->IgnoreZeroMovements() && MoveDelta.SquaredLength() < FMathd::ZeroTolerance)
	{
		return false;
	}

	return true;
}


bool UClothEditorWeightMapPaintTool::ApplyStamp()
{
	SCOPE_CYCLE_COUNTER(WeightMapPaintToolApplyStamp);

	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();

	// yuck
	FMeshVertexWeightMapEditBrushOp* WeightBrushOp = (FMeshVertexWeightMapEditBrushOp*)UseBrushOp.Get();

	FDynamicMesh3* Mesh = GetSculptMesh();
	WeightBrushOp->ApplyStampByVertices(Mesh, CurrentStamp, VertexROI, ROIWeightValueBuffer);

	bool bUpdated = SyncMeshWithWeightBuffer(Mesh);

	LastStamp = CurrentStamp;
	LastStamp.TimeStamp = FDateTime::Now();

	return bUpdated;
}




bool UClothEditorWeightMapPaintTool::SyncMeshWithWeightBuffer(FDynamicMesh3* Mesh)
{
	int NumModified = 0;
	const int32 NumT = VertexROI.Num();
	if (ActiveWeightMap)
	{
		// change update could be async here if we collected array of <idx,orig,new> and dispatched independenlty
		for (int32 k = 0; k < NumT; ++k)
		{
			int VertIdx = VertexROI[k];
			double CurWeight = GetCurrentWeightValue(VertIdx);

			if (ROIWeightValueBuffer[k] != CurWeight)
			{
				ActiveWeightEditChangeTracker->SaveVertexOneRingTriangles(VertIdx, true);
				ActiveWeightMap->SetValue(VertIdx, &ROIWeightValueBuffer[k]);
				NumModified++;
			}
		}
	}
	return (NumModified > 0);
}

bool UClothEditorWeightMapPaintTool::SyncWeightBufferWithMesh(const FDynamicMesh3* Mesh)
{
	int NumModified = 0;
	const int32 NumT = VertexROI.Num();
	if (ActiveWeightMap)
	{
		// change update could be async here if we collected array of <idx,orig,new> and dispatched independenlty
		for (int32 k = 0; k < NumT; ++k)
		{
			int VertIdx = VertexROI[k];
			double CurWeight = GetCurrentWeightValue(VertIdx);
			if (ROIWeightValueBuffer[k] != CurWeight)
			{
				ROIWeightValueBuffer[k] = CurWeight;
				NumModified++;
			}
		}
	}
	return (NumModified > 0);
}

template<typename RealType>
static bool FindPolylineSelfIntersection(
	const TArray<UE::Math::TVector2<RealType>>& Polyline, 
	UE::Math::TVector2<RealType>& IntersectionPointOut, 
	FIndex2i& IntersectionIndexOut,
	bool bParallel = true)
{
	int32 N = Polyline.Num();
	std::atomic<bool> bSelfIntersects(false);
	ParallelFor(N - 1, [&](int32 i)
	{
		TSegment2<RealType> SegA(Polyline[i], Polyline[i + 1]);
		for (int32 j = i + 2; j < N - 1 && bSelfIntersects == false; ++j)
		{
			TSegment2<RealType> SegB(Polyline[j], Polyline[j + 1]);
			if (SegA.Intersects(SegB) && bSelfIntersects == false)		
			{
				bool ExpectedValue = false;
				if (std::atomic_compare_exchange_strong(&bSelfIntersects, &ExpectedValue, true))
				{
					TIntrSegment2Segment2<RealType> Intersection(SegA, SegB);
					Intersection.Find();
					IntersectionPointOut = Intersection.Point0;
					IntersectionIndexOut = FIndex2i(i, j);
					return;
				}
			}
		}
	}, (bParallel) ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread );

	return bSelfIntersects;
}



template<typename RealType>
static bool FindPolylineSegmentIntersection(
	const TArray<UE::Math::TVector2<RealType>>& Polyline,
	const TSegment2<RealType>& Segment,
	UE::Math::TVector2<RealType>& IntersectionPointOut,
	int& IntersectionIndexOut)
{

	int32 N = Polyline.Num();
	for (int32 i = 0; i < N-1; ++i)
	{
		TSegment2<RealType> PolySeg(Polyline[i], Polyline[i + 1]);
		if (Segment.Intersects(PolySeg))
		{
			TIntrSegment2Segment2<RealType> Intersection(Segment, PolySeg);
			Intersection.Find();
			IntersectionPointOut = Intersection.Point0;
			IntersectionIndexOut = i;
			return true;
		}
	}
	return false;
}



bool ApproxSelfClipPolyline(TArray<FVector2f>& Polyline)
{
	int32 N = Polyline.Num();

	// handle already-closed polylines
	if (Distance(Polyline[0], Polyline[N-1]) < 0.0001f)
	{
		return true;
	}

	FVector2f IntersectPoint;
	FIndex2i IntersectionIndex(-1, -1);
	bool bSelfIntersects = FindPolylineSelfIntersection(Polyline, IntersectPoint, IntersectionIndex);
	if (bSelfIntersects)
	{
		TArray<FVector2f> NewPolyline;
		NewPolyline.Add(IntersectPoint);
		for (int32 i = IntersectionIndex.A; i <= IntersectionIndex.B; ++i)
		{
			NewPolyline.Add(Polyline[i]);
		}
		NewPolyline.Add(IntersectPoint);
		Polyline = MoveTemp(NewPolyline);
		return true;
	}


	FVector2f StartDirOut = UE::Geometry::Normalized(Polyline[0] - Polyline[1]);
	FLine2f StartLine(Polyline[0], StartDirOut);
	FVector2f EndDirOut = UE::Geometry::Normalized(Polyline[N - 1] - Polyline[N - 2]);
	FLine2f EndLine(Polyline[N - 1], EndDirOut);
	FIntrLine2Line2f LineIntr(StartLine, EndLine);
	bool bIntersects = false;
	if (LineIntr.Find())
	{
		bIntersects = LineIntr.IsSimpleIntersection() && (LineIntr.Segment1Parameter > 0) && (LineIntr.Segment2Parameter > 0);
		if (bIntersects)
		{
			Polyline.Add(StartLine.PointAt(LineIntr.Segment1Parameter));
			Polyline.Add(StartLine.Origin);
			return true;
		}
	}


	FAxisAlignedBox2f Bounds;
	for (const FVector2f& P : Polyline)
	{
		Bounds.Contain(P);
	}
	float Size = Bounds.DiagonalLength();

	FVector2f StartPos = Polyline[0] + 0.001f * StartDirOut;
	if (FindPolylineSegmentIntersection(Polyline, FSegment2f(StartPos, StartPos + 2*Size*StartDirOut), IntersectPoint, IntersectionIndex.A))
	{
		return true;
	}

	FVector2f EndPos = Polyline[N-1] + 0.001f * EndDirOut;
	if (FindPolylineSegmentIntersection(Polyline, FSegment2f(EndPos, EndPos + 2*Size*EndDirOut), IntersectPoint, IntersectionIndex.A))
	{
		return true;
	}

	return false;
}



void UClothEditorWeightMapPaintTool::OnPolyLassoFinished(const FCameraPolyLasso& Lasso, bool bCanceled)
{
	// construct polyline
	TArray<FVector2f> Polyline;
	for (FVector2D Pos : Lasso.Polyline)
	{
		Polyline.Add((FVector2f)Pos);
	}
	int32 N = Polyline.Num();
	if (N < 2)
	{
		return;
	}

	// Try to clip polyline to be closed, or closed-enough for winding evaluation to work.
	// If that returns false, the polyline is "too open". In that case we will extend
	// outwards from the endpoints and then try to create a closed very large polygon
	if (ApproxSelfClipPolyline(Polyline) == false)
	{
		FVector2f StartDirOut = UE::Geometry::Normalized(Polyline[0] - Polyline[1]);
		FLine2f StartLine(Polyline[0], StartDirOut);
		FVector2f EndDirOut = UE::Geometry::Normalized(Polyline[N-1] - Polyline[N-2]);
		FLine2f EndLine(Polyline[N-1], EndDirOut);

		// if we did not intersect, we are in ambiguous territory. Check if a segment along either end-direction
		// intersects the polyline. If it does, we have something like a spiral and will be OK. 
		// If not, make a closed polygon by interpolating outwards from each endpoint, and then in perp-directions.
		FPolygon2f Polygon(Polyline);
		float PerpSign = Polygon.IsClockwise() ? -1.0 : 1.0;

		Polyline.Insert(StartLine.PointAt(10000.0f), 0);
		Polyline.Insert(Polyline[0] + 1000 * PerpSign * UE::Geometry::PerpCW(StartDirOut), 0);

		Polyline.Add(EndLine.PointAt(10000.0f));
		Polyline.Add(Polyline.Last() + 1000 * PerpSign * UE::Geometry::PerpCW(EndDirOut));
		FVector2f StartPos = Polyline[0];
		Polyline.Add(StartPos);		// close polyline (cannot use Polyline[0] in case Add resizes!)
	}

	N = Polyline.Num();

	// project each mesh vertex to view plane and evaluate winding integral of polyline
	const FDynamicMesh3* Mesh = GetSculptMesh();
	TempROIBuffer.SetNum(Mesh->MaxVertexID());
	ParallelFor(Mesh->MaxVertexID(), [&](int32 vid)
	{
		if (Mesh->IsVertex(vid))
		{
			FVector3d WorldPos = CurTargetTransform.TransformPosition(Mesh->GetVertex(vid));
			FVector2f PlanePos = (FVector2f)Lasso.GetProjectedPoint((FVector)WorldPos);

			double WindingSum = 0;
			FVector2f a = Polyline[0] - PlanePos, b = FVector2f::Zero();
			for (int32 i = 1; i < N; ++i)
			{
				b = Polyline[i] - PlanePos;
				WindingSum += (double)FMathf::Atan2(a.X*b.Y - a.Y*b.X, a.X*b.X + a.Y*b.Y);
				a = b;
			}
			WindingSum /= FMathd::TwoPi;
			bool bInside = FMathd::Abs(WindingSum) > 0.3;
			TempROIBuffer[vid] = bInside ? 1 : 0;
		}
		else
		{
			TempROIBuffer[vid] = -1;
		}
	});

	// convert to vertex selection, and then select fully-enclosed faces
	FMeshVertexSelection VertexSelection(Mesh);
	VertexSelection.SelectByVertexID([&](int32 vid) { return TempROIBuffer[vid] == 1; });

	double SetWeightValue = GetInEraseStroke() ? 0.0 : FilterProperties->StrengthValue;
	SetVerticesToWeightMap(VertexSelection.AsSet(), SetWeightValue, GetInEraseStroke());
}




void UClothEditorWeightMapPaintTool::SetVerticesToWeightMap(const TSet<int32>& Vertices, double WeightValue, bool bIsErase)
{
	BeginChange();

	TempROIBuffer.SetNum(0, false);
	for (int32 vid : Vertices)
	{
		TempROIBuffer.Add(vid);
	}

	if (HaveVisibilityFilter())
	{
		TArray<int32> VisibleVertices;
		VisibleVertices.Reserve(TempROIBuffer.Num());
		ApplyVisibilityFilter(TempROIBuffer, VisibleVertices);
		TempROIBuffer = MoveTemp(VisibleVertices);
	}

	for (int32 vid : TempROIBuffer)
	{
		ActiveWeightEditChangeTracker->SaveVertexOneRingTriangles(vid, true);
	}
	for (int32 vid : TempROIBuffer)
	{		
		ActiveWeightMap->SetValue(vid, &WeightValue);
	}


	DynamicMeshComponent->FastNotifyVertexAttributesUpdated(false, true, false);
	GetToolManager()->PostInvalidation();
	

	EndChange();
	
}



bool UClothEditorWeightMapPaintTool::HaveVisibilityFilter() const
{
	return FilterProperties->VisibilityFilter != EClothEditorWeightMapPaintVisibilityType::None;
}


void UClothEditorWeightMapPaintTool::ApplyVisibilityFilter(TSet<int32>& Vertices, TArray<int32>& ROIBuffer, TArray<int32>& OutputBuffer)
{
	ROIBuffer.SetNum(0, false);
	ROIBuffer.Reserve(Vertices.Num());
	for (int32 vid : Vertices)
	{
		ROIBuffer.Add(vid);
	}
	
	OutputBuffer.Reset();
	ApplyVisibilityFilter(TempROIBuffer, OutputBuffer);

	Vertices.Reset();
	for (int32 vid : OutputBuffer)
	{
		Vertices.Add(vid);
	}
}

void UClothEditorWeightMapPaintTool::ApplyVisibilityFilter(const TArray<int32>& Vertices, TArray<int32>& VisibleVertices)
{
	if (!HaveVisibilityFilter())
	{
		VisibleVertices = Vertices;
		return;
	}

	FViewCameraState StateOut;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(StateOut);
	FTransform3d LocalToWorld = UE::ToolTarget::GetLocalToWorldTransform(Target);
	FVector3d LocalEyePosition(LocalToWorld.InverseTransformPosition(StateOut.Position));

	const FDynamicMesh3* Mesh = GetSculptMesh();

	int32 NumVertices = Vertices.Num();

	VisibilityFilterBuffer.SetNum(NumVertices, false);
	ParallelFor(NumVertices, [&](int32 idx)
	{
		VisibilityFilterBuffer[idx] = true;
		UE::Geometry::FVertexInfo VertexInfo;
		Mesh->GetVertex(Vertices[idx], VertexInfo, true, false, false);
		FVector3d Centroid = VertexInfo.Position;
		FVector3d FaceNormal = (FVector3d)VertexInfo.Normal;
		if (FaceNormal.Dot((Centroid - LocalEyePosition)) > 0)
		{
			VisibilityFilterBuffer[idx] = false;
		}
		if (FilterProperties->VisibilityFilter == EClothEditorWeightMapPaintVisibilityType::Unoccluded)
		{
			int32 HitTID = Octree.FindNearestHitObject(FRay3d(LocalEyePosition, UE::Geometry::Normalized(Centroid - LocalEyePosition)));
			if (HitTID != IndexConstants::InvalidID && Mesh->IsTriangle(HitTID))
			{
				// Check to see if our vertex has been occulded by another triangle.
				FIndex3i TriVertices = Mesh->GetTriangle(HitTID);
				if (TriVertices[0] != Vertices[idx] && TriVertices[1] != Vertices[idx] && TriVertices[2] != Vertices[idx])
				{
					VisibilityFilterBuffer[idx] = false;
				}
			}
		}
	});

	VisibleVertices.Reset();
	for (int32 k = 0; k < NumVertices; ++k)
	{
		if (VisibilityFilterBuffer[k])
		{
			VisibleVertices.Add(Vertices[k]);
		}
	}
}



int32 UClothEditorWeightMapPaintTool::FindHitSculptMeshTriangle(const FRay3d& LocalRay)
{
	// TODO: Figure out what the actual position on the triangle is when hit.
	CurrentBaryCentricCoords = FVector3d(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);

	if (!IsInBrushSubMode())
	{
		return IndexConstants::InvalidID;
	}

	if (GetBrushCanHitBackFaces())
	{
		return Octree.FindNearestHitObject(LocalRay);
	}
	else
	{
		FDynamicMesh3* Mesh = GetSculptMesh();

		FViewCameraState StateOut;
		GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(StateOut);
		FVector3d LocalEyePosition(CurTargetTransform.InverseTransformPosition((FVector3d)StateOut.Position));
		int HitTID = Octree.FindNearestHitObject(LocalRay,
			[this, Mesh, &LocalEyePosition](int TriangleID) {
			FVector3d Normal, Centroid;
			double Area;
			Mesh->GetTriInfo(TriangleID, Normal, Area, Centroid);
			return Normal.Dot((Centroid - LocalEyePosition)) < 0;
		});
		return HitTID;
	}
}

int32 UClothEditorWeightMapPaintTool::FindHitTargetMeshTriangle(const FRay3d& LocalRay)
{
	check(false);
	return IndexConstants::InvalidID;
}



bool UClothEditorWeightMapPaintTool::UpdateBrushPosition(const FRay& WorldRay)
{
	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();

	bool bHit = false; 
	ESculptBrushOpTargetType TargetType = UseBrushOp->GetBrushTargetType();
	switch (TargetType)
	{
	case ESculptBrushOpTargetType::SculptMesh:
	case ESculptBrushOpTargetType::TargetMesh:
		bHit = UpdateBrushPositionOnSculptMesh(WorldRay, false);
		break;
	case ESculptBrushOpTargetType::ActivePlane:
		check(false);
		bHit = UpdateBrushPositionOnSculptMesh(WorldRay, false);
		break;
	}

	if (bHit && UseBrushOp->GetAlignStampToView())
	{
		AlignBrushToView();
	}

	return bHit;
}




bool UClothEditorWeightMapPaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	PendingStampType = FilterProperties->PrimaryBrushType;

	if(ensure(InStroke() == false))
	{
		UpdateBrushPosition(DevicePos.WorldRay);
	}
	return true;
}


void UClothEditorWeightMapPaintTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (PolyLassoMechanic)
	{
		// because the actual Weight change is deferred until mouse release, color the lasso to let the user know whether it will erase
		PolyLassoMechanic->LineColor = GetInEraseStroke() ? FLinearColor::Red : FLinearColor::Green;
		PolyLassoMechanic->DrawHUD(Canvas, RenderAPI);
	}
}


void UClothEditorWeightMapPaintTool::OnTick(float DeltaTime)
{
	UMeshSculptToolBase::OnTick(DeltaTime);
	MeshElementsDisplay->OnTick(DeltaTime);

	bool bIsLasso = (FilterProperties->SubToolType == EClothEditorWeightMapPaintInteractionType::PolyLasso);
	PolyLassoMechanic->SetIsEnabled(bIsLasso);

	ConfigureIndicator(false);
	SetIndicatorVisibility(bIsLasso == false);

	if (bHavePendingAction)
	{
		ApplyAction(PendingAction);
		bHavePendingAction = false;
		PendingAction = EClothEditorWeightMapPaintToolActions::NoAction;
	}

	SCOPE_CYCLE_COUNTER(WeightMapPaintToolTick);

	// process the undo update
	if (bUndoUpdatePending)
	{
		// wait for updates
		WaitForPendingUndoRedo();

		// post rendering update
		DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(AccumulatedTriangleROI, EMeshRenderAttributeFlags::VertexColors);
		GetToolManager()->PostInvalidation();

		// ignore stamp and wait for next tick to do anything else
		bUndoUpdatePending = false;
		return;
	}

	if (bPendingPickWeight)
	{
		if (GetBrushTriangleID() >= 0 && IsStampPending() == false )
		{
			if (GetSculptMesh()->IsTriangle(GetBrushTriangleID()))
			{
				double HitWeightValue = GetCurrentWeightValueUnderBrush();
				if (bPendingPickWeight)
				{
					FilterProperties->StrengthValue = HitWeightValue;
					NotifyOfPropertyChangeByTool(FilterProperties);
				}
			}
		}
		bPendingPickWeight = false;
	}


	if (IsInBrushSubMode())
	{
		if (InStroke())
		{
			SCOPE_CYCLE_COUNTER(WeightMapPaintTool_Tick_ApplyStampBlock);

			// update brush position
			if (UpdateStampPosition(GetPendingStampRayWorld()) == false)
			{
				return;
			}
			UpdateStampPendingState();
			if (IsStampPending() == false)
			{
				return;
			}

			// update sculpt ROI
			UpdateROI(CurrentStamp);

			// append updated ROI to modified region (async)
			FDynamicMesh3* Mesh = GetSculptMesh();
			TFuture<void> AccumulateROI = Async(WeightPaintToolAsyncExecTarget, [&]()
			{
				UE::Geometry::VertexToTriangleOneRing(Mesh, VertexROI, AccumulatedTriangleROI);
			});

			// apply the stamp
			bool bWeightsModified = ApplyStamp();

			if (bWeightsModified)
			{
				SCOPE_CYCLE_COUNTER(WeightMapPaintTool_Tick_UpdateMeshBlock);
				DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TriangleROI, EMeshRenderAttributeFlags::VertexColors);
				GetToolManager()->PostInvalidation();
			}

			// we don't really need to wait for these to happen to end Tick()...
			AccumulateROI.Wait();
		}
	}

}




FColor UClothEditorWeightMapPaintTool::GetColorForWeightValue(double WeightValue)
{
	FColor MaxColor = LinearColors::ForestGreen3b();
	FColor MinColor = LinearColors::White3b();
	FColor Color;
	double ClampedValue = FMath::Clamp(WeightValue, 0.0, 1.0);
	Color.R = FMath::LerpStable(MinColor.R, MaxColor.R, ClampedValue);
	Color.G = FMath::LerpStable(MinColor.G, MaxColor.G, ClampedValue);
	Color.B = FMath::LerpStable(MinColor.B, MaxColor.B, ClampedValue);
	Color.A = 1.0;

	return Color;
}

void UClothEditorWeightMapPaintTool::FloodFillCurrentWeightAction()
{
	if (!ActiveWeightMap)
	{
		return;
	}

	BeginChange();

	float SetWeightValue = FilterProperties->StrengthValue;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 vid : Mesh->VertexIndicesItr())
	{
		TempROIBuffer.Add(vid);
	}
	for (int32 vid : TempROIBuffer)
	{
		ActiveWeightEditChangeTracker->SaveVertexOneRingTriangles(vid, true);
	}
	for (int32 vid : TempROIBuffer)
	{
		ActiveWeightMap->SetValue(vid, &SetWeightValue);
	}

	DynamicMeshComponent->FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UClothEditorWeightMapPaintTool::ClearAllWeightsAction()
{
	if (!ActiveWeightMap)
	{
		return;
	}

	BeginChange();

	float SetWeightValue = 0.0f;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 vid : Mesh->VertexIndicesItr())
	{
		TempROIBuffer.Add(vid);
	}
	for (int32 vid : TempROIBuffer)
	{
		ActiveWeightEditChangeTracker->SaveVertexOneRingTriangles(vid, true);
	}
	for (int32 vid : TempROIBuffer)
	{
		ActiveWeightMap->SetValue(vid, &SetWeightValue);
	}

	DynamicMeshComponent->FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UClothEditorWeightMapPaintTool::InitializeWeightMapNames()
{
	const FDynamicMesh3* const Mesh = GetSculptMesh();

	if (!WeightMapSetProperties || !Mesh || !Mesh->HasAttributes())
	{
		return;
	}

	TArray<FString>& PropWeightMapNames = WeightMapSetProperties->WeightMapsList;
	PropWeightMapNames.Reset();
	PropWeightMapNames.Add(TEXT("None"));

	// get names from the mesh attributes
	for (int32 WeightMapIndex = 0; WeightMapIndex < Mesh->Attributes()->NumWeightLayers(); ++WeightMapIndex)
	{
		const FName& MeshWeightMapName = Mesh->Attributes()->GetWeightLayer(WeightMapIndex)->GetName();
		PropWeightMapNames.Add(MeshWeightMapName.ToString());
	}

	// if selected weight map name is no longer in the list of valid names, reset it
	if (!PropWeightMapNames.Contains(WeightMapSetProperties->WeightMap.ToString()))
	{
		WeightMapSetProperties->WeightMap = FName(PropWeightMapNames[0]);
	}
}


void UClothEditorWeightMapPaintTool::AddWeightMapAction(const FName& NewWeightMapName)
{
	if (DynamicMeshComponent && DynamicMeshComponent->GetMesh() && DynamicMeshComponent->GetMesh()->HasAttributes())
	{
		FDynamicMesh3* const Mesh = DynamicMeshComponent->GetMesh();

		FDynamicMeshAttributeSet* const MeshAttributes = Mesh->Attributes();
		const int32 NumExistingWeights = MeshAttributes->NumWeightLayers();
		
		if (NewWeightMapName.IsNone())
		{
			GetToolManager()->DisplayMessage(LOCTEXT("InvalidAttributeName", "Invalid weight map name"), EToolMessageLevel::UserWarning);
			return;
		}

		// Check for matching name	
		for (int32 WeightMapIndex = 0; WeightMapIndex < NumExistingWeights; ++WeightMapIndex)
		{
			const FDynamicMeshWeightAttribute* const ExistingWeightMap = MeshAttributes->GetWeightLayer(WeightMapIndex);
			if (ExistingWeightMap->GetName() == NewWeightMapName)
			{
				GetToolManager()->DisplayMessage(LOCTEXT("ErrorAddingDuplicateNameMessage", "Weight map with this name already exists"), EToolMessageLevel::UserWarning);
				return;
			}
		}

		MeshAttributes->SetNumWeightLayers(NumExistingWeights + 1);

		const int32 NewWeightMapIndex = NumExistingWeights;
		FDynamicMeshWeightAttribute* const NewWeightMap = MeshAttributes->GetWeightLayer(NewWeightMapIndex);
		NewWeightMap->SetName(NewWeightMapName);

		// Refresh the drop down of available weight maps and select the new one
		InitializeWeightMapNames();
		WeightMapSetProperties->SetSelectedFromWeightMapIndex(NewWeightMapIndex);

		// Clear the New Name text field
		ClothEditorWeightMapActions->NewWeightMapName.Empty();
	}
}


void UClothEditorWeightMapPaintTool::DeleteWeightMapAction(const FName& SelectedWeightMapName)
{
	FDynamicMeshAttributeSet* const MeshAttributes = DynamicMeshComponent->GetMesh()->Attributes();

	bool bFoundSelectedWeightMap = false;
	
	const int32 NumWeightMaps = MeshAttributes->NumWeightLayers();
	for (int32 WeightMapIndex = 0; WeightMapIndex < NumWeightMaps; ++WeightMapIndex)
	{
		const FDynamicMeshWeightAttribute* const WeightMap = MeshAttributes->GetWeightLayer(WeightMapIndex);
		if (WeightMap->GetName() == SelectedWeightMapName)
		{
			bFoundSelectedWeightMap = true;
			MeshAttributes->RemoveWeightLayer(WeightMapIndex);		// Hopefully nothing else is holding indices into this...
			break;
		}
	}

	if (!bFoundSelectedWeightMap)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("InvalidExistingWeightMapName", "No weight map with the specified name exists"), EToolMessageLevel::UserWarning);
	}
	else
	{
		InitializeWeightMapNames();
		UpdateActiveWeightMap();
		GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserWarning);
	}
}

//
// Change Tracking
//

namespace ClothWeightPaintLocals
{

	/**
	 * A wrapper change that applies a given change to the unwrap canonical mesh of an input, and uses that
	 * to update the other views. Causes a broadcast of OnCanonicalModified.
	 */
	class  FClothWeightPaintMeshChange : public FToolCommandChange
	{
	public:
		FClothWeightPaintMeshChange(UDynamicMeshComponent* DynamicMeshComponentIn, TUniquePtr<FDynamicMeshChange> DynamicMeshChangeIn)
			: DynamicMeshComponent(DynamicMeshComponentIn)
			, DynamicMeshChange(MoveTemp(DynamicMeshChangeIn))
		{
			ensure(DynamicMeshComponentIn);
			ensure(DynamicMeshChange);
		};

		virtual void Apply(UObject* Object) override
		{
			DynamicMeshChange->Apply(DynamicMeshComponent->GetMesh(), false);			
		}

		virtual void Revert(UObject* Object) override
		{
			DynamicMeshChange->Apply(DynamicMeshComponent->GetMesh(), true);			
		}

		virtual bool HasExpired(UObject* Object) const override
		{
			return !(DynamicMeshComponent.IsValid() && DynamicMeshChange);
		}


		virtual FString ToString() const override
		{
			return TEXT("FClothWeightPaintMeshChange");
		}

	protected:
		TWeakObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;
		TUniquePtr<FDynamicMeshChange> DynamicMeshChange;
	};
}

void UClothEditorWeightMapPaintTool::BeginChange()
{

	check(ActiveWeightEditChangeTracker == nullptr);
	
	ActiveWeightEditChangeTracker = MakeUnique<FDynamicMeshChangeTracker>(GetSculptMesh());
	ActiveWeightEditChangeTracker->BeginChange();
}

void UClothEditorWeightMapPaintTool::EndChange()
{
	check(ActiveWeightEditChangeTracker);

	TUniquePtr < FDynamicMeshChange > EditResult = ActiveWeightEditChangeTracker->EndChange();
	TUniquePtr<ClothWeightPaintLocals::FClothWeightPaintMeshChange> ClothWeightPaintMeshChange =
		MakeUnique<ClothWeightPaintLocals::FClothWeightPaintMeshChange>(DynamicMeshComponent.Get(), MoveTemp(EditResult));
	ActiveWeightEditChangeTracker = nullptr;

	TUniquePtr<TWrappedToolCommandChange<ClothWeightPaintLocals::FClothWeightPaintMeshChange>> NewChange = MakeUnique<TWrappedToolCommandChange<ClothWeightPaintLocals::FClothWeightPaintMeshChange>>();
	NewChange->WrappedChange = MoveTemp(ClothWeightPaintMeshChange);
	NewChange->BeforeModify = [this](bool bRevert)
	{
		this->WaitForPendingUndoRedo();
	};

	GetToolManager()->EmitObjectChange(DynamicMeshComponent, MoveTemp(NewChange), LOCTEXT("WeightPaintChange", "Weight Stroke"));
}


void UClothEditorWeightMapPaintTool::WaitForPendingUndoRedo()
{
	if (bUndoUpdatePending)
	{
		bUndoUpdatePending = false;
	}
}

void UClothEditorWeightMapPaintTool::OnDynamicMeshComponentChanged(UDynamicMeshComponent* Component, const FMeshVertexChange* Change, bool bRevert)
{
	// update octree
	FDynamicMesh3* Mesh = GetSculptMesh();

	// make sure any previous async computations are done, and update the undo ROI
	if (bUndoUpdatePending)
	{
		// we should never hit this anymore, because of pre-change calling WaitForPendingUndoRedo()
		WaitForPendingUndoRedo();

		// this is not right because now we are going to do extra recomputation, but it's very messy otherwise...
		UE::Geometry::VertexToTriangleOneRing(Mesh, Change->Vertices, AccumulatedTriangleROI);
	}
	else
	{
		AccumulatedTriangleROI.Reset();
		UE::Geometry::VertexToTriangleOneRing(Mesh, Change->Vertices, AccumulatedTriangleROI);
	}

	// note that we have a pending update
	bUndoUpdatePending = true;
}


void UClothEditorWeightMapPaintTool::PrecomputeFilterData()
{
	const FDynamicMesh3* Mesh = GetSculptMesh();
	
	TriNormals.SetNum(Mesh->MaxTriangleID());
	ParallelFor(Mesh->MaxTriangleID(), [&](int32 tid)
	{
		if (Mesh->IsTriangle(tid))
		{
			TriNormals[tid] = Mesh->GetTriNormal(tid);
		}
	});

	const FDynamicMeshNormalOverlay* Normals = Mesh->Attributes()->PrimaryNormals();
	const FDynamicMeshUVOverlay* UVs = Mesh->Attributes()->PrimaryUV();
	UVSeamEdges.SetNum(Mesh->MaxEdgeID());
	NormalSeamEdges.SetNum(Mesh->MaxEdgeID());
	ParallelFor(Mesh->MaxEdgeID(), [&](int32 eid)
	{
		if (Mesh->IsEdge(eid))
		{
			UVSeamEdges[eid] = UVs->IsSeamEdge(eid);
			NormalSeamEdges[eid] = Normals->IsSeamEdge(eid);
		}
	});
}

double UClothEditorWeightMapPaintTool::GetCurrentWeightValue(int32 VertexId) const
{
	float WeightValue = 0.0;
	if (ActiveWeightMap && VertexId != IndexConstants::InvalidID)
	{
		ActiveWeightMap->GetValue(VertexId, &WeightValue);
	}
	return WeightValue;
}

double UClothEditorWeightMapPaintTool::GetCurrentWeightValueUnderBrush() const
{
	float WeightValue = 0.0;
	int32 VertexID = GetBrushNearestVertex();
	if (ActiveWeightMap && VertexID != IndexConstants::InvalidID)
	{
		ActiveWeightMap->GetValue(VertexID, &WeightValue);
	}
	return WeightValue;
}

int32 UClothEditorWeightMapPaintTool::GetBrushNearestVertex() const
{
	int TriangleVertex = 0;

	if (CurrentBaryCentricCoords.X >= CurrentBaryCentricCoords.Y && CurrentBaryCentricCoords.X >= CurrentBaryCentricCoords.Z)
	{
		TriangleVertex = 0;
	}
	else
	{
		if (CurrentBaryCentricCoords.Y >= CurrentBaryCentricCoords.X && CurrentBaryCentricCoords.Y >= CurrentBaryCentricCoords.Z)
		{
			TriangleVertex = 1;
		}
		else
		{
			TriangleVertex = 2;
		}
	}
	const FDynamicMesh3* Mesh = GetSculptMesh();

	int32 Tid = GetBrushTriangleID();
	if (Tid == IndexConstants::InvalidID)
	{
		return IndexConstants::InvalidID;
	}
	
	FIndex3i Vertices = Mesh->GetTriangle(Tid);
	return Vertices[TriangleVertex];
}


void UClothEditorWeightMapPaintTool::OnSelectedWeightMapChanged()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("ChangeActiveWeightLayer", "Change Weight Layer"));

	FDynamicMeshWeightAttribute* CurrentActiveLayer = ActiveWeightMap;
	UpdateActiveWeightMap();
	FDynamicMeshWeightAttribute* NewActiveLayer = ActiveWeightMap; 

	if (CurrentActiveLayer != NewActiveLayer)
	{
		// A value of -1 matches the property set's "None" label.
		int32 CurrentActiveLayerIndex = -1;
		int32 NewActiveLayerIndex = -1;
		if (CurrentActiveLayer) {
			WeightMapSetProperties->GetWeightMapsFunc().Find(CurrentActiveLayer->GetName().ToString(), CurrentActiveLayerIndex);
			CurrentActiveLayerIndex = CurrentActiveLayerIndex - 1;
		}
		if (NewActiveLayer)
		{
			WeightMapSetProperties->GetWeightMapsFunc().Find(NewActiveLayer->GetName().ToString(), NewActiveLayerIndex);
			NewActiveLayerIndex = NewActiveLayerIndex - 1;
		}

		TUniquePtr<TSimpleValueLambdaChange<int32>> WeightLayerChange = MakeUnique<TSimpleValueLambdaChange<int32>>();
		WeightLayerChange->FromValue = CurrentActiveLayerIndex;
		WeightLayerChange->ToValue = NewActiveLayerIndex;
		WeightLayerChange->ValueChangeFunc = [this](UObject*, int32 FromIndex, int32 ToIndex, bool)
		{
			this->WeightMapSetProperties->SetSelectedFromWeightMapIndex(ToIndex);
			this->WeightMapSetProperties->SilentUpdateWatched();		// to prevent OnSelectedWeightMapChanged() from being called immediately
			this->UpdateActiveWeightMap();
		};
		GetToolManager()->EmitObjectChange(this, MoveTemp(WeightLayerChange), LOCTEXT("ChangeActiveWeightSet", "Change Weight Set"));
	}

	GetToolManager()->EndUndoTransaction();
}


void UClothEditorWeightMapPaintTool::UpdateActiveWeightMap()
{
	int32 SelectedWeightSetIndex;
	FName SelectedName = WeightMapSetProperties->WeightMap;
	WeightMapSetProperties->GetWeightMapsFunc().Find(SelectedName.ToString(), SelectedWeightSetIndex);

	FDynamicMesh3* Mesh = GetSculptMesh();
	// Properties are 1-indexed, due to an initial None option
	if (Mesh->Attributes()->NumWeightLayers() >= SelectedWeightSetIndex && SelectedWeightSetIndex > 0)
	{
		ActiveWeightMap = Mesh->Attributes()->GetWeightLayer(SelectedWeightSetIndex-1);
	}
	else
	{
		ActiveWeightMap = nullptr;
	}

	// update colors
	DynamicMeshComponent->FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
}



void UClothEditorWeightMapPaintTool::UpdateSubToolType(EClothEditorWeightMapPaintInteractionType NewType)
{
	// Currenly we mirror base-brush properties in UClothEditorWeightMapPaintBrushFilterProperties, so we never
	// want to show both
	//bool bSculptPropsVisible = (NewType == EClothEditorWeightMapPaintInteractionType::Brush);
	//SetToolPropertySourceEnabled(UMeshSculptToolBase::BrushProperties, bSculptPropsVisible);
	SetToolPropertySourceEnabled(UMeshSculptToolBase::BrushProperties, false);

	SetToolPropertySourceEnabled(FilterProperties, true);
	SetBrushOpPropsVisibility(false);
}


void UClothEditorWeightMapPaintTool::UpdateBrushType(EClothEditorWeightMapPaintBrushType BrushType)
{
	static const FText BaseMessage = LOCTEXT("OnStartTool", "Hold Shift to Erase. [/] and S/D change Size (+Shift to small-step). Shift+Q for New Group, Shift+G to pick Group, Shift+F to Freeze Group.");
	FTextBuilder Builder;
	Builder.AppendLine(BaseMessage);

	SetActivePrimaryBrushType((int32)BrushType);

	SetToolPropertySourceEnabled(GizmoProperties, false);

	GetToolManager()->DisplayMessage(Builder.ToText(), EToolMessageLevel::UserNotification);
}




void UClothEditorWeightMapPaintTool::RequestAction(EClothEditorWeightMapPaintToolActions ActionType)
{
	if (!bHavePendingAction)
	{
		PendingAction = ActionType;
		bHavePendingAction = true;
	}
}


void UClothEditorWeightMapPaintTool::ApplyAction(EClothEditorWeightMapPaintToolActions ActionType)
{
	switch (ActionType)
	{
	case EClothEditorWeightMapPaintToolActions::FloodFillCurrent:
		FloodFillCurrentWeightAction();
		break;

	case EClothEditorWeightMapPaintToolActions::ClearAll:
		ClearAllWeightsAction();
		break;

	case EClothEditorWeightMapPaintToolActions::AddWeightMap:
		AddWeightMapAction(FName(ClothEditorWeightMapActions->NewWeightMapName));
		break;

	case EClothEditorWeightMapPaintToolActions::DeleteWeightMap:
		DeleteWeightMapAction(WeightMapSetProperties->WeightMap);
		break;

	}
}




#undef LOCTEXT_NAMESPACE
