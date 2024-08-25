// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/UnrealPortabilityHelpers.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MuCO/CustomizableObject.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ObjectSaveContext.h"
#include "Stats/Stats.h"


ETickableTickType UCustomizableObjectInstanceUsage::GetTickableTickType() const
{ 
	return (HasAnyFlags(RF_ClassDefaultObject))
		? ETickableTickType::Never
		: ETickableTickType::Conditional;
}


void UCustomizableObjectInstanceUsage::Callbacks() const
{
	if (CustomizableSkeletalComponent)
	{
		CustomizableSkeletalComponent->UpdatedDelegate.ExecuteIfBound();

		if (UpdatedDelegate.IsBound() && CustomizableSkeletalComponent->UpdatedDelegate.IsBound())
		{
			UE_LOG(LogMutable, Error, TEXT("The UpdatedDelegate is bound both in the UCustomizableObjectInstanceUsage and in its parent CustomizableSkeletalComponent. Only one should be bound."));
			ensure(false);
		}
	}
	
	UpdatedDelegate.ExecuteIfBound();
}


void UCustomizableObjectInstanceUsage::SetCustomizableObjectInstance(UCustomizableObjectInstance* CustomizableObjectInstance)
{
	if (CustomizableSkeletalComponent)
	{
		CustomizableSkeletalComponent->CustomizableObjectInstance = CustomizableObjectInstance;
	}
	else
	{
		UsedCustomizableObjectInstance = CustomizableObjectInstance;
	}
}


UCustomizableObjectInstance* UCustomizableObjectInstanceUsage::GetCustomizableObjectInstance() const
{
	if (CustomizableSkeletalComponent)
	{
		return CustomizableSkeletalComponent->CustomizableObjectInstance;
	}
	else
	{
		return UsedCustomizableObjectInstance;
	}
}


void UCustomizableObjectInstanceUsage::SetComponentIndex(int32 ComponentIndex)
{
	if (CustomizableSkeletalComponent)
	{
		CustomizableSkeletalComponent->ComponentIndex = ComponentIndex;
	}
	else
	{
		UsedComponentIndex = ComponentIndex;
	}
}


int32 UCustomizableObjectInstanceUsage::GetComponentIndex() const
{
	if (CustomizableSkeletalComponent)
	{
		return CustomizableSkeletalComponent->ComponentIndex;
	}
	else
	{
		return UsedComponentIndex;
	}
}


void UCustomizableObjectInstanceUsage::SetPendingSetSkeletalMesh(bool bIsActive)
{
	if (CustomizableSkeletalComponent)
	{
		CustomizableSkeletalComponent->bPendingSetSkeletalMesh = bIsActive;
	}
	else
	{
		bUsedPendingSetSkeletalMesh = bIsActive;
	}
}


bool UCustomizableObjectInstanceUsage::GetPendingSetSkeletalMesh() const
{
	if (CustomizableSkeletalComponent)
	{
		return CustomizableSkeletalComponent->bPendingSetSkeletalMesh;
	}
	else
	{
		return bUsedPendingSetSkeletalMesh;
	}
}


void UCustomizableObjectInstanceUsage::SetSkipSetReferenceSkeletalMesh(bool bIsActive)
{
	if (CustomizableSkeletalComponent)
	{
		CustomizableSkeletalComponent->bSkipSetReferenceSkeletalMesh = bIsActive;
	}
	else
	{
		bUsedSkipSetReferenceSkeletalMesh = bIsActive;
	}
}


bool UCustomizableObjectInstanceUsage::GetSkipSetReferenceSkeletalMesh() const
{
	if (CustomizableSkeletalComponent)
	{
		return CustomizableSkeletalComponent->bSkipSetReferenceSkeletalMesh;
	}
	else
	{
		return bUsedSkipSetReferenceSkeletalMesh;
	}
}


void UCustomizableObjectInstanceUsage::AttachTo(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (CustomizableSkeletalComponent)
	{
		UE_LOG(LogMutable, Error, TEXT("Cannot change the attachment of a UCustomizableObjectInstanceUsage that has been automatically created by a CustomizableSkeletalComponent. Reattach the CustomizableSkeletalComponent instead."));
		ensure(false);
	}
	else
	{
		if (IsValid(SkeletalMeshComponent))
		{
			UsedSkeletalMeshComponent = SkeletalMeshComponent;
		}
		else
		{
			UsedSkeletalMeshComponent = nullptr;
		}

		// To mimic the behavior of UCustomizableSkeletalComponent::OnAttachmentChanged()
		SetPendingSetSkeletalMesh(true);
	}
}


USkeletalMeshComponent* UCustomizableObjectInstanceUsage::GetAttachParent() const
{
	if (CustomizableSkeletalComponent)
	{
		return Cast<USkeletalMeshComponent>(CustomizableSkeletalComponent->GetAttachParent());
	}
	else if(UsedSkeletalMeshComponent.IsValid())
	{
		return UsedSkeletalMeshComponent.Get();
	}

	return nullptr;
}


USkeletalMesh* UCustomizableObjectInstanceUsage::GetSkeletalMesh() const
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	return CustomizableObjectInstance ? CustomizableObjectInstance->GetSkeletalMesh(GetComponentIndex()) : nullptr;
}


bool RequiresReinitPose(USkeletalMesh* CurrentSkeletalMesh, USkeletalMesh* SkeletalMesh)
{
	if (CurrentSkeletalMesh == SkeletalMesh)
	{
		return false;
	}

	if (!CurrentSkeletalMesh || !SkeletalMesh)
	{
		return SkeletalMesh != nullptr;
	}

	if (CurrentSkeletalMesh->GetLODNum() != SkeletalMesh->GetLODNum())
	{
		return true;
	}

	const FSkeletalMeshRenderData* CurrentRenderData = CurrentSkeletalMesh->GetResourceForRendering();
	const FSkeletalMeshRenderData* NewRenderData = SkeletalMesh->GetResourceForRendering();
	if (!CurrentRenderData || !NewRenderData)
	{
		return false;
	}

	const int32 NumLODs = SkeletalMesh->GetLODNum();
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		if (CurrentRenderData->LODRenderData[LODIndex].RequiredBones != NewRenderData->LODRenderData[LODIndex].RequiredBones)
		{
			return true;
		}
	}

	return false;
}


void UCustomizableObjectInstanceUsage::SetSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent)
	{
		Parent->SetSkeletalMesh(SkeletalMesh, RequiresReinitPose(Parent->GetSkeletalMeshAsset(), SkeletalMesh));

		const UCustomizableObjectInstance* Instance = GetCustomizableObjectInstance();
		const UCustomizableObject* CustomizableObject = Instance ? Instance->GetCustomizableObject() : nullptr;

		if (Parent->HasOverrideMaterials())
		{
			// For some reason the reference skeletal mesh materials are added as override materials, clear them if necessary
			Parent->EmptyOverrideMaterials();
		}
		
		if (CustomizableObject &&
			CustomizableObject->bEnableMeshCache &&
			CVarEnableMeshCache.GetValueOnAnyThread())
		{
			if (FCustomizableInstanceComponentData* ComponentData = Instance->GetPrivate()->GetComponentData(GetComponentIndex()))
			{
				for (int32 Index = 0; Index < ComponentData->OverrideMaterials.Num(); ++Index)
				{
					Parent->SetMaterial(Index, ComponentData->OverrideMaterials[Index]);
				}
			}
		}	
	}
}

void UCustomizableObjectInstanceUsage::SetPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent && Parent->GetWorld())
	{
		Parent->SetPhysicsAsset(PhysicsAsset, true);
	}
}


USkeletalMesh* UCustomizableObjectInstanceUsage::GetAttachedSkeletalMesh() const
{
	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent)
	{
		return UE_MUTABLE_GETSKELETALMESHASSET(Parent);
	}

	return nullptr;
}


void UCustomizableObjectInstanceUsage::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsync(false, false);
	}
}


void UCustomizableObjectInstanceUsage::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->UpdateSkeletalMeshAsyncResult(Callback, false, false);
	}
}


void UCustomizableObjectInstanceUsage::BeginDestroy()
{
	Super::BeginDestroy();
}


#if WITH_EDITOR

void UCustomizableObjectInstanceUsage::UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition)
{
	// We want instances in the editor to be generated
	if (!GetWorld() || GetWorld()->WorldType != EWorldType::Editor)
	{
		return;
	}

	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetAttachParent();
		AActor* ParentActor = SkeletalMeshComponent ? SkeletalMeshComponent->GetAttachmentRootActor() : nullptr;
		if (ParentActor && ParentActor->IsValidLowLevel())
		{
			// update distance to camera and set the instance as being used by a component
			CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(UsedByComponent);

			float SquareDist = FVector::DistSquared(CameraPosition, ParentActor->GetActorLocation());
			CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = 
				FMath::Min(SquareDist, CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer);
		}

		USkeletalMesh* AttachedSkeletalMesh = GetAttachedSkeletalMesh();
		const int32 ComponentIndex = GetComponentIndex();

		const bool bInstanceGenerated = CustomizableObjectInstance->GetPrivate()->SkeletalMeshStatus != ESkeletalMeshStatus::NotGenerated;
		USkeletalMesh* GeneratedSkeletalMesh = bInstanceGenerated ? CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex) :
			CustomizableObjectInstance->GetCustomizableObject()->GetRefSkeletalMesh(ComponentIndex);

		if (AttachedSkeletalMesh != GeneratedSkeletalMesh)
		{
			SetSkeletalMesh(GeneratedSkeletalMesh);
		}
	}
}


void UCustomizableObjectInstanceUsage::EditorUpdateComponent()
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(UsedByComponent);

		USkeletalMeshComponent* SkeletalMeshComponent = GetAttachParent();
		AActor* ParentActor = SkeletalMeshComponent ? SkeletalMeshComponent->GetAttachmentRootActor() : nullptr;
		
		if (ParentActor)
		{
			USkeletalMesh* AttachedSkeletalMesh = GetAttachedSkeletalMesh();
			const int32 ComponentIndex = GetComponentIndex();

			const bool bInstanceGenerated = CustomizableObjectInstance->GetPrivate()->SkeletalMeshStatus != ESkeletalMeshStatus::NotGenerated;
			USkeletalMesh* GeneratedSkeletalMesh = bInstanceGenerated ? CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex) :
				CustomizableObjectInstance->GetCustomizableObject()->GetRefSkeletalMesh(ComponentIndex);

			if (AttachedSkeletalMesh != GeneratedSkeletalMesh)
			{
				SetSkeletalMesh(GeneratedSkeletalMesh);
			}
		}
	}
}
#endif


void UCustomizableObjectInstanceUsage::SetVisibilityOfSkeletalMeshSectionWithMaterialName(bool bInVisible, const FString& MaterialName, int32 LOD)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();
	int32 ComponentIndex = GetComponentIndex();

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


void UCustomizableObjectInstanceUsage::UpdateDistFromComponentToPlayer(const AActor* ViewCenter, const bool bForceEvenIfNotBegunPlay)
{
	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (CustomizableObjectInstance)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetAttachParent();
		AActor* ParentActor = SkeletalMeshComponent ? SkeletalMeshComponent->GetAttachmentRootActor() : nullptr;
		
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
					CustomizableObjectInstance->GetPrivate()->NearestToActor = this;
					CustomizableObjectInstance->GetPrivate()->NearestToViewCenter = ViewCenter;
				}
			}
		}

		int32 ComponentIndex = GetComponentIndex();

		if (ParentActor && GetAttachedSkeletalMesh() == nullptr && CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex))
		{
			SetSkeletalMesh(CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex));
		}
	}
}


void UCustomizableObjectInstanceUsage::Tick(float DeltaTime)
{
	if (!IsValid(this))
	{
		return;
	}

	UCustomizableObjectInstance* CustomizableObjectInstance = GetCustomizableObjectInstance();

	if (!GetPendingSetSkeletalMesh() || !CustomizableObjectInstance || !IsValid(CustomizableObjectInstance))
	{
		return;
	}
	
	UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	if (!CustomizableObject || !IsValid(CustomizableObject))
	{
		return;
	}	

	if (USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		const int32 ComponentIndex = GetComponentIndex();

		USkeletalMesh* SkeletalMesh = nullptr;

		const bool bInstanceGenerated = CustomizableObjectInstance->GetPrivate()->SkeletalMeshStatus == ESkeletalMeshStatus::Success;		
		if (bInstanceGenerated)
		{
			// Generated SkeletalMesh to set, can be null if the component is empty
			SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);
		}
		else
		{
			// If not generated yet, conditionally set the SkeletalMesh of reference
			if (!bInstanceGenerated && !GetSkipSetReferenceSkeletalMesh() && 
				CustomizableObject->bEnableUseRefSkeletalMeshAsPlaceholder)
			{
				// Can be nullptr
				SkeletalMesh = CustomizableObject->GetRefSkeletalMesh(ComponentIndex);
			}
		}

		// Set SkeletalMesh
		if (bInstanceGenerated || SkeletalMesh)
		{
			Parent->SetSkeletalMesh(SkeletalMesh, RequiresReinitPose(Parent->GetSkeletalMeshAsset(), SkeletalMesh));

			if (Parent->HasOverrideMaterials())
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (Parent->GetClass()->GetFName() != FName(TEXT("SkeletalMeshComponentBudgeted"))) // Reduce unnecessary logging
				{
					UE_LOG(LogMutable, Warning, TEXT("Attaching Customizable Skeletal Component to Skeletal Mesh Component with overriden materials! Deleting overrides."));
				}
#endif

				Parent->EmptyOverrideMaterials();
			}

			if (CustomizableObject &&
				bInstanceGenerated &&
				CustomizableObject->bEnableMeshCache &&
				CVarEnableMeshCache.GetValueOnAnyThread())
			{
				if (FCustomizableInstanceComponentData* ComponentData = CustomizableObjectInstance->GetPrivate()->GetComponentData(GetComponentIndex()))
				{
					for (int32 Index = 0; Index < ComponentData->OverrideMaterials.Num(); ++Index)
					{
						Parent->SetMaterial(Index, ComponentData->OverrideMaterials[Index]);
					}
				}
			}

			SetPendingSetSkeletalMesh(false);
		}
	}
}


TStatId UCustomizableObjectInstanceUsage::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCustomizableObjectInstanceUsage, STATGROUP_Tickables);
}


bool UCustomizableObjectInstanceUsage::IsTickable() const
{
	return !HasAnyFlags(RF_BeginDestroyed);
}


bool UCustomizableObjectInstanceUsage::IsNetMode(ENetMode InNetMode) const
{
	if (CustomizableSkeletalComponent)
	{
		return CustomizableSkeletalComponent->IsNetMode(InNetMode);
	}
	else if(UsedSkeletalMeshComponent.IsValid())
	{
		return UsedSkeletalMeshComponent->IsNetMode(InNetMode);
	}

	return false;
}
