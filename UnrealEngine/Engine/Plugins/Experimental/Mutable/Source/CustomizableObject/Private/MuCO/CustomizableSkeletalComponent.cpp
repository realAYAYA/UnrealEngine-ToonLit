// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalComponent.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableInstancePrivateData.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ObjectSaveContext.h"
#include "MuCO/UnrealPortabilityHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableSkeletalComponent)


UCustomizableSkeletalComponent::UCustomizableSkeletalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	//bTickInEditor = true;
}


void UCustomizableSkeletalComponent::Callbacks() const
{
	UpdatedDelegate.ExecuteIfBound();
}


USkeletalMesh* UCustomizableSkeletalComponent::GetSkeletalMesh() const
{
	return CustomizableObjectInstance ? CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex) : nullptr;
}

void UCustomizableSkeletalComponent::SetSkeletalMesh(USkeletalMesh* SkeletalMesh, bool bReinitPose, bool bForceClothReset)
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	// LINUX_PLATFORM needs a more aggressive workaround so morph and cloth glitches
	// are not visible. 
	// TODO: Try to find a better way of setting BP Morphs and clothing and remove
	// the workaround.  
	if (Parent)
	{
#if PLATFORM_LINUX
		const bool bDisableClothSimulation = Parent->bDisableClothSimulation;
		if (bForceClothReset)
		{
			Parent->bDisableClothSimulation = true;
		}
#endif

		if(UE_MUTABLE_GETSKINNEDASSET(Parent) == SkeletalMesh)
		{
			Parent->RecreateRenderState_Concurrent();
		}
		
		TMap<FName, float> MorphTargetCurves = Parent->GetMorphTargetCurves();
		PreUpdateDelegate.ExecuteIfBound(this, SkeletalMesh);
		Parent->SetSkeletalMesh(SkeletalMesh, bReinitPose);
		
		// USkeletalMeshCompoent MorphTargetCurves are reset when SetSkeletalMesh is called.
		// Re-enable them if not bReinitPose.
		if (!bReinitPose && MorphTargetCurves.Num() > 0)
		{
			for (const TPair<FName, float>& MorphTarget : MorphTargetCurves)
			{
				Parent->SetMorphTarget(MorphTarget.Key, MorphTarget.Value);
			}

#if PLATFORM_LINUX
			FRenderStateRecreator RenderStateRecreator(Parent);
			FAnimationRuntime::AppendActiveMorphTargets(SkeletalMesh, Parent->GetMorphTargetCurves(), Parent->ActiveMorphTargets, Parent->MorphTargetWeights);		
#endif
		}

		if (bForceClothReset)
		{
			Parent->ForceClothNextUpdateTeleportAndReset();

#if PLATFORM_LINUX
			Parent->bDisableClothSimulation = bDisableClothSimulation;
#endif
		}

		if (Parent->GetNumOverrideMaterials() > 0)
		{
			// For some reason the reference skeletal mesh materials are added as override materials, clear them if necessary
			Parent->EmptyOverrideMaterials();
		}	
	}
}

void UCustomizableSkeletalComponent::SetPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent && Parent->GetWorld())
	{
		Parent->SetPhysicsAsset(PhysicsAsset, true);
	}
}


USkeletalMesh* UCustomizableSkeletalComponent::GetAttachedSkeletalMesh() const
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent)
	{
		return UE_MUTABLE_GETSKELETALMESHASSET(Parent);
	}

	return nullptr;
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsync(false, false);
	}
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsyncResult(Callback, false, false);
	}
}


void UCustomizableSkeletalComponent::BeginDestroy()
{
	Super::BeginDestroy();
}


#if WITH_EDITOR

void UCustomizableSkeletalComponent::UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition)
{
	// We want instances in the editor to be generated
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor)
		return;

	if (CustomizableObjectInstance)
	{
		AActor* ParentActor = GetAttachmentRootActor();
		if (ParentActor && ParentActor->IsValidLowLevel())
		{
			// update distance to camera and set the instance as being used by a component
			CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(UsedByComponent);

			float SquareDist = FVector::DistSquared(CameraPosition, ParentActor->GetActorLocation());
			CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = 
				FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
		}

		USkeletalMesh* AttachedSkeletalMesh = GetAttachedSkeletalMesh();
		USkeletalMesh* GeneratedSKeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);
		if (!AttachedSkeletalMesh && !CustomizableObjectInstance->GetPrivate()->HasCOInstanceFlags(CreatingSkeletalMesh))
		{
			UCustomizableObject* Object = CustomizableObjectInstance->GetCustomizableObject(); 
			USkeletalMesh* RefMesh = Object ? Object->GetRefSkeletalMesh(ComponentIndex) : nullptr;
			SetSkeletalMesh(GeneratedSKeletalMesh ? GeneratedSKeletalMesh : RefMesh);
		}
		else if (GeneratedSKeletalMesh && AttachedSkeletalMesh != GeneratedSKeletalMesh)
		{
			SetSkeletalMesh(GeneratedSKeletalMesh);
		}
	}
}


void UCustomizableSkeletalComponent::EditorUpdateComponent()
{
	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags((ECOInstanceFlags)(UsedByComponent | ForceGenerateAllLODs));

		AActor* ParentActor = GetAttachmentRootActor();
		if (ParentActor)
		{
			USkeletalMesh* AttachedSkeletalMesh = GetAttachedSkeletalMesh();
			USkeletalMesh* GeneratedSKeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);
			if (!AttachedSkeletalMesh && !CustomizableObjectInstance->GetPrivate()->HasCOInstanceFlags(CreatingSkeletalMesh))
			{
				SetSkeletalMesh(GeneratedSKeletalMesh ? GeneratedSKeletalMesh : CustomizableObjectInstance->GetCustomizableObject()->GetRefSkeletalMesh(ComponentIndex));
			}
			else if (GeneratedSKeletalMesh && AttachedSkeletalMesh != GeneratedSKeletalMesh)
			{
				SetSkeletalMesh(GeneratedSKeletalMesh);
			}
		}
	}
}
#endif


void UCustomizableSkeletalComponent::SetVisibilityOfSkeletalMeshSectionWithMaterialName(bool bInVisible, const FString& MaterialName, int32 LOD)
{
	USkeletalMesh* SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);

	if (!SkeletalMesh)
	{
		return;
	}

	for (int32 i = 0; i < SkeletalMesh->GetMaterials().Num(); ++i)
	{
		UMaterialInstanceDynamic* MatInst = Cast<UMaterialInstanceDynamic>(SkeletalMesh->GetMaterials()[i].MaterialInterface);

		if (MatInst && MatInst->Parent->GetName() == MaterialName)
		{
			if (FSkeletalMeshRenderData* SkelResource = SkeletalMesh->GetResourceForRendering() )
			{
				if ( SkelResource->LODRenderData.IsValidIndex(LOD))
				{
					FSkeletalMeshLODRenderData& LodData = SkelResource->LODRenderData[LOD];

					for (int32 j = 0; j < LodData.RenderSections.Num(); ++j)
					{
						FSkelMeshRenderSection& Section = LodData.RenderSections[j];

						if (Section.MaterialIndex == i)
						{
							Section.bDisabled = !bInVisible;
						}
					}
				}
			}
		}
	}
}


void UCustomizableSkeletalComponent::UpdateDistFromComponentToPlayer(const AActor* ViewCenter, const bool bForceEvenIfNotBegunPlay)
{
	if (CustomizableObjectInstance)
	{
		AActor* ParentActor = GetAttachmentRootActor();
		CustomizableObjectInstance->SetIsPlayerOrNearIt(false);

		if (ParentActor && ParentActor->IsValidLowLevel())
		{
			if (ParentActor->HasActorBegunPlay() || bForceEvenIfNotBegunPlay)
			{
				float SquareDist = FLT_MAX;

				if (ViewCenter && ViewCenter->IsValidLowLevel())
				{
					APawn* Pawn = Cast<APawn>(ParentActor);
					bool bIsPlayer = Pawn ? Pawn->IsPlayerControlled() : false;
					CustomizableObjectInstance->SetIsPlayerOrNearIt(bIsPlayer);

					if (bIsPlayer)
					{
						SquareDist = -0.01f; // Negative value to give the player character more priority than any other character
					}
					else
					{
						SquareDist = FVector::DistSquared(ViewCenter->GetActorLocation(), ParentActor->GetActorLocation());
					}
				}
				else if (bForceEvenIfNotBegunPlay)
				{
					SquareDist = -0.01f; // This is a manual update before begin play and the creation of the pawn, so it should probably be high priority
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				}
				else
				{
					SquareDist = 0.f; // This a mutable tick before begin play and the creation of the pawn, so it should have a definite and high priority but less than a manual update
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				}

				CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
				CustomizableObjectInstance->SetIsBeingUsedByComponentInPlay(true);

				if (CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer == SquareDist)
				{
					CustomizableObjectInstance->NearestToActor = this;
					CustomizableObjectInstance->NearestToViewCenter = ViewCenter;
				}
			}
		}

		if (ParentActor && GetAttachedSkeletalMesh() == nullptr && CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex) && !CustomizableObjectInstance->GetPrivate()->HasCOInstanceFlags(CreatingSkeletalMesh))
		{
			SetSkeletalMesh(CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex));
		}
	}
}


void UCustomizableSkeletalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPendingSetSkeletalMesh || !CustomizableObjectInstance || !CustomizableObjectInstance->GetCustomizableObject())
	{
		return;
	}

	if (USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();

		// Hacky. Replace once we know if the instance has been generated
		const bool bInstanceGenerated = CustomizableObjectInstance->HasAnySkeletalMesh();

		// Generated SkeletalMesh to set, can be null if the component is empty
		USkeletalMesh* SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);

		// If not generated yet, conditionally set the SkeletalMesh of reference
		if (!bInstanceGenerated && !bSkipSetReferenceSkeletalMesh)
		{
			// Can be nullptr
			SkeletalMesh = CustomizableObject->GetRefSkeletalMesh(ComponentIndex);
		}

		// Set SkeletalMesh
		if (bInstanceGenerated || SkeletalMesh)
		{
			Parent->SetSkeletalMesh(SkeletalMesh);

			if (Parent->OverrideMaterials.Num() > 0)
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (Parent->GetClass()->GetFName() != FName(TEXT("SkeletalMeshComponentBudgeted"))) // Reduce unnecessary logging
				{
					UE_LOG(LogMutable, Warning, TEXT("Attaching Customizable Skeletal Component to Skeletal Mesh Component with overriden materials! Deleting overrides."));
				}
#endif

				Parent->EmptyOverrideMaterials();
			}

			bPendingSetSkeletalMesh = false;
		}
	}
}


void UCustomizableSkeletalComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent)
	{
		bPendingSetSkeletalMesh = true;
	}
	else if(!GetAttachParent())
	{
		DestroyComponent();
	}
}
