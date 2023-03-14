// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlateComponent.h"
#include "MediaPlateModule.h"
#include "MediaTexture.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "MediaPlateAssetUserData.h"
#include "UObject/ObjectSaveContext.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaPlate)

#define LOCTEXT_NAMESPACE "MediaPlate"

FLazyName AMediaPlate::MediaPlateComponentName(TEXT("MediaPlateComponent0"));
FLazyName AMediaPlate::MediaTextureName("MediaTexture");

AMediaPlate::AMediaPlate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Set up media component.
	MediaPlateComponent = CreateDefaultSubobject<UMediaPlateComponent>(MediaPlateComponentName);

	// Set up static mesh component.
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(RootComponent);
	StaticMeshComponent->bCastStaticShadow = false;
	StaticMeshComponent->bCastDynamicShadow = false;
	MediaPlateComponent->StaticMeshComponent = StaticMeshComponent;

	// Hook up mesh.
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Plane;
		FConstructorStatics()
			: Plane(TEXT("/MediaPlate/SM_MediaPlateScreen"))
		{}
	};

	static FConstructorStatics ConstructorStatics;
	if (ConstructorStatics.Plane.Object != nullptr)
	{
		StaticMeshComponent->SetStaticMesh(ConstructorStatics.Plane.Object);
	}

#if WITH_EDITOR
	// If we are not the class default object...
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Hook into pre/post save.
		FEditorDelegates::PreSaveWorldWithContext.AddUObject(this, &AMediaPlate::OnPreSaveWorld);
		FEditorDelegates::PostSaveWorldWithContext.AddUObject(this, &AMediaPlate::OnPostSaveWorld);
	}
#endif // WITH_EDITOR
}

void AMediaPlate::PostActorCreated()
{
	Super::PostActorCreated();

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Set which material to use.
		UseDefaultMaterial();
	}
#endif
}

void AMediaPlate::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	// If this media plate is in a Sequencer media track,
	// and we go though a convert to spawnable/possessible,
	// then a multi user client will not receive the correct material
	// as multi user cannot send materials that are not assets.
	// So if we have an override material but its nullptr, then just use the default material.
	if ((StaticMeshComponent != nullptr) &&
		(StaticMeshComponent->GetNumOverrideMaterials() > 0) &&
		(StaticMeshComponent->OverrideMaterials[0] == nullptr))
	{
		UseDefaultMaterial();		
	}
	
	AddAssetUserData();
#endif // WITH_EDITOR
}

void AMediaPlate::BeginDestroy()
{
	Super::BeginDestroy();
}

UMaterialInterface* AMediaPlate::GetCurrentMaterial() const
{
	if (StaticMeshComponent != nullptr)
	{
		return StaticMeshComponent->GetMaterial(0);
	}

	return nullptr;
}

#if WITH_EDITOR

void AMediaPlate::UseDefaultMaterial()
{
	UMaterial* DefaultMaterial = LoadObject<UMaterial>(NULL, TEXT("/MediaPlate/M_MediaPlate"), NULL, LOAD_None, NULL);
	
	ApplyMaterial(DefaultMaterial);
}

void AMediaPlate::ApplyCurrentMaterial()
{
	UMaterialInterface* MaterialInterface = GetCurrentMaterial();
	
	if ((MaterialInterface != nullptr) && (LastMaterial != MaterialInterface))
	{
		ApplyMaterial(MaterialInterface);
	}
}

void AMediaPlate::ApplyMaterial(UMaterialInterface* Material)
{
	if (Material != nullptr && StaticMeshComponent != nullptr)
	{
		if (GEditor == nullptr)
		{
			UMaterialInstanceDynamic* MaterialDynamic = StaticMeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Material);
			MaterialDynamic->SetTextureParameterValue(MediaTextureName, MediaPlateComponent->GetMediaTexture());

			LastMaterial = MaterialDynamic;
		}
		else
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
			UMaterialInterface* Result = nullptr;

			// See if we can modify this material.
			bool bCanModify = true;
			FMediaPlateModule* MediaPlateModule = FModuleManager::GetModulePtr<FMediaPlateModule>("MediaPlate");
			if (MediaPlateModule != nullptr)
			{
				MediaPlateModule->OnMediaPlateApplyMaterial.Broadcast(Material, this, bCanModify);
			}

			if (bCanModify == false)
			{
				LastMaterial = Material;
			}
			else if (MID != nullptr)
			{
				MID->SetTextureParameterValue(MediaTextureName, MediaPlateComponent->GetMediaTexture());

				Result = MID;
			}
			else
			{
				// Change M_ to MI_ in material name and then generate a unique one.
				FString MaterialName = Material->GetName();
				if (MaterialName.StartsWith(TEXT("M_")))
				{
					MaterialName.InsertAt(1, TEXT("I"));
				}
				FName MaterialUniqueName = MakeUniqueObjectName(StaticMeshComponent, UMaterialInstanceConstant::StaticClass(),
					FName(*MaterialName));

				// Create instance.
				UMaterialInstanceConstant* MaterialInstance =
					NewObject<UMaterialInstanceConstant>(StaticMeshComponent, MaterialUniqueName);
				MaterialInstance->SetParentEditorOnly(Material);
				MaterialInstance->CopyMaterialUniformParametersEditorOnly(Material);
				MaterialInstance->SetTextureParameterValueEditorOnly(
					FMaterialParameterInfo(MediaTextureName),
					MediaPlateComponent->GetMediaTexture());
				MaterialInstance->PostEditChange();

				// We force call post-load to indirectly call UpdateParameters() (for integration with VPUtilities plugin).
				MaterialInstance->PostLoad();

				Result = MaterialInstance;
			}

			// Update static mesh.
			if (Result != nullptr)
			{
				StaticMeshComponent->Modify();
				StaticMeshComponent->SetMaterial(0, Result);

				LastMaterial = Result;
			}
		}
	}
}

void AMediaPlate::OnPreSaveWorld(UWorld* InWorld, FObjectPreSaveContext ObjectSaveContext)
{
	// We need to remove our asset user data before saving, as we do not need to save it out
	// and only use it to know when the static mesh component changes.
	RemoveAssetUserData();
}

void AMediaPlate::OnPostSaveWorld(UWorld* InWorld, FObjectPostSaveContext ObjectSaveContext)
{
	AddAssetUserData();
}

void AMediaPlate::AddAssetUserData()
{
	if (StaticMeshComponent != nullptr)
	{
		UMediaPlateAssetUserData* AssetUserData = NewObject<UMediaPlateAssetUserData>(GetTransientPackage());
		AssetUserData->OnPostEditChangeOwner.BindUObject(this, &AMediaPlate::ApplyCurrentMaterial);
		StaticMeshComponent->AddAssetUserData(AssetUserData);
	}
}

void AMediaPlate::RemoveAssetUserData()
{
	if (StaticMeshComponent != nullptr)
	{
		StaticMeshComponent->RemoveUserDataOfClass(UMediaPlateAssetUserData::StaticClass());
	}
}

#endif

#undef LOCTEXT_NAMESPACE

