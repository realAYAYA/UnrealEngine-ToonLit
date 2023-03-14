// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusHandComponent.h"
#include "OculusInput.h"

#include "Engine/SkeletalMesh.h"
#include "Components/InputComponent.h"
#include "Materials/MaterialInterface.h"

#include "GameFramework/PlayerController.h"

const FQuat HandRootFixupRotation = FQuat(-0.5f, -0.5f, 0.5f, 0.5f);

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_UOculusHandComponent::UDEPRECATED_UOculusHandComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bHasAuthority = false;
	bAutoActivate = true;

	bWantsInitializeComponent = true;

	for (uint8 BoneIndex = 0; BoneIndex < (uint8)EBone::Bone_Max; BoneIndex++)
	{
		BoneNameMappings.Add((EBone)BoneIndex, TEXT(""));
	}
}

void UDEPRECATED_UOculusHandComponent::BeginPlay()
{
	Super::BeginPlay();

	// Use custom mesh if a skeletal mesh is already set, else try to load the runtime mesh
	if (GetSkinnedAsset())
	{
		bCustomHandMesh = true;
		bSkeletalMeshInitialized = true;
	}
	else
	{
		RuntimeSkeletalMesh = NewObject<USkeletalMesh>(this, TEXT("OculusHandMesh"));
		InitializeSkeletalMesh();
	}
}

void UDEPRECATED_UOculusHandComponent::InitializeSkeletalMesh()
{
	if (RuntimeSkeletalMesh)
	{
		if (UDEPRECATED_UOculusInputFunctionLibrary::GetHandSkeletalMesh(RuntimeSkeletalMesh, SkeletonType, MeshType))
		{
			SetSkinnedAssetAndUpdate(RuntimeSkeletalMesh, true);
			if (MaterialOverride)
			{
				SetMaterial(0, MaterialOverride);
			}
			CachedBaseMaterial = GetMaterial(0);
			bSkeletalMeshInitialized = true;

			// Initialize physics capsules on the runtime mesh
			if (bInitializePhysics)
			{
				CollisionCapsules = UDEPRECATED_UOculusInputFunctionLibrary::InitializeHandPhysics(SkeletonType, this);
			}
		}
	}
}

void UDEPRECATED_UOculusHandComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_EDITOR
	if (!bSkeletalMeshInitialized && !bCustomHandMesh)
	{
		InitializeSkeletalMesh();
	}
#endif

	if (IsInGameThread())
	{
		// Cache state from the game thread for use on the render thread
		const AActor* MyOwner = GetOwner();
		bHasAuthority = MyOwner->HasLocalNetOwner();
	}

	if (bHasAuthority)
	{
		bool bHidden = false;
		if (UDEPRECATED_UOculusInputFunctionLibrary::IsHandTrackingEnabled())
		{
			// Update Visibility based on Confidence
			if (ConfidenceBehavior == EConfidenceBehavior::HideActor)
			{
				ETrackingConfidence TrackingConfidence = UDEPRECATED_UOculusInputFunctionLibrary::GetTrackingConfidence(SkeletonType);
				bHidden |= TrackingConfidence != ETrackingConfidence::High;
			}

			// Update Hand Scale
			if (bUpdateHandScale)
			{
				float NewScale = UDEPRECATED_UOculusInputFunctionLibrary::GetHandScale(SkeletonType);
				SetRelativeScale3D(FVector(NewScale));
			}

			// Update Bone Pose Rotations
			if (GetSkinnedAsset())
			{
				UpdateBonePose();
			}

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
			// Check for system gesture pressed through player controller
			if (APawn* Pawn = Cast<APawn>(GetOwner()))
			{
				if (APlayerController* PC = Pawn->GetController<APlayerController>())
				{
					if (PC->WasInputKeyJustPressed(SkeletonType == EOculusHandType::HandLeft ? OculusInput::FOculusKey::OculusHand_Left_SystemGesture : OculusInput::FOculusKey::OculusHand_Right_SystemGesture))
					{
						SystemGesturePressed();
					}
					if (PC->WasInputKeyJustReleased(SkeletonType == EOculusHandType::HandLeft ? OculusInput::FOculusKey::OculusHand_Left_SystemGesture : OculusInput::FOculusKey::OculusHand_Right_SystemGesture))
					{
						SystemGestureReleased();
					}
				}
			}
#endif
		}
		else
		{
			bHidden = true;
		}

		if (bHidden != bHiddenInGame)
		{
			SetHiddenInGame(bHidden);
			for (int32 i = 0; i < CollisionCapsules.Num(); i++)
			{
				CollisionCapsules[i].Capsule->SetCollisionEnabled(bHidden ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
			}
		}
	}
}

void UDEPRECATED_UOculusHandComponent::UpdateBonePose()
{
	if (bCustomHandMesh)
	{
		for (auto& BoneElem : BoneNameMappings)
		{
			// Set Root Bone Rotaiton
			if (BoneElem.Key == EBone::Wrist_Root)
			{
				FQuat RootBoneRotation = UDEPRECATED_UOculusInputFunctionLibrary::GetBoneRotation(SkeletonType, EBone::Wrist_Root);
				RootBoneRotation *= HandRootFixupRotation;
				RootBoneRotation.Normalize();
				BoneSpaceTransforms[0].SetRotation(RootBoneRotation);

			}
			else
			{
				// Set Remaing Bone Rotations
				int32 BoneIndex = GetSkinnedAsset()->GetRefSkeleton().FindBoneIndex(BoneElem.Value);
				if (BoneIndex >= 0)
				{
					FQuat BoneRotation = UDEPRECATED_UOculusInputFunctionLibrary::GetBoneRotation(SkeletonType, (EBone)BoneElem.Key);
					BoneSpaceTransforms[BoneIndex].SetRotation(BoneRotation);
				}
			}
		}
	}
	else
	{
		// Set Root Bone Rotation
		FQuat RootBoneRotation = UDEPRECATED_UOculusInputFunctionLibrary::GetBoneRotation(SkeletonType, EBone::Wrist_Root);
		RootBoneRotation *= HandRootFixupRotation;
		RootBoneRotation.Normalize();
		BoneSpaceTransforms[0].SetRotation(RootBoneRotation);

		// Set Remaining Bone Rotations
		for (uint32 BoneIndex = 1; BoneIndex < (uint32)GetSkinnedAsset()->GetRefSkeleton().GetNum(); BoneIndex++)
		{
			FQuat BoneRotation = UDEPRECATED_UOculusInputFunctionLibrary::GetBoneRotation(SkeletonType, (EBone)BoneIndex);
			BoneSpaceTransforms[BoneIndex].SetRotation(BoneRotation);
		}
	}
	MarkRefreshTransformDirty();
}

void UDEPRECATED_UOculusHandComponent::SystemGesturePressed()
{
	if (SystemGestureBehavior == ESystemGestureBehavior::SwapMaterial)
	{
		if (SystemGestureMaterial)
		{
			SetMaterial(0, SystemGestureMaterial);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("System Gesture Behavior was set to Swap Material but no System Gesture Material was provided!"));
		}
	}
}

void UDEPRECATED_UOculusHandComponent::SystemGestureReleased()
{
	if (SystemGestureBehavior == ESystemGestureBehavior::SwapMaterial)
	{
		if (CachedBaseMaterial)
		{
			SetMaterial(0, CachedBaseMaterial);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("System Gesture Behavior was set to Swap Material but no System Gesture Material was provided!"));
		}
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
