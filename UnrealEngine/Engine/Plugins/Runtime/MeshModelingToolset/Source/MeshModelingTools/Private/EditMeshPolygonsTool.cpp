// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMeshPolygonsTool.h"

#include "Algo/ForEach.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "BaseGizmos/TransformProxy.h"
#include "CompGeom/PolygonTriangulation.h"
#include "ConstrainedDelaunay2.h"
#include "Components/BrushComponent.h"
#include "ContextObjectStore.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshIndexUtil.h" // TriangleToVertexIDs
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "FaceGroupUtil.h"
#include "GroupTopology.h"
#include "InteractiveGizmoManager.h"
#include "InteractiveToolManager.h"
#include "Mechanics/DragAlignmentMechanic.h"
#include "MeshBoundaryLoops.h"
#include "MeshOpPreviewHelpers.h" // UMeshOpPreviewWithBackgroundCompute
#include "MeshRegionBoundaryLoops.h"
#include "ModelingToolTargetUtil.h" // UE::ToolTarget:: functions
#include "Operations/SimpleHoleFiller.h"
#include "Operations/MinimalHoleFiller.h"
#include "Operations/PolygroupRemesh.h"
#include "Operations/WeldEdgeSequence.h"
#include "Selection/PersistentMeshSelection.h"
#include "Selection/StoredMeshSelectionUtil.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "Selections/MeshConnectedComponents.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionCommitter.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ToolActivities/PolyEditActivityContext.h"
#include "ToolActivities/PolyEditExtrudeActivity.h"
#include "ToolActivities/PolyEditInsertEdgeActivity.h"
#include "ToolActivities/PolyEditInsertEdgeLoopActivity.h"
#include "ToolActivities/PolyEditInsetOutsetActivity.h"
#include "ToolActivities/PolyEditCutFacesActivity.h"
#include "ToolActivities/PolyEditPlanarProjectionUVActivity.h"
#include "ToolActivities/PolyEditBevelEdgeActivity.h"
#include "ToolContextInterfaces.h" // FToolBuilderState
#include "ToolSetupUtil.h"
#include "ToolTargetManager.h"
#include "TransformTypes.h"
#include "Util/CompactMaps.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditMeshPolygonsTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UEditMeshPolygonsTool"

namespace EditMeshPolygonsToolLocals
{
	FText PolyEditDefaultMessage = LOCTEXT("OnStartEditMeshPolygonsTool_TriangleMode", "Select triangles to edit mesh. Use middle mouse on gizmo to "
		"reposition it. Hold Ctrl while translating or (in local mode) rotating to align to scene. Shift and Ctrl "
		"change marquee select behavior. Q toggles Gizmo Orientation Lock.");

	FText TriEditDefaultMessage = LOCTEXT("OnStartEditMeshPolygonsTool", "Select PolyGroups to edit mesh. Use middle mouse on gizmo to reposition it. "
		"Hold Ctrl while translating or (in local mode) rotating to align to scene. Shift and Ctrl change marquee select "
		"behavior. Q toggles Gizmo Orientation Lock.");

	FString GetPropertyCacheIdentifier(bool bTriangleMode)
	{
		return bTriangleMode ? TEXT("TriEditTool") : TEXT("PolyEditTool");
	}

	TAutoConsoleVariable<int32> CVarEdgeLimit(
		TEXT("modeling.PolyEdit.EdgeLimit"),
		60000,
		TEXT("Maximal number of edges that PolyEd and TriEd support. Meshes that would require "
			"more than this number of edges to be rendered in PolyEd or TriEd force the tools to "
			"be disabled to avoid hanging the editor."));

	// Allows undo/redo of addition of extra corners in the group topology based on user angle thresholds.
	// Used after user-triggered topology corner changes where the mesh was not actually edited.
	class FExtraCornerChange : public FToolCommandChange
	{
	public:
		FExtraCornerChange(const TSet<int32>& BeforeIn, const TSet<int32>& AfterIn)
			: Before(BeforeIn)
			, After(AfterIn)
		{
		}
		virtual void Apply(UObject* Object) override 
		{
			Cast<UEditMeshPolygonsTool>(Object)->RebuildTopologyWithGivenExtraCorners(After);
		}
		virtual void Revert(UObject* Object) override
		{
			Cast<UEditMeshPolygonsTool>(Object)->RebuildTopologyWithGivenExtraCorners(Before);
		}
		virtual bool HasExpired(UObject* Object) const override
		{
			return false;
		}
		virtual FString ToString() const override
		{
			return TEXT("FExtraCornerChange");
		}

	protected:
		TSet<int32> Before;
		TSet<int32> After;
	};
}

/*
 * ToolBuilder
 */


USingleSelectionMeshEditingTool* UEditMeshPolygonsToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UEditMeshPolygonsTool>(SceneState.ToolManager);
}

void UEditMeshPolygonsToolBuilder::InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const
{
	USingleSelectionMeshEditingToolBuilder::InitializeNewTool(Tool, SceneState);
	UEditMeshPolygonsTool* EditPolygonsTool = CastChecked<UEditMeshPolygonsTool>(Tool);
	if (bTriangleMode)
	{
		EditPolygonsTool->EnableTriangleMode();
	}
}



void UEditMeshPolygonsActionModeToolBuilder::InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const
{
	UEditMeshPolygonsToolBuilder::InitializeNewTool(Tool, SceneState);
	UEditMeshPolygonsTool* EditPolygonsTool = CastChecked<UEditMeshPolygonsTool>(Tool);

	EEditMeshPolygonsToolActions UseAction = StartupAction;
	EditPolygonsTool->PostSetupFunction = [UseAction](UEditMeshPolygonsTool* PolyTool)
	{
		PolyTool->SetToSelectionModeInterface();
		PolyTool->RequestAction(UseAction);
	};
}



void UEditMeshPolygonsSelectionModeToolBuilder::InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const
{
	UEditMeshPolygonsToolBuilder::InitializeNewTool(Tool, SceneState);
	UEditMeshPolygonsTool* EditPolygonsTool = CastChecked<UEditMeshPolygonsTool>(Tool);

	EEditMeshPolygonsToolSelectionMode UseMode = SelectionMode;
	EditPolygonsTool->PostSetupFunction = [UseMode](UEditMeshPolygonsTool* PolyTool)
	{
		PolyTool->SetToSelectionModeInterface();

		UPolygonSelectionMechanic* SelectionMechanic = PolyTool->SelectionMechanic;
		UPolygonSelectionMechanicProperties* SelectionProps = SelectionMechanic->Properties;
		SelectionProps->bSelectFaces = SelectionProps->bSelectEdges = SelectionProps->bSelectVertices = false;
		SelectionProps->bSelectEdgeLoops = SelectionProps->bSelectEdgeRings = false;

		switch (UseMode)
		{
		default:
		case EEditMeshPolygonsToolSelectionMode::Faces:
			SelectionProps->bSelectFaces = true;
			break;
		case EEditMeshPolygonsToolSelectionMode::Edges:
			SelectionProps->bSelectEdges = true;
			break;
		case EEditMeshPolygonsToolSelectionMode::Vertices:
			SelectionProps->bSelectVertices = true;
			break;
		case EEditMeshPolygonsToolSelectionMode::Loops:
			SelectionProps->bSelectEdges = true;
			SelectionProps->bSelectEdgeLoops = true;
			break;
		case EEditMeshPolygonsToolSelectionMode::Rings:
			SelectionProps->bSelectEdges = true;
			SelectionProps->bSelectEdgeRings = true;
			break;
		case EEditMeshPolygonsToolSelectionMode::FacesEdgesVertices:
			SelectionProps->bSelectFaces = SelectionProps->bSelectEdges = SelectionProps->bSelectVertices = true;
			break;
		}
	};
}


void UEditMeshPolygonsTool::SetToSelectionModeInterface()
{
	if (EditActions) SetToolPropertySourceEnabled(EditActions, false);
	if (EditEdgeActions) SetToolPropertySourceEnabled(EditEdgeActions, false);
	if (EditUVActions) SetToolPropertySourceEnabled(EditUVActions, false);
}



void UEditMeshPolygonsToolActionPropertySet::PostAction(EEditMeshPolygonsToolActions Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}

/*
* Tool methods
*/

UEditMeshPolygonsTool::UEditMeshPolygonsTool()
{
	SetToolDisplayName(LOCTEXT("EditMeshPolygonsToolName", "PolyGroup Edit"));
}

void UEditMeshPolygonsTool::EnableTriangleMode()
{
	check(Preview == nullptr);		// must not have been initialized!
	bTriangleMode = true;
}

void UEditMeshPolygonsTool::Setup()
{
	using namespace EditMeshPolygonsToolLocals;

	// TODO: Currently we draw all the edges in the tool with PDI and can lock up the editor on high-res meshes. 
	// As a hack, disable everything if the number of edges is too high, so that user doesn't lose work accidentally
	// if they start the tool on the wrong thing.
	int32 MaxEdges = CVarEdgeLimit.GetValueOnGameThread();

	CurrentMesh = MakeShared<FDynamicMesh3>(UE::ToolTarget::GetDynamicMeshCopy(Target));
	WorldTransform = UE::ToolTarget::GetLocalToWorldTransform(Target);
	FVector ScaleToBake = WorldTransform.GetScale();
	BakedTransform = FTransformSRT3d(FQuaterniond::Identity(), FVector::Zero(), ScaleToBake);
	WorldTransform.SetScale(FVector::One());
	MeshTransforms::ApplyTransform(*CurrentMesh, BakedTransform, true);

	if (bTriangleMode)
	{
		bToolDisabled = CurrentMesh->EdgeCount() > MaxEdges;
		if (bToolDisabled)
		{
			GetToolManager()->DisplayMessage(FText::Format(
				LOCTEXT("TriEditTooManyEdges", 
					"This tool is currently disallowed from operating on a mesh of this resolution. "
					"Current limit set by \"modeling.PolyEdit.EdgeLimit\" is {0} edges, and mesh has "
					"{1}. Limit can be changed but exists to avoid hanging the editor when trying to "
					"render too many edges using the current system, so make sure to save your work "
					"if you change the upper limit and try to edit a very dense mesh."),
				MaxEdges, CurrentMesh->EdgeCount()), EToolMessageLevel::UserError);
			return;
		}

		Topology = MakeShared<FTriangleGroupTopology, ESPMode::ThreadSafe>(CurrentMesh.Get(), false);
	}
	else
	{
		Topology = MakeShared<FGroupTopology, ESPMode::ThreadSafe>(CurrentMesh.Get(), false);
		
		Topology->ShouldAddExtraCornerAtVert = 
			[this](const FGroupTopology& GroupTopology, int32 Vid, const FIndex2i& AttachedGroupEdgeEids)
		{
			if (!TopologyProperties->bAddExtraCorners)
			{
				return false;
			}

			// Note: it's important that we don't use CurrentMesh here. It's possible that an activity might create a copy of 
			// the topology that uses the same corner forcing function but points to a different mesh, so we want to use
			// whatever mesh the passed-in topology uses.
			const FDynamicMesh3* Mesh = GroupTopology.GetMesh();
			
			if (!ensure(Mesh->IsEdge(AttachedGroupEdgeEids.A) && Mesh->IsEdge(AttachedGroupEdgeEids.B)))
			{
				return false;
			}

			// Gets vector pointing from the Vid along the edge.
			auto GetEdgeUnitVector = [Mesh, Vid](int32 Eid, FVector3d& VectorOut)->bool
			{
				FIndex2i EdgeVids = Mesh->GetEdgeV(Eid);
				// Make sure that the Vid is at EdgeVids.A
				if (EdgeVids.B == Vid)
				{
					Swap(EdgeVids.A, EdgeVids.B);
				}
				VectorOut = Mesh->GetVertex(EdgeVids.B) - Mesh->GetVertex(EdgeVids.A);
				return VectorOut.Normalize(KINDA_SMALL_NUMBER);
			};

			FVector Edge1, Edge2;
			if (!GetEdgeUnitVector(AttachedGroupEdgeEids.A, Edge1) || !GetEdgeUnitVector(AttachedGroupEdgeEids.B, Edge2))
			{
				// If either edge was degenerate, we won't consider this a corner because otherwise we will end up
				// with two corners connected by the degenerate edge, which is not ideal.
				return false;
			}

			return Edge1.Dot(Edge2) >= ExtraCornerDotProductThreshold;
		};
	}

	TopologyProperties = NewObject<UPolyEditTopologyProperties>(this);
	TopologyProperties->Initialize(this);
	TopologyProperties->RestoreProperties(this, GetPropertyCacheIdentifier(bTriangleMode));

	auto UpdateExtraCornerThreshold = [this]() { ExtraCornerDotProductThreshold = FMathd::Cos(TopologyProperties->ExtraCornerAngleThresholdDegrees * FMathd::DegToRad); };
	UpdateExtraCornerThreshold();
	TopologyProperties->WatchProperty(TopologyProperties->ExtraCornerAngleThresholdDegrees,
		[this, UpdateExtraCornerThreshold](double) { UpdateExtraCornerThreshold(); });

	Topology->RebuildTopology();

	if (!bTriangleMode)
	{
		int32 NumEdgesToRender = 0;
		for (const FGroupTopology::FGroupEdge& Edge : Topology->Edges)
		{
			NumEdgesToRender += Edge.Span.Edges.Num();
		}

		bToolDisabled = NumEdgesToRender > MaxEdges;
		if (bToolDisabled)
		{
			GetToolManager()->DisplayMessage(FText::Format(
				LOCTEXT("PolyEditTooManyEdges",
					"This tool is currently disallowed from operating on a group topology of this resolution. "
					"Current limit set by \"modeling.PolyEdit.EdgeLimit\" is {0} displayed edges, and topology has "
					"{1} edge segments to display. Limit can be changed, but it exists to avoid hanging the editor "
					"when trying to render too many edges using the current system, so make sure to save your work "
					"if you change the upper limit and try to edit a very complicated topology."),
				MaxEdges, NumEdgesToRender), EToolMessageLevel::UserError);
			return;
		}
	}

	// Start by adding the actions, because we want them at the top.
	if (bTriangleMode)
	{
		EditActions_Triangles = NewObject<UEditMeshPolygonsToolActions_Triangles>();
		EditActions_Triangles->Initialize(this);
		AddToolPropertySource(EditActions_Triangles);

		EditEdgeActions_Triangles = NewObject<UEditMeshPolygonsToolEdgeActions_Triangles>();
		EditEdgeActions_Triangles->Initialize(this);
		AddToolPropertySource(EditEdgeActions_Triangles);

		SetToolDisplayName(LOCTEXT("EditMeshTrianglesToolName", "Triangle Edit"));
		DefaultMessage = PolyEditDefaultMessage;
	}
	else
	{
		EditActions = NewObject<UEditMeshPolygonsToolActions>();
		EditActions->Initialize(this);
		AddToolPropertySource(EditActions);

		EditEdgeActions = NewObject<UEditMeshPolygonsToolEdgeActions>();
		EditEdgeActions->Initialize(this);
		AddToolPropertySource(EditEdgeActions);

		EditUVActions = NewObject<UEditMeshPolygonsToolUVActions>();
		EditUVActions->Initialize(this);
		AddToolPropertySource(EditUVActions);

		DefaultMessage = TriEditDefaultMessage;
	}

	GetToolManager()->DisplayMessage(DefaultMessage,
		EToolMessageLevel::UserNotification);

	// We add an empty line for the error message so that things don't jump when we use it.
	GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserWarning);

	CancelAction = NewObject<UEditMeshPolygonsToolCancelAction>();
	CancelAction->Initialize(this);
	AddToolPropertySource(CancelAction);
	SetToolPropertySourceEnabled(CancelAction, false);

	AcceptCancelAction = NewObject<UEditMeshPolygonsToolAcceptCancelAction>();
	AcceptCancelAction->Initialize(this);
	AddToolPropertySource(AcceptCancelAction);
	SetToolPropertySourceEnabled(AcceptCancelAction, false);


	// Initialize the common properties but don't add them yet, because we want them to be under the activity-specific ones.
	CommonProps = NewObject<UPolyEditCommonProperties>(this);
	CommonProps->RestoreProperties(this, GetPropertyCacheIdentifier(bTriangleMode));

	CommonProps->WatchProperty(CommonProps->LocalFrameMode,
		[this](ELocalFrameMode) { UpdateGizmoFrame(); });
	CommonProps->WatchProperty(CommonProps->bLockRotation,
		[this](bool) { LockedTransfomerFrame = LastTransformerFrame; });
	CommonProps->WatchProperty(CommonProps->bGizmoVisible,
		[this](bool)
		{
			if (!CurrentActivity)
			{
				UpdateGizmoVisibility();
				ResetUserMessage();
			}
		});

	// We are going to SilentUpdate here because otherwise the Watches above will immediately fire
	// and cause UpdateGizmoFrame() to be called emitting a spurious Transform change. 
	CommonProps->SilentUpdateWatched();

	// TODO: Do we need this?
	FMeshNormals::QuickComputeVertexNormals(*CurrentMesh);

	// Create the preview object
	Preview = NewObject<UMeshOpPreviewWithBackgroundCompute>();
	Preview->Setup(TargetWorld);
	ToolSetupUtil::ApplyRenderingConfigurationToPreview(Preview->PreviewMesh, Target); 
	Preview->PreviewMesh->SetTransform((FTransform)WorldTransform);

	// We'll use the spatial inside preview mesh mainly for the convenience of having it update automatically.
	Preview->PreviewMesh->bBuildSpatialDataStructure = true;

	// set materials
	FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
	Preview->ConfigureMaterials(MaterialSet.Materials, ToolSetupUtil::GetDefaultWorkingMaterial(GetToolManager()));

	// configure secondary render material
	UMaterialInterface* SelectionMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor::Yellow, GetToolManager());
	if (SelectionMaterial != nullptr)
	{
		// Note that you have to do it this way rather than reaching into the PreviewMesh because the background compute
		// mesh has to be able to swap in/out a working material and restore the primary/secondary ones.
		Preview->SecondaryMaterial = SelectionMaterial;
	}

	Preview->PreviewMesh->EnableSecondaryTriangleBuffers(
		[this](const FDynamicMesh3* Mesh, int32 TriangleID)
	{
		return SelectionMechanic->GetActiveSelection().IsSelectedTriangle(Mesh, Topology.Get(), TriangleID);
	});

	Preview->PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	Preview->PreviewMesh->UpdatePreview(CurrentMesh.Get());
	Preview->PreviewMesh->EnableWireframe(CommonProps->bShowWireframe);
	Preview->SetVisibility(true);

	// initialize AABBTree
	MeshSpatial = MakeShared<FDynamicMeshAABBTree3>();
	MeshSpatial->SetMesh(CurrentMesh.Get());

	// set up SelectionMechanic
	SelectionMechanic = NewObject<UPolygonSelectionMechanic>(this);
	SelectionMechanic->bAddSelectionFilterPropertiesToParentTool = false; // We'll do this ourselves later
	SelectionMechanic->Setup(this);
	SelectionMechanic->SetShowSelectableCorners(CommonProps->bShowSelectableCorners);
	SelectionMechanic->Properties->RestoreProperties(this, GetPropertyCacheIdentifier(bTriangleMode));
	SelectionMechanic->Properties->bDisplayPolygroupReliantControls = !bTriangleMode;
	SelectionMechanic->OnSelectionChanged.AddUObject(this, &UEditMeshPolygonsTool::OnSelectionModifiedEvent);
	SelectionMechanic->OnFaceSelectionPreviewChanged.AddWeakLambda(this, [this]() {
		Preview->PreviewMesh->FastNotifySecondaryTrianglesChanged();
	});
	if (bTriangleMode)
	{
		SelectionMechanic->PolyEdgesRenderer.LineThickness = 1.0;
	}
	SelectionMechanic->Initialize(CurrentMesh.Get(),
		(FTransform3d)Preview->PreviewMesh->GetTransform(),
		TargetWorld,
		Topology.Get(),
		[this]() { return &GetSpatial(); }
	);

	LinearDeformer.Initialize(CurrentMesh.Get(), Topology.Get());

	// Have to load selection after initializing the selection mechanic since we need to have
	// the topology built.
	if (HasInputSelection() && IsToolInputSelectionUsable(GetInputSelection()))
	{
		SelectionMechanic->LoadSelection(*GetInputSelection());
	}

	bSelectionStateDirty = SelectionMechanic->HasSelection();

	// Set UV Scale factor based on initial mesh bounds
	float BoundsMaxDim = CurrentMesh->GetBounds().MaxDim();
	if (BoundsMaxDim > 0)
	{
		UVScaleFactor = 1.0 / BoundsMaxDim;
	}

	// Wrap the data structures into a context that we can give to the activities
	ActivityContext = NewObject<UPolyEditActivityContext>();
	ActivityContext->bTriangleMode = bTriangleMode;
	ActivityContext->CommonProperties = CommonProps;
	ActivityContext->CurrentMesh = CurrentMesh;
	ActivityContext->Preview = Preview;
	ActivityContext->CurrentTopology = Topology;
	ActivityContext->MeshSpatial = MeshSpatial;
	ActivityContext->SelectionMechanic = SelectionMechanic;
	ActivityContext->EmitActivityStart = [this](const FText& TransactionLabel)
	{
		EmitActivityStart(TransactionLabel);
	};
	ActivityContext->EmitCurrentMeshChangeAndUpdate = [this](const FText& TransactionLabel,
		TUniquePtr<UE::Geometry::FDynamicMeshChange> MeshChangeIn,
		const UE::Geometry::FGroupTopologySelection& OutputSelection) 
	{
		EmitCurrentMeshChangeAndUpdate(TransactionLabel, MoveTemp(MeshChangeIn), OutputSelection);
	};
	GetToolManager()->GetContextObjectStore()->RemoveContextObjectsOfType(UPolyEditActivityContext::StaticClass());
	GetToolManager()->GetContextObjectStore()->AddContextObject(ActivityContext);

	ExtrudeActivity = NewObject<UPolyEditExtrudeActivity>();
	ExtrudeActivity->Setup(this);
	
	InsetOutsetActivity = NewObject<UPolyEditInsetOutsetActivity>();
	InsetOutsetActivity->Setup(this);

	CutFacesActivity = NewObject<UPolyEditCutFacesActivity>();
	CutFacesActivity->Setup(this);

	PlanarProjectionUVActivity = NewObject<UPolyEditPlanarProjectionUVActivity>();
	PlanarProjectionUVActivity->Setup(this);

	InsertEdgeLoopActivity = NewObject<UPolyEditInsertEdgeLoopActivity>();
	InsertEdgeLoopActivity->Setup(this);

	InsertEdgeActivity = NewObject<UPolyEditInsertEdgeActivity>();
	InsertEdgeActivity->Setup(this);

	BevelEdgeActivity = NewObject<UPolyEditBevelEdgeActivity>();
	BevelEdgeActivity->Setup(this);

	// Now that we've initialized the activities, add in the selection settings and 
	// CommonProps so that they are at the bottom.
	AddToolPropertySource(SelectionMechanic->Properties);
	AddToolPropertySource(CommonProps);
	if (!bTriangleMode)
	{
		AddToolPropertySource(TopologyProperties);
	}
	else
	{
		// Not actually necessary since we don't use the forcing function in triangle mode, but might as well turn it off here too.
		TopologyProperties->bAddExtraCorners = false;
	}

	// hide input StaticMeshComponent
	UE::ToolTarget::HideSourceObject(Target);

	UInteractiveGizmoManager* GizmoManager = GetToolManager()->GetPairedGizmoManager();
	
	TransformGizmo = UE::TransformGizmoUtil::CreateCustomRepositionableTransformGizmo(GizmoManager,
		ETransformGizmoSubElements::FullTranslateRotateScale, this);
	if (ensure(TransformGizmo)) // If we don't get a valid gizmo a lot of interactions won't work, but at least we won't crash
	{
		// Stop scaling at 0 rather than going negative
		TransformGizmo->SetDisallowNegativeScaling(true);
		// We allow non uniform scale even when the gizmo mode is set to "world" because we're not scaling components- we're
		// moving vertices, so we don't care which axes we "scale" along.
		TransformGizmo->SetIsNonUniformScaleAllowedFunction([]() {
			return true;
			});

		// Hook up callbacks
		TransformProxy = NewObject<UTransformProxy>(this);
		TransformProxy->OnTransformChanged.AddUObject(this, &UEditMeshPolygonsTool::OnGizmoTransformChanged);
		TransformProxy->OnBeginTransformEdit.AddUObject(this, &UEditMeshPolygonsTool::OnBeginGizmoTransform);
		TransformProxy->OnEndTransformEdit.AddUObject(this, &UEditMeshPolygonsTool::OnEndGizmoTransform);
		TransformProxy->OnEndPivotEdit.AddWeakLambda(this, [this](UTransformProxy* Proxy) {
			LastTransformerFrame = FFrame3d(Proxy->GetTransform());
			if (CommonProps->bLockRotation)
			{
				LockedTransfomerFrame = LastTransformerFrame;
			}
			});
		TransformGizmo->SetActiveTarget(TransformProxy, GetToolManager());
		TransformGizmo->SetVisibility(false);
	}

	DragAlignmentMechanic = NewObject<UDragAlignmentMechanic>(this);
	DragAlignmentMechanic->Setup(this);
	DragAlignmentMechanic->InitializeDeformedMeshRayCast(
		[this]() { return &GetSpatial(); },
		WorldTransform, &LinearDeformer); // Should happen after LinearDeformer is initialized

	if (TransformGizmo)
	{
		DragAlignmentMechanic->AddToGizmo(TransformGizmo);
	}

	if (Topology->Groups.Num() < 2)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("NoGroupsWarning",
			        "This object has only a single Polygroup. Use the GrpGen, GrpPnt or TriSel (Create Polygroup) tools to modify PolyGroups."),
			EToolMessageLevel::UserWarning);
	}

	if (PostSetupFunction)
	{
		PostSetupFunction(this);
	}
}

void UEditMeshPolygonsTool::ResetUserMessage()
{
	// When the gizmo is hidden, notify the user and
	// specify the toggle hotkey to prevent panic
	if (!TransformGizmo->IsVisible())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("ToggleTransformGizmoNotify", "Transform Gizmo Hidden, Press 'R' to Unhide"),
			EToolMessageLevel::UserNotification);
	}
	else
	{
		GetToolManager()->DisplayMessage(
			DefaultMessage,
			EToolMessageLevel::UserNotification);
	}
}

bool UEditMeshPolygonsTool::IsToolInputSelectionUsable(const UPersistentMeshSelection* InputSelectionIn)
{
	// TODO: We currently don't support persistent selection on volume brushes because
	// a conversion back to a brush involves a simplification step that may make the 
	// same vids unrecoverable. Once we have persistence of dynamic meshes, this will
	// hopefully not become a problem, and this function (along with stored selection
	// identifying info) will change.
	return !Cast<UBrushComponent>(UE::ToolTarget::GetTargetComponent(Target))

		&& InputSelectionIn
		&& InputSelectionIn->GetSelectionType() == (bTriangleMode ?
			FGenericMeshSelection::ETopologyType::FTriangleGroupTopology
			: FGenericMeshSelection::ETopologyType::FGroupTopology)
		&& InputSelectionIn->GetTargetComponent() == UE::ToolTarget::GetTargetComponent(Target)
		&& !InputSelectionIn->IsEmpty();
}

void UEditMeshPolygonsTool::OnShutdown(EToolShutdownType ShutdownType)
{
	using namespace EditMeshPolygonsToolLocals;

	if (bToolDisabled)
	{
		CurrentMesh.Reset();
		Topology.Reset();
		return;
	}

	if (CurrentActivity)
	{
		CurrentActivity->End(ShutdownType);
		CurrentActivity = nullptr;
	}
	CommonProps->SaveProperties(this, GetPropertyCacheIdentifier(bTriangleMode));
	SelectionMechanic->Properties->SaveProperties(this, GetPropertyCacheIdentifier(bTriangleMode));
	TopologyProperties->SaveProperties(this, GetPropertyCacheIdentifier(bTriangleMode));

	GetToolManager()->GetContextObjectStore()->RemoveContextObjectsOfType(UPolyEditActivityContext::StaticClass());
	ActivityContext = nullptr;

	ExtrudeActivity->Shutdown(ShutdownType);
	InsetOutsetActivity->Shutdown(ShutdownType);
	CutFacesActivity->Shutdown(ShutdownType);
	PlanarProjectionUVActivity->Shutdown(ShutdownType);
	InsertEdgeActivity->Shutdown(ShutdownType);
	InsertEdgeLoopActivity->Shutdown(ShutdownType);
	BevelEdgeActivity->Shutdown(ShutdownType);

	GetToolManager()->GetPairedGizmoManager()->DestroyAllGizmosByOwner(this);

	DragAlignmentMechanic->Shutdown();
	// We wait to shut down the selection mechanic in case we need to do work to store the selection.

	if (Preview != nullptr)
	{
		UE::ToolTarget::ShowSourceObject(Target);

		if (ShutdownType == EToolShutdownType::Accept)
		{
			UPersistentMeshSelection* OutputSelection = nullptr;
			FCompactMaps CompactMaps;

			// Prep if we have a selection to store. We don't support storing selections for volumes
			// because the conversion will change vids.
			if (!SelectionMechanic->GetActiveSelection().IsEmpty()
				&& !Cast<UBrushComponent>(UE::ToolTarget::GetTargetComponent(Target)))
			{
				OutputSelection = NewObject<UPersistentMeshSelection>();
				FGenericMeshSelection NewSelection;
				NewSelection.SourceComponent = UE::ToolTarget::GetTargetComponent(Target);
				NewSelection.TopologyType = (bTriangleMode ?
					FGenericMeshSelection::ETopologyType::FTriangleGroupTopology
					: FGenericMeshSelection::ETopologyType::FGroupTopology);
				OutputSelection->SetSelection(NewSelection);
			}

			// Note: When not in triangle mode, ModifiedTopologyCounter refers to polygroup topology, so does not tell us
			// about the triangle topology.  In this case, we just assume the triangle topology may have been modified.
			bool bModifiedTriangleTopology = bTriangleMode ? ModifiedTopologyCounter > 0 : true;

			// may need to compact the mesh if we did undo on a mesh edit, then vertices will be dense but compact checks will fail...
			if (bModifiedTriangleTopology)
			{
				// Store the compact maps if we have a selection that we need to update
				CurrentMesh->CompactInPlace(OutputSelection ? &CompactMaps : nullptr);
			}

			// Finish prepping the stored selection
			if (OutputSelection)
			{
				SelectionMechanic->GetSelection(*OutputSelection, bModifiedTriangleTopology ? &CompactMaps : nullptr);
			}

			// Bake CurrentMesh back to target inside an undo transaction
			GetToolManager()->BeginUndoTransaction(LOCTEXT("EditMeshPolygonsToolTransactionName", "Deform Mesh"));
			MeshTransforms::ApplyTransformInverse(*CurrentMesh, BakedTransform, true);
			UE::ToolTarget::CommitDynamicMeshUpdate(Target, *CurrentMesh, bModifiedTriangleTopology);

			UE::Geometry::SetToolOutputSelection(this, OutputSelection);
		
			GetToolManager()->EndUndoTransaction();
		}

		Preview->Shutdown();
		Preview = nullptr;
	}

	// The seleciton mechanic shutdown has to happen after (potentially) saving selection above
	SelectionMechanic->Shutdown();

	// We null out as many pointers as we can because the tool pointer usually ends up sticking
	// around in the undo stack.
	TargetWorld = nullptr;
	CommonProps = nullptr;
	EditActions = nullptr;
	EditActions_Triangles = nullptr;
	EditEdgeActions = nullptr;
	EditEdgeActions_Triangles = nullptr;
	EditUVActions = nullptr;
	CancelAction = nullptr;
	AcceptCancelAction = nullptr;

	ExtrudeActivity = nullptr;
	InsetOutsetActivity = nullptr;
	CutFacesActivity = nullptr;
	PlanarProjectionUVActivity = nullptr;
	InsertEdgeActivity = nullptr;
	InsertEdgeLoopActivity = nullptr;
	BevelEdgeActivity = nullptr;

	SelectionMechanic = nullptr;
	DragAlignmentMechanic = nullptr;

	TransformGizmo = nullptr;
	TransformProxy = nullptr;

	CurrentMesh.Reset();
	Topology.Reset();
	MeshSpatial.Reset();

}


void UEditMeshPolygonsTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 2,
		TEXT("ToggleLockRotation"),
		LOCTEXT("ToggleLockRotationUIName", "Lock Rotation"),
		LOCTEXT("ToggleLockRotationTooltip", "Toggle Frame Rotation Lock on and off"),
		EModifierKey::None, EKeys::Q,
		[this]() { CommonProps->bLockRotation = !CommonProps->bLockRotation; });
	
	// Backspace and delete both trigger deletion (as long as the delete button is also enabled)
	auto OnDeletionKeyPress = [this]() 
	{
		if ((EditActions && EditActions->IsPropertySetEnabled())
			|| (EditActions_Triangles && EditActions_Triangles->IsPropertySetEnabled()))
		{
			RequestAction(EEditMeshPolygonsToolActions::Delete);
		}
	};
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 3,
		TEXT("DeleteSelectionBackSpaceKey"),
		LOCTEXT("DeleteSelectionUIName", "Delete Selection"),
		LOCTEXT("DeleteSelectionTooltip", "Delete Selection"),
		EModifierKey::None, EKeys::BackSpace, OnDeletionKeyPress);

	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 4,
		TEXT("DeleteSelectionDeleteKey"),
		LOCTEXT("DeleteSelectionUIName", "Delete Selection"),
		LOCTEXT("DeleteSelectionTooltip", "Delete Selection"),
		EModifierKey::None, EKeys::Delete, OnDeletionKeyPress);
	ActionSet.RegisterAction(this, (int32)EStandardToolActions::BaseClientDefinedActionID + 5,
		TEXT("ToggleTransformGizmoAKey"),
		LOCTEXT("ToggleTransformGizmoUIName", "Toggle Transform Gizmo Visibility"),
		LOCTEXT("ToggleTransformGizmoTooltip", "Toggle Transform Gizmo Visibility"),
		EModifierKey::None, EKeys::R,
		[this]() 
		{
			if (!CurrentActivity)
			{
				CommonProps->bGizmoVisible = !CommonProps->bGizmoVisible;
			}
		});

	// TODO: Esc should be made to exit out of current activity if one is active. However this
	// requires a bit of work because we don't seem to be able to register conditional actions,
	// and we don't want to always capture Esc.
}


void UEditMeshPolygonsTool::RequestAction(EEditMeshPolygonsToolActions ActionType)
{
	if (SelectionMechanic && SelectionMechanic->IsCurrentlyMarqueeDragging())
	{
		PendingAction = EEditMeshPolygonsToolActions::NoAction;
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotActDuringMarquee", "Cannot perform action while marquee selecting"), 
			EToolMessageLevel::UserWarning);
		return;
	}

	if (PendingAction != EEditMeshPolygonsToolActions::NoAction)
	{
		return;
	}

	PendingAction = ActionType;
}

FDynamicMeshAABBTree3& UEditMeshPolygonsTool::GetSpatial()
{
	if (bSpatialDirty)
	{
		MeshSpatial->Build();
		bSpatialDirty = false;
	}
	return *MeshSpatial;
}

void UEditMeshPolygonsTool::UpdateGizmoFrame(const FFrame3d* UseFrame)
{
	FFrame3d SetFrame = LastTransformerFrame;
	if (UseFrame == nullptr)
	{
		if (CommonProps->LocalFrameMode == ELocalFrameMode::FromGeometry)
		{
			SetFrame = LastGeometryFrame;
		}
		else
		{
			SetFrame = FFrame3d(LastGeometryFrame.Origin, WorldTransform.GetRotation());
		}
	}
	else
	{
		SetFrame = *UseFrame;
	}

	if (CommonProps->bLockRotation)
	{
		SetFrame.Rotation = LockedTransfomerFrame.Rotation;
	}

	LastTransformerFrame = SetFrame;

	if (TransformGizmo)
	{
		// This resets the scale as well
		TransformGizmo->ReinitializeGizmoTransform(SetFrame.ToFTransform());
	}
}


FBox UEditMeshPolygonsTool::GetWorldSpaceFocusBox()
{
	if (ensure(SelectionMechanic))
	{
		FAxisAlignedBox3d Bounds = SelectionMechanic->GetSelectionBounds(true);
		return (FBox)Bounds;
	}
	return FBox(EForceInit::ForceInit);
}

bool UEditMeshPolygonsTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	FRay3d LocalRay(WorldTransform.InverseTransformPosition((FVector3d)WorldRay.Origin),
		WorldTransform.InverseTransformVector((FVector3d)WorldRay.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);

	int32 HitTID = GetSpatial().FindNearestHitTriangle(LocalRay);
	if (HitTID != IndexConstants::InvalidID)
	{
		FIntrRay3Triangle3d TriHit = TMeshQueries<FDynamicMesh3>::TriangleIntersection(*GetSpatial().GetMesh(), HitTID, LocalRay);
		FVector3d LocalPos = LocalRay.PointAt(TriHit.RayParameter);
		PointOut = (FVector)WorldTransform.TransformPosition(LocalPos);
		return true;
	}
	return false;
}


void UEditMeshPolygonsTool::OnSelectionModifiedEvent()
{
	bSelectionStateDirty = true;
}


void UEditMeshPolygonsTool::OnBeginGizmoTransform(UTransformProxy* Proxy)
{
	SelectionMechanic->ClearHighlight();
	UpdateDeformerFromSelection( SelectionMechanic->GetActiveSelection() );
	
	FTransform Transform = Proxy->GetTransform();
	InitialGizmoFrame = FFrame3d(Transform);
	InitialGizmoScale = FVector3d(Transform.GetScale3D());

	BeginDeformerChange();

	bInGizmoDrag = true;
}

void UEditMeshPolygonsTool::OnGizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform)
{
	if (bInGizmoDrag)
	{
		LastUpdateGizmoFrame = FFrame3d(Transform);
		LastUpdateGizmoScale = FVector3d(Transform.GetScale3D());
		GetToolManager()->PostInvalidation();
		bGizmoUpdatePending = true;
		bLastUpdateUsedWorldFrame = (TransformGizmo ? TransformGizmo->CurrentCoordinateSystem == EToolContextCoordinateSystem::World : false);
	}
}

void UEditMeshPolygonsTool::OnEndGizmoTransform(UTransformProxy* Proxy)
{
	bInGizmoDrag = false;
	bGizmoUpdatePending = false;
	bSpatialDirty = true;
	SelectionMechanic->NotifyMeshChanged(false);

	FFrame3d TransformFrame(Proxy->GetTransform());

	if (TransformGizmo)
	{
		if (CommonProps->bLockRotation)
		{
			FFrame3d SetFrame = TransformFrame;
			SetFrame.Rotation = LockedTransfomerFrame.Rotation;
			TransformGizmo->ReinitializeGizmoTransform(SetFrame.ToFTransform());		
		}
		else
		{
			TransformGizmo->SetNewChildScale(FVector::OneVector);
		}
	}

	LastTransformerFrame = TransformFrame;

	// close change record
	EndDeformerChange();
}


void UEditMeshPolygonsTool::UpdateDeformerFromSelection(const FGroupTopologySelection& Selection)
{
	//Determine which of the following (corners, edges or faces) has been selected by counting the associated feature's IDs
	if (Selection.SelectedCornerIDs.Num() > 0)
	{
		//Add all the the Corner's adjacent poly-groups (NbrGroups) to the ongoing array of groups.
		LinearDeformer.SetActiveHandleCorners(Selection.SelectedCornerIDs.Array());
	}
	else if (Selection.SelectedEdgeIDs.Num() > 0)
	{
		//Add all the the edge's adjacent poly-groups (NbrGroups) to the ongoing array of groups.
		LinearDeformer.SetActiveHandleEdges(Selection.SelectedEdgeIDs.Array());
	}
	else if (Selection.SelectedGroupIDs.Num() > 0)
	{
		LinearDeformer.SetActiveHandleFaces(Selection.SelectedGroupIDs.Array());
	}
}

void UEditMeshPolygonsTool::ComputeUpdate_Gizmo()
{
	if (SelectionMechanic->HasSelection() == false || bGizmoUpdatePending == false)
	{
		return;
	}
	bGizmoUpdatePending = false;

	FFrame3d CurFrame = LastUpdateGizmoFrame;
	FVector3d CurScale = LastUpdateGizmoScale;
	FVector3d TranslationDelta = CurFrame.Origin - InitialGizmoFrame.Origin;
	FQuaterniond RotateDelta = CurFrame.Rotation - InitialGizmoFrame.Rotation;
	FVector3d CurScaleDelta = CurScale - InitialGizmoScale;
	FVector3d LocalTranslation = WorldTransform.InverseTransformVector(TranslationDelta);

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	if (TranslationDelta.SquaredLength() > 0.0001 || RotateDelta.SquaredLength() > 0.0001 || CurScaleDelta.SquaredLength() > 0.0001)
	{
		if (bLastUpdateUsedWorldFrame)
		{
			// For a world frame gizmo, the scaling needs to happen in world aligned gizmo space, but the 
			// rotation is still encoded in the local gizmo frame change.
			FQuaterniond RotationToApply = CurFrame.Rotation * InitialGizmoFrame.Rotation.Inverse();
			LinearDeformer.UpdateSolution(Mesh, [&](FDynamicMesh3* TargetMesh, int VertIdx)
			{
				FVector3d PosLocal = TargetMesh->GetVertex(VertIdx);
				FVector3d PosWorld = WorldTransform.TransformPosition(PosLocal);
				FVector3d PosWorldGizmo = PosWorld - InitialGizmoFrame.Origin;

				FVector3d NewPosWorld = RotationToApply * (PosWorldGizmo * CurScale) + CurFrame.Origin;
				FVector3d NewPosLocal = WorldTransform.InverseTransformPosition(NewPosWorld);
				return NewPosLocal;
			});
		}
		else
		{
			LinearDeformer.UpdateSolution(Mesh, [&](FDynamicMesh3* TargetMesh, int VertIdx)
			{
				// For a local gizmo, we just get the coordinates in the original frame, scale in that frame,
				// then interpret them as coordinates in the new frame.
				FVector3d PosLocal = TargetMesh->GetVertex(VertIdx);
				FVector3d PosWorld = WorldTransform.TransformPosition(PosLocal);
				FVector3d PosGizmo = InitialGizmoFrame.ToFramePoint(PosWorld);
				PosGizmo = CurScale * PosGizmo;
				FVector3d NewPosWorld = CurFrame.FromFramePoint(PosGizmo);
				FVector3d NewPosLocal = WorldTransform.InverseTransformPosition(NewPosWorld);
				return NewPosLocal;
			});
		}
	}
	else
	{
		// Reset mesh to initial positions.
		LinearDeformer.ClearSolution(Mesh);
	}

	Preview->PreviewMesh->UpdatePreview(CurrentMesh.Get(),
		// It's important to use the fast update path for the gizmo manipulations that only
		// affect positions.
		UPreviewMesh::ERenderUpdateMode::FastUpdate,
		EMeshRenderAttributeFlags::Positions | EMeshRenderAttributeFlags::VertexNormals);

	GetToolManager()->PostInvalidation();
}



void UEditMeshPolygonsTool::OnTick(float DeltaTime)
{
	if (bToolDisabled)
	{
		return;
	}

	Preview->Tick(DeltaTime);

	if (CurrentActivity)
	{
		CurrentActivity->Tick(DeltaTime);
	}

	bool bLocalCoordSystem = GetToolManager()->GetPairedGizmoManager()
		->GetContextQueriesAPI()->GetCurrentCoordinateSystem() == EToolContextCoordinateSystem::Local;
	if (CommonProps->bLocalCoordSystem != bLocalCoordSystem)
	{
		CommonProps->bLocalCoordSystem = bLocalCoordSystem;
		NotifyOfPropertyChangeByTool(CommonProps);
	}

	if (bGizmoUpdatePending)
	{
		ComputeUpdate_Gizmo();
	}

	if (bSelectionStateDirty)
	{
		// update color highlights
		Preview->PreviewMesh->FastNotifySecondaryTrianglesChanged();

		UpdateGizmoVisibility();

		bSelectionStateDirty = false;
	}

	if (PendingAction != EEditMeshPolygonsToolActions::NoAction)
	{
		// Clear any existing error messages.
		GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserWarning);

		switch (PendingAction)
		{

		//Interactive operations:
		case EEditMeshPolygonsToolActions::Extrude:
		{
			ExtrudeActivity->ExtrudeMode = FExtrudeOp::EExtrudeMode::MoveAndStitch;
			ExtrudeActivity->PropertySetToUse = UPolyEditExtrudeActivity::EPropertySetToUse::Extrude;
			StartActivity(ExtrudeActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::PushPull:
		{
			ExtrudeActivity->ExtrudeMode = FExtrudeOp::EExtrudeMode::Boolean;
			ExtrudeActivity->PropertySetToUse = UPolyEditExtrudeActivity::EPropertySetToUse::PushPull;
			StartActivity(ExtrudeActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::Offset:
		{
			ExtrudeActivity->ExtrudeMode = FExtrudeOp::EExtrudeMode::MoveAndStitch;
			ExtrudeActivity->PropertySetToUse = UPolyEditExtrudeActivity::EPropertySetToUse::Offset;
			StartActivity(ExtrudeActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::Inset:
		{
			InsetOutsetActivity->Settings->bOutset = false;
			StartActivity(InsetOutsetActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::Outset:
		{
			InsetOutsetActivity->Settings->bOutset = true;
			StartActivity(InsetOutsetActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::CutFaces:
		{
			StartActivity(CutFacesActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::PlanarProjectionUV:
		{
			StartActivity(PlanarProjectionUVActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::InsertEdge:
		{
			StartActivity(InsertEdgeActivity);
			break;
		}
		case EEditMeshPolygonsToolActions::InsertEdgeLoop:
		{
			StartActivity(InsertEdgeLoopActivity);
			break;
		}

		case EEditMeshPolygonsToolActions::BevelFaces:
		case EEditMeshPolygonsToolActions::BevelEdges:
		{
			StartActivity(BevelEdgeActivity);
			break;
		}

		case EEditMeshPolygonsToolActions::CancelCurrent:
		{
			EndCurrentActivity(EToolShutdownType::Cancel);
			break;
		}
		case EEditMeshPolygonsToolActions::AcceptCurrent:
		{
			EndCurrentActivity(EToolShutdownType::Accept);
			break;
		}

		// Single action operations:
		case EEditMeshPolygonsToolActions::Merge:
			ApplyMerge();
			break;
		case  EEditMeshPolygonsToolActions::Delete:
			ApplyDelete();
			break;
		case EEditMeshPolygonsToolActions::RecalculateNormals:
			ApplyRecalcNormals();
			break;
		case EEditMeshPolygonsToolActions::FlipNormals:
			ApplyFlipNormals();
			break;
		case EEditMeshPolygonsToolActions::CollapseEdge:
			ApplyCollapseEdge();
			break;
		case EEditMeshPolygonsToolActions::WeldEdges:
			ApplyWeldEdges();
			break;
		case EEditMeshPolygonsToolActions::StraightenEdge:
			ApplyStraightenEdges();
			break;
		case EEditMeshPolygonsToolActions::FillHole:
			ApplyFillHole();
			break;
		case EEditMeshPolygonsToolActions::BridgeEdges:
			ApplyBridgeEdges();
			break;
		case EEditMeshPolygonsToolActions::Retriangulate:
			ApplyRetriangulate();
			break;
		case EEditMeshPolygonsToolActions::Decompose:
			ApplyDecompose();
			break;
		case EEditMeshPolygonsToolActions::Disconnect:
			ApplyDisconnect();
			break;
		case EEditMeshPolygonsToolActions::Duplicate:
			ApplyDuplicate();
			break;
		case EEditMeshPolygonsToolActions::PokeSingleFace:
			ApplyPokeSingleFace();
			break;
		case EEditMeshPolygonsToolActions::SplitSingleEdge:
			ApplySplitSingleEdge();
			break;
		case EEditMeshPolygonsToolActions::CollapseSingleEdge:
			ApplyCollapseSingleEdge();
			break;
		case EEditMeshPolygonsToolActions::FlipSingleEdge:
			ApplyFlipSingleEdge();
			break;
		case EEditMeshPolygonsToolActions::SimplifyByGroups:
			SimplifyByGroups();
			break;
		case EEditMeshPolygonsToolActions::RegenerateExtraCorners:
			ApplyRegenerateExtraCorners();
			break;
		}

		PendingAction = EEditMeshPolygonsToolActions::NoAction;
	}
}

void UEditMeshPolygonsTool::StartActivity(TObjectPtr<UInteractiveToolActivity> Activity)
{
	EndCurrentActivity();

	// Right now we rely on the activity to fail to start or to issue an error message if the
	// conditions are not right. Someday, we are going to disable the buttons based on a CanStart
	// call.
	if (Activity->Start() == EToolActivityStartResult::Running)
	{
		if (TransformGizmo)
		{
			TransformGizmo->SetVisibility(false);
		}
		SelectionMechanic->SetIsEnabled(false);
		SetToolPropertySourceEnabled(SelectionMechanic->Properties, false);
		SetToolPropertySourceEnabled(TopologyProperties, false);
		CurrentActivity = Activity;
		if (CurrentActivity->HasAccept())
		{
			SetToolPropertySourceEnabled(AcceptCancelAction, true);
		}
		else
		{
			SetToolPropertySourceEnabled(CancelAction, true);
		}
		SetActionButtonPanelsVisible(false);
	}
}

void UEditMeshPolygonsTool::EndCurrentActivity(EToolShutdownType ShutdownType)
{
	if (CurrentActivity)
	{
		if (CurrentActivity->IsRunning())
		{
			CurrentActivity->End(ShutdownType);
		}

		CurrentActivity = nullptr;
		++ActivityTimestamp;

		SetToolPropertySourceEnabled(CancelAction, false);
		SetToolPropertySourceEnabled(AcceptCancelAction, false);
		SetActionButtonPanelsVisible(true);
		SelectionMechanic->SetIsEnabled(true);
		SetToolPropertySourceEnabled(TopologyProperties, true);
		SetToolPropertySourceEnabled(SelectionMechanic->Properties, true);
		UpdateGizmoVisibility();
	}

	// If an activity displays a notification, it should be
	// overwritten with an appropriate notification once finished
	ResetUserMessage();
}

void UEditMeshPolygonsTool::NotifyActivitySelfEnded(UInteractiveToolActivity* Activity)
{
	EndCurrentActivity();
}

void UEditMeshPolygonsTool::UpdateGizmoVisibility()
{
	// Only allow gizmo to become visible if something is selected,
	// the gizmo isn't hidden, and there is no current activity.
	if (SelectionMechanic->HasSelection() && CommonProps->bGizmoVisible && !CurrentActivity)
	{
		if (TransformGizmo)
		{
			TransformGizmo->SetVisibility(true);
		}

		// Update frame because we might be here due to an undo event/etc,
		// rather than an explicit selection change
		LastGeometryFrame = SelectionMechanic->GetSelectionFrame(true, &LastGeometryFrame);
		UpdateGizmoFrame();
	}
	else if(TransformGizmo)
	{
		TransformGizmo->SetVisibility(false);
	}
}

void UEditMeshPolygonsTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (bToolDisabled)
	{
		return;
	}

	Preview->PreviewMesh->EnableWireframe(CommonProps->bShowWireframe);
	SelectionMechanic->Render(RenderAPI);
	DragAlignmentMechanic->Render(RenderAPI);

	if (CurrentActivity)
	{
		CurrentActivity->Render(RenderAPI);
	}
}

void UEditMeshPolygonsTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (bToolDisabled)
	{
		return;
	}

	SelectionMechanic->DrawHUD(Canvas, RenderAPI);
}

void UEditMeshPolygonsTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (Property && (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPolyEditCommonProperties, bShowSelectableCorners)))
	{
		SelectionMechanic->SetShowSelectableCorners(CommonProps->bShowSelectableCorners);
	}
}

//
// Gizmo change tracking
//
void UEditMeshPolygonsTool::UpdateDeformerChangeFromROI(bool bFinal)
{
	if (ActiveVertexChange == nullptr)
	{
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	ActiveVertexChange->SaveVertices(Mesh, LinearDeformer.GetModifiedVertices(), !bFinal);
	ActiveVertexChange->SaveOverlayNormals(Mesh, LinearDeformer.GetModifiedOverlayNormals(), !bFinal);
}

void UEditMeshPolygonsTool::BeginDeformerChange()
{
	if (ActiveVertexChange == nullptr)
	{
		ActiveVertexChange = new FMeshVertexChangeBuilder(EMeshVertexChangeComponents::VertexPositions | EMeshVertexChangeComponents::OverlayNormals);
		UpdateDeformerChangeFromROI(false);
	}
}

void UEditMeshPolygonsTool::EndDeformerChange()
{
	if (ActiveVertexChange != nullptr)
	{
		UpdateDeformerChangeFromROI(true);
		GetToolManager()->EmitObjectChange(this, MoveTemp(ActiveVertexChange->Change), 
			LOCTEXT("PolyMeshDeformationChange", "PolyMesh Edit"));
	}

	delete ActiveVertexChange;
	ActiveVertexChange = nullptr;
}

// This gets called by vertex change events emitted via gizmo (deformer) interaction
void UEditMeshPolygonsTool::ApplyChange(const FMeshVertexChange* Change, bool bRevert)
{
	Preview->PreviewMesh->ApplyChange(Change, bRevert);
	CurrentMesh->Copy(*Preview->PreviewMesh->GetMesh());
	bSpatialDirty = true;
	SelectionMechanic->NotifyMeshChanged(false);

	// Topology does not need updating
}


void UEditMeshPolygonsTool::UpdateFromCurrentMesh(bool bUpdateTopology)
{
	Preview->PreviewMesh->UpdatePreview(CurrentMesh.Get(), UPreviewMesh::ERenderUpdateMode::FullUpdate);
	bSpatialDirty = true;
	SelectionMechanic->NotifyMeshChanged(bUpdateTopology);

	if (bUpdateTopology)
	{
		Topology->RebuildTopology();
	}
}



void UEditMeshPolygonsTool::ApplyMerge()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnMergeFailedMessage", "Cannot Merge Current Selection"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	ChangeTracker.SaveTriangles(ActiveTriangleSelection, true);
	FMeshConnectedComponents Components(Mesh);
	Components.FindConnectedTriangles(ActiveTriangleSelection);
	FGroupTopologySelection NewSelection;
	for (const FMeshConnectedComponents::FComponent& Component : Components)
	{
		int32 NewGroupID = Mesh->AllocateTriangleGroup();
		FaceGroupUtil::SetGroupID(*Mesh, Component.Indices, NewGroupID);
		NewSelection.SelectedGroupIDs.Add(NewGroupID);
	}
	
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshMergeChange", "Merge"),
		ChangeTracker.EndChange(), NewSelection);
}






void UEditMeshPolygonsTool::ApplyDelete()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnDeleteFailedMessage", "Cannot Delete Current Selection"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();

	// prevent deleting all triangles
	if (ActiveTriangleSelection.Num() >= Mesh->TriangleCount())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnDeleteAllFailedMessage", "Cannot Delete Entire Mesh"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	ChangeTracker.SaveTriangles(ActiveTriangleSelection, true);
	FDynamicMeshEditor Editor(Mesh);
	Editor.RemoveTriangles(ActiveTriangleSelection, true);

	FGroupTopologySelection NewSelection;
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshDeleteChange", "Delete"),
		ChangeTracker.EndChange(), NewSelection);
}



void UEditMeshPolygonsTool::ApplyRecalcNormals()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnRecalcNormalsFailedMessage", "Cannot Recalculate Normals for Current Selection"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FDynamicMeshEditor Editor(Mesh);
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	for (int32 GroupID : ActiveSelection.SelectedGroupIDs)
	{
		ChangeTracker.SaveTriangles(Topology->GetGroupTriangles(GroupID), true);
		Editor.SetTriangleNormals(Topology->GetGroupTriangles(GroupID));
	}

	// We actually don't even need any of the wrapper around this change since we're not altering
	// positions or topology (so no other structures need updating), but we go ahead and go the
	// same route as everything else. See :HandlePositionOnlyMeshChanges
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshRecalcNormalsChange", "Recalculate Normals"), 
		ChangeTracker.EndChange(), ActiveSelection);
}


void UEditMeshPolygonsTool::ApplyFlipNormals()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnFlipNormalsFailedMessage", "Cannot Flip Normals for Current  Selection"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FDynamicMeshEditor Editor(Mesh);
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	for (int32 GroupID : ActiveSelection.SelectedGroupIDs)
	{
		for ( int32 tid : Topology->GetGroupTriangles(GroupID) )
		{ 
			ChangeTracker.SaveTriangle(tid, true);
			Mesh->ReverseTriOrientation(tid);
		}
	}

	// Note the topology can change in that the ordering of edge elements can reverse
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshFlipNormalsChange", "Flip Normals"), 
		ChangeTracker.EndChange(), ActiveSelection);
}


void UEditMeshPolygonsTool::ApplyRetriangulate()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage( LOCTEXT("OnRetriangulateFailed", "Cannot Retriangulate Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	int32 nCompleted = 0;
	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FDynamicMeshEditor Editor(Mesh);
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	for (int32 GroupID : ActiveSelection.SelectedGroupIDs)
	{
		const TArray<int32>& Triangles = Topology->GetGroupTriangles(GroupID);
		ChangeTracker.SaveTriangles(Triangles, true);
		FMeshRegionBoundaryLoops RegionLoops(Mesh, Triangles, true);
		if (!RegionLoops.bFailed && RegionLoops.Loops.Num() == 1 && Triangles.Num() > 1)
		{
			TArray<FMeshRegionBoundaryLoops::VidOverlayMap<FVector2f>> VidUVMaps;
			if (Mesh->HasAttributes())
			{
				const FDynamicMeshAttributeSet* Attributes = Mesh->Attributes();
				for (int i = 0; i < Attributes->NumUVLayers(); ++i)
				{
					VidUVMaps.Emplace();
					RegionLoops.GetLoopOverlayMap(RegionLoops.Loops[0], *Attributes->GetUVLayer(i), VidUVMaps.Last());
				}
			}

			// We don't want to remove isolated vertices while removing triangles because we don't
			// want to throw away boundary verts. However, this means that we'll have to go back
			// through these vertices later to throw away isolated internal verts.
			TArray<int32> OldVertices;
			UE::Geometry::TriangleToVertexIDs(Mesh, Triangles, OldVertices);
			Editor.RemoveTriangles(Topology->GetGroupTriangles(GroupID), false);

			RegionLoops.Loops[0].Reverse();
			FSimpleHoleFiller Filler(Mesh, RegionLoops.Loops[0]);
			Filler.FillType = FSimpleHoleFiller::EFillType::PolygonEarClipping;
			Filler.Fill(GroupID);

			// Throw away any of the old verts that are still isolated (they were in the interior of the group)
			Algo::ForEachIf(OldVertices, 
				[Mesh](int32 Vid) 
			{ 
				return !Mesh->IsReferencedVertex(Vid); 
			},
				[Mesh](int32 Vid) 
			{
				checkSlow(!Mesh->IsReferencedVertex(Vid));
				constexpr bool bPreserveManifold = false;
				Mesh->RemoveVertex(Vid, bPreserveManifold);
			}
			);

			if (Mesh->HasAttributes())
			{
				const FDynamicMeshAttributeSet* Attributes = Mesh->Attributes();
				for (int i = 0; i < Attributes->NumUVLayers(); ++i)
				{
					RegionLoops.UpdateLoopOverlayMapValidity(VidUVMaps[i], *Attributes->GetUVLayer(i));
				}
				Filler.UpdateAttributes(VidUVMaps);
			}

			nCompleted++;
		}
	}
	if (nCompleted != ActiveSelection.SelectedGroupIDs.Num())
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnRetriangulateFailures", "Some faces could not be retriangulated"), EToolMessageLevel::UserWarning);
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshRetriangulateChange", "Retriangulate"),
		ChangeTracker.EndChange(), ActiveSelection);
}



void UEditMeshPolygonsTool::SimplifyByGroups()
{
	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	ChangeTracker.SaveTriangles(Mesh->TriangleIndicesItr(), true); // We will change the entire mesh
	
	FPolygroupRemesh Remesh(Mesh, Topology.Get(), ConstrainedDelaunayTriangulate<double>);
	bool bSuccess = Remesh.Compute();
	if (!bSuccess)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnSimplifyByGroupFailures", "Some polygroups could not be correctly simplified"), EToolMessageLevel::UserWarning);
	}

	FGroupTopologySelection NewSelection; // Empty the selection

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshSimplifyByGroup", "Simplify by Group"),
		ChangeTracker.EndChange(), NewSelection);
}



void UEditMeshPolygonsTool::ApplyRegenerateExtraCorners()
{
	if (!ensure(!bTriangleMode && Topology))
	{
		return;
	}

	// We need to remember the extra corners that get generated and put them into the undo system so that if we
	// change the settings later, undoing still brings us back to the result we saw at that time.
	TSet<int32> PreviousExtraCorners(Topology->GetCurrentExtraCornerVids());
	Topology->RebuildTopology();
	const TSet<int32>& NewExtraCorners = Topology->GetCurrentExtraCornerVids();

	bool bCornersChanged = PreviousExtraCorners.Num() != NewExtraCorners.Num() || !PreviousExtraCorners.Includes(NewExtraCorners);
	if (bCornersChanged)
	{
		const FText TransactionLabel = LOCTEXT("RegenerateCornersTransactionName", "Regenerate Corners");

		GetToolManager()->BeginUndoTransaction(TransactionLabel);
		if (SelectionMechanic && !SelectionMechanic->GetActiveSelection().IsEmpty())
		{
			SelectionMechanic->BeginChange();
			SelectionMechanic->ClearSelection();
			GetToolManager()->EmitObjectChange(SelectionMechanic, SelectionMechanic->EndChange(), TransactionLabel);
		}

		GetToolManager()->EmitObjectChange(this,
			MakeUnique<EditMeshPolygonsToolLocals::FExtraCornerChange>(PreviousExtraCorners, NewExtraCorners),
			TransactionLabel);

		GetToolManager()->EndUndoTransaction();
	}
	
	if (SelectionMechanic)
	{
		SelectionMechanic->NotifyMeshChanged(true);
	}
}


void UEditMeshPolygonsTool::RebuildTopologyWithGivenExtraCorners(const TSet<int32>& Vids)
{
	Topology->RebuildTopologyWithSpecificExtraCorners(Vids);
	SelectionMechanic->NotifyMeshChanged(true);
}



void UEditMeshPolygonsTool::ApplyDecompose()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnDecomposeFailed", "Cannot Decompose Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FGroupTopologySelection NewSelection;
	for (int32 GroupID : SelectionMechanic->GetActiveSelection().SelectedGroupIDs)
	{
		const TArray<int32>& Triangles = Topology->GetGroupTriangles(GroupID);
		ChangeTracker.SaveTriangles(Triangles, true);
		for (int32 tid : Triangles)
		{
			int32 NewGroupID = Mesh->AllocateTriangleGroup();
			Mesh->SetTriangleGroup(tid, NewGroupID);
			NewSelection.SelectedGroupIDs.Add(NewGroupID);
		}
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshDecomposeChange", "Decompose"),
		ChangeTracker.EndChange(), NewSelection);
}


void UEditMeshPolygonsTool::ApplyDisconnect()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnDisconnectFailed", "Cannot Disconnect Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	TArray<int32> AllTriangles;
	for (int32 GroupID : ActiveSelection.SelectedGroupIDs)
	{
		AllTriangles.Append(Topology->GetGroupTriangles(GroupID));
	}
	ChangeTracker.SaveTriangles(AllTriangles, true);
	FDynamicMeshEditor Editor(Mesh);
	Editor.DisconnectTriangles(AllTriangles, false);

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshDisconnectChange", "Disconnect"),
		ChangeTracker.EndChange(), ActiveSelection);
}




void UEditMeshPolygonsTool::ApplyDuplicate()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnDuplicateFailed", "Cannot Duplicate Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	TArray<int32> AllTriangles;
	for (int32 GroupID : ActiveSelection.SelectedGroupIDs)
	{
		AllTriangles.Append(Topology->GetGroupTriangles(GroupID));
	}
	FDynamicMeshEditor Editor(Mesh);
	FMeshIndexMappings Mappings;
	FDynamicMeshEditResult EditResult;
	Editor.DuplicateTriangles(AllTriangles, Mappings, EditResult);

	FGroupTopologySelection NewSelection;
	NewSelection.SelectedGroupIDs.Append(bTriangleMode ? EditResult.NewTriangles : EditResult.NewGroups);

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshDisconnectChange", "Disconnect"),
		ChangeTracker.EndChange(), NewSelection);
}




void UEditMeshPolygonsTool::ApplyCollapseEdge()
{
	// AAAHHH cannot do because of overlays!
	return;

	if (SelectionMechanic->GetActiveSelection().SelectedEdgeIDs.Num() != 1 || BeginMeshEdgeEditChange() == false)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnEdgeColllapseFailed", "Cannot Collapse current selection"),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();

	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	//const TArray<int32>& EdgeIDs = ActiveEdgeSelection[0].EdgeIDs;
	//for (int32 eid : EdgeIDs)
	//{
	//	if (Mesh->IsEdge(eid))
	//	{
	//		FIndex2i EdgeVerts = Mesh->GetEdgeV(eid);
	//		ChangeTracker.SaveVertexOneRingTriangles(EdgeVerts.A, true);
	//		ChangeTracker.SaveVertexOneRingTriangles(EdgeVerts.B, true);
	//		FDynamicMesh3::FEdgeCollapseInfo CollapseInfo;
	//		Mesh->CollapseEdge()
	//	}
	//}

	// emit undo
	FGroupTopologySelection NewSelection;
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshEdgeCollapseChange", "Collapse"),
		ChangeTracker.EndChange(), NewSelection);
}



void UEditMeshPolygonsTool::ApplyWeldEdges()
{
	if (SelectionMechanic->GetActiveSelection().SelectedEdgeIDs.Num() != 2)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnWeldEdgesFailedEdgeCount", "Cannot Weld current selection, selection must be exactly 2 edges."),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMesh3 MeshCopy(*Mesh);				// We are going to operate on this copy and, if successful, copy the changes back into Mesh
	
	FGroupTopologySelection CurrentSelection = SelectionMechanic->GetActiveSelection();
	TArray<int32> SelectedEdgeIDs = CurrentSelection.SelectedEdgeIDs.Array();
	FEdgeSpan& SpanA = Topology->Edges[SelectedEdgeIDs[0]].Span;
	FEdgeSpan& SpanB = Topology->Edges[SelectedEdgeIDs[1]].Span;
	
	if (SpanA.Vertices[0] == SpanA.Vertices.Last() || SpanB.Vertices[0] == SpanB.Vertices.Last())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("OnWeldEdgesFailedEdgesAreLoops", "Cannot Weld current selection, selected edges must not be loops."),
			EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();

	// Save one ring tri's for vertices along first edge
	for (int Vert : SpanA.Vertices)
	{
		ChangeTracker.SaveVertexOneRingTriangles(Vert, true);
	}

	// Save one ring tri's for vertices along second edge
	for (int Vert : SpanB.Vertices)
	{
		ChangeTracker.SaveVertexOneRingTriangles(Vert, true);
	}

	FWeldEdgeSequence EdgeWelder(&MeshCopy, SpanA, SpanB);
	EdgeWelder.bAllowIntermediateTriangleDeletion = true;
	EdgeWelder.bAllowFailedMerge = true;
	
	FWeldEdgeSequence::EWeldResult Result = EdgeWelder.Weld();
	if (Result != FWeldEdgeSequence::EWeldResult::Ok)
	{
		switch (Result)
		{
		case FWeldEdgeSequence::EWeldResult::Failed_EdgesNotBoundaryEdges:
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesFailedBoundary", "Cannot Weld current selection, selected edges must be boundary edges."),
				EToolMessageLevel::UserWarning);
			break;

		case FWeldEdgeSequence::EWeldResult::Failed_CannotSplitEdge:
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesFailedSplitEdge", "Cannot Weld current selection, failed to insert vertex."), 
				EToolMessageLevel::UserWarning);
			break;

		case FWeldEdgeSequence::EWeldResult::Failed_TriangleDeletionDisabled:
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesFailedTriDeleteDisabled", "Cannot Weld current selection, deletion of edges connecting selected edges is disabled."), 
				EToolMessageLevel::UserWarning);
			break;

		case FWeldEdgeSequence::EWeldResult::Failed_CannotDeleteTriangle:
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesFailedTriDeleteFailed", "Cannot Weld current selection, failed to delete edge connecting selected edges."), 
				EToolMessageLevel::UserWarning);
			break;

		case FWeldEdgeSequence::EWeldResult::Failed_Other:
		default:
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesFailedOther", "Cannot Weld current selection, bad geometry."), 
				EToolMessageLevel::UserWarning);
			break;
		}

		return;
	}
	else
	{
		// On success, apply the result by copying over the existing mesh
		*Mesh = MeshCopy;

		if (EdgeWelder.UnmergedEdgePairsOut.Num() != 0)
		{
			GetToolManager()->DisplayMessage(
				LOCTEXT("OnWeldEdgesCompletedSeamsRemain", "Warning: welding incomplete because it would create "
					"invalid geometry (attached non manifold edge or duplicate triangle). Seam still exists at weld "
					"location. Modify attached triangles and retry, or undo."),
				EToolMessageLevel::UserWarning);
		}
	}

	FGroupTopologySelection NewSelection;
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshWeldEdgeChange", "Weld Edges"),
		ChangeTracker.EndChange(), NewSelection);
}

void UEditMeshPolygonsTool::ApplyStraightenEdges()
{
	if (BeginMeshEdgeEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnStraightenEdgesFailed", "Cannot Straighten current selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();

	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();

	for (const FSelectedEdge& Edge : ActiveEdgeSelection)
	{
		const TArray<int32>& EdgeVerts = Topology->GetGroupEdgeVertices(Edge.EdgeTopoID);
		int32 NumV = EdgeVerts.Num();
		if ( NumV > 2 )
		{
			ChangeTracker.SaveVertexOneRingTriangles(EdgeVerts, true);
			FVector3d A(Mesh->GetVertex(EdgeVerts[0])), B(Mesh->GetVertex(EdgeVerts[NumV-1]));
			TArray<double> VtxArcLengths;
			double EdgeArcLen = Topology->GetEdgeArcLength(Edge.EdgeTopoID, &VtxArcLengths);
			for (int k = 1; k < NumV-1; ++k)
			{
				double t = VtxArcLengths[k] / EdgeArcLen;
				Mesh->SetVertex(EdgeVerts[k], UE::Geometry::Lerp(A, B, t));
			}
		}
	}

	// TODO :HandlePositionOnlyMeshChanges Due to the group topology storing edge IDs that do not stay the same across
	// undo/redo events even when the mesh topology stays the same after a FDynamicMeshChange, we actually have to treat
	// all FDynamicMeshChange-based transactions as affecting group topology. Here we only changed vertex positions so
	// we could add a separate overload that takes a FMeshVertexChange, and possibly one that takes an attribute change
	// (or unify the three via an interface)
	FGroupTopologySelection NewSelection;
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshStraightenEdgeChange", "Straighten Edges"), 
		ChangeTracker.EndChange(), NewSelection);
}



void UEditMeshPolygonsTool::ApplyFillHole()
{
	if (BeginMeshBoundaryEdgeEditChange(false) == false)
	{
		GetToolManager()->DisplayMessage( LOCTEXT("OnEdgeFillFailed", "Cannot Fill current selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FGroupTopologySelection NewSelection;
	for (FSelectedEdge& FillEdge : ActiveEdgeSelection)
	{
		if (Mesh->IsBoundaryEdge(FillEdge.EdgeIDs[0]))		// may no longer be boundary due to previous fill
		{
			FMeshBoundaryLoops BoundaryLoops(Mesh);
			int32 LoopID = BoundaryLoops.FindLoopContainingEdge(FillEdge.EdgeIDs[0]);
			if (LoopID >= 0)
			{
				FEdgeLoop& Loop = BoundaryLoops.Loops[LoopID];
				FSimpleHoleFiller Filler(Mesh, Loop);
				Filler.FillType = FSimpleHoleFiller::EFillType::PolygonEarClipping;
				int32 NewGroupID = Mesh->AllocateTriangleGroup();
				Filler.Fill(NewGroupID);
				if (!bTriangleMode)
				{
					NewSelection.SelectedGroupIDs.Add(NewGroupID);
				}
				else
				{
					NewSelection.SelectedGroupIDs.Append(Filler.NewTriangles);
				}

				// Compute normals and UVs
				if (Mesh->HasAttributes())
				{
					TArray<FVector3d> VertexPositions;
					Loop.GetVertices(VertexPositions);
					FVector3d PlaneOrigin;
					FVector3d PlaneNormal;
					PolygonTriangulation::ComputePolygonPlane<double>(VertexPositions, PlaneNormal, PlaneOrigin);

					FDynamicMeshEditor Editor(Mesh);
					FFrame3d ProjectionFrame(PlaneOrigin, PlaneNormal);
					Editor.SetTriangleNormals(Filler.NewTriangles);
					Editor.SetTriangleUVsFromProjection(Filler.NewTriangles, ProjectionFrame, UVScaleFactor);
				}
			}
		}
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshFillHoleChange", "Fill Hole"),
		ChangeTracker.EndChange(), NewSelection);
}

void UEditMeshPolygonsTool::ApplyBridgeEdges()
{
	const FText BridgeFailMessage = LOCTEXT("OnEdgeBridgeFailed", "Cannot Bridge current selection");

	if (SelectionMechanic->GetActiveSelection().SelectedEdgeIDs.Num() != 2 || BeginMeshBoundaryEdgeEditChange(false) == false)
	{
		GetToolManager()->DisplayMessage(BridgeFailMessage, EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	FGroupTopologySelection CurrentSelection = SelectionMechanic->GetActiveSelection();

	TArray<int32> LoopVertices;
	TArray<int32> LoopEdges;
	TArray<int32> SelectedEdgeIDs = CurrentSelection.SelectedEdgeIDs.Array();
	
	// I think doing this will guarantee that every edge in the span stores the vertices corresponding to the connected triangles orientation
	FEdgeSpan& SpanA = Topology->Edges[SelectedEdgeIDs[0]].Span;
	FEdgeSpan& SpanB = Topology->Edges[SelectedEdgeIDs[1]].Span;
	SpanA.SetCorrectOrientation();
	SpanB.SetCorrectOrientation();

	// Disallow bridging of edge loops for now
	if (SpanA.Vertices[0] == SpanA.Vertices.Last() || SpanB.Vertices[0] == SpanB.Vertices.Last())
	{
		GetToolManager()->DisplayMessage(BridgeFailMessage, EToolMessageLevel::UserWarning);
		return;
	}

	// Add all vertices from first edge
	LoopVertices = SpanA.Vertices;

	// If first vertex of second edge is not a duplicate of a terminating vertex of the first edge, add vertex
	if (SpanB.Vertices[0] != SpanA.Vertices[0] && SpanB.Vertices[0] != SpanA.Vertices.Last())
	{
		LoopVertices.Add(SpanB.Vertices[0]);
	}

	// Definitely add the non-terminating vertices of second edge.
	for (int Vertex = 1; Vertex < SpanB.Vertices.Num() - 1; ++Vertex)
	{
		LoopVertices.Add(SpanB.Vertices[Vertex]);
	}

	// If last vertex of second edge is not a duplicate of a terminating vertex of the first edge, add vertex
	if (SpanB.Vertices.Last() != SpanA.Vertices[0] && SpanB.Vertices.Last() != SpanA.Vertices.Last())
	{
		LoopVertices.Add(SpanB.Vertices.Last());
	}

	FEdgeLoop::VertexLoopToEdgeLoop(Mesh, LoopVertices, LoopEdges);
	FEdgeLoop Loop(Mesh, LoopVertices, LoopEdges);

	// We could always use the minimal hole filler, but it doesn't quite do what "bridge" would suggest when
	// the area to be bridged is concave (across two curved-inward edges). Meanwhile simple ear clipping
	// seems to fail in some common cases for reasons that we should investigate. For now, start with ear
	// clipping, and revert to minimal if needed.
	FSimpleHoleFiller SimpleHoleFiller(Mesh, Loop, FSimpleHoleFiller::EFillType::PolygonEarClipping);
	TArray<int32> NewTriangles;

	// Fill the hole
	if (!SimpleHoleFiller.Fill())
	{
		//Ear clipping doesn't add vertices, so don't need to delete isolated verts
		FDynamicMeshEditor Editor(Mesh);
		Editor.RemoveTriangles(SimpleHoleFiller.NewTriangles, false);

		FMinimalHoleFiller MinimalHoleFiller(Mesh, Loop);

		if (!MinimalHoleFiller.Fill())
		{
			Editor.RemoveTriangles(MinimalHoleFiller.NewTriangles, false);
			GetToolManager()->DisplayMessage(BridgeFailMessage, EToolMessageLevel::UserWarning);
			// Even though we've manually 'undone' the changes, this will still change mesh timestamps, so we need to register the mesh update
			UpdateFromCurrentMesh(false);
			return;
		}
		else
		{
			NewTriangles = MinimalHoleFiller.NewTriangles;
		}
	}
	else {
		NewTriangles = SimpleHoleFiller.NewTriangles;
	}

	// Compute normals and UVs
	if (Mesh->HasAttributes())
	{
		TArray<FVector3d> VertexPositions;
		Loop.GetVertices(VertexPositions);
		FVector3d PlaneOrigin;
		FVector3d PlaneNormal;
		PolygonTriangulation::ComputePolygonPlane<double>(VertexPositions, PlaneNormal, PlaneOrigin);

		FDynamicMeshEditor Editor(Mesh);
		FFrame3d ProjectionFrame(PlaneOrigin, PlaneNormal);
		Editor.SetTriangleNormals(NewTriangles);
		Editor.SetTriangleUVsFromProjection(NewTriangles, ProjectionFrame, UVScaleFactor);
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshBridgeEdgeChange", "Bridge Edge"),
		ChangeTracker.EndChange(), CurrentSelection);
}


void UEditMeshPolygonsTool::ApplyPokeSingleFace()
{
	if (BeginMeshFaceEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnPokeFailedMessage", "Cannot Poke Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	ChangeTracker.SaveTriangles(ActiveTriangleSelection, true);
	FGroupTopologySelection NewSelection;
	for (int32 tid : ActiveTriangleSelection)
	{
		FDynamicMesh3::FPokeTriangleInfo PokeInfo;
		NewSelection.SelectedGroupIDs.Add(tid);
		if (Mesh->PokeTriangle(tid, PokeInfo) == EMeshResult::Ok)
		{
			NewSelection.SelectedGroupIDs.Add(PokeInfo.NewTriangles.A);
			NewSelection.SelectedGroupIDs.Add(PokeInfo.NewTriangles.B);
		}
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshPokeChange", "Poke Faces"),
		ChangeTracker.EndChange(), NewSelection);
}



void UEditMeshPolygonsTool::ApplyFlipSingleEdge()
{
	if (BeginMeshEdgeEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnFlipFailedMessage", "Cannot Flip Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	for (FSelectedEdge& Edge : ActiveEdgeSelection)
	{
		int32 eid = Edge.EdgeIDs[0];
		if (Mesh->IsEdge(eid) && Mesh->IsBoundaryEdge(eid) == false && Mesh->Attributes()->IsSeamEdge(eid) == false)
		{
			FIndex2i et = Mesh->GetEdgeT(eid);
			ChangeTracker.SaveTriangle(et.A, true);
			ChangeTracker.SaveTriangle(et.B, true);
			FDynamicMesh3::FEdgeFlipInfo FlipInfo;
			Mesh->FlipEdge(eid, FlipInfo);
		}
	}

	// Group topology may or may not change, but just assume that it does.
	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshFlipChange", "Flip Edges"),
		ChangeTracker.EndChange(), ActiveSelection);
}

void UEditMeshPolygonsTool::ApplyCollapseSingleEdge()
{
	if (BeginMeshEdgeEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnCollapseFailedMessage", "Cannot Collapse Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FGroupTopologySelection ActiveSelection = SelectionMechanic->GetActiveSelection();
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	TSet<int32> ValidEdgeIDs;
	for (FSelectedEdge& Edge : ActiveEdgeSelection)
	{
		int32 eid = Edge.EdgeIDs[0];
		if (Mesh->IsEdge(eid) && Mesh->Attributes()->IsSeamEdge(eid) == false)
		{
			ValidEdgeIDs.Add(eid);
		}
	}
	TSet<int32> DoneEdgeIDs;
	for (int32 eid : ValidEdgeIDs)
	{
		if (DoneEdgeIDs.Contains(eid) == false && Mesh->IsEdge(eid))
		{
			FIndex2i ev = Mesh->GetEdgeV(eid);
			ChangeTracker.SaveVertexOneRingTriangles(ev.A, true);
			ChangeTracker.SaveVertexOneRingTriangles(ev.B, true);
			FDynamicMesh3::FEdgeCollapseInfo CollapseInfo;
			if (Mesh->CollapseEdge(ev.A, ev.B, CollapseInfo) == EMeshResult::Ok)
			{
				DoneEdgeIDs.Add(eid);
				DoneEdgeIDs.Add(CollapseInfo.RemovedEdges.A);
				DoneEdgeIDs.Add(CollapseInfo.RemovedEdges.B);
			}
		}
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshCollapseChange", "Collapse Edges"), 
		ChangeTracker.EndChange(), FGroupTopologySelection());
}

void UEditMeshPolygonsTool::ApplySplitSingleEdge()
{
	if (BeginMeshEdgeEditChange() == false)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("OnSplitFailedMessage", "Cannot Split Current Selection"), EToolMessageLevel::UserWarning);
		return;
	}

	FDynamicMesh3* Mesh = CurrentMesh.Get();
	FGroupTopologySelection NewSelection;
	FDynamicMeshChangeTracker ChangeTracker(Mesh);
	ChangeTracker.BeginChange();
	for (FSelectedEdge& Edge : ActiveEdgeSelection)
	{
		int32 eid = Edge.EdgeIDs[0];
		if (Mesh->IsEdge(eid))
		{
			FIndex2i et = Mesh->GetEdgeT(eid);
			ChangeTracker.SaveTriangle(et.A, true);
			NewSelection.SelectedGroupIDs.Add(et.A);
			if (et.B != FDynamicMesh3::InvalidID)
			{
				ChangeTracker.SaveTriangle(et.B, true);
				NewSelection.SelectedGroupIDs.Add(et.B);
			}
			FDynamicMesh3::FEdgeSplitInfo SplitInfo;
			if (Mesh->SplitEdge(eid, SplitInfo) == EMeshResult::Ok)
			{
				NewSelection.SelectedGroupIDs.Add(SplitInfo.NewTriangles.A);
				if (SplitInfo.NewTriangles.B != FDynamicMesh3::InvalidID)
				{
					NewSelection.SelectedGroupIDs.Add(SplitInfo.NewTriangles.A);
				}
			}
		}
	}

	EmitCurrentMeshChangeAndUpdate(LOCTEXT("PolyMeshSplitChange", "Split Edges"),
		ChangeTracker.EndChange(), NewSelection);
}




bool UEditMeshPolygonsTool::BeginMeshFaceEditChange()
{
	ActiveTriangleSelection.Reset();

	// need some selected faces
	const FGroupTopologySelection& ActiveSelection = SelectionMechanic->GetActiveSelection();
	Topology->GetSelectedTriangles(ActiveSelection, ActiveTriangleSelection);
	if (ActiveSelection.SelectedGroupIDs.Num() == 0 || ActiveTriangleSelection.Num() == 0)
	{
		return false;
	}

	const FDynamicMesh3* Mesh = CurrentMesh.Get();
	ActiveSelectionBounds = FAxisAlignedBox3d::Empty();
	for (int tid : ActiveTriangleSelection)
	{
		ActiveSelectionBounds.Contain(Mesh->GetTriBounds(tid));
	}

	// world and local frames
	ActiveSelectionFrameLocal = Topology->GetSelectionFrame(ActiveSelection);
	ActiveSelectionFrameWorld = ActiveSelectionFrameLocal;
	ActiveSelectionFrameWorld.Transform(WorldTransform);

	return true;
}

void UEditMeshPolygonsTool::EmitCurrentMeshChangeAndUpdate(const FText& TransactionLabel,
	TUniquePtr<FDynamicMeshChange> MeshChangeIn,
	const FGroupTopologySelection& OutputSelection)
{
	// We used to take this as a paremeter, but even if we happen to know that the FDynamicMeshChange doesn't
	// involve topology changes, it acts via deleting/reinserting triangles in undo/redo, which changes the
	// eids in a mesh and causes problems. So we always treat the group topology as modified in this function.
	// TODO: Have an overload that uses a vertex change for non-topology-modifying cases.
	constexpr bool bGroupTopologyModified = true;

	// open top-level transaction
	GetToolManager()->BeginUndoTransaction(TransactionLabel);

	// Since we clear the selection in the selection mechanic when topology changes, we need to know
	// when OutputSelection is pointing to the selection in the selection mechanic and is not empty,
	// so that we can copy it ahead of time and reinstate it.
	bool bReferencingSameSelection = (&SelectionMechanic->GetActiveSelection() == &OutputSelection);

	// Not actually relevant since our assumption of topology being modified means we always clear existing selection.
	// bool bSelectionModified = !bReferencingSameSelection && SelectionMechanic->GetActiveSelection() != OutputSelection;

	// In case we need to make a selection copy
	FGroupTopologySelection TempSelection;
	const FGroupTopologySelection* OutputSelectionToUse = &OutputSelection;

	// Emit a selection clear before emitting the mesh change, so that undo restores it properly.
	if (!SelectionMechanic->GetActiveSelection().IsEmpty() /* && (bSelectionModified || bGroupTopologyModified) */)
	{
		if (bReferencingSameSelection)
		{
			// Need to make a copy because OutputSelection will get cleared
			TempSelection = OutputSelection;
			OutputSelectionToUse = &TempSelection;
		}

		SelectionMechanic->BeginChange();
		SelectionMechanic->ClearSelection();
		GetToolManager()->EmitObjectChange(SelectionMechanic, SelectionMechanic->EndChange(), LOCTEXT("ClearSelection", "Clear Selection"));
	}

	// Prep and emit the mesh change. This needs to be bookended by the change in extra corners, since
	// those get regenerated in the topology rebuild.
	TUniquePtr<FEditMeshPolygonsToolMeshChange> ChangeToEmit = MakeUnique<FEditMeshPolygonsToolMeshChange>(MoveTemp(MeshChangeIn));
	if (!Topology->GetCurrentExtraCornerVids().IsEmpty())
	{
		ChangeToEmit->ExtraCornerVidsBefore = Topology->GetCurrentExtraCornerVids();
	}
	Topology->RebuildTopology();
	if (!Topology->GetCurrentExtraCornerVids().IsEmpty())
	{
		ChangeToEmit->ExtraCornerVidsAfter = Topology->GetCurrentExtraCornerVids();
	}
	GetToolManager()->EmitObjectChange(this, 
		MoveTemp(ChangeToEmit),
		TransactionLabel);

	// Update other related structures
	UpdateFromCurrentMesh(false);
	SelectionMechanic->NotifyMeshChanged(true); // This wasn't updated in UpdateFromCurrentMesh because we didn't ask to rebuild topology
	ModifiedTopologyCounter += bGroupTopologyModified;

	// Set output selection if there's a non-empty one. We know we've cleared the selection by
	// this point due to treating topology as always modified.
	if (!OutputSelectionToUse->IsEmpty() /* && (bSelectionModified || bGroupTopologyModified) */)
	{
		SelectionMechanic->BeginChange();
		SelectionMechanic->SetSelection(*OutputSelectionToUse);
		GetToolManager()->EmitObjectChange(SelectionMechanic, SelectionMechanic->EndChange(), LOCTEXT("SetSelection", "Set Selection"));
	}

	GetToolManager()->EndUndoTransaction();
}

void UEditMeshPolygonsTool::EmitActivityStart(const FText& TransactionLabel)
{
	++ActivityTimestamp;

	GetToolManager()->BeginUndoTransaction(TransactionLabel);
	GetToolManager()->EmitObjectChange(this,
		MakeUnique<FPolyEditActivityStartChange>(ActivityTimestamp),
		TransactionLabel);
	GetToolManager()->EndUndoTransaction();
}


bool UEditMeshPolygonsTool::BeginMeshEdgeEditChange()
{
	return BeginMeshEdgeEditChange([](int32) {return true; });
}

bool UEditMeshPolygonsTool::BeginMeshBoundaryEdgeEditChange(bool bOnlySimple)
{
	if (bOnlySimple)
	{
		return BeginMeshEdgeEditChange(
			[&](int32 GroupEdgeID) { return Topology->IsBoundaryEdge(GroupEdgeID) && Topology->IsSimpleGroupEdge(GroupEdgeID); });
	}
	else
	{
		return BeginMeshEdgeEditChange(
			[&](int32 GroupEdgeID) { return Topology->IsBoundaryEdge(GroupEdgeID); });
	}
}

bool UEditMeshPolygonsTool::BeginMeshEdgeEditChange(TFunctionRef<bool(int32)> GroupEdgeIDFilterFunc)
{
	ActiveEdgeSelection.Reset();

	const FGroupTopologySelection& ActiveSelection = SelectionMechanic->GetActiveSelection();
	int NumEdges = ActiveSelection.SelectedEdgeIDs.Num();
	if (NumEdges == 0)
	{
		return false;
	}
	ActiveEdgeSelection.Reserve(NumEdges);
	for (int32 EdgeID : ActiveSelection.SelectedEdgeIDs)
	{
		if (GroupEdgeIDFilterFunc(EdgeID))
		{
			FSelectedEdge& Edge = ActiveEdgeSelection.Emplace_GetRef();
			Edge.EdgeTopoID = EdgeID;
			Edge.EdgeIDs = Topology->GetGroupEdgeEdges(EdgeID);
		}
	}

	return ActiveEdgeSelection.Num() > 0;
}

void UEditMeshPolygonsTool::SetActionButtonPanelsVisible(bool bVisible)
{
	if (bTriangleMode == false)
	{
		if (EditActions)
		{
			SetToolPropertySourceEnabled(EditActions, bVisible);
		}
		if (EditEdgeActions)
		{
			SetToolPropertySourceEnabled(EditEdgeActions, bVisible);
		}
		if (EditUVActions)
		{
			SetToolPropertySourceEnabled(EditUVActions, bVisible);
		}
	}
	else
	{
		if (EditActions_Triangles)
		{
			SetToolPropertySourceEnabled(EditActions_Triangles, bVisible);
		}
		if (EditEdgeActions_Triangles)
		{
			SetToolPropertySourceEnabled(EditEdgeActions_Triangles, bVisible);
		}
	}
}


bool UEditMeshPolygonsTool::CanCurrentlyNestedCancel()
{
	return CurrentActivity != nullptr 
		|| (SelectionMechanic && !SelectionMechanic->GetActiveSelection().IsEmpty());
}

bool UEditMeshPolygonsTool::ExecuteNestedCancelCommand()
{
	if (CurrentActivity)
	{
		EndCurrentActivity(EToolShutdownType::Cancel);
		return true;
	}
	else if (SelectionMechanic && !SelectionMechanic->GetActiveSelection().IsEmpty())
	{
		SelectionMechanic->BeginChange();
		SelectionMechanic->ClearSelection();
		GetToolManager()->EmitObjectChange(SelectionMechanic, SelectionMechanic->EndChange(), LOCTEXT("ClearSelection", "Clear Selection"));
		return true;
	}
	return false;
}

bool UEditMeshPolygonsTool::CanCurrentlyNestedAccept()
{
	return CurrentActivity != nullptr;
}

bool UEditMeshPolygonsTool::ExecuteNestedAcceptCommand()
{
	if (CurrentActivity)
	{
		EndCurrentActivity(EToolShutdownType::Accept);
		return true;
	}
	return false;
}

void FEditMeshPolygonsToolMeshChange::Apply(UObject* Object)
{
	UEditMeshPolygonsTool* Tool = Cast<UEditMeshPolygonsTool>(Object);
	
	MeshChange->Apply(Tool->CurrentMesh.Get(), false);
	Tool->UpdateFromCurrentMesh(false);
	++Tool->ModifiedTopologyCounter;

	Tool->RebuildTopologyWithGivenExtraCorners(ExtraCornerVidsAfter);

	Tool->ActivityContext->OnUndoRedo.Broadcast(true);
}

void FEditMeshPolygonsToolMeshChange::Revert(UObject* Object)
{
	UEditMeshPolygonsTool* Tool = Cast<UEditMeshPolygonsTool>(Object);
	
	MeshChange->Apply(Tool->CurrentMesh.Get(), true);
	Tool->UpdateFromCurrentMesh(false);
	++Tool->ModifiedTopologyCounter;

	Tool->RebuildTopologyWithGivenExtraCorners(ExtraCornerVidsBefore);

	Tool->ActivityContext->OnUndoRedo.Broadcast(true);
}

FString FEditMeshPolygonsToolMeshChange::ToString() const
{
	return TEXT("FEditMeshPolygonsToolMeshChange");
}

void FPolyEditActivityStartChange::Revert(UObject* Object)
{
	Cast<UEditMeshPolygonsTool>(Object)->EndCurrentActivity();
	bHaveDoneUndo = true;
}
bool FPolyEditActivityStartChange::HasExpired(UObject* Object) const
{
	return bHaveDoneUndo 
		|| Cast<UEditMeshPolygonsTool>(Object)->ActivityTimestamp != ActivityTimestamp;
}
FString FPolyEditActivityStartChange::ToString() const
{
	return TEXT("FPolyEditActivityStartChange");
}

#undef LOCTEXT_NAMESPACE

