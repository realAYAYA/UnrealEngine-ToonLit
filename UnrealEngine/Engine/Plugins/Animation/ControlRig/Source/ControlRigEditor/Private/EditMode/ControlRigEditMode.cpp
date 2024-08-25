// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/ControlRigEditMode.h"

#include "AnimationEditorPreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "EditMode/ControlRigEditModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "EditMode/SControlRigEditModeTools.h"
#include "Algo/Transform.h"
#include "ControlRig.h"
#include "HitProxies.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "EditMode/SControlRigDetails.h"
#include "EditMode/SControlRigOutliner.h"
#include "ISequencer.h"
#include "SequencerSettings.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "MovieScene.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "Components/SkeletalMeshComponent.h"
#include "EditMode/ControlRigEditModeCommands.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ControlRigEditorModule.h"
#include "Constraint.h"
#include "EngineUtils.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "IControlRigObjectBinding.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRigBlueprint.h"
#include "ControlRigGizmoActor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SEditorViewport.h"
#include "EditMode/AnimDetailsProxy.h"
#include "ScopedTransaction.h"
#include "RigVMModel/RigVMController.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/FKControlRig.h"
#include "ControlRigComponent.h"
#include "EngineUtils.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "IPersonaPreviewScene.h"
#include "PersonaSelectionProxies.h"
#include "PropertyHandle.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/ControlRigSettings.h"
#include "ToolMenus.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "ControlRigSpaceChannelEditors.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "LevelSequence.h"
#include "LevelEditor.h"
#include "InteractiveToolManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "Constraints/MovieSceneConstraintChannelHelper.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "TransformConstraint.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Materials/Material.h"
#include "ControlRigEditorStyle.h"
#include "DragTool_BoxSelect.h"
#include "DragTool_FrustumSelect.h"
#include "AnimationEditorViewportClient.h"
#include "EditorInteractiveGizmoManager.h"
#include "Tools/BakingHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigEditMode)

TAutoConsoleVariable<bool> CVarClickSelectThroughGizmo(TEXT("ControlRig.Sequencer.ClickSelectThroughGizmo"), false, TEXT("When false you can't click through a gizmo and change selection if you will select the gizmo when in Animation Mode, default to false."));

void UControlRigEditModeDelegateHelper::OnPoseInitialized()
{
	if (EditMode)
	{
		EditMode->OnPoseInitialized();
	}
}
void UControlRigEditModeDelegateHelper::PostPoseUpdate()
{
	if (EditMode)
	{
		EditMode->PostPoseUpdate();
	}
}

void UControlRigEditModeDelegateHelper::AddDelegates(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (BoundComponent.IsValid())
	{
		if (BoundComponent.Get() == InSkeletalMeshComponent)
		{
			return;
		}
	}

	RemoveDelegates();

	BoundComponent = InSkeletalMeshComponent;

	if (BoundComponent.IsValid())
	{
		BoundComponent->OnAnimInitialized.AddDynamic(this, &UControlRigEditModeDelegateHelper::OnPoseInitialized);
		OnBoneTransformsFinalizedHandle = BoundComponent->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UControlRigEditModeDelegateHelper::PostPoseUpdate));
	}
}

void UControlRigEditModeDelegateHelper::RemoveDelegates()
{
	if(BoundComponent.IsValid())
	{
		BoundComponent->OnAnimInitialized.RemoveAll(this);
		BoundComponent->UnregisterOnBoneTransformsFinalizedDelegate(OnBoneTransformsFinalizedHandle);
		OnBoneTransformsFinalizedHandle.Reset();
		BoundComponent = nullptr;
	}
}


FName FControlRigEditMode::ModeName("EditMode.ControlRig");

#define LOCTEXT_NAMESPACE "ControlRigEditMode"

/** The different parts of a transform that manipulators can support */
enum class ETransformComponent
{
	None,

	Rotation,

	Translation,

	Scale
};

namespace ControlRigSelectionConstants
{
	/** Distance to trace for physics bodies */
	static const float BodyTraceDistance = 100000.0f;
}

FControlRigEditMode::FControlRigEditMode()
	: bIsChangingControlShapeTransform(false)
	, bIsTracking(false)
	, bManipulatorMadeChange(false)
	, bSelecting(false)
	, bSelectionChanged(false)
	, RecreateControlShapesRequired(ERecreateControlRigShape::RecreateNone)
	, bSuspendHierarchyNotifs(false)
	, CurrentViewportClient(nullptr)
	, bIsChangingCoordSystem(false)
	, InteractionType((uint8)EControlRigInteractionType::None)
	, bShowControlsAsOverlay(false)
	, bIsConstructionEventRunning(false)
{
	ControlProxy = NewObject<UControlRigDetailPanelControlProxies>(GetTransientPackage(), NAME_None);
	ControlProxy->SetFlags(RF_Transactional);
	DetailKeyFrameCache = MakeShared<FDetailKeyFrameCacheAndHandler>();

	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	bShowControlsAsOverlay = Settings->bShowControlsAsOverlay;

	Settings->GizmoScaleDelegate.AddLambda([this](float GizmoScale)
	{
		if (FEditorModeTools* ModeTools = GetModeManager())
		{
			ModeTools->SetWidgetScale(GizmoScale);
		}
	});

	CommandBindings = MakeShareable(new FUICommandList);
	BindCommands();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this, &FControlRigEditMode::OnObjectsReplaced);
#endif
}

FControlRigEditMode::~FControlRigEditMode()
{	
	CommandBindings = nullptr;

	DestroyShapesActors(nullptr);
	OnControlRigAddedOrRemovedDelegate.Clear();
	OnControlRigSelectedDelegate.Clear();

	TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
	for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
	{
		if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
		{
			RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
		}
	}
	RuntimeControlRigs.Reset();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif

}

bool FControlRigEditMode::SetSequencer(TWeakPtr<ISequencer> InSequencer)
{
	if (InSequencer != WeakSequencer)
	{
		WeakSequencer = InSequencer;

		DetailKeyFrameCache->UnsetDelegates();

		DestroyShapesActors(nullptr);
		TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
		for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
		{
			if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
			{
				RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
			}
		}
		RuntimeControlRigs.Reset();
		if (InSequencer.IsValid())
		{
			TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
			if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Sequencer->GetFocusedMovieSceneSequence()))
			{
				TArray<FControlRigSequencerBindingProxy> Proxies = UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);
				for (FControlRigSequencerBindingProxy& Proxy : Proxies)
				{
					if (UControlRig* ControlRig = Proxy.ControlRig.Get())
					{
						AddControlRigInternal(ControlRig);
					}
				}
			}
			LastMovieSceneSig = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetSignature();
			DetailKeyFrameCache->SetDelegates(WeakSequencer, this);
			ControlProxy->SetSequencer(WeakSequencer);
		}
		SetObjects_Internal();
		if (FControlRigEditModeToolkit::Details.IsValid())
		{
			FControlRigEditModeToolkit::Details->SetEditMode(*this);
		}
		if (FControlRigEditModeToolkit::Outliner.IsValid())
		{
			FControlRigEditModeToolkit::Outliner->SetEditMode(*this);
		}
	}
	return false;
}

bool FControlRigEditMode::AddControlRigObject(UControlRig* ControlRig, TWeakPtr<ISequencer> InSequencer)
{
	if (ControlRig)
	{
		if (RuntimeControlRigs.Contains(ControlRig) == false)
		{
			if (InSequencer.IsValid())
			{
				if (SetSequencer(InSequencer) == false) //was already there so just add it,otherwise this function will add everything in the active 
				{
					AddControlRigInternal(ControlRig);
					SetObjects_Internal();
				}
				return true;
			}		
		}
	}
	return false;
}

void FControlRigEditMode::SetObjects(UControlRig* ControlRig,  UObject* BindingObject, TWeakPtr<ISequencer> InSequencer)
{
	TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
	for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
	{
		if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
		{
			RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
		}
	}
	RuntimeControlRigs.Reset();

	if (InSequencer.IsValid())
	{
		WeakSequencer = InSequencer;
	
	}
	// if we get binding object, set it to control rig binding object
	if (BindingObject && ControlRig)
	{
		if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
		{
			if (ObjectBinding->GetBoundObject() == nullptr)
			{
				ObjectBinding->BindToObject(BindingObject);
			}
		}

		AddControlRigInternal(ControlRig);
	}
	else if (ControlRig)
	{
		AddControlRigInternal(ControlRig);
	}

	SetObjects_Internal();
}

bool FControlRigEditMode::IsInLevelEditor() const
{
	return GetModeManager() == &GLevelEditorModeTools();
}

void FControlRigEditMode::SetUpDetailPanel()
{
	if (!AreEditingControlRigDirectly() && Toolkit)
	{
		StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetSequencer(WeakSequencer.Pin());
		StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetSettingsDetailsObject(GetMutableDefault<UControlRigEditModeSettings>());	
	}
}

void FControlRigEditMode::SetObjects_Internal()
{
	bool bHasValidRuntimeControlRig = false;
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* RuntimeControlRig = RuntimeRigPtr.Get())
		{
			RuntimeControlRig->ControlModified().RemoveAll(this);
			RuntimeControlRig->GetHierarchy()->OnModified().RemoveAll(this);

			RuntimeControlRig->ControlModified().AddSP(this, &FControlRigEditMode::OnControlModified);
			RuntimeControlRig->GetHierarchy()->OnModified().AddSP(this, &FControlRigEditMode::OnHierarchyModified_AnyThread);
			if (USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(GetHostingSceneComponent(RuntimeControlRig)))
			{
				TStrongObjectPtr<UControlRigEditModeDelegateHelper>* DelegateHelper = DelegateHelpers.Find(RuntimeControlRig);
				if (!DelegateHelper)
				{
					DelegateHelpers.Add(RuntimeControlRig, TStrongObjectPtr<UControlRigEditModeDelegateHelper>(NewObject<UControlRigEditModeDelegateHelper>()));
					DelegateHelper = DelegateHelpers.Find(RuntimeControlRig);
				}
				else if (DelegateHelper->IsValid() == false)
				{
					DelegateHelper->Get()->RemoveDelegates();
					DelegateHelpers.Remove(RuntimeControlRig);
					*DelegateHelper = TStrongObjectPtr<UControlRigEditModeDelegateHelper>(NewObject<UControlRigEditModeDelegateHelper>());
					DelegateHelper->Get()->EditMode = this;
					DelegateHelper->Get()->AddDelegates(MeshComponent);
					DelegateHelpers.Add(RuntimeControlRig, *DelegateHelper);
				}
				
				if (DelegateHelper && DelegateHelper->IsValid())
				{
					bHasValidRuntimeControlRig = true;
				}
			}
		}
	}

	if (UsesToolkits() && Toolkit.IsValid())
	{
		StaticCastSharedPtr<SControlRigEditModeTools>(Toolkit->GetInlineContent())->SetControlRigs(RuntimeControlRigs);
	}

	if (!bHasValidRuntimeControlRig)
	{
		DestroyShapesActors(nullptr);
		SetUpDetailPanel();
	}
	else
	{
		// create default manipulation layer
		RequestToRecreateControlShapeActors(nullptr);
	}
}

bool FControlRigEditMode::UsesToolkits() const
{
	return true;
}

void FControlRigEditMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();
	LastMovieSceneSig = FGuid();
	if (UsesToolkits())
	{
		if (!AreEditingControlRigDirectly())
		{
			if (WeakSequencer.IsValid() == false)
			{
				SetSequencer(FBakingHelper::GetSequencer());
			}
		}
		if (!Toolkit.IsValid())
		{
			Toolkit = MakeShareable(new FControlRigEditModeToolkit(*this));
			Toolkit->Init(Owner->GetToolkitHost());
		}

		FEditorModeTools* ModeManager = GetModeManager();

		bIsChangingCoordSystem = false;
		if (CoordSystemPerWidgetMode.Num() < (UE::Widget::WM_Max))
		{
			CoordSystemPerWidgetMode.SetNum(UE::Widget::WM_Max);
			ECoordSystem CoordSystem = ModeManager->GetCoordSystem();
			for (int32 i = 0; i < UE::Widget::WM_Max; ++i)
			{
				CoordSystemPerWidgetMode[i] = CoordSystem;
			}
		}

		ModeManager->OnWidgetModeChanged().AddSP(this, &FControlRigEditMode::OnWidgetModeChanged);
		ModeManager->OnCoordSystemChanged().AddSP(this, &FControlRigEditMode::OnCoordSystemChanged);
	}
	WorldPtr = GetWorld();
	OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddSP(this, &FControlRigEditMode::OnWorldCleanup);
	SetObjects_Internal();

	//set up gizmo scale to what we had last and save what it was.
	PreviousGizmoScale = GetModeManager()->GetWidgetScale();
	if (const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>())
	{
		GetModeManager()->SetWidgetScale(Settings->GizmoScale);
	}
}

//todo get working with Persona
static void ClearOutAnyActiveTools()
{
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin();

		if (LevelEditorPtr.IsValid())
		{
			FString ActiveToolName = LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->GetActiveToolName(EToolSide::Left);
			if (ActiveToolName == TEXT("SequencerPivotTool"))
			{
				LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Completed);
			}
		}
	}
}

void FControlRigEditMode::Exit()
{
	ClearOutAnyActiveTools();
	OnControlRigAddedOrRemovedDelegate.Clear();
	OnControlRigSelectedDelegate.Clear();
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			ControlRig->ClearControlSelection();
		}
	}

	if (InteractionScopes.Num() >0)
	{

		if (GEditor)
		{
			GEditor->EndTransaction();
		}

		for (TPair<UControlRig*,FControlRigInteractionScope*>& InteractionScope : InteractionScopes)
		{
			if (InteractionScope.Value)
			{
				delete InteractionScope.Value;
			}
		}
		InteractionScopes.Reset();
		bManipulatorMadeChange = false;
	}

	if (FControlRigEditModeToolkit::Details.IsValid())
	{
		FControlRigEditModeToolkit::Details.Reset();
	}
	if (FControlRigEditModeToolkit::Outliner.IsValid())
	{
		FControlRigEditModeToolkit::Outliner.Reset();
	}
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	}

	DestroyShapesActors(nullptr);


	TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
	for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
	{
		if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
		{
			RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
		}
	}
	RuntimeControlRigs.Reset();

	//clear delegates
	FEditorModeTools* ModeManager = GetModeManager();
	ModeManager->OnWidgetModeChanged().RemoveAll(this);
	ModeManager->OnCoordSystemChanged().RemoveAll(this);

	//clear proxies
	ControlProxy->RemoveAllProxies();

	//make sure the widget is reset
	ResetControlShapeSize();

	// Call parent implementation
	FEdMode::Exit();
}

void FControlRigEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
	
	//if we have don't have a viewport client or viewport, bail we can be in UMG for example
	if (ViewportClient == nullptr || ViewportClient->Viewport == nullptr)
	{
		return;
	}
	CheckMovieSceneSig();

	if (bool* GameView = ViewportToGameView.Find(ViewportClient->Viewport))
	{
		*GameView = ViewportClient->IsInGameView();
	} 
	else
	{
		ViewportToGameView.Add(ViewportClient->Viewport, ViewportClient->IsInGameView());
	}

	if(DeferredItemsToFrame.Num() > 0)
	{
		TGuardValue<FEditorViewportClient*> ViewportGuard(CurrentViewportClient, ViewportClient);
		FrameItems(DeferredItemsToFrame);
		DeferredItemsToFrame.Reset();
	}

	if (bSelectionChanged)
	{
		SetUpDetailPanel();
		HandleSelectionChanged();
		bSelectionChanged = false;
	}
	else
	{
		// HandleSelectionChanged() will already update the pivots 
		UpdatePivotTransforms();
	}
	
	if (!AreEditingControlRigDirectly() == false)
	{
		ViewportClient->Invalidate();
	}

	// check if the settings for xray rendering are different for any of the control shape actors
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	if(bShowControlsAsOverlay != Settings->bShowControlsAsOverlay)
	{
		bShowControlsAsOverlay = Settings->bShowControlsAsOverlay;
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* RuntimeControlRig = RuntimeRigPtr.Get())
			{
				UpdateSelectabilityOnSkeletalMeshes(RuntimeControlRig, !bShowControlsAsOverlay);
			}
		}
		RequestToRecreateControlShapeActors();
	}

	// Defer creation of shapes if manipulating the viewport
	if (RecreateControlShapesRequired != ERecreateControlRigShape::RecreateNone && !(FSlateApplication::Get().HasAnyMouseCaptor() || GUnrealEd->IsUserInteracting()))
	{
		RecreateControlShapeActors();
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* ControlRig = RuntimeRigPtr.Get())
			{
				TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
				for (const FRigElementKey& SelectedKey : SelectedRigElements)
				{
					if (SelectedKey.Type == ERigElementType::Control)
					{
						AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig,SelectedKey.Name);
						if (ShapeActor)
						{
							ShapeActor->SetSelected(true);
						}

						if (!AreEditingControlRigDirectly())
						{
							FRigControlElement* ControlElement = ControlRig->FindControl(SelectedKey.Name);
							if (ControlElement)
							{
								if (!ControlRig->IsCurveControl(ControlElement))
								{
									ControlProxy->AddProxy(ControlRig, ControlElement);
								}
							}
						}
					}
				}
			}
		}
		SetUpDetailPanel();
		HandleSelectionChanged();
		RecreateControlShapesRequired = ERecreateControlRigShape::RecreateNone;
		ControlRigsToRecreate.SetNum(0);
	}

	{
		// We need to tick here since changing a bone for example
		// might have changed the transform of the Control
		PostPoseUpdate();

		if (!AreEditingControlRigDirectly() == false) //only do this check if not in level editor
		{
			for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
			{
				if (UControlRig* ControlRig = RuntimeRigPtr.Get())
				{
					TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
					UE::Widget::EWidgetMode CurrentWidgetMode = ViewportClient->GetWidgetMode();
					if(!RequestedWidgetModes.IsEmpty())
					{
						if(RequestedWidgetModes.Last() != CurrentWidgetMode)
						{
							CurrentWidgetMode = RequestedWidgetModes.Last();
							ViewportClient->SetWidgetMode(CurrentWidgetMode);
						}
						RequestedWidgetModes.Reset();
					}
					for (FRigElementKey SelectedRigElement : SelectedRigElements)
					{
						//need to loop through the shape actors and set widget based upon the first one
						if (AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig, SelectedRigElement.Name))
						{
							if (!ModeSupportedByShapeActor(ShapeActor, CurrentWidgetMode))
							{
								if (FRigControlElement* ControlElement = ControlRig->FindControl(SelectedRigElement.Name))
								{
									switch (ControlElement->Settings.ControlType)
									{
									case ERigControlType::Float:
									case ERigControlType::Integer:
									case ERigControlType::Vector2D:
									case ERigControlType::Position:
									case ERigControlType::Transform:
									case ERigControlType::TransformNoScale:
									case ERigControlType::EulerTransform:
									{
										ViewportClient->SetWidgetMode(UE::Widget::WM_Translate);
										break;
									}
									case ERigControlType::Rotator:
									{
										ViewportClient->SetWidgetMode(UE::Widget::WM_Rotate);
										break;
									}
									case ERigControlType::Scale:
									case ERigControlType::ScaleFloat:
									{
										ViewportClient->SetWidgetMode(UE::Widget::WM_Scale);
										break;
									}
									}
									return; //exit if we switchted
								}
							}
						}
					}
				}
			}
		}
	}
	DetailKeyFrameCache->UpdateIfDirty();
}

//Hit proxy for FK Rigs and bones.
struct  HFKRigBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()

	FName BoneName;
	UControlRig* ControlRig;

	HFKRigBoneProxy()
		: HHitProxy(HPP_Foreground)
		, BoneName(NAME_None)
		, ControlRig(nullptr)
	{}

	HFKRigBoneProxy(FName InBoneName, UControlRig *InControlRig)
		: HHitProxy(HPP_Foreground)
		, BoneName(InBoneName)
		, ControlRig(InControlRig)
	{
	}

	// HHitProxy interface
	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::Crosshairs; }
	// End of HHitProxy interface
};

IMPLEMENT_HIT_PROXY(HFKRigBoneProxy, HHitProxy)


TSet<FName> FControlRigEditMode::GetActiveControlsFromSequencer(UControlRig* ControlRig)
{
	TSet<FName> ActiveControls;
	if (WeakSequencer.IsValid() == false)
	{
		return ActiveControls;
	}
	if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
	{
		USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
		if (!Component)
		{
			return ActiveControls;
		}
		const bool bCreateHandleIfMissing = false;
		FName CreatedFolderName = NAME_None;
		FGuid ObjectHandle = WeakSequencer.Pin()->GetHandleToObject(Component, bCreateHandleIfMissing);
		if (!ObjectHandle.IsValid())
		{
			UObject* ActorObject = Component->GetOwner();
			ObjectHandle = WeakSequencer.Pin()->GetHandleToObject(ActorObject, bCreateHandleIfMissing);
			if (!ObjectHandle.IsValid())
			{
				return ActiveControls;
			}
		}
		bool bCreateTrack = false;
		UMovieScene* MovieScene = WeakSequencer.Pin()->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (!MovieScene)
		{
			return ActiveControls;
		}
		if (FMovieSceneBinding* Binding = MovieScene->FindBinding(ObjectHandle))
		{
			for (UMovieSceneTrack* Track : Binding->GetTracks())
			{
				if (UMovieSceneControlRigParameterTrack* ControlRigParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
				{
					if (ControlRigParameterTrack->GetControlRig() == ControlRig)
					{
						UMovieSceneControlRigParameterSection* ActiveSection = Cast<UMovieSceneControlRigParameterSection>(ControlRigParameterTrack->GetSectionToKey());
						if (ActiveSection)
						{
							TArray<FRigControlElement*> Controls;
							ControlRig->GetControlsInOrder(Controls);
							TArray<bool> Mask = ActiveSection->GetControlsMask();

							TArray<FName> Names;
							int Index = 0;
							for (FRigControlElement* ControlElement : Controls)
							{
								if (Mask[Index])
								{
									ActiveControls.Add(ControlElement->GetFName());
								}
								++Index;
							}
						}
					}
				}
			}
		}
	}
	return ActiveControls;
}


void FControlRigEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	DragToolHandler.Render3DDragTool(View, PDI);

	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	const bool bIsInGameView = !AreEditingControlRigDirectly() ? (ViewportToGameView.Find(Viewport) && ViewportToGameView[Viewport]) : false;
	bool bRender = !Settings->bHideControlShapes;
	for (TWeakObjectPtr<UControlRig>& ControlRigPtr : RuntimeControlRigs)
	{
		UControlRig* ControlRig = ControlRigPtr.Get();
		//actor game view drawing is handled by not drawing in game via SetActorHiddenInGame().
		if (bRender && ControlRig && ControlRig->GetControlsVisible())
		{
			FTransform ComponentTransform = FTransform::Identity;
			if (!AreEditingControlRigDirectly())
			{
				ComponentTransform = GetHostingSceneComponentTransform(ControlRig);
			}
			/*
			const TArray<AControlRigShapeActor*>* ShapeActors = ControlRigShapeActors.Find(ControlRig);
			if (ShapeActors)
			{
				for (AControlRigShapeActor* Actor : *ShapeActors)
				{
					if (GIsEditor && Actor->GetWorld() != nullptr && !Actor->GetWorld()->IsPlayInEditor())
					{
						Actor->SetIsTemporarilyHiddenInEditor(false);
					}
				}
			}
			*/
			
			//only draw stuff if not in game view
			if (!bIsInGameView)
			{
				URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
				const bool bHasFKRig = (ControlRig->IsA<UAdditiveControlRig>() || ControlRig->IsA<UFKControlRig>());
				if (Settings->bDisplayHierarchy || bHasFKRig)
				{
					const bool bBoolSetHitProxies = PDI && PDI->IsHitTesting() && bHasFKRig;
					TSet<FName> ActiveControlName;
					if (bHasFKRig)
					{
						ActiveControlName = GetActiveControlsFromSequencer(ControlRig);
					}
					Hierarchy->ForEach<FRigTransformElement>([PDI, Hierarchy, ComponentTransform, ControlRig, bHasFKRig, bBoolSetHitProxies, ActiveControlName](FRigTransformElement* TransformElement) -> bool
						{
							if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(TransformElement))
							{
								if(ControlElement->Settings.AnimationType != ERigControlAnimationType::AnimationControl)
								{
									return true;
								}
							}
						
							const FTransform Transform = Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentGlobal);

							FRigBaseElementParentArray Parents = Hierarchy->GetParents(TransformElement);
							for (FRigBaseElement* ParentElement : Parents)
							{
								if (FRigTransformElement* ParentTransformElement = Cast<FRigTransformElement>(ParentElement))
								{
									FLinearColor Color = FLinearColor::White;
									if (bHasFKRig)
									{
										FName ControlName = UFKControlRig::GetControlName(ParentTransformElement->GetFName(), ParentTransformElement->GetType());
										if (ActiveControlName.Num() > 0 && ActiveControlName.Contains(ControlName) == false)
										{
											continue;
										}
										if (ControlRig->IsControlSelected(ControlName))
										{
											Color = FLinearColor::Yellow;
										}
									}
									const FTransform ParentTransform = Hierarchy->GetTransform(ParentTransformElement, ERigTransformType::CurrentGlobal);
									const bool bHitTesting = bBoolSetHitProxies && (ParentTransformElement->GetType() == ERigElementType::Bone);
									if (PDI)
									{
										if (bHitTesting)
										{
											PDI->SetHitProxy(new HFKRigBoneProxy(ParentTransformElement->GetFName(), ControlRig));
										}
										PDI->DrawLine(ComponentTransform.TransformPosition(Transform.GetLocation()), ComponentTransform.TransformPosition(ParentTransform.GetLocation()), Color, SDPG_Foreground);
										if (bHitTesting)
										{
											PDI->SetHitProxy(nullptr);
										}
									}
								}
							}

							FLinearColor Color = FLinearColor::White;
							if (bHasFKRig)
							{
								FName ControlName = UFKControlRig::GetControlName(TransformElement->GetFName(), TransformElement->GetType());
								if (ActiveControlName.Num() > 0 && ActiveControlName.Contains(ControlName) == false)
								{
									return true;
								}
								if (ControlRig->IsControlSelected(ControlName))
								{
									Color = FLinearColor::Yellow;
								}
							}
							if (PDI)
							{
								const bool bHitTesting = PDI->IsHitTesting() && bBoolSetHitProxies && (TransformElement->GetType() == ERigElementType::Bone);
								if (bHitTesting)
								{
									PDI->SetHitProxy(new HFKRigBoneProxy(TransformElement->GetFName(), ControlRig));
								}
								PDI->DrawPoint(ComponentTransform.TransformPosition(Transform.GetLocation()), Color, 5.0f, SDPG_Foreground);

								if (bHitTesting)
								{
									PDI->SetHitProxy(nullptr);
								}
							}

							return true;
						});
				}

				EWorldType::Type WorldType = Viewport->GetClient()->GetWorld()->WorldType;
				const bool bIsAssetEditor = WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview;
				if (bIsAssetEditor && (Settings->bDisplayNulls || ControlRig->IsConstructionModeEnabled()))
				{
					TArray<FTransform> SpaceTransforms;
					TArray<FTransform> SelectedSpaceTransforms;
					Hierarchy->ForEach<FRigNullElement>([&SpaceTransforms, &SelectedSpaceTransforms, Hierarchy](FRigNullElement* NullElement) -> bool
						{
							if (Hierarchy->IsSelected(NullElement->GetIndex()))
							{
								SelectedSpaceTransforms.Add(Hierarchy->GetTransform(NullElement, ERigTransformType::CurrentGlobal));
							}
							else
							{
								SpaceTransforms.Add(Hierarchy->GetTransform(NullElement, ERigTransformType::CurrentGlobal));
							}
							return true;
						});

					ControlRig->DrawInterface.DrawAxes(FTransform::Identity, SpaceTransforms, Settings->AxisScale);
					ControlRig->DrawInterface.DrawAxes(FTransform::Identity, SelectedSpaceTransforms, FLinearColor(1.0f, 0.34f, 0.0f, 1.0f), Settings->AxisScale);
				}

				if (bIsAssetEditor && (Settings->bDisplayAxesOnSelection && Settings->AxisScale > SMALL_NUMBER))
				{
					TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
					const float Scale = Settings->AxisScale;
					PDI->AddReserveLines(SDPG_Foreground, SelectedRigElements.Num() * 3);

					for (const FRigElementKey& SelectedElement : SelectedRigElements)
					{
						FTransform ElementTransform = Hierarchy->GetGlobalTransform(SelectedElement);
						ElementTransform = ElementTransform * ComponentTransform;

						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(Scale, 0.f, 0.f)), FLinearColor::Red, SDPG_Foreground);
						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(0.f, Scale, 0.f)), FLinearColor::Green, SDPG_Foreground);
						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(0.f, 0.f, Scale)), FLinearColor::Blue, SDPG_Foreground);
					}
				}

				// temporary implementation to draw sockets in 3D
				if (bIsAssetEditor && (Settings->bDisplaySockets || ControlRig->IsConstructionModeEnabled()) && Settings->AxisScale > SMALL_NUMBER)
				{
					const float Scale = Settings->AxisScale;
					PDI->AddReserveLines(SDPG_Foreground, Hierarchy->Num(ERigElementType::Socket) * 3);
					static const FLinearColor SocketColor = FControlRigEditorStyle::Get().SocketUserInterfaceColor;

					Hierarchy->ForEach<FRigSocketElement>([this, Hierarchy, PDI, ComponentTransform, Scale](FRigSocketElement* Socket)
					{
						FTransform ElementTransform = Hierarchy->GetGlobalTransform(Socket->GetIndex());
						ElementTransform = ElementTransform * ComponentTransform;

						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(Scale, 0.f, 0.f)), SocketColor, SDPG_Foreground);
						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(0.f, Scale, 0.f)), SocketColor, SDPG_Foreground);
						PDI->DrawLine(ElementTransform.GetTranslation(), ElementTransform.TransformPosition(FVector(0.f, 0.f, Scale)), SocketColor, SDPG_Foreground);

						return true;
					});
				}
				ControlRig->DrawIntoPDI(PDI, ComponentTransform);
			}
		}
	}
}

void FControlRigEditMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	IPersonaEditMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
	DragToolHandler.RenderDragTool(View, Canvas);
}

bool FControlRigEditMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	if (InEvent != IE_Released)
	{
		TGuardValue<FEditorViewportClient*> ViewportGuard(CurrentViewportClient, InViewportClient);

		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		if (CommandBindings->ProcessCommandBindings(InKey, KeyState, (InEvent == IE_Repeat)))
		{
			return true;
		}
		if (IsDragAnimSliderToolPressed(InViewport)) //this is needed to make sure we get all of the processed mouse events, for some reason the above may not return true
		{
			return true;
		}
	}

	return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);
}

bool FControlRigEditMode::ProcessCapturedMouseMoves(FEditorViewportClient* InViewportClient, FViewport* InViewport, const TArrayView<FIntPoint>& CapturedMouseMoves)
{
	const bool bChange = IsDragAnimSliderToolPressed(InViewport);
	if (CapturedMouseMoves.Num() > 0)
	{
		if (bChange)
		{
			for (int32 Index = 0; Index < CapturedMouseMoves.Num(); ++Index)
			{
				int32 X = CapturedMouseMoves[Index].X;
				if (StartXValue.IsSet() == false)
				{
					StartAnimSliderTool(X);
				}
				else
				{
					int32 Diff = X - StartXValue.GetValue();
					if (Diff != 0)
					{
						FIntPoint Origin, Size;
						InViewportClient->GetViewportDimensions(Origin, Size);
						const double ViewPortSize = (double)Size.X * 0.125; // 1/8 screen drag should do full blend, todo perhaps expose this as a sensitivity
						const double ViewDiff = (double)(Diff) / ViewPortSize;
						DragAnimSliderTool(ViewDiff);
					}
				}
			}
		}
		else if (bisTrackingAnimToolDrag && bChange == false)
		{
			ResetAnimSlider();
		}
		return bisTrackingAnimToolDrag;
	}
	else if(bisTrackingAnimToolDrag && bChange == false)
	{
		ResetAnimSlider();
	}
	return false;
}

bool FControlRigEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if(RuntimeControlRigs.IsEmpty())
	{
		return false;
	}
	
	//break right out if doing anim slider scrub

	if (IsDragAnimSliderToolPressed(InViewport))
	{
		return true;
	}

	if (IsMovingCamera(InViewport))
	{
		InViewportClient->SetCurrentWidgetAxis(EAxisList::None);
		return true;
	}
	if (IsDoingDrag(InViewport))
	{
		DragToolHandler.MakeDragTool(InViewportClient);
		return DragToolHandler.StartTracking(InViewportClient, InViewport);
	}

	return HandleBeginTransform(InViewportClient);
}

bool FControlRigEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if(RuntimeControlRigs.IsEmpty())
	{
		return false;
	}

	if (bisTrackingAnimToolDrag)
	{
		ResetAnimSlider();
	}
	if (IsDragAnimSliderToolPressed(InViewport))
	{
		return true;
	}

	if (IsMovingCamera(InViewport))
	{
		return true;
	}
	if (DragToolHandler.EndTracking(InViewportClient, InViewport))
	{
		return true;
	}

	return HandleEndTransform(InViewportClient);
}

bool FControlRigEditMode::BeginTransform(const FGizmoState& InState)
{
	return HandleBeginTransform(Owner->GetFocusedViewportClient());
}

bool FControlRigEditMode::EndTransform(const FGizmoState& InState)
{
	return HandleEndTransform(Owner->GetFocusedViewportClient());
}

bool FControlRigEditMode::HandleBeginTransform(const FEditorViewportClient* InViewportClient)
{
	if (!InViewportClient)
	{
		return false;
	}
	
	InteractionType = GetInteractionType(InViewportClient);
	bIsTracking = true;
	
	if (InteractionScopes.Num() == 0)
	{
		const bool bShouldModify = [this]() -> bool
		{
			if (AreEditingControlRigDirectly())
			{
				for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
				{
					if (UControlRig* ControlRig = RuntimeRigPtr.Get())
					{
						TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
						for (const FRigElementKey& Key : SelectedRigElements)
						{
							if (Key.Type != ERigElementType::Control)
							{
								return true;
							}
						}
					}
				}
			}
			
			return !AreEditingControlRigDirectly();
		}();

		if (AreEditingControlRigDirectly())
		{
			for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
			{
				if (UControlRig* ControlRig = RuntimeRigPtr.Get())
				{
					UObject* Blueprint = ControlRig->GetClass()->ClassGeneratedBy;
					if (Blueprint)
					{
						Blueprint->SetFlags(RF_Transactional);
						if (bShouldModify)
						{
							Blueprint->Modify();
						}
					}
					ControlRig->SetFlags(RF_Transactional);
					if (bShouldModify)
					{
						ControlRig->Modify();
					}
				}
			}
		}

	}

	//in level editor only transact if we have at least one control selected, in editor we only select CR stuff so always transact

	if (!AreEditingControlRigDirectly())
	{
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* ControlRig = RuntimeRigPtr.Get())
			{
				if (AreRigElementSelectedAndMovable(ControlRig))
				{
					//todo need to add multiple
					FControlRigInteractionScope* InteractionScope = new FControlRigInteractionScope(ControlRig);
					InteractionScopes.Add(ControlRig,InteractionScope);
					ControlRig->bInteractionJustBegan = true;
				}
				else
				{
					bManipulatorMadeChange = false;
				}
			}
		}
	}
	else if(UControlRigEditorSettings::Get()->bEnableUndoForPoseInteraction)
	{
		UControlRig* ControlRig = RuntimeControlRigs[0].Get();
		FControlRigInteractionScope* InteractionScope = new FControlRigInteractionScope(ControlRig);
		InteractionScopes.Add(ControlRig,InteractionScope);
	}
	else
	{
		bManipulatorMadeChange = false;
	}
	return InteractionScopes.Num() != 0;
}

bool FControlRigEditMode::HandleEndTransform(FEditorViewportClient* InViewportClient)
{
	if (!InViewportClient)
	{
		return false;
	}
	
	const bool bWasInteracting = bManipulatorMadeChange && InteractionType != (uint8)EControlRigInteractionType::None;
	
	InteractionType = (uint8)EControlRigInteractionType::None;
	bIsTracking = false;
	
	if (InteractionScopes.Num() > 0)
	{		
		if (bManipulatorMadeChange)
		{
			bManipulatorMadeChange = false;
			GEditor->EndTransaction();
		}

		for (TPair<UControlRig*, FControlRigInteractionScope*>& InteractionScope : InteractionScopes)
		{
			if (InteractionScope.Value)
			{
				delete InteractionScope.Value; 
			}
		}
		InteractionScopes.Reset();

		if (bWasInteracting && !AreEditingControlRigDirectly())
		{
			// We invalidate the hit proxies when in level editor to ensure that the gizmo's hit proxy is up to date.
			// The invalidation is called here to avoid useless viewport update in the FControlRigEditMode::Tick
			// function (that does an update when not in level editor)
			TickManipulatableObjects(0.f);
			
			static constexpr bool bInvalidateChildViews = false;
			static constexpr bool bInvalidateHitProxies = true;
			InViewportClient->Invalidate(bInvalidateChildViews, bInvalidateHitProxies);
		}
		
		return true;
	}

	bManipulatorMadeChange = false;
	
	return false;
}

bool FControlRigEditMode::UsesTransformWidget() const
{
	for (const auto& Pairs : ControlRigShapeActors)
	{
		for (const AControlRigShapeActor* ShapeActor : Pairs.Value)
		{
			if (ShapeActor->IsSelected())
			{
				return true;
			}
		}
		if (AreRigElementSelectedAndMovable(Pairs.Key))
		{
			return true;
		}
	}
	return FEdMode::UsesTransformWidget();
}

bool FControlRigEditMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	for (const auto& Pairs : ControlRigShapeActors)
	{
		for (const AControlRigShapeActor* ShapeActor : Pairs.Value)
		{
			if (ShapeActor->IsSelected())
			{
				return ModeSupportedByShapeActor(ShapeActor, CheckMode);
			}
		}
		if (AreRigElementSelectedAndMovable(Pairs.Key))
		{
			return true;
		}
	}
	return FEdMode::UsesTransformWidget(CheckMode);
}

FVector FControlRigEditMode::GetWidgetLocation() const
{
	FVector PivotLocation(0.0, 0.0, 0.0);
	int NumSelected = 0;
	for (const auto& Pairs : ControlRigShapeActors)
	{
		if (AreRigElementSelectedAndMovable(Pairs.Key))
		{
			if (const FTransform* PivotTransform = PivotTransforms.Find(Pairs.Key))
			{
				// check that the cached pivot is up-to-date and update it if needed
				FTransform Transform = *PivotTransform;
				UpdatePivotTransformsIfNeeded(Pairs.Key, Transform);
				const FTransform ComponentTransform = GetHostingSceneComponentTransform(Pairs.Key);
				PivotLocation += ComponentTransform.TransformPosition(Transform.GetLocation());
				++NumSelected;
			}
		}
	}	
	if (NumSelected > 0)
	{
		PivotLocation /= (NumSelected);
		return PivotLocation;
	}

	return FEdMode::GetWidgetLocation();
}

bool FControlRigEditMode::GetPivotForOrbit(FVector& OutPivot) const
{
	if (UsesTransformWidget())
	{
		OutPivot = GetWidgetLocation();
		return true;
	}
	return FEdMode::GetPivotForOrbit(OutPivot);
}

bool FControlRigEditMode::GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	//since we strip translation just want the first one
	for (const auto& Pairs : ControlRigShapeActors)
	{
		if (AreRigElementSelectedAndMovable(Pairs.Key))
		{
			if (const FTransform* PivotTransform = PivotTransforms.Find(Pairs.Key))
			{
				// check that the cached pivot is up-to-date and update it if needed
				FTransform Transform = *PivotTransform;
				UpdatePivotTransformsIfNeeded(Pairs.Key, Transform);
				OutMatrix = Transform.ToMatrixNoScale().RemoveTranslation();
				return true;
			}
		}
	}
	return false;
}

bool FControlRigEditMode::GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(OutMatrix, InData);
}

bool FControlRigEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	const bool bClickSelectThroughGizmo = CVarClickSelectThroughGizmo.GetValueOnGameThread();
	//if Control is down we act like we are selecting an axis so don't do this check
	//if doing control else we can't do control selection anymore, see FMouseDeltaTracker::DetermineCurrentAxis(
	if (Click.IsControlDown() == false && bClickSelectThroughGizmo == false)
	{
		const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
		//if we are hitting a widget, besides arcball then bail saying we are handling it
		if (CurrentAxis != EAxisList::None)
		{
			return true;
		}
	}

	InteractionType = GetInteractionType(InViewportClient);

	if(HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
	{
		if(ActorHitProxy->Actor)
		{
			if (ActorHitProxy->Actor->IsA<AControlRigShapeActor>())
			{
				AControlRigShapeActor* ShapeActor = CastChecked<AControlRigShapeActor>(ActorHitProxy->Actor);
				if (ShapeActor->IsSelectable() && ShapeActor->ControlRig.IsValid())
				{
					FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);

					// temporarily disable the interaction scope
					FControlRigInteractionScope** InteractionScope = InteractionScopes.Find(ShapeActor->ControlRig.Get());
					const bool bInteractionScopePresent = InteractionScope != nullptr;
					if (bInteractionScopePresent)
					{
						delete* InteractionScope;
						InteractionScopes.Remove(ShapeActor->ControlRig.Get());
					}
					
					const FName& ControlName = ShapeActor->ControlName;
					if (Click.IsShiftDown()) //guess we just select
					{
						SetRigElementSelection(ShapeActor->ControlRig.Get(),ERigElementType::Control, ControlName, true);
					}
					else if(Click.IsControlDown()) //if ctrl we toggle selection
					{
						if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
						{
							bool bIsSelected = ControlRig->IsControlSelected(ControlName);
							SetRigElementSelection(ControlRig, ERigElementType::Control, ControlName, !bIsSelected);
						}
					}
					else
					{
						//also need to clear actor selection. Sequencer will handle this automatically if done in Sequencder UI but not if done by clicking
						if (!AreEditingControlRigDirectly())
						{
							if (GEditor && GEditor->GetSelectedActorCount())
							{
								GEditor->SelectNone(false, true);
								GEditor->NoteSelectionChange();
							}
						}
						ClearRigElementSelection(ValidControlTypeMask());
						SetRigElementSelection(ShapeActor->ControlRig.Get(),ERigElementType::Control, ControlName, true);
					}

					if (bInteractionScopePresent)
					{
						*InteractionScope = new FControlRigInteractionScope(ShapeActor->ControlRig.Get());
						InteractionScopes.Add(ShapeActor->ControlRig.Get(), *InteractionScope);

					}

					// for now we show this menu all the time if body is selected
					// if we want some global menu, we'll have to move this
					if (Click.GetKey() == EKeys::RightMouseButton)
					{
						OpenContextMenu(InViewportClient);
					}
	
					return true;
				}

				return true;
			}
			else 
			{ 
				for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
				{
					if (UControlRig* ControlRig = RuntimeRigPtr.Get())
					{

						//if we have an additive or fk control rig active select the control based upon the selected bone.
						UAdditiveControlRig* AdditiveControlRig = Cast<UAdditiveControlRig>(ControlRig);
						UFKControlRig* FKControlRig = Cast<UFKControlRig>(ControlRig);

						if ((AdditiveControlRig || FKControlRig) && ControlRig->GetObjectBinding().IsValid())
						{
							if (USkeletalMeshComponent* RigMeshComp = Cast<USkeletalMeshComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
							{
								const USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(ActorHitProxy->PrimComponent);

								if (SkelComp == RigMeshComp)
								{
									FHitResult Result(1.0f);
									bool bHit = RigMeshComp->LineTraceComponent(Result, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * ControlRigSelectionConstants::BodyTraceDistance, FCollisionQueryParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(), true));

									if (bHit)
									{
										FName ControlName(*(Result.BoneName.ToString() + TEXT("_CONTROL")));
										if (ControlRig->FindControl(ControlName))
										{
											FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);

											if (Click.IsShiftDown()) //guess we just select
											{
												SetRigElementSelection(ControlRig,ERigElementType::Control, ControlName, true);
											}
											else if (Click.IsControlDown()) //if ctrl we toggle selection
											{
												bool bIsSelected = ControlRig->IsControlSelected(ControlName);
												SetRigElementSelection(ControlRig, ERigElementType::Control, ControlName, !bIsSelected);
											}
											else
											{
												ClearRigElementSelection(ValidControlTypeMask());
												SetRigElementSelection(ControlRig,ERigElementType::Control, ControlName, true);
											}
											return true;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else if (HFKRigBoneProxy* FKBoneProxy = HitProxyCast<HFKRigBoneProxy>(HitProxy))
	{
		FName ControlName(*(FKBoneProxy->BoneName.ToString() + TEXT("_CONTROL")));
		if (FKBoneProxy->ControlRig->FindControl(ControlName))
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);

			if (Click.IsShiftDown()) //guess we just select
			{
				SetRigElementSelection(FKBoneProxy->ControlRig,ERigElementType::Control, ControlName, true);
			}
			else if (Click.IsControlDown()) //if ctrl we toggle selection
			{
				for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
				{
					if (UControlRig* ControlRig = RuntimeRigPtr.Get())
					{
						bool bIsSelected = ControlRig->IsControlSelected(ControlName);
						SetRigElementSelection(FKBoneProxy->ControlRig, ERigElementType::Control, ControlName, !bIsSelected);
					}
				}
			}
			else
			{
				ClearRigElementSelection(ValidControlTypeMask());
				SetRigElementSelection(FKBoneProxy->ControlRig,ERigElementType::Control, ControlName, true);
			}
			return true;
		}
	}
	else if (HPersonaBoneHitProxy* BoneHitProxy = HitProxyCast<HPersonaBoneHitProxy>(HitProxy))
	{
		if (RuntimeControlRigs.Num() > 0)
		{
			if (UControlRig* DebuggedControlRig = RuntimeControlRigs[0].Get())
			{
				URigHierarchy* Hierarchy = DebuggedControlRig->GetHierarchy();

				// Cache mapping?
				for (int32 Index = 0; Index < Hierarchy->Num(); Index++)
				{
					const FRigElementKey ElementToSelect = Hierarchy->GetKey(Index);
					if (ElementToSelect.Type == ERigElementType::Bone && ElementToSelect.Name == BoneHitProxy->BoneName)
					{
						if (FSlateApplication::Get().GetModifierKeys().IsShiftDown())
						{
							Hierarchy->GetController()->SelectElement(ElementToSelect, true);
						}
						else if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
						{
							const bool bSelect = !Hierarchy->IsSelected(ElementToSelect);
							Hierarchy->GetController()->SelectElement(ElementToSelect, bSelect);
						}
						else
						{
							TArray<FRigElementKey> NewSelection;
							NewSelection.Add(ElementToSelect);
							Hierarchy->GetController()->SetSelection(NewSelection);
						}
						return true;
					}
				}
			}
		}
	}
	else
	{
		InteractionType = (uint8)EControlRigInteractionType::None;
	}

	// for now we show this menu all the time if body is selected
	// if we want some global menu, we'll have to move this
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		OpenContextMenu(InViewportClient);
		return true;
	}

	
	// clear selected controls
	if (Click.IsShiftDown() ==false && Click.IsControlDown() == false)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);
		ClearRigElementSelection(FRigElementTypeHelper::ToMask(ERigElementType::All));
	}

	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();

	if (Settings && Settings->bOnlySelectRigControls)
	{
		return true;
	}
	/*
	if(!InViewportClient->IsLevelEditorClient() && !InViewportClient->IsSimulateInEditorViewport())
	{
		bool bHandled = false;
		const bool bSelectingSections = GetAnimPreviewScene().AllowMeshHitProxies();

		USkeletalMeshComponent* MeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();

		if ( HitProxy )
		{
			if ( HitProxy->IsA( HPersonaBoneProxy::StaticGetType() ) )
			{			
				SetRigElementSelection(ERigElementType::Bone, static_cast<HPersonaBoneProxy*>(HitProxy)->BoneName, true);
				bHandled = true;
			}
		}
		
		if ( !bHandled && !bSelectingSections )
		{
			// Cast for phys bodies if we didn't get any hit proxies
			FHitResult Result(1.0f);
			UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
			bool bHit = PreviewMeshComponent->LineTraceComponent(Result, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 10000.0f, FCollisionQueryParams(NAME_None, FCollisionQueryParams::GetUnknownStatId(),true));
			
			if(bHit)
			{
				SetRigElementSelection(ERigElementType::Bone, Result.BoneName, true);
				bHandled = true;
			}
		}
	}
	*/
	
	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

void FControlRigEditMode::OpenContextMenu(FEditorViewportClient* InViewportClient)
{
	TSharedPtr<FUICommandList> Commands = CommandBindings;
	if (OnContextMenuCommandsDelegate.IsBound())
	{
		Commands = OnContextMenuCommandsDelegate.Execute();
	}

	if (OnGetContextMenuDelegate.IsBound())
	{
		TSharedPtr<SWidget> MenuWidget = SNullWidget::NullWidget;
		
		if (UToolMenu* ContextMenu = OnGetContextMenuDelegate.Execute())
		{
			UToolMenus* ToolMenus = UToolMenus::Get();
			MenuWidget = ToolMenus->GenerateWidget(ContextMenu);
		}

		TSharedPtr<SWidget> ParentWidget = InViewportClient->GetEditorViewportWidget();

		if (MenuWidget.IsValid() && ParentWidget.IsValid())
		{
			const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();

			FSlateApplication::Get().PushMenu(
				ParentWidget.ToSharedRef(),
				FWidgetPath(),
				MenuWidget.ToSharedRef(),
				MouseCursorLocation,
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
		}
	}
}

bool FControlRigEditMode::IntersectSelect(bool InSelect, const TFunctionRef<bool(const AControlRigShapeActor*, const FTransform&)>& Intersects)
{
	bool bSelected = false;

	for (auto& Pairs : ControlRigShapeActors)
	{
		FTransform ComponentTransform = GetHostingSceneComponentTransform(Pairs.Key);
		for (AControlRigShapeActor* ShapeActor : Pairs.Value)
		{
			if (ShapeActor->IsHiddenEd())
			{
				continue;
			}

			const FTransform ControlTransform = ShapeActor->GetGlobalTransform() * ComponentTransform;
			if (Intersects(ShapeActor, ControlTransform))
			{
				SetRigElementSelection(Pairs.Key,ERigElementType::Control, ShapeActor->ControlName, InSelect);
				bSelected = true;
			}
		}
	}

	return bSelected;
}

static FConvexVolume GetVolumeFromBox(const FBox& InBox)
{
	FConvexVolume ConvexVolume;
	ConvexVolume.Planes.Empty(6);

	ConvexVolume.Planes.Add(FPlane(FVector::LeftVector, -InBox.Min.Y));
	ConvexVolume.Planes.Add(FPlane(FVector::RightVector, InBox.Max.Y));
	ConvexVolume.Planes.Add(FPlane(FVector::UpVector, InBox.Max.Z));
	ConvexVolume.Planes.Add(FPlane(FVector::DownVector, -InBox.Min.Z));
	ConvexVolume.Planes.Add(FPlane(FVector::ForwardVector, InBox.Max.X));
	ConvexVolume.Planes.Add(FPlane(FVector::BackwardVector, -InBox.Min.X));

	ConvexVolume.Init();

	return ConvexVolume;
}

bool IntersectsBox( AActor& InActor, const FBox& InBox, FLevelEditorViewportClient* LevelViewportClient, bool bUseStrictSelection )
{
	bool bActorHitByBox = false;
	if (InActor.IsHiddenEd())
	{
		return false;
	}

	const TArray<FName>& HiddenLayers = LevelViewportClient->ViewHiddenLayers;
	bool bActorIsVisible = true;
	for ( auto Layer : InActor.Layers )
	{
		// Check the actor isn't in one of the layers hidden from this viewport.
		if( HiddenLayers.Contains( Layer ) )
		{
			return false;
		}
	}

	// Iterate over all actor components, selecting out primitive components
	for (UActorComponent* Component : InActor.GetComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (PrimitiveComponent && PrimitiveComponent->IsRegistered() && PrimitiveComponent->IsVisibleInEditor())
		{
			if (PrimitiveComponent->IsShown(LevelViewportClient->EngineShowFlags) && PrimitiveComponent->ComponentIsTouchingSelectionBox(InBox, false, bUseStrictSelection))
			{
				return true;
			}
		}
	}
	
	return false;
}

bool FControlRigEditMode::BoxSelect(FBox& InBox, bool InSelect)
{

	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	FLevelEditorViewportClient* LevelViewportClient = GCurrentLevelEditingViewportClient;
	if (LevelViewportClient->IsInGameView() == true || Settings->bHideControlShapes)
	{
		return  FEdMode::BoxSelect(InBox, InSelect);
	}
	const bool bStrictDragSelection = GetDefault<ULevelEditorViewportSettings>()->bStrictBoxSelection;

	FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);
	const bool bShiftDown = LevelViewportClient->Viewport->KeyState(EKeys::LeftShift) || LevelViewportClient->Viewport->KeyState(EKeys::RightShift);
	if (!bShiftDown)
	{
		ClearRigElementSelection(ValidControlTypeMask());
	}

	// Select all actors that are within the selection box area.  Be aware that certain modes do special processing below.	
	bool bSomethingSelected = false;
	UWorld* IteratorWorld = GWorld;
	for( FActorIterator It(IteratorWorld); It; ++It )
	{
		AActor* Actor = *It;

		if (!Actor->IsA<AControlRigShapeActor>())
		{
			continue;
		}

		AControlRigShapeActor* ShapeActor = CastChecked<AControlRigShapeActor>(Actor);
		if (!ShapeActor->IsSelectable() || ShapeActor->ControlRig.IsValid () == false || ShapeActor->ControlRig->GetControlsVisible() == false)
		{
			continue;
		}

		if (IntersectsBox(*Actor, InBox, LevelViewportClient, bStrictDragSelection))
		{
			bSomethingSelected = true;
			const FName& ControlName = ShapeActor->ControlName;
			SetRigElementSelection(ShapeActor->ControlRig.Get(),ERigElementType::Control, ControlName, true);

			if (bShiftDown)
			{
			}
			else
			{
				SetRigElementSelection(ShapeActor->ControlRig.Get(),ERigElementType::Control, ControlName, true);
			}
		}
	}
	if (bSomethingSelected == true)
	{
		return true;
	}	
	ScopedTransaction.Cancel();
	//if only selecting controls return true to stop any more selections
	if (Settings && Settings->bOnlySelectRigControls)
	{
		return true;
	}
	return FEdMode::BoxSelect(InBox, InSelect);
}

bool FControlRigEditMode::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect)
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	if (!Settings)
	{
		return false;
	}
	
	//need to check for a zero frustum since ComponentIsTouchingSelectionFrustum will return true, selecting everything, when this is the case
	const bool bMalformedFrustum = (InFrustum.Planes[0].IsNearlyZero() && InFrustum.Planes[2].IsNearlyZero()) || (InFrustum.Planes[3].IsNearlyZero() &&
		InFrustum.Planes[4].IsNearlyZero());
	if (bMalformedFrustum || InViewportClient->IsInGameView() == true || Settings->bHideControlShapes)
	{
		if (Settings->bOnlySelectRigControls)
		{
			return true;
		}
		else
		{
			return false;
		}
		return FEdMode::FrustumSelect(InFrustum, InViewportClient, InSelect);
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);
	bool bSomethingSelected(false);
	const bool bShiftDown = InViewportClient->Viewport->KeyState(EKeys::LeftShift) || InViewportClient->Viewport->KeyState(EKeys::RightShift);
	if (!bShiftDown)
	{
		ClearRigElementSelection(ValidControlTypeMask());
	}

	for (auto& Pairs : ControlRigShapeActors)
	{
		for (AControlRigShapeActor* ShapeActor : Pairs.Value)
		{
			for (UActorComponent* Component : ShapeActor->GetComponents())
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
				if (PrimitiveComponent && PrimitiveComponent->IsRegistered() && PrimitiveComponent->IsVisibleInEditor())
				{
					if (PrimitiveComponent->IsShown(InViewportClient->EngineShowFlags) && PrimitiveComponent->ComponentIsTouchingSelectionFrustum(InFrustum, false /*only bsp*/, false/*encompass entire*/))
					{
						if (ShapeActor->IsSelectable() && ShapeActor->ControlRig.IsValid() &&  ShapeActor->ControlRig->GetControlsVisible() )
						{
							bSomethingSelected = true;
							const FName& ControlName = ShapeActor->ControlName;
							SetRigElementSelection(Pairs.Key,ERigElementType::Control, ControlName, true);
						}
					}
				}
			}
		}
	}

	EWorldType::Type WorldType = InViewportClient->GetWorld()->WorldType;
	const bool bIsAssetEditor =
		(WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview) &&
			!InViewportClient->IsLevelEditorClient();

	if (bIsAssetEditor)
	{
		float BoneRadius = 1;
		EBoneDrawMode::Type BoneDrawMode = EBoneDrawMode::None;
		if (const FAnimationViewportClient* AnimViewportClient = static_cast<FAnimationViewportClient*>(InViewportClient))
		{
			BoneDrawMode = AnimViewportClient->GetBoneDrawMode();
			BoneRadius = AnimViewportClient->GetBoneDrawSize();
		}

		if(BoneDrawMode != EBoneDrawMode::None)
		{
			for(TWeakObjectPtr<UControlRig> WeakControlRig : RuntimeControlRigs)
			{
				if(UControlRig* ControlRig = WeakControlRig.Get())
				{
					if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
					{
						TArray<FRigBoneElement*> Bones = Hierarchy->GetBones();
						for(int32 Index = 0; Index < Bones.Num(); Index++)
						{
							const int32 BoneIndex = Bones[Index]->GetIndex();
							const TArray<int32> Children = Hierarchy->GetChildren(BoneIndex);

							const FVector Start = Hierarchy->GetGlobalTransform(BoneIndex).GetLocation();

							if(InFrustum.IntersectSphere(Start, 0.1f * BoneRadius))
							{
								bSomethingSelected = true;
								SetRigElementSelection(ControlRig, ERigElementType::Bone, Bones[Index]->GetFName(), true);
								continue;
							}

							bool bSelectedBone = false;
							for(int32 ChildIndex : Children)
							{
								if(Hierarchy->Get(ChildIndex)->GetType() != ERigElementType::Bone)
								{
									continue;
								}
								
								const FVector End = Hierarchy->GetGlobalTransform(ChildIndex).GetLocation();

								const float BoneLength = (End - Start).Size();
								const float Radius = FMath::Max(BoneLength * 0.05f, 0.1f) * BoneRadius;
								const int32 Steps = FMath::CeilToInt(BoneLength / (Radius * 1.5f) + 0.5);
								const FVector Step = (End - Start) / FVector::FReal(Steps - 1);

								// intersect segment-wise along the bone
								FVector Position = Start;
								for(int32 StepIndex = 0; StepIndex < Steps; StepIndex++)
								{
									if(InFrustum.IntersectSphere(Position, Radius))
									{
										bSomethingSelected = true;
										bSelectedBone = true;
										SetRigElementSelection(ControlRig, ERigElementType::Bone, Bones[Index]->GetFName(), true);
										break;
									}
									Position += Step;
								}

								if(bSelectedBone)
								{
									break;
								}
							}
						}
					}
				}
			}
		}

		for(TWeakObjectPtr<UControlRig> WeakControlRig : RuntimeControlRigs)
		{
			if(UControlRig* ControlRig = WeakControlRig.Get())
			{
				if (Settings->bDisplayNulls || ControlRig->IsConstructionModeEnabled())
				{
					if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
					{
						TArray<FRigNullElement*> Nulls = Hierarchy->GetNulls();
						for(int32 Index = 0; Index < Nulls.Num(); Index++)
						{
							const int32 NullIndex = Nulls[Index]->GetIndex();

							const FTransform Transform = Hierarchy->GetGlobalTransform(NullIndex);
							const FVector Origin = Transform.GetLocation();
							const float MaxScale = Transform.GetMaximumAxisScale();

							if(InFrustum.IntersectSphere(Origin, MaxScale * Settings->AxisScale))
							{
								bSomethingSelected = true;
								SetRigElementSelection(ControlRig, ERigElementType::Null, Nulls[Index]->GetFName(), true);
							}
						}
					}
				}
			}
		}
	}

	if (bSomethingSelected == true)
	{
		return true;
	}
	
	ScopedTransaction.Cancel();
	//if only selecting controls return true to stop any more selections
	if (Settings->bOnlySelectRigControls)
	{
		return true;
	}
	return FEdMode::FrustumSelect(InFrustum, InViewportClient, InSelect);
}

void FControlRigEditMode::SelectNone()
{
	ClearRigElementSelection(FRigElementTypeHelper::ToMask(ERigElementType::All));

	FEdMode::SelectNone();
}

bool FControlRigEditMode::IsMovingCamera(const FViewport* InViewport) const
{
	const bool LeftMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);
	const bool bIsAltKeyDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
	return LeftMouseButtonDown && bIsAltKeyDown;
}

bool FControlRigEditMode::IsDoingDrag(const FViewport* InViewport) const
{
	if(!UControlRigEditorSettings::Get()->bLeftMouseDragDoesMarquee)
	{
		return false;
	}

	if (Owner && Owner->GetInteractiveToolsContext()->InputRouter->HasActiveMouseCapture())
	{
		// don't start dragging if the ITF handled tracking event first   
		return false;
	}
	
	const bool LeftMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);
	const bool bIsCtrlKeyDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bIsAltKeyDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
	const EAxisList::Type CurrentAxis = GetCurrentWidgetAxis();
	
	//if shift is down we still want to drag
	return LeftMouseButtonDown && (CurrentAxis == EAxisList::None) && !bIsCtrlKeyDown && !bIsAltKeyDown;
}

bool FControlRigEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (IsDragAnimSliderToolPressed(InViewport)) //this is needed to make sure we get all of the processed mouse events, for some reason the above may not return true
	{
		//handled by processed mouse clicks
		return true;
	}

	if (IsDoingDrag(InViewport))
	{
		return DragToolHandler.InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
	}

	FVector Drag = InDrag;
	FRotator Rot = InRot;
	FVector Scale = InScale;

	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);

	//button down if left and ctrl and right is down, needed for indirect posting

	// enable MMB with the new TRS gizmos
	const bool bEnableMMB = UEditorInteractiveGizmoManager::UsesNewTRSGizmos();
	
	const bool bMouseButtonDown =
		InViewport->KeyState(EKeys::LeftMouseButton) ||
		(bCtrlDown && InViewport->KeyState(EKeys::RightMouseButton)) ||
		bEnableMMB;

	const UE::Widget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	const ECoordSystem CoordSystem = InViewportClient->GetWidgetCoordSystemSpace();

	const bool bDoRotation = !Rot.IsZero() && (WidgetMode == UE::Widget::WM_Rotate || WidgetMode == UE::Widget::WM_TranslateRotateZ);
	const bool bDoTranslation = !Drag.IsZero() && (WidgetMode == UE::Widget::WM_Translate || WidgetMode == UE::Widget::WM_TranslateRotateZ);
	const bool bDoScale = !Scale.IsZero() && WidgetMode == UE::Widget::WM_Scale;


	if (InteractionScopes.Num() > 0 && bMouseButtonDown && CurrentAxis != EAxisList::None
		&& (bDoRotation || bDoTranslation || bDoScale))
	{
		for (auto& Pairs : ControlRigShapeActors)
		{
			if (AreRigElementsSelected(ValidControlTypeMask(), Pairs.Key))
			{
				FTransform ComponentTransform = GetHostingSceneComponentTransform(Pairs.Key);

				if (bIsChangingControlShapeTransform)
				{
					for (AControlRigShapeActor* ShapeActor : Pairs.Value)
					{
						if (ShapeActor->IsSelected())
						{
							if (bManipulatorMadeChange == false)
							{
								GEditor->BeginTransaction(LOCTEXT("ChangeControlShapeTransaction", "Change Control Shape Transform"));
							}

							ChangeControlShapeTransform(ShapeActor, bDoTranslation, InDrag, bDoRotation, InRot, bDoScale, InScale, ComponentTransform);
							bManipulatorMadeChange = true;

							// break here since we only support changing shape transform of a single control at a time
							break;
						}
					}
				}
				else
				{
					const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
					bool bDoLocal = (CoordSystem == ECoordSystem::COORD_Local && Settings && Settings->bLocalTransformsInEachLocalSpace);
					bool bUseLocal = false;
					bool bCalcLocal = bDoLocal;
					bool bFirstTime = true;
					FTransform InOutLocal = FTransform::Identity;
					
					bool const bJustStartedManipulation = !bManipulatorMadeChange;
					bool bAnyAdditiveRig = false;
					for (AControlRigShapeActor* ShapeActor : Pairs.Value)
					{
						if (ShapeActor->ControlRig.IsValid())
						{
							if (ShapeActor->ControlRig->IsAdditive())
							{
								bAnyAdditiveRig = true;
								break;
							}
						}
					}

					for (AControlRigShapeActor* ShapeActor : Pairs.Value)
					{
						if (ShapeActor->IsEnabled() && ShapeActor->IsSelected())
						{
							// test local vs global
							if (bManipulatorMadeChange == false)
							{
								GEditor->BeginTransaction(LOCTEXT("MoveControlTransaction", "Move Control"));
							}

							// Cannot benefit of same local transform when applying to additive rigs
							if (!bAnyAdditiveRig)
							{
								if (bFirstTime)
								{
									bFirstTime = false;
								}
								else
								{
									if (bDoLocal)
									{
										bUseLocal = true;
										bDoLocal = false;
									}
								}
							}

							if(bJustStartedManipulation)
							{
								if(const FRigControlElement* ControlElement = Pairs.Key->FindControl(ShapeActor->ControlName))
								{
									ShapeActor->OffsetTransform = Pairs.Key->GetHierarchy()->GetGlobalControlOffsetTransform(ControlElement->GetKey(), false);
								}
							}

							MoveControlShape(ShapeActor, bDoTranslation, InDrag, bDoRotation, InRot, bDoScale, InScale, ComponentTransform,
								bUseLocal, bDoLocal, InOutLocal);
							bManipulatorMadeChange = true;
						}
					}
				}
			}
			else if (AreRigElementSelectedAndMovable(Pairs.Key))
			{
				FTransform ComponentTransform = GetHostingSceneComponentTransform(Pairs.Key);

				// set Bone transform
				// that will set initial Bone transform
				TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(Pairs.Key);

				for (int32 Index = 0; Index < SelectedRigElements.Num(); ++Index)
				{
					const ERigElementType SelectedRigElementType = SelectedRigElements[Index].Type;

					if (SelectedRigElementType == ERigElementType::Control)
					{
						FTransform NewWorldTransform = OnGetRigElementTransformDelegate.Execute(SelectedRigElements[Index], false, true) * ComponentTransform;
						bool bTransformChanged = false;
						if (bDoRotation)
						{
							FQuat CurrentRotation = NewWorldTransform.GetRotation();
							CurrentRotation = (Rot.Quaternion() * CurrentRotation);
							NewWorldTransform.SetRotation(CurrentRotation);
							bTransformChanged = true;
						}

						if (bDoTranslation)
						{
							FVector CurrentLocation = NewWorldTransform.GetLocation();
							CurrentLocation = CurrentLocation + Drag;
							NewWorldTransform.SetLocation(CurrentLocation);
							bTransformChanged = true;
						}

						if (bDoScale)
						{
							FVector CurrentScale = NewWorldTransform.GetScale3D();
							CurrentScale = CurrentScale + Scale;
							NewWorldTransform.SetScale3D(CurrentScale);
							bTransformChanged = true;
						}

						if (bTransformChanged)
						{
							if (bManipulatorMadeChange == false)
							{
								GEditor->BeginTransaction(LOCTEXT("MoveControlTransaction", "Move Control"));
							}
							FTransform NewComponentTransform = NewWorldTransform.GetRelativeTransform(ComponentTransform);
							OnSetRigElementTransformDelegate.Execute(SelectedRigElements[Index], NewComponentTransform, false);
							bManipulatorMadeChange = true;
						}
					}
				}
			}
		}
	}

	UpdatePivotTransforms();

	if (bManipulatorMadeChange)
	{
		TickManipulatableObjects(0.f);
	}
	//if in level editor we want to move other things also
	return IsInLevelEditor() ? false :bManipulatorMadeChange;
}

bool FControlRigEditMode::ShouldDrawWidget() const
{
	for (const TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (AreRigElementSelectedAndMovable(ControlRig))
			{
				return true;
			}
		}
	}
	return FEdMode::ShouldDrawWidget();
}

bool FControlRigEditMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return OtherModeID == FName(TEXT("EM_SequencerMode"), FNAME_Find) || OtherModeID == FName(TEXT("MotionTrailEditorMode"), FNAME_Find); /*|| OtherModeID == FName(TEXT("EditMode.ControlRigEditor"), FNAME_Find);*/
}

void FControlRigEditMode::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (auto& ShapeActors : ControlRigShapeActors)
	{
		for (auto& ShapeActor : ShapeActors.Value)
		{		
			Collector.AddReferencedObject(ShapeActor);
		}
	}
	if (ControlProxy)
	{
		Collector.AddReferencedObject(ControlProxy);
	}
}

void FControlRigEditMode::ClearRigElementSelection(uint32 InTypes)
{
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (!AreEditingControlRigDirectly())
			{
				if (URigHierarchyController* Controller = ControlRig->GetHierarchy()->GetController())
				{
					Controller->ClearSelection();
				}
			}
			else if (UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy))
			{
				Blueprint->GetHierarchyController()->ClearSelection();
			}
		}
	}
}

// internal private function that doesn't use guarding.
void FControlRigEditMode::SetRigElementSelectionInternal(UControlRig* ControlRig, ERigElementType Type, const FName& InRigElementName, bool bSelected)
{
	if(URigHierarchyController* Controller = ControlRig->GetHierarchy()->GetController())
	{
		Controller->SelectElement(FRigElementKey(InRigElementName, Type), bSelected);
	}
}

void FControlRigEditMode::SetRigElementSelection(UControlRig* ControlRig, ERigElementType Type, const FName& InRigElementName, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		SetRigElementSelectionInternal(ControlRig,Type, InRigElementName, bSelected);

		HandleSelectionChanged();
	}
}

void FControlRigEditMode::SetRigElementSelection(UControlRig* ControlRig, ERigElementType Type, const TArray<FName>& InRigElementNames, bool bSelected)
{
	if (!bSelecting)
	{
		TGuardValue<bool> ReentrantGuard(bSelecting, true);

		for (const FName& ElementName : InRigElementNames)
		{
			SetRigElementSelectionInternal(ControlRig, Type, ElementName, bSelected);
		}

		HandleSelectionChanged();
	}
}

TArray<FRigElementKey> FControlRigEditMode::GetSelectedRigElements(UControlRig* ControlRig) const
{
	if (ControlRig == nullptr && GetControlRigs().Num() > 0)
	{
		ControlRig = GetControlRigs()[0].Get();
	}

	TArray<FRigElementKey> SelectedKeys;

	if (ControlRig->GetHierarchy())
	{
		SelectedKeys = ControlRig->GetHierarchy()->GetSelectedKeys();
	}

	// currently only 1 transient control is allowed at a time
	// Transient Control's bSelected flag is never set to true, probably to avoid confusing other parts of the system
	// But since Edit Mode directly deals with transient controls, its selection status is given special treatment here.
	// So basically, whenever a bone is selected, and there is a transient control present, we consider both selected.
	if (SelectedKeys.Num() == 1)
	{
		if (SelectedKeys[0].Type == ERigElementType::Bone || SelectedKeys[0].Type == ERigElementType::Null)
		{
			const FName ControlName = UControlRig::GetNameForTransientControl(SelectedKeys[0]);
			const FRigElementKey TransientControlKey = FRigElementKey(ControlName, ERigElementType::Control);
			if(ControlRig->GetHierarchy()->Contains(TransientControlKey))
			{
				SelectedKeys.Add(TransientControlKey);
			}

		}
	}
	else
	{
		// check if there is a pin value transient control active
		// when a pin control is active, all existing selection should have been cleared
		TArray<FRigControlElement*> TransientControls = ControlRig->GetHierarchy()->GetTransientControls();

		if (TransientControls.Num() > 0)
		{
			if (ensure(SelectedKeys.Num() == 0))
			{
				SelectedKeys.Add(TransientControls[0]->GetKey());
			}
		}
	}
	return SelectedKeys;
}

bool FControlRigEditMode::AreRigElementsSelected(uint32 InTypes, UControlRig* InControlRig) const
{
	TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(InControlRig);

	for (const FRigElementKey& Ele : SelectedRigElements)
	{
		if (FRigElementTypeHelper::DoesHave(InTypes, Ele.Type))
		{
			return true;
		}
	}

	return false;
}

int32 FControlRigEditMode::GetNumSelectedRigElements(uint32 InTypes, UControlRig* InControlRig) const
{
	TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(InControlRig);
	if (FRigElementTypeHelper::DoesHave(InTypes, ERigElementType::All))
	{
		return SelectedRigElements.Num();
	}
	else
	{
		int32 NumSelected = 0;
		for (const FRigElementKey& Ele : SelectedRigElements)
		{
			if (FRigElementTypeHelper::DoesHave(InTypes, Ele.Type))
			{
				++NumSelected;
			}
		}

		return NumSelected;
	}

	return 0;
}

void FControlRigEditMode::RefreshObjects()
{
	SetObjects_Internal();
}

bool FControlRigEditMode::CanRemoveFromPreviewScene(const USceneComponent* InComponent)
{
	for (auto& ShapeActors : ControlRigShapeActors)
	{
		for (auto& ShapeActor : ShapeActors.Value)
		{
			TInlineComponentArray<USceneComponent*> SceneComponents;
			ShapeActor->GetComponents(SceneComponents, true);
			if (SceneComponents.Contains(InComponent))
			{
				return false;
			}
		}
	}

	// we don't need it 
	return true;
}

ECoordSystem FControlRigEditMode::GetCoordSystemSpace() const
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	if (Settings && Settings->bCoordSystemPerWidgetMode)
	{
		const int32 WidgetMode = static_cast<int32>(GetModeManager()->GetWidgetMode());
		if (CoordSystemPerWidgetMode.IsValidIndex(WidgetMode))
		{
			return CoordSystemPerWidgetMode[WidgetMode];
		}
	}

	return GetModeManager()->GetCoordSystem();	
}

bool FControlRigEditMode::ComputePivotFromEditedShape(UControlRig* InControlRig, FTransform& OutTransform) const
{
	const URigHierarchy* Hierarchy = InControlRig ? InControlRig->GetHierarchy() : nullptr;
	if (!Hierarchy)
	{
		return false;
	}

	if (!ensure(bIsChangingControlShapeTransform))
	{
		return false;
	}
	
	OutTransform = FTransform::Identity;
	
	if (auto* ShapeActors = ControlRigShapeActors.Find(InControlRig))
	{
		// we just want to change the shape transform of one single control.
		const int32 Index = ShapeActors->IndexOfByPredicate([](const TObjectPtr<AControlRigShapeActor>& ShapeActor)
		{
			return IsValid(ShapeActor) && ShapeActor->IsSelected();
		});

		if (Index != INDEX_NONE)
		{
			if (FRigControlElement* ControlElement = InControlRig->FindControl((*ShapeActors)[Index]->ControlName))
			{
				OutTransform = Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentGlobal);
			}				
		}
	}
	
	return true;
}

bool FControlRigEditMode::ComputePivotFromShapeActors(UControlRig* InControlRig, const bool bEachLocalSpace, const bool bIsParentSpace, FTransform& OutTransform) const
{
	if (!ensure(!bIsChangingControlShapeTransform))
	{
		return false;
	}
	
	const URigHierarchy* Hierarchy = InControlRig ? InControlRig->GetHierarchy() : nullptr;
	if (!Hierarchy)
	{
		return false;
	}
	const FTransform ComponentTransform = GetHostingSceneComponentTransform(InControlRig);

	FTransform LastTransform = FTransform::Identity, PivotTransform = FTransform::Identity;

	if (const auto* ShapeActors = ControlRigShapeActors.Find(InControlRig))
	{
		// if in local just use the first selected actor transform
		// otherwise, compute the average location as pivot location
		
		int32 NumSelectedControls = 0;
		FVector PivotLocation = FVector::ZeroVector;
		for (const TObjectPtr<AControlRigShapeActor>& ShapeActor : *ShapeActors)
		{
			if (IsValid(ShapeActor) && ShapeActor->IsSelected())
			{
				const FRigElementKey ControlKey = ShapeActor->GetElementKey();
				bool bGetParentTransform = bIsParentSpace && Hierarchy->GetNumberOfParents(ControlKey);
	
				const FTransform ShapeTransform = ShapeActor->GetActorTransform().GetRelativeTransform(ComponentTransform);
				LastTransform = bGetParentTransform ? Hierarchy->GetParentTransform(ControlKey) : ShapeTransform;
				PivotLocation += ShapeTransform.GetLocation();
				
				++NumSelectedControls;
				if (bEachLocalSpace)
				{
					break;
				}
			}
		}

		if (NumSelectedControls > 1)
		{
			PivotLocation /= static_cast<double>(NumSelectedControls);
		}
		PivotTransform.SetLocation(PivotLocation);
	}

	// Use the last transform's rotation as pivot rotation
	const FTransform WorldTransform = LastTransform * ComponentTransform;
	PivotTransform.SetRotation(WorldTransform.GetRotation());
	
	OutTransform = PivotTransform;
	
	return true;
}

bool FControlRigEditMode::ComputePivotFromElements(UControlRig* InControlRig, FTransform& OutTransform) const
{
	if (!ensure(!bIsChangingControlShapeTransform))
	{
		return false;
	}
	
	const URigHierarchy* Hierarchy = InControlRig ? InControlRig->GetHierarchy() : nullptr;
	if (!Hierarchy)
	{
		return false;
	}
	
	const FTransform ComponentTransform = GetHostingSceneComponentTransform(InControlRig);
	
	int32 NumSelection = 0;
	FTransform LastTransform = FTransform::Identity, PivotTransform = FTransform::Identity;
	FVector PivotLocation = FVector::ZeroVector;
	const TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(InControlRig);
	
	for (int32 Index = 0; Index < SelectedRigElements.Num(); ++Index)
	{
		if (SelectedRigElements[Index].Type == ERigElementType::Control)
		{
			LastTransform = OnGetRigElementTransformDelegate.Execute(SelectedRigElements[Index], false, true);
			PivotLocation += LastTransform.GetLocation();
			++NumSelection;
		}
	}

	if (NumSelection == 1)
	{
		// A single control just uses its own transform
		const FTransform WorldTransform = LastTransform * ComponentTransform;
		PivotTransform.SetRotation(WorldTransform.GetRotation());
	}
	else if (NumSelection > 1)
	{
		PivotLocation /= static_cast<double>(NumSelection);
		PivotTransform.SetRotation(ComponentTransform.GetRotation());
	}
		
	PivotTransform.SetLocation(PivotLocation);
	OutTransform = PivotTransform;

	return true;
}

void FControlRigEditMode::UpdatePivotTransforms()
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	const bool bEachLocalSpace = Settings && Settings->bLocalTransformsInEachLocalSpace;
	const bool bIsParentSpace = GetCoordSystemSpace() == COORD_Parent;

	PivotTransforms.Reset();

	for (const TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			bool bAdd = false;
			FTransform Pivot = FTransform::Identity;
			if (AreRigElementsSelected(ValidControlTypeMask(), ControlRig))
			{
				if (bIsChangingControlShapeTransform)
				{
					bAdd = ComputePivotFromEditedShape(ControlRig, Pivot);
				}
				else
				{
					bAdd = ComputePivotFromShapeActors(ControlRig, bEachLocalSpace, bIsParentSpace, Pivot);			
				}
			}
			else if (AreRigElementSelectedAndMovable(ControlRig))
			{
				// do we even get in here ?!
				// we will enter the if first as AreRigElementsSelected will return true before AreRigElementSelectedAndMovable does...
				bAdd = ComputePivotFromElements(ControlRig, Pivot);
			}
			if (bAdd)
			{
				PivotTransforms.Add(ControlRig, MoveTemp(Pivot));
			}
		}
	}

	bPivotsNeedUpdate = false;

	//If in level editor and the transforms changed we need to force hit proxy invalidate so widget hit testing 
	//doesn't work off of it's last transform.  Similar to what sequencer does on re-evaluation but do to how edit modes and widget ticks happen
	//it doesn't work for control rig gizmo's
	if (IsInLevelEditor())
	{
		if (HasPivotTransformsChanged())
		{
			for (FEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
			{
				if (LevelVC)
				{
					if (!LevelVC->IsRealtime())
					{
						LevelVC->RequestRealTimeFrames(1);
					}

					if (LevelVC->Viewport)
					{
						LevelVC->Viewport->InvalidateHitProxy();
					}
				}
			}
		}
		LastPivotTransforms = PivotTransforms;
	}
}

void FControlRigEditMode::RequestTransformWidgetMode(UE::Widget::EWidgetMode InWidgetMode)
{
	RequestedWidgetModes.Add(InWidgetMode);
}

bool FControlRigEditMode::HasPivotTransformsChanged() const
{
	if (PivotTransforms.Num() != LastPivotTransforms.Num())
	{
		return true;
	}
	for (const TPair<UControlRig*, FTransform>& Transform : PivotTransforms)
	{
		if (const FTransform* LastTransform = LastPivotTransforms.Find(Transform.Key))
		{
			if (Transform.Value.Equals(*LastTransform, 1e-4f) == false)
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}
	return false;
}

void FControlRigEditMode::UpdatePivotTransformsIfNeeded(UControlRig* InControlRig, FTransform& InOutTransform) const
{
	if (!bPivotsNeedUpdate)
	{
		return;
	}

	if (!InControlRig)
	{
		return;
	}

	// Update shape actors transforms
	if (auto* ShapeActors = ControlRigShapeActors.Find(InControlRig))
	{
		FTransform ComponentTransform = FTransform::Identity;
		if (!AreEditingControlRigDirectly())
		{
			ComponentTransform = GetHostingSceneComponentTransform(InControlRig);
		}
		for (AControlRigShapeActor* ShapeActor : *ShapeActors)
		{
			const FTransform Transform = InControlRig->GetControlGlobalTransform(ShapeActor->ControlName);
			ShapeActor->SetActorTransform(Transform * ComponentTransform);
		}
	}

	// Update pivot
	if (AreRigElementsSelected(ValidControlTypeMask(), InControlRig))
	{
		if (bIsChangingControlShapeTransform)
		{
			ComputePivotFromEditedShape(InControlRig, InOutTransform);
		}
		else
		{
			const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
			const bool bEachLocalSpace = Settings && Settings->bLocalTransformsInEachLocalSpace;
			const bool bIsParentSpace = GetCoordSystemSpace() == COORD_Parent;
			ComputePivotFromShapeActors(InControlRig, bEachLocalSpace, bIsParentSpace, InOutTransform);			
		}
	}
	else if (AreRigElementSelectedAndMovable(InControlRig))
	{
		ComputePivotFromElements(InControlRig, InOutTransform);
	}
}

void FControlRigEditMode::HandleSelectionChanged()
{
	for (const auto& ShapeActors : ControlRigShapeActors)
	{
		for (const TObjectPtr<AControlRigShapeActor>& ShapeActor : ShapeActors.Value)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
			ShapeActor->GetComponents(PrimitiveComponents, true);
			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				PrimitiveComponent->PushSelectionToProxy();
			}
		}
	}

	// automatically exit shape transform edit mode if there is no shape selected
	if (bIsChangingControlShapeTransform)
	{
		if (!CanChangeControlShapeTransform())
		{
			bIsChangingControlShapeTransform = false;
		}
	}

	// update the pivot transform of our selected objects (they could be animating)
	UpdatePivotTransforms();
	
	//need to force the redraw also
	if (!AreEditingControlRigDirectly())
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FControlRigEditMode::BindCommands()
{
	const FControlRigEditModeCommands& Commands = FControlRigEditModeCommands::Get();

	CommandBindings->MapAction(
		Commands.ToggleManipulators,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleManipulators));
	CommandBindings->MapAction(
		Commands.ToggleAllManipulators,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleAllManipulators));
	CommandBindings->MapAction(
		Commands.ZeroTransforms,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ZeroTransforms, true));
	CommandBindings->MapAction(
		Commands.ZeroAllTransforms,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ZeroTransforms, false));
	CommandBindings->MapAction(
		Commands.InvertTransforms,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::InvertInputPose, true));
	CommandBindings->MapAction(
		Commands.InvertAllTransforms,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::InvertInputPose, false));
	CommandBindings->MapAction(
		Commands.ClearSelection,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ClearSelection));

	CommandBindings->MapAction(
		Commands.ChangeAnimSliderTool,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ChangeAnimSliderTool),
		FCanExecuteAction::CreateRaw(this, &FControlRigEditMode::CanChangeAnimSliderTool)
	);

	CommandBindings->MapAction(
		Commands.FrameSelection,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::FrameSelection),
		FCanExecuteAction::CreateRaw(this, &FControlRigEditMode::CanFrameSelection)
	);

	CommandBindings->MapAction(
		Commands.IncreaseControlShapeSize,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::IncreaseShapeSize));

	CommandBindings->MapAction(
		Commands.DecreaseControlShapeSize,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::DecreaseShapeSize));

	CommandBindings->MapAction(
		Commands.ResetControlShapeSize,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ResetControlShapeSize));

	CommandBindings->MapAction(
		Commands.ToggleControlShapeTransformEdit,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::ToggleControlShapeTransformEdit));

	CommandBindings->MapAction(
		Commands.OpenSpacePickerWidget,
		FExecuteAction::CreateRaw(this, &FControlRigEditMode::OpenSpacePickerWidget));
}

bool FControlRigEditMode::IsControlSelected() const
{
	static uint32 TypeFlag = (uint32)ERigElementType::Control;
	for (const TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (AreRigElementsSelected(TypeFlag,ControlRig))
			{
				return true;
			}
		}
	}
	return false;
}

bool FControlRigEditMode::CanFrameSelection()
{
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (GetSelectedRigElements(ControlRig).Num() > 0)
			{
				return true;
			}
		}
	}
	return  false;;
}

void FControlRigEditMode::ClearSelection()
{
	ClearRigElementSelection(FRigElementTypeHelper::ToMask(ERigElementType::All));
	if (GEditor)
	{
		GEditor->Exec(GetWorld(), TEXT("SELECT NONE"));
	}
}

void FControlRigEditMode::FrameSelection()
{
	if(CurrentViewportClient)
	{
		FSphere Sphere(EForceInit::ForceInit);
		if(GetCameraTarget(Sphere))
		{
			FBox Bounds(EForceInit::ForceInit);
			Bounds += Sphere.Center;
			Bounds += Sphere.Center + FVector::OneVector * Sphere.W;
			Bounds += Sphere.Center - FVector::OneVector * Sphere.W;
			CurrentViewportClient->FocusViewportOnBox(Bounds);
			return;
		}
    }

	TArray<AActor*> Actors;
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
			for (const FRigElementKey& SelectedKey : SelectedRigElements)
			{
				if (SelectedKey.Type == ERigElementType::Control)
				{
					AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig,SelectedKey.Name);
					if (ShapeActor)
					{
						Actors.Add(ShapeActor);
					}
				}
			}
		}
	}

	if (Actors.Num())
	{
		TArray<UPrimitiveComponent*> SelectedComponents;
		GEditor->MoveViewportCamerasToActor(Actors, SelectedComponents, true);
	}
}

void FControlRigEditMode::FrameItems(const TArray<FRigElementKey>& InItems)
{
	if(!OnGetRigElementTransformDelegate.IsBound())
	{
		return;
	}

	if(CurrentViewportClient == nullptr)
	{
		DeferredItemsToFrame = InItems;
		return;
	}

	FBox Box(ForceInit);

	for (int32 Index = 0; Index < InItems.Num(); ++Index)
	{
		static const float Radius = 20.f;
		if (InItems[Index].Type == ERigElementType::Bone || InItems[Index].Type == ERigElementType::Null)
		{
			FTransform Transform = OnGetRigElementTransformDelegate.Execute(InItems[Index], false, true);
			Box += Transform.TransformPosition(FVector::OneVector * Radius);
			Box += Transform.TransformPosition(FVector::OneVector * -Radius);
		}
		else if (InItems[Index].Type == ERigElementType::Control)
		{
			FTransform Transform = OnGetRigElementTransformDelegate.Execute(InItems[Index], false, true);
			Box += Transform.TransformPosition(FVector::OneVector * Radius);
			Box += Transform.TransformPosition(FVector::OneVector * -Radius);
		}
	}

	if(Box.IsValid)
	{
		CurrentViewportClient->FocusViewportOnBox(Box);
	}
}

void FControlRigEditMode::IncreaseShapeSize()
{
	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	Settings->GizmoScale += 0.1f;
	GetModeManager()->SetWidgetScale(Settings->GizmoScale);
}

void FControlRigEditMode::DecreaseShapeSize()
{
	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	Settings->GizmoScale -= 0.1f;
	GetModeManager()->SetWidgetScale(Settings->GizmoScale);
}

void FControlRigEditMode::ResetControlShapeSize()
{
	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	GetModeManager()->SetWidgetScale(PreviousGizmoScale);
}

uint8 FControlRigEditMode::GetInteractionType(const FEditorViewportClient* InViewportClient)
{
	EControlRigInteractionType Result = EControlRigInteractionType::None;
	if (InViewportClient->IsMovingCamera())
	{
		return static_cast<uint8>(Result);
	}
	
	switch (InViewportClient->GetWidgetMode())
	{
		case UE::Widget::WM_Translate:
			EnumAddFlags(Result, EControlRigInteractionType::Translate);
			break;
		case UE::Widget::WM_TranslateRotateZ:
			EnumAddFlags(Result, EControlRigInteractionType::Translate);
			EnumAddFlags(Result, EControlRigInteractionType::Rotate);
			break;
		case UE::Widget::WM_Rotate:
			EnumAddFlags(Result, EControlRigInteractionType::Rotate);
			break;
		case UE::Widget::WM_Scale:
			EnumAddFlags(Result, EControlRigInteractionType::Scale);
			break;
		default:
			break;
	}
	return static_cast<uint8>(Result);
}

void FControlRigEditMode::ToggleControlShapeTransformEdit()
{ 
	if (bIsChangingControlShapeTransform)
	{
		bIsChangingControlShapeTransform = false;
	}
	else if (CanChangeControlShapeTransform())
	{
		bIsChangingControlShapeTransform = true;
	}
}

void FControlRigEditMode::GetAllSelectedControls(TMap<UControlRig*, TArray<FRigElementKey>>& OutSelectedControls) const
{
	for (const TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
			{
				TArray<FRigElementKey> SelectedControls = Hierarchy->GetSelectedKeys(ERigElementType::Control);
				if (SelectedControls.Num() > 0)
				{
					OutSelectedControls.Add(ControlRig, SelectedControls);
				}
			}
		}
	}
}

/** If Anim Slider is open, got to the next tool*/
void FControlRigEditMode::ChangeAnimSliderTool()
{
	if (Toolkit.IsValid())
	{
		StaticCastSharedPtr<FControlRigEditModeToolkit>(Toolkit)->GetToNextActiveSlider();
	}
}

/** If Anim Slider is open, then can drag*/
bool FControlRigEditMode::CanChangeAnimSliderTool() const
{
	if (Toolkit.IsValid())
	{
		return (StaticCastSharedPtr<FControlRigEditModeToolkit>(Toolkit)->CanChangeAnimSliderTool());
	}
	return false;
}

/** If Anim Slider is open, drag the tool*/
void FControlRigEditMode::DragAnimSliderTool(double IncrementVal)
{
	if (Toolkit.IsValid())
	{
		StaticCastSharedPtr<FControlRigEditModeToolkit>(Toolkit)->DragAnimSliderTool(IncrementVal);
	}
}

void FControlRigEditMode::StartAnimSliderTool(int32 InX)
{
	if (Toolkit.IsValid())
	{
		bisTrackingAnimToolDrag = true;
		GEditor->BeginTransaction(LOCTEXT("AnimSliderBlend", "AnimSlider Blend"));
		StartXValue = InX;
		StaticCastSharedPtr<FControlRigEditModeToolkit>(Toolkit)->StartAnimSliderTool();
	}
}

/** Reset and stop user the anim slider tool*/
void FControlRigEditMode::ResetAnimSlider()
{
	if (Toolkit.IsValid())
	{
		GEditor->EndTransaction();
		bisTrackingAnimToolDrag = false;
		StartXValue.Reset();
		StaticCastSharedPtr<FControlRigEditModeToolkit>(Toolkit)->ResetAnimSlider();
	}
}

/** If the Drag Anim Slider Tool is pressed*/
bool FControlRigEditMode::IsDragAnimSliderToolPressed(FViewport* InViewport)
{
	if (IsInLevelEditor() && Toolkit.IsValid() &&  CanChangeAnimSliderTool())
	{
		bool bIsMovingSlider = false;
		const FControlRigEditModeCommands& Commands = FControlRigEditModeCommands::Get();
		// Need to iterate through primary and secondary to make sure they are all pressed.
		for (uint32 i = 0; i < static_cast<uint8>(EMultipleKeyBindingIndex::NumChords); ++i)
		{
			EMultipleKeyBindingIndex ChordIndex = static_cast<EMultipleKeyBindingIndex>(i);
			const FInputChord& Chord = *Commands.DragAnimSliderTool->GetActiveChord(ChordIndex);
			bIsMovingSlider |= Chord.IsValidChord() && InViewport->KeyState(Chord.Key);
		}
		return bIsMovingSlider;
	}
	return false;
}

void FControlRigEditMode::OpenSpacePickerWidget()
{
	TMap<UControlRig*, TArray<FRigElementKey>> SelectedControlRigsAndControls;
	GetAllSelectedControls(SelectedControlRigsAndControls);

	if (SelectedControlRigsAndControls.Num() < 1)
	{
		return;
	}

	TArray<UControlRig*> ControlRigs;
	TArray<TArray<FRigElementKey>> AllSelectedControls;
	SelectedControlRigsAndControls.GenerateKeyArray(ControlRigs);
	SelectedControlRigsAndControls.GenerateValueArray(AllSelectedControls);


	//mz todo handle multiple control rigs with space picker
	UControlRig* RuntimeRig = ControlRigs[0];
	TArray<FRigElementKey>& SelectedControls = AllSelectedControls[0];

	URigHierarchy* Hierarchy = RuntimeRig->GetHierarchy();

	TSharedRef<SRigSpacePickerWidget> PickerWidget =
	SNew(SRigSpacePickerWidget)
	.Hierarchy(Hierarchy)
	.Controls(SelectedControls)
	.Title(LOCTEXT("PickSpace", "Pick Space"))
	.AllowDelete(AreEditingControlRigDirectly())
	.AllowReorder(AreEditingControlRigDirectly())
	.AllowAdd(AreEditingControlRigDirectly())
	.GetControlCustomization_Lambda([this, RuntimeRig](URigHierarchy*, const FRigElementKey& InControlKey)
	{
		return RuntimeRig->GetControlCustomization(InControlKey);
	})
	.OnActiveSpaceChanged_Lambda([this, SelectedControls, RuntimeRig](URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const FRigElementKey& InSpaceKey)
	{
		check(SelectedControls.Contains(InControlKey));
		if (!AreEditingControlRigDirectly())
		{
			if (WeakSequencer.IsValid())
			{
				if (const FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
				{
					ISequencer* Sequencer = WeakSequencer.Pin().Get();
					if (Sequencer)
					{
						FScopedTransaction Transaction(LOCTEXT("KeyControlRigSpace", "Key Control Rig Space"));
						FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(RuntimeRig, InControlKey.Name, Sequencer, true /*bCreateIfNeeded*/);
						if (SpaceChannelAndSection.SpaceChannel)
						{
							const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
							const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
							FFrameNumber CurrentTime = FrameTime.GetFrame();
							FControlRigSpaceChannelHelpers::SequencerKeyControlRigSpaceChannel(RuntimeRig, Sequencer, SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey, CurrentTime, InHierarchy, InControlKey, InSpaceKey);
						}
					}
				}
			}
		}
		else
		{
			if (RuntimeRig->IsAdditive())
			{
				const FTransform Transform = RuntimeRig->GetControlGlobalTransform(InControlKey.Name);
			   RuntimeRig->SwitchToParent(InControlKey, InSpaceKey, false, true);
			   RuntimeRig->Evaluate_AnyThread();
			   FRigControlValue ControlValue = RuntimeRig->GetControlValueFromGlobalTransform(InControlKey.Name, Transform, ERigTransformType::CurrentGlobal);
			   RuntimeRig->SetControlValue(InControlKey.Name, ControlValue);
			   RuntimeRig->Evaluate_AnyThread();
			}
			else
			{
				const FTransform Transform = InHierarchy->GetGlobalTransform(InControlKey);
				URigHierarchy::TElementDependencyMap Dependencies = InHierarchy->GetDependenciesForVM(RuntimeRig->GetVM());
				FString OutFailureReason;
				if (InHierarchy->SwitchToParent(InControlKey, InSpaceKey, false, true, Dependencies, &OutFailureReason))
				{
					InHierarchy->SetGlobalTransform(InControlKey, Transform);
				}
				else
				{
					if(URigHierarchyController* Controller = InHierarchy->GetController())
					{
						static constexpr TCHAR MessageFormat[] = TEXT("Could not switch %s to parent %s: %s");
						Controller->ReportAndNotifyErrorf(MessageFormat,
							*InControlKey.Name.ToString(),
							*InSpaceKey.Name.ToString(),
							*OutFailureReason);
					}
				}
			}
		}
		
	})
	.OnSpaceListChanged_Lambda([this, SelectedControls, RuntimeRig](URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const TArray<FRigElementKey>& InSpaceList)
	{
		check(SelectedControls.Contains(InControlKey));

		// check if we are in the control rig editor or in the level
		if(AreEditingControlRigDirectly())
		{
			if (UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(RuntimeRig->GetClass()->ClassGeneratedBy))
			{
				if(URigHierarchy* Hierarchy = Blueprint->Hierarchy)
				{
					// update the settings in the control element
					if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InControlKey))
					{
						Blueprint->Modify();
						FScopedTransaction Transaction(LOCTEXT("ControlChangeAvailableSpaces", "Edit Available Spaces"));

						ControlElement->Settings.Customization.AvailableSpaces = InSpaceList;
						Hierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
					}

					// also update the debugged instance
					if(Hierarchy != InHierarchy)
					{
						if(FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
						{
							ControlElement->Settings.Customization.AvailableSpaces = InSpaceList;
							InHierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
						}
					}
				}
			}
		}
		else
		{
			// update the settings in the control element
			if(FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
			{
				FScopedTransaction Transaction(LOCTEXT("ControlChangeAvailableSpaces", "Edit Available Spaces"));

				InHierarchy->Modify();

				FRigControlElementCustomization ControlCustomization = *RuntimeRig->GetControlCustomization(InControlKey);	
				ControlCustomization.AvailableSpaces = InSpaceList;
				ControlCustomization.RemovedSpaces.Reset();

				// remember  the elements which are in the asset's available list but removed by the user
				for(const FRigElementKey& AvailableSpace : ControlElement->Settings.Customization.AvailableSpaces)
				{
					if(!ControlCustomization.AvailableSpaces.Contains(AvailableSpace))
					{
						ControlCustomization.RemovedSpaces.Add(AvailableSpace);
					}
				}

				RuntimeRig->SetControlCustomization(InControlKey, ControlCustomization);
				InHierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
			}
		}

	});
	// todo: implement GetAdditionalSpacesDelegate to pull spaces from sequencer

	PickerWidget->OpenDialog(false);
}

FText FControlRigEditMode::GetToggleControlShapeTransformEditHotKey() const
{
	const FControlRigEditModeCommands& Commands = FControlRigEditModeCommands::Get();
	return Commands.ToggleControlShapeTransformEdit->GetInputText();
}

void FControlRigEditMode::ToggleManipulators()
{
	if (!AreEditingControlRigDirectly())
	{
		TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
		GetAllSelectedControls(SelectedControls);
		TArray<UControlRig*> ControlRigs;
		SelectedControls.GenerateKeyArray(ControlRigs);
		for (UControlRig* ControlRig : ControlRigs)
		{
			if (ControlRig)
			{
				FScopedTransaction ScopedTransaction(LOCTEXT("ToggleControlsVisibility", "Toggle Controls Visibility"),!GIsTransacting);
				ControlRig->Modify();
				ControlRig->ToggleControlsVisible();
			}
		}
	}
	else
	{
		UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
		Settings->bHideControlShapes = !Settings->bHideControlShapes;
	}
}

void FControlRigEditMode::ToggleAllManipulators()
{	
	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	Settings->bHideControlShapes = !Settings->bHideControlShapes;

	//turn on all if in level editor in case any where off
	if (!AreEditingControlRigDirectly() && Settings->bHideControlShapes)
	{
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* ControlRig = RuntimeRigPtr.Get())
			{
				ControlRig->SetControlsVisible(true);
			}
		}
	}
}

void FControlRigEditMode::ZeroTransforms(bool bSelectionOnly)
{
	// Gather up the control rigs for the selected controls
	TArray<UControlRig*> ControlRigs;
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (!bSelectionOnly || ControlRig->CurrentControlSelection().Num() > 0)
			{
				ControlRigs.Add(ControlRig);
			}
		}
	}
	if (ControlRigs.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("HierarchyZeroTransforms", "Zero Transforms"));

	for (UControlRig* ControlRig : ControlRigs)
	{
		TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
		if (ControlRig->IsAdditive())
		{
			// For additive rigs, ignore boolean controls
			SelectedRigElements = SelectedRigElements.FilterByPredicate([ControlRig](const FRigElementKey& Key)
			{
				if (FRigControlElement* Element = ControlRig->FindControl(Key.Name))
				{
					return Element->CanTreatAsAdditive();
				}
				return true;
			});
		}
		TArray<FRigElementKey> ControlsToReset = SelectedRigElements;
		TArray<FRigElementKey> ControlsInteracting = SelectedRigElements;
		TArray<FRigElementKey> TransformElementsToReset = SelectedRigElements;
		if (!bSelectionOnly)
		{
			TArray<FRigBaseElement*> Elements = ControlRig->GetHierarchy()->GetElementsOfType<FRigBaseElement>(true);
			TransformElementsToReset.Reset();
			TransformElementsToReset.Reserve(Elements.Num());
			for (const FRigBaseElement* Element : Elements)
			{
				// For additive rigs, ignore non-additive controls
				if (const FRigControlElement* Control = Cast<FRigControlElement>(Element))
				{
					if (ControlRig->IsAdditive() && !Control->CanTreatAsAdditive())
					{
						continue;
					}
				}
				TransformElementsToReset.Add(Element->GetKey());
			}
			
			TArray<FRigControlElement*> Controls;
			ControlRig->GetControlsInOrder(Controls);
			ControlsToReset.SetNum(0);
			ControlsInteracting.SetNum(0);
			for (const FRigControlElement* Control : Controls)
			{
				// For additive rigs, ignore boolean controls
				if (ControlRig->IsAdditive() && Control->Settings.ControlType == ERigControlType::Bool)
				{
					continue;
				}
				ControlsToReset.Add(Control->GetKey());
				if(Control->Settings.AnimationType == ERigControlAnimationType::AnimationControl ||
					Control->IsAnimationChannel())
				{
					ControlsInteracting.Add(Control->GetKey());
				}
			}
		}
		bool bHasNonDefaultParent = false;
		TMap<FRigElementKey, FRigElementKey> Parents;
		for (const FRigElementKey& Key : TransformElementsToReset)
		{
			FRigElementKey SpaceKey = ControlRig->GetHierarchy()->GetActiveParent(Key);
			Parents.Add(Key, SpaceKey);
			if (!bHasNonDefaultParent && SpaceKey != ControlRig->GetHierarchy()->GetDefaultParentKey())
			{
				bHasNonDefaultParent = true;
			}
		}

		FControlRigInteractionScope InteractionScope(ControlRig, ControlsInteracting);
		for (const FRigElementKey& ElementToReset : TransformElementsToReset)
		{
			FRigControlElement* ControlElement = nullptr;
			if (ElementToReset.Type == ERigElementType::Control)
			{
				ControlElement = ControlRig->FindControl(ElementToReset.Name);
				if (ControlElement->Settings.bIsTransientControl)
				{
					if(UControlRig::GetNodeNameFromTransientControl(ControlElement->GetKey()).IsEmpty())
					{
						ControlElement = nullptr;
					}
				}
			}
			
			const FTransform InitialLocalTransform = ControlRig->GetInitialLocalTransform(ElementToReset);
			ControlRig->Modify();
			if (bHasNonDefaultParent == true) //possibly not at default parent so switch to it
			{
				ControlRig->GetHierarchy()->SwitchToDefaultParent(ElementToReset);
			}
			if (ControlElement)
			{
				ControlRig->SetControlLocalTransform(ElementToReset.Name, InitialLocalTransform, true, FRigControlModifiedContext(), true, true);
				const FVector InitialAngles = ControlRig->GetHierarchy()->GetControlPreferredEulerAngles(ControlElement, ControlElement->Settings.PreferredRotationOrder, true);
				ControlRig->GetHierarchy()->SetControlPreferredEulerAngles(ControlElement, InitialAngles, ControlElement->Settings.PreferredRotationOrder);
				NotifyDrivenControls(ControlRig, ElementToReset);
				if (bHasNonDefaultParent == false)
				{
					ControlRig->ControlModified().Broadcast(ControlRig, ControlElement, EControlRigSetKey::DoNotCare);
				}
			}
			else
			{
				ControlRig->GetHierarchy()->SetLocalTransform(ElementToReset, InitialLocalTransform, false, true, true);
			}

			//@helge not sure what to do if the non-default parent
			if (UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy))
			{
				Blueprint->Hierarchy->SetLocalTransform(ElementToReset, InitialLocalTransform);
			}
		}

		if (bHasNonDefaultParent == true) //now we have the initial pose setup we need to get the global transforms as specified now then set them in the current parent space
		{
			ControlRig->Evaluate_AnyThread();

			//get global transforms
			TMap<FRigElementKey, FTransform> GlobalTransforms;
			for (const FRigElementKey& ElementToReset : TransformElementsToReset)
			{
				if (ElementToReset.IsTypeOf(ERigElementType::Control))
				{
					FRigControlElement* ControlElement = ControlRig->FindControl(ElementToReset.Name);
					if (ControlElement && !ControlElement->Settings.bIsTransientControl)
					{
						FTransform GlobalTransform = ControlRig->GetHierarchy()->GetGlobalTransform(ElementToReset);
						GlobalTransforms.Add(ElementToReset, GlobalTransform);
					}
					NotifyDrivenControls(ControlRig, ElementToReset);
				}
				else
				{
					FTransform GlobalTransform = ControlRig->GetHierarchy()->GetGlobalTransform(ElementToReset);
					GlobalTransforms.Add(ElementToReset, GlobalTransform);
				}
			}
			//switch back to original parent space
			for (const FRigElementKey& ElementToReset : TransformElementsToReset)
			{
				if (const FRigElementKey* SpaceKey = Parents.Find(ElementToReset))
				{
					if (ElementToReset.IsTypeOf(ERigElementType::Control))
					{
						FRigControlElement* ControlElement = ControlRig->FindControl(ElementToReset.Name);
						if (ControlElement && !ControlElement->Settings.bIsTransientControl)
						{
							ControlRig->GetHierarchy()->SwitchToParent(ElementToReset, *SpaceKey);
						}
					}
					else
					{
						ControlRig->GetHierarchy()->SwitchToParent(ElementToReset, *SpaceKey);
					}
				}
			}
			//set global transforms in this space // do it twice since ControlsInOrder is not really always in order
			for (int32 SetHack = 0; SetHack < 2; ++SetHack)
			{
				ControlRig->Evaluate_AnyThread();
				for (const FRigElementKey& ElementToReset : TransformElementsToReset)
				{
					if (const FTransform* GlobalTransform = GlobalTransforms.Find(ElementToReset))
					{
						if (ElementToReset.IsTypeOf(ERigElementType::Control))
						{
							FRigControlElement* ControlElement = ControlRig->FindControl(ElementToReset.Name);
							if (ControlElement && !ControlElement->Settings.bIsTransientControl)
							{
								ControlRig->SetControlGlobalTransform(ElementToReset.Name, *GlobalTransform, true);
								ControlRig->Evaluate_AnyThread();
								NotifyDrivenControls(ControlRig, ElementToReset);
							}
						}
						else
						{
							ControlRig->GetHierarchy()->SetGlobalTransform(ElementToReset, *GlobalTransform, false, true, true);
						}
					}
				}
			}
			//send notifies

			for (const FRigElementKey& ControlToReset : ControlsToReset)
			{
				FRigControlElement* ControlElement = ControlRig->FindControl(ControlToReset.Name);
				if (ControlElement && !ControlElement->Settings.bIsTransientControl)
				{
					ControlRig->ControlModified().Broadcast(ControlRig, ControlElement, EControlRigSetKey::DoNotCare);
				}
			}
		}
		else
		{
			// we have to insert the interaction event before we run current events
			TArray<FName> NewEventQueue = {FRigUnit_InteractionExecution::EventName};
			NewEventQueue.Append(ControlRig->EventQueue);
			TGuardValue<TArray<FName>> EventGuard(ControlRig->EventQueue, NewEventQueue);
			ControlRig->Evaluate_AnyThread();
			for (const FRigElementKey& ControlToReset : ControlsToReset)
			{
				NotifyDrivenControls(ControlRig, ControlToReset);
			}
		}
	}
}

void FControlRigEditMode::InvertInputPose(bool bSelectionOnly)
{
	// Gather up the control rigs for the selected controls
	TArray<UControlRig*> ControlRigs;
	for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			if (!bSelectionOnly || ControlRig->CurrentControlSelection().Num() > 0)
			{
				ControlRigs.Add(ControlRig);
			}
		}
	}
	if (ControlRigs.Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("HierarchyInvertTransformsToRestPose", "Invert Transforms to Rest Pose"));

	for (UControlRig* ControlRig : ControlRigs)
	{
		if (!ControlRig->IsAdditive())
		{
			ZeroTransforms(bSelectionOnly);
			continue;
		}

		TArray<FRigElementKey> SelectedRigElements;
		if (bSelectionOnly)
		{
			SelectedRigElements = GetSelectedRigElements(ControlRig);
			SelectedRigElements = SelectedRigElements.FilterByPredicate([ControlRig](const FRigElementKey& Key)
			{
				if (FRigControlElement* Element = ControlRig->FindControl(Key.Name))
				{
					return Element->Settings.ControlType != ERigControlType::Bool;
				}
				return true;
			});
		}

		const TArray<FRigControlElement*> ModifiedElements = ControlRig->InvertInputPose(SelectedRigElements, EControlRigSetKey::Never);
		ControlRig->Evaluate_AnyThread();

		for (FRigControlElement* ControlElement : ModifiedElements)
		{
			ControlRig->ControlModified().Broadcast(ControlRig, ControlElement, EControlRigSetKey::DoNotCare);
		}
	}
}

bool FControlRigEditMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	// Inform units of hover state
	HActor* ActorHitProxy = HitProxyCast<HActor>(Viewport->GetHitProxy(x, y));
	if(ActorHitProxy && ActorHitProxy->Actor)
	{
		if (ActorHitProxy->Actor->IsA<AControlRigShapeActor>())
		{
			for (auto& ShapeActors : ControlRigShapeActors)
			{
				for (AControlRigShapeActor* ShapeActor : ShapeActors.Value)
				{
					ShapeActor->SetHovered(ShapeActor == ActorHitProxy->Actor);
				}
			}
		}
	}

	return false;
}

bool FControlRigEditMode::MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	for (auto& ShapeActors : ControlRigShapeActors)
	{
		for (AControlRigShapeActor* ShapeActor : ShapeActors.Value)
		{
			ShapeActor->SetHovered(false);
		}
	}

	return false;
}

bool FControlRigEditMode::CheckMovieSceneSig()
{
	bool bSomethingChanged = false;
	if (WeakSequencer.IsValid())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer->GetFocusedMovieSceneSequence() && Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene())
		{
			FGuid CurrentMovieSceneSig = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetSignature();
			if (LastMovieSceneSig != CurrentMovieSceneSig)
			{
				if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Sequencer->GetFocusedMovieSceneSequence()))
				{
					TArray<TWeakObjectPtr<UControlRig>> CurrentControlRigs;
					TArray<FControlRigSequencerBindingProxy> Proxies = UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);
					for (FControlRigSequencerBindingProxy& Proxy : Proxies)
					{
						if (UControlRig* ControlRig = Proxy.ControlRig.Get())
						{
							CurrentControlRigs.Add(ControlRig);
							if (RuntimeControlRigs.Contains(ControlRig) == false)
							{
								AddControlRigInternal(ControlRig);
								bSomethingChanged = true;
							}
						}
					}
					TArray<TWeakObjectPtr<UControlRig>> ControlRigsToRemove;
					for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
					{
						if (CurrentControlRigs.Contains(RuntimeRigPtr) == false)
						{
							ControlRigsToRemove.Add(RuntimeRigPtr);
						}
					}
					for (TWeakObjectPtr<UControlRig>& OldRuntimeRigPtr : ControlRigsToRemove)
					{
						RemoveControlRig(OldRuntimeRigPtr.Get());
					}
				}
				LastMovieSceneSig = CurrentMovieSceneSig;
				if (bSomethingChanged)
				{
					SetObjects_Internal();
				}
				DetailKeyFrameCache->ResetCachedData();
			}
		}
	}
	return bSomethingChanged;
}

void FControlRigEditMode::PostUndo()
{
	bool bInvalidateViewport = false;
	if (WeakSequencer.IsValid())
	{
		bool bHaveInvalidControlRig = false;
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (RuntimeRigPtr.IsValid() == false)
			{				
				bHaveInvalidControlRig = bInvalidateViewport = true;
				break;
			}
		}
		//if one is invalid we need to clear everything,since no longer have ptr to selectively delete
		if (bHaveInvalidControlRig == true)
		{
			TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
			for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
			{
				if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
				{
					RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
				}
			}
			RuntimeControlRigs.Reset();
			DestroyShapesActors(nullptr);
			DelegateHelpers.Reset();
			RuntimeControlRigs.Reset();
		}
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Sequencer->GetFocusedMovieSceneSequence()))
		{
			bool bSomethingAdded = false;
			TArray<FControlRigSequencerBindingProxy> Proxies = UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);
			for (FControlRigSequencerBindingProxy& Proxy : Proxies)
			{
				if (UControlRig* ControlRig = Proxy.ControlRig.Get())
				{
					if (RuntimeControlRigs.Contains(ControlRig) == false)
					{
						AddControlRigInternal(ControlRig);
						bSomethingAdded = true;

					}
				}
			}
			if (bSomethingAdded)
			{
				Sequencer->ForceEvaluate();
				SetObjects_Internal();
				bInvalidateViewport = true;
			}
		}
	}
	else
	{
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (RuntimeRigPtr.IsValid() == false)
			{
				DestroyShapesActors(RuntimeRigPtr.Get());
				bInvalidateViewport = true;
			}
		}
	}

	//normal actor undo will force the redraw, so we need to do the same for our transients/controls.
	if (!AreEditingControlRigDirectly() && (bInvalidateViewport || UsesTransformWidget()))
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([this]()
		{
			//due to tick ordering need to manually make sure we get everything done in correct order.
			PostPoseUpdate();
			UpdatePivotTransforms();
			GEditor->RedrawLevelEditingViewports(true);
		});
	}

}

void FControlRigEditMode::RequestToRecreateControlShapeActors(UControlRig* ControlRig)
{ 
	if (ControlRig)
	{
		if (RecreateControlShapesRequired != ERecreateControlRigShape::RecreateAll)
		{
			RecreateControlShapesRequired = ERecreateControlRigShape::RecreateSpecified;
			if (ControlRigsToRecreate.Find(ControlRig) == INDEX_NONE)
			{
				ControlRigsToRecreate.Add(ControlRig);
			}
		}
	}
	else
	{
		RecreateControlShapesRequired = ERecreateControlRigShape::RecreateAll;
	}
}

// temporarily we just support following types of gizmo
static bool IsSupportedControlType(const ERigControlType ControlType)
{
	switch (ControlType)
	{
	case ERigControlType::Float:
	case ERigControlType::ScaleFloat:
	case ERigControlType::Integer:
	case ERigControlType::Vector2D:
	case ERigControlType::Position:
	case ERigControlType::Scale:
	case ERigControlType::Rotator:
	case ERigControlType::Transform:
	case ERigControlType::TransformNoScale:
	case ERigControlType::EulerTransform:
	{
		return true;
	}
	default:
	{
		break;
	}
	}

	return false;
}

void FControlRigEditMode::RecreateControlShapeActors(const TArray<FRigElementKey>& InSelectedElements)
{
	if (RecreateControlShapesRequired == ERecreateControlRigShape::RecreateAll)
	{
		for (TWeakObjectPtr<UControlRig>& RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* RuntimeControlRig = RuntimeRigPtr.Get())
			{
				DestroyShapesActors(RuntimeControlRig);
				CreateShapeActors(RuntimeControlRig);
			}
		}
	}
	else if (ControlRigsToRecreate.Num() > 0)
	{
		TArray < UControlRig*> ControlRigsCopy = ControlRigsToRecreate;
		for (UControlRig* ControlRig : ControlRigsCopy)
		{
			//check to see if actors have really changed, if not don't do it
			bool bRecreateThem = true;
			if (auto* ShapeActors = ControlRigShapeActors.Find(ControlRig))
			{
				TArray<FRigControlElement*> Controls = ControlRig->AvailableControls();
				TArray<FRigControlElement*> ControlPerShapeActor;
				ControlPerShapeActor.SetNumZeroed(ShapeActors->Num());
				
				if(Controls.Num() == ShapeActors->Num())
				{
					for (int32 ControlIndex = Controls.Num() - 1; ControlIndex >= 0; --ControlIndex)
					{
						FRigControlElement* ControlElement = Controls[ControlIndex];
						if (!ControlElement->Settings.SupportsShape() || !IsSupportedControlType(ControlElement->Settings.ControlType))
						{
							Controls.RemoveAtSwap(ControlIndex);
						}
					}
					//unfortunately n*n-ish but this should be very rare and much faster than recreating them
					for (int32 ShapeActorIndex = 0; ShapeActorIndex < ShapeActors->Num(); ShapeActorIndex++)
					{
						const AControlRigShapeActor* Actor = ShapeActors->operator[](ShapeActorIndex).Get();
						if (Actor)
						{
							for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ++ControlIndex)
							{
								FRigControlElement* Element = Controls[ControlIndex];
								if (Element && Element->GetFName() == Actor->ControlName)
								{
									Controls.RemoveAtSwap(ControlIndex);
									ControlPerShapeActor[ShapeActorIndex] = Element;
									break;
								}
							}
						}
						else //no actor just recreate
						{
							break;
						}
					}
				}
				if (Controls.Num() == 0)
				{
					bRecreateThem = false;

					// we have matching controls - we should at least sync their settings.
					// PostPoseUpdate / TickControlShape is going to take care of color, visibility etc.
					// MeshTransform has to be handled here.
					for (int32 ShapeActorIndex = 0; ShapeActorIndex < ShapeActors->Num(); ShapeActorIndex++)
					{
						const AControlRigShapeActor* ShapeActor = ShapeActors->operator[](ShapeActorIndex).Get();
						FRigControlElement* ControlElement = ControlPerShapeActor[ShapeActorIndex];
						if (ShapeActor && ControlElement)
						{
							const FTransform ShapeTransform = ControlRig->GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
							FTransform MeshTransform = FTransform::Identity;
							if (const FControlRigShapeDefinition* ShapeDef = UControlRigShapeLibrary::GetShapeByName(ControlElement->Settings.ShapeName, ControlRig->GetShapeLibraries(), ControlRig->ShapeLibraryNameMap))
							{
								MeshTransform = ShapeDef->Transform;

								if(UStaticMesh* ShapeMesh = ShapeDef->StaticMesh.LoadSynchronous())
								{
									if(ShapeActor->StaticMeshComponent->GetStaticMesh() != ShapeMesh)
									{
										ShapeActor->StaticMeshComponent->SetStaticMesh(ShapeMesh);
									}
								}
							}
							ShapeActor->StaticMeshComponent->SetRelativeTransform(MeshTransform * ShapeTransform);
						}
					}
					
					PostPoseUpdate();
				}
			}
			if (bRecreateThem)
			{
				DestroyShapesActors(ControlRig);
				CreateShapeActors(ControlRig);
			}
		}
		ControlRigsToRecreate.SetNum(0);
	}
}

void FControlRigEditMode::CreateShapeActors(UControlRig* ControlRig)
{
	// create gizmo actors
	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.bTemporaryEditorActor = true;

	if(bShowControlsAsOverlay)
	{
		// enable translucent selection
		GetMutableDefault<UEditorPerProjectUserSettings>()->bAllowSelectTranslucent = true;
	}

	TArray<FRigControlElement*> Controls = ControlRig->AvailableControls();
	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries = ControlRig->GetShapeLibraries();
	int32 ControlRigIndex = RuntimeControlRigs.Find(ControlRig);
	for (FRigControlElement* ControlElement : Controls)
	{
		if (!ControlElement->Settings.SupportsShape())
		{
			continue;
		}
		if (IsSupportedControlType(ControlElement->Settings.ControlType))
		{
			FControlShapeActorCreationParam Param;
			Param.ManipObj = ControlRig;
			Param.ControlRigIndex = ControlRigIndex;
			Param.ControlRig = ControlRig;
			Param.ControlName = ControlElement->GetFName();
			Param.ShapeName = ControlElement->Settings.ShapeName;
			Param.SpawnTransform = ControlRig->GetControlGlobalTransform(ControlElement->GetFName());
			Param.ShapeTransform = ControlRig->GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
			Param.bSelectable = ControlElement->Settings.IsSelectable(false);

			if (const FControlRigShapeDefinition* ShapeDef = UControlRigShapeLibrary::GetShapeByName(ControlElement->Settings.ShapeName, ShapeLibraries, ControlRig->ShapeLibraryNameMap))
			{
				Param.MeshTransform = ShapeDef->Transform;
				Param.StaticMesh = ShapeDef->StaticMesh;
				Param.Material = ShapeDef->Library->DefaultMaterial;
				if (bShowControlsAsOverlay)
				{
					TSoftObjectPtr<UMaterial> XRayMaterial = ShapeDef->Library->XRayMaterial;
					if (XRayMaterial.IsPending())
					{
						XRayMaterial.LoadSynchronous();
					}
					if (XRayMaterial.IsValid())
					{
						Param.Material = XRayMaterial;
					}
				}
				Param.ColorParameterName = ShapeDef->Library->MaterialColorParameter;
			}

			Param.Color = ControlElement->Settings.ShapeColor;

			AControlRigShapeActor* ShapeActor = FControlRigShapeHelper::CreateDefaultShapeActor(WorldPtr, Param);
			if (ShapeActor)
			{
				//not drawn in game or in game view.
				ShapeActor->SetActorHiddenInGame(true);
				auto* ShapeActors = ControlRigShapeActors.Find(ControlRig);
				if (ShapeActors)
				{
					ShapeActors->Add(ShapeActor);
				}
				else
				{
					TArray<AControlRigShapeActor*> NewShapeActors;
					NewShapeActors.Add(ShapeActor);
					ControlRigShapeActors.Add(ControlRig, ObjectPtrWrap(NewShapeActors));
				}
			}
		}
	}


	USceneComponent* Component = GetHostingSceneComponent(ControlRig);
	if (Component)
	{
		AActor* PreviewActor = Component->GetOwner();

		const auto* ShapeActors = ControlRigShapeActors.Find(ControlRig);
		if (ShapeActors)
		{
			for (AControlRigShapeActor* ShapeActor : *ShapeActors)
			{
				// attach to preview actor, so that we can communicate via relative transfrom from the previewactor
				ShapeActor->AttachToActor(PreviewActor, FAttachmentTransformRules::KeepWorldTransform);

				TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
				ShapeActor->GetComponents(PrimitiveComponents, true);
				for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
				{
					PrimitiveComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FControlRigEditMode::ShapeSelectionOverride);
					PrimitiveComponent->PushSelectionToProxy();
				}
			}
		}
	}
	if (!AreEditingControlRigDirectly())
	{

		if (ControlProxy)
		{
			ControlProxy->RecreateAllProxies(ControlRig);
		}
	}
	/** MZ got rid of this make sure it's okay
	for (const FRigElementKey& SelectedElement : InSelectedElements)
	{
		if(FRigControlElement* ControlElement = ControlRig->FindControl(SelectedElement.Name))
		{
			OnHierarchyModified(ERigHierarchyNotification::ElementSelected, ControlRig->GetHierarchy(), ControlElement);
		}
	}
	*/
	
}

FControlRigEditMode* FControlRigEditMode::GetEditModeFromWorldContext(UWorld* InWorldContext)
{
	return nullptr;
}

bool FControlRigEditMode::ShapeSelectionOverride(const UPrimitiveComponent* InComponent) const
{
    //Think we only want to do this in regular editor, in the level editor we are driving selection
	if (AreEditingControlRigDirectly())
	{
	    AControlRigShapeActor* OwnerActor = Cast<AControlRigShapeActor>(InComponent->GetOwner());
	    if (OwnerActor)
	    {
		    // See if the actor is in a selected unit proxy
		    return OwnerActor->IsSelected();
	    }
	}

	return false;
}

void FControlRigEditMode::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (int32 RigIndex = 0; RigIndex < RuntimeControlRigs.Num(); RigIndex++)
	{
		UObject* OldObject = RuntimeControlRigs[RigIndex].Get();
		UObject* NewObject = OldToNewInstanceMap.FindRef(OldObject);
		if (NewObject)
		{
			TArray<TWeakObjectPtr<UControlRig>> PreviousRuntimeRigs = RuntimeControlRigs;
			for (int32 PreviousRuntimeRigIndex = 0; PreviousRuntimeRigIndex < PreviousRuntimeRigs.Num(); PreviousRuntimeRigIndex++)
			{
				if (PreviousRuntimeRigs[PreviousRuntimeRigIndex].IsValid())
				{
					RemoveControlRig(PreviousRuntimeRigs[PreviousRuntimeRigIndex].Get());
				}
			}
			RuntimeControlRigs.Reset();

			UControlRig* NewRig = Cast<UControlRig>(NewObject);
			AddControlRigInternal(NewRig);

			NewRig->Initialize();

			SetObjects_Internal();
		}
	}
}

bool FControlRigEditMode::IsTransformDelegateAvailable() const
{
	return (OnGetRigElementTransformDelegate.IsBound() && OnSetRigElementTransformDelegate.IsBound());
}

bool FControlRigEditMode::AreRigElementSelectedAndMovable(UControlRig* ControlRig) const
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();

	if (!Settings || !ControlRig)
	{
		return false;
	}

	auto IsAnySelectedControlMovable = [this, ControlRig]()
	{
		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		if (!Hierarchy)
		{
			return false;
		}
		
		const TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
		return SelectedRigElements.ContainsByPredicate([Hierarchy](const FRigElementKey& Element)
		{
			if (!FRigElementTypeHelper::DoesHave(ValidControlTypeMask(), Element.Type))
			{
				return false;
			}
			const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Element);
			if (!ControlElement)
			{
				return false;
			}
			// can a control non selectable in the viewport be movable?  
			return ControlElement->Settings.IsSelectable();
		});
	};
	
	if (!IsAnySelectedControlMovable())
	{
		return false;
	}

	//when in sequencer/level we don't have that delegate so don't check.
	if (AreEditingControlRigDirectly())
	{
		if (!IsTransformDelegateAvailable())
		{
			return false;
		}
	}
	else //do check for the binding though
	{
		// if (GetHostingSceneComponent(ControlRig) == nullptr)
		// {
		// 	return false;
		// }
	}

	return true;
}

void FControlRigEditMode::ReplaceControlRig(UControlRig* OldControlRig, UControlRig* NewControlRig)
{
	if (OldControlRig != nullptr)
	{
		RemoveControlRig(OldControlRig);
	}
	AddControlRigInternal(NewControlRig);
	SetObjects_Internal();
	RequestToRecreateControlShapeActors(NewControlRig);

}
void FControlRigEditMode::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(bSuspendHierarchyNotifs || InElement == nullptr)
	{
		return;
	}

	check(InElement);
	switch(InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		case ERigHierarchyNotification::ElementRemoved:
		case ERigHierarchyNotification::ElementRenamed:
		case ERigHierarchyNotification::ElementReordered:
		case ERigHierarchyNotification::HierarchyReset:
		{
			UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>();
			RequestToRecreateControlShapeActors(ControlRig);
			break;
		}
		case ERigHierarchyNotification::ControlSettingChanged:
		case ERigHierarchyNotification::ControlVisibilityChanged:
		case ERigHierarchyNotification::ControlShapeTransformChanged:
		{
			const FRigElementKey Key = InElement->GetKey();
			UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>();
			if (Key.Type == ERigElementType::Control)
			{
				if (const FRigControlElement* ControlElement = Cast<FRigControlElement>(InElement))
				{
					if (AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig,Key.Name))
					{
						// try to lazily apply the changes to the actor
						const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
						if (ShapeActor->UpdateControlSettings(InNotif, ControlRig, ControlElement, Settings->bHideControlShapes, !AreEditingControlRigDirectly()))
						{
							break;
						}
					}
				}
			}

			if(ControlRig != nullptr)
			{
				// if we can't deal with this lazily, let's fall back to recreating all control shape actors
				RequestToRecreateControlShapeActors(ControlRig);
			}
			break;
		}
		case ERigHierarchyNotification::ControlDrivenListChanged:
		{
			if (!AreEditingControlRigDirectly())
			{
				// to synchronize the selection between the viewport / editmode and the details panel / sequencer
				// we re-select the control. during deselection we recover the previously set driven list
				// and then select the control again with the up2date list. this makes sure that the tracks
				// are correctly selected in sequencer to match what the proxy control is driving.
				if (FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InElement->GetKey()))
				{
					UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>();
					if(ControlProxy->IsSelected(ControlRig, ControlElement))
					{
						// reselect the control - to affect the details panel / sequencer
						if(URigHierarchyController* Controller = InHierarchy->GetController())
						{
							const FRigElementKey Key = ControlElement->GetKey();
							{
								// Restore the previously selected driven elements
								// so that we can deselect them accordingly.
								TGuardValue<TArray<FRigElementKey>> DrivenGuard(
									ControlElement->Settings.DrivenControls,
									ControlElement->Settings.PreviouslyDrivenControls);
								
								Controller->DeselectElement(Key);
							}

							// now select the proxy control again given the new driven list
							Controller->SelectElement(Key);
						}
					}
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementSelected:
		case ERigHierarchyNotification::ElementDeselected:
		{
			const FRigElementKey Key = InElement->GetKey();

			switch (InElement->GetType())
			{
				case ERigElementType::Bone:
            	case ERigElementType::Null:
            	case ERigElementType::Curve:
            	case ERigElementType::Control:
            	case ERigElementType::RigidBody:
            	case ERigElementType::Reference:
            	case ERigElementType::Connector:
            	case ERigElementType::Socket:
				{
					const bool bSelected = InNotif == ERigHierarchyNotification::ElementSelected;
					// users may select gizmo and control rig units, so we have to let them go through both of them if they do
						// first go through gizmo actor
					UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>();
					if (ControlRig == nullptr)
					{
						if (RuntimeControlRigs.Num() > 0)
						{
							ControlRig = RuntimeControlRigs[0].Get();
						}
					}
					if (ControlRig)
					{
						OnControlRigSelectedDelegate.Broadcast(ControlRig, Key, bSelected);
					}
					// if it's control
					if (Key.Type == ERigElementType::Control)
					{
						FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !AreEditingControlRigDirectly() && !GIsTransacting);
						if (!AreEditingControlRigDirectly())
						{
							ControlProxy->Modify();
						}
						
						AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig,Key.Name);
						if (ShapeActor)
						{
							ShapeActor->SetSelected(bSelected);

						}
						if (!AreEditingControlRigDirectly())
						{
							if (FRigControlElement* ControlElement = ControlRig->GetHierarchy()->Find<FRigControlElement>(Key))
							{
								ControlProxy->SelectProxy(ControlRig, ControlElement, bSelected);

								if(ControlElement->CanDriveControls())
								{
									const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();

									const TArray<FRigElementKey>& DrivenKeys = ControlElement->Settings.DrivenControls;
									for(const FRigElementKey& DrivenKey : DrivenKeys)
									{
										if (FRigControlElement* DrivenControl = ControlRig->GetHierarchy()->Find<FRigControlElement>(DrivenKey))
										{
											ControlProxy->SelectProxy(ControlRig, DrivenControl, bSelected);

											if (AControlRigShapeActor* DrivenShapeActor = GetControlShapeFromControlName(ControlRig,DrivenControl->GetFName()))
											{
												if(bSelected)
												{
													DrivenShapeActor->OverrideColor = Settings->DrivenControlColor;
												}
												else
												{
													DrivenShapeActor->OverrideColor = FLinearColor(0, 0, 0, 0);
												}
											}
										}
									}

									ControlRig->GetHierarchy()->ForEach<FRigControlElement>(
										[this, ControlRig, ControlElement, DrivenKeys, bSelected](FRigControlElement* AnimationChannelControl) -> bool
										{
											if(AnimationChannelControl->IsAnimationChannel())
											{
												if(const FRigControlElement* ParentControlElement =
													Cast<FRigControlElement>(ControlRig->GetHierarchy()->GetFirstParent(AnimationChannelControl)))
												{
													if(DrivenKeys.Contains(ParentControlElement->GetKey()) ||
														ParentControlElement->GetKey() == ControlElement->GetKey())
													{
														ControlProxy->SelectProxy(ControlRig, AnimationChannelControl, bSelected);
													}
												}
											}
											return true;
										}
									);
								}
							}
						}

					}
					bSelectionChanged = true;
		
					break;
				}
				default:
				{
					ensureMsgf(false, TEXT("Unsupported Type of RigElement: %s"), *Key.ToString());
					break;
				}
			}
		}
		case ERigHierarchyNotification::ParentWeightsChanged:
		{
			// TickManipulatableObjects(0.f);
			break;
		}
		case ERigHierarchyNotification::InteractionBracketOpened:
		case ERigHierarchyNotification::InteractionBracketClosed:
		default:
		{
			break;
		}
	}
}

void FControlRigEditMode::OnHierarchyModified_AnyThread(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(bSuspendHierarchyNotifs)
	{
		return;
	}

	if(bIsConstructionEventRunning)
	{
		return;
	}

	if(IsInGameThread())
	{
		OnHierarchyModified(InNotif, InHierarchy, InElement);
		return;
	}

	if (InNotif != ERigHierarchyNotification::ControlSettingChanged &&
		InNotif != ERigHierarchyNotification::ControlVisibilityChanged &&
		InNotif != ERigHierarchyNotification::ControlDrivenListChanged &&
		InNotif != ERigHierarchyNotification::ControlShapeTransformChanged &&
		InNotif != ERigHierarchyNotification::ElementSelected &&
		InNotif != ERigHierarchyNotification::ElementDeselected)
	{
		OnHierarchyModified(InNotif, InHierarchy, InElement);
		return;
	}
	
	FRigElementKey Key;
	if(InElement)
	{
		Key = InElement->GetKey();
	}

	TWeakObjectPtr<URigHierarchy> WeakHierarchy = InHierarchy;
	
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, InNotif, WeakHierarchy, Key]()
	{
		if(!WeakHierarchy.IsValid())
		{
			return;
		}
		if (const FRigBaseElement* Element = WeakHierarchy.Get()->Find(Key))
		{
			OnHierarchyModified(InNotif, WeakHierarchy.Get(), Element);
		}
		
	}, TStatId(), NULL, ENamedThreads::GameThread);
}

void FControlRigEditMode::OnControlModified(UControlRig* Subject, FRigControlElement* InControlElement, const FRigControlModifiedContext& Context)
{
	//this makes sure the details panel ui get's updated, don't remove
	const bool bModify = Context.SetKey != EControlRigSetKey::Never;
	ControlProxy->ProxyChanged(Subject, InControlElement, bModify);

	bPivotsNeedUpdate = true;
	
	/*
	FScopedTransaction ScopedTransaction(LOCTEXT("ModifyControlTransaction", "Modify Control"),!GIsTransacting && Context.SetKey != EControlRigSetKey::Never);
	ControlProxy->Modify();
	RecalcPivotTransform();

	if (UControlRig* ControlRig = static_cast<UControlRig*>(Subject))
	{
		FTransform ComponentTransform = GetHostingSceneComponentTransform();
		if (AControlRigShapeActor* const* Actor = GizmoToControlMap.FindKey(InControl.Index))
		{
			TickControlShape(*Actor, ComponentTransform);
		}
	}
	*/
}

void FControlRigEditMode::OnPreConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	bIsConstructionEventRunning = true;
}

void FControlRigEditMode::OnPostConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	bIsConstructionEventRunning = false;

	const int32 RigIndex = RuntimeControlRigs.Find(InRig);
	if(!LastHierarchyHash.IsValidIndex(RigIndex) || !LastShapeLibraryHash.IsValidIndex(RigIndex))
	{
		return;
	}
	
	const int32 HierarchyHash = InRig->GetHierarchy()->GetTopologyHash(false, true);
	const int32 ShapeLibraryHash = InRig->GetShapeLibraryHash();
	if((LastHierarchyHash[RigIndex] != HierarchyHash) ||
		(LastShapeLibraryHash[RigIndex] != ShapeLibraryHash))
	{
		LastHierarchyHash[RigIndex] = HierarchyHash;
		LastShapeLibraryHash[RigIndex] = ShapeLibraryHash;

		auto Task = [this, InRig]()
		{
			RequestToRecreateControlShapeActors(InRig);
			RecreateControlShapeActors();
			HandleSelectionChanged();
		};
				
		if(IsInGameThread())
		{
			Task();
		}
		else
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([Task]()
			{
				Task();
			}, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

void FControlRigEditMode::OnWidgetModeChanged(UE::Widget::EWidgetMode InWidgetMode)
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	if (Settings && Settings->bCoordSystemPerWidgetMode)
	{
		TGuardValue<bool> ReentrantGuardSelf(bIsChangingCoordSystem, true);

		FEditorModeTools* ModeManager = GetModeManager();
		int32 WidgetMode = (int32)ModeManager->GetWidgetMode();
		if (WidgetMode >= 0 && WidgetMode < CoordSystemPerWidgetMode.Num())
		{
			ModeManager->SetCoordSystem(CoordSystemPerWidgetMode[WidgetMode]);
		}
	}
}

void FControlRigEditMode::OnCoordSystemChanged(ECoordSystem InCoordSystem)
{
	TGuardValue<bool> ReentrantGuardSelf(bIsChangingCoordSystem, true);

	FEditorModeTools* ModeManager = GetModeManager();
	int32 WidgetMode = (int32)ModeManager->GetWidgetMode();
	ECoordSystem CoordSystem = ModeManager->GetCoordSystem();
	if (WidgetMode >= 0 && WidgetMode < CoordSystemPerWidgetMode.Num())
	{
		CoordSystemPerWidgetMode[WidgetMode] = CoordSystem;
	}
}

bool FControlRigEditMode::CanChangeControlShapeTransform()
{
	if (AreEditingControlRigDirectly())
	{
		for (TWeakObjectPtr<UControlRig> RuntimeRigPtr : RuntimeControlRigs)
		{
			if (UControlRig* ControlRig = RuntimeRigPtr.Get())
			{
				TArray<FRigElementKey> SelectedRigElements = GetSelectedRigElements(ControlRig);
				// do not allow multi-select
				if (SelectedRigElements.Num() == 1)
				{
					if (AreRigElementsSelected(ValidControlTypeMask(),ControlRig))
					{
						// only enable for a Control with Gizmo enabled and visible
						if (FRigControlElement* ControlElement = ControlRig->GetHierarchy()->Find<FRigControlElement>(SelectedRigElements[0]))
						{
							if (ControlElement->Settings.IsVisible())
							{
								if (AControlRigShapeActor* ShapeActor = GetControlShapeFromControlName(ControlRig,SelectedRigElements[0].Name))
								{
									if (ensure(ShapeActor->IsSelected()))
									{
										return true;
									}
								}
							}
						}
						
					}
				}
			}
		}
	}

	return false;
}

void FControlRigEditMode::SetControlShapeTransform(
	const AControlRigShapeActor* InShapeActor,
	const FTransform& InGlobalTransform,
	const FTransform& InToWorldTransform,
	const FRigControlModifiedContext& InContext,
	const bool bPrintPython,
	const bool bFixEulerFlips) const
{
	UControlRig* ControlRig = InShapeActor->ControlRig.Get();
	if (!ControlRig)
	{
		return;
	}

	static constexpr bool bNotify = true, bUndo = true;
	if (AreEditingControlRigDirectly())
	{
		// assumes it's attached to actor
		ControlRig->SetControlGlobalTransform(
			InShapeActor->ControlName, InGlobalTransform, bNotify, InContext, bUndo, bPrintPython, bFixEulerFlips);
		return;
	}

	auto EvaluateRigIfAdditive = [ControlRig]()
	{
		// skip compensation and evaluate the rig to force notifications: auto-key and constraints updates (among others) are based on
		// UControlRig::OnControlModified being broadcast but this only happens on evaluation for additive rigs.
		// constraint compensation is disabled while manipulating in that case to avoid re-entrant evaluations 
		if (ControlRig->IsAdditive())
		{
			TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
			ControlRig->Evaluate_AnyThread();
		}
	};
	
	// find the last constraint in the stack (this could be cached on mouse press)
	TArray< TWeakObjectPtr<UTickableConstraint> > Constraints;
	FTransformConstraintUtils::GetParentConstraints(ControlRig->GetWorld(), InShapeActor, Constraints);

	const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(Constraints);
	const bool bNeedsConstraintPostProcess = Constraints.IsValidIndex(LastActiveIndex);
	
	// set the global space, assumes it's attached to actor
	// no need to compensate for constraints here, this will be done after when setting the control in the constraint space
	{
		TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
		ControlRig->SetControlGlobalTransform(
			InShapeActor->ControlName, InGlobalTransform, bNotify, InContext, bUndo, bPrintPython, bFixEulerFlips);
		EvaluateRigIfAdditive();
	}

	if (!bNeedsConstraintPostProcess)
	{
		return;
	}
	
	// switch to constraint space
	const FTransform WorldTransform = InGlobalTransform * InToWorldTransform;
	FTransform LocalTransform = ControlRig->GetControlLocalTransform(InShapeActor->ControlName);

	const TOptional<FTransform> RelativeTransform =
		FTransformConstraintUtils::GetConstraintsRelativeTransform(Constraints, LocalTransform, WorldTransform);
	if (RelativeTransform)
	{
		LocalTransform = *RelativeTransform; 
	}

	FRigControlModifiedContext Context = InContext;
	Context.bConstraintUpdate = false;
	
	ControlRig->SetControlLocalTransform(InShapeActor->ControlName, LocalTransform, bNotify, Context, bUndo, bFixEulerFlips);
	EvaluateRigIfAdditive();
	
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(ControlRig->GetWorld());
	Controller.EvaluateAllConstraints();
}

FTransform FControlRigEditMode::GetControlShapeTransform(const AControlRigShapeActor* ShapeActor)
{
	if (const UControlRig* ControlRig = ShapeActor->ControlRig.Get())
	{
		return ControlRig->GetControlGlobalTransform(ShapeActor->ControlName);
	}
	return FTransform::Identity;
}

void FControlRigEditMode::MoveControlShape(AControlRigShapeActor* ShapeActor, const bool bTranslation, FVector& InDrag,
	const bool bRotation, FRotator& InRot, const bool bScale, FVector& InScale, const FTransform& ToWorldTransform,
	bool bUseLocal, bool bCalcLocal, FTransform& InOutLocal)
{
	bool bTransformChanged = false;

	auto RotatorToStr = [](const FRotator& InRotator)
	{
		FRigPreferredEulerAngles EulerAngles;
		EulerAngles.SetRotator(InRotator);
		return EulerAngles.Current.ToString();
	};

	auto UpdatePreferredEulerAngles = [ShapeActor, bRotation, InRot, RotatorToStr](UControlRig* InControlRig)
	{
		if(bRotation)
		{
			if(FRigControlElement* ControlElement = InControlRig->GetHierarchy()->Find<FRigControlElement>(ShapeActor->GetElementKey()))
			{
				//if(ControlElement->Settings.bUsePreferredRotationOrder) always set rotation order since
				//sequencer depends upon it
				{
					FRotator Rot = ControlElement->PreferredEulerAngles.GetRotator();

					// Split the current rotation between winding and remainder
					FRotator CurrentRotWind, CurrentRotRem;
					Rot.GetWindingAndRemainder(CurrentRotWind, CurrentRotRem);

					// Apply the delta to the current remainder, and normalize
					FRotator NewRotRem = CurrentRotRem + InRot;
					NewRotRem.Normalize();

					// Add the current winding to the new remainder
					const FRotator Final = CurrentRotWind + NewRotRem;
					
					InControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Final, false, false);
				}
			}
		}
	};
	
	//first case is where we do all controls by the local diff.
	if (bUseLocal)
	{
		if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
		{
			FRigControlModifiedContext Context;
			Context.EventName = FRigUnit_BeginExecution::EventName;
			FTransform CurrentLocalTransform = ControlRig->GetControlLocalTransform(ShapeActor->ControlName);
			if (bRotation)
			{

				FQuat CurrentRotation = CurrentLocalTransform.GetRotation();
				CurrentRotation = (CurrentRotation * InOutLocal.GetRotation());
				CurrentLocalTransform.SetRotation(CurrentRotation);
				bTransformChanged = true;
			}

			if (bTranslation)
			{
				FVector CurrentLocation = CurrentLocalTransform.GetLocation();
				CurrentLocation = CurrentLocation + InOutLocal.GetLocation();
				CurrentLocalTransform.SetLocation(CurrentLocation);
				bTransformChanged = true;
			}

			if (bTransformChanged)
			{
				ControlRig->InteractionType = InteractionType;
				ControlRig->ElementsBeingInteracted.AddUnique(ShapeActor->GetElementKey());
				
				ControlRig->SetControlLocalTransform(ShapeActor->ControlName, CurrentLocalTransform,true, FRigControlModifiedContext(), true, /*fix eulers*/ true);
				//UpdatePreferredEulerAngles(ControlRig);

				FTransform CurrentTransform  = ControlRig->GetControlGlobalTransform(ShapeActor->ControlName);			// assumes it's attached to actor
				CurrentTransform = ToWorldTransform * CurrentTransform;

				// make the transform relative to the offset transform again.
				// first we'll make it relative to the offset used at the time of starting the drag
				// and then we'll make it absolute again based on the current offset. these two can be
				// different if we are interacting on a control on an animated character
				CurrentTransform = CurrentTransform.GetRelativeTransform(ShapeActor->OffsetTransform);
				CurrentTransform = CurrentTransform * ControlRig->GetHierarchy()->GetGlobalControlOffsetTransform(ShapeActor->GetElementKey(), false);
				
				ShapeActor->SetGlobalTransform(CurrentTransform);

				ControlRig->Evaluate_AnyThread();
			}
		}
	}
	if(!bTransformChanged) //not local or doing scale.
	{
		FTransform CurrentTransform = GetControlShapeTransform(ShapeActor) * ToWorldTransform;

		if (bRotation)
		{
			FQuat CurrentRotation = CurrentTransform.GetRotation();
			CurrentRotation = (InRot.Quaternion() * CurrentRotation);
			CurrentTransform.SetRotation(CurrentRotation);
			bTransformChanged = true;
		}

		if (bTranslation)
		{
			FVector CurrentLocation = CurrentTransform.GetLocation();
			CurrentLocation = CurrentLocation + InDrag;
			CurrentTransform.SetLocation(CurrentLocation);
			bTransformChanged = true;
		}

		if (bScale)
		{
			FVector CurrentScale = CurrentTransform.GetScale3D();
			CurrentScale = CurrentScale + InScale;
			CurrentTransform.SetScale3D(CurrentScale);
			bTransformChanged = true;
		}

		if (bTransformChanged)
		{
			if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
			{
				ControlRig->InteractionType = InteractionType;
				ControlRig->ElementsBeingInteracted.AddUnique(ShapeActor->GetElementKey());

				FTransform NewTransform = CurrentTransform.GetRelativeTransform(ToWorldTransform);
				FRigControlModifiedContext Context;
				Context.EventName = FRigUnit_BeginExecution::EventName;
				Context.bConstraintUpdate = true;
				if (bCalcLocal)
				{
					InOutLocal = ControlRig->GetControlLocalTransform(ShapeActor->ControlName);
				}

				bool bPrintPythonCommands = false;
				if (UWorld* World = ControlRig->GetWorld())
				{
					bPrintPythonCommands = World->IsPreviewWorld();
				}

				bool bIsTransientControl = false;
				if(const FRigControlElement* ControlElement = ControlRig->FindControl(ShapeActor->ControlName))
				{
					bIsTransientControl = ControlElement->Settings.bIsTransientControl;
				}

				// if we are operating on a PIE instance which is playing we need to reapply the input pose
				// since the hierarchy will also have been brought into the solved pose. by reapplying the
				// input pose we avoid double transformation / double forward solve results.
				if(bIsTransientControl && ControlRig->GetWorld()->IsPlayInEditor() && !ControlRig->GetWorld()->IsPaused())
				{
					ControlRig->GetHierarchy()->SetPose(ControlRig->InputPoseOnDebuggedRig);
				}
				
				ControlRig->Evaluate_AnyThread();
				SetControlShapeTransform(ShapeActor, NewTransform, ToWorldTransform, Context, bPrintPythonCommands, /*fix flips*/ true);
				//UpdatePreferredEulerAngles(ControlRig);
				NotifyDrivenControls(ControlRig, ShapeActor->GetElementKey());
				if(const FRigControlElement* ControlElement = ControlRig->FindControl(ShapeActor->ControlName))
				{
					if(!bIsTransientControl)
					{
						ControlRig->Evaluate_AnyThread();
					}
				}
				ShapeActor->SetGlobalTransform(CurrentTransform);
				if (bCalcLocal)
				{
					FTransform NewLocal = ControlRig->GetControlLocalTransform(ShapeActor->ControlName);
					InOutLocal = NewLocal.GetRelativeTransform(InOutLocal);
				}

			}
		}
	}
}

void FControlRigEditMode::ChangeControlShapeTransform(AControlRigShapeActor* ShapeActor, const bool bTranslation, FVector& InDrag,
	const bool bRotation, FRotator& InRot, const bool bScale, FVector& InScale, const FTransform& ToWorldTransform)
{
	bool bTransformChanged = false; 

	FTransform CurrentTransform = FTransform::Identity;

	if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
	{
		if (FRigControlElement* ControlElement = ControlRig->GetHierarchy()->Find<FRigControlElement>(ShapeActor->GetElementKey()))
		{
			CurrentTransform = ControlRig->GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentGlobal);
			CurrentTransform = CurrentTransform * ToWorldTransform;
		}
	}

	if (bRotation)
	{
		FQuat CurrentRotation = CurrentTransform.GetRotation();
		CurrentRotation = (InRot.Quaternion() * CurrentRotation);
		CurrentTransform.SetRotation(CurrentRotation);
		bTransformChanged = true;
	}

	if (bTranslation)
	{
		FVector CurrentLocation = CurrentTransform.GetLocation();
		CurrentLocation = CurrentLocation + InDrag;
		CurrentTransform.SetLocation(CurrentLocation);
		bTransformChanged = true;
	}

	if (bScale)
	{
		FVector CurrentScale = CurrentTransform.GetScale3D();
		CurrentScale = CurrentScale + InScale;
		CurrentTransform.SetScale3D(CurrentScale);
		bTransformChanged = true;
	}

	if (bTransformChanged)
	{
		if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
		{

			FTransform NewTransform = CurrentTransform.GetRelativeTransform(ToWorldTransform);
			FRigControlModifiedContext Context;

			if (FRigControlElement* ControlElement = ControlRig->GetHierarchy()->Find<FRigControlElement>(ShapeActor->GetElementKey()))
			{
				// do not setup undo for this first step since it is just used to calculate the local transform
				ControlRig->GetHierarchy()->SetControlShapeTransform(ControlElement, NewTransform, ERigTransformType::CurrentGlobal, false);
				FTransform CurrentLocalShapeTransform = ControlRig->GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
				// this call should trigger an instance-to-BP update in ControlRigEditor
				ControlRig->GetHierarchy()->SetControlShapeTransform(ControlElement, CurrentLocalShapeTransform, ERigTransformType::InitialLocal, true);

				FTransform MeshTransform = FTransform::Identity;
				FTransform ShapeTransform = CurrentLocalShapeTransform;
				if (const FControlRigShapeDefinition* Gizmo = UControlRigShapeLibrary::GetShapeByName(ControlElement->Settings.ShapeName, ControlRig->GetShapeLibraries(), ControlRig->ShapeLibraryNameMap))
				{
					MeshTransform = Gizmo->Transform;
				}
				ShapeActor->StaticMeshComponent->SetRelativeTransform(MeshTransform * ShapeTransform);
			}
		}
	} 
}


bool FControlRigEditMode::ModeSupportedByShapeActor(const AControlRigShapeActor* ShapeActor, UE::Widget::EWidgetMode InMode) const
{
	if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
	{
		const FRigControlElement* ControlElement = ControlRig->FindControl(ShapeActor->ControlName);
		if (ControlElement)
		{
			if (bIsChangingControlShapeTransform)
			{
				return true;
			}

			if (IsSupportedControlType(ControlElement->Settings.ControlType))
			{
				switch (InMode)
				{
					case UE::Widget::WM_None:
						return true;
					case UE::Widget::WM_Rotate:
					{
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Rotator:
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							case ERigControlType::EulerTransform:
							{
								return true;
							}
							default:
							{
								break;
							}
						}
						break;
					}
					case UE::Widget::WM_Translate:
					{
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Float:
							case ERigControlType::Integer:
							case ERigControlType::Vector2D:
							case ERigControlType::Position:
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							case ERigControlType::EulerTransform:
							{
								return true;
							}
							default:
							{
								break;
							}
						}
						break;
					}
					case UE::Widget::WM_Scale:
					{
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Scale:
							case ERigControlType::ScaleFloat:
							case ERigControlType::Transform:
							case ERigControlType::EulerTransform:
							{
								return true;
							}
							default:
							{
								break;
							}
						}
						break;
					}
					case UE::Widget::WM_TranslateRotateZ:
					{
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							case ERigControlType::EulerTransform:
							{
								return true;
							}
							default:
							{
								break;
							}
						}
						break;
					}
				}
			}
		}
	}
	return false;
}

bool FControlRigEditMode::IsControlRigSkelMeshVisible(UControlRig* ControlRig) const
{
	if (IsInLevelEditor())
	{
		if (ControlRig)
		{
			if (USceneComponent* SceneComponent = GetHostingSceneComponent(ControlRig))
			{
				const AActor* Actor = SceneComponent->GetTypedOuter<AActor>();
				return Actor ? (Actor->IsHiddenEd() == false && SceneComponent->IsVisibleInEditor()) : SceneComponent->IsVisibleInEditor();
			}
		}
		return false;
	}
	return true;
}

void FControlRigEditMode::TickControlShape(AControlRigShapeActor* ShapeActor, const FTransform& ComponentTransform) const
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	if (ShapeActor)
	{
		if (UControlRig* ControlRig = ShapeActor->ControlRig.Get())
		{
			const FTransform Transform = ControlRig->GetControlGlobalTransform(ShapeActor->ControlName);
			ShapeActor->SetActorTransform(Transform * ComponentTransform);

			if (FRigControlElement* ControlElement = ControlRig->FindControl(ShapeActor->ControlName))
			{
				const bool bControlsHiddenInViewport = Settings->bHideControlShapes || !ControlRig->GetControlsVisible()
					|| (IsControlRigSkelMeshVisible(ControlRig) == false);

				bool bIsVisible = ControlElement->Settings.IsVisible();
				bool bRespectVisibilityForSelection = true; 

				if(!bControlsHiddenInViewport)
				{
					if(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
					{					
						bRespectVisibilityForSelection = false;
						if(Settings->bShowAllProxyControls)
						{
							bIsVisible = true;
						}
					}
				}
				
				ShapeActor->SetShapeColor(ShapeActor->OverrideColor.A < SMALL_NUMBER ?
					ControlElement->Settings.ShapeColor : ShapeActor->OverrideColor);
				ShapeActor->SetIsTemporarilyHiddenInEditor(
					!bIsVisible ||
					bControlsHiddenInViewport);
				
				ShapeActor->SetSelectable(
					ControlElement->Settings.IsSelectable(bRespectVisibilityForSelection));
			}
		}
	}
}

AControlRigShapeActor* FControlRigEditMode::GetControlShapeFromControlName(UControlRig* InControlRig,const FName& ControlName) const
{
	const auto* ShapeActors = ControlRigShapeActors.Find(InControlRig);
	if (ShapeActors)
	{
		for (AControlRigShapeActor* ShapeActor : *ShapeActors)
		{
			if (ShapeActor->ControlName == ControlName)
			{
				return ShapeActor;
			}
		}
	}

	return nullptr;
}

void FControlRigEditMode::AddControlRigInternal(UControlRig* InControlRig)
{
	RuntimeControlRigs.AddUnique(InControlRig);
	LastHierarchyHash.Add(INDEX_NONE);
	LastShapeLibraryHash.Add(INDEX_NONE);

	InControlRig->SetControlsVisible(true);
	InControlRig->PostInitInstanceIfRequired();
	InControlRig->GetHierarchy()->OnModified().RemoveAll(this);
	InControlRig->GetHierarchy()->OnModified().AddSP(this, &FControlRigEditMode::OnHierarchyModified_AnyThread);
	InControlRig->OnPostConstruction_AnyThread().AddSP(this, &FControlRigEditMode::OnPostConstruction_AnyThread);

	//needed for the control rig track editor delegates to get hooked up
	if (WeakSequencer.IsValid())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		Sequencer->ObjectImplicitlyAdded(InControlRig);
	}
	OnControlRigAddedOrRemovedDelegate.Broadcast(InControlRig, true);

	UpdateSelectabilityOnSkeletalMeshes(InControlRig, !bShowControlsAsOverlay);
}

TArrayView<const TWeakObjectPtr<UControlRig>> FControlRigEditMode::GetControlRigs() const
{
	return MakeArrayView(RuntimeControlRigs);
}

TArrayView<TWeakObjectPtr<UControlRig>> FControlRigEditMode::GetControlRigs() 
{
	return MakeArrayView(RuntimeControlRigs);
}

TArray<UControlRig*> FControlRigEditMode::GetControlRigsArray(bool bIsVisible)
{
	TArray < UControlRig*> ControlRigs;
	for (TWeakObjectPtr<UControlRig> ControlRigPtr : RuntimeControlRigs)
	{
		if (ControlRigPtr.IsValid() && ControlRigPtr.Get() != nullptr && (bIsVisible == false ||ControlRigPtr.Get()->GetControlsVisible()))
		{
			ControlRigs.Add(ControlRigPtr.Get());
		}
	}
	return ControlRigs;
}

TArray<const UControlRig*> FControlRigEditMode::GetControlRigsArray(bool bIsVisible) const
{
	TArray<const UControlRig*> ControlRigs;
	for (const TWeakObjectPtr<UControlRig> ControlRigPtr : RuntimeControlRigs)
	{
		if (ControlRigPtr.IsValid() && ControlRigPtr.Get() != nullptr && (bIsVisible == false || ControlRigPtr.Get()->GetControlsVisible()))
		{
			ControlRigs.Add(ControlRigPtr.Get());
		}
	}
	return ControlRigs;
}

void FControlRigEditMode::RemoveControlRig(UControlRig* InControlRig)
{
	if (InControlRig == nullptr)
	{
		return;
	}
	InControlRig->ControlModified().RemoveAll(this);
	InControlRig->GetHierarchy()->OnModified().RemoveAll(this);
	InControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
	InControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
	
	int32 Index = RuntimeControlRigs.Find(InControlRig);
	TStrongObjectPtr<UControlRigEditModeDelegateHelper>* DelegateHelper = DelegateHelpers.Find(InControlRig);
	if (DelegateHelper && DelegateHelper->IsValid())
	{
		DelegateHelper->Get()->RemoveDelegates();
		DelegateHelper->Reset();
		DelegateHelpers.Remove(InControlRig);
	}
	DestroyShapesActors(InControlRig);
	if (RuntimeControlRigs.IsValidIndex(Index))
	{
		RuntimeControlRigs.RemoveAt(Index);
	}
	if (LastHierarchyHash.IsValidIndex(Index))
	{
		LastHierarchyHash.RemoveAt(Index);
	}
	if (LastShapeLibraryHash.IsValidIndex(Index))
	{
		LastShapeLibraryHash.RemoveAt(Index);
	}

	//needed for the control rig track editor delegates to get removed
	if (WeakSequencer.IsValid())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		Sequencer->ObjectImplicitlyRemoved(InControlRig);
	}
	OnControlRigAddedOrRemovedDelegate.Broadcast(InControlRig, false);
	
	UpdateSelectabilityOnSkeletalMeshes(InControlRig, true);
}

void FControlRigEditMode::TickManipulatableObjects(float DeltaTime)
{
	for (TWeakObjectPtr<UControlRig> RuntimeRigPtr : RuntimeControlRigs)
	{
		if (UControlRig* ControlRig = RuntimeRigPtr.Get())
		{
			// tick skeletalmeshcomponent, that's how they update their transform from rig change
			USceneComponent* SceneComponent = GetHostingSceneComponent(ControlRig);
			if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(SceneComponent))
			{
				ControlRigComponent->Update();
			}
			else if (USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(SceneComponent))
			{
				// NOTE: we have to update/tick ALL children skeletal mesh components here because user can attach
				// additional skeletal meshes via the "Copy Pose from Mesh" node.
				//
				// If this is left up to the viewport tick(), the attached meshes will render before they get the latest
				// parent bone transforms resulting in a visible lag on all attached components.

				// get hierarchically ordered list of ALL child skeletal mesh components (recursive)
				const AActor* ThisActor = MeshComponent->GetOwner();
				TArray<USceneComponent*> ChildrenComponents;
				MeshComponent->GetChildrenComponents(true, ChildrenComponents);
				TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshesToUpdate;
				SkeletalMeshesToUpdate.Add(MeshComponent);
				for (USceneComponent* ChildComponent : ChildrenComponents)
				{
					if (USkeletalMeshComponent* ChildMeshComponent = Cast<USkeletalMeshComponent>(ChildComponent))
					{
						if (ThisActor == ChildMeshComponent->GetOwner())
						{
							SkeletalMeshesToUpdate.Add(ChildMeshComponent);
						}
					}
				}

				// update pose of all children skeletal meshes in this actor
				for (USkeletalMeshComponent* SkeletalMeshToUpdate : SkeletalMeshesToUpdate)
				{
					// "Copy Pose from Mesh" requires AnimInstance::PreUpdate() to copy the parent bone transforms.
					// have to TickAnimation() to ensure that PreUpdate() is called on all anim instances
					SkeletalMeshToUpdate->TickAnimation(0.0f, false);
					SkeletalMeshToUpdate->RefreshBoneTransforms();
					SkeletalMeshToUpdate->RefreshFollowerComponents	();
					SkeletalMeshToUpdate->UpdateComponentToWorld();
					SkeletalMeshToUpdate->FinalizeBoneTransform();
					SkeletalMeshToUpdate->MarkRenderTransformDirty();
					SkeletalMeshToUpdate->MarkRenderDynamicDataDirty();
				}
			}
		}
	}
	PostPoseUpdate();
}


void FControlRigEditMode::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// if world gets cleaned up first, we destroy gizmo actors
	if (WorldPtr == World)
	{
		DestroyShapesActors(nullptr);
	}
}

void FControlRigEditMode::OnEditorClosed()
{
	ControlRigShapeActors.Reset();
	ControlRigsToRecreate.Reset();
}

FControlRigEditMode::FMarqueeDragTool::FMarqueeDragTool()
	: bIsDeletingDragTool(false)
{
}

bool FControlRigEditMode::FMarqueeDragTool::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return (DragTool.IsValid() && InViewportClient->GetCurrentWidgetAxis() == EAxisList::None);
}

bool FControlRigEditMode::FMarqueeDragTool::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (!bIsDeletingDragTool)
	{
		// Ending the drag tool may pop up a modal dialog which can cause unwanted reentrancy - protect against this.
		TGuardValue<bool> RecursionGuard(bIsDeletingDragTool, true);

		// Delete the drag tool if one exists.
		if (DragTool.IsValid())
		{
			if (DragTool->IsDragging())
			{
				DragTool->EndDrag();
			}
			DragTool.Reset();
			return true;
		}
	}
	
	return false;
}

void FControlRigEditMode::FMarqueeDragTool::MakeDragTool(FEditorViewportClient* InViewportClient)
{
	DragTool.Reset();
	if (InViewportClient->IsOrtho())
	{
		DragTool = MakeShareable( new FDragTool_ActorBoxSelect(InViewportClient) );
	}
	else
	{
		DragTool = MakeShareable( new FDragTool_ActorFrustumSelect(InViewportClient) );
	}
}

bool FControlRigEditMode::FMarqueeDragTool::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (DragTool.IsValid() == false || InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		return false;
	}
	if (DragTool->IsDragging() == false)
	{
		int32 InX = InViewport->GetMouseX();
		int32 InY = InViewport->GetMouseY();
		FVector2D Start(InX, InY);

		DragTool->StartDrag(InViewportClient, GEditor->ClickLocation,Start);
	}
	const bool bUsingDragTool = UsingDragTool();
	if (bUsingDragTool == false)
	{
		return false;
	}

	DragTool->AddDelta(InDrag);
	return true;
}

bool FControlRigEditMode::FMarqueeDragTool::UsingDragTool() const
{
	return DragTool.IsValid() && DragTool->IsDragging();
}

void FControlRigEditMode::FMarqueeDragTool::Render3DDragTool(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (DragTool.IsValid())
	{
		DragTool->Render3D(View, PDI);
	}
}

void FControlRigEditMode::FMarqueeDragTool::RenderDragTool(const FSceneView* View, FCanvas* Canvas)
{
	if (DragTool.IsValid())
	{
		DragTool->Render(View, Canvas);
	}
}

void FControlRigEditMode::DestroyShapesActors(UControlRig* ControlRig)
{
	if (ControlRig == nullptr)
	{
		for(auto& ShapeActors: ControlRigShapeActors)
		{
			for (AControlRigShapeActor* ShapeActor : ShapeActors.Value)
			{
				UWorld* World = ShapeActor->GetWorld();
				if (World)
				{
					ShapeActor->UnregisterAllComponents();
					if (ShapeActor->GetAttachParentActor())
					{
						ShapeActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
					}
					World->EditorDestroyActor(ShapeActor, true);
				}
			}
		}
		ControlRigShapeActors.Reset();
		ControlRigsToRecreate.Reset();
		if (OnWorldCleanupHandle.IsValid())
		{
			FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
		}
	}
	else
	{
		ControlRigsToRecreate.Remove(ControlRig);
		const auto* ShapeActors = ControlRigShapeActors.Find(ControlRig);
		if (ShapeActors)
		{
			for (AControlRigShapeActor* ShapeActor : *ShapeActors)
			{
				UWorld* World = ShapeActor->GetWorld();
				if (World)
				{
					if (ShapeActor->GetAttachParentActor())
					{
						ShapeActor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
					}
					World->EditorDestroyActor(ShapeActor,true);
				}
			}
			ControlRigShapeActors.Remove(ControlRig);
		}
	}
}

USceneComponent* FControlRigEditMode::GetHostingSceneComponent(const UControlRig* ControlRig) const
{
	if (ControlRig == nullptr && GetControlRigs().Num() > 0)
	{
		ControlRig = GetControlRigs()[0].Get();
	}
	if (ControlRig)
	{
		TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding();
		if (ObjectBinding.IsValid())
		{
			if (USceneComponent* BoundSceneComponent = Cast<USceneComponent>(ObjectBinding->GetBoundObject()))
			{
				return BoundSceneComponent;
			}
			else if (USkeleton* BoundSkeleton = Cast<USkeleton>(ObjectBinding->GetBoundObject()))
			{
				// Bound to a Skeleton means we are previewing an Animation Sequence
				if (WorldPtr)
				{
					TObjectPtr<AActor>* PreviewActor = WorldPtr->PersistentLevel->Actors.FindByPredicate([](TObjectPtr<AActor> Actor)
					{
						return Actor && Actor->GetClass() == AAnimationEditorPreviewActor::StaticClass();
					});

					if(PreviewActor)
					{
						if (UDebugSkelMeshComponent* DebugComponent = (*PreviewActor)->FindComponentByClass<UDebugSkelMeshComponent>())
						{
							return DebugComponent;
						}
					}
				}
			}			
		}
		
	}	

	return nullptr;
}

FTransform FControlRigEditMode::GetHostingSceneComponentTransform(const UControlRig* ControlRig) const
{
	// we care about this transform only in the level,
	// since in the control rig editor the debug skeletal mesh component
	// is set at identity anyway.
	if(IsInLevelEditor())
	{
		if (ControlRig == nullptr && GetControlRigs().Num() > 0)
		{
			ControlRig = GetControlRigs()[0].Get();
		}

		USceneComponent* HostingComponent = GetHostingSceneComponent(ControlRig);
		return HostingComponent ? HostingComponent->GetComponentTransform() : FTransform::Identity;
	}
	return FTransform::Identity;
}

void FControlRigEditMode::OnPoseInitialized()
{
	OnAnimSystemInitializedDelegate.Broadcast();
}

void FControlRigEditMode::PostPoseUpdate() const
{
	for (auto& ShapeActors : ControlRigShapeActors)
	{
		FTransform ComponentTransform = FTransform::Identity;
		if (!AreEditingControlRigDirectly())
		{
			ComponentTransform = GetHostingSceneComponentTransform(ShapeActors.Key);
		}
		for (AControlRigShapeActor* ShapeActor : ShapeActors.Value)
		{
			TickControlShape(ShapeActor, ComponentTransform);	
		}
	}

}

void FControlRigEditMode::NotifyDrivenControls(UControlRig* InControlRig, const FRigElementKey& InKey)
{
	// if we are changing a proxy control - we also need to notify the change for the driven controls
	if (FRigControlElement* ControlElement = InControlRig->GetHierarchy()->Find<FRigControlElement>(InKey))
	{
		if(ControlElement->CanDriveControls())
		{
			FRigControlModifiedContext Context;
			Context.EventName = FRigUnit_BeginExecution::EventName;

			for(const FRigElementKey& DrivenKey : ControlElement->Settings.DrivenControls)
			{
				if(DrivenKey.Type == ERigElementType::Control)
				{
					const FTransform DrivenTransform = InControlRig->GetControlLocalTransform(DrivenKey.Name);
					InControlRig->SetControlLocalTransform(DrivenKey.Name, DrivenTransform, true, Context, false /*undo*/, true/* bFixEulerFlips*/);
				}
			}
		}
	}
}

void FControlRigEditMode::UpdateSelectabilityOnSkeletalMeshes(UControlRig* InControlRig, bool bEnabled)
{
	if(const USceneComponent* HostingComponent = GetHostingSceneComponent(InControlRig))
	{
		if(const AActor* HostingOwner = HostingComponent->GetOwner())
		{
			for(UActorComponent* ActorComponent : HostingOwner->GetComponents())
			{
				if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorComponent))
				{
					SkeletalMeshComponent->bSelectable = bEnabled;
					SkeletalMeshComponent->MarkRenderStateDirty();
				}
				else if(UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(ActorComponent))
				{
					StaticMeshComponent->bSelectable = bEnabled;
					StaticMeshComponent->MarkRenderStateDirty();
				}
			}
		}
	}
}

void FControlRigEditMode::SetOnlySelectRigControls(bool Val)
{
	UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
	Settings->bOnlySelectRigControls = Val;
}

bool FControlRigEditMode::GetOnlySelectRigControls()const
{
	const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>();
	return Settings->bOnlySelectRigControls;
}

/**
* FDetailKeyFrameCacheAndHandler
*/

bool FDetailKeyFrameCacheAndHandler::IsPropertyKeyable(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	InPropertyHandle.GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		if (UControlRigControlsProxy* Proxy = Cast< UControlRigControlsProxy>(OuterObjects[0]))
		{
			for (const TPair<TWeakObjectPtr<UControlRig>, FControlRigProxyItem>& Items : Proxy->ControlRigItems)
			{
				if (UControlRig* ControlRig = Items.Value.ControlRig.Get())
				{
					for (const FName& CName : Items.Value.ControlElements)
					{
						if (FRigControlElement* ControlElement = Items.Value.GetControlElement(CName))
						{
							if (!ControlRig->GetHierarchy()->IsAnimatable(ControlElement))
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	if (InObjectClass
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyTransform::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyLocation::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyRotation::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyScale::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyVector2D::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyFloat::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyBool::StaticClass())
		&& InObjectClass->IsChildOf(UAnimDetailControlsProxyInteger::StaticClass())
		)
	{
		return true;
	}

	if ((InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyTransform::StaticClass()) && InPropertyHandle.GetProperty())
		&& (InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Location) ||
		InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Rotation) ||
		InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Scale)))
	{
		return true;
	}


	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyLocation::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyLocation, Location))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyRotation::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyRotation, Rotation))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyScale::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyScale, Scale))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyVector2D::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyVector2D, Vector2D))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyInteger::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyInteger, Integer))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyBool::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyBool, Bool))
	{
		return true;
	}

	if (InObjectClass && InObjectClass->IsChildOf(UAnimDetailControlsProxyFloat::StaticClass()) && InPropertyHandle.GetProperty()
		&& InPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyFloat, Float))
	{
		return true;
	}
	FCanKeyPropertyParams CanKeyPropertyParams(InObjectClass, InPropertyHandle);
	if (WeakSequencer.IsValid() && WeakSequencer.Pin()->CanKeyProperty(CanKeyPropertyParams))
	{
		return true;
	}

	return false;
}

bool FDetailKeyFrameCacheAndHandler::IsPropertyKeyingEnabled() const
{
	if (WeakSequencer.IsValid() &&  WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		return true;
	}

	return false;
}

bool FDetailKeyFrameCacheAndHandler::IsPropertyAnimated(const IPropertyHandle& PropertyHandle, UObject* ParentObject) const
{
	if (WeakSequencer.IsValid() && WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		constexpr bool bCreateHandleIfMissing = false;
		FGuid ObjectHandle = Sequencer->GetHandleToObject(ParentObject, bCreateHandleIfMissing);
		if (ObjectHandle.IsValid())
		{
			UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
			FProperty* Property = PropertyHandle.GetProperty();
			TSharedRef<FPropertyPath> PropertyPath = FPropertyPath::CreateEmpty();
			PropertyPath->AddProperty(FPropertyInfo(Property));
			FName PropertyName(*PropertyPath->ToString(TEXT(".")));
			TSubclassOf<UMovieSceneTrack> TrackClass; //use empty @todo find way to get the UMovieSceneTrack from the Property type.
			return MovieScene->FindTrack(TrackClass, ObjectHandle, PropertyName) != nullptr;
		}
	}
	return false;
}

void FDetailKeyFrameCacheAndHandler::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	if (WeakSequencer.IsValid() && !WeakSequencer.Pin()->IsAllowedToChange())
	{
		return;
	}
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects(Objects);
	for (UObject* Object : Objects)
	{
		UControlRigControlsProxy* Proxy = Cast< UControlRigControlsProxy>(Object);
		if (Proxy)
		{
			Proxy->SetKey(SequencerPtr, KeyedPropertyHandle);
		}
	}
}

EPropertyKeyedStatus FDetailKeyFrameCacheAndHandler::GetPropertyKeyedStatus(const IPropertyHandle& PropertyHandle) const
{
	if (WeakSequencer.IsValid() == false)
	{
		return EPropertyKeyedStatus::NotKeyed;
	}		

	if (const EPropertyKeyedStatus* ExistingKeyedStatus = CachedPropertyKeyedStatusMap.Find(&PropertyHandle))
	{
		return *ExistingKeyedStatus;
	}
	//hack so we can get the reset cache state updated, use ToggleEditable state
	{
		IPropertyHandle* NotConst = const_cast<IPropertyHandle*>(&PropertyHandle);
		NotConst->NotifyPostChange(EPropertyChangeType::ToggleEditable);
	}

	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	UMovieSceneSequence* Sequence = SequencerPtr->GetFocusedMovieSceneSequence();
	EPropertyKeyedStatus KeyedStatus = EPropertyKeyedStatus::NotKeyed;

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return KeyedStatus;
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle.GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return EPropertyKeyedStatus::NotKeyed;
	}
	
	for (UObject* Object : OuterObjects)
	{
		if (UControlRigControlsProxy* Proxy = Cast< UControlRigControlsProxy>(Object))
		{
			KeyedStatus = Proxy->GetPropertyKeyedStatus(SequencerPtr,PropertyHandle);
		}
		//else check to see if it's in sequencer
	}
	CachedPropertyKeyedStatusMap.Add(&PropertyHandle, KeyedStatus);

	return KeyedStatus;
}

void FDetailKeyFrameCacheAndHandler::SetDelegates(TWeakPtr<ISequencer>& InWeakSequencer, FControlRigEditMode* InEditMode)
{
	WeakSequencer = InWeakSequencer;
	EditMode = InEditMode;
	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FDetailKeyFrameCacheAndHandler::OnMovieSceneDataChanged);
		Sequencer->OnGlobalTimeChanged().AddRaw(this, &FDetailKeyFrameCacheAndHandler::OnGlobalTimeChanged);
		Sequencer->OnEndScrubbingEvent().AddRaw(this, &FDetailKeyFrameCacheAndHandler::ResetCachedData);
		Sequencer->OnChannelChanged().AddRaw(this, &FDetailKeyFrameCacheAndHandler::OnChannelChanged);
		Sequencer->OnStopEvent().AddRaw(this, &FDetailKeyFrameCacheAndHandler::ResetCachedData);
	}
}

void FDetailKeyFrameCacheAndHandler::UnsetDelegates()
{
	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		Sequencer->OnMovieSceneDataChanged().RemoveAll(this);
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
		Sequencer->OnEndScrubbingEvent().RemoveAll(this);
		Sequencer->OnChannelChanged().RemoveAll(this);
		Sequencer->OnStopEvent().RemoveAll(this);
	}
}

void FDetailKeyFrameCacheAndHandler::OnGlobalTimeChanged()
{
	// Only reset cached data when not playing
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid() && Sequencer->GetPlaybackStatus() != EMovieScenePlayerStatus::Playing)
	{
		ResetCachedData();
	}
}

void FDetailKeyFrameCacheAndHandler::OnMovieSceneDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	if (DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemAdded
		|| DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved
		|| DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemsChanged
		|| DataChangeType == EMovieSceneDataChangeType::ActiveMovieSceneChanged
		|| DataChangeType == EMovieSceneDataChangeType::RefreshAllImmediately)
	{
		ResetCachedData();
	}
}

void FDetailKeyFrameCacheAndHandler::OnChannelChanged(const FMovieSceneChannelMetaData*, UMovieSceneSection*)
{
	ResetCachedData();
}

void FDetailKeyFrameCacheAndHandler::ResetCachedData()
{
	CachedPropertyKeyedStatusMap.Reset();
	bValuesDirty = true;
}

void FDetailKeyFrameCacheAndHandler::UpdateIfDirty()
{
	if (bValuesDirty == true)
	{
		if (FMovieSceneConstraintChannelHelper::bDoNotCompensate == false) //if compensating don't reset this.
		{
			if (EditMode && EditMode->GetControlProxy())
			{
				EditMode->GetControlProxy()->ValuesChanged();
			}
			bValuesDirty = false;
		}
	}
}


#undef LOCTEXT_NAMESPACE
