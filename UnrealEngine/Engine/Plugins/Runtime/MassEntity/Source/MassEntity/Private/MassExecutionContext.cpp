// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassExecutionContext.h"
#include "MassArchetypeData.h"


//////////////////////////////////////////////////////////////////////////
// FMassExecutionContext

FMassExecutionContext::FMassExecutionContext(FMassEntityManager& InEntityManager, const float InDeltaTimeSeconds, const bool bInFlushDeferredCommands)
	: SubsystemAccess(InEntityManager.GetWorld())
	, DeltaTimeSeconds(InDeltaTimeSeconds)
	, EntityManager(InEntityManager.AsShared())
	, bFlushDeferredCommands(bInFlushDeferredCommands)
{
}

void FMassExecutionContext::FlushDeferred()
{
	if (bFlushDeferredCommands && DeferredCommandBuffer)
	{
		EntityManager->FlushCommands(DeferredCommandBuffer);
	}
}

void FMassExecutionContext::ClearExecutionData()
{
	FragmentViews.Reset();
	ChunkFragmentViews.Reset();
	ConstSharedFragmentViews.Reset();
	SharedFragmentViews.Reset();
	CurrentArchetypeCompositionDescriptor.Reset();
	ChunkSerialModificationNumber = -1;
}

bool FMassExecutionContext::CacheSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements)
{
	return SubsystemAccess.CacheSubsystemRequirements(SubsystemRequirements);
}

void FMassExecutionContext::SetEntityCollection(const FMassArchetypeEntityCollection& InEntityCollection)
{
	check(EntityCollection.IsEmpty());
	EntityCollection = InEntityCollection;
}

void FMassExecutionContext::SetEntityCollection(FMassArchetypeEntityCollection&& InEntityCollection)
{
	check(EntityCollection.IsEmpty());
	EntityCollection = MoveTemp(InEntityCollection);
}

void FMassExecutionContext::SetFragmentRequirements(const FMassFragmentRequirements& FragmentRequirements)
{
	FragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			FragmentViews.Emplace(Requirement);
		}
	}

	ChunkFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetChunkFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			ChunkFragmentViews.Emplace(Requirement);
		}
	}

	ConstSharedFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetConstSharedFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			ConstSharedFragmentViews.Emplace(Requirement);
		}
	}

	SharedFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetSharedFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			SharedFragmentViews.Emplace(Requirement);
		}
	}
}

UWorld* FMassExecutionContext::GetWorld() 
{ 
	return EntityManager->GetWorld(); 
}
