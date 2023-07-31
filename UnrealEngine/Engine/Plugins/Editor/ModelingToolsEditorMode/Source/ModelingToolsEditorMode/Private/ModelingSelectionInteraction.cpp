// Copyright Epic Games, Inc. All Rights Reserved.


#include "ModelingSelectionInteraction.h"
#include "InteractiveToolsContext.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Selection/GeometrySelectionManager.h"
#include "ToolSceneQueriesUtil.h"
#include "SceneQueries/SceneSnappingManager.h"

using namespace UE::Geometry;

void UModelingSelectionInteraction::Initialize(
	TObjectPtr<UGeometrySelectionManager> SelectionManagerIn,
	TUniqueFunction<bool()> CanChangeSelectionCallbackIn,
	TUniqueFunction<bool(const FInputDeviceRay&)> ExternalHitCaptureCallbackIn)
{
	SelectionManager = SelectionManagerIn;

	CanChangeSelectionCallback = MoveTemp(CanChangeSelectionCallbackIn);
	ExternalHitCaptureCallback = MoveTemp(ExternalHitCaptureCallbackIn);

	// create click behavior and set ourselves as click target
	ClickBehavior = NewObject<USingleClickInputBehavior>();
	ClickBehavior->Modifiers.RegisterModifier(AddToSelectionModifier, FInputDeviceState::IsShiftKeyDown);
	ClickBehavior->Modifiers.RegisterModifier(ToggleSelectionModifier, FInputDeviceState::IsCtrlKeyDown);
	ClickBehavior->Initialize(this);

	BehaviorSet = NewObject<UInputBehaviorSet>();
	BehaviorSet->Add(ClickBehavior, this);

	TransformProxy = NewObject<UTransformProxy>(this);
	// todo: make this repositionable etc. Maybe make this function a delegate? or allow caller to provide the gizmo?
	TransformGizmo = UE::TransformGizmoUtil::Create3AxisTransformGizmo(
		SelectionManager->GetToolsContext()->GizmoManager, this, TEXT("ModelingSelectionInteraction") );
	TransformGizmo->SetActiveTarget(TransformProxy, SelectionManager->GetToolsContext()->GizmoManager);
	TransformGizmo->SetVisibility(false);

	// listen for change events on transform proxy
	TransformProxy->OnTransformChanged.AddUObject(this, &UModelingSelectionInteraction::OnGizmoTransformChanged);
	TransformProxy->OnBeginTransformEdit.AddUObject(this, &UModelingSelectionInteraction::OnBeginGizmoTransform);
	TransformProxy->OnEndTransformEdit.AddUObject(this, &UModelingSelectionInteraction::OnEndGizmoTransform);

	// listen for selection changes to update gizmo
	OnSelectionModifiedDelegate = SelectionManager->OnSelectionModified.AddUObject(this, &UModelingSelectionInteraction::OnSelectionManager_SelectionModified);
}

void UModelingSelectionInteraction::Shutdown()
{
	if (IsValid(SelectionManager))
	{
		SelectionManager->OnSelectionModified.Remove(OnSelectionModifiedDelegate);
	}

	if (TransformGizmo)
	{
		SelectionManager->GetToolsContext()->GizmoManager->DestroyGizmo(TransformGizmo);
		TransformGizmo = nullptr;
	}
}




void UModelingSelectionInteraction::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	// update modifier state flags
	if (ModifierID == AddToSelectionModifier)
	{
		bAddToSelectionEnabled = bIsOn;
	}
	else if (ModifierID == ToggleSelectionModifier)
	{
		bToggleSelectionEnabled = bIsOn;
	}
}

PRAGMA_DISABLE_OPTIMIZATION
void UModelingSelectionInteraction::ComputeSceneHits(const FInputDeviceRay& ClickPos,
	bool& bHitActiveObjects, FInputRayHit& ActiveObjectHit,
	bool& bHitInactiveObjectFirst, FInputRayHit& InactiveObjectHit)
{
	ActiveObjectHit = FInputRayHit();
	bHitActiveObjects = SelectionManager->RayHitTest(ClickPos.WorldRay, ActiveObjectHit);

	// We want to filter out hits against nearer objects. This is...tricky.
	// TODO: we probably should not actually do this here. Modeling Mode probably needs to 
	// have a behavior that mimics editor selection, that would have just received the closer
	// FInputRayHit and taken the click, instead of this function...
	bHitInactiveObjectFirst = false;
	InactiveObjectHit = FInputRayHit();
	if (bHitActiveObjects)
	{
		TArray<const UPrimitiveComponent*> IgnoreComponents;
		if (ActiveObjectHit.HitObject.IsValid())
		{
			if (UPrimitiveComponent* SelectionComponent = Cast<UPrimitiveComponent>(ActiveObjectHit.HitObject.Get()) )
			{
				IgnoreComponents.Add(SelectionComponent);
			}
		}
		FHitResult HitResultOut;
		if (ToolSceneQueriesUtil::FindNearestVisibleObjectHit(USceneSnappingManager::Find(SelectionManager->GetToolsContext()->ToolManager), 
			HitResultOut, ClickPos.WorldRay, &IgnoreComponents, nullptr))
		{
			if (HitResultOut.Distance < ActiveObjectHit.HitDepth)
			{
				bHitInactiveObjectFirst = true;
				InactiveObjectHit.HitDepth = HitResultOut.Distance;
				InactiveObjectHit.bHit = true;
			}
		}
	}
}
PRAGMA_ENABLE_OPTIMIZATION

PRAGMA_DISABLE_OPTIMIZATION
FInputRayHit UModelingSelectionInteraction::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	// ignore hits in these cases
	if (  (SelectionManager->GetMeshTopologyMode() == UGeometrySelectionManager::EMeshTopologyMode::None) 
		|| (CanChangeSelectionCallback() == false)
		|| ExternalHitCaptureCallback(ClickPos) )
	{
		return FInputRayHit();
	}

	bool bHitActiveObjects, bHitInactiveObjectFirst;
	FInputRayHit ActiveObjectHit, InactiveObjectHit;
	ComputeSceneHits(ClickPos, bHitActiveObjects, ActiveObjectHit, bHitInactiveObjectFirst, InactiveObjectHit);

	// Note: currently this flow will produce somewhat undesirable behavior, in that if 
	// some other object not in the selection target(s) is clicked, the selection will be 
	// cleared but that object will have to be clicked again to select it. We cannot currently 
	// clear the mesh-selection and change the actor-selection inside a single transaction in a
	// single click. This is because we are relying on the Editor to do the actor-selection change.
	// So if we were to (eg) ClearSelection() and return no-hit here, the ClearSelection() call
	// would emit a transaction, and then the actor-selection-change would emit a second transaction.
	// 
	// Behavior in many DCCS is that they can (1) clear active mesh-selection, (2) select new target
	// object, and (3) do new mesh-selection in a single click. This will require quite a bit of effort
	// to achieve in the future. 
	//
	// So for now we set a bClearSelectionOnClicked flag and consume the click. This means the 1-2-3
	// flow above requires 3 separate clicks :( 


	bClearSelectionOnClicked = false;
	if (SelectionManager->HasSelection())
	{
		if (bHitInactiveObjectFirst)
		{
			//bClearSelectionOnClicked = true;
			//return InactiveObjectHit;
			return FInputRayHit();
		}
		else if (bHitActiveObjects)
		{
			return ActiveObjectHit;
		}
		else
		{
			// if we have active selection we still want to capture this hit so that we can clear in OnClicked
			//bClearSelectionOnClicked = true;
			//return FInputRayHit(TNumericLimits<double>::Max());
			return FInputRayHit();
		}
	}
	else
	{
		if (bHitActiveObjects && bHitInactiveObjectFirst == false)
		{
			return ActiveObjectHit;
		}
	}

	return FInputRayHit();
}
PRAGMA_ENABLE_OPTIMIZATION



void UModelingSelectionInteraction::OnClicked(const FInputDeviceRay& ClickPos)
{
	// this flag is set in IsHitByClick test
	if (bClearSelectionOnClicked)
	{
		SelectionManager->ClearSelection();
		bClearSelectionOnClicked = false;
		return;
	}

	FGeometrySelectionUpdateConfig UpdateConfig;
	UpdateConfig.ChangeType = EGeometrySelectionChangeType::Replace;
	if (bAddToSelectionEnabled)
	{
		UpdateConfig.ChangeType = EGeometrySelectionChangeType::Add;
	}
	else if (bToggleSelectionEnabled)
	{
		UpdateConfig.ChangeType = EGeometrySelectionChangeType::Remove;
	}

	FGeometrySelectionUpdateResult Result;
	SelectionManager->UpdateSelectionViaRaycast(
		ClickPos.WorldRay,
		UpdateConfig,
		Result);
}


void UModelingSelectionInteraction::UpdateGizmoOnSelectionChange()
{
	if (SelectionManager->HasSelection() == false)
	{
		TransformGizmo->SetVisibility(false);
	}
	else
	{
		TransformGizmo->SetVisibility(true);

		FFrame3d SelectionFrame;
		SelectionManager->GetSelectionWorldFrame(SelectionFrame);
		TransformGizmo->ReinitializeGizmoTransform( SelectionFrame.ToFTransform() );

		//FGeometrySelectionBounds Bounds;
		//SelectionManager->GetSelectionBounds(Bounds);
		//TransformGizmo->ReinitializeGizmoTransform( FTransform(Bounds.WorldBounds.Center()) );
	}
}


void UModelingSelectionInteraction::OnSelectionManager_SelectionModified()
{
	UpdateGizmoOnSelectionChange();
}



void UModelingSelectionInteraction::OnBeginGizmoTransform(UTransformProxy* Proxy)
{
	FTransform Transform = Proxy->GetTransform();
	InitialGizmoFrame = FFrame3d(Transform);
	InitialGizmoScale = FVector3d(Transform.GetScale3D());

	bInActiveTransform = SelectionManager->BeginTransformation();

	OnTransformBegin.Broadcast();
}

void UModelingSelectionInteraction::OnEndGizmoTransform(UTransformProxy* Proxy)
{
	if (bInActiveTransform)
	{
		SelectionManager->EndTransformation();
		bInActiveTransform = false;

		OnTransformEnd.Broadcast();
	}
}

void UModelingSelectionInteraction::OnGizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform)
{
	if (bInActiveTransform)
	{
		LastUpdateGizmoFrame = FFrame3d(Transform);
		LastUpdateGizmoScale = FVector3d(Transform.GetScale3D());
		bGizmoUpdatePending = true;
	}
}


void UModelingSelectionInteraction::ApplyPendingTransformInteractions()
{
	if (bInActiveTransform == false || bGizmoUpdatePending == false)
	{
		return;
	}

	FFrame3d CurFrame = LastUpdateGizmoFrame;
	FVector3d CurScale = LastUpdateGizmoScale;
	FVector3d TranslationDelta = CurFrame.Origin - InitialGizmoFrame.Origin;
	FQuaterniond RotateDelta = CurFrame.Rotation - InitialGizmoFrame.Rotation;
	FVector3d CurScaleDelta = CurScale - InitialGizmoScale;

	const bool bLastUpdateUsedWorldFrame = true;	// for later local-frame support

	if (TranslationDelta.SquaredLength() > 0.0001 || RotateDelta.SquaredLength() > 0.0001 || CurScaleDelta.SquaredLength() > 0.0001)
	{
		if (bLastUpdateUsedWorldFrame)
		{
			// For a world frame gizmo, the scaling needs to happen in world aligned gizmo space, but the 
			// rotation is still encoded in the local gizmo frame change.
			FQuaterniond RotationToApply = CurFrame.Rotation * InitialGizmoFrame.Rotation.Inverse();
			SelectionManager->UpdateTransformation( [&](int32 VertexID, const FVector3d& InitialPosition, const FTransform3d& WorldTransform)
			{
				FVector3d LocalTranslation = WorldTransform.InverseTransformVector(TranslationDelta);

				FVector3d PosLocal = InitialPosition;
				FVector3d PosWorld = WorldTransform.TransformPosition(PosLocal);
				FVector3d PosWorldGizmo = PosWorld - InitialGizmoFrame.Origin;

				FVector3d NewPosWorld = RotationToApply * (PosWorldGizmo * CurScale) + CurFrame.Origin;
				FVector3d NewPosLocal = WorldTransform.InverseTransformPosition(NewPosWorld);
				return NewPosLocal;
			});
		}
		else
		{
			SelectionManager->UpdateTransformation( [&](int32 VertexID, const FVector3d& InitialPosition, const FTransform3d& WorldTransform)
			{
				// For a local gizmo, we just get the coordinates in the original frame, scale in that frame,
				// then interpret them as coordinates in the new frame.
				FVector3d PosLocal = InitialPosition;
				FVector3d PosWorld = WorldTransform.TransformPosition(PosLocal);
				FVector3d PosGizmo = InitialGizmoFrame.ToFramePoint(PosWorld);
				PosGizmo = CurScale * PosGizmo;
				FVector3d NewPosWorld = CurFrame.FromFramePoint(PosGizmo);
				FVector3d NewPosLocal = WorldTransform.InverseTransformPosition(NewPosWorld);
				return NewPosLocal;
			});
		}
	}




	bGizmoUpdatePending = false;
}