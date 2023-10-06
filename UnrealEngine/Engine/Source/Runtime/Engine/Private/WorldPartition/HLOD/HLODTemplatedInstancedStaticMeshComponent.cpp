// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODTemplatedInstancedStaticMeshComponent.h"

#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/ObjectSaveOverride.h"

UHLODTemplatedInstancedStaticMeshComponent::UHLODTemplatedInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHLODTemplatedInstancedStaticMeshComponent::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
#if WITH_EDITOR
	// Ensure transient overrides are applied before the base implementation of PreSave
	if (!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		auto GetTransientPropertyOverride = [](FName PropertyName) -> FPropertySaveOverride
		{
			FProperty* OverrideProperty = FindFProperty<FProperty>(UStaticMeshComponent::StaticClass(), PropertyName);
			check(OverrideProperty);

			FPropertySaveOverride PropOverride;
			PropOverride.PropertyPath = FFieldPath(OverrideProperty);
			PropOverride.bMarkTransient = true;
			return PropOverride;
		};

		FObjectSaveOverride ObjectSaveOverride;

		// Only add overrides to clear properties that are identical to the ones provided by template actor class
		const UStaticMeshComponent* TemplateSMC = AActor::GetActorClassDefaultComponentByName<UStaticMeshComponent>(TemplateActorClass, TemplateComponentName);
		if (TemplateSMC)
		{
			// StaticMesh
			if (GetStaticMesh() == TemplateSMC->GetStaticMesh())
			{
				ObjectSaveOverride.PropOverrides.Add(GetTransientPropertyOverride(GetMemberNameChecked_StaticMesh()));
			}

			// OverrideMaterials
			if (OverrideMaterials == TemplateSMC->OverrideMaterials)
			{
				ObjectSaveOverride.PropOverrides.Add(GetTransientPropertyOverride(GET_MEMBER_NAME_CHECKED(UMeshComponent, OverrideMaterials)));
			}

			// OverlayMaterial
			if (OverlayMaterial == TemplateSMC->OverlayMaterial)
			{
				ObjectSaveOverride.PropOverrides.Add(GetTransientPropertyOverride(GET_MEMBER_NAME_CHECKED(UMeshComponent, OverlayMaterial)));
			}
		}		

		if (!ObjectSaveOverride.PropOverrides.IsEmpty())
		{
			ObjectSaveContext.AddSaveOverride(this, ObjectSaveOverride);
		}
	}
#endif

	Super::PreSave(ObjectSaveContext);
}

void UHLODTemplatedInstancedStaticMeshComponent::PostLoad()
{
	if (!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		RestoreAssetsFromActorTemplate();
	}

	Super::PostLoad();
}

void UHLODTemplatedInstancedStaticMeshComponent::SetTemplateActorClass(const TSubclassOf<AActor>& InTemplateActorClass)
{
	TemplateActorClass = InTemplateActorClass;
}

void UHLODTemplatedInstancedStaticMeshComponent::SetTemplateComponentName(const FName& InTemplatePropertyName)
{
	TemplateComponentName = InTemplatePropertyName;
}

void UHLODTemplatedInstancedStaticMeshComponent::RestoreAssetsFromActorTemplate()
{
	// Restore null properties from the template actor class
	const UStaticMeshComponent* TemplateSMC = AActor::GetActorClassDefaultComponentByName<UStaticMeshComponent>(TemplateActorClass, TemplateComponentName);
	if (TemplateSMC)
	{
		// StaticMesh
		if (GetStaticMesh() == nullptr)
		{
			if (UStaticMesh* TemplateStaticMesh = TemplateSMC->GetStaticMesh())
			{
				SetStaticMesh(TemplateStaticMesh);
				SetForcedLodModel(TemplateStaticMesh->GetNumLODs());
			}
		}

		// OverrideMaterials
		if (OverrideMaterials.IsEmpty())
		{
			OverrideMaterials = TemplateSMC->OverrideMaterials;
		}

		// OverlayMaterial
		if (OverlayMaterial == nullptr)
		{
			OverlayMaterial = TemplateSMC->OverlayMaterial;
		}
	}
}
