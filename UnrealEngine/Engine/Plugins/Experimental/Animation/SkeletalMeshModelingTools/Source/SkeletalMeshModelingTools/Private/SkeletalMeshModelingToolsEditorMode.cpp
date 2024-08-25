// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsEditorMode.h"

#include "AttributeEditorTool.h"
#include "SkeletalMeshModelingToolsEditorModeToolkit.h"
#include "SkeletalMeshModelingToolsCommands.h"
#include "SkeletalMeshGizmoUtils.h"

#include "BaseGizmos/TransformGizmoUtil.h"
#include "EdMode.h"
#include "EdModeInteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"

// Stylus support is currently disabled due to issues with the stylus plugin
// We are leaving the code in this cpp file, defined out, so that it is easier to bring back if/when the stylus plugin is improved.
#define ENABLE_STYLUS_SUPPORT 0

#if ENABLE_STYLUS_SUPPORT 
#include "IStylusInputModule.h"
#include "IStylusState.h"
#endif

#include "ToolTargets/SkeletalMeshComponentToolTarget.h"
#include "Components/SkeletalMeshComponent.h"

#include "ConvertToPolygonsTool.h"
#include "DeformMeshPolygonsTool.h"
#include "DisplaceMeshTool.h"
#include "DynamicMeshSculptTool.h"
#include "EditMeshPolygonsTool.h"
#include "EditorInteractiveGizmoManager.h"
#include "HoleFillTool.h"
#include "ISkeletalMeshEditor.h"
#include "LatticeDeformerTool.h"
#include "MeshAttributePaintTool.h"
#include "MeshGroupPaintTool.h"
#include "MeshSpaceDeformerTool.h"
#include "MeshVertexSculptTool.h"
#include "ModelingToolsManagerActions.h"
#include "OffsetMeshTool.h"
#include "PersonaModule.h"
#include "PolygonOnMeshTool.h"
#include "ProjectToTargetTool.h"
#include "RemeshMeshTool.h"
#include "RemoveOccludedTrianglesTool.h"
#include "SimplifyMeshTool.h"
#include "SkeletalMeshEditorUtils.h"
#include "SmoothMeshTool.h"
#include "ToolTargetManager.h"
#include "WeldMeshEdgesTool.h"

#include "SkeletalMeshNotifier.h"
#include "Animation/DebugSkelMeshComponent.h"

#include "SkeletalMesh/SkeletalMeshEditionInterface.h"
#include "SkeletalMesh/SkeletonEditingTool.h"
#include "SkeletalMesh/SkinWeightsBindingTool.h"
#include "SkeletalMesh/SkinWeightsPaintTool.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsEditorMode"

#if ENABLE_STYLUS_SUPPORT
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
#endif // ENABLE_STYLUS_SUPPORT


// NOTE: This is a simple proxy at the moment. In the future we want to pull in more of the 
// modeling tools as we add support in the skelmesh storage.

const FEditorModeID USkeletalMeshModelingToolsEditorMode::Id("SkeletalMeshModelingToolsEditorMode");


USkeletalMeshModelingToolsEditorMode::USkeletalMeshModelingToolsEditorMode() 
{
	Info = FEditorModeInfo(Id, LOCTEXT("SkeletalMeshEditingMode", "Skeletal Mesh Editing"), FSlateIcon(), false);
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

	UEditorInteractiveToolsContext* InteractiveToolsContext = GetInteractiveToolsContext();
	
	InteractiveToolsContext->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentToolTargetFactory>(InteractiveToolsContext->TargetManager));

#if ENABLE_STYLUS_SUPPORT
	StylusStateTracker = MakeUnique<FStylusStateTracker>();
#endif
	// register gizmo helper
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(InteractiveToolsContext);
	UE::SkeletalMeshGizmoUtils::RegisterTransformGizmoContextObject(InteractiveToolsContext);
	UE::SkeletalMeshEditorUtils::RegisterEditorContextObject(InteractiveToolsContext);

	const FModelingToolsManagerCommands& ToolManagerCommands = FModelingToolsManagerCommands::Get();

	RegisterTool(ToolManagerCommands.BeginPolyEditTool, TEXT("BeginPolyEditTool"), NewObject<UEditMeshPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginPolyDeformTool, TEXT("BeginPolyDeformTool"), NewObject<UDeformMeshPolygonsToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginHoleFillTool, TEXT("BeginHoleFillTool"), NewObject<UHoleFillToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginPolygonCutTool, TEXT("BeginPolyCutTool"), NewObject<UPolygonOnMeshToolBuilder>());

	RegisterTool(ToolManagerCommands.BeginSimplifyMeshTool, TEXT("BeginSimplifyMeshTool"), NewObject<USimplifyMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginRemeshMeshTool, TEXT("BeginRemeshMeshTool"), NewObject<URemeshMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginWeldEdgesTool, TEXT("BeginWeldEdgesTool"), NewObject<UWeldMeshEdgesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginRemoveOccludedTrianglesTool, TEXT("BeginRemoveOccludedTrianglesTool"), NewObject<URemoveOccludedTrianglesToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginProjectToTargetTool, TEXT("BeginProjectToTargetTool"), NewObject<UProjectToTargetToolBuilder>());
	
	RegisterTool(ToolManagerCommands.BeginPolyGroupsTool, TEXT("BeginPolyGroupsTool"), NewObject<UConvertToPolygonsToolBuilder>());
	UMeshGroupPaintToolBuilder* MeshGroupPaintToolBuilder = NewObject<UMeshGroupPaintToolBuilder>();
#if ENABLE_STYLUS_SUPPORT 
	MeshGroupPaintToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginMeshGroupPaintTool, TEXT("BeginMeshGroupPaintTool"), MeshGroupPaintToolBuilder);
	
	UMeshVertexSculptToolBuilder* MoveVerticesToolBuilder = NewObject<UMeshVertexSculptToolBuilder>();
#if ENABLE_STYLUS_SUPPORT
	MoveVerticesToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginSculptMeshTool, TEXT("BeginSculptMeshTool"), MoveVerticesToolBuilder);

	UDynamicMeshSculptToolBuilder* DynaSculptToolBuilder = NewObject<UDynamicMeshSculptToolBuilder>();
	DynaSculptToolBuilder->bEnableRemeshing = true;
#if ENABLE_STYLUS_SUPPORT
	DynaSculptToolBuilder->StylusAPI = StylusStateTracker.Get();
#endif
	RegisterTool(ToolManagerCommands.BeginRemeshSculptMeshTool, TEXT("BeginRemeshSculptMeshTool"), DynaSculptToolBuilder);
	
	RegisterTool(ToolManagerCommands.BeginSmoothMeshTool, TEXT("BeginSmoothMeshTool"), NewObject<USmoothMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginOffsetMeshTool, TEXT("BeginOffsetMeshTool"), NewObject<UOffsetMeshToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginMeshSpaceDeformerTool, TEXT("BeginMeshSpaceDeformerTool"), NewObject<UMeshSpaceDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginLatticeDeformerTool, TEXT("BeginLatticeDeformerTool"), NewObject<ULatticeDeformerToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginDisplaceMeshTool, TEXT("BeginDisplaceMeshTool"), NewObject<UDisplaceMeshToolBuilder>());

	RegisterTool(ToolManagerCommands.BeginAttributeEditorTool, TEXT("BeginAttributeEditorTool"), NewObject<UAttributeEditorToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginMeshAttributePaintTool, TEXT("BeginMeshAttributePaintTool"), NewObject<UMeshAttributePaintToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSkinWeightsPaintTool, TEXT("BeginSkinWeightsPaintTool"), NewObject<USkinWeightsPaintToolBuilder>());
	RegisterTool(ToolManagerCommands.BeginSkinWeightsBindingTool, TEXT("BeginSkinWeightsBindingTool"), NewObject<USkinWeightsBindingToolBuilder>());

	// Skeleton Editing
	RegisterTool(ToolManagerCommands.BeginSkeletonEditingTool, TEXT("BeginSkeletonEditingTool"), NewObject<USkeletonEditingToolBuilder>());
	
	GetInteractiveToolsContext()->ToolManager->SelectActiveToolType(EToolSide::Left, TEXT("BeginSkinWeightsPaintTool"));
}

UDebugSkelMeshComponent* USkeletalMeshModelingToolsEditorMode::GetSkelMeshComponent() const
{
	FToolBuilderState State; GetToolManager()->GetContextQueriesAPI()->GetCurrentSelectionState(State);
	UActorComponent* SkeletalMeshComponent = ToolBuilderUtil::FindFirstComponent(State, [&](UActorComponent* Component)
	{
		return IsValid(Component) && Component->IsA<UDebugSkelMeshComponent>();
	});

	return Cast<UDebugSkelMeshComponent>(SkeletalMeshComponent);
}

void USkeletalMeshModelingToolsEditorMode::Exit()
{
	UEditorInteractiveToolsContext* InteractiveToolsContext = GetInteractiveToolsContext();
	UE::TransformGizmoUtil::DeregisterTransformGizmoContextObject(InteractiveToolsContext);
	UE::SkeletalMeshGizmoUtils::UnregisterTransformGizmoContextObject(InteractiveToolsContext);
	UE::SkeletalMeshEditorUtils::UnregisterEditorContextObject(InteractiveToolsContext);
	
#if ENABLE_STYLUS_SUPPORT
	StylusStateTracker = nullptr;
#endif

	UEdMode::Exit();
}

void USkeletalMeshModelingToolsEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FSkeletalMeshModelingToolsEditorModeToolkit);
}

bool USkeletalMeshModelingToolsEditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	if (OtherModeID == FPersonaEditModes::SkeletonSelection)
	{
		return true;
	}
	
	return Super::IsCompatibleWith(OtherModeID);
}

void USkeletalMeshModelingToolsEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	Super::Tick(ViewportClient, DeltaTime);
}

bool USkeletalMeshModelingToolsEditorMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if (Binding.IsValid())
	{
		TSharedPtr<ISkeletalMeshEditorBinding> BindingPtr = Binding.Pin();

		TArray<FName> SelectedBones;
		if (HitProxy && BindingPtr->GetNameFunction())
		{
			if (TOptional<FName> BoneName = BindingPtr->GetNameFunction()(HitProxy))
			{
				SelectedBones.Emplace(*BoneName);
			}
		}
		
		BindingPtr->GetNotifier().HandleNotification( SelectedBones, ESkeletalMeshNotifyType::BonesSelected);
	}
	
	return Super::HandleClick(InViewportClient, HitProxy, Click);
}

bool USkeletalMeshModelingToolsEditorMode::ComputeBoundingBoxForViewportFocus(AActor* Actor, UPrimitiveComponent* PrimitiveComponent, FBox& InOutBox) const
{
	// if Tool supports custom Focus box, use that first
	if (GetToolManager()->HasAnyActiveTool())
	{
		UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
		IInteractiveToolCameraFocusAPI* FocusAPI = Cast<IInteractiveToolCameraFocusAPI>(Tool);
		if (FocusAPI && FocusAPI->SupportsWorldSpaceFocusBox())
		{
			InOutBox = FocusAPI->GetWorldSpaceFocusBox();
			return true;
		}
	}

	// focus using selected bones in skel mesh editor
	if (const USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(PrimitiveComponent))
	{
		check(Component->GetSkeletalMeshAsset());
		
		if (Binding.IsValid())
		{
			const TArray<FName> Selection = Binding.Pin()->GetSelectedBones();
			if (!Selection.IsEmpty())
			{
				TArray<FName> AllChildren;

				const FReferenceSkeleton& RefSkeleton = Component->GetSkeletalMeshAsset()->GetRefSkeleton();
				for (const FName& BoneName: Selection)
				{
					const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
					if (BoneIndex > INDEX_NONE)
					{
						// enlarge box
						InOutBox += Component->GetBoneLocation(BoneName);

						// get direct children
						TArray<int32> Children;
						RefSkeleton.GetDirectChildBones(BoneIndex, Children);
						Algo::Transform(Children, AllChildren, [&RefSkeleton](int ChildrenIndex)
						{
							return RefSkeleton.GetBoneName(ChildrenIndex);
						});
					}
				}

				// enlarge box using direct children
				for (const FName& BoneName: AllChildren)
				{
					InOutBox += Component->GetBoneLocation(BoneName);
				}
				
				return true; 
			}
		}
	}
	
	return Super::ComputeBoundingBoxForViewportFocus(Actor, PrimitiveComponent, InOutBox);
}

void USkeletalMeshModelingToolsEditorMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FSkeletalMeshModelingToolsActionCommands::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), false);

	// deactivate SkeletonSelection when a tool is activated.
	// each tool is responsible for activating SkeletonSelection if necessary
	if (Owner)
	{
		Owner->DeactivateMode(FPersonaEditModes::SkeletonSelection);
	}
}

void USkeletalMeshModelingToolsEditorMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FSkeletalMeshModelingToolsActionCommands::UpdateToolCommandBinding(Tool, Toolkit->GetToolkitCommands(), true);

	// reactivate SkeletonSelection when deactivating a tool
	if (Owner)
	{
		Owner->ActivateMode(FPersonaEditModes::SkeletonSelection);
	}
}

void USkeletalMeshModelingToolsEditorMode::SetEditorBinding(const TWeakPtr<ISkeletalMeshEditor>& InSkeletalMeshEditor)
{
	if (!InSkeletalMeshEditor.IsValid())
	{
		return;
	}
	
	Binding = InSkeletalMeshEditor.Pin()->GetBinding();

	if (USkeletalMeshEditorContextObject* ContextObject = UE::SkeletalMeshEditorUtils::GetEditorContextObject(GetInteractiveToolsContext()))
	{
		ContextObject->Init(InSkeletalMeshEditor);
	}
}

ISkeletalMeshEditingInterface* USkeletalMeshModelingToolsEditorMode::GetSkeletonInterface(UInteractiveTool* InTool)
{
	if (!IsValid(InTool) || !InTool->Implements<USkeletalMeshEditingInterface>())
	{
		return nullptr;
	}
	return static_cast<ISkeletalMeshEditingInterface*>(InTool->GetInterfaceAddress(USkeletalMeshEditingInterface::StaticClass()));
}

bool USkeletalMeshModelingToolsEditorMode::NeedsTransformGizmo() const
{
	UInteractiveTool* Tool = GetToolManager()->GetActiveTool(EToolSide::Mouse);
	if (const ISkeletalMeshEditingInterface* SkeletonInterface = GetSkeletonInterface(Tool))
	{
		return !SkeletonInterface->GetSelectedBones().IsEmpty();
	}

	if (Binding.IsValid())
	{
		return !Binding.Pin()->GetSelectedBones().IsEmpty();
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE
