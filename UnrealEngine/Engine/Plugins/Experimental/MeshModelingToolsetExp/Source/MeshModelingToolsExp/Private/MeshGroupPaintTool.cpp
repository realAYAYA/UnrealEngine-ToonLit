// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshGroupPaintTool.h"
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
#include "Polygroups/PolygroupUtil.h"
#include "Polygon2.h"
#include "Intersection/IntrLine2Line2.h"
#include "Intersection/IntrSegment2Segment2.h"

#include "Changes/MeshVertexChange.h"
#include "Changes/MeshPolygroupChange.h"
#include "Changes/BasicChanges.h"

#include "Sculpting/MeshGroupPaintBrushOps.h"
#include "Sculpting/StampFalloffs.h"
#include "Sculpting/MeshSculptUtil.h"

#include "CanvasTypes.h"
#include "CanvasItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshGroupPaintTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UMeshGroupPaintTool"

namespace
{
	// probably should be something defined for the whole tool framework...
#if WITH_EDITOR
	static EAsyncExecution GroupPaintToolAsyncExecTarget = EAsyncExecution::LargeThreadPool;
#else
	static EAsyncExecution GroupPaintToolAsyncExecTarget = EAsyncExecution::ThreadPool;
#endif
}


/*
 * ToolBuilder
 */
UMeshSurfacePointTool* UMeshGroupPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UMeshGroupPaintTool* SculptTool = NewObject<UMeshGroupPaintTool>(SceneState.ToolManager);
	SculptTool->SetWorld(SceneState.World);
	return SculptTool;
}


/*
 * Properties
 */
void UMeshGroupPaintToolActionPropertySet::PostAction(EMeshGroupPaintToolActions Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}





/*
 * Tool
 */

void UMeshGroupPaintTool::Setup()
{
	UMeshSculptToolBase::Setup();

	SetToolDisplayName(LOCTEXT("ToolName", "Paint PolyGroups"));

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
	OnDynamicMeshComponentChangedHandle = DynamicMeshComponent->OnMeshVerticesChanged.AddUObject(this, &UMeshGroupPaintTool::OnDynamicMeshComponentChanged);

	FDynamicMesh3* Mesh = GetSculptMesh();
	Mesh->EnableVertexColors(FVector3f::One());
	FAxisAlignedBox3d Bounds = Mesh->GetBounds(true);

	TFuture<void> PrecomputeFuture = Async(GroupPaintToolAsyncExecTarget, [&]()
	{
		PrecomputeFilterData();
	});

	TFuture<void> OctreeFuture = Async(GroupPaintToolAsyncExecTarget, [&]()
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
		//Octree.CheckValidity(EValidityCheckFailMode::Check, true, true);
		//FDynamicMeshOctree3::FStatistics Stats;
		//Octree.ComputeStatistics(Stats);
		//UE_LOG(LogTemp, Warning, TEXT("Octree Stats: %s"), *Stats.ToString());
	});

	// initialize render decomposition
	TUniquePtr<FMeshRenderDecomposition> Decomp = MakeUnique<FMeshRenderDecomposition>();
	FMeshRenderDecomposition::BuildChunkedDecomposition(Mesh, &MaterialSet, *Decomp);
	Decomp->BuildAssociations(Mesh);
	//UE_LOG(LogTemp, Warning, TEXT("Decomposition has %d groups"), Decomp->Num());
	DynamicMeshComponent->SetExternalDecomposition(MoveTemp(Decomp));

	// initialize brush radius range interval, brush properties
	UMeshSculptToolBase::InitializeBrushSizeRange(Bounds);

	// Set up control points mechanic
	PolyLassoMechanic = NewObject<UPolyLassoMarqueeMechanic>(this);
	PolyLassoMechanic->Setup(this);
	PolyLassoMechanic->SetIsEnabled(false);
	PolyLassoMechanic->SpacingTolerance = 10.0f;
	PolyLassoMechanic->OnDrawPolyLassoFinished.AddUObject(this, &UMeshGroupPaintTool::OnPolyLassoFinished);

	PolygroupLayerProperties = NewObject<UPolygroupLayersProperties>(this);
	PolygroupLayerProperties->RestoreProperties(this, TEXT("MeshGroupPaintTool"));
	PolygroupLayerProperties->InitializeGroupLayers(GetSculptMesh());
	PolygroupLayerProperties->WatchProperty(PolygroupLayerProperties->ActiveGroupLayer, [&](FName) { OnSelectedGroupLayerChanged(); });
	UpdateActiveGroupLayer();
	AddToolPropertySource(PolygroupLayerProperties);

	// initialize other properties
	FilterProperties = NewObject<UGroupPaintBrushFilterProperties>(this);
	FilterProperties->WatchProperty(FilterProperties->SubToolType,
		[this](EMeshGroupPaintInteractionType NewType) { UpdateSubToolType(NewType); });
	FilterProperties->WatchProperty(FilterProperties->BrushSize,
		[this](float NewSize) { UMeshSculptToolBase::BrushProperties->BrushSize.AdaptiveSize = NewSize; });
	FilterProperties->WatchProperty(FilterProperties->bHitBackFaces,
		[this](bool bNewValue) { UMeshSculptToolBase::BrushProperties->bHitBackFaces = bNewValue; });
	FilterProperties->RestoreProperties(this);
	FilterProperties->BrushSize = UMeshSculptToolBase::BrushProperties->BrushSize.AdaptiveSize;
	FilterProperties->bHitBackFaces = UMeshSculptToolBase::BrushProperties->bHitBackFaces;
	FilterProperties->SetGroup = ActiveGroupSet->MaxGroupID;
	AddToolPropertySource(FilterProperties);

	InitializeIndicator();

	// initialize our properties
	AddToolPropertySource(UMeshSculptToolBase::BrushProperties);
	UMeshSculptToolBase::BrushProperties->bShowPerBrushProps = false;
	UMeshSculptToolBase::BrushProperties->bShowFalloff = false;
	UMeshSculptToolBase::BrushProperties->bShowLazyness = false;
	CalculateBrushRadius();

	PaintBrushOpProperties = NewObject<UGroupPaintBrushOpProps>(this);
	RegisterBrushType((int32)EMeshGroupPaintBrushType::Paint, LOCTEXT("Paint", "Paint"),
		MakeUnique<FLambdaMeshSculptBrushOpFactory>([this]() { return MakeUnique<FGroupPaintBrushOp>(); }),
		PaintBrushOpProperties);

	// secondary brushes
	EraseBrushOpProperties = NewObject<UGroupEraseBrushOpProps>(this);
	EraseBrushOpProperties->GetCurrentGroupLambda = [this]() { return PaintBrushOpProperties->GetGroup(); };

	RegisterSecondaryBrushType((int32)EMeshGroupPaintBrushType::Erase, LOCTEXT("Erase", "Erase"),
		MakeUnique<TBasicMeshSculptBrushOpFactory<FGroupEraseBrushOp>>(),
		EraseBrushOpProperties);

	AddToolPropertySource(UMeshSculptToolBase::ViewProperties);

	AddToolPropertySource(UMeshSculptToolBase::GizmoProperties);
	SetToolPropertySourceEnabled(UMeshSculptToolBase::GizmoProperties, false);


	// register watchers
	FilterProperties->WatchProperty( FilterProperties->PrimaryBrushType,
		[this](EMeshGroupPaintBrushType NewType) { UpdateBrushType(NewType); });

	// must call before updating brush type so that we register all brush properties?
	UMeshSculptToolBase::OnCompleteSetup();

	UpdateBrushType(FilterProperties->PrimaryBrushType);
	SetActiveSecondaryBrushType((int32)EMeshGroupPaintBrushType::Erase);

	FreezeActions = NewObject<UMeshGroupPaintToolFreezeActions>(this);
	FreezeActions->Initialize(this);
	AddToolPropertySource(FreezeActions);

	MeshElementsDisplay = NewObject<UMeshElementsVisualizer>(this);
	MeshElementsDisplay->CreateInWorld(DynamicMeshComponent->GetWorld(), DynamicMeshComponent->GetComponentTransform());
	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->RestoreProperties(this, TEXT("MeshGroupPaintTool"));
		AddToolPropertySource(MeshElementsDisplay->Settings);
	}
	MeshElementsDisplay->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) {
		ProcessFunc(*GetSculptMesh());
	});

	// force colors update... ?
	DynamicMeshComponent->SetTriangleColorFunction([this](const FDynamicMesh3* Mesh, int TriangleID)
	{
		return GetColorForGroup(ActiveGroupSet->GetGroup(TriangleID));
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

void UMeshGroupPaintTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (DynamicMeshComponent != nullptr)
	{
		DynamicMeshComponent->OnMeshChanged.Remove(OnDynamicMeshComponentChangedHandle);
	}

	if (ensure(MeshElementsDisplay->Settings))
	{
		MeshElementsDisplay->Settings->SaveProperties(this, TEXT("MeshGroupPaintTool"));
	}
	MeshElementsDisplay->Disconnect();

	FilterProperties->SaveProperties(this);
	PolygroupLayerProperties->SaveProperties(this, TEXT("MeshGroupPaintTool"));

	if (PreviewMeshActor != nullptr)
	{
		PreviewMeshActor->Destroy();
		PreviewMeshActor = nullptr;
	}

	UMeshSculptToolBase::Shutdown(ShutdownType);
}


void UMeshGroupPaintTool::CommitResult(UBaseDynamicMeshComponent* Component, bool bModifiedTopology)
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("GroupPaintToolTransactionName", "Paint Groups"));
	Component->ProcessMesh([&](const FDynamicMesh3& CurMesh)
	{
		UE::ToolTarget::CommitDynamicMeshUpdate(Target, CurMesh, true);
	});
	GetToolManager()->EndUndoTransaction();
}


void UMeshGroupPaintTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	UMeshSculptToolBase::RegisterActions(ActionSet);

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 500,
		TEXT("PickGroupColorUnderCursor"),
		LOCTEXT("PickGroupColorUnderCursor", "Pick PolyGroup"),
		LOCTEXT("PickGroupColorUnderCursorTooltip", "Switch the active PolyGroup to the group currently under the cursor"),
		EModifierKey::Shift, EKeys::G,
		[this]() { bPendingPickGroup = true; });

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 501,
		TEXT("ToggleFrozenGroup"),
		LOCTEXT("ToggleFrozenGroup", "Toggle Group Frozen State"),
		LOCTEXT("ToggleFrozenGroupTooltip", "Toggle Group Frozen State"),
		EModifierKey::Shift, EKeys::F,
		[this]() { bPendingToggleFreezeGroup = true; });


	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 502,
		TEXT("CreateNewGroup"),
		LOCTEXT("CreateNewGroup", "New Group"),
		LOCTEXT("CreateNewGroupTooltip", "Allocate a new Polygroup and set as Current"),
		EModifierKey::Shift, EKeys::Q,
		[this]() { AllocateNewGroupAndSetAsCurrentAction(); });
};


TUniquePtr<FMeshSculptBrushOp>& UMeshGroupPaintTool::GetActiveBrushOp()
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


void UMeshGroupPaintTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	CalculateBrushRadius();
}


void UMeshGroupPaintTool::IncreaseBrushRadiusAction()
{
	Super::IncreaseBrushRadiusAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UMeshGroupPaintTool::DecreaseBrushRadiusAction()
{
	Super::DecreaseBrushRadiusAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UMeshGroupPaintTool::IncreaseBrushRadiusSmallStepAction()
{
	Super::IncreaseBrushRadiusSmallStepAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}

void UMeshGroupPaintTool::DecreaseBrushRadiusSmallStepAction()
{
	Super::DecreaseBrushRadiusSmallStepAction();
	FilterProperties->BrushSize = BrushProperties->BrushSize.AdaptiveSize;
	NotifyOfPropertyChangeByTool(FilterProperties);
}


bool UMeshGroupPaintTool::IsInBrushSubMode() const
{
	return FilterProperties->SubToolType == EMeshGroupPaintInteractionType::Brush
		|| FilterProperties->SubToolType == EMeshGroupPaintInteractionType::Fill
		|| FilterProperties->SubToolType == EMeshGroupPaintInteractionType::GroupFill;
}


void UMeshGroupPaintTool::OnBeginStroke(const FRay& WorldRay)
{
	UpdateBrushPosition(WorldRay);

	if (PaintBrushOpProperties)
	{
		PaintBrushOpProperties->Group = FilterProperties->SetGroup;
		PaintBrushOpProperties->bOnlyPaintUngrouped = FilterProperties->bOnlySetUngrouped;
	}
	if (EraseBrushOpProperties)
	{
		EraseBrushOpProperties->Group = FilterProperties->EraseGroup;
		EraseBrushOpProperties->bOnlyEraseCurrent = FilterProperties->bOnlyEraseCurrent;
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
	//SculptOptions.bPreserveUVFlow = false; // FilterProperties->bPreserveUVFlow;
	SculptOptions.ConstantReferencePlane = GetCurrentStrokeReferencePlane();

	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();
	UseBrushOp->ConfigureOptions(SculptOptions);
	UseBrushOp->BeginStroke(GetSculptMesh(), LastStamp, VertexROI);

	AccumulatedTriangleROI.Reset();

	// begin change here? or wait for first stamp?
	BeginChange();
}

void UMeshGroupPaintTool::OnEndStroke()
{
	GetActiveBrushOp()->EndStroke(GetSculptMesh(), LastStamp, VertexROI);

	// close change record
	EndChange();
}




void UMeshGroupPaintTool::UpdateROI(const FSculptBrushStamp& BrushStamp)
{
	SCOPE_CYCLE_COUNTER(GroupPaintTool_UpdateROI);

	int32 SetGroupID = GetInEraseStroke() ? FilterProperties->EraseGroup : FilterProperties->SetGroup;

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

	bool bVolumetric = (FilterProperties->BrushAreaMode == EMeshGroupPaintBrushAreaType::Volumetric);
	bool bUseAngleThreshold = (bVolumetric == false) && (FilterProperties->AngleThreshold < 180.0f);
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

	bool bFill = (FilterProperties->SubToolType == EMeshGroupPaintInteractionType::Fill);
	bool bGroupFill = (FilterProperties->SubToolType == EMeshGroupPaintInteractionType::GroupFill);

	if (bVolumetric)
	{
		Octree.RangeQuery(BrushBox,
			[&](int TriIdx) {

			if ((Mesh->GetTriCentroid(TriIdx) - BrushPos).SquaredLength() < RadiusSqr)
			{
				TriangleROI.Add(TriIdx);
			}
		});
	}
	else
	{
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
	else if (bGroupFill)
	{
		TArray<int32> StartROI;
		TSet<int32> FillGroups;
		for (int32 tid : TriangleROI)
		{
			if (ActiveGroupSet->GetGroup(tid) != SetGroupID)
			{
				StartROI.Add(tid);
				FillGroups.Add(ActiveGroupSet->GetGroup(tid));
			}
		}
		FMeshConnectedComponents::GrowToConnectedTriangles(Mesh, StartROI, TriangleROI, &TempROIBuffer,
			[&](int t1, int t2)
		{
			return (FillGroups.Contains(ActiveGroupSet->GetGroup(t2)));
		});
	}


	// apply visibility filter
	if (FilterProperties->VisibilityFilter != EMeshGroupPaintVisibilityType::None)
	{
		TArray<int32> ResultBuffer;
		ApplyVisibilityFilter(TriangleROI, TempROIBuffer, ResultBuffer);
	}

	// construct ROI vertex set
	VertexSetBuffer.Reset();
	for (int32 tid : TriangleROI)
	{
		FIndex3i Tri = Mesh->GetTriangle(tid);
		VertexSetBuffer.Add(Tri.A);  VertexSetBuffer.Add(Tri.B);  VertexSetBuffer.Add(Tri.C);
	}
	VertexROI.SetNum(0, false);
	BufferUtil::AppendElements(VertexROI, VertexSetBuffer);

	// construct ROI triangle and group buffers
	ROITriangleBuffer.Reserve(TriangleROI.Num());
	ROITriangleBuffer.SetNum(0, false);
	for (int32 tid : TriangleROI)
	{
		ROITriangleBuffer.Add(tid);
	}
	ROIGroupBuffer.SetNum(ROITriangleBuffer.Num(), false);
}

bool UMeshGroupPaintTool::UpdateStampPosition(const FRay& WorldRay)
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


bool UMeshGroupPaintTool::ApplyStamp()
{
	SCOPE_CYCLE_COUNTER(GroupPaintToolApplyStamp);

	TUniquePtr<FMeshSculptBrushOp>& UseBrushOp = GetActiveBrushOp();

	// yuck
	FMeshTriangleGroupEditBrushOp* GroupBrushOp = (FMeshTriangleGroupEditBrushOp*)UseBrushOp.Get();

	FDynamicMesh3* Mesh = GetSculptMesh();
	GroupBrushOp->ApplyStampByTriangles(Mesh, CurrentStamp, ROITriangleBuffer, ROIGroupBuffer);

	bool bUpdated = SyncMeshWithGroupBuffer(Mesh);

	LastStamp = CurrentStamp;
	LastStamp.TimeStamp = FDateTime::Now();

	return bUpdated;
}




bool UMeshGroupPaintTool::SyncMeshWithGroupBuffer(FDynamicMesh3* Mesh)
{
	int NumModified = 0;
	const int32 NumT = ROITriangleBuffer.Num();
	// change update could be async here if we collected array of <idx,orig,new> and dispatched independenlty
	for ( int32 k = 0; k < NumT; ++k)
	{
		int TriIdx = ROITriangleBuffer[k];
		int32 CurGroupID = ActiveGroupSet->GetGroup(TriIdx);
		if (FrozenGroups.Contains(CurGroupID))		// skip frozen groups
		{
			continue;
		}

		if (ROIGroupBuffer[k] != CurGroupID)
		{
			ActiveGroupEditBuilder->SaveTriangle(TriIdx, CurGroupID, ROIGroupBuffer[k]);
			ActiveGroupSet->SetGroup(TriIdx, ROIGroupBuffer[k], *Mesh);
			//ActiveVertexChange->UpdateVertexColor(VertIdx, OrigColor, NewColor);
			NumModified++;
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
		//TArray<FVector2f> NewPolyline;
		//for (int32 i = 0; i <= IntersectionIndex.A; ++i)
		//{
		//	NewPolyline.Add(Polyline[i]);
		//}
		//NewPolyline.Add(IntersectPoint);
		//NewPolyline.Add(Polyline[0]);
		//Polyline = MoveTemp(NewPolyline);
		return true;
	}

	FVector2f EndPos = Polyline[N-1] + 0.001f * EndDirOut;
	if (FindPolylineSegmentIntersection(Polyline, FSegment2f(EndPos, EndPos + 2*Size*EndDirOut), IntersectPoint, IntersectionIndex.A))
	{
		//TArray<FVector2f> NewPolyline;
		//NewPolyline.Add(IntersectPoint);
		//for (int32 i = IntersectionIndex.A+1; i < N; ++i)
		//{
		//	NewPolyline.Add(Polyline[i]);
		//}
		//NewPolyline.Add(Polyline[0]);
		//NewPolyline.Add(IntersectPoint);
		//Polyline = MoveTemp(NewPolyline);
		return true;
	}

	return false;
}



void UMeshGroupPaintTool::OnPolyLassoFinished(const FCameraPolyLasso& Lasso, bool bCanceled)
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
 	FMeshFaceSelection FaceSelection(Mesh, VertexSelection, FilterProperties->MinTriVertCount);
	if (FaceSelection.Num() == 0)
	{
		return;
	}

	int32 SetGroupID = GetInEraseStroke() ? FilterProperties->EraseGroup : FilterProperties->SetGroup;
	SetTrianglesToGroupID(FaceSelection.AsSet(), SetGroupID, GetInEraseStroke());
}




void UMeshGroupPaintTool::SetTrianglesToGroupID(const TSet<int32>& Triangles, int32 ToGroupID, bool bIsErase)
{
	BeginChange();

	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Triangles)
	{
		int32 CurGroupID = ActiveGroupSet->GetGroup(tid);
		if (CurGroupID == ToGroupID || FrozenGroups.Contains(CurGroupID))		// skip frozen groups
		{
			continue;
		}
		if (bIsErase == false && FilterProperties->bOnlySetUngrouped && CurGroupID != 0)
		{
			continue;
		}
		if (bIsErase && FilterProperties->bOnlyEraseCurrent && CurGroupID != FilterProperties->SetGroup)
		{
			continue;
		}

		TempROIBuffer.Add(tid);
	}

	if (HaveVisibilityFilter())
	{
		TArray<int32> VisibleTriangles;
		VisibleTriangles.Reserve(TempROIBuffer.Num());
		ApplyVisibilityFilter(TempROIBuffer, VisibleTriangles);
		TempROIBuffer = MoveTemp(VisibleTriangles);
	}

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		ActiveGroupSet->SetGroup(tid, ToGroupID, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}



bool UMeshGroupPaintTool::HaveVisibilityFilter() const
{
	return FilterProperties->VisibilityFilter != EMeshGroupPaintVisibilityType::None;
}


void UMeshGroupPaintTool::ApplyVisibilityFilter(TSet<int32>& Triangles, TArray<int32>& ROIBuffer, TArray<int32>& OutputBuffer)
{
	ROIBuffer.SetNum(0, false);
	ROIBuffer.Reserve(Triangles.Num());
	for (int32 tid : Triangles)
	{
		ROIBuffer.Add(tid);
	}
	
	OutputBuffer.Reset();
	ApplyVisibilityFilter(TempROIBuffer, OutputBuffer);

	Triangles.Reset();
	for (int32 tid : OutputBuffer)
	{
		TriangleROI.Add(tid);
	}
}

void UMeshGroupPaintTool::ApplyVisibilityFilter(const TArray<int32>& Triangles, TArray<int32>& VisibleTriangles)
{
	if (!HaveVisibilityFilter())
	{
		VisibleTriangles = Triangles;
		return;
	}

	FViewCameraState StateOut;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(StateOut);
	FTransform3d LocalToWorld = UE::ToolTarget::GetLocalToWorldTransform(Target);
	FVector3d LocalEyePosition(LocalToWorld.InverseTransformPosition(StateOut.Position));

	const FDynamicMesh3* Mesh = GetSculptMesh();

	int32 NumTriangles = Triangles.Num();

	VisibilityFilterBuffer.SetNum(NumTriangles, false);
	ParallelFor(NumTriangles, [&](int32 idx)
	{
		VisibilityFilterBuffer[idx] = true;
		FVector3d Centroid = Mesh->GetTriCentroid(Triangles[idx]);
		FVector3d FaceNormal = Mesh->GetTriNormal(Triangles[idx]);
		if (FaceNormal.Dot((Centroid - LocalEyePosition)) > 0)
		{
			VisibilityFilterBuffer[idx] = false;
		}
		if (FilterProperties->VisibilityFilter == EMeshGroupPaintVisibilityType::Unoccluded)
		{
			int32 HitTID = Octree.FindNearestHitObject(FRay3d(LocalEyePosition, UE::Geometry::Normalized(Centroid - LocalEyePosition)));
			if (HitTID != Triangles[idx])
			{
				VisibilityFilterBuffer[idx] = false;
			}
		}
	});

	VisibleTriangles.Reset();
	for (int32 k = 0; k < NumTriangles; ++k)
	{
		if (VisibilityFilterBuffer[k])
		{
			VisibleTriangles.Add(Triangles[k]);
		}
	}
}



int32 UMeshGroupPaintTool::FindHitSculptMeshTriangle(const FRay3d& LocalRay)
{
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

int32 UMeshGroupPaintTool::FindHitTargetMeshTriangle(const FRay3d& LocalRay)
{
	check(false);
	return IndexConstants::InvalidID;
}



bool UMeshGroupPaintTool::UpdateBrushPosition(const FRay& WorldRay)
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




bool UMeshGroupPaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	PendingStampType = FilterProperties->PrimaryBrushType;

	if(ensure(InStroke() == false))
	{
		UpdateBrushPosition(DevicePos.WorldRay);
	}
	return true;
}


void UMeshGroupPaintTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (PolyLassoMechanic)
	{
		// because the actual group change is deferred until mouse release, color the lasso to let the user know whether it will erase
		PolyLassoMechanic->LineColor = GetInEraseStroke() ? FLinearColor::Red : FLinearColor::Green;
		PolyLassoMechanic->DrawHUD(Canvas, RenderAPI);
	}
}


void UMeshGroupPaintTool::OnTick(float DeltaTime)
{
	UMeshSculptToolBase::OnTick(DeltaTime);
	MeshElementsDisplay->OnTick(DeltaTime);

	bool bIsLasso = (FilterProperties->SubToolType == EMeshGroupPaintInteractionType::PolyLasso);
	PolyLassoMechanic->SetIsEnabled(bIsLasso);

	ConfigureIndicator(FilterProperties->BrushAreaMode == EMeshGroupPaintBrushAreaType::Volumetric);
	SetIndicatorVisibility(bIsLasso == false);

	if (bHavePendingAction)
	{
		ApplyAction(PendingAction);
		bHavePendingAction = false;
		PendingAction = EMeshGroupPaintToolActions::NoAction;
	}

	SCOPE_CYCLE_COUNTER(GroupPaintToolTick);

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

	if (bPendingPickGroup || bPendingToggleFreezeGroup)
	{
		if (GetBrushTriangleID() >= 0 && IsStampPending() == false )
		{
			if (GetSculptMesh()->IsTriangle(GetBrushTriangleID()))
			{
				int32 HitGroupID = ActiveGroupSet->GetGroup(GetBrushTriangleID());
				if (bPendingPickGroup)
				{
					FilterProperties->SetGroup = HitGroupID;
					NotifyOfPropertyChangeByTool(FilterProperties);
				}
				else if (bPendingToggleFreezeGroup)
				{
					ToggleFrozenGroup(HitGroupID);
				}
			}
		}
		bPendingPickGroup = bPendingToggleFreezeGroup = false;
	}


	if (IsInBrushSubMode())
	{
		if (InStroke())
		{
			SCOPE_CYCLE_COUNTER(GroupPaintTool_Tick_ApplyStampBlock);

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
			TFuture<void> AccumulateROI = Async(GroupPaintToolAsyncExecTarget, [&]()
			{
				AccumulatedTriangleROI.Append(TriangleROI);
			});

			// apply the stamp
			bool bGroupsModified = ApplyStamp();

			if (bGroupsModified)
			{
				SCOPE_CYCLE_COUNTER(GroupPaintTool_Tick_UpdateMeshBlock);
				DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TriangleROI, EMeshRenderAttributeFlags::VertexColors);
				GetToolManager()->PostInvalidation();
			}

			// we don't really need to wait for these to happen to end Tick()...
			AccumulateROI.Wait();
		}
	}

}







void UMeshGroupPaintTool::AllocateNewGroupAndSetAsCurrentAction()
{
	FilterProperties->SetGroup = ActiveGroupSet->MaxGroupID;
	NotifyOfPropertyChangeByTool(FilterProperties);
}



FColor UMeshGroupPaintTool::GetColorForGroup(int32 GroupID)
{
	FColor Color = LinearColors::SelectFColor(GroupID);
	if (FrozenGroups.Contains(GroupID))
	{
		int32 GrayValue = (Color.R + Color.G + Color.B) / 3;
		Color.R = Color.G = Color.B = FMath::Clamp(GrayValue, 0, 255);
	}
	return Color;
}

void UMeshGroupPaintTool::ToggleFrozenGroup(int32 FreezeGroupID)
{
	if (FreezeGroupID == 0) return;

	TArray<int32> InitialFrozenGroups = FrozenGroups;
	if (FrozenGroups.Contains(FreezeGroupID))
	{
		FrozenGroups.Remove(FreezeGroupID);
	}
	else
	{
		FrozenGroups.Add(FreezeGroupID);
	}

	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		int32 TriGroupID = ActiveGroupSet->GetGroup(tid);
		if (TriGroupID == FreezeGroupID)
		{
			TempROIBuffer.Add(tid);
		}
	}
	EmitFrozenGroupsChange(InitialFrozenGroups, FrozenGroups, LOCTEXT("ToggleFrozenGroupChange", "Toggle Frozen Group"));
	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
}

void UMeshGroupPaintTool::FreezeOtherGroups(int32 KeepGroupID)
{
	TArray<int32> InitialFrozenGroups = FrozenGroups;
	FrozenGroups.Reset();
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		int32 GroupID = ActiveGroupSet->GetGroup(tid);
		if ( GroupID != 0 && GroupID != KeepGroupID)
		{
			FrozenGroups.AddUnique(ActiveGroupSet->GetGroup(tid));
			TempROIBuffer.Add(tid);
		}
	}
	EmitFrozenGroupsChange(InitialFrozenGroups, FrozenGroups, LOCTEXT("FreezeOtherGroups", "Freeze Other Groups"));
	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
}

void UMeshGroupPaintTool::ClearAllFrozenGroups()
{
	TArray<int32> InitialFrozenGroups = FrozenGroups;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		if ( FrozenGroups.Contains(ActiveGroupSet->GetGroup(tid)) )
		{
			TempROIBuffer.Add(tid);
		}
	}
	FrozenGroups.Reset();
	EmitFrozenGroupsChange(InitialFrozenGroups, FrozenGroups, LOCTEXT("ClearAllFrozenGroups", "Clear Frozen Groups"));
	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
}



void UMeshGroupPaintTool::EmitFrozenGroupsChange(const TArray<int32>& FromGroups, const TArray<int32>& ToGroups, const FText& ChangeText)
{
	if (FromGroups != ToGroups)
	{
		TUniquePtr<TSimpleValueLambdaChange<TArray<int32>>> FrozenGroupsChange = MakeUnique<TSimpleValueLambdaChange<TArray<int32>>>();
		FrozenGroupsChange->FromValue = FromGroups;
		FrozenGroupsChange->ToValue = ToGroups;
		FrozenGroupsChange->ValueChangeFunc = [this](UObject*, const TArray<int32>& FromGroups, const TArray<int32>& ToGroups, bool)
		{	
			FrozenGroups = ToGroups;
			DynamicMeshComponent->FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexColors);
		};
		GetToolManager()->EmitObjectChange(this, MoveTemp(FrozenGroupsChange), ChangeText);
	}
}


void UMeshGroupPaintTool::GrowCurrentGroupAction()
{
	BeginChange();

	int32 CurrentGroupID = FilterProperties->SetGroup;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	FMeshFaceSelection InitialSelection(Mesh);
	InitialSelection.Select([&](int32 tid) { return ActiveGroupSet->GetGroup(tid) == CurrentGroupID; });
	FMeshFaceSelection ExpandSelection(InitialSelection);
	ExpandSelection.ExpandToOneRingNeighbours([&](int32 tid) { return FrozenGroups.Contains(ActiveGroupSet->GetGroup(tid)) == false; });
	TempROIBuffer.SetNum(0, false);
	ExpandSelection.SetDifference(InitialSelection, TempROIBuffer);

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		ActiveGroupSet->SetGroup(tid, CurrentGroupID, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UMeshGroupPaintTool::ShrinkCurrentGroupAction()
{
	BeginChange();

	int32 CurrentGroupID = FilterProperties->SetGroup;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	FMeshFaceSelection InitialSelection(Mesh);
	InitialSelection.Select([&](int32 tid) { return ActiveGroupSet->GetGroup(tid) == CurrentGroupID; });
	FMeshFaceSelection ContractSelection(InitialSelection);
	ContractSelection.ContractBorderByOneRingNeighbours();
	TempROIBuffer.SetNum(0, false);
	InitialSelection.SetDifference(ContractSelection, TempROIBuffer);

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		// todo: could probably guess boundary groups here...
		ActiveGroupSet->SetGroup(tid, 0, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UMeshGroupPaintTool::ClearCurrentGroupAction()
{
	BeginChange();

	int32 CurrentGroupID = FilterProperties->SetGroup;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		if (ActiveGroupSet->GetGroup(tid) == CurrentGroupID)
		{
			TempROIBuffer.Add(tid);
		}
	}

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		ActiveGroupSet->SetGroup(tid, 0, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UMeshGroupPaintTool::FloodFillCurrentGroupAction()
{
	BeginChange();

	int32 SetGroupID = FilterProperties->SetGroup;
	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		int32 GroupID = ActiveGroupSet->GetGroup(tid);
		if (GroupID == 0 && GroupID != SetGroupID && FrozenGroups.Contains(GroupID) == false)
		{
			TempROIBuffer.Add(tid);
		}
	}

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		ActiveGroupSet->SetGroup(tid, SetGroupID, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}


void UMeshGroupPaintTool::ClearAllGroupsAction()
{
	BeginChange();

	const FDynamicMesh3* Mesh = DynamicMeshComponent->GetMesh();
	TempROIBuffer.SetNum(0, false);
	for (int32 tid : Mesh->TriangleIndicesItr())
	{
		if (ActiveGroupSet->GetGroup(tid) != 0)
		{
			TempROIBuffer.Add(tid);
		}
	}

	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);
	for (int32 tid : TempROIBuffer)
	{
		ActiveGroupSet->SetGroup(tid, 0, *GetSculptMesh());
	}
	ActiveGroupEditBuilder->SaveTriangles(TempROIBuffer);

	DynamicMeshComponent->FastNotifyTriangleVerticesUpdated(TempROIBuffer, EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
	EndChange();
}




//
// Change Tracking
//
void UMeshGroupPaintTool::BeginChange()
{
	check(ActiveGroupEditBuilder == nullptr);
	ActiveGroupEditBuilder = MakeUnique<FDynamicMeshGroupEditBuilder>(ActiveGroupSet.Get());
}

void UMeshGroupPaintTool::EndChange()
{
	check(ActiveGroupEditBuilder);

	TUniquePtr<FDynamicMeshGroupEdit> EditResult = ActiveGroupEditBuilder->ExtractResult();
	ActiveGroupEditBuilder = nullptr;

	TUniquePtr<TWrappedToolCommandChange<FMeshPolygroupChange>> NewChange = MakeUnique<TWrappedToolCommandChange<FMeshPolygroupChange>>();
	NewChange->WrappedChange = MakeUnique<FMeshPolygroupChange>(MoveTemp(EditResult));
	NewChange->BeforeModify = [this](bool bRevert)
	{
		this->WaitForPendingUndoRedo();
	};

	GetToolManager()->EmitObjectChange(DynamicMeshComponent, MoveTemp(NewChange), LOCTEXT("GroupPaintChange", "Group Stroke"));
}


void UMeshGroupPaintTool::WaitForPendingUndoRedo()
{
	if (bUndoUpdatePending)
	{
		bUndoUpdatePending = false;
	}
}

void UMeshGroupPaintTool::OnDynamicMeshComponentChanged(UDynamicMeshComponent* Component, const FMeshVertexChange* Change, bool bRevert)
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


void UMeshGroupPaintTool::PrecomputeFilterData()
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


void UMeshGroupPaintTool::OnSelectedGroupLayerChanged()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("ChangeActiveGroupLayer", "Change Polygroup Layer"));

	TArray<int32> InitialFrozenGroups = FrozenGroups;

	int32 ActiveLayerIndex = (ActiveGroupSet) ? ActiveGroupSet->GetPolygroupIndex() : -1;
	UpdateActiveGroupLayer();
	int32 NewLayerIndex = (ActiveGroupSet) ? ActiveGroupSet->GetPolygroupIndex() : -1;

	if (ActiveLayerIndex != NewLayerIndex)
	{
		// clear frozen groups
		EmitFrozenGroupsChange(InitialFrozenGroups, FrozenGroups, LOCTEXT("ClearAllFrozenGroups", "Clear Frozen Groups"));

		TUniquePtr<TSimpleValueLambdaChange<int32>> GroupLayerChange = MakeUnique<TSimpleValueLambdaChange<int32>>();
		GroupLayerChange->FromValue = ActiveLayerIndex;
		GroupLayerChange->ToValue = NewLayerIndex;
		GroupLayerChange->ValueChangeFunc = [this](UObject*, int32 FromIndex, int32 ToIndex, bool)
		{
			this->PolygroupLayerProperties->SetSelectedFromPolygroupIndex(ToIndex);
			this->PolygroupLayerProperties->SilentUpdateWatched();		// to prevent OnSelectedGroupLayerChanged() from being called immediately
			this->UpdateActiveGroupLayer();
		};
		GetToolManager()->EmitObjectChange(this, MoveTemp(GroupLayerChange), LOCTEXT("ChangeActiveGroupLayer", "Change Polygroup Layer"));
	}

	GetToolManager()->EndUndoTransaction();
}


void UMeshGroupPaintTool::UpdateActiveGroupLayer()
{
	if (PolygroupLayerProperties->HasSelectedPolygroup() == false)
	{
		ActiveGroupSet = MakeUnique<UE::Geometry::FPolygroupSet>(GetSculptMesh());
	}
	else
	{
		FName SelectedName = PolygroupLayerProperties->ActiveGroupLayer;
		FDynamicMeshPolygroupAttribute* FoundAttrib = UE::Geometry::FindPolygroupLayerByName(*GetSculptMesh(), SelectedName);
		ensureMsgf(FoundAttrib, TEXT("Selected Attribute Not Found! Falling back to Default group layer."));
		ActiveGroupSet = MakeUnique<UE::Geometry::FPolygroupSet>(GetSculptMesh(), FoundAttrib);
	}

	// need to reset everything here...
	FrozenGroups.Reset();

	// update colors
	DynamicMeshComponent->FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexColors);
	GetToolManager()->PostInvalidation();
}



void UMeshGroupPaintTool::UpdateSubToolType(EMeshGroupPaintInteractionType NewType)
{
	// Currenly we mirror base-brush properties in UGroupPaintBrushFilterProperties, so we never
	// want to show both
	//bool bSculptPropsVisible = (NewType == EMeshGroupPaintInteractionType::Brush);
	//SetToolPropertySourceEnabled(UMeshSculptToolBase::BrushProperties, bSculptPropsVisible);
	SetToolPropertySourceEnabled(UMeshSculptToolBase::BrushProperties, false);

	SetToolPropertySourceEnabled(FilterProperties, true);
	SetBrushOpPropsVisibility(false);
}


void UMeshGroupPaintTool::UpdateBrushType(EMeshGroupPaintBrushType BrushType)
{
	static const FText BaseMessage = LOCTEXT("OnStartTool", "Hold Shift to Erase. [/] and S/D change Size (+Shift to small-step). Shift+Q for New Group, Shift+G to pick Group, Shift+F to Freeze Group.");
	FTextBuilder Builder;
	Builder.AppendLine(BaseMessage);

	SetActivePrimaryBrushType((int32)BrushType);

	SetToolPropertySourceEnabled(GizmoProperties, false);

	GetToolManager()->DisplayMessage(Builder.ToText(), EToolMessageLevel::UserNotification);
}




void UMeshGroupPaintTool::RequestAction(EMeshGroupPaintToolActions ActionType)
{
	if (!bHavePendingAction)
	{
		PendingAction = ActionType;
		bHavePendingAction = true;
	}
}


void UMeshGroupPaintTool::ApplyAction(EMeshGroupPaintToolActions ActionType)
{
	switch (ActionType)
	{
	case EMeshGroupPaintToolActions::ClearFrozen:
		ClearAllFrozenGroups();
		break;

	case EMeshGroupPaintToolActions::FreezeCurrent:
		ToggleFrozenGroup(FilterProperties->SetGroup);
		break;

	case EMeshGroupPaintToolActions::FreezeOthers:
		FreezeOtherGroups(FilterProperties->SetGroup);
		break;

	case EMeshGroupPaintToolActions::GrowCurrent:
		GrowCurrentGroupAction();
		break;

	case EMeshGroupPaintToolActions::ShrinkCurrent:
		ShrinkCurrentGroupAction();
		break;

	case EMeshGroupPaintToolActions::ClearCurrent:
		ClearCurrentGroupAction();
		break;

	case EMeshGroupPaintToolActions::FloodFillCurrent:
		FloodFillCurrentGroupAction();
		break;

	case EMeshGroupPaintToolActions::ClearAll:
		ClearAllGroupsAction();
		break;

	}
}




#undef LOCTEXT_NAMESPACE

