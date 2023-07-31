// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStateTreeTrait.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "StateTree.h"
#include "VisualLogger/VisualLogger.h"
#include "MassEntityTemplateRegistry.h"
#include "Engine/World.h"
#include "MassEntityUtils.h"


void UMassStateTreeTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	UMassStateTreeSubsystem* MassStateTreeSubsystem = World.GetSubsystem<UMassStateTreeSubsystem>();
	if (!MassStateTreeSubsystem)
	{
		UE_VLOG(&World, LogMassBehavior, Error, TEXT("Failed to get Mass StateTree Subsystem."));
		return;
	}

	if (!StateTree)
	{
		UE_VLOG(MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree asset is not set or unavailable."));
		return;
	}
	if (!StateTree->IsReadyToRun())
	{
		UE_VLOG(MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree asset is ready to run."));
		return;
	}

	FMassStateTreeSharedFragment SharedStateTree;
	SharedStateTree.StateTree = StateTree;
	
	const FConstSharedStruct StateTreeFragment = EntityManager.GetOrCreateConstSharedFragment(SharedStateTree);
	BuildContext.AddConstSharedFragment(StateTreeFragment);

	BuildContext.AddFragment<FMassStateTreeInstanceFragment>();
}

void UMassStateTreeTrait::ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	UMassStateTreeSubsystem* MassStateTreeSubsystem = World.GetSubsystem<UMassStateTreeSubsystem>();
	if (!MassStateTreeSubsystem)
	{
		UE_VLOG(&World, LogMassBehavior, Error, TEXT("Failed to get Mass StateTree Subsystem."));
		return;
	}

	if (!StateTree)
	{
		UE_VLOG(MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree asset is not set or unavailable."));
		return;
	}

	// Make sure all the required subsystems can be found.
	for (const FStateTreeExternalDataDesc& ItemDesc : StateTree->GetExternalDataDescs())
	{
		if (ensure(ItemDesc.Struct) && ItemDesc.Requirement == EStateTreeExternalDataRequirement::Required)
		{
			if (ItemDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				const TSubclassOf<UWorldSubsystem> SubClass = Cast<UClass>(const_cast<UStruct*>(ToRawPtr(ItemDesc.Struct)));
				USubsystem* Subsystem = World.GetSubsystemBase(SubClass);
				UE_CVLOG(!Subsystem, MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree %s: Could not find required subsystem %s"), *GetNameSafe(StateTree), *GetNameSafe(ItemDesc.Struct));
			}
			else if (ItemDesc.Struct->IsChildOf(FMassFragment::StaticStruct()))
			{
				const bool bContainsFragment = BuildContext.HasFragment(*CastChecked<UScriptStruct>(ItemDesc.Struct));
				UE_CVLOG(!bContainsFragment, MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree %s: Could not find required fragment %s"), *GetNameSafe(StateTree), *GetNameSafe(ItemDesc.Struct));
			}
			else if (ItemDesc.Struct->IsChildOf(FMassSharedFragment::StaticStruct()))
			{
				const bool bContainsFragment = BuildContext.HasSharedFragment(*CastChecked<UScriptStruct>(ItemDesc.Struct));
				UE_CVLOG(!bContainsFragment, MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree %s: Could not find required shared fragment %s"), *GetNameSafe(StateTree), *GetNameSafe(ItemDesc.Struct));
			}
			else
			{
				UE_VLOG(MassStateTreeSubsystem, LogMassBehavior, Error, TEXT("StateTree %s: Unsupported requirement %s"), *GetNameSafe(StateTree), *GetNameSafe(ItemDesc.Struct));
			}
		}
	}
}
