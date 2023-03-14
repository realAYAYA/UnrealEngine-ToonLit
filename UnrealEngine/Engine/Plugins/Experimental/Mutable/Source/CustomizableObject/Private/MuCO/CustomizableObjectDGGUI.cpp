// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectDGGUI.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectIterator.h"

void UDGGUI::OpenDGGUI(const int32 SlotID, UCustomizableSkeletalComponent* SelectedCustomizableSkeletalComponent, const UWorld* CurrentWorld, const int32 PlayerIndex)
{
#if WITH_EDITOR
	if (APlayerController* Player = UGameplayStatics::GetPlayerController(CurrentWorld, PlayerIndex))
	{
		FSoftClassPath DGUIPath(TEXT("/Mutable/UI/DynamicallyGeneratedGUI_DGGUI/DynamicallyGeneratedGUI_DGGUI.DynamicallyGeneratedGUI_DGGUI_C"));
		if (UClass* DGUI = DGUIPath.TryLoadClass<UDGGUI>())
		{
			UDGGUI* WDGUI = CreateWidget<UDGGUI>(Player, DGUI);
			if (WDGUI)
			{
				WDGUI->SetCustomizableSkeletalComponent(SelectedCustomizableSkeletalComponent);
				SelectedCustomizableSkeletalComponent->PreUpdateDelegate.BindUObject(WDGUI, &UDGGUI::CustomizableSkeletalMeshPreUpdate);
				WDGUI->AddToViewport();
				Player->SetShowMouseCursor(true);
			}
		}
	}
#endif // WITH_EDITOR
}

bool UDGGUI::CloseExistingDGGUI(const UWorld* CurrentWorld)
{
#if WITH_EDITOR
	bool bClosing = false;
	for (TObjectIterator<UDGGUI> PreviousGUI; PreviousGUI; ++PreviousGUI)
	{
		if (PreviousGUI->IsValidLowLevel())
		{
			if (UCustomizableSkeletalComponent* CustomizableComponent = PreviousGUI->GetCustomizableSkeletalComponent())
			{
				CustomizableComponent->PreUpdateDelegate.Unbind();
				CustomizableComponent->UpdatedDelegate.Unbind();
				PreviousGUI->SetCustomizableSkeletalComponent(nullptr);
				bClosing = true;
			}
			PreviousGUI->RemoveFromParent();
		}
	}
	if (bClosing)
	{
		if (APlayerController* Player = UGameplayStatics::GetPlayerController(CurrentWorld, 0))
		{
			Player->SetShowMouseCursor(false);
		}
		return true;
	}
#endif // WITH_EDITOR
	return false;
}


// This is all based on UMeshCosmeticsVariance_ApplyParameters::PostCustomizationFixup( , but I think this should be set from within mutable, like how Physics assets are set up without user intervention
void UDGGUI::CustomizableSkeletalMeshPreUpdate(UCustomizableSkeletalComponent* Component, USkeletalMesh* NextMesh)
{
#if WITH_EDITOR
	if (Component && NextMesh)
	{
		USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(Component->GetAttachParent());
		USkeleton* NextSkeleton = NextMesh->GetSkeleton();
		if (Parent && NextSkeleton)
		{
			if (USkeletalMesh* ParentSkeletalMesh = Cast<USkeletalMesh>(Parent->GetSkinnedAsset()))
			{
				if (USkeleton* ParentSkeleton = ParentSkeletalMesh->GetSkeleton())
				{
					UAnimInstance* AnimInstance = Parent->GetAnimInstance();
					UClass* AnimClass = Parent->GetAnimClass();
					IAnimClassInterface* Animation = Cast<IAnimClassInterface>(AnimClass);
					if (AnimInstance && Animation)
					{
						if (USkeleton* AnimationSkeleton = Animation->GetTargetSkeleton())
						{
							if (AnimationSkeleton != NextSkeleton && NextSkeleton->GetCompatibleSkeletons().Find(AnimationSkeleton) == INDEX_NONE)
							{
								NextSkeleton->AddCompatibleSkeleton(AnimationSkeleton);
								LastAnimationClass = AnimClass;
								Component->UpdatedDelegate.BindUObject(this, &UDGGUI::CustomizableSkeletalMeshUpdated);
							}
						}
					}
				}
			}
		}
	}
#endif // WITH_EDITOR
}


void UDGGUI::CustomizableSkeletalMeshUpdated()
{
#if WITH_EDITOR
	if (UCustomizableSkeletalComponent* CustomizableComponent = GetCustomizableSkeletalComponent())
	{
		if (USkeletalMeshComponent* CustomizedComponent = Cast<USkeletalMeshComponent>(CustomizableComponent->GetAttachParent()))
		{
			CustomizedComponent->SetAnimInstanceClass(LastAnimationClass);

			if (UAnimInstance* TargetAnimInstance = CustomizedComponent->GetAnimInstance())
			{
				const UCustomizableObjectInstance* Instance = CustomizableComponent->CustomizableObjectInstance;

				const int32 ComponentIndex = CustomizableComponent->ComponentIndex;

				IAnimClassInterface* AnimInstanceClassInterface = IAnimClassInterface::GetFromClass(LastAnimationClass);

				USkeleton* ProportionalSkel = AnimInstanceClassInterface->GetTargetSkeleton();
				check(ProportionalSkel);
				const auto LinkAnimClassBySlot = [&TargetAnimInstance, ProportionalSkel](int32 SlotIndex, TSubclassOf<UAnimInstance> SlottedInstanceClass)
				{
					if (SlottedInstanceClass)
					{
						IAnimClassInterface* SlottedClassInterface = IAnimClassInterface::GetFromClass(SlottedInstanceClass);
						USkeleton* SlottedSkeleton = SlottedClassInterface->GetTargetSkeleton();

						if (SlottedSkeleton != ProportionalSkel && SlottedSkeleton->GetCompatibleSkeletons().Find(ProportionalSkel) == INDEX_NONE)
						{
							SlottedSkeleton->AddCompatibleSkeleton(ProportionalSkel);
						}

						TargetAnimInstance->LinkAnimGraphByTag(*FString::Printf(TEXT("Part%d"), SlotIndex), SlottedInstanceClass);
					}
				};

				using ForEachDelType = FEachComponentAnimInstanceClassNativeDelegate;
				Instance->ForEachAnimInstance(ComponentIndex, ForEachDelType::CreateLambda(LinkAnimClassBySlot));
			}
		}
		//CustomizableComponent->UpdatedDelegate.Unbind();
	}
#endif // WITH_EDITOR
}