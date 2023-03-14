// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/PolygonSelectionMechanic.h"

#include "BaseBehaviors/SingleClickOrDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "InteractiveToolManager.h"
#include "Util/ColorConstants.h"
#include "Selection/PersistentMeshSelection.h"
#include "ToolSceneQueriesUtil.h"
#include "ToolSetupUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PolygonSelectionMechanic)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UPolygonSelectionMechanic"

void UPolygonSelectionMechanicProperties::InvertSelection()
{
	if (Mechanic.IsValid())
	{
		Mechanic->InvertSelection();
	}
}

void UPolygonSelectionMechanicProperties::SelectAll()
{
	if (Mechanic.IsValid())
	{
		Mechanic->SelectAll();
	}
}

UPolygonSelectionMechanic::~UPolygonSelectionMechanic()
{
	checkf(PreviewGeometryActor == nullptr, TEXT("Shutdown() should be called before UPolygonSelectionMechanic is destroyed."));
}

void UPolygonSelectionMechanic::Setup(UInteractiveTool* ParentToolIn)
{
	UInteractionMechanic::Setup(ParentToolIn);

	TopoSelector = MakeShared<FGroupTopologySelector, ESPMode::ThreadSafe>();

	HoverBehavior = NewObject<UMouseHoverBehavior>();
	// We use modifiers on hover to change the highlighting according to what can be selected  	
	HoverBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	HoverBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(BasePriority);
	ParentToolIn->AddInputBehavior(HoverBehavior, this);

	MarqueeMechanic = NewObject<URectangleMarqueeMechanic>();
	MarqueeMechanic->bUseExternalClickDragBehavior = true;
	MarqueeMechanic->Setup(ParentToolIn);
	MarqueeMechanic->OnDragRectangleStarted.AddUObject(this, &UPolygonSelectionMechanic::OnDragRectangleStarted);
	MarqueeMechanic->OnDragRectangleChanged.AddUObject(this, &UPolygonSelectionMechanic::OnDragRectangleChanged);
	MarqueeMechanic->OnDragRectangleFinished.AddUObject(this, &UPolygonSelectionMechanic::OnDragRectangleFinished);
	MarqueeMechanic->SetBasePriority(BasePriority.MakeLower());

	ClickOrDragBehavior = NewObject<USingleClickOrDragInputBehavior>();
	ClickOrDragBehavior->Initialize(this, MarqueeMechanic);
	ClickOrDragBehavior->Modifiers.RegisterModifier(ShiftModifierID, FInputDeviceState::IsShiftKeyDown);
	ClickOrDragBehavior->Modifiers.RegisterModifier(CtrlModifierID, FInputDeviceState::IsCtrlKeyDown);
	ClickOrDragBehavior->SetDefaultPriority(BasePriority);
	ParentTool->AddInputBehavior(ClickOrDragBehavior, this);

	Properties = NewObject<UPolygonSelectionMechanicProperties>(this);
	Properties->Initialize(this);
	if (bAddSelectionFilterPropertiesToParentTool)
	{
		AddToolPropertySource(Properties);
	}
	Properties->WatchProperty(Properties->bSelectVertices, [this](bool bSelectVertices) {
		UpdateMarqueeEnabled(); 
	});
	Properties->WatchProperty(Properties->bSelectEdges, [this](bool bSelectVertices) { 
		UpdateMarqueeEnabled(); 
	});
	Properties->WatchProperty(Properties->bSelectFaces, [this](bool bSelectFaces) {
		UpdateMarqueeEnabled();
	});
	Properties->WatchProperty(Properties->bSelectEdgeLoops, [this](bool bSelectEdgeLoops) {
		UpdateMarqueeEnabled();
	});
	Properties->WatchProperty(Properties->bSelectEdgeRings, [this](bool bSelectEdgeRings) {
		UpdateMarqueeEnabled();
	});
	Properties->WatchProperty(Properties->bEnableMarquee, [this](bool bEnableMarquee) { 
		UpdateMarqueeEnabled(); 
	});

	// set up visualizers
	PolyEdgesRenderer.LineColor = FLinearColor::Red;
	PolyEdgesRenderer.LineThickness = 2.0;
	PolyEdgesRenderer.PointColor = FLinearColor::Red;
	PolyEdgesRenderer.PointSize = 8.0f;
	HilightRenderer.LineColor = FLinearColor::Green;
	HilightRenderer.LineThickness = 4.0f;
	HilightRenderer.PointColor = FLinearColor::Green;
	HilightRenderer.PointSize = 10.0f;
	SelectionRenderer.LineColor = LinearColors::Gold3f();
	SelectionRenderer.LineThickness = 4.0f;
	SelectionRenderer.PointColor = LinearColors::Gold3f();
	SelectionRenderer.PointSize = 10.0f;

	float HighlightedFacePercentDepthOffset = 0.5f;
	HighlightedFaceMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor::Green, ParentToolIn->GetToolManager(), HighlightedFacePercentDepthOffset);
	// The rest of the highlighting setup has to be done in Initialize(), since we need the world to set up our drawing component.
}

void UPolygonSelectionMechanic::Shutdown()
{
	if (PreviewGeometryActor)
	{
		PreviewGeometryActor->Destroy();
		PreviewGeometryActor = nullptr;
	}
}

void UPolygonSelectionMechanic::GetSelection(UPersistentMeshSelection& SelectionOut, const FCompactMaps* CompactMapsToApply) const
{
	SelectionOut.SetSelection(*Topology, PersistentSelection, CompactMapsToApply);
}

void UPolygonSelectionMechanic::LoadSelection(const UPersistentMeshSelection& SelectionIn)
{
	SelectionIn.ExtractIntoSelectionObject(*Topology, PersistentSelection);
}

void UPolygonSelectionMechanic::Initialize(
	const FDynamicMesh3* MeshIn,
	FTransform3d TargetTransformIn,
	UWorld* WorldIn,
	const FGroupTopology* TopologyIn,
	TFunction<FDynamicMeshAABBTree3 * ()> GetSpatialSourceFuncIn)
{
	this->Mesh = MeshIn;
	this->Topology = TopologyIn;
	this->TargetTransform = TargetTransformIn;

	TopoSelector->Initialize(Mesh, Topology);
	this->GetSpatialFunc = GetSpatialSourceFuncIn;
	TopoSelector->SetSpatialSource(GetSpatialFunc);
	TopoSelector->PointsWithinToleranceTest = [this](const FVector3d& Position1, const FVector3d& Position2, double TolScale) {
		if (CameraState.bIsOrthographic)
		{
			// We could just always use ToolSceneQueriesUtil::PointSnapQuery. But in ortho viewports, we happen to know
			// that the only points that we will ever give this function will be the closest points between a ray and
			// some geometry, meaning that the vector between them will be orthogonal to the view ray. With this knowledge,
			// we can do the tolerance computation more efficiently than PointSnapQuery can, since we don't need to project
			// down to the view plane.
			// As in PointSnapQuery, we convert our angle-based tolerance to one we can use in an ortho viewport (instead of
			// dividing our field of view into 90 visual angle degrees, we divide the plane into 90 units).
			float OrthoTolerance = ToolSceneQueriesUtil::GetDefaultVisualAngleSnapThreshD() * CameraState.OrthoWorldCoordinateWidth / 90.0;
			OrthoTolerance *= TolScale;
			return DistanceSquared( TargetTransform.TransformPosition(Position1), TargetTransform.TransformPosition(Position2) ) < OrthoTolerance * OrthoTolerance;
		}
		else
		{
			return ToolSceneQueriesUtil::PointSnapQuery(CameraState,
				TargetTransform.TransformPosition(Position1), TargetTransform.TransformPosition(Position2),
				ToolSceneQueriesUtil::GetDefaultVisualAngleSnapThreshD() * TolScale);
		}
	};

	// Set up the component we use to draw highlighted triangles. Only needs to be done once, not when the mesh
	// changes (we are assuming that we won't swap worlds without creating a new mechanic).
	if (PreviewGeometryActor == nullptr)
	{
		FRotator Rotation(0.0f, 0.0f, 0.0f);
		FActorSpawnParameters SpawnInfo;;
		PreviewGeometryActor = WorldIn->SpawnActor<APreviewGeometryActor>(FVector::ZeroVector, Rotation, SpawnInfo);

		DrawnTriangleSetComponent = NewObject<UTriangleSetComponent>(PreviewGeometryActor);
		PreviewGeometryActor->SetRootComponent(DrawnTriangleSetComponent);
		DrawnTriangleSetComponent->RegisterComponent();
	}

	PreviewGeometryActor->SetActorTransform((FTransform)TargetTransformIn);

	DrawnTriangleSetComponent->Clear();
	CurrentlyHighlightedGroups.Empty();
}

void UPolygonSelectionMechanic::Initialize(
	UDynamicMeshComponent* MeshComponentIn,
	const FGroupTopology* TopologyIn,
	TFunction<FDynamicMeshAABBTree3 * ()> GetSpatialSourceFuncIn)
{

	Initialize(MeshComponentIn->GetMesh(),
		(FTransform3d)MeshComponentIn->GetComponentTransform(),
		MeshComponentIn->GetWorld(),
		TopologyIn,
		GetSpatialSourceFuncIn);
}

void UPolygonSelectionMechanic::DisableBehaviors(UInteractiveTool* ParentToolIn)
{
	ParentToolIn->RemoveInputBehaviorsBySource(this);
	ParentToolIn->RemoveInputBehaviorsBySource(MarqueeMechanic);

	// TODO: Is it worth adding a way to remove the property watchers for marquee?
}

void UPolygonSelectionMechanic::SetIsEnabled(bool bOn)
{
	bIsEnabled = bOn;
	UpdateMarqueeEnabled();
}

void UPolygonSelectionMechanic::SetBasePriority(const FInputCapturePriority &Priority)
{
	BasePriority = Priority;
	if (ClickOrDragBehavior)
	{
		ClickOrDragBehavior->SetDefaultPriority(Priority);
	}
	if (HoverBehavior)
	{
		HoverBehavior->SetDefaultPriority(Priority);
	}
	if (MarqueeMechanic)
	{
		MarqueeMechanic->SetBasePriority(Priority.MakeLower());
	}
}

TPair<FInputCapturePriority, FInputCapturePriority> UPolygonSelectionMechanic::GetPriorityRange() const
{
	TPair<FInputCapturePriority, FInputCapturePriority> Result;
	Result.Key = BasePriority;
	Result.Value = MarqueeMechanic->GetPriorityRange().Value;
	return Result;
}

void UPolygonSelectionMechanic::Render(IToolsContextRenderAPI* RenderAPI)
{
	MarqueeMechanic->Render(RenderAPI);

	// Cache the view camera state so we can use for snapping/etc.
	// This should not happen in Render() though...
	GetParentTool()->GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(CameraState);

	FViewCameraState RenderCameraState = RenderAPI->GetCameraState();

	const FDynamicMesh3* TargetMesh = this->Mesh;
	FTransform Transform = (FTransform)TargetTransform;

	PolyEdgesRenderer.BeginFrame(RenderAPI, RenderCameraState);
	PolyEdgesRenderer.SetTransform(Transform);
	for (const FGroupTopology::FGroupEdge& Edge : Topology->Edges)
	{
		FVector3d A, B;
		for (int32 eid : Edge.Span.Edges)
		{
			TargetMesh->GetEdgeV(eid, A, B);
			PolyEdgesRenderer.DrawLine(A, B);
		}
	}
	if (bShowSelectableCorners)
	{
		for (const FGroupTopology::FCorner& Corner : Topology->Corners)
		{
			FVector3d A = TargetMesh->GetVertex(Corner.VertexID);
			PolyEdgesRenderer.DrawPoint(A);
		}
	}
	PolyEdgesRenderer.EndFrame();

	if (PersistentSelection.IsEmpty() == false)
	{
		SelectionRenderer.BeginFrame(RenderAPI, RenderCameraState);
		SelectionRenderer.SetTransform(Transform);
		TopoSelector->DrawSelection(PersistentSelection, &SelectionRenderer, &RenderCameraState);
		SelectionRenderer.EndFrame();
	}

	HilightRenderer.BeginFrame(RenderAPI, RenderCameraState);
	HilightRenderer.SetTransform(Transform);
	TopoSelector->DrawSelection(HilightSelection, &HilightRenderer, &RenderCameraState, FGroupTopologySelector::ECornerDrawStyle::Circle);
	HilightRenderer.EndFrame();
}

void UPolygonSelectionMechanic::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	MarqueeMechanic->DrawHUD(Canvas, RenderAPI);
}




void UPolygonSelectionMechanic::ClearHighlight()
{
	checkf(DrawnTriangleSetComponent != nullptr, TEXT("Initialize() not called on UPolygonSelectionMechanic."));

	HilightSelection.Clear();
	DrawnTriangleSetComponent->Clear();
	CurrentlyHighlightedGroups.Empty();
}


void UPolygonSelectionMechanic::NotifyMeshChanged(bool bTopologyModified)
{
	ClearHighlight();
	TopoSelector->Invalidate(true, bTopologyModified);
	if (bTopologyModified)
	{
		PersistentSelection.Clear();
		SelectionTimestamp++;
		OnSelectionChanged.Broadcast();
	}
}


bool UPolygonSelectionMechanic::TopologyHitTest(const FRay& WorldRay, FHitResult& OutHit, bool bUseOrthoSettings)
{
	FGroupTopologySelection Selection;
	return TopologyHitTest(WorldRay, OutHit, Selection, bUseOrthoSettings);
}

bool UPolygonSelectionMechanic::TopologyHitTest(const FRay& WorldRay, FHitResult& OutHit, FGroupTopologySelection& OutSelection, bool bUseOrthoSettings)
{
	FRay3d LocalRay(TargetTransform.InverseTransformPosition((FVector3d)WorldRay.Origin),
		TargetTransform.InverseTransformVector((FVector3d)WorldRay.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);

	FVector3d LocalPosition, LocalNormal;
	int32 EdgeSegmentId; // Only used if hit is an edge
	FGroupTopologySelector::FSelectionSettings TopoSelectorSettings = GetTopoSelectorSettings(bUseOrthoSettings);
	if (TopoSelector->FindSelectedElement(TopoSelectorSettings, LocalRay, OutSelection, LocalPosition, LocalNormal, &EdgeSegmentId) == false)
	{
		return false;
	}

	if (OutSelection.SelectedCornerIDs.Num() > 0)
	{
		OutHit.FaceIndex = OutSelection.GetASelectedCornerID();
		OutHit.Distance = LocalRay.GetParameter(LocalPosition);
		OutHit.ImpactPoint = (FVector)TargetTransform.TransformPosition(LocalRay.PointAt(OutHit.Distance));
	}
	else if (OutSelection.SelectedEdgeIDs.Num() > 0)
	{
		OutHit.FaceIndex = OutSelection.GetASelectedEdgeID();
		OutHit.Distance = LocalRay.GetParameter(LocalPosition);
		OutHit.ImpactPoint = (FVector)TargetTransform.TransformPosition(LocalRay.PointAt(OutHit.Distance));
		OutHit.Item = EdgeSegmentId;
	}
	else
	{
		FDynamicMeshAABBTree3* Spatial = GetSpatialFunc();
		int HitTID = Spatial->FindNearestHitTriangle(LocalRay);
		if (HitTID != IndexConstants::InvalidID)
		{
			FTriangle3d Triangle;
			Spatial->GetMesh()->GetTriVertices(HitTID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);
			FIntrRay3Triangle3d Query(LocalRay, Triangle);
			if (Query.Find())
			{
				OutHit.FaceIndex = HitTID;
				OutHit.Distance = (float)Query.RayParameter;
				OutHit.Normal = (FVector)TargetTransform.TransformVectorNoScale(Spatial->GetMesh()->GetTriNormal(HitTID));
				OutHit.ImpactPoint = (FVector)TargetTransform.TransformPosition(LocalRay.PointAt(Query.RayParameter));
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}



FGroupTopologySelector::FSelectionSettings UPolygonSelectionMechanic::GetTopoSelectorSettings(bool bUseOrthoSettings)
{
	FGroupTopologySelector::FSelectionSettings Settings;

	Settings.bEnableFaceHits = Properties->bSelectFaces;
	Settings.bEnableEdgeHits = Properties->bSelectEdges || Properties->bSelectEdgeLoops || Properties->bSelectEdgeRings;
	Settings.bEnableCornerHits = Properties->bSelectVertices;
	Settings.bHitBackFaces = Properties->bHitBackFaces;

	if (!PersistentSelection.IsEmpty() && (ShouldAddToSelectionFunc() || ShouldRemoveFromSelectionFunc()))
	{
		// If we have a selection and we're adding/removing/toggling elements make sure we only hit elements with compatible types
		Settings.bEnableFaceHits = Settings.bEnableFaceHits && PersistentSelection.SelectedGroupIDs.Num() > 0;
		Settings.bEnableEdgeHits = Settings.bEnableEdgeHits && PersistentSelection.SelectedEdgeIDs.Num() > 0;
		Settings.bEnableCornerHits = Settings.bEnableCornerHits && PersistentSelection.SelectedCornerIDs.Num() > 0;
	}

	if (bUseOrthoSettings)
	{
		Settings.bPreferProjectedElement = Properties->bPreferProjectedElement;
		Settings.bSelectDownRay = Properties->bSelectDownRay;
		Settings.bIgnoreOcclusion = Properties->bIgnoreOcclusion;
	}

	return Settings;
}




bool UPolygonSelectionMechanic::UpdateHighlight(const FRay& WorldRay)
{
	checkf(DrawnTriangleSetComponent != nullptr, TEXT("Initialize() not called on UPolygonSelectionMechanic."));

	FRay3d LocalRay(TargetTransform.InverseTransformPosition((FVector3d)WorldRay.Origin),
		TargetTransform.InverseTransformVector((FVector3d)WorldRay.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);

	HilightSelection.Clear();
	FVector3d LocalPosition, LocalNormal;
	FGroupTopologySelector::FSelectionSettings TopoSelectorSettings = GetTopoSelectorSettings(CameraState.bIsOrthographic);
	bool bHit = TopoSelector->FindSelectedElement(TopoSelectorSettings, LocalRay, HilightSelection, LocalPosition, LocalNormal);

	if (HilightSelection.SelectedEdgeIDs.Num() > 0 && Properties->bSelectEdgeRings && ShouldSelectEdgeRingsFunc())
	{
		TopoSelector->ExpandSelectionByEdgeRings(HilightSelection);
	}
	if (HilightSelection.SelectedEdgeIDs.Num() > 0 && Properties->bSelectEdgeLoops && ShouldSelectEdgeLoopsFunc())
	{
		TopoSelector->ExpandSelectionByEdgeLoops(HilightSelection);
	}

	// Don't hover highlight a selection that we already selected, because people didn't like that
	if (PersistentSelection.Contains(HilightSelection))
	{
		HilightSelection.Clear();
	}

	// Currently we draw highlighted edges/vertices differently from highlighted faces. Edges/vertices
	// get drawn in the Render() call, so it is sufficient to just update HighlightSelection above.
	// Faces, meanwhile, get placed into a Component that is rendered through the normal rendering system.
	// So, we need to update the component when the highlighted selection changes.

	// Put hovered groups in a set to easily compare to current
	TSet<int> NewlyHighlightedGroups;
	NewlyHighlightedGroups.Append(HilightSelection.SelectedGroupIDs);

	// See if we're currently highlighting any groups that we're not supposed to
	if (!NewlyHighlightedGroups.Includes(CurrentlyHighlightedGroups))
	{
		DrawnTriangleSetComponent->Clear();
		CurrentlyHighlightedGroups.Empty();
	}

	// See if we need to add any groups
	if (!CurrentlyHighlightedGroups.Includes(NewlyHighlightedGroups))
	{
		// Add triangles for each new group
		for (int Gid : HilightSelection.SelectedGroupIDs)
		{
			if (!CurrentlyHighlightedGroups.Contains(Gid))
			{
				for (int32 Tid : Topology->GetGroupTriangles(Gid))
				{
					// We use the triangle normals because the normal overlay isn't guaranteed to be valid as we edit the mesh
					FVector3d TriangleNormal = Mesh->GetTriNormal(Tid);

					// The UV's and colors here don't currently get used by HighlightedFaceMaterial, but we set them anyway
					FIndex3i VertIndices = Mesh->GetTriangle(Tid);
					DrawnTriangleSetComponent->AddTriangle(FRenderableTriangle(HighlightedFaceMaterial,
						FRenderableTriangleVertex((FVector)Mesh->GetVertex(VertIndices.A), (FVector2D)Mesh->GetVertexUV(VertIndices.A), (FVector)TriangleNormal, FLinearColor(Mesh->GetVertexColor(VertIndices.A)).ToFColor(true)),
						FRenderableTriangleVertex((FVector)Mesh->GetVertex(VertIndices.B), (FVector2D)Mesh->GetVertexUV(VertIndices.B), (FVector)TriangleNormal, FLinearColor(Mesh->GetVertexColor(VertIndices.B)).ToFColor(true)),
						FRenderableTriangleVertex((FVector)Mesh->GetVertex(VertIndices.C), (FVector2D)Mesh->GetVertexUV(VertIndices.C), (FVector)TriangleNormal, FLinearColor(Mesh->GetVertexColor(VertIndices.C)).ToFColor(true)) ));
				}

				CurrentlyHighlightedGroups.Add(Gid);
			}
		}//end iterating through groups
	}//end if groups need to be added

	return bHit;
}




bool UPolygonSelectionMechanic::HasSelection() const
{
	return PersistentSelection.IsEmpty() == false;
}


bool UPolygonSelectionMechanic::UpdateSelection(const FRay& WorldRay, FVector3d& LocalHitPositionOut, FVector3d& LocalHitNormalOut)
{
	FRay3d LocalRay(TargetTransform.InverseTransformPosition((FVector3d)WorldRay.Origin),
		TargetTransform.InverseTransformVector((FVector3d)WorldRay.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);

	const FGroupTopologySelection PreviousSelection = PersistentSelection;

	FVector3d LocalPosition, LocalNormal;
	FGroupTopologySelection Selection;
	FGroupTopologySelector::FSelectionSettings TopoSelectorSettings = GetTopoSelectorSettings(CameraState.bIsOrthographic);
	if (TopoSelector->FindSelectedElement(TopoSelectorSettings, LocalRay, Selection, LocalPosition, LocalNormal))
	{
		LocalHitPositionOut = LocalPosition;
		LocalHitNormalOut = LocalNormal;

		if (Selection.SelectedEdgeIDs.Num() > 0 && Properties->bSelectEdgeRings && ShouldSelectEdgeRingsFunc())
		{
			TopoSelector->ExpandSelectionByEdgeRings(Selection);
		}
		if (Selection.SelectedEdgeIDs.Num() > 0 && Properties->bSelectEdgeLoops && ShouldSelectEdgeLoopsFunc())
		{
			TopoSelector->ExpandSelectionByEdgeLoops(Selection);
		}
	}

	if (ShouldAddToSelectionFunc())
	{
		if (ShouldRemoveFromSelectionFunc())
		{
			PersistentSelection.Toggle(Selection);
		}
		else
		{
			PersistentSelection.Append(Selection);
		}
	}
	else if (ShouldRemoveFromSelectionFunc())
	{
		PersistentSelection.Remove(Selection);
	}
	else
	{
		PersistentSelection = Selection;
	}

	if (PersistentSelection != PreviousSelection)
	{
		SelectionTimestamp++;
		OnSelectionChanged.Broadcast();
		return true;
	}

	return false;
}


void UPolygonSelectionMechanic::SetSelection(const FGroupTopologySelection& Selection, bool bBroadcast)
{
	PersistentSelection = Selection;
	SelectionTimestamp++;
	if (bBroadcast)
	{
		OnSelectionChanged.Broadcast();
	}
}


void UPolygonSelectionMechanic::ClearSelection()
{
	PersistentSelection.Clear();
	SelectionTimestamp++;
	OnSelectionChanged.Broadcast();
}

void UPolygonSelectionMechanic::InvertSelection()
{
	if (PersistentSelection.IsEmpty())
	{
		SelectAll();
		return;
	}

	ParentTool->GetToolManager()->BeginUndoTransaction(LOCTEXT("SelectionChange", "Selection"));
	BeginChange();

	const FGroupTopologySelection PreviousSelection = PersistentSelection;
	PersistentSelection.Clear();

	if (!PreviousSelection.SelectedCornerIDs.IsEmpty())
	{
		for (int32 CornerID = 0; CornerID < Topology->Corners.Num(); ++CornerID)
		{
			if (!PreviousSelection.SelectedCornerIDs.Contains(CornerID))
			{
				PersistentSelection.SelectedCornerIDs.Add(CornerID);
			}
		}
	}
	else if (!PreviousSelection.SelectedEdgeIDs.IsEmpty())
	{
		for (int32 EdgeID = 0; EdgeID < Topology->Edges.Num(); ++EdgeID)
		{
			if (!PreviousSelection.SelectedEdgeIDs.Contains(EdgeID))
			{
				PersistentSelection.SelectedEdgeIDs.Add(EdgeID);
			}
		}
	}
	else if (!PreviousSelection.SelectedGroupIDs.IsEmpty())
	{
		for (const FGroupTopology::FGroup& Group : Topology->Groups)
		{
			if (!PreviousSelection.SelectedGroupIDs.Contains(Group.GroupID))
			{
				PersistentSelection.SelectedGroupIDs.Add(Group.GroupID);
			}
		}
	}

	SelectionTimestamp++;
	OnSelectionChanged.Broadcast();
	EndChangeAndEmitIfModified();
	ParentTool->GetToolManager()->EndUndoTransaction();
}

void UPolygonSelectionMechanic::SelectAll()
{
	const FGroupTopologySelection PreviousSelection = PersistentSelection;

	ParentTool->GetToolManager()->BeginUndoTransaction(LOCTEXT("SelectionChange", "Selection"));
	BeginChange();

	auto SelectAllIndices = [](int32 MaxExclusiveIndex, TSet<int32>& ContainerOut)
	{
		for (int32 i = 0; i < MaxExclusiveIndex; ++i)
		{
			ContainerOut.Add(i);
		}
	};

	PersistentSelection.Clear();

	// Select based on settings, prefering corners to edges to groups (since this is the preference we have
	// elsewhere, eg in marquee).
	if (Properties->bSelectVertices)
	{
		SelectAllIndices(Topology->Corners.Num(), PersistentSelection.SelectedCornerIDs);
	}
	else if (Properties->bSelectEdges || Properties->bSelectEdgeLoops || Properties->bSelectEdgeRings)
	{
		SelectAllIndices(Topology->Edges.Num(), PersistentSelection.SelectedEdgeIDs);
	}
	else if (Properties->bSelectFaces)
	{
		for (const FGroupTopology::FGroup& Group : Topology->Groups)
		{
			PersistentSelection.SelectedGroupIDs.Add(Group.GroupID);
		}
	}

	SelectionTimestamp++;
	OnSelectionChanged.Broadcast();
	
	if (PreviousSelection != PersistentSelection)
	{
		SelectionTimestamp++;
		OnSelectionChanged.Broadcast();
	}
	
	EndChangeAndEmitIfModified();
	ParentTool->GetToolManager()->EndUndoTransaction();
}

FInputRayHit UPolygonSelectionMechanic::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	if (!bIsEnabled)
	{
		return FInputRayHit(); // bHit is false
	}

	FHitResult OutHit;
	FGroupTopologySelection Selection;
	if (TopologyHitTest(ClickPos.WorldRay, OutHit, Selection, CameraState.bIsOrthographic))
	{
		return FInputRayHit(OutHit.Distance);
	}

	// Return a hit so we always capture and can clear the selection
	return FInputRayHit(TNumericLimits<float>::Max());
}

void UPolygonSelectionMechanic::OnClicked(const FInputDeviceRay& ClickPos)
{
	// update selection
	ParentTool->GetToolManager()->BeginUndoTransaction(LOCTEXT("SelectionChange", "Selection"));
	BeginChange();

	// This will fire off a OnSelectionChanged delegate.
	UpdateSelection(ClickPos.WorldRay, LastClickedHitPosition, LastClickedHitNormal);

	EndChangeAndEmitIfModified();
	ParentTool->GetToolManager()->EndUndoTransaction();
}

FInputRayHit UPolygonSelectionMechanic::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FHitResult OutHit;
	if (bIsEnabled && TopologyHitTest(PressPos.WorldRay, OutHit))
	{
		return FInputRayHit(OutHit.Distance);
	}
	return FInputRayHit(); // bHit is false
}

void UPolygonSelectionMechanic::OnBeginHover(const FInputDeviceRay& DevicePos)
{
}

bool UPolygonSelectionMechanic::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	UpdateHighlight(DevicePos.WorldRay);
	return true;
}

void UPolygonSelectionMechanic::OnEndHover()
{
	ClearHighlight();
}

void UPolygonSelectionMechanic::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	switch (ModifierID)
	{
	case ShiftModifierID:
		bShiftToggle = bIsOn;
		break;
	case CtrlModifierID:
		bCtrlToggle = bIsOn;
		break;
	default:
		break;
	}
}

void UPolygonSelectionMechanic::OnDragRectangleStarted()
{
	bCurrentlyMarqueeDragging = true;

	ParentTool->GetToolManager()->BeginUndoTransaction(LOCTEXT("SelectionChange", "Selection"));
	BeginChange();

	PreDragPersistentSelection = PersistentSelection;
	LastUpdateRectangleSelection = PersistentSelection;
	PreDragTopoSelectorSettings = GetTopoSelectorSettings(false);
	PreDragTopoSelectorSettings.bIgnoreOcclusion = Properties->bMarqueeIgnoreOcclusion; // uses a separate setting for marquee
}

void UPolygonSelectionMechanic::OnDragRectangleChanged(const FCameraRectangle& CurrentRectangle)
{
	FGroupTopologySelection RectangleSelection;

	TopoSelector->FindSelectedElement(PreDragTopoSelectorSettings, CurrentRectangle, TargetTransform,
		RectangleSelection, &TriIsOccludedCache);

	if (ShouldAddToSelectionFunc())
	{
		PersistentSelection = PreDragPersistentSelection;
		if (ShouldRemoveFromSelectionFunc())
		{
			PersistentSelection.Toggle(RectangleSelection);
		}
		else
		{
			PersistentSelection.Append(RectangleSelection);
		}
	}
	else if (ShouldRemoveFromSelectionFunc())
	{
		PersistentSelection = PreDragPersistentSelection;
		PersistentSelection.Remove(RectangleSelection);
	}
	else
	{
		// Neither key pressed.
		PersistentSelection = RectangleSelection;
	}

	// If we modified the currently selected edges/vertices, they will be properly displayed in our
	// Render() call. However, the mechanic is not responsible for face highlighting, so if we modified
	// that, we need to notify the user so that they can update the highlighting (since OnSelectionChanged
	// only gets broadcast at rectangle end).
	if ((!PersistentSelection.SelectedGroupIDs.IsEmpty() || !LastUpdateRectangleSelection.SelectedGroupIDs.IsEmpty()) // if groups are involved
		&& PersistentSelection != LastUpdateRectangleSelection)
	{
		LastUpdateRectangleSelection = PersistentSelection;
		OnFaceSelectionPreviewChanged.Broadcast();
	}
}

void UPolygonSelectionMechanic::OnDragRectangleFinished(const FCameraRectangle& Rectangle, bool bCancelled)
{
	bCurrentlyMarqueeDragging = false;

	TriIsOccludedCache.Reset();

	if (PersistentSelection != PreDragPersistentSelection)
	{
		SelectionTimestamp++;
		OnSelectionChanged.Broadcast();
	}

	EndChangeAndEmitIfModified();
	ParentTool->GetToolManager()->EndUndoTransaction();
}

void UPolygonSelectionMechanic::UpdateMarqueeEnabled()
{
	MarqueeMechanic->SetIsEnabled(
		bIsEnabled 
		&& Properties->bEnableMarquee
		&& (Properties->bSelectVertices || Properties->bSelectEdges || Properties->bSelectFaces
			|| Properties->bSelectEdgeLoops || Properties->bSelectEdgeRings));
}


void UPolygonSelectionMechanic::BeginChange()
{
	// If you hit this ensure, either you didn't match a BeginChange with an EndChange(), or you
	// interleaved two actions that both needed a BeginChange(). For instance, are you sure you
	// are not performing actions while a marquee rectangle is active?
	ensure(ActiveChange.IsValid() == false);
	ActiveChange = MakeUnique<FPolygonSelectionMechanicSelectionChange>();
	ActiveChange->Before = PersistentSelection;
	ActiveChange->Timestamp = SelectionTimestamp;
}

TUniquePtr<FToolCommandChange> UPolygonSelectionMechanic::EndChange()
{
	if (ensure(ActiveChange.IsValid()) && SelectionTimestamp != ActiveChange->Timestamp)
	{
		ActiveChange->After = PersistentSelection;
		return MoveTemp(ActiveChange);
	}
	ActiveChange = TUniquePtr<FPolygonSelectionMechanicSelectionChange>();
	return TUniquePtr<FToolCommandChange>();
}

bool UPolygonSelectionMechanic::EndChangeAndEmitIfModified()
{
	if (ensure(ActiveChange.IsValid()) && SelectionTimestamp != ActiveChange->Timestamp)
	{
		ActiveChange->After = PersistentSelection;
		GetParentTool()->GetToolManager()->EmitObjectChange(this, MoveTemp(ActiveChange),
			LOCTEXT("SelectionChangeMessage", "Selection Change"));
		return true;
	}
	ActiveChange = TUniquePtr<FPolygonSelectionMechanicSelectionChange>();
	return false;
}

void UPolygonSelectionMechanic::GetClickedHitPosition(FVector3d& PositionOut, FVector3d& NormalOut) const
{
	PositionOut = LastClickedHitPosition;
	NormalOut = LastClickedHitNormal;
}

FFrame3d UPolygonSelectionMechanic::GetSelectionFrame(bool bWorld, FFrame3d* InitialLocalFrame) const
{
	FFrame3d UseFrame;
	if (PersistentSelection.IsEmpty() == false)
	{
		UseFrame = Topology->GetSelectionFrame(PersistentSelection, InitialLocalFrame);
	}

	if (bWorld)
	{
		UseFrame.Transform(TargetTransform);
	}

	return UseFrame;
}

FAxisAlignedBox3d UPolygonSelectionMechanic::GetSelectionBounds(bool bWorld) const
{
	if ( ! PersistentSelection.IsEmpty() )
	{
		if (bWorld)
		{
			return Topology->GetSelectionBounds(PersistentSelection, [this](const FVector3d& Pos) { return TargetTransform.TransformPosition(Pos); });
		}
		else
		{
			return Topology->GetSelectionBounds(PersistentSelection, [this](const FVector3d& Pos) { return Pos; });
		}
	}
	else 
	{
		if (bWorld)
		{
			return FAxisAlignedBox3d(Mesh->GetBounds(), TargetTransform);
		}
		else
		{
			return Mesh->GetBounds();
		}
	}
}

void UPolygonSelectionMechanic::SetShowSelectableCorners(bool bShowCorners)
{
	bShowSelectableCorners = bShowCorners;
}



void FPolygonSelectionMechanicSelectionChange::Apply(UObject* Object)
{
	UPolygonSelectionMechanic* Mechanic = Cast<UPolygonSelectionMechanic>(Object);
	if (Mechanic)
	{
		Mechanic->PersistentSelection = After;
		Mechanic->OnSelectionChanged.Broadcast();
	}
}
void FPolygonSelectionMechanicSelectionChange::Revert(UObject* Object)
{
	UPolygonSelectionMechanic* Mechanic = Cast<UPolygonSelectionMechanic>(Object);
	if (Mechanic)
	{
		Mechanic->PersistentSelection = Before;
		Mechanic->OnSelectionChanged.Broadcast();
	}
}
FString FPolygonSelectionMechanicSelectionChange::ToString() const
{
	return TEXT("FPolygonSelectionMechanicSelectionChange");
}



#undef LOCTEXT_NAMESPACE
