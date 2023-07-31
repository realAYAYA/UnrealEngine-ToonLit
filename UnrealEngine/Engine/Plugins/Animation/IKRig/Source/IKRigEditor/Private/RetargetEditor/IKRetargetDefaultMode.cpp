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

	// center the view on the last selected item
	switch (Controller->GetLastSelectedItemType())
	{
	case ERetargetSelectionType::BONE:
		{
			// target the selected bones
			const TArray<FName>& SelectedBoneNames = Controller->GetSelectedBones();
			if (SelectedBoneNames.IsEmpty())
			{
				return false;
			}

			TArray<FVector> TargetPoints;
			const UDebugSkelMeshComponent* SkeletalMeshComponent = Controller->GetSkeletalMeshComponent(Controller->GetSourceOrTarget());
			const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent->GetReferenceSkeleton();
			TArray<int32> ChildrenIndices;
			for (const FName& SelectedBoneName : SelectedBoneNames)
			{
				const int32 BoneIndex = RefSkeleton.FindBoneIndex(SelectedBoneName);
				if (BoneIndex == INDEX_NONE)
				{
					continue;
				}

				TargetPoints.Add(SkeletalMeshComponent->GetBoneTransform(BoneIndex).GetLocation());
				ChildrenIndices.Reset();
				RefSkeleton.GetDirectChildBones(BoneIndex, ChildrenIndices);
				for (const int32 ChildIndex : ChildrenIndices)
				{
					TargetPoints.Add(SkeletalMeshComponent->GetBoneTransform(ChildIndex).GetLocation());
				}
			}
	
			// create a sphere that contains all the target points
			if (TargetPoints.Num() == 0)
			{
				TargetPoints.Add(FVector::ZeroVector);
			}
			OutTarget = FSphere(&TargetPoints[0], TargetPoints.Num());
			return true;
		}
		
	case ERetargetSelectionType::CHAIN:
		{
			const ERetargetSourceOrTarget SourceOrTarget = Controller->GetSourceOrTarget();
			const UIKRigDefinition* IKRig = Controller->AssetController->GetIKRig(SourceOrTarget);
			if (!IKRig)
			{
				return false;
			}

			const UDebugSkelMeshComponent* SkeletalMeshComponent = Controller->GetSkeletalMeshComponent(Controller->GetSourceOrTarget());
			const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent->GetReferenceSkeleton();

			// get target points from start/end bone of all selected chains on the currently active skeleton (source or target)
			TArray<FVector> TargetPoints;
			const TArray<FName>& SelectedChains = Controller->GetSelectedChains();
			for (const FName SelectedChainName : SelectedChains)
			{
				const URetargetChainSettings* ChainMap = Controller->AssetController->GetChainMappingByTargetChainName(SelectedChainName);
				if (!ChainMap)
				{
					continue;
				}

				const FName& ChainName = SourceOrTarget == ERetargetSourceOrTarget::Target ? SelectedChainName : ChainMap->SourceChain;
				if (ChainName == NAME_None)
				{
					continue;
				}

				const FBoneChain* BoneChain = IKRig->GetRetargetChainByName(ChainName);
				if (!BoneChain)
				{
					continue;
				}

				const int32 StartBoneIndex = RefSkeleton.FindBoneIndex(BoneChain->StartBone.BoneName);
				if (StartBoneIndex != INDEX_NONE)
				{
					TargetPoints.Add(SkeletalMeshComponent->GetBoneTransform(StartBoneIndex).GetLocation());
				}

				const int32 EndBoneIndex = RefSkeleton.FindBoneIndex(BoneChain->EndBone.BoneName);
				if (EndBoneIndex != INDEX_NONE)
				{
					TargetPoints.Add(SkeletalMeshComponent->GetBoneTransform(EndBoneIndex).GetLocation());
				}
			}

			// create a sphere that contains all the target points
			if (TargetPoints.Num() == 0)
			{
				TargetPoints.Add(FVector::ZeroVector);
			}
			OutTarget = FSphere(&TargetPoints[0], TargetPoints.Num());
			return true;	
		}
	case ERetargetSelectionType::ROOT:
	case ERetargetSelectionType::MESH:
	case ERetargetSelectionType::NONE:
	default:
		// target the current mesh
		if (const UPrimitiveComponent* SelectedMesh = Controller->GetSkeletalMeshComponent(Controller->GetSourceOrTarget()))
		{
			OutTarget = SelectedMesh->Bounds.GetSphere();
			return true;
		}
	}

	return false;
}

IPersonaPreviewScene& FIKRetargetDefaultMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

void FIKRetargetDefaultMode::GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const
{
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
		Controller->SetSelectedMesh(const_cast<UPrimitiveComponent*>(ActorProxy->PrimComponent));
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

bool FIKRetargetDefaultMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
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

void FIKRetargetDefaultMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);
}

#undef LOCTEXT_NAMESPACE
