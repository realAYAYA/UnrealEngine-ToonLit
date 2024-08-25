// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorldLibrary.h"
#include "Engine/World.h"
#include "Scheduler/AnimNextTickFunctionBinding.h"

UClass* UAnimNextWorldLibrary::GetSupportedClass() const
{
	return UWorld::StaticClass();
}

double UAnimNextWorldLibrary::GetTimeSeconds(UWorld* InWorld)
{
	return InWorld->GetTimeSeconds();
}

double UAnimNextWorldLibrary::GetUnpausedTimeSeconds(UWorld* InWorld)
{
	return InWorld->GetUnpausedTimeSeconds();
}

double UAnimNextWorldLibrary::GetRealTimeSeconds(UWorld* InWorld)
{
	return InWorld->GetRealTimeSeconds();
}

float UAnimNextWorldLibrary::GetDeltaSeconds(UWorld* InWorld)
{
	return InWorld->GetDeltaSeconds();
}

FAnimNextTickFunctionBinding UAnimNextWorldLibrary::GetPhysicsTick(UWorld* InWorld)
{
	FAnimNextTickFunctionBinding Binding;
	Binding.Object = InWorld;
	Binding.TickFunction = &InWorld->EndPhysicsTickFunction;
	return Binding;
}
