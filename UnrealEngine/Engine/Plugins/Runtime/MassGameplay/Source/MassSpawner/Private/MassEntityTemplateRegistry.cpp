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
	uint32 CalcHash(const FInstancedStruct& StructInstance)
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
			UMassEntityTemplateRegistry& Registry = SpawnerSystem->GetTemplateRegistryInstance();
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
// UMassEntityTemplateRegistry 
//----------------------------------------------------------------------//
TMap<const UScriptStruct*, UMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate> UMassEntityTemplateRegistry::StructBasedBuilders;

void UMassEntityTemplateRegistry::BeginDestroy()
{
	// force release of memory owned by individual templates (especially the hosted InstancedScriptStructs).
	TemplateIDToTemplateMap.Reset();

	Super::BeginDestroy();
}

UWorld* UMassEntityTemplateRegistry::GetWorld() const 
{
	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

UMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate& UMassEntityTemplateRegistry::FindOrAdd(const UScriptStruct& DataType)
{
	return StructBasedBuilders.FindOrAdd(&DataType);
}

const FMassEntityTemplate* UMassEntityTemplateRegistry::FindOrBuildStructTemplate(const FInstancedStruct& StructInstance)
{
	// thou shall not call this function on CDO
	check(HasAnyFlags(RF_ClassDefaultObject) == false);

	const UScriptStruct* Type = StructInstance.GetScriptStruct();
	check(Type);
	// 1. Check if we already have the template stored.
	// 2. If not, 
	//	a. build it
	//	b. store it

	const uint32 StructInstanceHash = FTemplateRegistryHelpers::CalcHash(StructInstance);
	const uint32 HashLookup = HashCombine(GetTypeHash(Type->GetFName()), StructInstanceHash);

	FMassEntityTemplateID* TemplateID = LookupTemplateIDMap.Find(HashLookup);

	if (TemplateID != nullptr)
	{
		if (const FMassEntityTemplate* TemplateFound = TemplateIDToTemplateMap.Find(*TemplateID))
		{
			return TemplateFound;
		}
	}

	// this means we don't have an entry for given struct. Let's see if we know how to make one
	FMassEntityTemplate* NewTemplate = nullptr;
	FStructToTemplateBuilderDelegate* Builder = StructBasedBuilders.Find(StructInstance.GetScriptStruct());
	if (Builder)
	{
		if (TemplateID == nullptr)
		{
			// TODO consider removing the need for strings here
			// Use the class name string for the hash here so the hash can be deterministic between client and server
			const uint32 NameStringHash = GetTypeHash(Type->GetName());
			const uint32 Hash = HashCombine(NameStringHash, StructInstanceHash);

			TemplateID = &LookupTemplateIDMap.Add(HashLookup, FMassEntityTemplateID(Hash, EMassEntityTemplateIDType::ScriptStruct));
		}

		NewTemplate = &TemplateIDToTemplateMap.Add(*TemplateID);

		check(NewTemplate);
		NewTemplate->SetTemplateID(*TemplateID);

		BuildTemplateImpl(*Builder, StructInstance, *NewTemplate);
	}
	UE_CVLOG_UELOG(Builder == nullptr, this, LogMassSpawner, Warning, TEXT("Attempting to build a MassAgentTemplate for struct type %s while template builder has not been registered for this type")
		, *GetNameSafe(Type));

	return NewTemplate;
}

bool UMassEntityTemplateRegistry::BuildTemplateImpl(const FStructToTemplateBuilderDelegate& Builder, const FInstancedStruct& StructInstance, FMassEntityTemplate& OutTemplate)
{
	UWorld* World = GetWorld();
	FMassEntityTemplateBuildContext Context(OutTemplate);
	Builder.Execute(World, StructInstance, Context);
	if (ensure(!OutTemplate.IsEmpty())) // need at least one fragment to create an Archetype
	{
		InitializeEntityTemplate(OutTemplate);

		UE_VLOG(this, LogMassSpawner, Log, TEXT("Created entity template for %s:\n%s"), *GetNameSafe(StructInstance.GetScriptStruct())
			, *OutTemplate.DebugGetDescription(UE::Mass::Utils::GetEntityManager(World)));

		return true;
	}
	return false;
}

void UMassEntityTemplateRegistry::InitializeEntityTemplate(FMassEntityTemplate& OutTemplate) const
{
	// expected to be ensured by the caller
	check(!OutTemplate.IsEmpty());

	UWorld* World = GetWorld();
	check(World);
	// find or create template
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// Sort anything there is to sort for later comparison purposes
	OutTemplate.Sort();

	const FMassArchetypeHandle ArchetypeHandle = EntityManager.CreateArchetype(OutTemplate.GetCompositionDescriptor(), FName(OutTemplate.GetTemplateName()));
	OutTemplate.SetArchetype(ArchetypeHandle);
}

void UMassEntityTemplateRegistry::DebugReset()
{
#if WITH_MASSGAMEPLAY_DEBUG
	LookupTemplateIDMap.Reset();
	TemplateIDToTemplateMap.Reset();
#endif // WITH_MASSGAMEPLAY_DEBUG
}

const FMassEntityTemplate* UMassEntityTemplateRegistry::FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const 
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

FMassEntityTemplate* UMassEntityTemplateRegistry::FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID) 
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

FMassEntityTemplate& UMassEntityTemplateRegistry::CreateTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID)
{
	checkSlow(!LookupTemplateIDMap.Contains(HashLookup));
	LookupTemplateIDMap.Add(HashLookup, TemplateID);
	checkSlow(!TemplateIDToTemplateMap.Contains(TemplateID));
	FMassEntityTemplate& NewTemplate = TemplateIDToTemplateMap.Add(TemplateID);
	NewTemplate.SetTemplateID(TemplateID);
	return NewTemplate;
}

void UMassEntityTemplateRegistry::DestroyTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID)
{
	LookupTemplateIDMap.Remove(HashLookup);
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
				check(CurrentStruct);
				check(CurrentTrait);
				UE_LOG(LogMass, Error, TEXT("Fragment(%s) was added multiple time and can only be added by one trait. Fragment was added by:"), *CurrentStruct->GetName());
				UE_LOG(LogMass, Error, TEXT("\t\t%s"), *CurrentTrait->GetClass()->GetName());
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
