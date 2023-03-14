// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalMeshActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Containers/UnrealString.h"
#include "Engine/EngineTypes.h"
#include "HAL/PlatformCrt.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "CustomizableObject"


ACustomizableSkeletalMeshActor::ACustomizableSkeletalMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CustomizableSkeletalComponent = CreateDefaultSubobject<UCustomizableSkeletalComponent>(TEXT("CustomizableSkeletalComponent0"));
	CustomizableSkeletalComponents.Add(CustomizableSkeletalComponent);
	if (USkeletalMeshComponent* SkeletalMeshComp = GetSkeletalMeshComponent()) 
	{
		SkeletalMeshComponents.Add(SkeletalMeshComp);
		bool Success = CustomizableSkeletalComponents[0]->AttachToComponent(SkeletalMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
	}
}


void ACustomizableSkeletalMeshActor::AttachNewComponent()
{
	int32 CurrentIndex = CustomizableSkeletalComponents.Num();
	FString CustomizableComponentName = FString::Printf(TEXT("CustomizableSkeletalComponent%d"), CurrentIndex);
	FString SkeletalMeshComponentName = FString::Printf(TEXT("SkeletalMeshComponent%d"), CurrentIndex);

	//USkeletalMeshComponent* SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(FName(*SkeletalMeshComponentName));
	USkeletalMeshComponent* SkeletalMesh = NewObject<USkeletalMeshComponent>(this, USkeletalMeshComponent::StaticClass(), FName(*SkeletalMeshComponentName));
	USkeletalMeshComponent* RootSkeletalMesh = GetSkeletalMeshComponent();
	
	if (SkeletalMesh && RootSkeletalMesh)
	{		
		bool Success = SkeletalMesh->AttachToComponent(RootSkeletalMesh, FAttachmentTransformRules::KeepRelativeTransform);
		if (Success)
		{
			//UCustomizableSkeletalComponent* CustomizableSkeletalComponent = CreateDefaultSubobject<UCustomizableSkeletalComponent>(FName(*CustomizableComponentName));
			UCustomizableSkeletalComponent* NewCustomizableSkeletalComponent = NewObject<UCustomizableSkeletalComponent>(this, UCustomizableSkeletalComponent::StaticClass(), FName(*CustomizableComponentName));

			if (NewCustomizableSkeletalComponent)
			{
				Success = NewCustomizableSkeletalComponent->AttachToComponent(SkeletalMesh, FAttachmentTransformRules::KeepRelativeTransform);

				if (Success)
				{
					SkeletalMeshComponents.Add(SkeletalMesh);
					CustomizableSkeletalComponents.Add(NewCustomizableSkeletalComponent);
				}
			}
		}
	}

}

#undef LOCTEXT_NAMESPACE
