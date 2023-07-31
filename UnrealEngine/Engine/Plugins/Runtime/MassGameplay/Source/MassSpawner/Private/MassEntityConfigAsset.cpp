// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassSpawnerTypes.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "Serialization/ArchiveObjectCrc32.h"
#include "VisualLogger/VisualLogger.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "Mass"

namespace UE::MassSpawner
{
	uint32 HashTraits(const TArray<UMassEntityTraitBase*>& CombinedTraits)
	{
		class FArchiveObjectCRC32AgentConfig : public FArchiveObjectCrc32
		{
		public:
			virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
			{
				check(InProperty);
				return FArchiveObjectCrc32::ShouldSkipProperty(InProperty) || InProperty->HasAllPropertyFlags(CPF_Transient);
			}
		};

		uint32 CRC = 0;
		for (UMassEntityTraitBase* Trait : CombinedTraits)
		{
			FArchiveObjectCRC32AgentConfig Archive;
			CRC = Archive.Crc32(Trait, CRC);
		}
		return CRC;
	}
} // UE::MassSpawner

FMassEntityConfig::FMassEntityConfig(UMassEntityConfigAsset& InParent)
	: Parent(&InParent)
{

}

#if WITH_EDITOR
void UMassEntityConfigAsset::ValidateEntityConfig()
{
	if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		if (Config.ValidateEntityTemplate(*EditorWorld, *this))
		{
			FMessageLog EditorInfo("LogMass");
			EditorInfo.Info(LOCTEXT("MassEntityConfigAssetNoErrorsDetected", "There were no error detected during validation of the EntityConfigAsset"));
			EditorInfo.Notify(LOCTEXT("MassEntityConfigAssetNoErrorsDetected", "There were no error detected during validation of the EntityConfigAsset"), EMessageSeverity::Info, true /*bForce*/);
		}
	}
}
#endif // WITH_EDITOR

const FMassEntityTemplate& FMassEntityConfig::GetOrCreateEntityTemplate(const UWorld& World, const UObject& ConfigOwner) const
{
	uint32 Hash;
	FMassEntityTemplateID TemplateID;
	TArray<UMassEntityTraitBase*> CombinedTraits;
	if (const FMassEntityTemplate* ExistingTemplate = GetEntityTemplateInternal(World, ConfigOwner, Hash, TemplateID, CombinedTraits))
	{
		return *ExistingTemplate;
	}

	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(&World);
	check(SpawnerSystem);
	UMassEntityTemplateRegistry& TemplateRegistry = SpawnerSystem->GetTemplateRegistryInstance();

	// Build new template
	// TODO: Add methods to FMassEntityTemplateBuildContext to indicate dependency vs setup.
	// Dependency should add a fragment with default values (which later can be overridden),
	// while setup would override values and should be run just once.
	FMassEntityTemplate& Template = TemplateRegistry.CreateTemplate(Hash, TemplateID);
	FMassEntityTemplateBuildContext BuildContext(Template);

	BuildContext.BuildFromTraits(CombinedTraits, World);
	Template.SetTemplateName(ConfigOwner.GetName());

	if (ensureMsgf(!Template.IsEmpty(), TEXT("Need at least one fragment to create an Archetype")))
	{
		TemplateRegistry.InitializeEntityTemplate(Template);
	}

	return Template;
}

void FMassEntityConfig::DestroyEntityTemplate(const UWorld& World, const UObject& ConfigOwner) const
{
	uint32 Hash;
	FMassEntityTemplateID TemplateID;
	TArray<UMassEntityTraitBase*> CombinedTraits;
	const FMassEntityTemplate* Template = GetEntityTemplateInternal(World, ConfigOwner, Hash, TemplateID, CombinedTraits);
	if (Template == nullptr)
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(&World);
	check(SpawnerSystem);
	UMassEntityTemplateRegistry& TemplateRegistry = SpawnerSystem->GetTemplateRegistryInstance();


	for (const UMassEntityTraitBase* Trait : CombinedTraits)
	{
		check(Trait);
		Trait->DestroyTemplate();
	}

	// TODO - The templates are not being torn down completely, resulting in traits that leave data in various subsystems. (Representation system)
	
	TemplateRegistry.DestroyTemplate(Hash, TemplateID);
}

const FMassEntityTemplate& FMassEntityConfig::GetEntityTemplateChecked(const UWorld& World, const UObject& ConfigOwner) const
{
	uint32 Hash;
	FMassEntityTemplateID TemplateID;
	TArray<UMassEntityTraitBase*> CombinedTraits;
	const FMassEntityTemplate* ExistingTemplate = GetEntityTemplateInternal(World, ConfigOwner, Hash, TemplateID, CombinedTraits);
	check(ExistingTemplate);
	return *ExistingTemplate;
}

const FMassEntityTemplate* FMassEntityConfig::GetEntityTemplateInternal(const UWorld& World, const UObject& ConfigOwner, uint32& HashOut, FMassEntityTemplateID& TemplateIDOut, TArray<UMassEntityTraitBase*>& CombinedTraitsOut) const
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(&World);
	check(SpawnerSystem);
	const UMassEntityTemplateRegistry& TemplateRegistry = SpawnerSystem->GetTemplateRegistryInstance();

	// Combine all the features into one array
	// @todo this is an inefficient way assuming given template is expected to have already been created. Figure out a way to cache it.
	TArray<const UObject*> Visited;
	CombinedTraitsOut.Reset();
	Visited.Add(&ConfigOwner);
	GetCombinedTraits(CombinedTraitsOut, Visited, ConfigOwner);

	// Return existing template if found.
	// TODO: cache the hash.
	HashOut = UE::MassSpawner::HashTraits(CombinedTraitsOut);
	TemplateIDOut = FMassEntityTemplateID(HashOut, EMassEntityTemplateIDType::ScriptStruct); // TODO: add proper ID type
	return TemplateRegistry.FindTemplateFromTemplateID(TemplateIDOut);
}

void FMassEntityConfig::GetCombinedTraits(TArray<UMassEntityTraitBase*>& OutTraits, TArray<const UObject*>& Visited, const UObject& ConfigOwner) const
{
	if (Parent)
	{
		if (Visited.IndexOfByKey(Parent) != INDEX_NONE)
		{
			// Infinite loop detected.
			FString Path;
			for (const UObject* Object : Visited)
			{
				Path += Object->GetName();
				Path += TEXT("/");
			}
			UE_VLOG(&ConfigOwner, LogMassSpawner, Error, TEXT("%s: Encountered %s as parent second time (Infinite loop). %s"), *GetNameSafe(&ConfigOwner), *GetNameSafe(Parent), *Path);
		}
		else
		{
			Visited.Add(Parent);
			Parent->GetConfig().GetCombinedTraits(OutTraits, Visited, ConfigOwner);
		}
	}

	for (UMassEntityTraitBase* Trait : Traits)
	{
		if (!Trait)
		{
			continue;
		}
		// Allow only one feature per type. This is also used to allow child configs override parent features.
		const int32 Index = OutTraits.IndexOfByPredicate([Trait](const UMassEntityTraitBase* ExistingFeature) -> bool { return Trait->GetClass() == ExistingFeature->GetClass(); });
		if (Index != INDEX_NONE)
		{
			OutTraits[Index] = Trait;
		}
		else
		{
			OutTraits.Add(Trait);
		}
	}
}

void FMassEntityConfig::AddTrait(UMassEntityTraitBase& Trait)
{
	Traits.Add(&Trait);
}

bool FMassEntityConfig::ValidateEntityTemplate(const UWorld& World, const UObject& ConfigOwner)
{
	TArray<const UObject*> Visited;
	TArray<UMassEntityTraitBase*> CombinedTraits;
	Visited.Add(&ConfigOwner);
	GetCombinedTraits(CombinedTraits, Visited, ConfigOwner);

	FMassEntityTemplate Template;
	FMassEntityTemplateBuildContext BuildContext(Template);

	return BuildContext.BuildFromTraits(CombinedTraits, World);
}

#undef LOCTEXT_NAMESPACE 
