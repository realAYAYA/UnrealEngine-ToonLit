// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackedLevelActor/PackedLevelActorRecursiveBuilder.h"

#if WITH_EDITOR

#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "PackedLevelActor/PackedLevelActor.h"

#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"

#include "Engine/Blueprint.h"

#include "Misc/Crc.h"

FPackedLevelActorBuilderID FPackedLevelActorRecursiveBuilder::BuilderID = 'RECP';

FPackedLevelActorBuilderID FPackedLevelActorRecursiveBuilder::GetID() const
{
	return BuilderID;
}

void FPackedLevelActorRecursiveBuilder::GetPackClusters(FPackedLevelActorBuilderContext& InContext, AActor* InActor) const
{
	if (ALevelInstance* LevelInstance = Cast<ALevelInstance>(InActor))
	{
		FPackedLevelActorBuilderClusterID ClusterID(MakeUnique<FPackedLevelActorRecursiveBuilderCluster>(GetID(), LevelInstance));
		InContext.FindOrAddCluster(MoveTemp(ClusterID));

		// This Actor can be safely discarded without warning because it is a container
		InContext.DiscardActor(LevelInstance);

		ULevelInstanceSubsystem* LevelInstanceSubsystem = LevelInstance->GetLevelInstanceSubsystem();
		check(LevelInstanceSubsystem);
		if (ULevel* Level = LevelInstanceSubsystem->GetLevelInstanceLevel(LevelInstance))
		{
			for (AActor* LevelActor : Level->Actors)
			{
				if (LevelActor)
				{
					InContext.ClusterLevelActor(LevelActor);
				}
			}
		}
	}
}

void FPackedLevelActorRecursiveBuilder::PackActors(FPackedLevelActorBuilderContext& InContext, APackedLevelActor* InPackingActor, const FPackedLevelActorBuilderClusterID& InClusterID, const TArray<UActorComponent*>& InComponents) const
{
	check(InClusterID.GetBuilderID() == GetID());
	FPackedLevelActorRecursiveBuilderCluster* LevelInstanceCluster = (FPackedLevelActorRecursiveBuilderCluster*)InClusterID.GetData();
	check(LevelInstanceCluster);

	if (LevelInstanceCluster->LevelInstance->IsA<APackedLevelActor>())
	{
		if (UBlueprint* GeneratedBy = Cast<UBlueprint>(LevelInstanceCluster->LevelInstance->GetClass()->ClassGeneratedBy))
		{
			InPackingActor->PackedBPDependencies.AddUnique(GeneratedBy);
		}
	}
}

FPackedLevelActorRecursiveBuilderCluster::FPackedLevelActorRecursiveBuilderCluster(FPackedLevelActorBuilderID InBuilderID, ALevelInstance* InLevelInstance)
	: FPackedLevelActorBuilderCluster(InBuilderID), LevelInstance(InLevelInstance)
{
}

bool FPackedLevelActorRecursiveBuilderCluster::Equals(const FPackedLevelActorBuilderCluster& InOther) const
{
	if (!FPackedLevelActorBuilderCluster::Equals(InOther))
	{
		return false;
	}

	const FPackedLevelActorRecursiveBuilderCluster& LevelInstanceCluster = (const FPackedLevelActorRecursiveBuilderCluster&)InOther;
	return LevelInstance == LevelInstanceCluster.LevelInstance;
}
	
uint32 FPackedLevelActorRecursiveBuilderCluster::ComputeHash() const
{
	return FCrc::TypeCrc32(LevelInstance->GetLevelInstanceID(), FPackedLevelActorBuilderCluster::ComputeHash());
}

#endif
