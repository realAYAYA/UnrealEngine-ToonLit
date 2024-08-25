// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectInstanceFactory.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableSkeletalMeshActor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCO/UnrealPortabilityHelpers.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectInstanceFactory"


UCustomizableObjectInstanceFactory::UCustomizableObjectInstanceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    DisplayName = LOCTEXT("CustomizableObjectInstanceDisplayName", "Customizable Object Instance");
    NewActorClass = ACustomizableSkeletalMeshActor::StaticClass();
    bUseSurfaceOrientation = true;
}


void UCustomizableObjectInstanceFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);
	UCustomizableObjectInstance* COInstance = Cast<UCustomizableObjectInstance>(Asset);
	ACustomizableSkeletalMeshActor* NewCSMActor = CastChecked<ACustomizableSkeletalMeshActor>(NewActor);
	if (NewCSMActor && COInstance)
	{
		int32 NumComponents = GetNumberOfComponents(COInstance);

		for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
		{
			USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset(Asset, ComponentIndex);

			if (ComponentIndex > 0 && ComponentIndex > NewCSMActor->GetNumComponents() - 1)
			{
				NewCSMActor->AttachNewComponent();
			}

			if (USkeletalMeshComponent* SkeletalMeshComp = NewCSMActor->GetSkeletalMeshComponentAt(ComponentIndex))
			{
				SkeletalMeshComp->UnregisterComponent();
				UE_MUTABLE_SETSKINNEDASSET(SkeletalMeshComp, SkeletalMesh);

				if (ComponentIndex == 0 && NewCSMActor->GetWorld()->IsGameWorld())
				{
					NewCSMActor->ReplicatedMesh = SkeletalMesh;
				}

				if (UCustomizableSkeletalComponent* CustomSkeletalComp = NewCSMActor->GetCustomizableSkeletalComponent(ComponentIndex))
				{
					CustomSkeletalComp->UnregisterComponent();
					CustomSkeletalComp->CustomizableObjectInstance = COInstance;
					CustomSkeletalComp->ComponentIndex = ComponentIndex;
					CustomSkeletalComp->SetSkeletalMesh(SkeletalMesh);
					CustomSkeletalComp->UpdateSkeletalMeshAsync();
					CustomSkeletalComp->RegisterComponent();
				}

				SkeletalMeshComp->RegisterComponent();
			}
		}
	}
}

UObject* UCustomizableObjectInstanceFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	if (ACustomizableSkeletalMeshActor* CSMActor = CastChecked<ACustomizableSkeletalMeshActor>(ActorInstance))
	{
		if (CSMActor->GetNumComponents() > 0)
		{
			if (UCustomizableSkeletalComponent* CustomSkeletalComp = CSMActor->GetCustomizableSkeletalComponent(0))
			{
				return CustomSkeletalComp->CustomizableObjectInstance;
			}
		}
	}
	return nullptr;
}

bool UCustomizableObjectInstanceFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
    if (!AssetData.IsValid() ||
        (!AssetData.GetClass()->IsChildOf(UCustomizableObjectInstance::StaticClass())))
    {
        OutErrorMsg = LOCTEXT("NoCOISeq", "A valid customizable object instance must be specified.");
        return false;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData CustomizableObjectInstanceData;

    if (AssetData.GetClass()->IsChildOf(UCustomizableObjectInstance::StaticClass()))
    {
        if (UCustomizableObjectInstance* CustomizableObjectInstance = Cast<UCustomizableObjectInstance>(AssetData.GetAsset()))
        {
            if (USkeletalMesh* SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh())
            {
                return true;
            }
            else
            {
                if (UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject())
                {
                    return true;
                }
                else
                {
                    OutErrorMsg = LOCTEXT("NoCustomizableObjectInstance", "The UCustomizableObjectInstance does not have a customizableObject.");
                    return false;
                }
            }
        }
        else
        {
            OutErrorMsg = LOCTEXT("NoCustomizableObjectInstanceIsNull", "The CustomizableObjectInstance is null.");
        }
    }

    if (USkeletalMesh* SkeletalMeshCDO = Cast<USkeletalMesh>(AssetData.GetClass()->GetDefaultObject()))
    {
        if (SkeletalMeshCDO->HasCustomActorFactory())
        {
            return false;
        }
    }

    return true;
}

USkeletalMesh* UCustomizableObjectInstanceFactory::GetSkeletalMeshFromAsset(UObject* Asset, int32 ComponentIndex) const
{
    USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset);
    UCustomizableObjectInstance* CustomizableObjectInstance = Cast<UCustomizableObjectInstance>(Asset);

    if (SkeletalMesh == nullptr && CustomizableObjectInstance != nullptr)
    {
        SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(ComponentIndex);
        if (SkeletalMesh == nullptr)
        {
            if (UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject())
            {
                SkeletalMesh = CustomizableObject->GetRefSkeletalMesh(ComponentIndex);
            }
        }
    }

    //check(SkeletalMesh != NULL);
    return SkeletalMesh;
}


FQuat UCustomizableObjectInstanceFactory::AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const
{
    // Meshes align the Z (up) axis with the surface normal
    return FindActorAlignmentRotation(ActorRotation, FVector(0.f, 0.f, 1.f), InSurfaceNormal);
}


int32 UCustomizableObjectInstanceFactory::GetNumberOfComponents(UCustomizableObjectInstance* COInstance)
{
	int32 NumMeshComponents = 0;

	if (const UCustomizableObject* CustomizableObject = COInstance->GetCustomizableObject();
		CustomizableObject &&
		CustomizableObject->Source)
	{
		TArray<UCustomizableObjectNodeObject*> RootNodes;
		CustomizableObject->Source->GetNodesOfClass<UCustomizableObjectNodeObject>(RootNodes);

		for (int32 i = 0; i < RootNodes.Num(); ++i)
		{
			if (RootNodes[i]->bIsBase)
			{
				NumMeshComponents = RootNodes[i]->NumMeshComponents;
				break;
			}
		}
	}

	return NumMeshComponents;
}


#undef LOCTEXT_NAMESPACE
