// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureCharacter.h"

#include "RetargetAnimInstance.h"
#include "RetargetComponent.h"

// Sets default values
ACaptureCharacter::ACaptureCharacter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RetargetComponent = CreateDefaultSubobject<URetargetComponent>("RetargetComponent");
	RetargetComponent->ControlledSkeletalMeshComponent.OverrideComponent = Cast<USkeletalMeshComponent>(GetRootComponent());
	RetargetComponent->SourceSkeletalMeshComponent.OverrideComponent = nullptr;
}


//Update the custom retarget profile
void ACaptureCharacter::SetCustomRetargetProfile(FRetargetProfile InProfile)
{
	RetargetComponent->SetCustomRetargetProfile(InProfile);
}

//Get the custom retarget profile
FRetargetProfile ACaptureCharacter::GetCustomRetargetProfile()
{
	FRetargetProfile OutProfile = RetargetComponent->GetCustomRetargetProfile();
	return OutProfile;
}

void ACaptureCharacter::SetSourcePerformer(ACapturePerformer* InPerformer)
{
	if(IsValid(InPerformer))
	{
		SourcePerformer = InPerformer;
		RetargetComponent->SetSourcePerformerMesh(InPerformer->GetSkeletalMeshComponent());
		RetargetComponent->InitiateAnimation();
	}
	else
	{
		SourcePerformer = nullptr;
		RetargetComponent->SetSourcePerformerMesh(GetSkeletalMeshComponent());
		RetargetComponent->InitiateAnimation();
	}
}

//Update the retarget asset
void ACaptureCharacter::SetRetargetAsset(UIKRetargeter* InRetargetAsset)
{
	RetargetAsset = InRetargetAsset;
	RetargetComponent->SetRetargetAsset(InRetargetAsset);
}

void ACaptureCharacter::SetForceAllSkeletalMeshesToFollowLeader(bool InFollowLeader)
{
	bForceAllSkeletalMeshesToFollowLeader = InFollowLeader;
	RetargetComponent->bForceOtherMeshesToFollowControlledMesh = bForceAllSkeletalMeshesToFollowLeader;
}

void ACaptureCharacter::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RetargetComponent->SetControlledMesh(GetSkeletalMeshComponent());
		SetSourcePerformer(SourcePerformer);
		RetargetComponent->SetRetargetAsset(RetargetAsset);
		RetargetComponent->SetForceOtherMeshesToFollowControlledMesh(bForceAllSkeletalMeshesToFollowLeader);
		RetargetComponent->InitiateAnimation();
	}
}

#if WITH_EDITOR
void ACaptureCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(PropertyChangedEvent.Property != nullptr)
	{
		if(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ACaptureCharacter, SourcePerformer))
		{
			SetSourcePerformer(SourcePerformer);
			RetargetComponent->InitiateAnimation();
		}
		
		if(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ACaptureCharacter, RetargetAsset))
		{
			SetRetargetAsset(RetargetAsset);
			RetargetComponent->InitiateAnimation();
		}
		
		if(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ACaptureCharacter, bForceAllSkeletalMeshesToFollowLeader))
		{
			RetargetComponent->bForceOtherMeshesToFollowControlledMesh = bForceAllSkeletalMeshesToFollowLeader;
			RetargetComponent->InitiateAnimation();
		}
	}
}
#endif