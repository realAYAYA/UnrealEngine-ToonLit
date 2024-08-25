// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassSubsystemBase.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"


namespace UE::Mass::Private
{
/** 
 * A helper function calling PostInitialize and OnWorldBeginPlay for the given subsystem, provided the world has already begun play.
 * @see UMassSubsystemBase::HandleLateCreation for more detail
 */
void HandleLateCreation(UWorldSubsystem& MassWorldSubsystem, const UE::Mass::Subsystems::FInitializationState InitializationState)
{
	// handle late creation
	UWorld* World = MassWorldSubsystem.GetWorld();
	if (World)
	{
		if (World->IsInitialized() == true && InitializationState.bPostInitializeCalled == false)
		{
			MassWorldSubsystem.PostInitialize();
		}
		if (World->HasBegunPlay() == true && InitializationState.bOnWorldBeginPlayCalled == false)
		{
			MassWorldSubsystem.OnWorldBeginPlay(*World);
		}
	}
}

bool bRuntimeSubsystemsEnabled = true;

namespace
{
	FAutoConsoleVariableRef AnonymousCVars[] =
	{
		{ TEXT("mass.RuntimeSubsystemsEnabled")
		, bRuntimeSubsystemsEnabled
		, TEXT("true by default, setting to false will prevent auto-creation of game-time Mass-related subsystems. Needs to be set before world loading.")
		, ECVF_Default }
	};
}
} // UE::Mass::Private


//-----------------------------------------------------------------------------
// UMassSubsystemBase
//-----------------------------------------------------------------------------
bool UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(UObject* Outer)
{
	return UE::Mass::Private::bRuntimeSubsystemsEnabled;
}

bool UMassSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const 
{
	return UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(Outer) && Super::ShouldCreateSubsystem(Outer);
}

void UMassSubsystemBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bInitializeCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bInitializeCalled = true;
}

void UMassSubsystemBase::PostInitialize()
{
	Super::PostInitialize();

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bPostInitializeCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bPostInitializeCalled = true;
}

void UMassSubsystemBase::Deinitialize()
{
	InitializationState = UE::Mass::Subsystems::FInitializationState();

	Super::Deinitialize();
}

void UMassSubsystemBase::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bOnWorldBeginPlayCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bOnWorldBeginPlayCalled = true;
}

void UMassSubsystemBase::HandleLateCreation()
{
	UE::Mass::Private::HandleLateCreation(*this, InitializationState);
}

//-----------------------------------------------------------------------------
// UMassTickableSubsystemBase
//-----------------------------------------------------------------------------
bool UMassTickableSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const
{
	return UMassSubsystemBase::AreRuntimeMassSubsystemsAllowed(Outer) && Super::ShouldCreateSubsystem(Outer);
}

void UMassTickableSubsystemBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bInitializeCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bInitializeCalled = true;
}

void UMassTickableSubsystemBase::PostInitialize()
{
	Super::PostInitialize();

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bPostInitializeCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bPostInitializeCalled = true;
}

void UMassTickableSubsystemBase::Deinitialize()
{
	InitializationState = UE::Mass::Subsystems::FInitializationState();

	Super::Deinitialize();
}

void UMassTickableSubsystemBase::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// This ensure is here to make sure we handle HandleLateCreation() gracefully, we dont expect it to ever trigger unless users start to manually call the functions
	ensureMsgf(InitializationState.bOnWorldBeginPlayCalled == false, TEXT("%hs called multiple times"), __FUNCTION__);
	InitializationState.bOnWorldBeginPlayCalled = true;
}

void UMassTickableSubsystemBase::HandleLateCreation()
{
	UE::Mass::Private::HandleLateCreation(*this, InitializationState);
}
