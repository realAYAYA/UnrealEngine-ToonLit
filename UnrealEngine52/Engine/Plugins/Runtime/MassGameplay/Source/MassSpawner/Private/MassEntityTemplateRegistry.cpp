// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTemplateRegistry.h"
#include "MassSpawnerTypes.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"
#include "HAL/IConsoleManager.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "Logging/MessageLog.h"
#include "MassEntityUtils.h"


#define LOCTEXT_NAMESPACE "Mass"

namespace FTemplateRegistryHelpers
{
	uint32 CalcHash(const FConstStructView StructInstance)
	{
		const UScriptStruct* Type = StructInstance.GetScriptStruct();
		const uint8* Memory = StructInstance.GetMemory();
		return Type && Memory ? Type->GetStructTypeHash(Memory) : 0;
	}

	void FragmentInstancesToTypes(TArrayView<const FInstancedStruct> FragmentList, TArray<const UScriptStruct*>& OutFragmentTypes)
	{
		for (const FInstancedStruct& Instance : FragmentList)
		{
			// at this point FragmentList is assumed to have no duplicates nor nulls
			OutFragmentTypes.Add(Instance.GetScriptStruct());
		}
	}

	void ResetEntityTemplates(const TArray<FString>& Args, UWorld* InWorld)
	{
		UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(InWorld);
		if (ensure(SpawnerSystem))
		{
			FMassEntityTemplateRegistry& Registry = SpawnerSystem->GetMutableTemplateRegistryInstance();
			Registry.DebugReset();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs EnableCategoryNameCmd(
		TEXT("ai.mass.reset_entity_templates"),
		TEXT("Clears all the runtime information cached by MassEntityTemplateRegistry. Will result in lazily building all entity templates again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResetEntityTemplates)
	);

}

//----------------------------------------------------------------------//
// FMassEntityTemplateRegistry 
//----------------------------------------------------------------------//
TMap<const UScriptStruct*, FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate> FMassEntityTemplateRegistry::StructBasedBuilders;

FMassEntityTemplateRegistry::FMassEntityTemplateRegistry(UObject* InOwner)
	: Owner(InOwner)
{
}

UWorld* FMassEntityTemplateRegistry::GetWorld() const 
{
	return Owner.IsValid() ? Owner->GetWorld() : nullptr;
}

FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate& FMassEntityTemplateRegistry::FindOrAdd(const UScriptStruct& DataType)
{
	return StructBasedBuilders.FindOrAdd(&DataType);
}

bool FMassEntityTemplateRegistry::BuildTemplateImpl(const FStructToTemplateBuilderDelegate& Builder, const FConstStructView StructInstance, FMassEntityTemplate& OutTemplate)
{
	UWorld* World = GetWorld();
	FMassEntityTemplateBuildContext Context(OutTemplate);
	Builder.Execute(World, StructInstance, Context);
	if (ensure(!OutTemplate.IsEmpty())) // need at least one fragment to create an Archetype
	{
		InitializeEntityTemplate(OutTemplate);

		UE_VLOG(Owner.Get(), LogMassSpawner, Log, TEXT("Created entity template for %s:\n%s"), *GetNameSafe(StructInstance.GetScriptStruct())
			, *OutTemplate.DebugGetDescription(UE::Mass::Utils::GetEntityManager(World)));

		return true;
	}
	return false;
}

void FMassEntityTemplateRegistry::InitializeEntityTemplate(FMassEntityTemplate& InOutTemplate) const
{
	UWorld* World = GetWorld();
	check(World);
	// find or create template
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// Sort anything there is to sort for later comparison purposes
	InOutTemplate.Sort();

	const FMassArchetypeHandle ArchetypeHandle = EntityManager.CreateArchetype(InOutTemplate.GetCompositionDescriptor(), FName(InOutTemplate.GetTemplateName()));
	InOutTemplate.SetArchetype(ArchetypeHandle);
}

void FMassEntityTemplateRegistry::DebugReset()
{
#if WITH_MASSGAMEPLAY_DEBUG
	TemplateIDToTemplateMap.Reset();
#endif // WITH_MASSGAMEPLAY_DEBUG
}

const FMassEntityTemplate* FMassEntityTemplateRegistry::FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const 
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

FMassEntityTemplate* FMassEntityTemplateRegistry::FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID) 
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

FMassEntityTemplate& FMassEntityTemplateRegistry::CreateTemplate(FMassEntityTemplateID TemplateID)
{
	checkSlow(!TemplateIDToTemplateMap.Contains(TemplateID));
	FMassEntityTemplate& NewTemplate = TemplateIDToTemplateMap.Add(TemplateID);
	NewTemplate.SetTemplateID(TemplateID);
	return NewTemplate;
}

void FMassEntityTemplateRegistry::DestroyTemplate(FMassEntityTemplateID TemplateID)
{
	TemplateIDToTemplateMap.Remove(TemplateID);
}

//----------------------------------------------------------------------//
// FMassEntityTemplateBuildContext 
//----------------------------------------------------------------------//
bool FMassEntityTemplateBuildContext::BuildFromTraits(TConstArrayView<UMassEntityTraitBase*> Traits, const UWorld& World)
{
	TraitAddedTypes.Reset();
	TraitsDependencies.Reset();

	for (const UMassEntityTraitBase* Trait : Traits)
	{
		check(Trait);
		BuildingTrait = Trait;
		BuildingTrait->BuildTemplate(*this, World);
	}

	BuildingTrait = nullptr;

	return ValidateBuildContext(World);
}

bool FMassEntityTemplateBuildContext::ValidateBuildContext(const UWorld& World)
{
	// Group same types(key) together
	TraitAddedTypes.KeySort( [](const UStruct& LHS, const UStruct& RHS) { return LHS.GetName() < RHS.GetName(); } );

	// Loop through all the registered fragments and make sure only one trait registered them.
	const UStruct* CurrentStruct = nullptr;
	const UMassEntityTraitBase* CurrentTrait = nullptr;
	bool bHeaderOutputed = false;
	bool bFragmentHasMultipleOwners = false;
	for (const auto& Pair : TraitAddedTypes)
	{
		if (CurrentStruct != Pair.Key)
		{
			CurrentStruct = Pair.Key;
			CurrentTrait = Pair.Value;
			check(CurrentTrait);
			CurrentTrait->ValidateTemplate(*this, World);
			bHeaderOutputed = false;
		}
		else
		{
			if (!bHeaderOutputed)
			{
				UE_LOG(LogMass, Error, TEXT("Fragment(%s) was added multiple time and can only be added by one trait. Fragment was added by:"), CurrentStruct ? *CurrentStruct->GetName() : TEXT("null"));
				UE_LOG(LogMass, Error, TEXT("\t\t%s"), CurrentTrait ? *CurrentTrait->GetClass()->GetName() : TEXT("null"));
				bHeaderOutputed = true;
			}
			UE_LOG(LogMass, Error, TEXT("\t\t%s"), *Pair.Value->GetClass()->GetName());
			bFragmentHasMultipleOwners = true;
		}
 	}

	// Loop through all the traits dependencies and check if they have been added
	CurrentTrait = nullptr;
	bHeaderOutputed = false;
	bool bMissingFragmentDependencies = false;
	for (const auto& Dependency : TraitsDependencies)
	{
		if (CurrentTrait != Dependency.Get<1>())
		{
			CurrentTrait = Dependency.Get<1>();
			bHeaderOutputed = false;
		}
		if (!TraitAddedTypes.Contains(Dependency.Get<0>()))
		{
			if (!bHeaderOutputed)
			{
				check(CurrentTrait);
				UE_LOG(LogMass, Error, TEXT("Trait(%s) has missing dependency:"),  *CurrentTrait->GetClass()->GetName() );
				bHeaderOutputed = true;
			}
			UE_LOG(LogMass, Error, TEXT("\t\t%s"), *Dependency.Get<0>()->GetName());
			bMissingFragmentDependencies = true;
		}
	}

#if WITH_UNREAL_DEVELOPER_TOOLS
	if (bFragmentHasMultipleOwners || bMissingFragmentDependencies)
	{
		FMessageLog EditorErrors("LogMass");
		if (bFragmentHasMultipleOwners)
		{
			EditorErrors.Error(LOCTEXT("MassEntityTraitsFragmentOwnershipError", "Some fragments are added by multiple traits and can only be added by one!"));
			EditorErrors.Notify(LOCTEXT("MassEntityTraitsFragmentOwnershipError", "Some fragments are added by multiple traits and can only be added by one!"));
		}
		if (bMissingFragmentDependencies)
		{
			EditorErrors.Error(LOCTEXT("MassEntityTraitsMissingFragment", "Some traits are requiring the presence of fragments which are missing!"));
			EditorErrors.Notify(LOCTEXT("MassEntityTraitsMissingFragment", "Some traits are requiring the presence of fragments which are missing!"));
		}
		EditorErrors.Info(FText::FromString(TEXT("See the log for details")));
	}
#endif // WITH_UNREAL_DEVELOPER_TOOLS

	return !bFragmentHasMultipleOwners && !bMissingFragmentDependencies;
}

#undef LOCTEXT_NAMESPACE 
