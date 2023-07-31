// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsEditorMode.h"

#include "SkeletalMeshModelingToolsEditorModeToolkit.h"
#include "SkeletalMeshModelingToolsCommands.h"

#include "BaseGizmos/TransformGizmoUtil.h"
#include "EdMode.h"
#include "EdModeInteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "IStylusInputModule.h"
#include "IStylusState.h"
#include "ToolTargets/SkeletalMeshComponentToolTarget.h"

#include "DeformMeshPolygonsTool.h"
#include "DisplaceMeshTool.h"
#include "DynamicMeshSculptTool.h"
#include "EditMeshPolygonsTool.h"
#include "HoleFillTool.h"
#include "LatticeDeformerTool.h"
#include "MeshAttributePaintTool.h"
#include "MeshSpaceDeformerTool.h"
#include "MeshVertexSculptTool.h"
#include "ModelingToolsManagerActions.h"
#include "OffsetMeshTool.h"
#include "PolygonOnMeshTool.h"
#include "ProjectToTargetTool.h"
#include "RemeshMeshTool.h"
#include "RemoveOccludedTrianglesTool.h"
#include "SimplifyMeshTool.h"
#include "SkinWeightsBindingTool.h"
#include "SkinWeightsPaintTool.h"
#include "SmoothMeshTool.h"
#include "ToolTargetManager.h"
#include "WeldMeshEdgesTool.h"


#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsEditorMode"


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
		UStylusInputSubsystem* StylusSubsystem = GEditor->GetEditorSubsystem<UStylusInputSubsystem>();
		StylusSubsystem->RemoveMessageHandler(*this);
	}

	void OnStylusStateChanged(const FStylusState& NewState, int32 StylusIndex) override
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


// NOTE: This is a simple proxy at the moment. In the future we want to pull in more of the 
// modeling tools as we add support in the skelmesh storage.

const FEditorModeID USkeletalMeshModelingToolsEditorMode::Id("SkeletalMeshModelingToolsEditorMode");


USkeletalMeshModelingToolsEditorMode::USkeletalMeshModelingToolsEditorMode() 
{
	Info = FEditorModeInfo(Id, LOCTEXT("SkeletalMeshModelingMode", "Skeletal Mesh Modeling"), FSlateIcon(), false);
}


USkeletalMeshModelingToolsEditorMode::USkeletalMeshModelingToolsEditorMode(FVTableHelper& Helper) :
	UBaseLegacyWidgetEdMode(Helper)
{
	
}


USkeletalMeshModelingToolsEditorMode::~USkeletalMeshModelingToolsEditorMode()
{
	// Implemented in the CPP file so that the destructor for TUniquePtr<FStylusStateTracker> gets correctly compiled.
}


void USkeletalMeshModelingToolsEditorMode::Initialize()
{
	UBaseLegacyWidgetEdMode::Initialize();
}


void USkeletalMeshModelingToolsEditorMode::Enter()
{
	UEdMode::Enter();

	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentToolTargetFactory>(GetInteractiveToolsContext()->TargetManager));

	StylusStateTracker = MakeUnique<FStylusStateTracker>();
	// register gizmo helper
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(GetInteractiveToolsContext());

	const FModelingToolsManagerCommands& ToolManagerCommands = FModelingToolsManagerCommands::Get();

		/*
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyEditTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolyDeformTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginHoleFillTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPolygonCutTool);
	}
	else if (PaletteName == ProcessingTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSimplifyMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemeshMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginWeldEdgesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemoveOccludedTrianglesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginProjectToTargetTool);
	}
	else if (PaletteName == DeformTabName)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSculptMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginRemeshSculptMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSmoothMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginOffsetMeshTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeshSpaceDeformerTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginLatticeDeformerTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginDisplaceMeshTool);
		*/

	RegisterTool(ToolManagerCommands.BeginPolyEditTool, TEXT("BeginPolyEditTool"), NewObject<UEditMeshPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginPolyDeformTool, TEXT("BeginPolyDeformTool"), NewObject<UDeformMeshPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginHoleFillTool, TEXT("BeginHoleFillTool"), NewObject<UHoleFillToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginPolygonCutTool, TEXT("BeginPolyCutTool"), NewObject<UPolygonOnMeshToolBuilder>());

	RegisterTool(ToolManagerCommands.BeginSimplifyMeshTool, TEXT("BeginSimplifyMeshTool"), NewObject<USimplifyMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginRemeshMeshTool, TEXT("BeginRemeshMeshTool"), NewObject<URemeshMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginWeldEdgesTool, TEXT("BeginWeldEdgesTool"), NewObject<UWeldMeshEdgesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginRemoveOccludedTrianglesTool, TEXT("BeginRemoveOccludedTrianglesTool"), NewObject<URemoveOccludedTrianglesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginProjectToTargetTool, TEXT("BeginProjectToTargetTool"), NewObject<UProjectToTargetToolBuilder>());
	

	auto MoveVerticesToolBuilder = NewObject<UMeshVertexSculptToolBuilder>();
	MoveVerticesToolBuilder->StylusAPI = StylusStateTracker.Get();
	RegisterTool(ToolManagerCommands.BeginSculptMeshTool, TEXT("BeginSculptMeshTool"), MoveVerticesToolBuilder);

	auto DynaSculptToolBuilder = NewObject<UDynamicMeshSculptToolBuilder>();
	DynaSculptToolBuilder->bEnableRemeshing = true;
	DynaSculptToolBuilder->StylusAPI = StylusStateTracker.Get();
	RegisterTool(ToolManagerCommands.BeginRemeshSculptMeshTool, TEXT("BeginRemeshSculptMeshTool"), DynaSculptToolBuilder);
	
	RegisterTool(ToolManagerCommands.BeginSmoothMeshTool, TEXT("BeginSmoothMeshTool"), NewObject<USmoothMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginOffsetMeshTool, TEXT("BeginOffsetMeshTool"), NewObject<UOffsetMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginMeshSpaceDeformerTool, TEXT("BeginMeshSpaceDeformerTool"), NewObject<UMeshSpaceDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginLatticeDeformerTool, TEXT("BeginLatticeDeformerTool"), NewObject<ULatticeDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDisplaceMeshTool, TEXT("BeginDisplaceMeshTool"), NewObject<UDisplaceMeshToolBuilder>());

	// RegisterTool(ToolManagerCommands.BeginMeshAttributePaintTool, TEXT("BeginMeshAttributePaintTool"), NewObject<UMeshAttributePaintToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSkinWeightsPaintTool, TEXT("BeginSkinWeightsPaintTool"), NewObject<USkinWeightsPaintToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSkinWeightsBindingTool, TEXT("BeginSkinWeightsBindingTool"), NewObject<USkinWeightsBindingToolBuilder>());

	GetInteractiveToolsContext()->ToolManager->SelectActiveToolType(EToolSide::Left, TEXT("BeginSkinWeightsPaintTool"));
}


void USkeletalMeshModelingToolsEditorMode::Exit()
{
	StylusStateTracker = nullptr;

	UEdMode::Exit();
}


void USkeletalMeshModelingToolsEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FSkeletalMeshModelingToolsEditorModeToolkit);
}


void USkeletalMeshModelingToolsEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	Super::Tick(ViewportClient, DeltaTime);
}


#undef LOCTEXT_NAMESPACE
