// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetComponent.h"
#include "LiveLinkInstance.h"
#include "RetargetAnimInstance.h"

// Sets default values for this component's properties
URetargetComponent::URetargetComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame. 
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
}

void URetargetComponent::SetCustomRetargetProfile(FRetargetProfile InProfile)
{
	CustomRetargetProfile = InProfile;

	const TObjectPtr<USkeletalMeshComponent> ControlledMesh = Cast<USkeletalMeshComponent>(ControlledSkeletalMeshComponent.GetComponent(GetOwner()));
	if(ControlledMesh)
	{
		if(ControlledMesh->GetAnimInstance())
		{
			URetargetAnimInstance* AnimInstance = Cast<URetargetAnimInstance>(ControlledMesh->GetAnimInstance());
			AnimInstance->UpdateCustomRetargetProfile(CustomRetargetProfile);
		}
	}
}

FRetargetProfile URetargetComponent::GetCustomRetargetProfile()
{
	/// Check for valid components and refs and then get the custom retarget profile struct
	FRetargetProfile OutProfile;
    TObjectPtr<USkeletalMeshComponent> ControlledMesh;
    ControlledMesh = Cast<USkeletalMeshComponent>(ControlledSkeletalMeshComponent.GetComponent(GetOwner()));
	if(ControlledMesh)
	{
		if(ControlledMesh->GetAnimInstance())
		{
			URetargetAnimInstance* AnimInstance;
			AnimInstance = Cast<URetargetAnimInstance>(ControlledMesh->GetAnimInstance());
			OutProfile = AnimInstance->GetRetargetProfile();
		}
	}
	return OutProfile;
}

void URetargetComponent::SetSourcePerformerMesh(USkeletalMeshComponent* InPerformerMesh)
{
	SourceSkeletalMeshComponent.OverrideComponent = InPerformerMesh;
	InitiateAnimation();
}


void URetargetComponent::SetControlledMesh(USkeletalMeshComponent* InControlledMesh)
{
	ControlledSkeletalMeshComponent.OverrideComponent = InControlledMesh;
	InitiateAnimation();
}

void URetargetComponent::SetRetargetAsset(UIKRetargeter* InRetargetAsset)
{
	RetargetAsset = InRetargetAsset; //It is possible to pass a nullptr to the AnimInstance 

	/// Check for valid components and refs and then reint animation to reset pose
	TObjectPtr<USkeletalMeshComponent> ControlledMesh = Cast<USkeletalMeshComponent>(ControlledSkeletalMeshComponent.GetComponent(GetOwner()));
	if(ControlledMesh)
	{
		if(ControlledMesh->GetAnimInstance())
		{
			URetargetAnimInstance* AnimInstance = Cast<URetargetAnimInstance>(ControlledMesh->GetAnimInstance());

			const TObjectPtr<USkeletalMeshComponent> SourceMesh = Cast<USkeletalMeshComponent>(SourceSkeletalMeshComponent.GetComponent(GetOwner()));
		
			if(SourceSkeletalMeshComponent.GetComponent(GetOwner()) && AnimInstance)
			{
				AnimInstance->ConfigureAnimInstance(RetargetAsset, SourceMesh, CustomRetargetProfile);
				ControlledMesh->InitAnim(true /*bForceReinit*/);
			}
			ControlledMesh->InitAnim(true /*bForceReinit*/);
		}
	}
}

void URetargetComponent::OnRegister()
{
	Super::OnRegister();

	const USkeletalMeshComponent* ControlledMesh = Cast<USkeletalMeshComponent> (ControlledSkeletalMeshComponent.GetComponent(GetOwner()));

	if (ControlledMesh)
	{
		const TObjectPtr<URetargetAnimInstance> AnimInstance = Cast<URetargetAnimInstance>(ControlledMesh->GetAnimInstance());

		if(AnimInstance) //Only set these properties if we have a valid AnimInstance of the correct class.
		{
			SetForceOtherMeshesToFollowControlledMesh(bForceOtherMeshesToFollowControlledMesh);
			SetCustomRetargetProfile(CustomRetargetProfile);
		}
	}

	bIsDirty = true;
}

void URetargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if(bIsDirty)
	{
		InitiateAnimation();
	}
}

void URetargetComponent::DestroyComponent(bool bPromoteChildren)
{
	//When the component is removed, reset the state of the skelmeshes in the owner actor
	TArray<USkeletalMeshComponent*> OwnerSkeletalMeshComponents;
	GetOwner()->GetComponents(OwnerSkeletalMeshComponents);
	
	for (USkeletalMeshComponent* OwnerSkeletalMeshComponent : OwnerSkeletalMeshComponents)
	{
		//If the anim class is LiveLinkInstance assume user does not want to reset that skeletal mesh component
		if(OwnerSkeletalMeshComponent->GetAnimClass()!=ULiveLinkInstance::StaticClass())
		{
			OwnerSkeletalMeshComponent->SetAnimClass(nullptr);
			OwnerSkeletalMeshComponent->SetLeaderPoseComponent(nullptr);
			OwnerSkeletalMeshComponent->InitAnim(true /*bForceReinit*/);
			OwnerSkeletalMeshComponent->SetUpdateAnimationInEditor(false);
		}
	}
	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITOR
void URetargetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if(PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URetargetComponent, SourceSkeletalMeshComponent))
		{
			bIsDirty=true;
		}

		Super::PostEditChangeProperty(PropertyChangedEvent);
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URetargetComponent, ControlledSkeletalMeshComponent))
		{
			bIsDirty=true;
		}

		Super::PostEditChangeProperty(PropertyChangedEvent);
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URetargetComponent, bForceOtherMeshesToFollowControlledMesh))
		{
			SetForceOtherMeshesToFollowControlledMesh(bForceOtherMeshesToFollowControlledMesh);
		}

		Super::PostEditChangeProperty(PropertyChangedEvent);
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URetargetComponent, RetargetAsset))
		{
			SetRetargetAsset(RetargetAsset);
			bIsDirty=true;
		}
		
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URetargetComponent, CustomRetargetProfile))
		{
			SetCustomRetargetProfile(CustomRetargetProfile);
		}
	}
}
#endif

void URetargetComponent::SetForceOtherMeshesToFollowControlledMesh(bool bInBool)
{
	bForceOtherMeshesToFollowControlledMesh = bInBool;
	
	USkeletalMeshComponent* ControlledMesh = Cast<USkeletalMeshComponent> (ControlledSkeletalMeshComponent.GetComponent
(GetOwner()));
	const USkeletalMeshComponent* SourceMesh = Cast<USkeletalMeshComponent>(SourceSkeletalMeshComponent.GetComponent(GetOwner()));
	
	TArray<USkeletalMeshComponent*> OwnerSkeletalMeshComponents;
	GetOwner()->GetComponents(OwnerSkeletalMeshComponents);

	if(IsValid(ControlledMesh) && IsValid(SourceMesh))
	{
		for (USkeletalMeshComponent* OwnerSkeletalMeshComponent : OwnerSkeletalMeshComponents)
		{
			if(IsValid(OwnerSkeletalMeshComponent))
			{
				const bool bShouldResetMesh = OwnerSkeletalMeshComponent->GetAnimClass()!=ULiveLinkInstance::StaticClass()
				&& OwnerSkeletalMeshComponent!=ControlledMesh && OwnerSkeletalMeshComponent!=SourceMesh;
				if(bShouldResetMesh)
				{
					OwnerSkeletalMeshComponent->SetAnimClass(nullptr);
					OwnerSkeletalMeshComponent->InitAnim(true /*bForceReinit*/);
					OwnerSkeletalMeshComponent->SetUpdateAnimationInEditor(true);
					OwnerSkeletalMeshComponent->SetLeaderPoseComponent(nullptr);
				}	
			}
		}
		
		if(bForceOtherMeshesToFollowControlledMesh)
		{
			//Set all skeletal meshes to tick in editor
			for (USkeletalMeshComponent* OwnerSkeletalMeshComponent : OwnerSkeletalMeshComponents)
			{
				OwnerSkeletalMeshComponent->SetUpdateAnimationInEditor(bInBool);

				///If not Controlled or the Source mesh then set skeletal meshes to follow the ControlledMesh. Exception for LiveLink instances as we assume those are being driven directly be mocap data using the PCapPerformerComponent
				///
				const bool bShouldSetLeaderPose  = OwnerSkeletalMeshComponent!=ControlledMesh && OwnerSkeletalMeshComponent->GetAnimClass()!=ULiveLinkInstance::StaticClass() && OwnerSkeletalMeshComponent!=SourceMesh;
				if(bShouldSetLeaderPose)
				{
					OwnerSkeletalMeshComponent->SetLeaderPoseComponent(ControlledMesh,true /*ForceUpdate*/, true /*FollowerShouldTickPose*/);
				}
			}
		}
	}
}

void URetargetComponent::InitiateAnimation()
{
	USkeletalMeshComponent* ControlledMesh = Cast<USkeletalMeshComponent> (ControlledSkeletalMeshComponent.GetComponent(GetOwner()));
	USkeletalMeshComponent* SourceMesh = Cast<USkeletalMeshComponent>(SourceSkeletalMeshComponent.GetComponent(GetOwner()));
	
	const bool bShouldBeReinitialized = IsValid(SourceMesh) && IsValid(RetargetAsset) && IsValid(ControlledMesh);
	
	if(bShouldBeReinitialized)
	{
		//Set the anim instance class on the controlled mesh to use RetargetAnimInstance
		ControlledMesh->SetAnimClass(URetargetAnimInstance::StaticClass());

		TObjectPtr<URetargetAnimInstance> AnimInstance = Cast<URetargetAnimInstance>(ControlledMesh->GetAnimInstance());
		
		if(AnimInstance)
		{
			AnimInstance->ConfigureAnimInstance(RetargetAsset, SourceMesh, CustomRetargetProfile);
			ControlledMesh->SetUpdateAnimationInEditor(true);
			ControlledMesh->bPropagateCurvesToFollowers = true;
			ControlledMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			ControlledMesh->InitAnim(true /*bForceReinit*/);
		}
		ControlledMesh->InitAnim(true /*bForceReinit*/);
	}
	bIsDirty=false;
}
