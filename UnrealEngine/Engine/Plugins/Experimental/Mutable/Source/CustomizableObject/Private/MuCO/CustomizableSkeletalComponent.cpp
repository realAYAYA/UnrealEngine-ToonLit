// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"

#include "UObject/UObjectGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObject.h"


void UCustomizableSkeletalComponent::CreateCustomizableObjectInstanceUsage()
{
	if (CustomizableObjectInstanceUsage)
	{
		// CustomizableObjectInstanceUsage may already exist if duplicated from an existing Customizable Skeletal Component
		if (CustomizableObjectInstanceUsage->CustomizableSkeletalComponent != this)
		{
			CustomizableObjectInstanceUsage = nullptr;
		}
	}

	AActor* RootActor = GetAttachmentRootActor();
	bool bIsDefaultActor = RootActor ?
		RootActor->HasAnyFlags(RF_ClassDefaultObject) :
		false;

	if (!CustomizableObjectInstanceUsage && !HasAnyFlags(RF_ClassDefaultObject) && !bIsDefaultActor)
	{
		CustomizableObjectInstanceUsage = NewObject<UCustomizableObjectInstanceUsage>(this, TEXT("InstanceUsage"), RF_Transient);
		CustomizableObjectInstanceUsage->CustomizableSkeletalComponent = this;
	}
}


void UCustomizableSkeletalComponent::Callbacks() const
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->Callbacks();
	}
}


USkeletalMesh* UCustomizableSkeletalComponent::GetSkeletalMesh() const
{
	if (CustomizableObjectInstanceUsage)
	{
		return CustomizableObjectInstanceUsage->GetSkeletalMesh();
	}

	return nullptr;
}


void UCustomizableSkeletalComponent::SetSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetSkeletalMesh(SkeletalMesh);
	}
}


void UCustomizableSkeletalComponent::SetPhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetPhysicsAsset(PhysicsAsset);
	}
}


USkeletalMesh* UCustomizableSkeletalComponent::GetAttachedSkeletalMesh() const
{
	if (CustomizableObjectInstanceUsage)
	{
		return CustomizableObjectInstanceUsage->GetAttachedSkeletalMesh();
	}

	return nullptr;
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->UpdateSkeletalMeshAsync(bNeverSkipUpdate);
	}
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->UpdateSkeletalMeshAsyncResult(Callback, bIgnoreCloseDist, bForceHighPriority);
	}
}


#if WITH_EDITOR

void UCustomizableSkeletalComponent::EditorUpdateComponent()
{
	if (CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->EditorUpdateComponent();
	}
}
#endif


void UCustomizableSkeletalComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (Parent && CustomizableObjectInstanceUsage)
	{
		CustomizableObjectInstanceUsage->SetPendingSetSkeletalMesh(true);
	}
	else if(!GetAttachParent())
	{
		DestroyComponent();
	}
}


void UCustomizableSkeletalComponent::PostInitProperties()
{
	Super::PostInitProperties();

	CreateCustomizableObjectInstanceUsage();
}
