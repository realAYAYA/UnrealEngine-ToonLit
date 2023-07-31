// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionTrailTool.h"
#include "LevelEditorSequencerIntegration.h"
#include "ISequencer.h"
#include "Sequencer/SequencerTrailHierarchy.h"
#include "Sequencer/MovieSceneTransformTrail.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "InteractiveGizmoManager.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/SingleKeyCaptureBehavior.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Framework/Commands/GenericCommands.h"

#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
// for raycast into World
#include "CollisionQueryParams.h"
#include "Engine/World.h"

#include "SceneManagement.h"
#include "ScopedTransaction.h"
#include "Tools/MotionTrailOptions.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MotionTrailTool)

#define LOCTEXT_NAMESPACE "SequencerAnimTools"

UInteractiveTool* UMotionTrailToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UMotionTrailTool* NewTool = NewObject<UMotionTrailTool>(SceneState.ToolManager);
	UEdMode* EdMode = GetTypedOuter<UEdMode>();
	FEditorModeTools* ModeManager = nullptr;
	if (EdMode)
	{
		ModeManager = EdMode->GetModeManager();
	}
	NewTool->SetWorldGizmoModeManager(SceneState.World, SceneState.GizmoManager, ModeManager);

	return NewTool;
}

bool UMotionTrailTool::ProcessCommandBindings(const FKey Key, const bool bRepeat) const
{
	if (CommandBindings.IsValid())
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
		return CommandBindings->ProcessCommandBindings(Key, KeyState, bRepeat);
	}
	return false;
}

void FMotionTrailCommands::RegisterCommands()
{
	UI_COMMAND(TranslateSelectedKeysLeft, "Translate Selected Keys Left", "Translate selected keys one frame to the left", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Left));
	UI_COMMAND(TranslateSelectedKeysRight, "Translate Selected Keys Right", "Translate selected keys one frame to the right", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Right));
	UI_COMMAND(FrameSelection, "Frame Selection", "Frame camera around current key selection", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
}

FString UMotionTrailTool::TrailKeyTransformGizmoInstanceIdentifier = TEXT("TrailKeyTransformGizmoInstanceIdentifier");


void UMotionTrailTool::Setup()
{
	UInteractiveTool::Setup();

	LeftTarget.Target = this;
	LeftTarget.bIsLeft = true;
	RightTarget.Target = this;
	RightTarget.bIsLeft = false;

	LeftClickBehavior = NewObject<USingleClickInputBehavior>(this);
	LeftClickBehavior->SetUseLeftMouseButton(); //default
	LeftClickBehavior->Initialize(&LeftTarget);
	LeftClickBehavior->Modifiers.RegisterModifier(ShiftModifierId, FInputDeviceState::IsShiftKeyDown);
	LeftClickBehavior->Modifiers.RegisterModifier(CtrlModifierId, FInputDeviceState::IsCtrlKeyDown);
	LeftClickBehavior->Modifiers.RegisterModifier(AltModiferId, FInputDeviceState::IsAltKeyDown);
	AddInputBehavior(LeftClickBehavior);

	RightClickBehavior = NewObject<USingleClickInputBehavior>(this);
	RightClickBehavior->SetUseRightMouseButton();
	RightClickBehavior->Initialize(&RightTarget);
	RightClickBehavior->Modifiers.RegisterModifier(ShiftModifierId, FInputDeviceState::IsShiftKeyDown);
	RightClickBehavior->Modifiers.RegisterModifier(CtrlModifierId, FInputDeviceState::IsCtrlKeyDown);
	RightClickBehavior->Modifiers.RegisterModifier(AltModiferId, FInputDeviceState::IsAltKeyDown);
	AddInputBehavior(RightClickBehavior);

	TransformProxy = NewObject<UTransformProxy>(this);
	//TransformGizmo = GetToolManager()->GetPairedGizmoManager()->Create3AxisTransformGizmo(this, UMotionTrailTool::TrailKeyTransformGizmoInstanceIdentifier);

	FString GizmoIdentifier = TEXT("PivotToolGizmoIdentifier");
	ETransformGizmoSubElements Elements = ETransformGizmoSubElements::StandardTranslateRotate;
	TransformGizmo = GizmoManager->CreateCustomTransformGizmo(Elements, this, UMotionTrailTool::TrailKeyTransformGizmoInstanceIdentifier);

	TransformProxy->OnTransformChanged.AddUObject(this, &UMotionTrailTool::GizmoTransformChanged);
	TransformProxy->OnBeginTransformEdit.AddUObject(this, &UMotionTrailTool::GizmoTransformStarted);
	TransformProxy->OnEndTransformEdit.AddUObject(this, &UMotionTrailTool::GizmoTransformEnded);
	TransformGizmo->SetActiveTarget(TransformProxy, GetToolManager());
	TransformGizmo->SetVisibility(false);
	SetupIntegration();
}

void UMotionTrailTool::SetupIntegration()
{

	OnSequencersChangedHandle = FLevelEditorSequencerIntegration::Get().GetOnSequencersChanged().AddLambda([this] {
		for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			TrailHierarchy->Destroy();
		}

		TrailHierarchies.Reset();
		SequencerHierarchies.Reset();
		// TODO: kind of cheap for now, later should check with member TMap<ISequencer*, FTrailHierarchy*> TrackedSequencers
		for (TWeakPtr<ISequencer> WeakSequencer : FLevelEditorSequencerIntegration::Get().GetSequencers())
		{
			TrailHierarchies.Add_GetRef(MakeUnique<UE::SequencerAnimTools::FSequencerTrailHierarchy>(WeakSequencer))->Initialize();
			SequencerHierarchies.Add(WeakSequencer.Pin().Get(), TrailHierarchies.Last().Get());
		}
	});

	for (TWeakPtr<ISequencer> WeakSequencer : FLevelEditorSequencerIntegration::Get().GetSequencers())
	{
		TrailHierarchies.Add_GetRef(MakeUnique<UE::SequencerAnimTools::FSequencerTrailHierarchy>(WeakSequencer))->Initialize();
		SequencerHierarchies.Add(WeakSequencer.Pin().Get(), TrailHierarchies.Last().Get());
	}

	CommandBindings = MakeShareable(new FUICommandList);

	FMotionTrailCommands::Register();

	const FMotionTrailCommands& Commands = FMotionTrailCommands::Get();

	CommandBindings->MapAction(
		Commands.TranslateSelectedKeysLeft,
		FExecuteAction::CreateUObject(this, &UMotionTrailTool::TranslateSelectedKeysLeft),
		FCanExecuteAction::CreateUObject(this, &UMotionTrailTool::SomeKeysAreSelected));

	CommandBindings->MapAction(
		Commands.TranslateSelectedKeysRight,
		FExecuteAction::CreateUObject(this, &UMotionTrailTool::TranslateSelectedKeysRight),
		FCanExecuteAction::CreateUObject(this, &UMotionTrailTool::SomeKeysAreSelected));

	CommandBindings->MapAction(
		Commands.FrameSelection,
		FExecuteAction::CreateUObject(this, &UMotionTrailTool::FrameSelection),
		FCanExecuteAction::CreateUObject(this, &UMotionTrailTool::SomeKeysAreSelected));

	CommandBindings->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateUObject(this, &UMotionTrailTool::DeleteSelectedKeys),
		FCanExecuteAction::CreateUObject(this, &UMotionTrailTool::SomeKeysAreSelected));
}


void UMotionTrailTool::Shutdown(EToolShutdownType ShutdownType)
{
	Super::Shutdown(ShutdownType);
	GizmoManager->DestroyAllGizmosByOwner(this);
	ShutdownIntegration();
}

void UMotionTrailTool::ShutdownIntegration()
{
	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		// TODO: just use dtor?
		TrailHierarchy->Destroy();
	}

	TrailHierarchies.Reset();
	FLevelEditorSequencerIntegration::Get().GetOnSequencersChanged().Remove(OnSequencersChangedHandle);
}



// Detects Ctrl and Shift key states
void UMotionTrailTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	if (ModifierID == ShiftModifierId)
	{
		bShiftModifierOn = bIsOn;
	}
	else if (ModifierID == CtrlModifierId)
	{
		bCtrlModifierOn = bIsOn;
	}
	if (bShiftModifierOn && bCtrlModifierOn)
	{
		bAltModifierOn = true;
		bShiftModifierOn = false;
		bCtrlModifierOn = false;
	}
	//ALT MODIFIER doesn't work, we can not even use FSlateApplication::Get().GetModifierKeys().IsAltDown() 
	//since if there is an alt it just bails !!!!l
	
}

FInputRayHit UMotionTrailTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	FInputRayHit ReturnHit = FInputRayHit();
	FViewport* FocusedViewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	HHitProxy* HitResult = FocusedViewport->GetHitProxy(ClickPos.ScreenPosition.X, ClickPos.ScreenPosition.Y);
	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		if (TrailHierarchy->IsHitByClick(HitResult))
		{
			ReturnHit.bHit = true;
			break;
		}
	}

	return ReturnHit;
}

void UMotionTrailTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	FEditorViewportClient* ViewportClient = ModeManager->GetFocusedViewportClient();
	FViewport* FocusedViewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	HHitProxy* HitResult = FocusedViewport->GetHitProxy(ClickPos.ScreenPosition.X, ClickPos.ScreenPosition.Y);

	UE::SequencerAnimTools::FInputClick InputClick(bAltModifierOn, bCtrlModifierOn, bShiftModifierOn);
	if (RightTarget.bClicked)
	{
		InputClick.bIsRightMouse = true;
	}

	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->HandleClick(ViewportClient, HitResult, InputClick);
	}
	UpdateGizmoLocation();
}


void UMotionTrailTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->GetRenderer()->Render(RenderAPI->GetSceneView(), RenderAPI->GetPrimitiveDrawInterface());
	}
}

void UMotionTrailTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->GetRenderer()->DrawHUD(RenderAPI->GetSceneView(), Canvas);
	}
}


void UMotionTrailTool::OnTick(float DeltaTime)
{
	for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->Update();
	}
	UpdateGizmoLocation();
}

TArray<UObject*> UMotionTrailTool::GetToolProperties(bool bEnabledOnly) const
{
	return  TArray<UObject*>();
}


void UMotionTrailTool::GizmoTransformStarted(UTransformProxy* Proxy)
{
	//TransactionIndex = GEditor->BeginTransaction(nullptr, LOCTEXT("MoveMotionTrail", "Move Motion Trail"), nullptr);

	StartDragTransform = Proxy->GetTransform();

	bGizmoBeingDragged = true;
	bManipulatorMadeChange = false;
	GizmoTransform = StartDragTransform;
	for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->StartTracking();
	}
}

void UMotionTrailTool::GizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform)
{
	if (!bGizmoBeingDragged)
	{
		return;
	}
	GizmoTransform = Transform;
	FTransform Diff = Transform.GetRelativeTransform(StartDragTransform);
	FVector LocationDiff = GizmoTransform.GetLocation() - StartDragTransform.GetLocation();
	for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		FVector WidgetLocation = Transform.GetLocation();
		FVector Location;
		bool bIsHandled = TrailHierarchy->ApplyDelta(LocationDiff, Diff.GetRotation().Rotator(), WidgetLocation);
		if (bIsHandled)
		{
			bManipulatorMadeChange = true;
		}
	}
	StartDragTransform = Transform;
	UpdateGizmoLocation();

}

void UMotionTrailTool::GizmoTransformEnded(UTransformProxy* Proxy)
{
	for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		TrailHierarchy->EndTracking();
	}
	if (bManipulatorMadeChange && TransactionIndex != INDEX_NONE)
	{
		//GEditor->EndTransaction();
	}
	else if (TransactionIndex != INDEX_NONE)
	{
	//	GEditor->CancelTransaction(TransactionIndex);
	}
	bGizmoBeingDragged = false;
	bManipulatorMadeChange = false;
	UpdateGizmoLocation();
}

void UMotionTrailTool::UpdateGizmoVisibility()
{
	if (TransformGizmo)
	{
		bool bAnythingSelected = false;

		for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			FVector Location;
			bool bIsHandled = TrailHierarchy->IsAnythingSelected();
			if (bIsHandled)
			{
				bAnythingSelected = true;
			}
		}
		TransformGizmo->SetVisibility(bAnythingSelected);
	}
}

void UMotionTrailTool::UpdateGizmoLocation()
{
	if (!TransformGizmo)
	{
		return;
	}
	UpdateGizmoVisibility();
	FVector NewGizmoLocation(0., 0., 0.);
	int NumSelected = 0;
	for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		FVector Location;
		bool bIsHandled = TrailHierarchy->IsAnythingSelected(Location);
		if (bIsHandled)
		{
			NewGizmoLocation += Location;
			++NumSelected;
		}
	}
	if (NumSelected > 0)
	{
		NewGizmoLocation /= (double)NumSelected;
	}
	GizmoTransform.SetLocation(NewGizmoLocation);
	TransformGizmo->ReinitializeGizmoTransform(GizmoTransform);
}


void UMotionTrailTool::TranslateSelectedKeysLeft()
{
	if (SomeKeysAreSelected())
	{
		const FScopedTransaction Transaction(LOCTEXT("TranslateSelectedKeysLeft", "Translate Selected Keys Left"));
		for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			TrailHierarchy->TranslateSelectedKeys(false);
		}
	}
}

void UMotionTrailTool::TranslateSelectedKeysRight()
{
	if (SomeKeysAreSelected())
	{
		const FScopedTransaction Transaction(LOCTEXT("TranslateSelectedKeysRight", "Translate Selected Keys Right"));
		for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			TrailHierarchy->TranslateSelectedKeys(true);
		}
	}
}

void UMotionTrailTool::FrameSelection()
{
	if (SomeKeysAreSelected())
	{
		//if (CurrentViewportClient)
		{
			FBox Bounds(EForceInit::ForceInit);
			TArray<FVector> Positions;
			for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
			{
				if (TrailHierarchy->IsAnythingSelected(Positions))
				{
					for (const FVector Pos : Positions)
					{
						Bounds += Pos;
						Bounds += Pos + FVector::OneVector * 5.f;
						Bounds += Pos - FVector::OneVector * 5.f;
					}
				}
			}
			if (Bounds.IsValid)
			{
				//CurrentViewportClient->FocusViewportOnBox(Bounds);
			}
		}
	}
}

void UMotionTrailTool::SelectNone()
{
	if (SomeKeysAreSelected())
	{
		for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			TrailHierarchy->SelectNone();
		}
	}
}

void UMotionTrailTool::DeleteSelectedKeys()
{
	if (SomeKeysAreSelected())
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteSelectedKeys", "Delete Selected Keys"));

		for (TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
		{
			TrailHierarchy->DeleteSelectedKeys();
		}
	}
}

bool UMotionTrailTool::SomeKeysAreSelected() const
{
	bool bAnythingSelected = false;

	for (const TUniquePtr<UE::SequencerAnimTools::FTrailHierarchy>& TrailHierarchy : TrailHierarchies)
	{
		FVector Location;
		bool bIsHandled = TrailHierarchy->IsAnythingSelected();
		if (bIsHandled)
		{
			bAnythingSelected = true;
		}
	}
	return bAnythingSelected;
}


#undef LOCTEXT_NAMESPACE

