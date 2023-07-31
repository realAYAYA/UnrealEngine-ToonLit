// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "LevelInstance/LevelInstancePrivate.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PackedLevelActor)

APackedLevelActor::APackedLevelActor()
	: Super()
#if WITH_EDITORONLY_DATA
	, bChildChanged(false)
#endif
{
#if WITH_EDITORONLY_DATA
	// Packed Level Instances don't support level streaming or sub actors
	DesiredRuntimeBehavior = ELevelInstanceRuntimeBehavior::None;
#endif
}

void APackedLevelActor::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	// We want to make sure we serialize that property so we can compare to the CDO
	if (!Ar.IsFilterEditorOnly() && Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::PackedLevelInstanceVersion)
	{
		Ar << PackedVersion;
	}
#endif
}

bool APackedLevelActor::IsLoadingEnabled() const
{
#if WITH_EDITOR
	return HasChildEdit() || IsLoaded();
#else
	return false;
#endif
}

#if WITH_EDITOR

void APackedLevelActor::PostLoad()
{
	Super::PostLoad();

	// Non CDO: Set the Guid to something different then the default value so that we actually run the construction script on actors that haven't been resaved against their latest BP
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && GetLinkerCustomVersion(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::PackedLevelInstanceVersion)
	{
		const static FGuid NoVersionGUID(0x50817615, 0x74A547A3, 0x9295D655, 0x8A852C0F);
		PackedVersion = NoVersionGUID;
	}
}

void APackedLevelActor::RerunConstructionScripts()
{
	bool bShouldRerunConstructionScript = true;

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		bShouldRerunConstructionScript = false;

		// Only rerun if version mismatchs
		if (PackedVersion != GetClass()->GetDefaultObject<APackedLevelActor>()->PackedVersion)
		{
			bShouldRerunConstructionScript = true;
			UE_LOG(LogLevelInstance, Verbose, TEXT("RerunConstructionScript was executed on %s (%s) because its version (%s) doesn't match latest version (%s). Resaving this actor will fix this"),
				*GetPathName(),
				*GetPackage()->GetPathName(),
				*PackedVersion.ToString(),
				*GetClass()->GetDefaultObject<APackedLevelActor>()->PackedVersion.ToString());
		}
	}
	
	if(bShouldRerunConstructionScript)
	{
		Super::RerunConstructionScripts();
		PackedVersion = GetClass()->GetDefaultObject<APackedLevelActor>()->PackedVersion;
	}
}

TUniquePtr<class FWorldPartitionActorDesc> APackedLevelActor::CreateClassActorDesc() const
{
	return AActor::CreateClassActorDesc();
}

bool APackedLevelActor::CreateOrUpdateBlueprint(ALevelInstance* InLevelInstance, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave)
{
	return FPackedLevelActorBuilder::CreateDefaultBuilder()->CreateOrUpdateBlueprint(InLevelInstance, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);
}

bool APackedLevelActor::CreateOrUpdateBlueprint(TSoftObjectPtr<UWorld> InWorldAsset, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave)
{
	return FPackedLevelActorBuilder::CreateDefaultBuilder()->CreateOrUpdateBlueprint(InWorldAsset, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);
}

FName APackedLevelActor::GetPackedComponentTag()
{
	static FName PackedComponentTag("PackedComponent");
	return PackedComponentTag;
}

void APackedLevelActor::UpdateLevelInstanceFromWorldAsset()
{
	Super::UpdateLevelInstanceFromWorldAsset();

	if (!Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
	{
		if (IsWorldAssetValid())
		{
			TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
			Builder->PackActor(this, GetWorldAsset());
		}
		else
		{
			DestroyPackedComponents();
		}
	}
}

void APackedLevelActor::OnEditChild()
{
	Super::OnEditChild();

	check(GetLevelInstanceSubsystem()->GetLevelInstanceLevel(this) != nullptr);
	MarkComponentsRenderStateDirty();
}

void APackedLevelActor::OnCommitChild(bool bChanged)
{
	Super::OnCommitChild(bChanged);

	ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem();
	check(GetLevelInstanceSubsystem()->GetLevelInstanceLevel(this));
	bChildChanged |= bChanged;
	if (!HasChildEdit())
	{
		UnloadLevelInstance();

		if (bChildChanged)
		{
			bChildChanged = false;

			// Reflect child changes
			TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();

			if (UBlueprint* GeneratedBy = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
			{
				Builder->UpdateBlueprint(GeneratedBy);
				return; // return here because Actor might have been reinstanced
			}
			else
			{
				Builder->PackActor(this, GetWorldAsset());
			}
		}
		// When child edit state changes we need to dirty render state so that actor is no longer hidden
		MarkComponentsRenderStateDirty();
	}
}

void APackedLevelActor::OnEdit()
{
	Super::OnEdit();
	MarkComponentsRenderStateDirty();
}

void APackedLevelActor::OnCommit(bool bChanged)
{
	Super::OnCommit(bChanged);

	if (bChanged)
	{
		if (UBlueprint* GeneratedBy = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			TSharedPtr<FPackedLevelActorBuilder> Builder = FPackedLevelActorBuilder::CreateDefaultBuilder();
			Builder->UpdateBlueprint(GeneratedBy);
			return; // return here because Actor might have been reinstanced
		}
	}
	
	// When edit state changes we need to dirty render state so that actor is no longer hidden
	MarkComponentsRenderStateDirty();
}

bool APackedLevelActor::IsHiddenEd() const
{
	return Super::IsHiddenEd() || IsEditing() || HasChildEdit();
}

bool APackedLevelActor::IsHLODRelevant() const
{
	// Bypass base class ALevelInstance (because it always returns true). We want the same implementation as AActor.
	return AActor::IsHLODRelevant();
}

bool APackedLevelActor::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Disallow change of the World on Packed Level Actors as it is set once when creating the blueprint.
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(APackedLevelActor, WorldAsset))
	{
		return false;
	}

	return true;
}

void APackedLevelActor::GetPackedComponents(TArray<UActorComponent*>& OutPackedComponents) const
{
	const TSet<UActorComponent*>& Components = GetComponents();
	OutPackedComponents.Reserve(Components.Num());
		
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->ComponentHasTag(GetPackedComponentTag()))
		{
			OutPackedComponents.Add(Component);
		}
	}
}

void APackedLevelActor::DestroyPackedComponents()
{
	Modify();
	TArray<UActorComponent*> PackedComponents;
	GetPackedComponents(PackedComponents);
	for (UActorComponent* PackedComponent : PackedComponents)
	{
		PackedComponent->Modify();
		PackedComponent->DestroyComponent();
	}
}
#endif
