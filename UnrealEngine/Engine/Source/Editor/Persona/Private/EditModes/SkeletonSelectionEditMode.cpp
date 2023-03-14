// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditModes/SkeletonSelectionEditMode.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimationEditorViewportClient.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "AnimPreviewInstance.h"
#include "ISkeletonTree.h"
#include "AssetEditorModeManager.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EngineUtils.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/WindDirectionalSource.h"

#include "IPersonaToolkit.h"
#include "IEditableSkeleton.h"

#define LOCTEXT_NAMESPACE "SkeletonSelectionEditMode"

namespace SkeletonSelectionModeConstants
{
	/** Distance to trace for physics bodies */
	static const float BodyTraceDistance = 10000.0f;
}

FSkeletonSelectionEditMode::FSkeletonSelectionEditMode()
	: bManipulating(false)
	, bInTransaction(false)
{
	// Disable grid drawing for this mode as the viewport handles this
	bDrawGrid = false;
}

bool FSkeletonSelectionEditMode::GetCameraTarget(FSphere& OutTarget) const
{
	bool bHandled = false;

	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();

	if(PreviewMeshComponent && PreviewMeshComponent->GetSkeletalMeshAsset())
	{
		if (GetAnimPreviewScene().GetSelectedBoneIndex() != INDEX_NONE)
		{
			const int32 FocusBoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
			if (FocusBoneIndex != INDEX_NONE && PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().IsValidIndex(FocusBoneIndex))
			{
				const FName BoneName = PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(FocusBoneIndex);
				OutTarget.Center = PreviewMeshComponent->GetBoneLocation(BoneName);
				OutTarget.W = 30.0f;
				bHandled = true;
			}
		}

		if (!bHandled && GetAnimPreviewScene().GetSelectedSocket().IsValid())
		{
			USkeletalMeshSocket * Socket = GetAnimPreviewScene().GetSelectedSocket().Socket;
			if (Socket)
			{
				OutTarget.Center = Socket->GetSocketLocation(PreviewMeshComponent);
				OutTarget.W = 30.0f;
				bHandled = true;
			}
		}
	}

	return bHandled;
}

IPersonaPreviewScene& FSkeletonSelectionEditMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

void FSkeletonSelectionEditMode::GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const
{

}

FSelectedSocketInfo FSkeletonSelectionEditMode::DuplicateAndSelectSocket(const FSelectedSocketInfo& SocketInfoToDuplicate)
{
	USkeletalMesh* SkeletalMesh = GetAnimPreviewScene().GetPreviewMeshComponent()->GetSkeletalMeshAsset();
	USkeletalMeshSocket* NewSocket = GetAnimPreviewScene().GetPersonaToolkit()->GetEditableSkeleton()->DuplicateSocket(SocketInfoToDuplicate, SocketInfoToDuplicate.Socket->BoneName, SkeletalMesh);

	FSelectedSocketInfo NewSocketInfo(NewSocket, SocketInfoToDuplicate.bSocketIsOnSkeleton);
	GetAnimPreviewScene().DeselectAll();
	GetAnimPreviewScene().SetSelectedSocket(NewSocketInfo);

	return NewSocketInfo;
}

bool FSkeletonSelectionEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	UE::Widget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();

	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if(PreviewMeshComponent != nullptr && PreviewMeshComponent->GetSkeletalMeshAsset() != nullptr)
	{
		int32 BoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
		USkeletalMeshSocket* SelectedSocket = GetAnimPreviewScene().GetSelectedSocket().Socket;
		AActor* SelectedActor = GetAnimPreviewScene().GetSelectedActor();

		if ((BoneIndex >= 0 && PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().IsValidIndex(BoneIndex)) || SelectedSocket != nullptr || SelectedActor != nullptr)
		{
			bool bValidAxis = false;
			FVector WorldAxisDir;

			if ( ((CurrentAxis & EAxisList::XYZ) | (CurrentAxis & EAxisList::Screen)) != 0)
			{
				FSelectedSocketInfo SelectedSocketInfo = GetAnimPreviewScene().GetSelectedSocket();
				if (SelectedSocketInfo.IsValid())
				{
					const bool bAltDown = InViewportClient->IsAltPressed();

					if (bAltDown)
					{
						// Rather than moving/rotating the selected socket, copy it and move the copy instead
						SelectedSocketInfo = DuplicateAndSelectSocket(SelectedSocketInfo);
					}

					// Socket movement is transactional - we want undo/redo and saving of it
					USkeletalMeshSocket* Socket = SelectedSocketInfo.Socket;

					if (Socket && bInTransaction == false)
					{
						if (WidgetMode == UE::Widget::WM_Rotate)
						{
							GEditor->BeginTransaction(LOCTEXT("AnimationEditorViewport_RotateSocket", "Rotate Socket"));
						}
						else
						{
							GEditor->BeginTransaction(LOCTEXT("AnimationEditorViewport_TranslateSocket", "Translate Socket"));
						}

						Socket->SetFlags(RF_Transactional);	// Undo doesn't work without this!
						Socket->Modify();
						bInTransaction = true;
					}
				}
				else if (BoneIndex >= 0)
				{
					if (bInTransaction == false)
					{
						// we also allow undo/redo of bone manipulations
						if (WidgetMode == UE::Widget::WM_Rotate)
						{
							GEditor->BeginTransaction(LOCTEXT("AnimationEditorViewport_RotateBone", "Rotate Bone"));
						}
						else
						{
							GEditor->BeginTransaction(LOCTEXT("AnimationEditorViewport_TranslateBone", "Translate Bone"));
						}

						PreviewMeshComponent->PreviewInstance->SetFlags(RF_Transactional);	// Undo doesn't work without this!
						PreviewMeshComponent->PreviewInstance->Modify();
						bInTransaction = true;

						// now modify the bone array
						const FName BoneName = PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex);
						PreviewMeshComponent->PreviewInstance->ModifyBone(BoneName);
					}
				}
			}

			bManipulating = true;
			return true;
		}
	}
	
	return false;
}

bool FSkeletonSelectionEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (bManipulating)
	{
		// Socket movement is transactional - we want undo/redo and saving of it
		if (bInTransaction)
		{
			GEditor->EndTransaction();
			bInTransaction = false;
		}

		bManipulating = false;
		return true;
	}

	return false;
}

bool FSkeletonSelectionEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();
	const UE::Widget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();
	const ECoordSystem CoordSystem = InViewportClient->GetWidgetCoordSystemSpace();

	// Get some useful info about buttons being held down
	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
	const bool bMouseButtonDown = InViewport->KeyState( EKeys::LeftMouseButton ) || InViewport->KeyState( EKeys::MiddleMouseButton ) || InViewport->KeyState( EKeys::RightMouseButton );

	bool bHandled = false;

	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();

	if ( bManipulating && CurrentAxis != EAxisList::None )
	{
		bHandled = true;

		int32 BoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
		USkeletalMeshSocket* SelectedSocket = GetAnimPreviewScene().GetSelectedSocket().Socket;
		AActor* SelectedActor = GetAnimPreviewScene().GetSelectedActor();
		FAnimNode_ModifyBone* SkelControl = nullptr;

		if ( BoneIndex >= 0 )
		{
			//Get the skeleton control manipulating this bone
			const FName BoneName = PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex);
			SkelControl = &(PreviewMeshComponent->PreviewInstance->ModifyBone(BoneName));
		}

		if ( SkelControl || SelectedSocket )
		{
			FTransform CurrentSkelControlTM(
				SelectedSocket ? SelectedSocket->RelativeRotation : SkelControl->Rotation,
				SelectedSocket ? SelectedSocket->RelativeLocation : SkelControl->Translation,
				SelectedSocket ? SelectedSocket->RelativeScale : SkelControl->Scale);

			FTransform BaseTM;

			if ( SelectedSocket )
			{
				BaseTM = SelectedSocket->GetSocketTransform( PreviewMeshComponent );
			}
			else
			{
				BaseTM = PreviewMeshComponent->GetBoneTransform( BoneIndex );
			}

			// Remove SkelControl's orientation from BoneMatrix, as we need to translate/rotate in the non-SkelControlled space
			BaseTM = BaseTM.GetRelativeTransformReverse(CurrentSkelControlTM);

			const bool bDoRotation    = WidgetMode == UE::Widget::WM_Rotate    || WidgetMode == UE::Widget::WM_TranslateRotateZ;
			const bool bDoTranslation = WidgetMode == UE::Widget::WM_Translate || WidgetMode == UE::Widget::WM_TranslateRotateZ;
			const bool bDoScale = WidgetMode == UE::Widget::WM_Scale;

			if (bDoRotation)
			{
				FVector RotAxis;
				float RotAngle;
				InRot.Quaternion().ToAxisAndAngle( RotAxis, RotAngle );

				FVector4 BoneSpaceAxis = BaseTM.TransformVectorNoScale( RotAxis );

				//Calculate the new delta rotation
				FQuat DeltaQuat( BoneSpaceAxis, RotAngle );
				DeltaQuat.Normalize();

				FRotator NewRotation = ( CurrentSkelControlTM * FTransform( DeltaQuat )).Rotator();

				if ( SelectedSocket )
				{
					SelectedSocket->RelativeRotation = NewRotation;
				}
				else
				{
					SkelControl->Rotation = NewRotation;
				}
			}

			if (bDoTranslation)
			{
				FVector4 BoneSpaceOffset = BaseTM.TransformVector(InDrag);
				if (SelectedSocket)
				{
					SelectedSocket->RelativeLocation += BoneSpaceOffset;
				}
				else
				{
					SkelControl->Translation += BoneSpaceOffset;
				}
			}
			if(bDoScale)
			{
				FVector4 BoneSpaceScaleOffset;

				if (CoordSystem == COORD_World)
				{
					BoneSpaceScaleOffset = BaseTM.TransformVector(InScale);
				}
				else
				{
					BoneSpaceScaleOffset = InScale;
				}

				if(SelectedSocket)
				{
					SelectedSocket->RelativeScale += BoneSpaceScaleOffset;
				}
				else
				{
					SkelControl->Scale += BoneSpaceScaleOffset;
				}
			}

		}
		else if( SelectedActor != nullptr )
		{
			if (WidgetMode == UE::Widget::WM_Rotate)
			{
				FTransform Transform = SelectedActor->GetTransform();
				FRotator NewRotation = (Transform * FTransform( InRot ) ).Rotator();

				SelectedActor->SetActorRotation( NewRotation );
			}
			else
			{
				FVector Location = SelectedActor->GetActorLocation();
				Location += InDrag;
				SelectedActor->SetActorLocation(Location);
			}
		}

		InViewport->Invalidate();
	}

	return bHandled;
}

FIntPoint FSkeletonSelectionEditMode::GetDPIUnscaledSize(FViewport* Viewport, FViewportClient* Client)
{
	const FIntPoint Size = Viewport->GetSizeXY();
	const float DPIScale = Client->GetDPIScale();
	// (FIntPoint / float) implicitly casts the float to an int if you try to divide it directly
	return FIntPoint(Size.X / DPIScale, Size.Y / DPIScale);
}

void FSkeletonSelectionEditMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (PreviewMeshComponent == nullptr || PreviewMeshComponent->GetSkeletalMeshAsset() == nullptr || PreviewMeshComponent->GetSkeletalMeshAsset()->IsCompiling())
	{
		return;
	}

	FReferenceSkeleton& RefSkeleton = PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	int32 BoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();

	// Draw name of selected bone
	if (RefSkeleton.IsValidIndex(BoneIndex) && (IsSelectedBoneRequired() || RefSkeleton.GetRequiredVirtualBones().Contains(BoneIndex)))
	{
		const FIntPoint ViewPortSize = GetDPIUnscaledSize(Viewport, ViewportClient);
		const int32 HalfX = ViewPortSize.X / 2;
		const int32 HalfY = ViewPortSize.Y / 2;

		const FName BoneName = PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex);

		FMatrix BoneMatrix = PreviewMeshComponent->GetBoneMatrix(BoneIndex);
		const FPlane Proj = View->Project(BoneMatrix.GetOrigin());
		if (Proj.W > 0.f)
		{
			const int32 XPos = HalfX + (HalfX * Proj.X);
			const int32 YPos = HalfY + (HalfY * (Proj.Y * -1));

			FQuat BoneQuat = PreviewMeshComponent->GetBoneQuaternion(BoneName);
			FVector Loc = PreviewMeshComponent->GetBoneLocation(BoneName);
			FCanvasTextItem TextItem(FVector2D(XPos, YPos), FText::FromString(BoneName.ToString()), GEngine->GetSmallFont(), FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}

	// Draw name of selected socket
	if (GetAnimPreviewScene().GetSelectedSocket().IsValid())
	{
		USkeletalMeshSocket* Socket = GetAnimPreviewScene().GetSelectedSocket().Socket;

		FMatrix SocketMatrix;
		Socket->GetSocketMatrix(SocketMatrix, PreviewMeshComponent);
		const FVector SocketPos = SocketMatrix.GetOrigin();

		const FPlane Proj = View->Project(SocketPos);
		if (Proj.W > 0.f)
		{
			const FIntPoint ViewPortSize = GetDPIUnscaledSize(Viewport, ViewportClient);
			const int32 HalfX = ViewPortSize.X / 2;
			const int32 HalfY = ViewPortSize.Y / 2;

			const int32 XPos = HalfX + (HalfX * Proj.X);
			const int32 YPos = HalfY + (HalfY * (Proj.Y * -1));
			FCanvasTextItem TextItem(FVector2D(XPos, YPos), FText::FromString(Socket->SocketName.ToString()), GEngine->GetSmallFont(), FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}
}

bool FSkeletonSelectionEditMode::AllowWidgetMove()
{
	return ShouldDrawWidget();
}

bool FSkeletonSelectionEditMode::IsSelectedBoneRequired() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	int32 SelectedBoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
	if (SelectedBoneIndex != INDEX_NONE && PreviewMeshComponent->GetSkeletalMeshRenderData())
	{
		//Get current LOD
		FSkeletalMeshRenderData* SkelMeshRenderData = PreviewMeshComponent->GetSkeletalMeshRenderData();
		if(SkelMeshRenderData->LODRenderData.Num() > 0)
		{
			const int32 LODIndex = FMath::Clamp(PreviewMeshComponent->GetPredictedLODLevel(), 0, SkelMeshRenderData->LODRenderData.Num() - 1);
			FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];

			//Check whether the bone is vertex weighted
			return LODData.RequiredBones.Find(SelectedBoneIndex) != INDEX_NONE;
		}
	}

	return false;
}

bool FSkeletonSelectionEditMode::ShouldDrawWidget() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (!PreviewMeshComponent->IsAnimBlueprintInstanced())
	{
		return IsSelectedBoneRequired() || GetAnimPreviewScene().GetSelectedSocket().IsValid() || GetAnimPreviewScene().GetSelectedActor() != nullptr;
	}

	return false;
}

bool FSkeletonSelectionEditMode::UsesTransformWidget() const
{
	return true;
}

bool FSkeletonSelectionEditMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	return ShouldDrawWidget() && (CheckMode == UE::Widget::WM_Scale || CheckMode == UE::Widget::WM_Translate || CheckMode == UE::Widget::WM_Rotate);
}

bool FSkeletonSelectionEditMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if(PreviewMeshComponent && PreviewMeshComponent->GetSkeletalMeshAsset() && !PreviewMeshComponent->GetSkeletalMeshAsset()->IsCompiling())
	{
		int32 BoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
		if (BoneIndex != INDEX_NONE && PreviewMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton().IsValidIndex(BoneIndex))
		{
			FTransform BoneMatrix = PreviewMeshComponent->GetBoneTransform(BoneIndex);

			InMatrix = BoneMatrix.ToMatrixNoScale().RemoveTranslation();
			return true;
		}
		else if (GetAnimPreviewScene().GetSelectedSocket().IsValid())
		{
			USkeletalMeshSocket* Socket = GetAnimPreviewScene().GetSelectedSocket().Socket;

			FTransform SocketMatrix = Socket->GetSocketTransform(PreviewMeshComponent);

			InMatrix = SocketMatrix.ToMatrixNoScale().RemoveTranslation();
			return true;
		}
		else if (AActor* SelectedActor = GetAnimPreviewScene().GetSelectedActor())
		{
			InMatrix = SelectedActor->GetTransform().ToMatrixNoScale().RemoveTranslation();
			return true;
		}
	}
	
	return false;
}

bool FSkeletonSelectionEditMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	return GetCustomDrawingCoordinateSystem(InMatrix, InData);
}

FVector FSkeletonSelectionEditMode::GetWidgetLocation() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	USkeletalMesh* SkeletalMesh = PreviewMeshComponent ? PreviewMeshComponent->GetSkeletalMeshAsset() : nullptr;
	const bool bAssetIsCompiling = SkeletalMesh ? SkeletalMesh->IsCompiling() : false;

	int32 BoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
	if (BoneIndex != INDEX_NONE)
	{
		if (!bAssetIsCompiling
			&& SkeletalMesh
			&& SkeletalMesh->GetRefSkeleton().IsValidIndex(BoneIndex))
		{
			const FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex);

			const FMatrix BoneMatrix = PreviewMeshComponent->GetBoneMatrix(BoneIndex);

			return BoneMatrix.GetOrigin();
		}		
	}
	else if (GetAnimPreviewScene().GetSelectedSocket().IsValid())
	{
		if (!bAssetIsCompiling)
		{
			USkeletalMeshSocket* Socket = GetAnimPreviewScene().GetSelectedSocket().Socket;

			FMatrix SocketMatrix;
			Socket->GetSocketMatrix(SocketMatrix, PreviewMeshComponent);

			return SocketMatrix.GetOrigin();
		}
	}
	else if (AActor* SelectedActor = GetAnimPreviewScene().GetSelectedActor())
	{
		return SelectedActor->GetActorLocation();
	}

	return FVector::ZeroVector;
}

bool FSkeletonSelectionEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	GetAnimPreviewScene().DeselectAll();
	USkeletalMeshComponent* MeshComponent = GetAnimPreviewScene().GetPreviewMeshComponent();
	if (MeshComponent)
	{
		MeshComponent->SetSelectedEditorSection(INDEX_NONE);
	}

	if (!HitProxy)
	{
		return false;
	}

	if (HPersonaBoneHitProxy* BoneHitProxy = HitProxyCast<HPersonaBoneHitProxy>(HitProxy))
	{
		GetAnimPreviewScene().SetSelectedBone(BoneHitProxy->BoneName, ESelectInfo::OnMouseClick);
		return true;
	}
	if (HPersonaSocketHitProxy* SocketHitProxy = HitProxyCast<HPersonaSocketHitProxy>(HitProxy))
	{
		if (USkeleton* Skeleton = GetAnimPreviewScene().GetPersonaToolkit()->GetSkeleton())
		{
			FSelectedSocketInfo SocketInfo;
			SocketInfo.Socket = SocketHitProxy->Socket;
			SocketInfo.bSocketIsOnSkeleton = !SocketInfo.Socket->GetOuter()->IsA<USkeletalMesh>();
			GetAnimPreviewScene().SetSelectedSocket(SocketInfo);
			return true;
		}
	}
	if (HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
	{
		if (ActorHitProxy->Actor)
		{
			if (ActorHitProxy->Actor->IsA<AWindDirectionalSource>())
			{
				AWindDirectionalSource* WindSourceActor = CastChecked<AWindDirectionalSource>(ActorHitProxy->Actor);
				if (WindSourceActor->IsSelectable())
				{
					GetAnimPreviewScene().SetSelectedActor(WindSourceActor);
					return true;
				}
			}
		}
		GetAnimPreviewScene().BroadcastMeshClick(ActorHitProxy, Click); // This can pop up menu which redraws viewport and invalidates HitProxy!
		return true;
	}

	return false;
}

bool FSkeletonSelectionEditMode::CanCycleWidgetMode() const
{
	int32 SelectedBoneIndex = GetAnimPreviewScene().GetSelectedBoneIndex();
	USkeletalMeshSocket* SelectedSocket = GetAnimPreviewScene().GetSelectedSocket().Socket;
	AActor* SelectedActor = GetAnimPreviewScene().GetSelectedActor();

	return (SelectedBoneIndex >= 0 || SelectedSocket || SelectedActor != nullptr);
}

#undef LOCTEXT_NAMESPACE
