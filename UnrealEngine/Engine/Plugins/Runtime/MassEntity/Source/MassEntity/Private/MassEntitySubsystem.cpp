// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntitySubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntitySubsystem)



//////////////////////////////////////////////////////////////////////
// UMassEntitySubsystem

UMassEntitySubsystem::UMassEntitySubsystem()
	: EntityManager(MakeShareable(new FMassEntityManager(this)))
{
	
}

void UMassEntitySubsystem::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	EntityManager->GetResourceSizeEx(CumulativeResourceSize);
}

void UMassEntitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	EntityManager->Initialize();
}

void UMassEntitySubsystem::PostInitialize()
{
	// this needs to be done after all the subsystems have been initialized since some processors might want to access
	// them during processors' initialization
	EntityManager->PostInitialize();
}

void UMassEntitySubsystem::Deinitialize()
{
	EntityManager->Deinitialize();
	EntityManager.Reset();
}

#if WITH_MASSENTITY_DEBUG
//////////////////////////////////////////////////////////////////////
// Debug commands

FAutoConsoleCommandWithWorldArgsAndOutputDevice GPrintArchetypesCmd(
	TEXT("EntityManager.PrintArchetypes"),
	TEXT("Prints information about all archetypes in the current world"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
		{
			if (const UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr)
			{
				EntitySubsystem->GetEntityManager().DebugPrintArchetypes(Ar);
			}
			else
			{
				Ar.Logf(ELogVerbosity::Error, TEXT("Failed to find Entity Subsystem for world %s"), *GetPathNameSafe(World));
			}
		}));
#endif // WITH_MASSENTITY_DEBUG