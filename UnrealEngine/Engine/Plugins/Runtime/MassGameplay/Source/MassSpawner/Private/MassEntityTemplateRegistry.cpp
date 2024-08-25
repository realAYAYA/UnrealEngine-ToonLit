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

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "Mass"

//----------------------------------------------------------------------//
// FMassEntityTemplateRegistry 
//----------------------------------------------------------------------//
TMap<const UScriptStruct*, FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate> FMassEntityTemplateRegistry::StructBasedBuilders;

FMassEntityTemplateRegistry::FMassEntityTemplateRegistry(UObject* InOwner)
	: Owner(InOwner)
{
}

void FMassEntityTemplateRegistry::ShutDown()
{
	TemplateIDToTemplateMap.Reset();
	EntityManager = nullptr;
}

UWorld* FMassEntityTemplateRegistry::GetWorld() const 
{
	return Owner.IsValid() ? Owner->GetWorld() : nullptr;
}

FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate& FMassEntityTemplateRegistry::FindOrAdd(const UScriptStruct& DataType)
{
	return StructBasedBuilders.FindOrAdd(&DataType);
}

void FMassEntityTemplateRegistry::Initialize(const TSharedPtr<FMassEntityManager>& InEntityManager)
{
	if (EntityManager)
	{
		ensureMsgf(EntityManager == InEntityManager, TEXT("Attempting to store a different EntityManager then the previously stored one - this indicated a set up issue, attempting to use multiple EntityManager instances"));
		return;
	}

	EntityManager = InEntityManager;
}

void FMassEntityTemplateRegistry::DebugReset()
{
#if WITH_MASSGAMEPLAY_DEBUG
	TemplateIDToTemplateMap.Reset();
#endif // WITH_MASSGAMEPLAY_DEBUG
}

const TSharedRef<FMassEntityTemplate>* FMassEntityTemplateRegistry::FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

const TSharedRef<FMassEntityTemplate>& FMassEntityTemplateRegistry::FindOrAddTemplate(FMassEntityTemplateID TemplateID, FMassEntityTemplateData&& TemplateData)
{
	check(EntityManager);
	const TSharedRef<FMassEntityTemplate>* ExistingTemplate = FindTemplateFromTemplateID(TemplateID);
	if (ExistingTemplate != nullptr)
	{
		return *ExistingTemplate;
	}

	return TemplateIDToTemplateMap.Add(TemplateID, FMassEntityTemplate::MakeFinalTemplate(*EntityManager, MoveTemp(TemplateData), TemplateID));
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
	bool bHeaderOutputted = false;
	bool bFragmentHasMultipleOwners = false;
	for (const auto& Pair : TraitAddedTypes)
	{
		if (CurrentStruct != Pair.Key)
		{
			CurrentStruct = Pair.Key;
			CurrentTrait = Pair.Value;
			check(CurrentTrait);
			CurrentTrait->ValidateTemplate(*this, World);
			bHeaderOutputted = false;
		}
		else
		{
			if (!bHeaderOutputted)
			{
				UE_LOG(LogMass, Warning, TEXT("%s: Fragment(%s) was added multiple time and should only be added by one trait. Fragment was added by:")
					, CurrentTrait ? *GetNameSafe(CurrentTrait->GetOuter()) : TEXT("None")
					, CurrentStruct ? *CurrentStruct->GetName() : TEXT("null"));
				UE_LOG(LogMass, Warning, TEXT("\t\t%s"), CurrentTrait ? *CurrentTrait->GetClass()->GetName() : TEXT("null"));
				bHeaderOutputted = true;
			}
			UE_LOG(LogMass, Warning, TEXT("\t\t%s"), *Pair.Value->GetClass()->GetName());
			bFragmentHasMultipleOwners = true;
		}
 	}

	// Loop through all the traits dependencies and check if they have been added
	CurrentTrait = nullptr;
	bHeaderOutputted = false;
	bool bMissingFragmentDependencies = false;
	for (const auto& Dependency : TraitsDependencies)
	{
		if (CurrentTrait != Dependency.Get<1>())
		{
			CurrentTrait = Dependency.Get<1>();
			bHeaderOutputted = false;
		}
		if (!TraitAddedTypes.Contains(Dependency.Get<0>()))
		{
			if (!bHeaderOutputted)
			{
				check(CurrentTrait);
				UE_LOG(LogMass, Error, TEXT("%s: Trait(%s) has missing dependency:"), *GetNameSafe(CurrentTrait->GetOuter())
					, *CurrentTrait->GetClass()->GetName());
				bHeaderOutputted = true;
			}
			UE_LOG(LogMass, Error, TEXT("\t\t%s"), *Dependency.Get<0>()->GetName());
			bMissingFragmentDependencies = true;
		}
	}

#if WITH_UNREAL_DEVELOPER_TOOLS && WITH_EDITOR
	if (GEditor && (bFragmentHasMultipleOwners || bMissingFragmentDependencies))
	{
		FMessageLog EditorErrors("MassEntity");
		if (bFragmentHasMultipleOwners)
		{
			EditorErrors.Warning(LOCTEXT("MassEntityTraitsFragmentOwnershipError", "Some fragments are added by multiple traits and can only be added by one!"));
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

//-----------------------------------------------------------------------------
// DEPRECATED
//-----------------------------------------------------------------------------
FMassEntityTemplate* FMassEntityTemplateRegistry::FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID)
{
	return nullptr;
}

FMassEntityTemplate& FMassEntityTemplateRegistry::CreateTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID)
{
	static FMassEntityTemplate Dummy;
	return Dummy;
}

void FMassEntityTemplateRegistry::InitializeEntityTemplate(FMassEntityTemplate& InOutTemplate) const
{}

#undef LOCTEXT_NAMESPACE 
