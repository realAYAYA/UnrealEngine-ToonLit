// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsEditorMode.h"
#include "Components/StaticMeshComponent.h"
#include "IGeometryProcessingInterfacesModule.h"
#include "Editor/UnrealEdEngine.h"
#include "EdModeInteractiveToolsContext.h"
#include "GeometryProcessingInterfaces/IUVEditorModularFeature.h"
#include "IAnalyticsProviderET.h"
#include "ModelingToolsEditorModeToolkit.h"
#include "ILevelEditor.h"
#include "ModelingToolsEditorModeSettings.h"
#include "ModelingToolsHostCustomizationAPI.h"
#include "ToolTargetManager.h"
#include "ContextObjectStore.h"
#include "InputRouter.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "Selection.h"
#include "ToolTargets/VolumeComponentToolTarget.h"
#include "ToolTargets/DynamicMeshComponentToolTarget.h"
#include "ToolTargets/SkeletalMeshComponentToolTarget.h"
#include "EngineAnalytics.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Snapping/ModelingSceneSnappingManager.h"
#include "Scene/LevelObjectsObserver.h"
#include "UnrealEdGlobals.h" // GUnrealEd

#include "Features/IModularFeatures.h"
#include "ModelingModeToolExtensions.h"

#include "DynamicMeshSculptTool.h"
#include "MeshVertexSculptTool.h"
#include "EditMeshPolygonsTool.h"
#include "DeformMeshPolygonsTool.h"
#include "SubdividePolyTool.h"
#include "ConvertToPolygonsTool.h"
#include "AddPrimitiveTool.h"
#include "AddPatchTool.h"
#include "CubeGridTool.h"
#include "RevolveBoundaryTool.h"
#include "SmoothMeshTool.h"
#include "OffsetMeshTool.h"
#include "SimplifyMeshTool.h"
#include "MeshInspectorTool.h"
#include "WeldMeshEdgesTool.h"
#include "DrawPolygonTool.h"
#include "DrawPolyPathTool.h"
#include "DrawAndRevolveTool.h"
#include "DrawSplineTool.h"
#include "RevolveSplineTool.h"
#include "ShapeSprayTool.h"
#include "VoxelSolidifyMeshesTool.h"
#include "VoxelBlendMeshesTool.h"
#include "VoxelMorphologyMeshesTool.h"
#include "PlaneCutTool.h"
#include "MirrorTool.h"
#include "SelfUnionMeshesTool.h"
#include "CSGMeshesTool.h"
#include "CutMeshWithMeshTool.h"
#include "BspConversionTool.h"
#include "MeshToVolumeTool.h"
#include "VolumeToMeshTool.h"
#include "HoleFillTool.h"
#include "PolygonOnMeshTool.h"
#include "DisplaceMeshTool.h"
#include "MeshSpaceDeformerTool.h"
#include "EditNormalsTool.h"
#include "RemoveOccludedTrianglesTool.h"
#include "AttributeEditorTool.h"
#include "TransformMeshesTool.h"
#include "UVProjectionTool.h"
#include "UVLayoutTool.h"
#include "EditMeshMaterialsTool.h"
#include "AddPivotActorTool.h"
#include "EditPivotTool.h"
#include "BakeTransformTool.h"
#include "CombineMeshesTool.h"
#include "AlignObjectsTool.h"
#include "EditUVIslandsTool.h"
#include "BakeMeshAttributeMapsTool.h"
#include "BakeMultiMeshAttributeMapsTool.h"
#include "BakeMeshAttributeVertexTool.h"
#include "BakeRenderCaptureTool.h"
#include "MeshAttributePaintTool.h"
#include "ParameterizeMeshTool.h"
#include "RecomputeUVsTool.h"
#include "MeshTangentsTool.h"
#include "ProjectToTargetTool.h"
#include "LatticeDeformerTool.h"
#include "SeamSculptTool.h"
#include "MeshGroupPaintTool.h"
#include "MeshVertexPaintTool.h"
#include "TransferMeshTool.h"
#include "ConvertMeshesTool.h"
#include "SplitMeshesTool.h"
#include "PatternTool.h"
#include "HarvestInstancesTool.h"
#include "TriangulateSplinesTool.h"

#include "Polymodeling/ExtrudeMeshSelectionTool.h"
#include "Polymodeling/OffsetMeshSelectionTool.h"

#include "Physics/PhysicsInspectorTool.h"
#include "Physics/SetCollisionGeometryTool.h"
#include "Physics/ExtractCollisionGeometryTool.h"
#include "Physics/SimpleCollisionEditorTool.h"

// asset tools
#include "Tools/GenerateStaticMeshLODAssetTool.h"
#include "Tools/LODManagerTool.h"
#include "ISMEditorTool.h"

// commands
#include "Commands/DeleteGeometrySelectionCommand.h"
#include "Commands/DisconnectGeometrySelectionCommand.h"
#include "Commands/RetriangulateGeometrySelectionCommand.h"
#include "Commands/ModifyGeometrySelectionCommand.h"


#include "EditorModeManager.h"
#include "UnrealWidget.h"

// Stylus support is currently disabled due to issues with the stylus plugin
// We are leaving the code in this cpp file, defined out, so that it is easier to bring back if/when the stylus plugin is improved.
#define ENABLE_STYLUS_SUPPORT 0

#if ENABLE_STYLUS_SUPPORT 
#include "IStylusState.h"
#include "IStylusInputModule.h"
#endif

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Application/ThrottleManager.h"

#include "ModelingToolsActions.h"
#include "ModelingToolsManagerActions.h"
#include "ModelingModeAssetUtils.h"
#include "EditorModelingObjectsCreationAPI.h"

#include "Selection/GeometrySelectionManager.h"
#include "Selection/VolumeSelector.h"
#include "Selection/StaticMeshSelector.h"
#include "ModelingSelectionInteraction.h"
#include "DynamicMeshActor.h"
#include "Components/BrushComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h" // FWorldDelegates

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModelingToolsEditorMode)

#if WITH_PROXYLOD
#include "MergeMeshesTool.h"
#include "VoxelCSGMeshesTool.h"
#endif

#define LOCTEXT_NAMESPACE "UModelingToolsEditorMode"


const FEditorModeID UModelingToolsEditorMode::EM_ModelingToolsEditorModeId = TEXT("EM_ModelingToolsEditorMode");

FDateTime UModelingToolsEditorMode::LastModeStartTimestamp;
FDateTime UModelingToolsEditorMode::LastToolStartTimestamp;

FDelegateHandle UModelingToolsEditorMode::GlobalModelingWorldTeardownEventHandle;

namespace
{
FString GetToolName(const UInteractiveTool& Tool)
{
	const FString* ToolName = FTextInspector::GetSourceString(Tool.GetToolInfo().ToolDisplayName);
	return ToolName ? *ToolName : FString(TEXT("<Invalid ToolName>"));
}
}

UModelingToolsEditorMode::UModelingToolsEditorMode()
{
	Info = FEditorModeInfo(
		EM_ModelingToolsEditorModeId,
		LOCTEXT("ModelingToolsEditorModeName", "Modeling"),
		FSlateIcon("ModelingToolsStyle", "LevelEditor.ModelingToolsMode", "LevelEditor.ModelingToolsMode.Small"),
		true,
		5000);
}

UModelingToolsEditorMode::UModelingToolsEditorMode(FVTableHelper& Helper)
	: UBaseLegacyWidgetEdMode(Helper)
{
}

UModelingToolsEditorMode::~UModelingToolsEditorMode()
{
}

bool UModelingToolsEditorMode::ProcessEditDelete()
{
	if (GetSelectionManager() && GetSelectionManager()->HasSelection())
	{
		if ( UDeleteGeometrySelectionCommand* DeleteCommand = Cast<UDeleteGeometrySelectionCommand>(ModelingModeCommands[0]) )
		{
			GetSelectionManager()->ExecuteSelectionCommand(DeleteCommand);
			return true;
		}
	}

	if (UEdMode::ProcessEditDelete())
	{
		return true;
	}

	// for now we disable deleting in an Accept-style tool because it can result in crashes if we are deleting target object
	if ( GetToolManager()->HasAnyActiveTool() && GetToolManager()->GetActiveTool(EToolSide::Mouse)->HasAccept() )
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotDeleteWarning", "Cannot delete objects while this Tool is active"), EToolMessageLevel::UserWarning);
		return true;
	}

	return false;
}


bool UModelingToolsEditorMode::ProcessEditCut()
{
	// for now we disable deleting in an Accept-style tool because it can result in crashes if we are deleting target object
	if (GetToolManager()->HasAnyActiveTool() && GetToolManager()->GetActiveTool(EToolSide::Mouse)->HasAccept())
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotCutWarning", "Cannot cut objects while this Tool is active"), EToolMessageLevel::UserWarning);
		return true;
	}

	return false;
}


void UModelingToolsEditorMode::ActorSelectionChangeNotify()
{
	// would like to clear selection here, but this is called multiple times, including after a transaction when
	// we cannot identify that the selection should not be cleared
}


bool UModelingToolsEditorMode::CanAutoSave() const
{
	// prevent autosave if any tool is active
	return GetToolManager()->HasAnyActiveTool() == false;
}

bool UModelingToolsEditorMode::ShouldDrawWidget() const
{ 
	// hide standard xform gizmo if we have an active tool, unless it explicitly opts in via the IInteractiveToolEditorGizmoAPI
	if (GetInteractiveToolsContext() != nullptr && GetToolManager()->HasAnyActiveTool())
	{
		IInteractiveToolEditorGizmoAPI* GizmoAPI = Cast<IInteractiveToolEditorGizmoAPI>(GetToolManager()->GetActiveTool(EToolSide::Left));
		if (!GizmoAPI || !GizmoAPI->GetAllowStandardEditorGizmos())
		{
			return false;
		}
	}

	// hide standard xform gizmo if we have an active selection
	if (GetSelectionManager() && (GetSelectionManager()->HasSelection() || GetSelectionManager()->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None))
	{
		return false;
	}

	return UBaseLegacyWidgetEdMode::ShouldDrawWidget(); 
}

void UModelingToolsEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	Super::Tick(ViewportClient, DeltaTime);

	if (Toolkit.IsValid())
	{
		FModelingToolsEditorModeToolkit* ModelingToolkit = (FModelingToolsEditorModeToolkit*)Toolkit.Get();
		ModelingToolkit->ShowRealtimeAndModeWarnings(ViewportClient->IsRealtime() == false);
	}
}

// Note: Stylus support is currently non-functioning; the code to enable it is left here as reference in case it is brought back
#if ENABLE_STYLUS_SUPPORT
//
// FStylusStateTracker registers itself as a listener for stylus events and implements
// the IToolStylusStateProviderAPI interface, which allows MeshSurfacePointTool implementations
 // to query for the pen pressure.
//
// This is kind of a hack. Unfortunately the current Stylus module is a Plugin so it
// cannot be used in the base ToolsFramework, and we need this in the Mode as a workaround.
//
class FStylusStateTracker : public IStylusMessageHandler, public IToolStylusStateProviderAPI
{
public:
	const IStylusInputDevice* ActiveDevice = nullptr;
	int32 ActiveDeviceIndex = -1;

	bool bPenDown = false;
	float ActivePressure = 1.0;

	FStylusStateTracker()
	{
		UStylusInputSubsystem* StylusSubsystem = GEditor->GetEditorSubsystem<UStylusInputSubsystem>();
		StylusSubsystem->AddMessageHandler(*this);

		ActiveDevice = FindFirstPenDevice(StylusSubsystem, ActiveDeviceIndex);
		bPenDown = false;
	}

	virtual ~FStylusStateTracker()
	{
		if (GEditor)
		{
			if (UStylusInputSubsystem* StylusSubsystem = GEditor->GetEditorSubsystem<UStylusInputSubsystem>())
			{
				StylusSubsystem->RemoveMessageHandler(*this);
			}
		}
	}

	virtual void OnStylusStateChanged(const FStylusState& NewState, int32 StylusIndex) override
	{
		if (ActiveDevice == nullptr)
		{
			UStylusInputSubsystem* StylusSubsystem = GEditor->GetEditorSubsystem<UStylusInputSubsystem>();
			ActiveDevice = FindFirstPenDevice(StylusSubsystem, ActiveDeviceIndex);
			bPenDown = false;
		}
		if (ActiveDevice != nullptr && ActiveDeviceIndex == StylusIndex)
		{
			bPenDown = NewState.IsStylusDown();
			ActivePressure = NewState.GetPressure();
		}
	}


	bool HaveActiveStylusState() const
	{
		return ActiveDevice != nullptr && bPenDown;
	}

	static const IStylusInputDevice* FindFirstPenDevice(const UStylusInputSubsystem* StylusSubsystem, int32& ActiveDeviceOut)
	{
		int32 NumDevices = StylusSubsystem->NumInputDevices();
		for (int32 k = 0; k < NumDevices; ++k)
		{
			const IStylusInputDevice* Device = StylusSubsystem->GetInputDevice(k);
			const TArray<EStylusInputType>& Inputs = Device->GetSupportedInputs();
			for (EStylusInputType Input : Inputs)
			{
				if (Input == EStylusInputType::Pressure)
				{
					ActiveDeviceOut = k;
					return Device;
				}
			}
		}
		return nullptr;
	}



	// IToolStylusStateProviderAPI implementation
	virtual float GetCurrentPressure() const override
	{
		return (ActiveDevice != nullptr && bPenDown) ? ActivePressure : 1.0f;
	}

};
#endif // ENABLE_STYLUS_SUPPORT






void UModelingToolsEditorMode::Enter()
{
	UEdMode::Enter();

	const UModelingToolsEditorModeSettings* ModelingModeSettings = GetDefault<UModelingToolsEditorModeSettings>();
	const UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetDefault<UModelingToolsModeCustomizationSettings>();
	bSelectionSystemEnabled = ModelingModeSettings->bEnablePersistentSelections;

	// Register builders for tool targets that the mode uses.
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UStaticMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UVolumeComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDynamicMeshComponentToolTargetFactory>(GetToolManager()));

	// Register read-only skeletal mesh tool targets. Currently tools that write to meshes risk breaking
	// skin weights.
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentReadOnlyToolTargetFactory>(GetToolManager()));

	// listen to post-build
	GetToolManager()->OnToolPostBuild.AddUObject(this, &UModelingToolsEditorMode::OnToolPostBuild);

	// forward shutdown requests
	GetToolManager()->OnToolShutdownRequest.BindLambda([this](UInteractiveToolManager*, UInteractiveTool* Tool, EToolShutdownType ShutdownType)
	{
		GetInteractiveToolsContext()->EndTool(ShutdownType); 
		return true;
	});

	// register for OnRender and OnDrawHUD extensions
	GetInteractiveToolsContext()->OnRender.AddUObject(this, &UModelingToolsEditorMode::OnToolsContextRender);
	GetInteractiveToolsContext()->OnDrawHUD.AddUObject(this, &UModelingToolsEditorMode::OnToolsContextDrawHUD);

#if ENABLE_STYLUS_SUPPORT 
	// register stylus event handler
	StylusStateTracker = MakeUnique<FStylusStateTracker>();
#endif

	// register gizmo helper
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(GetInteractiveToolsContext());
	// configure mode-level Gizmo options
	if (ModelingModeSettings)
	{
		GetInteractiveToolsContext()->SetForceCombinedGizmoMode(ModelingModeSettings->bRespectLevelEditorGizmoMode == false);
		GetInteractiveToolsContext()->SetAbsoluteWorldSnappingEnabled(ModelingModeSettings->bEnableAbsoluteWorldSnapping);
	}

	// Now that we have the gizmo helper, bind the numerical UI.
	if (ensure(Toolkit.IsValid()))
	{
		((FModelingToolsEditorModeToolkit*)Toolkit.Get())->BindGizmoNumericalUI();
	}
	
	// register snapping manager
	UE::Geometry::RegisterSceneSnappingManager(GetInteractiveToolsContext());
	SceneSnappingManager = UE::Geometry::FindModelingSceneSnappingManager(GetToolManager());

	// register tool shutdown button customizer
	if (ensure(Toolkit.IsValid()))
	{
		UModelingToolsHostCustomizationAPI::Register(GetInteractiveToolsContext(), 
			StaticCastSharedRef<FModelingToolsEditorModeToolkit>(Toolkit.ToSharedRef()));
	}

	// set up SelectionManager and register known factory types
	SelectionManager = NewObject<UGeometrySelectionManager>(GetToolManager());
	SelectionManager->Initialize(GetInteractiveToolsContext(), GetToolManager()->GetContextTransactionsAPI());
	SelectionManager->RegisterSelectorFactory(MakeUnique<FDynamicMeshComponentSelectorFactory>());
	SelectionManager->RegisterSelectorFactory(MakeUnique<FBrushComponentSelectorFactory>());
	SelectionManager->RegisterSelectorFactory(MakeUnique<FStaticMeshComponentSelectorFactory>());

	// this is hopefully temporary? kinda gross...
	GetInteractiveToolsContext()->ContextObjectStore->AddContextObject(SelectionManager);

	// rebuild tool palette on any selection changes. This is expensive and ideally will be
	// optimized in the future.
	//SelectionManager_SelectionModifiedHandle = SelectionManager->OnSelectionModified.AddLambda([this]()
	//{
	//	((FModelingToolsEditorModeToolkit*)Toolkit.Get())->ForceToolPaletteRebuild();
	//});

	// set up the selection interaction
	SelectionInteraction = NewObject<UModelingSelectionInteraction>(GetToolManager());
	SelectionInteraction->Initialize(SelectionManager,
		[this]() { return GetGeometrySelectionChangesAllowed(); },
		[this](const FInputDeviceRay& DeviceRay) { return TestForEditorGizmoHit(DeviceRay); });
	GetInteractiveToolsContext()->InputRouter->RegisterSource(SelectionInteraction);

	SelectionInteraction->OnTransformBegin.AddLambda([this]() 
	{ 
		// Disable the SnappingManager while the SelectionInteraction is editing a mesh via transform gizmo
		SceneSnappingManager->PauseSceneGeometryUpdates();

		// If the transform is happening via the gizmo numerical UI, then we can run into the same slate
		// throttling issues as tools. We need to continue receiving render/tick while user scrubs the slate values.
		FSlateThrottleManager::Get().DisableThrottle(true);
	});
	SelectionInteraction->OnTransformEnd.AddLambda([this]() 
	{ 
		FSlateThrottleManager::Get().DisableThrottle(false);
		SceneSnappingManager->UnPauseSceneGeometryUpdates(); 
	});


	// register level objects observer that will update the snapping manager as the scene changes
	LevelObjectsObserver = MakeShared<FLevelObjectsObserver>();
	LevelObjectsObserver->OnActorAdded.AddLambda([this](AActor* Actor)
	{
		if (SceneSnappingManager)
		{
			SceneSnappingManager->OnActorAdded(Actor, [](UPrimitiveComponent*) { return true; });
		}
	});
	LevelObjectsObserver->OnActorRemoved.AddLambda([this](AActor* Actor)
	{
		if (SceneSnappingManager)
		{
			SceneSnappingManager->OnActorRemoved(Actor);
		}
	});
	// tracker will auto-populate w/ the current level, but must have registered the handlers first!
	LevelObjectsObserver->Initialize(GetWorld());

	// disable HitProxy rendering, it is not used in Modeling Mode and adds overhead to Render() calls
	GetInteractiveToolsContext()->SetEnableRenderingDuringHitProxyPass(false);

	// register object creation api
	UEditorModelingObjectsCreationAPI* ModelCreationAPI = UEditorModelingObjectsCreationAPI::Register(GetInteractiveToolsContext());
	if (ModelCreationAPI)
	{
		ModelCreationAPI->GetNewAssetPathNameCallback.BindLambda([](const FString& BaseName, const UWorld* TargetWorld, FString SuggestedFolder)
		{
			return UE::Modeling::GetNewAssetPathName(BaseName, TargetWorld, SuggestedFolder);
		});
		MeshCreatedEventHandle = ModelCreationAPI->OnModelingMeshCreated.AddLambda([this](const FCreateMeshObjectResult& CreatedInfo) 
		{
			if (CreatedInfo.NewAsset != nullptr)
			{
				UE::Modeling::OnNewAssetCreated(CreatedInfo.NewAsset);
				// If we are creating a new asset or component, it should be initially unlocked in the Selection system.
				// Currently have no generic way to do this, the Selection Manager does not necessarily support Static Meshes
				// or Brush Components. So doing it here...
				if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(CreatedInfo.NewAsset))
				{
					FStaticMeshSelector::SetAssetUnlockedOnCreation(StaticMesh);
				}
			}
			if ( UBrushComponent* BrushComponent = Cast<UBrushComponent>(CreatedInfo.NewComponent) )
			{
				FVolumeSelector::SetComponentUnlockedOnCreation(BrushComponent);
			}
		});
		TextureCreatedEventHandle = ModelCreationAPI->OnModelingTextureCreated.AddLambda([](const FCreateTextureObjectResult& CreatedInfo)
		{
			if (CreatedInfo.NewAsset != nullptr)
			{
				UE::Modeling::OnNewAssetCreated(CreatedInfo.NewAsset);
			}
		});
		MaterialCreatedEventHandle = ModelCreationAPI->OnModelingMaterialCreated.AddLambda([](const FCreateMaterialObjectResult& CreatedInfo)
		{
			if (CreatedInfo.NewAsset != nullptr)
			{
				UE::Modeling::OnNewAssetCreated(CreatedInfo.NewAsset);
			}
		});
	}

	const FModelingToolsManagerCommands& ToolManagerCommands = FModelingToolsManagerCommands::Get();

	// register tool set

	//
	// primitive tools
	//
	auto RegisterPrimitiveToolFunc  =
		[this](TSharedPtr<FUICommandInfo> UICommand,
								  FString&& ToolIdentifier,
								  UAddPrimitiveToolBuilder::EMakeMeshShapeType ShapeTypeIn)
	{
		auto AddPrimitiveToolBuilder = NewObject<UAddPrimitiveToolBuilder>();
		AddPrimitiveToolBuilder->ShapeType = ShapeTypeIn;
		RegisterTool(UICommand, ToolIdentifier, AddPrimitiveToolBuilder);
	};
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddBoxPrimitiveTool,
							  TEXT("BeginAddBoxPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Box);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddCylinderPrimitiveTool,
							  TEXT("BeginAddCylinderPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Cylinder);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddConePrimitiveTool,
							  TEXT("BeginAddConePrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Cone);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddArrowPrimitiveTool,
							  TEXT("BeginAddArrowPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Arrow);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddRectanglePrimitiveTool,
							  TEXT("BeginAddRectanglePrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Rectangle);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddDiscPrimitiveTool,
							  TEXT("BeginAddDiscPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Disc);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddTorusPrimitiveTool,
							  TEXT("BeginAddTorusPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Torus);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddSpherePrimitiveTool,
							  TEXT("BeginAddSpherePrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Sphere);
	RegisterPrimitiveToolFunc(ToolManagerCommands.BeginAddStairsPrimitiveTool,
							  TEXT("BeginAddStairsPrimitiveTool"),
							  UAddPrimitiveToolBuilder::EMakeMeshShapeType::Stairs);

	//
	// make shape tools
	//
	auto AddPatchToolBuilder = NewObject<UAddPatchToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginAddPatchTool, TEXT("BeginAddPatchTool"), AddPatchToolBuilder);

	auto RevolveBoundaryToolBuilder = NewObject<URevolveBoundaryToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginRevolveBoundaryTool, TEXT("BeginRevolveBoundaryTool"), RevolveBoundaryToolBuilder);

	auto DrawPolygonToolBuilder = NewObject<UDrawPolygonToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginDrawPolygonTool, TEXT("BeginDrawPolygonTool"), DrawPolygonToolBuilder);

	auto DrawPolyPathToolBuilder = NewObject<UDrawPolyPathToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginDrawPolyPathTool, TEXT("BeginDrawPolyPathTool"), DrawPolyPathToolBuilder);

	auto DrawAndRevolveToolBuilder = NewObject<UDrawAndRevolveToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginDrawAndRevolveTool, TEXT("BeginDrawAndRevolveTool"), DrawAndRevolveToolBuilder);

	auto RevolveSplineToolBuilder = NewObject<URevolveSplineToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginRevolveSplineTool, TEXT("BeginRevolveSplineTool"), RevolveSplineToolBuilder);

	auto ShapeSprayToolBuilder = NewObject<UShapeSprayToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginShapeSprayTool, TEXT("BeginShapeSprayTool"), ShapeSprayToolBuilder);

	auto CubeGridToolBuilder = NewObject<UCubeGridToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginCubeGridTool, TEXT("BeginCubeGridTool"), CubeGridToolBuilder);

	auto DrawSplineToolBuilder = NewObject<UDrawSplineToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginDrawSplineTool, TEXT("BeginDrawSplineTool"), DrawSplineToolBuilder);

	auto TriangulateSplinesToolBuilder = NewObject<UTriangulateSplinesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginTriangulateSplinesTool, TEXT("BeginTriangulateSplinesTool"), TriangulateSplinesToolBuilder);

	//
	// vertex deform tools
	//

	auto MoveVerticesToolBuilder = NewObject<UMeshVertexSculptToolBuilder>();
#if ENABLE_STYLUS_SUPPORT 
	MoveVerticesToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginSculptMeshTool, TEXT("BeginSculptMeshTool"), MoveVerticesToolBuilder);

	auto MeshGroupPaintToolBuilder = NewObject<UMeshGroupPaintToolBuilder>();
#if ENABLE_STYLUS_SUPPORT 
	MeshGroupPaintToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginMeshGroupPaintTool, TEXT("BeginMeshGroupPaintTool"), MeshGroupPaintToolBuilder);
	RegisterTool(ToolManagerCommands.BeginMeshVertexPaintTool, TEXT("BeginMeshVertexPaintTool"), NewObject<UMeshVertexPaintToolBuilder>());

	RegisterTool(ToolManagerCommands.BeginPolyEditTool, TEXT("BeginPolyEditTool"), NewObject<UEditMeshPolygonsToolBuilder>());
	UEditMeshPolygonsToolBuilder* TriEditBuilder = NewObject<UEditMeshPolygonsToolBuilder>();
	TriEditBuilder->bTriangleMode = true;
	RegisterTool(ToolManagerCommands.BeginTriEditTool, TEXT("BeginTriEditTool"), TriEditBuilder);
	RegisterTool(ToolManagerCommands.BeginPolyDeformTool, TEXT("BeginPolyDeformTool"), NewObject<UDeformMeshPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSmoothMeshTool, TEXT("BeginSmoothMeshTool"), NewObject<USmoothMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginOffsetMeshTool, TEXT("BeginOffsetMeshTool"), NewObject<UOffsetMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDisplaceMeshTool, TEXT("BeginDisplaceMeshTool"), NewObject<UDisplaceMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginMeshSpaceDeformerTool, TEXT("BeginMeshSpaceDeformerTool"), NewObject<UMeshSpaceDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginTransformMeshesTool, TEXT("BeginTransformMeshesTool"), NewObject<UTransformMeshesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginAddPivotActorTool, TEXT("BeginAddPivotActorTool"), NewObject<UAddPivotActorToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginEditPivotTool, TEXT("BeginEditPivotTool"), NewObject<UEditPivotToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginAlignObjectsTool, TEXT("BeginAlignObjectsTool"), NewObject<UAlignObjectsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginTransferMeshTool, TEXT("BeginTransferMeshTool"), NewObject<UTransferMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginConvertMeshesTool, TEXT("BeginConvertMeshesTool"), NewObject<UConvertMeshesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSplitMeshesTool, TEXT("BeginSplitMeshesTool"), NewObject<USplitMeshesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginBakeTransformTool, TEXT("BeginBakeTransformTool"), NewObject<UBakeTransformToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginTransformUVIslandsTool, TEXT("BeginTransformUVIslandsTool"), NewObject<UEditUVIslandsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginLatticeDeformerTool, TEXT("BeginLatticeDeformerTool"), NewObject<ULatticeDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSubdividePolyTool, TEXT("BeginSubdividePolyTool"), NewObject<USubdividePolyToolBuilder>());

	UPatternToolBuilder* PatternToolBuilder = NewObject<UPatternToolBuilder>();
	PatternToolBuilder->bEnableCreateISMCs = true;
	RegisterTool(ToolManagerCommands.BeginPatternTool, TEXT("BeginPatternTool"), PatternToolBuilder);
	RegisterTool(ToolManagerCommands.BeginHarvestInstancesTool, TEXT("BeginHarvestInstancesTool"), NewObject<UHarvestInstancesToolBuilder>());

	UCombineMeshesToolBuilder* CombineMeshesToolBuilder = NewObject<UCombineMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginCombineMeshesTool, TEXT("BeginCombineMeshesTool"), CombineMeshesToolBuilder);

	UCombineMeshesToolBuilder* DuplicateMeshesToolBuilder = NewObject<UCombineMeshesToolBuilder>();
	DuplicateMeshesToolBuilder->bIsDuplicateTool = true;
	RegisterTool(ToolManagerCommands.BeginDuplicateMeshesTool, TEXT("BeginDuplicateMeshesTool"), DuplicateMeshesToolBuilder);


	ULODManagerToolBuilder* LODManagerToolBuilder = NewObject<ULODManagerToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginLODManagerTool, TEXT("BeginLODManagerTool"), LODManagerToolBuilder);

	UGenerateStaticMeshLODAssetToolBuilder* GenerateSMLODToolBuilder = NewObject<UGenerateStaticMeshLODAssetToolBuilder>();
	GenerateSMLODToolBuilder->bInRestrictiveMode = ModelingModeSettings && ModelingModeSettings->InRestrictiveMode();
	RegisterTool(ToolManagerCommands.BeginGenerateStaticMeshLODAssetTool, TEXT("BeginGenerateStaticMeshLODAssetTool"), GenerateSMLODToolBuilder);

	UISMEditorToolBuilder* ISMEditorToolBuilder = NewObject<UISMEditorToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginISMEditorTool, TEXT("BeginISMEditorTool"), ISMEditorToolBuilder);


	// edit tools


	auto DynaSculptToolBuilder = NewObject<UDynamicMeshSculptToolBuilder>();
	DynaSculptToolBuilder->bEnableRemeshing = true;
#if ENABLE_STYLUS_SUPPORT 
	DynaSculptToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginRemeshSculptMeshTool, TEXT("BeginRemeshSculptMeshTool"), DynaSculptToolBuilder);

	RegisterTool(ToolManagerCommands.BeginRemeshMeshTool, TEXT("BeginRemeshMeshTool"), NewObject<URemeshMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginProjectToTargetTool, TEXT("BeginProjectToTargetTool"), NewObject<UProjectToTargetToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSimplifyMeshTool, TEXT("BeginSimplifyMeshTool"), NewObject<USimplifyMeshToolBuilder>());

	auto EditNormalsToolBuilder = NewObject<UEditNormalsToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginEditNormalsTool, TEXT("BeginEditNormalsTool"), EditNormalsToolBuilder);

	RegisterTool(ToolManagerCommands.BeginEditTangentsTool, TEXT("BeginEditTangentsTool"), NewObject<UMeshTangentsToolBuilder>());

	auto RemoveOccludedTrianglesToolBuilder = NewObject<URemoveOccludedTrianglesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginRemoveOccludedTrianglesTool, TEXT("BeginRemoveOccludedTrianglesTool"), RemoveOccludedTrianglesToolBuilder);

	RegisterTool(ToolManagerCommands.BeginHoleFillTool, TEXT("BeginHoleFillTool"), NewObject<UHoleFillToolBuilder>());

	auto UVProjectionToolBuilder = NewObject<UUVProjectionToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginUVProjectionTool, TEXT("BeginUVProjectionTool"), UVProjectionToolBuilder);

	auto UVLayoutToolBuilder = NewObject<UUVLayoutToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginUVLayoutTool, TEXT("BeginUVLayoutTool"), UVLayoutToolBuilder);

#if WITH_PROXYLOD
	auto MergeMeshesToolBuilder = NewObject<UMergeMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVoxelMergeTool, TEXT("BeginVoxelMergeTool"), MergeMeshesToolBuilder);

	auto VoxelCSGMeshesToolBuilder = NewObject<UVoxelCSGMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVoxelBooleanTool, TEXT("BeginVoxelBooleanTool"), VoxelCSGMeshesToolBuilder);
#endif	// WITH_PROXYLOD

	auto VoxelSolidifyMeshesToolBuilder = NewObject<UVoxelSolidifyMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVoxelSolidifyTool, TEXT("BeginVoxelSolidifyTool"), VoxelSolidifyMeshesToolBuilder);

	auto VoxelBlendMeshesToolBuilder = NewObject<UVoxelBlendMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVoxelBlendTool, TEXT("BeginVoxelBlendTool"), VoxelBlendMeshesToolBuilder);

	auto VoxelMorphologyMeshesToolBuilder = NewObject<UVoxelMorphologyMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVoxelMorphologyTool, TEXT("BeginVoxelMorphologyTool"), VoxelMorphologyMeshesToolBuilder);

	auto SelfUnionMeshesToolBuilder = NewObject<USelfUnionMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginSelfUnionTool, TEXT("BeginSelfUnionTool"), SelfUnionMeshesToolBuilder);

	auto CSGMeshesToolBuilder = NewObject<UCSGMeshesToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginMeshBooleanTool, TEXT("BeginMeshBooleanTool"), CSGMeshesToolBuilder);

	auto CutMeshWithMeshToolBuilder = NewObject<UCutMeshWithMeshToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginCutMeshWithMeshTool, TEXT("BeginCutMeshWithMeshTool"), CutMeshWithMeshToolBuilder);

	auto TrimMeshesToolBuilder = NewObject<UCSGMeshesToolBuilder>();
	TrimMeshesToolBuilder->bTrimMode = true;
	RegisterTool(ToolManagerCommands.BeginMeshTrimTool, TEXT("BeginMeshTrimTool"), TrimMeshesToolBuilder);

	// BSPConv is disabled in Restrictive Mode.
	if (ToolManagerCommands.BeginBspConversionTool)
	{
		RegisterTool(ToolManagerCommands.BeginBspConversionTool, TEXT("BeginBspConversionTool"), NewObject<UBspConversionToolBuilder>());
	}

	auto MeshToVolumeToolBuilder = NewObject<UMeshToVolumeToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginMeshToVolumeTool, TEXT("BeginMeshToVolumeTool"), MeshToVolumeToolBuilder);

	auto VolumeToMeshToolBuilder = NewObject<UVolumeToMeshToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginVolumeToMeshTool, TEXT("BeginVolumeToMeshTool"), VolumeToMeshToolBuilder);

	auto PlaneCutToolBuilder = NewObject<UPlaneCutToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginPlaneCutTool, TEXT("BeginPlaneCutTool"), PlaneCutToolBuilder);

	auto MirrorToolBuilder = NewObject<UMirrorToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginMirrorTool, TEXT("BeginMirrorTool"), MirrorToolBuilder);

	auto PolygonCutToolBuilder = NewObject<UPolygonOnMeshToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginPolygonCutTool, TEXT("BeginPolygonCutTool"), PolygonCutToolBuilder);

	auto GlobalUVGenerateToolBuilder = NewObject<UParameterizeMeshToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginGlobalUVGenerateTool, TEXT("BeginGlobalUVGenerateTool"), GlobalUVGenerateToolBuilder);

	auto RecomputeUVsToolBuilder = NewObject<URecomputeUVsToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginGroupUVGenerateTool, TEXT("BeginGroupUVGenerateTool"), RecomputeUVsToolBuilder);

	RegisterTool(ToolManagerCommands.BeginUVSeamEditTool, TEXT("BeginUVSeamEditTool"), NewObject< USeamSculptToolBuilder>());

	RegisterUVEditor();

	auto MeshSelectionToolBuilder = NewObject<UMeshSelectionToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginMeshSelectionTool, TEXT("BeginMeshSelectionTool"), MeshSelectionToolBuilder);

	auto EditMeshMaterialsToolBuilder = NewObject<UEditMeshMaterialsToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginEditMeshMaterialsTool, TEXT("BeginEditMeshMaterialsTool"), EditMeshMaterialsToolBuilder);
	
	RegisterTool(ToolManagerCommands.BeginMeshAttributePaintTool, TEXT("BeginMeshAttributePaintTool"), NewObject<UMeshAttributePaintToolBuilder>());

	auto BakeMeshAttributeMapsToolBuilder = NewObject<UBakeMeshAttributeMapsToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginBakeMeshAttributeMapsTool, TEXT("BeginBakeMeshAttributeMapsTool"), BakeMeshAttributeMapsToolBuilder);

	auto BakeMultiMeshAttributeMapsToolBuilder = NewObject<UBakeMultiMeshAttributeMapsToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginBakeMultiMeshAttributeMapsTool, TEXT("BeginBakeMultiMeshAttributeMapsTool"), BakeMultiMeshAttributeMapsToolBuilder);

	auto BakeRenderCaptureToolBuilder = NewObject<UBakeRenderCaptureToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginBakeRenderCaptureTool, TEXT("BeginBakeRenderCaptureTool"), BakeRenderCaptureToolBuilder);

	auto BakeMeshAttributeVertexToolBuilder = NewObject<UBakeMeshAttributeVertexToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginBakeMeshAttributeVertexTool, TEXT("BeginBakeMeshAttributeVertexTool"), BakeMeshAttributeVertexToolBuilder);

	// analysis tools

	RegisterTool(ToolManagerCommands.BeginMeshInspectorTool, TEXT("BeginMeshInspectorTool"), NewObject<UMeshInspectorToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginWeldEdgesTool, TEXT("BeginWeldEdgesTool"), NewObject<UWeldMeshEdgesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginPolyGroupsTool, TEXT("BeginPolyGroupsTool"), NewObject<UConvertToPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginAttributeEditorTool, TEXT("BeginAttributeEditorTool"), NewObject<UAttributeEditorToolBuilder>());


	// Physics Tools

	RegisterTool(ToolManagerCommands.BeginPhysicsInspectorTool, TEXT("BeginPhysicsInspectorTool"), NewObject<UPhysicsInspectorToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSimpleCollisionEditorTool, TEXT("BeginSimpleCollisionEditorTool"), NewObject<USimpleCollisionEditorToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSetCollisionGeometryTool, TEXT("BeginSetCollisionGeometryTool"), NewObject<USetCollisionGeometryToolBuilder>());

	auto ExtractCollisionGeoToolBuilder = NewObject<UExtractCollisionGeometryToolBuilder>();
	RegisterTool(ToolManagerCommands.BeginExtractCollisionGeometryTool, TEXT("BeginExtractCollisionGeometryTool"), ExtractCollisionGeoToolBuilder);



	bool bAlwaysShowAllSelectionCommands = !ModelingEditorSettings->bUseLegacyModelingPalette;

	// this function registers and tracks an active UGeometrySelectionEditCommand and it's associated UICommand
	auto RegisterSelectionTool = [&, bAlwaysShowAllSelectionCommands](TSharedPtr<FUICommandInfo> UICommand, FString ToolIdentifier, UInteractiveToolBuilder* Builder, bool bRequiresActiveTarget, bool bRequiresSelection)
	{
		UEditorInteractiveToolsContext* UseToolsContext = GetInteractiveToolsContext(EToolsContextScope::EdMode);
		const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
		UseToolsContext->ToolManager->RegisterToolType(ToolIdentifier, Builder);
		CommandList->MapAction(UICommand,
			FExecuteAction::CreateUObject(UseToolsContext, &UEdModeInteractiveToolsContext::StartTool, ToolIdentifier),
			FCanExecuteAction::CreateWeakLambda(UseToolsContext, [this, ToolIdentifier, UseToolsContext, bRequiresActiveTarget, bRequiresSelection, bAlwaysShowAllSelectionCommands]() {
				return ShouldToolStartBeAllowed(ToolIdentifier) &&
					( GetSelectionManager()->HasActiveTargets() || bRequiresActiveTarget == false) &&
					( GetSelectionManager()->HasSelection() || bRequiresSelection == false ) &&
					( GetSelectionManager()->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None || bAlwaysShowAllSelectionCommands ) &&
					UseToolsContext->ToolManager->CanActivateTool(EToolSide::Mouse, ToolIdentifier);
			}),
			FIsActionChecked::CreateUObject(UseToolsContext, &UEdModeInteractiveToolsContext::IsToolActive, EToolSide::Mouse, ToolIdentifier),
			//FIsActionButtonVisible::CreateUObject(GetSelectionManager(), &UGeometrySelectionManager::HasSelection),
			FIsActionButtonVisible::CreateWeakLambda(UseToolsContext, [this, bRequiresActiveTarget, bRequiresSelection, bAlwaysShowAllSelectionCommands]() {
				return (bAlwaysShowAllSelectionCommands) ? true : (
					( GetSelectionManager()->HasActiveTargets() || bRequiresActiveTarget == false) &&
					( GetSelectionManager()->HasSelection() || bRequiresSelection == false ) &&
					( GetSelectionManager()->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None ) );
			}),
			EUIActionRepeatMode::RepeatDisabled);
	};

	// register mesh-selection-driven tools
	RegisterSelectionTool(ToolManagerCommands.BeginSelectionAction_Extrude, TEXT("BeginSelectionExtrudeTool"), NewObject<UExtrudeMeshSelectionToolBuilder>(), true, true);
	RegisterSelectionTool(ToolManagerCommands.BeginSelectionAction_Offset, TEXT("BeginSelectionOffsetTool"), NewObject<UOffsetMeshSelectionToolBuilder>(), true, true);

	RegisterSelectionTool(ToolManagerCommands.BeginPolyModelTool_PolyEd, TEXT("BeginSelectionPolyEdTool"), NewObject<UEditMeshPolygonsSelectionModeToolBuilder>(), true, false);
	RegisterSelectionTool(ToolManagerCommands.BeginPolyModelTool_TriSel, TEXT("BeginSelectionTriEdTool"), NewObject<UMeshSelectionToolBuilder>(), true, false);


	auto RegisterPolyModelActionTool = [&](EEditMeshPolygonsToolActions Action, TSharedPtr<FUICommandInfo> UICommand, FString StringName, bool bRequiresSelection)
	{
		UEditMeshPolygonsActionModeToolBuilder* ActionModeBuilder = NewObject<UEditMeshPolygonsActionModeToolBuilder>();
		ActionModeBuilder->StartupAction = Action;
		RegisterSelectionTool(UICommand, StringName, ActionModeBuilder, true, bRequiresSelection);
	};
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::Inset, ToolManagerCommands.BeginPolyModelTool_Inset, TEXT("PolyEdit_Inset"), true);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::Outset, ToolManagerCommands.BeginPolyModelTool_Outset, TEXT("PolyEdit_Outset"), true);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::CutFaces, ToolManagerCommands.BeginPolyModelTool_CutFaces, TEXT("PolyEdit_CutFaces"), true);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::InsertEdgeLoop, ToolManagerCommands.BeginPolyModelTool_InsertEdgeLoop, TEXT("PolyEdit_InsertEdgeLoop"), false);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::ExtrudeEdges, ToolManagerCommands.BeginPolyModelTool_ExtrudeEdges, TEXT("PolyEdit_ExtrudeEdges"), true);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::PushPull, ToolManagerCommands.BeginPolyModelTool_PushPull, TEXT("PolyEdit_PushPull"), true);
	RegisterPolyModelActionTool(EEditMeshPolygonsToolActions::BevelAuto, ToolManagerCommands.BeginPolyModelTool_Bevel, TEXT("PolyEdit_Bevel"), true);
	

	// set up selection type toggles

	auto RegisterSelectionMode = [this](UGeometrySelectionManager::EMeshTopologyMode TopoMode, UE::Geometry::EGeometryElementType ElementMode, TSharedPtr<FUICommandInfo> UICommand)
	{
		Toolkit->GetToolkitCommands()->MapAction(UICommand,
			FExecuteAction::CreateLambda([this, TopoMode, ElementMode]() {
				if ( GetToolManager() && GetToolManager()->GetContextTransactionsAPI() && GetSelectionManager() )
				{
					GetToolManager()->GetContextTransactionsAPI()->BeginUndoTransaction(LOCTEXT("ChangeSelectionMode", "Selection Mode"));
					GetSelectionManager()->SetMeshTopologyMode(TopoMode); 
					GetSelectionManager()->SetSelectionElementType(ElementMode);
					GetToolManager()->GetContextTransactionsAPI()->EndUndoTransaction();
					if (UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>())
					{
						ModelingEditorSettings->LastMeshSelectionTopologyMode = static_cast<int>(TopoMode);
						ModelingEditorSettings->LastMeshSelectionElementType = static_cast<int>(ElementMode);
						ModelingEditorSettings->SaveConfig();
					}
				}
			}),
			FCanExecuteAction::CreateLambda([this]() { return (GetToolManager() != nullptr && GetToolManager()->HasAnyActiveTool() == false) ? true : false; }),
			FIsActionChecked::CreateLambda([this, TopoMode, ElementMode]() { return (GetSelectionManager() != nullptr && GetSelectionManager()->GetMeshTopologyMode() == TopoMode && GetSelectionManager()->GetSelectionElementType() == ElementMode); }),
			EUIActionRepeatMode::RepeatDisabled);
	};
	if (GetSelectionManager() != nullptr)
	{
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::None, UGeometrySelectionManager::EGeometryElementType::Face, ToolManagerCommands.MeshSelectionModeAction_NoSelection);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Triangle, UGeometrySelectionManager::EGeometryElementType::Face, ToolManagerCommands.MeshSelectionModeAction_MeshTriangles);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Triangle, UGeometrySelectionManager::EGeometryElementType::Vertex, ToolManagerCommands.MeshSelectionModeAction_MeshVertices);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Triangle, UGeometrySelectionManager::EGeometryElementType::Edge, ToolManagerCommands.MeshSelectionModeAction_MeshEdges);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Polygroup, UGeometrySelectionManager::EGeometryElementType::Face, ToolManagerCommands.MeshSelectionModeAction_GroupFaces);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Polygroup, UGeometrySelectionManager::EGeometryElementType::Vertex, ToolManagerCommands.MeshSelectionModeAction_GroupCorners);
		RegisterSelectionMode(UGeometrySelectionManager::EMeshTopologyMode::Polygroup, UGeometrySelectionManager::EGeometryElementType::Edge, ToolManagerCommands.MeshSelectionModeAction_GroupEdges);
	}


	// this function registers and tracks an active UGeometrySelectionEditCommand and it's associated UICommand
	auto RegisterSelectionCommand = [&](UGeometrySelectionEditCommand* Command, TSharedPtr<FUICommandInfo> UICommand, bool bAlwaysVisible)
	{
		ModelingModeCommands.Add(Command);
		const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
		CommandList->MapAction(UICommand,
			FExecuteAction::CreateUObject(GetSelectionManager(), &UGeometrySelectionManager::ExecuteSelectionCommand, Command),
			FCanExecuteAction::CreateUObject(GetSelectionManager(), &UGeometrySelectionManager::CanExecuteSelectionCommand, Command),
			FIsActionChecked(),
			(bAlwaysVisible) ? FIsActionButtonVisible() : 
				FIsActionButtonVisible::CreateUObject(GetSelectionManager(), &UGeometrySelectionManager::CanExecuteSelectionCommand, Command),
			EUIActionRepeatMode::RepeatDisabled);
	};

	// create and register InteractiveCommands for mesh selections
	RegisterSelectionCommand(NewObject<UDeleteGeometrySelectionCommand>(), ToolManagerCommands.BeginSelectionAction_Delete, bAlwaysShowAllSelectionCommands);
	RegisterSelectionCommand(NewObject<UDisconnectGeometrySelectionCommand>(), ToolManagerCommands.BeginSelectionAction_Disconnect, bAlwaysShowAllSelectionCommands);
	RegisterSelectionCommand(NewObject<URetriangulateGeometrySelectionCommand>(), ToolManagerCommands.BeginSelectionAction_Retriangulate, bAlwaysShowAllSelectionCommands);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand>(), ToolManagerCommands.BeginSelectionAction_SelectAll, true);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand_Invert>(), ToolManagerCommands.BeginSelectionAction_Invert, true);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand_ExpandToConnected>(), ToolManagerCommands.BeginSelectionAction_ExpandToConnected, true);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand_InvertConnected>(), ToolManagerCommands.BeginSelectionAction_InvertConnected, true);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand_Expand>(), ToolManagerCommands.BeginSelectionAction_Expand, true);
	RegisterSelectionCommand(NewObject<UModifyGeometrySelectionCommand_Contract>(), ToolManagerCommands.BeginSelectionAction_Contract, true);

	// register extensions
	TArray<IModelingModeToolExtension*> Extensions = IModularFeatures::Get().GetModularFeatureImplementations<IModelingModeToolExtension>(
		IModelingModeToolExtension::GetModularFeatureName());
	if (Extensions.Num() > 0)
	{
		FExtensionToolQueryInfo ExtensionQueryInfo;
		ExtensionQueryInfo.ToolsContext = GetInteractiveToolsContext();
		ExtensionQueryInfo.AssetAPI = nullptr;

		UE_LOG(LogTemp, Log, TEXT("ModelingMode: Found %d Tool Extension Modules"), Extensions.Num());
		for (int32 k = 0; k < Extensions.Num(); ++k)
		{
			// TODO: extension name
			FText ExtensionName = Extensions[k]->GetExtensionName();
			FString ExtensionPrefix = FString::Printf(TEXT("[%d][%s]"), k, *ExtensionName.ToString());

			TArray<FExtensionToolDescription> ToolSet;
			Extensions[k]->GetExtensionTools(ExtensionQueryInfo, ToolSet);
			for (const FExtensionToolDescription& ToolInfo : ToolSet)
			{
				UE_LOG(LogTemp, Log, TEXT("%s - Registering Tool [%s]"), *ExtensionPrefix, *ToolInfo.ToolName.ToString());

				RegisterTool(ToolInfo.ToolCommand, ToolInfo.ToolName.ToString(), ToolInfo.ToolBuilder);
			}

			TArray<TSubclassOf<UToolTargetFactory>> ExtensionToolTargetFactoryClasses;
			if (Extensions[k]->GetExtensionToolTargets(ExtensionToolTargetFactoryClasses))
			{
				for (const TSubclassOf<UToolTargetFactory>& ExtensionTargetFactoryClass : ExtensionToolTargetFactoryClasses)
				{
					GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UToolTargetFactory>(GetToolManager(), ExtensionTargetFactoryClass.Get()));
				}
			}
		}
	}


	GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("DynaSculptTool"));

	// Register modeling mode hotkeys. Note that we use the toolkit command list because we would like the hotkeys
	// to work even when the viewport is not focused, provided that nothing else captures the key presses.
	FModelingModeActionCommands::RegisterCommandBindings(Toolkit->GetToolkitCommands(), [this](EModelingModeActionCommands Command) {
		ModelingModeShortcutRequested(Command);
	});

	// enable realtime viewport override
	ConfigureRealTimeViewportsOverride(true);

	//
	// Engine Analytics
	//

	// Log mode starting
	if (FEngineAnalytics::IsAvailable())
	{
		LastModeStartTimestamp = FDateTime::UtcNow();

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), LastModeStartTimestamp.ToString()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.Enter"), Attributes);
	}

	// Log tool starting
	GetToolManager()->OnToolStarted.AddLambda([this](UInteractiveToolManager* Manager, UInteractiveTool* Tool)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			LastToolStartTimestamp = FDateTime::UtcNow();

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Add(FAnalyticsEventAttribute(TEXT("ToolName"), GetToolName(*Tool)));
			Attributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), LastToolStartTimestamp.ToString()));

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.ToolStarted"), Attributes);
		}
	});

	// Log tool ending
	GetToolManager()->OnToolEnded.AddLambda([this](UInteractiveToolManager* Manager, UInteractiveTool* Tool)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			const FDateTime Now = FDateTime::UtcNow();
			const FTimespan ToolUsageDuration = Now - LastToolStartTimestamp;

			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Add(FAnalyticsEventAttribute(TEXT("ToolName"), GetToolName(*Tool)));
			Attributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), Now.ToString()));
			Attributes.Add(FAnalyticsEventAttribute(TEXT("Duration.Seconds"), static_cast<float>(ToolUsageDuration.GetTotalSeconds())));

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.ToolEnded"), Attributes);
		}
	});

	// Restore saved selections when tool is cancelled or tool declares it is safe to do so via the IInteractiveToolManageGeometrySelectionAPI
	GetToolManager()->OnToolEndedWithStatus.AddLambda([this](UInteractiveToolManager* Manager, UInteractiveTool* Tool, EToolShutdownType ShutdownType)
	{
		bool bCanRestore = (ShutdownType == EToolShutdownType::Cancel);
		if (IInteractiveToolManageGeometrySelectionAPI* ManageSelectionTool = Cast<IInteractiveToolManageGeometrySelectionAPI>(Tool))
		{
			bCanRestore = bCanRestore || ManageSelectionTool->IsInputSelectionValidOnOutput();
		}
		if (bCanRestore)
		{
			GetSelectionManager()->RestoreSavedSelection();
		}
		else
		{
			GetSelectionManager()->DiscardSavedSelection();
		}
		ensureMsgf(!GetSelectionManager()->HasSavedSelection(), TEXT("Selection manager's saved selection should be cleared on tool end."));
	});

	// do any toolkit UI initialization that depends on the mode setup above
	if (Toolkit.IsValid())
	{
		FModelingToolsEditorModeToolkit* ModelingToolkit = (FModelingToolsEditorModeToolkit*)Toolkit.Get();
		ModelingToolkit->InitializeAfterModeSetup();
	}

	EditorClosedEventHandle = GEditor->OnEditorClose().AddUObject(this, &UModelingToolsEditorMode::OnEditorClosed);

	// Need to know about selection changes to (eg) clear mesh selections. 
	// Listening to USelection::SelectionChangedEvent here instead of the underlying UTypedElementSelectionSet
	// events because they do not fire at the right times, particular wrt undo/redo. 
	SelectionModifiedEventHandle = GetModeManager()->GetSelectedActors()->SelectionChangedEvent.AddLambda(
		[this](const UObject* Object)  { UpdateSelectionManagerOnEditorSelectionChange(); } );

	// restore various settings
	if (GetSelectionManager())
	{
		UE::Geometry::EGeometryElementType LastElementType = static_cast<UE::Geometry::EGeometryElementType>(ModelingEditorSettings->LastMeshSelectionElementType);
		if (LastElementType == UE::Geometry::EGeometryElementType::Edge || LastElementType == UE::Geometry::EGeometryElementType::Face || LastElementType == UE::Geometry::EGeometryElementType::Vertex)
		{
			GetSelectionManager()->SetSelectionElementType(LastElementType);
		}
		UGeometrySelectionManager::EMeshTopologyMode LastTopologyMode = static_cast<UGeometrySelectionManager::EMeshTopologyMode>(ModelingEditorSettings->LastMeshSelectionTopologyMode);
		if (LastTopologyMode == UGeometrySelectionManager::EMeshTopologyMode::None || LastTopologyMode == UGeometrySelectionManager::EMeshTopologyMode::Triangle || LastTopologyMode == UGeometrySelectionManager::EMeshTopologyMode::Polygroup)
		{
			GetSelectionManager()->SetMeshTopologyMode(LastTopologyMode);
		}

		bEnableStaticMeshElementSelection = ModelingEditorSettings->bLastMeshSelectionStaticMeshToggle;
		bEnableVolumeElementSelection = ModelingEditorSettings->bLastMeshSelectionVolumeToggle;
	}
	if ( SelectionInteraction )
	{
		EModelingSelectionInteraction_DragMode LastDragMode = static_cast<EModelingSelectionInteraction_DragMode>(ModelingEditorSettings->LastMeshSelectionDragMode);
		if ( LastDragMode == EModelingSelectionInteraction_DragMode::NoDragInteraction || LastDragMode == EModelingSelectionInteraction_DragMode::PathInteraction )
		{
			SelectionInteraction->SetActiveDragMode(LastDragMode);
		}

		EModelingSelectionInteraction_LocalFrameMode LastLocalFrameMode =
			static_cast<EModelingSelectionInteraction_LocalFrameMode>(ModelingEditorSettings->LastMeshSelectionLocalFrameMode);
		if ( LastLocalFrameMode == EModelingSelectionInteraction_LocalFrameMode::FromGeometry || LastLocalFrameMode == EModelingSelectionInteraction_LocalFrameMode::FromObject )
		{
			SelectionInteraction->SetLocalFrameMode(LastLocalFrameMode);
		}
	}

	// initialize SelectionManager w/ active selection
	UpdateSelectionManagerOnEditorSelectionChange(true);

	// Selection system currently requires the concept of 'locking' for Static Meshes and Volumes. This is maintained
	// by a global list that we do *not* want to clear between invocations of Modeling Mode (v annoying if frequently switching
	// modes) but *do* want to clear when the user loads a new level. So, the first time this runs, register a delegate that listens
	// for level editor map changes. This is a static member and will never be unregistered!
	if (GlobalModelingWorldTeardownEventHandle.IsValid() == false)
	{
		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		GlobalModelingWorldTeardownEventHandle = LevelEditor.OnMapChanged().AddLambda([](UWorld* World, EMapChangeType ChangeType)
		{
			if (ChangeType == EMapChangeType::TearDownWorld)
			{
				FVolumeSelector::ResetUnlockedBrushComponents();
				FStaticMeshSelector::ResetUnlockedStaticMeshAssets();
			}
		});
	}

	BlueprintPreCompileHandle = GEditor->OnBlueprintPreCompile().AddUObject(this, &UModelingToolsEditorMode::OnBlueprintPreCompile);

	// Removing levels from the world can happen either by entering/exiting level instance edit mode, or
	// by using the Levels panel. The problem is that any temporary actors we may have spawned in the 
	// level for visualization, gizmos, etc. will be garbage collected. While EdModeInteractiveToolsContext
	// should end the tools for us, we still have to take care of mode-level temporary actors.
	FWorldDelegates::PreLevelRemovedFromWorld.AddWeakLambda(this, [this](ULevel*, UWorld*) {
		// The ideal solution would be to just exit the mode, but we don't have a way to do that- we
		// can only request a mode switch on next tick.Since this is too late to prevent a crash, we
		// hand-clean up temporary actors here.
		if (SelectionInteraction)
		{
			SelectionInteraction->Shutdown();
		}

		// Since we're doing this hand-cleanup above, we could actually register to OnCurrentLevelChanged and
		// reinstate the temporary actors to stay in the mode. That seems a bit brittle, though, and there
		// is still some hope that we can someday exit the mode instead of having to keep track of what is
		// in danger of being garbage collected, so we might as well keep the workflow the same (i.e. exit
		// mode).
		GetModeManager()->ActivateDefaultMode();
	});
}

void UModelingToolsEditorMode::RegisterUVEditor()
{
	IUVEditorModularFeature* UVEditorAPI = nullptr;
	// We should be allowed to do GetModularFeatureImplementation directly without the check, but currently there is an assert
	// there (despite what the header for that function promises).
	if (IModularFeatures::Get().IsModularFeatureAvailable(IUVEditorModularFeature::GetModularFeatureName()))
	{
		UVEditorAPI = static_cast<IUVEditorModularFeature*>(IModularFeatures::Get().GetModularFeatureImplementation(IUVEditorModularFeature::GetModularFeatureName(), 0));
	}

	if (UVEditorAPI)
	{
		const FModelingToolsManagerCommands& ToolManagerCommands = FModelingToolsManagerCommands::Get();
		const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
		CommandList->MapAction(ToolManagerCommands.LaunchUVEditor,
			FExecuteAction::CreateLambda([this, UVEditorAPI]() 
            {
				if (UVEditorAPI) 
                {
					EToolsContextScope ToolScope = GetDefaultToolScope();
					UEditorInteractiveToolsContext* UseToolsContext = GetInteractiveToolsContext(ToolScope);
					if (ensure(UseToolsContext != nullptr) == false)
					{
						return;
					}

					TArray<UObject*> SelectedActors, SelectedComponents;
					TArray<TObjectPtr<UObject>> SelectedObjects;
					UseToolsContext->GetParentEditorModeManager()->GetSelectedActors()->GetSelectedObjects(SelectedActors);
					UseToolsContext->GetParentEditorModeManager()->GetSelectedComponents()->GetSelectedObjects(SelectedComponents);
					SelectedObjects.Append(SelectedActors);
					SelectedObjects.Append(SelectedComponents);
					UVEditorAPI->LaunchUVEditor(SelectedObjects);
				}
				}),
			FCanExecuteAction::CreateLambda([this, UVEditorAPI]() 
            {
				if (UVEditorAPI) 
                {
					EToolsContextScope ToolScope = GetDefaultToolScope();
					UEditorInteractiveToolsContext* UseToolsContext = GetInteractiveToolsContext(ToolScope);
					if (ensure(UseToolsContext != nullptr) == false)
					{
						return false;
					}

					TArray<UObject*> SelectedActors, SelectedComponents;
					TArray<TObjectPtr<UObject>> SelectedObjects;
					UseToolsContext->GetParentEditorModeManager()->GetSelectedActors()->GetSelectedObjects(SelectedActors);
					UseToolsContext->GetParentEditorModeManager()->GetSelectedComponents()->GetSelectedObjects(SelectedComponents);
					SelectedObjects.Append(SelectedActors);
					SelectedObjects.Append(SelectedComponents);
					return UVEditorAPI->CanLaunchUVEditor(SelectedObjects);
				}
				return false;
				})
			);
	}

}



void UModelingToolsEditorMode::Exit()
{
	FWorldDelegates::PreLevelRemovedFromWorld.RemoveAll(this);
	if (BlueprintPreCompileHandle.IsValid())
	{
		GEditor->OnBlueprintPreCompile().Remove(BlueprintPreCompileHandle);
	}

	// shutdown selection interaction
	if (SelectionInteraction != nullptr)
	{
		SelectionInteraction->Shutdown();
		GetInteractiveToolsContext()->InputRouter->ForceTerminateSource(SelectionInteraction);
		GetInteractiveToolsContext()->InputRouter->DeregisterSource(SelectionInteraction);
		SelectionInteraction = nullptr;
	}

	// stop listening to selection changes. On Editor Shutdown, some of these values become null, which will result in an ensure/crash
	if (SelectionModifiedEventHandle.IsValid() && UObjectInitialized() && GetModeManager() 
		&& ( GetModeManager()->GetSelectedActors() != nullptr) )
	{
		GetModeManager()->GetSelectedActors()->SelectionChangedEvent.Remove(SelectionModifiedEventHandle);
	}

	// exit any exclusive active tools w/ cancel
	if (UInteractiveTool* ActiveTool = GetToolManager()->GetActiveTool(EToolSide::Left))
	{
		if (Cast<IInteractiveToolExclusiveToolAPI>(ActiveTool))
		{
			GetToolManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);
		}
	}

	// Shutdown SelectionManager. Wait until after Tool shutdown in case some restore-selection
	// is involved (although since we are exiting Mode this currently would never matter)
	if (SelectionManager != nullptr)
	{
		SelectionManager->OnSelectionModified.Remove(SelectionManager_SelectionModifiedHandle);
		SelectionManager->ClearSelection();
		SelectionManager->Shutdown();		// will clear active targets

		// hopefully temporary...remove SelectionManager from ContextObjectStore
		GetInteractiveToolsContext()->ContextObjectStore->RemoveContextObject(SelectionManager);

		SelectionManager = nullptr;
	}

	//
	// Engine Analytics
	//
	// Log mode ending
	if (FEngineAnalytics::IsAvailable())
	{
		const FTimespan ModeUsageDuration = FDateTime::UtcNow() - LastModeStartTimestamp;

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), FDateTime::UtcNow().ToString()));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("Duration.Seconds"), static_cast<float>(ModeUsageDuration.GetTotalSeconds())));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.Exit"), Attributes);
	}

#if ENABLE_STYLUS_SUPPORT 
	StylusStateTracker = nullptr;
#endif

	UModelingToolsHostCustomizationAPI::Deregister(GetInteractiveToolsContext());

	// TODO: cannot deregister currently because if another mode is also registering, its Enter()
	// will be called before our Exit()
	//UE::TransformGizmoUtil::DeregisterTransformGizmoContextObject(ToolsContext.Get());
	
	// deregister snapping manager and shut down level objects tracker
	LevelObjectsObserver->Shutdown();		// do this first because it is going to fire events on the snapping manager
	LevelObjectsObserver.Reset();
	UE::Geometry::DeregisterSceneSnappingManager(GetInteractiveToolsContext());
	SceneSnappingManager = nullptr;

	// TODO: cannot deregister currently because if another mode is also registering, its Enter()
	// will be called before our Exit()
	UEditorModelingObjectsCreationAPI* ObjectCreationAPI = UEditorModelingObjectsCreationAPI::Find(GetInteractiveToolsContext());
	if (ObjectCreationAPI)
	{
		ObjectCreationAPI->GetNewAssetPathNameCallback.Unbind();
		ObjectCreationAPI->OnModelingMeshCreated.Remove(MeshCreatedEventHandle);
		ObjectCreationAPI->OnModelingTextureCreated.Remove(TextureCreatedEventHandle);
		ObjectCreationAPI->OnModelingMaterialCreated.Remove(MaterialCreatedEventHandle);
		//UEditorModelingObjectsCreationAPI::Deregister(ToolsContext.Get());		// cannot do currently because of shared ToolsContext, revisit in future
	}

	FModelingModeActionCommands::UnRegisterCommandBindings(Toolkit->GetToolkitCommands());

	// clear realtime viewport override
	ConfigureRealTimeViewportsOverride(false);

	// re-enable HitProxy rendering
	GetInteractiveToolsContext()->SetEnableRenderingDuringHitProxyPass(true);

	// Call base Exit method to ensure proper cleanup
	UEdMode::Exit();
}

void UModelingToolsEditorMode::OnEditorClosed()
{
	// On editor close, Exit() should run to clean up, but this happens very late.
	// Close out any active Tools or Selections to mitigate any late-destruction issues.

	if (SelectionManager != nullptr)
	{
		SelectionManager->ClearSelection();
		SelectionManager->ClearActiveTargets();
	}

	if (GetModeManager() != nullptr 
		&& GetInteractiveToolsContext() != nullptr 
		&& GetToolManager() != nullptr 
		&& GetToolManager()->HasAnyActiveTool())
	{
		GetToolManager()->DeactivateTool(EToolSide::Mouse, EToolShutdownType::Cancel);
	}

	if (EditorClosedEventHandle.IsValid() && GEditor)
	{
		GEditor->OnEditorClose().Remove(EditorClosedEventHandle);
	}
}


void UModelingToolsEditorMode::OnBlueprintPreCompile(UBlueprint* Blueprint)
{
	// if a Blueprint is compiled, all old instances of it in a level go "stale" and new
	// instances are created. Currently SelectionManager does not handle this replacement. 
	// Seems quite hard to know if Blueprint is a parent of any active targets, so if
	// a Blueprint is compiled we will just clear out any active selection & targets to avoid
	// potential crashes. Note that this also breaks undo somewhat, as the FChanges seem to 
	// still be registered against the 'old' instance pointer and hence are ignored/skipped.
	if (SelectionManager != nullptr)
	{
		SelectionManager->ClearSelection();
		SelectionManager->ClearActiveTargets();
	}
}


void UModelingToolsEditorMode::OnToolsContextRender(IToolsContextRenderAPI* RenderAPI)
{
	if (SelectionInteraction)
	{
		SelectionInteraction->Render(RenderAPI);

		// Bake in transform changes. Note that if we do this in OnToolsContextTick, it will 
		// still block rendering updates if it is too expensive, unless it is only done every second Tick
		SelectionInteraction->ApplyPendingTransformInteractions();
	}

	if (GetSelectionManager())
	{
		// currently relying on debug rendering to visualize selections
		GetSelectionManager()->DebugRender(RenderAPI);
	}
}

void UModelingToolsEditorMode::OnToolsContextDrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (SelectionInteraction)
	{
		SelectionInteraction->DrawHUD(Canvas, RenderAPI);
	}
}

bool UModelingToolsEditorMode::ShouldToolStartBeAllowed(const FString& ToolIdentifier) const
{
	if (UInteractiveToolManager* Manager = GetToolManager())
	{
		if (UInteractiveTool* Tool = Manager->GetActiveTool(EToolSide::Left))
		{
			IInteractiveToolExclusiveToolAPI* ExclusiveAPI = Cast<IInteractiveToolExclusiveToolAPI>(Tool);
			if (ExclusiveAPI)
			{
				return false;
			}
		}
	}
	return Super::ShouldToolStartBeAllowed(ToolIdentifier);
}


bool UModelingToolsEditorMode::GetGeometrySelectionChangesAllowed() const
{
	// disable selection system if it is...disabled
	if (GetMeshElementSelectionSystemEnabled() == false)
	{
		return false;
	}

	// disable selection system if we are in a Tool
	if ( GetToolManager() && GetToolManager()->HasAnyActiveTool() )
	{
		return false;
	}
	return true;
}

bool UModelingToolsEditorMode::TestForEditorGizmoHit(const FInputDeviceRay& ClickPos) const
{
	// Because the editor gizmo does not participate in InputRouter behavior system, in some input behaviors
	// we need to filter out clicks on the gizmo. This function can do this check.
	if ( ShouldDrawWidget() )
	{
		FViewport* FocusedViewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
		HHitProxy* HitResult = FocusedViewport->GetHitProxy(ClickPos.ScreenPosition.X, ClickPos.ScreenPosition.Y);
		if (HitResult && HitResult->IsA(HWidgetAxis::StaticGetType()))
		{
			return true;
		}
	}
	return false;
}


bool UModelingToolsEditorMode::GetMeshElementSelectionSystemEnabled() const
{
	return bSelectionSystemEnabled;
}

void UModelingToolsEditorMode::NotifySelectionSystemEnabledStateModified()
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	bool bNewState = Settings->bEnablePersistentSelections;
	if (bNewState != bSelectionSystemEnabled)
	{
		if (bNewState == true)
		{
			UpdateSelectionManagerOnEditorSelectionChange(true);		// do like a mode enter so that we get an undoable active-target state
		}
		else
		{
			if (SelectionManager && (SelectionManager->HasSelection() || SelectionManager->HasActiveTargets()))
			{
				GetToolManager()->GetContextTransactionsAPI()->BeginUndoTransaction(LOCTEXT("InitializeSelection", "Initialize Selection"));

				SelectionManager->SynchronizeActiveTargets(TArray<FGeometryIdentifier>(),
					[this]() { SelectionManager->ClearSelection(); });

				GetToolManager()->GetContextTransactionsAPI()->EndUndoTransaction();
			}
		}

		// update things

		bSelectionSystemEnabled = bNewState;
		GetInteractiveToolsContext()->PostInvalidation();
	}
}


void UModelingToolsEditorMode::UpdateSelectionManagerOnEditorSelectionChange(bool bEnteringMode)
{
	if (GetMeshElementSelectionSystemEnabled() == false || SelectionManager == nullptr) return;

	// if we are in undo/redo, ignore selection change notifications, the required
	// changes are handled via FChanges that SelectionManager has emitted
	if (GIsTransacting)
	{
		return;
	}

	// Find selected Component types that are currently supported. Currently determining this via explicit casting,
	// probably it should be handled by the Selector Factories registered in UModelingToolsEditorMode::Enter(),
	// possibly via the SelectionManager. 

	TArray<UDynamicMeshComponent*> SelectedDynamicMeshComponents;
	TArray<UStaticMeshComponent*> SelectedStaticMeshComponents;
	TArray<UBrushComponent*> SelectedBrushComponents;

	if (GEditor->GetSelectedComponents()->Num() > 0)
	{
		// if we have supported Components selected on a multi-Component Actor, they will be
		// returned via these functions
		GEditor->GetSelectedComponents()->GetSelectedObjects<UDynamicMeshComponent>(SelectedDynamicMeshComponents);
		GEditor->GetSelectedComponents()->GetSelectedObjects<UStaticMeshComponent>(SelectedStaticMeshComponents);
		GEditor->GetSelectedComponents()->GetSelectedObjects<UBrushComponent>(SelectedBrushComponents);
	}
	// Conceivably this could be an 'else', however currently in the Editor when a Volume Actor is selected,
	// GetSelectedComponents()->Num() > 0 but no BrushComponent will be found (it appears to be some kind of TypedElement wrapper).
	// However note that this might result in some unexpected Meshes being Selectable on multi-Component Actors
	if (SelectedDynamicMeshComponents.Num() == 0 && SelectedStaticMeshComponents.Num() == 0 && SelectedBrushComponents.Num() == 0)
	{
		// assume Actor selection, find all valid Components on the selected Actors
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
		for (AActor* Actor : SelectedActors)
		{
			Actor->ForEachComponent(false, [&](UActorComponent* Component) 
			{
				if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
				{
					SelectedDynamicMeshComponents.Add(DynamicMeshComponent);
				}
				if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
				{
					SelectedStaticMeshComponents.Add(StaticMeshComponent);
				}
				if (UBrushComponent* BrushComponent = Cast<UBrushComponent>(Component))
				{
					SelectedBrushComponents.Add(BrushComponent);
				}
			});
		}
	}

	// convert selected Component types into selection Identifiers
	TArray<FGeometryIdentifier> ValidIdentifiers;
	for (UDynamicMeshComponent* DynamicMeshComponent : SelectedDynamicMeshComponents)
	{
		ValidIdentifiers.Add(FGeometryIdentifier::PrimitiveComponent(DynamicMeshComponent, FGeometryIdentifier::EObjectType::DynamicMeshComponent));
	}
	if (bEnableStaticMeshElementSelection)
	{
		for (UStaticMeshComponent* StaticMeshComponent : SelectedStaticMeshComponents)
		{
			ValidIdentifiers.Add(FGeometryIdentifier::PrimitiveComponent(StaticMeshComponent, FGeometryIdentifier::EObjectType::StaticMeshComponent));
		}
	}
	if (bEnableVolumeElementSelection)
	{
		for (UBrushComponent* BrushComponent : SelectedBrushComponents)
		{
			ValidIdentifiers.Add(FGeometryIdentifier::PrimitiveComponent(BrushComponent, FGeometryIdentifier::EObjectType::BrushComponent));
		}
	}


	// This is gross. If we are entering the Mode, we need to update the SelectionManager w/ the current state. However this
	// update needs to be undoable. Since we are not part of whatever Transaction was involved in changing modes, we are going
	// to have to emit our own Transaction, which will then be an explicit undo step the user has to go through :(
	bool bPendingCloseTransaction = false;
	if (bEnteringMode && ValidIdentifiers.Num() > 0)
	{
		GetToolManager()->GetContextTransactionsAPI()->BeginUndoTransaction(LOCTEXT("InitializeSelection", "Initialize Selection"));
		bPendingCloseTransaction = true;
	}

	// If Editor is creating a transaction, we assume we must be in a selection change.
	// Need to handle all SelectionManager changes (deselect + change-targets) during
	// the transaction so that undo/redo works properly.
	// (note that if we are bEnteringMode, we just opened a transaction and so this branch will still be taken...)
	bool bCreatingTransaction = (GUndo != nullptr);
	if (bCreatingTransaction)
	{
		SelectionManager->SynchronizeActiveTargets(ValidIdentifiers,
			[this]() {
				SelectionManager->ClearSelection();
			});
	}

	// close out transaction if it was still open
	if (bPendingCloseTransaction)
	{
		GetToolManager()->GetContextTransactionsAPI()->EndUndoTransaction();
	}
}


bool UModelingToolsEditorMode::BoxSelect(FBox& InBox, bool InSelect) 
{
	// not handling yet
	return false;
}

bool UModelingToolsEditorMode::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect)
{
	if (GetMeshElementSelectionSystemEnabled()
		&& SelectionManager
		&& SelectionManager->HasActiveTargets() 
		&& SelectionManager->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None)
	{
		UE::Geometry::FGeometrySelectionUpdateConfig UpdateConfig;
		UpdateConfig.ChangeType = UE::Geometry::EGeometrySelectionChangeType::Replace;
		if ( InViewportClient->IsShiftPressed() )
		{
			UpdateConfig.ChangeType = UE::Geometry::EGeometrySelectionChangeType::Add;
		}
		else if ( InViewportClient->IsCtrlPressed() && InViewportClient->IsAltPressed() == false )
		{
			UpdateConfig.ChangeType = UE::Geometry::EGeometrySelectionChangeType::Remove;
		}			

		UE::Geometry::FGeometrySelectionUpdateResult Result;
		SelectionManager->UpdateSelectionViaConvex(
			InFrustum, UpdateConfig, Result);

		return true;	// always consume marquee even if it missed, as the miss will usually just be a mistake
	}

	// not handling yet
	return false;
}



void UModelingToolsEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FModelingToolsEditorModeToolkit);
}

void UModelingToolsEditorMode::OnToolPostBuild(
	UInteractiveToolManager* InToolManager, EToolSide InSide, 
	UInteractiveTool* InBuiltTool, UInteractiveToolBuilder* InToolBuilder, const FToolBuilderState& ToolState)
{
	// Want to clear active selection when a Tool starts, but we have to wait until after it has been
	// built, so that the Tool has a chance to see the Selection
	if (GetSelectionManager() && GetSelectionManager()->HasSelection())
	{
		ensureMsgf(!GetSelectionManager()->HasSavedSelection(), TEXT("Selection manager should not already have a saved selection before we save-on-clear here in tool setup."));
		GetSelectionManager()->ClearSelection(/*bSaveSelectionBeforeClear*/ true);
	}
}

void UModelingToolsEditorMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	// disable slate throttling so that Tool background computes responding to sliders can properly be processed
	// on Tool Tick. Otherwise, when a Tool kicks off a background update in a background thread, the computed
	// result will be ignored until the user moves the slider, ie you cannot hold down the mouse and wait to see
	// the result. This apparently broken behavior is currently by-design.
	FSlateThrottleManager::Get().DisableThrottle(true);

	FModelingToolActionCommands::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), false);
	
	if( FEngineAnalytics::IsAvailable() )
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.ToolStarted"),
		                                            TEXT("ToolName"),
		                                            GetToolName(*Tool));
	}
}

void UModelingToolsEditorMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	// re-enable slate throttling (see OnToolStarted)
	FSlateThrottleManager::Get().DisableThrottle(false);

	FModelingToolActionCommands::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), true);
	
	// We may require a gizmo location update despite not having changed the selection (transform tool,
	// edit pivot, etc).
	GUnrealEd->UpdatePivotLocationForSelection();

	if( FEngineAnalytics::IsAvailable() )
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MeshModelingMode.ToolEnded"),
		                                            TEXT("ToolName"),
		                                            GetToolName(*Tool));
	}
}

void UModelingToolsEditorMode::BindCommands()
{
	const FModelingToolsManagerCommands& ToolManagerCommands = FModelingToolsManagerCommands::Get();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	CommandList->MapAction(
		ToolManagerCommands.AcceptActiveTool,
		FExecuteAction::CreateLambda([this]() { 
			GetInteractiveToolsContext()->EndTool(EToolShutdownType::Accept); 
		}),
		FCanExecuteAction::CreateLambda([this]() { return GetInteractiveToolsContext()->CanAcceptActiveTool(); }),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateLambda([this]() {return GetInteractiveToolsContext()->ActiveToolHasAccept(); }),
		EUIActionRepeatMode::RepeatDisabled
		);

	CommandList->MapAction(
		ToolManagerCommands.CancelActiveTool,
		FExecuteAction::CreateLambda([this]() { GetInteractiveToolsContext()->EndTool(EToolShutdownType::Cancel); }),
		FCanExecuteAction::CreateLambda([this]() { return GetInteractiveToolsContext()->CanCancelActiveTool(); }),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateLambda([this]() {return GetInteractiveToolsContext()->ActiveToolHasAccept(); }),
		EUIActionRepeatMode::RepeatDisabled
		);

	CommandList->MapAction(
		ToolManagerCommands.CompleteActiveTool,
		FExecuteAction::CreateLambda([this]() { GetInteractiveToolsContext()->EndTool(EToolShutdownType::Completed); }),
		FCanExecuteAction::CreateLambda([this]() { return GetInteractiveToolsContext()->CanCompleteActiveTool(); }),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateLambda([this]() {return GetInteractiveToolsContext()->CanCompleteActiveTool(); }),
		EUIActionRepeatMode::RepeatDisabled
		);

	// These aren't activated by buttons but have default chords that bind the keypresses to the action.
	CommandList->MapAction(
		ToolManagerCommands.AcceptOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this]() { AcceptActiveToolActionOrTool(); }),
		FCanExecuteAction::CreateLambda([this]() {
				return GetInteractiveToolsContext()->CanAcceptActiveTool() || GetInteractiveToolsContext()->CanCompleteActiveTool();
			}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);

	CommandList->MapAction(
		ToolManagerCommands.CancelOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this]() { CancelActiveToolActionOrTool(); }),
		FCanExecuteAction::CreateLambda([this]() {
				return GetInteractiveToolsContext()->CanCompleteActiveTool() || GetInteractiveToolsContext()->CanCancelActiveTool();
			}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);
}


void UModelingToolsEditorMode::AcceptActiveToolActionOrTool()
{
	// if we have an active Tool that implements 
	if (GetToolManager()->HasAnyActiveTool())
	{
		UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
		IInteractiveToolNestedAcceptCancelAPI* CancelAPI = Cast<IInteractiveToolNestedAcceptCancelAPI>(Tool);
		if (CancelAPI && CancelAPI->SupportsNestedAcceptCommand() && CancelAPI->CanCurrentlyNestedAccept())
		{
			bool bAccepted = CancelAPI->ExecuteNestedAcceptCommand();
			if (bAccepted)
			{
				return;
			}
		}
	}

	const EToolShutdownType ShutdownType = GetInteractiveToolsContext()->CanAcceptActiveTool() ? EToolShutdownType::Accept : EToolShutdownType::Completed;
	GetInteractiveToolsContext()->EndTool(ShutdownType);
}


void UModelingToolsEditorMode::CancelActiveToolActionOrTool()
{
	// if we have an active Tool that implements 
	if (GetToolManager()->HasAnyActiveTool())
	{
		UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
		IInteractiveToolNestedAcceptCancelAPI* CancelAPI = Cast<IInteractiveToolNestedAcceptCancelAPI>(Tool);
		if (CancelAPI && CancelAPI->SupportsNestedCancelCommand() && CancelAPI->CanCurrentlyNestedCancel())
		{
			bool bCancelled = CancelAPI->ExecuteNestedCancelCommand();
			if (bCancelled)
			{
				return;
			}
		}
	}

	const EToolShutdownType ShutdownType = GetInteractiveToolsContext()->CanCancelActiveTool() ? EToolShutdownType::Cancel : EToolShutdownType::Completed;
	GetInteractiveToolsContext()->EndTool(ShutdownType);
}

void UModelingToolsEditorMode::ModelingModeShortcutRequested(EModelingModeActionCommands Command)
{
	if (Command == EModelingModeActionCommands::FocusViewToCursor)
	{
		FocusCameraAtCursorHotkey();
	}
	else if (Command == EModelingModeActionCommands::ToggleSelectionLockState)
	{
		if (SelectionManager)
		{
			if (SelectionManager->GetAnyCurrentTargetsLocked())
			{
				SelectionManager->SetCurrentTargetsLockState(false);
			}
			else
			{
				SelectionManager->SetCurrentTargetsLockState(true);
			}
		}
	}
}


void UModelingToolsEditorMode::FocusCameraAtCursorHotkey()
{
	FRay Ray = GetInteractiveToolsContext()->GetLastWorldRay();

	double NearestHitDist = (double)HALF_WORLD_MAX;
	FVector HitPoint = FVector::ZeroVector;

	// cast ray against visible objects
	FHitResult WorldHitResult;
	if (ToolSceneQueriesUtil::FindNearestVisibleObjectHit( USceneSnappingManager::Find(GetToolManager()), WorldHitResult, Ray) )
	{
		HitPoint = WorldHitResult.ImpactPoint;
		NearestHitDist = (double)Ray.GetParameter(HitPoint);
	}

	// cast ray against tool
	if (GetToolManager()->HasAnyActiveTool())
	{
		UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
		IInteractiveToolCameraFocusAPI* FocusAPI = Cast<IInteractiveToolCameraFocusAPI>(Tool);
		if (FocusAPI && FocusAPI->SupportsWorldSpaceFocusPoint())
		{
			FVector ToolHitPoint;
			if (FocusAPI->GetWorldSpaceFocusPoint(Ray, ToolHitPoint))
			{
				double HitDepth = (double)Ray.GetParameter(ToolHitPoint);
				if (HitDepth < NearestHitDist)
				{
					NearestHitDist = HitDepth;
					HitPoint = ToolHitPoint;
				}
			}
		}
	}


	if (NearestHitDist < (double)HALF_WORLD_MAX && GCurrentLevelEditingViewportClient)
	{
		GCurrentLevelEditingViewportClient->CenterViewportAtPoint(HitPoint, false);
	}
}


bool UModelingToolsEditorMode::ComputeBoundingBoxForViewportFocus(AActor* Actor, UPrimitiveComponent* PrimitiveComponent, FBox& InOutBox) const
{
	auto ProcessFocusBoxFunc = [](FBox& FocusBoxInOut)
	{
		double MaxDimension = FocusBoxInOut.GetExtent().GetMax();
		double ExpandAmount = (MaxDimension > SMALL_NUMBER) ? (MaxDimension * 0.2) : 25;		// 25 is a bit arbitrary here...
		FocusBoxInOut = FocusBoxInOut.ExpandBy(MaxDimension * 0.2);
	};

	// if Tool supports custom Focus box, use that
	if (GetToolManager()->HasAnyActiveTool())
	{
		UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
		IInteractiveToolCameraFocusAPI* FocusAPI = Cast<IInteractiveToolCameraFocusAPI>(Tool);
		if (FocusAPI && FocusAPI->SupportsWorldSpaceFocusBox() )
		{
			InOutBox = FocusAPI->GetWorldSpaceFocusBox();
			if (InOutBox.IsValid)
			{
				ProcessFocusBoxFunc(InOutBox);
				return true;
			}
		}
	}

	// if we have an active Selection we can focus on that
	if (GetSelectionManager() && GetSelectionManager()->HasSelection())
	{
		UE::Geometry::FGeometrySelectionBounds SelectionBounds;
		GetSelectionManager()->GetSelectionBounds(SelectionBounds);
		InOutBox = (FBox)SelectionBounds.WorldBounds;
		ProcessFocusBoxFunc(InOutBox);
		return true;
	}

	// fallback to base focus behavior
	return false;
}


bool UModelingToolsEditorMode::GetPivotForOrbit(FVector& OutPivot) const
{
	if (GCurrentLevelEditingViewportClient)
	{
		OutPivot = GCurrentLevelEditingViewportClient->GetViewTransform().GetLookAt();
		return true;
	}
	return false;
}



void UModelingToolsEditorMode::ConfigureRealTimeViewportsOverride(bool bEnable)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (const TSharedPtr<SLevelViewport>& ViewportWindow : Viewports)
		{
			if (ViewportWindow.IsValid())
			{
				FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
				const FText SystemDisplayName = LOCTEXT("RealtimeOverrideMessage_ModelingMode", "Modeling Mode");
				if (bEnable)
				{
					Viewport.AddRealtimeOverride(bEnable, SystemDisplayName);
				}
				else
				{
					Viewport.RemoveRealtimeOverride(SystemDisplayName, false);
				}
			}
		}
	}
}



#undef LOCTEXT_NAMESPACE

