// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectInstanceFactory.h"

#include "Animation/SkeletalMeshActor.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Array.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Vector.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableSkeletalMeshActor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectInstanceFactory"


UCustomizableObjectInstanceFactory::UCustomizableObjectInstanceFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DisplayName = LOCTEXT("CustomizableObjectInstanceDisplayName", "Customizable Object Instance");
    NewActorClass = ASkeletalMeshActor::StaticClass();
    bUseSurfaceOrientation = true;
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

    if (!CustomizableObjectInstanceData.IsValid())
    {
        OutErrorMsg = LOCTEXT("NoSkeletalMeshAss", "No valid skeletal mesh was found associated with the animation sequence.");
        return false;
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


void UCustomizableObjectInstanceFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
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
				SkeletalMeshComp->SetSkinnedAsset(SkeletalMesh);

				if (ComponentIndex == 0 && NewCSMActor->GetWorld()->IsGameWorld())
				{
					NewCSMActor->ReplicatedMesh = SkeletalMesh;
				}

				if (UCustomizableSkeletalComponent* CustomSkeletalComp = NewCSMActor->GetCustomizableSkeletalComponent(ComponentIndex))
				{
					CustomSkeletalComp->UnregisterComponent();
					CustomSkeletalComp->CustomizableObjectInstance = COInstance;
					CustomSkeletalComp->ComponentIndex = ComponentIndex;
					CustomSkeletalComp->SetSkeletalMesh(SkeletalMesh, false);
					CustomSkeletalComp->UpdateSkeletalMeshAsync();
					CustomSkeletalComp->RegisterComponent();
				}

				SkeletalMeshComp->RegisterComponent();
			}			
		}

		UActorFactory::PostSpawnActor(COInstance, NewActor);
	}
}


void UCustomizableObjectInstanceFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
    //if (Asset != NULL && CDO != NULL)
    //{
    //	USkeletalMesh* SkeletalMesh = GetSkeletalMeshFromAsset(Asset);
    //	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset);

    //	ASkeletalMeshActor* SkeletalMeshActor = CastChecked<ASkeletalMeshActor>(CDO);
    //	SkeletalMeshActor->GetSkeletalMeshComponent()->SkeletalMesh = SkeletalMesh;
    //	SkeletalMeshActor->GetSkeletalMeshComponent()->AnimClass = AnimBlueprint ? Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint->GeneratedClass) : NULL;
    //}
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
