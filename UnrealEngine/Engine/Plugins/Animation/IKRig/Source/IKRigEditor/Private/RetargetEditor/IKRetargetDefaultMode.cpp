// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetDefaultMode.h"

#include "EditorViewportClient.h"
#include "AssetEditorModeManager.h"
#include "EngineUtils.h"
#include "IKRigDebugRendering.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/IKRetargetHitProxies.h"
#include "ReferenceSkeleton.h"


#define LOCTEXT_NAMESPACE "IKRetargetDefaultMode"

FName FIKRetargetDefaultMode::ModeName("IKRetargetAssetDefaultMode");

bool FIKRetargetDefaultMode::GetCameraTarget(FSphere& OutTarget) const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false;
	}
	return Controller->GetCameraTargetForSelection(OutTarget);
}

IPersonaPreviewScene& FIKRetargetDefaultMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

void FIKRetargetDefaultMode::Initialize()
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return; 
	}

	// update offsets on preview meshes
	Controller->AddOffsetToMeshComponent(FVector::ZeroVector, Controller->SourceSkelMeshComponent);
	Controller->AddOffsetToMeshComponent(FVector::ZeroVector, Controller->TargetSkelMeshComponent);

	bIsInitialized = true;
}

void FIKRetargetDefaultMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	if (!EditorController.IsValid())
	{
		return;
	}
	
	const FIKRetargetEditorController* Controller = EditorController.Pin().Get();

	// render source and target skeletons
	Controller->RenderSkeleton(PDI, ERetargetSourceOrTarget::Source);
	Controller->RenderSkeleton(PDI, ERetargetSourceOrTarget::Target);

	// render all the chain and root debug proxies
	RenderDebugProxies(PDI, Controller);
}

void FIKRetargetDefaultMode::RenderDebugProxies(FPrimitiveDrawInterface* PDI, const FIKRetargetEditorController* Controller) const
{
	const UIKRetargeter* Asset = Controller->AssetController->GetAsset();
	if (!Asset->bDebugDraw)
	{
		return;
	}
	
	const UIKRetargetProcessor* RetargetProcessor = Controller->GetRetargetProcessor();
	if (!(RetargetProcessor && RetargetProcessor->IsInitialized()))
	{
		return;
	}

	UDebugSkelMeshComponent* TargetSkelMesh = Controller->GetSkeletalMeshComponent(ERetargetSourceOrTarget::Target);
	const FTransform ComponentTransform = TargetSkelMesh->GetComponentTransform();
	const FVector ComponentOffset = ComponentTransform.GetTranslation();

	const TArray<FName>& SelectedChains = Controller->GetSelectedChains();

	constexpr FLinearColor Muted = FLinearColor(0.5,0.5,0.5, 0.5);
	const FLinearColor SourceColor = (FLinearColor::Gray * FLinearColor::Blue) * Muted;
	const FLinearColor GoalColor = FLinearColor::Yellow;
	const FLinearColor MainColor = FLinearColor::Green;
	const FLinearColor NonSelected = FLinearColor::Gray * 0.3f;

	// get the root modification
	const FRootRetargeter& RootRetargeter = RetargetProcessor->GetRootRetargeter();
	const FVector RootModification = RootRetargeter.Target.RootTranslationDelta * RootRetargeter.Settings.GetAffectIKWeightVector();

	// draw IK goals on each IK chain
	const TArray<FRetargetChainPairIK>& IKChainPairs = RetargetProcessor->GetIKChainPairs();
	for (const FRetargetChainPairIK& IKChainPair : IKChainPairs)
	{
		const FChainDebugData& ChainDebugData = IKChainPair.IKChainRetargeter.DebugData;
		FTransform FinalTransform = ChainDebugData.OutputTransformEnd;
		FinalTransform.AddToTranslation(ComponentOffset);

		const bool bIsSelected = SelectedChains.Contains(IKChainPair.TargetBoneChainName);

		PDI->SetHitProxy(new HIKRetargetEditorChainProxy(IKChainPair.TargetBoneChainName));

		if (Asset->bDrawFinalGoals)
		{
			IKRigDebugRendering::DrawWireCube(
			PDI,
			FinalTransform,
			bIsSelected ? GoalColor : GoalColor * NonSelected,
			Asset->ChainDrawSize,
			Asset->ChainDrawThickness);
		}
		
		if (Asset->bDrawSourceLocations)
		{
			const FSourceChainIK& SourceChain = IKChainPair.IKChainRetargeter.Source;
			FTransform SourceGoalTransform;
			SourceGoalTransform.SetTranslation(SourceChain.CurrentEndPosition + ComponentOffset + RootModification);
			SourceGoalTransform.SetRotation(SourceChain.CurrentEndRotation);

			FLinearColor Color = bIsSelected ? SourceColor : SourceColor * NonSelected;

			DrawWireSphere(
				PDI,
				SourceGoalTransform,
				Color,
				Asset->ChainDrawSize * 0.5f,
				12,
				SDPG_World,
				0.0f,
				0.001f,
				false);

			if (Asset->bDrawFinalGoals)
			{
				DrawDashedLine(
					PDI,
					SourceGoalTransform.GetLocation(),
					FinalTransform.GetLocation(),
					Color,
					1.0f,
					SDPG_Foreground);
			}
		}

		// done drawing chain proxies
		PDI->SetHitProxy(nullptr);
	}

	// draw lines on each FK chain
	const TArray<FRetargetChainPairFK>& FKChainPairs = RetargetProcessor->GetFKChainPairs();
	for (const FRetargetChainPairFK& FKChainPair : FKChainPairs)
	{
		const TArray<int32>& TargetChainBoneIndices = FKChainPair.FKDecoder.BoneIndices;
		if (TargetChainBoneIndices.IsEmpty())
		{
			continue;
		}
		
		const bool bIsSelected = SelectedChains.Contains(FKChainPair.TargetBoneChainName);
		
		// draw a line from start to end of chain, or in the case of a chain with only 1 bone in it, draw a sphere
		PDI->SetHitProxy(new HIKRetargetEditorChainProxy(FKChainPair.TargetBoneChainName));
		if (TargetChainBoneIndices.Num() > 1)
		{
			FTransform StartTransform = TargetSkelMesh->GetBoneTransform(TargetChainBoneIndices[0], ComponentTransform);
			FTransform EndTransform = TargetSkelMesh->GetBoneTransform(TargetChainBoneIndices.Last(), ComponentTransform);
			PDI->DrawLine(
			StartTransform.GetLocation(),
			EndTransform.GetLocation(),
			bIsSelected ? MainColor : MainColor * NonSelected,
			SDPG_Foreground,
			Asset->ChainDrawThickness);
		}
		else
		{
			FTransform StartTransform = TargetSkelMesh->GetBoneTransform(TargetChainBoneIndices[0], ComponentTransform);
			
			DrawWireSphere(
				PDI,
				StartTransform.GetLocation(),
				bIsSelected ? MainColor : MainColor * NonSelected,
				Asset->ChainDrawSize,
				12,
				SDPG_World,
				Asset->ChainDrawThickness,
				0.001f,
				false);
		}
		
		PDI->SetHitProxy(nullptr);
	}

	// draw stride warping frame
	FTransform WarpingFrame = RetargetProcessor->DebugData.StrideWarpingFrame;
	DrawCoordinateSystem(PDI, WarpingFrame.GetLocation(), WarpingFrame.GetRotation().Rotator(), Asset->ChainDrawSize, SDPG_World, Asset->ChainDrawThickness);

	// root bone name
	const FName RootBoneName = Controller->AssetController->GetRetargetRootBone(ERetargetSourceOrTarget::Target);
	const int32 RootBoneIndex = TargetSkelMesh->GetReferenceSkeleton().FindBoneIndex(RootBoneName);
	if (RootBoneIndex == INDEX_NONE)
	{
		return;
	}
	const FTransform RootTransform = TargetSkelMesh->GetBoneTransform(RootBoneIndex, ComponentTransform);
	const FVector RootCircleLocation = RootTransform.GetLocation() * FVector(1,1,0);
	const bool bIsSelected = EditorController.Pin()->GetRootSelected();
	const FLinearColor RootColor = bIsSelected ? MainColor : MainColor * NonSelected;
	
	PDI->SetHitProxy(new HIKRetargetEditorRootProxy());
	DrawCircle(
		PDI,
		RootCircleLocation,
		FVector(1, 0, 0),
		FVector(0, 1, 0),
		RootColor,
		Asset->ChainDrawSize * 10.f,
		12,
		SDPG_World,
		Asset->ChainDrawThickness * 2.0f);
	PDI->SetHitProxy(nullptr);
}

bool FIKRetargetDefaultMode::AllowWidgetMove()
{
	return false;
}

bool FIKRetargetDefaultMode::ShouldDrawWidget() const
{
	return UsesTransformWidget(CurrentWidgetMode);
}

bool FIKRetargetDefaultMode::UsesTransformWidget() const
{
	return UsesTransformWidget(CurrentWidgetMode);
}

bool FIKRetargetDefaultMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false; 
	}
	
	const bool bTranslating = CheckMode == UE::Widget::EWidgetMode::WM_Translate;
	return bTranslating && IsValid(Controller->GetSelectedMesh());
}

FVector FIKRetargetDefaultMode::GetWidgetLocation() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return FVector::ZeroVector; 
	}
	
	if (!Controller->GetSelectedMesh())
	{
		return FVector::ZeroVector; // shouldn't get here
	}

	return Controller->GetSelectedMesh()->GetComponentTransform().GetLocation();
}

bool FIKRetargetDefaultMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false; 
	}

	const bool bLeftButtonClicked = Click.GetKey() == EKeys::LeftMouseButton;
	const bool bCtrlOrShiftHeld = Click.IsControlDown() || Click.IsShiftDown();
	const ESelectionEdit EditMode = bCtrlOrShiftHeld ? ESelectionEdit::Add : ESelectionEdit::Replace;
	
	// did we click on an actor in the viewport?
	const bool bHitActor = HitProxy && HitProxy->IsA(HActor::StaticGetType());
	if (bLeftButtonClicked && bHitActor)
	{
		const HActor* ActorProxy = static_cast<HActor*>(HitProxy);
		Controller->SetSelectedMesh(ConstCast(ActorProxy->PrimComponent));
		return true;
	}

	// did we click on a bone in the viewport?
	const bool bHitBone = HitProxy && HitProxy->IsA(HIKRetargetEditorBoneProxy::StaticGetType());
	if (bLeftButtonClicked && bHitBone)
	{
		const HIKRetargetEditorBoneProxy* BoneProxy = static_cast<HIKRetargetEditorBoneProxy*>(HitProxy);
		const TArray BoneNames{BoneProxy->BoneName};
		constexpr bool bFromHierarchy = false;
		Controller->EditBoneSelection(BoneNames, EditMode, bFromHierarchy);
		return true;
	}

	// did we click on a chain in the viewport?
	const bool bHitChain = HitProxy && HitProxy->IsA(HIKRetargetEditorChainProxy::StaticGetType());
	if (bLeftButtonClicked && bHitChain)
	{
		const HIKRetargetEditorChainProxy* ChainProxy = static_cast<HIKRetargetEditorChainProxy*>(HitProxy);
		const TArray ChainNames{ChainProxy->TargetChainName};
		constexpr bool bFromChainView = false;
		Controller->EditChainSelection(ChainNames, EditMode, bFromChainView);
		return true;
	}

	// did we click on the root in the viewport?
	const bool bHitRoot = HitProxy && HitProxy->IsA(HIKRetargetEditorRootProxy::StaticGetType());
	if (bLeftButtonClicked && bHitRoot)
	{
		Controller->SetRootSelected(true);
		return true;
	}

	// we didn't hit anything, therefore clicked in empty space in viewport
	Controller->ClearSelection(); // deselect all meshes, bones, chains and update details view
	return true;
}

bool FIKRetargetDefaultMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return HandleBeginTransform(InViewportClient);
}

bool FIKRetargetDefaultMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	return HandleEndTransform();
}

bool FIKRetargetDefaultMode::BeginTransform(const FGizmoState& InState)
{
	return HandleBeginTransform(Owner->GetFocusedViewportClient());
}

bool FIKRetargetDefaultMode::EndTransform(const FGizmoState& InState)
{
	return HandleEndTransform();
}

bool FIKRetargetDefaultMode::HandleBeginTransform(const FEditorViewportClient* InViewportClient)
{
	if (!InViewportClient)
	{
		return false;
	}
	
	bIsTranslating = false;

	// not manipulating any widget axes, so stop tracking
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	if (CurrentAxis == EAxisList::None)
	{
		return false; 
	}

	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false; // invalid editor state
	}

	const bool bTranslating = InViewportClient->GetWidgetMode() == UE::Widget::EWidgetMode::WM_Translate;
	if (bTranslating && IsValid(Controller->GetSelectedMesh()))
	{
		bIsTranslating = true;
		GEditor->BeginTransaction(LOCTEXT("MovePreviewMesh", "Move Preview Mesh"));
		Controller->AssetController->GetAsset()->Modify();
		return true;
	}

	return false;
}

bool FIKRetargetDefaultMode::HandleEndTransform()
{
	GEditor->EndTransaction();
	bIsTranslating = false;
	return true;
}

bool FIKRetargetDefaultMode::InputDelta(
	FEditorViewportClient* InViewportClient,
	FViewport* InViewport,
	FVector& InDrag,
	FRotator& InRot,
	FVector& InScale)
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false; 
	}
	
	if (!(bIsTranslating && IsValid(Controller->GetSelectedMesh())))
	{
		return false; // not handled
	}

	if(InViewportClient->GetWidgetMode() != UE::Widget::WM_Translate)
	{
		return false;
	}

	Controller->AddOffsetToMeshComponent(InDrag, Controller->GetSelectedMesh());
	
	return true;
}

bool FIKRetargetDefaultMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return false; 
	}

	if (!Controller->GetSelectedMesh())
	{
		return false;
	}

	InMatrix = Controller->GetSelectedMesh()->GetComponentTransform().ToMatrixNoScale().RemoveTranslation();
	return true;
}

bool FIKRetargetDefaultMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

void FIKRetargetDefaultMode::Enter()
{
	IPersonaEditMode::Enter();

	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return; 
	}

	// record which skeleton is being viewed/edited
	SkeletonMode = Controller->GetSourceOrTarget();
}

void FIKRetargetDefaultMode::Exit()
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return; 
	}
	
	IPersonaEditMode::Exit();
}

UDebugSkelMeshComponent* FIKRetargetDefaultMode::GetCurrentlyEditedMesh() const
{
	const TSharedPtr<FIKRetargetEditorController> Controller = EditorController.Pin();
	if (!Controller.IsValid())
	{
		return nullptr; 
	}
	
	return SkeletonMode == ERetargetSourceOrTarget::Source ? Controller->SourceSkelMeshComponent : Controller->TargetSkelMeshComponent;
}

void FIKRetargetDefaultMode::ApplyOffsetToMeshTransform(const FVector& Offset, USceneComponent* Component)
{
	constexpr bool bSweep = false;
	constexpr FHitResult* OutSweepHitResult = nullptr;
	constexpr ETeleportType Teleport = ETeleportType::ResetPhysics;
	Component->SetWorldLocation(Offset, bSweep, OutSweepHitResult, Teleport);
}

void FIKRetargetDefaultMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
	
	CurrentWidgetMode = ViewportClient->GetWidgetMode();

	// ensure selection callbacks have been generated
	if (!bIsInitialized)
	{
		Initialize();
	}
}

#undef LOCTEXT_NAMESPACE
