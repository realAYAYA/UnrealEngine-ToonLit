// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "Subsystems/Subsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldSubsystem)

// ----------------------------------------------------------------------------------

UWorldSubsystem::UWorldSubsystem()
	: USubsystem()
{

}

UWorld& UWorldSubsystem::GetWorldRef() const
{
	return *CastChecked<UWorld>(GetOuter(), ECastCheckedType::NullChecked);
}

UWorld* UWorldSubsystem::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

bool UWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	UWorld* World = Cast<UWorld>(Outer);
	check(World);
	return DoesSupportWorldType(World->WorldType);
}

bool UWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::Editor || WorldType == EWorldType::PIE;
}


// ----------------------------------------------------------------------------------

UTickableWorldSubsystem::UTickableWorldSubsystem()
{

}

ETickableTickType UTickableWorldSubsystem::GetTickableTickType() const 
{ 
	// By default (if the child class doesn't override GetTickableTickType), don't let CDOs ever tick: 
	return IsTemplate() ? ETickableTickType::Never : FTickableGameObject::GetTickableTickType(); 
}

bool UTickableWorldSubsystem::IsAllowedToTick() const
{
	// No matter what IsTickable says, don't let CDOs or uninitialized world subsystems tick :
	// Note: even if GetTickableTickType was overridden by the child class and returns something else than ETickableTickType::Never for CDOs, 
	//  it's probably a mistake, so by default, don't allow ticking. If the child class really intends its CDO to tick, it can always override IsAllowedToTick...
	// NOTE: `bInitialized` must be checked first as `IsTemplate()` might access a dangling `Outer` if we are awaiting GC but `Outer` has already been deleted.
	return bInitialized && !IsTemplate();
}

void UTickableWorldSubsystem::Tick(float DeltaTime)
{
	checkf(IsInitialized(), TEXT("Ticking should have been disabled for an uninitialized subsystem : remember to call IsInitialized in the subsystem's IsTickable, IsTickableInEditor and/or IsTickableWhenPaused implementation"));
}

void UTickableWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	check(!bInitialized);
	bInitialized = true;
}

void UTickableWorldSubsystem::Deinitialize()
{
	check(bInitialized);
	bInitialized = false;
}

