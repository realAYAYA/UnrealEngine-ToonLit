// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorDescList.h"
#include "WorldPartition/WorldPartitionLog.h"

#if WITH_EDITOR
FWorldPartitionActorDesc* FActorDescList::AddActor(const AActor* InActor)
{
	FWorldPartitionActorDesc* NewActorDesc = InActor->CreateActorDesc().Release();
	check(NewActorDesc);

	AddActorDescriptor(NewActorDesc);

	return NewActorDesc;
}

const FWorldPartitionActorDesc* FActorDescList::GetActorDesc(const FGuid& Guid) const
{
	const TUniquePtr<FWorldPartitionActorDesc>* const * ActorDesc = ActorsByGuid.Find(Guid);
	return ActorDesc ? (*ActorDesc)->Get() : nullptr;
}

FWorldPartitionActorDesc* FActorDescList::GetActorDesc(const FGuid& Guid)
{
	TUniquePtr<FWorldPartitionActorDesc>** ActorDesc = ActorsByGuid.Find(Guid);
	return ActorDesc ? (*ActorDesc)->Get() : nullptr;
}

const FWorldPartitionActorDesc& FActorDescList::GetActorDescChecked(const FGuid& Guid) const
{
	const TUniquePtr<FWorldPartitionActorDesc>* const ActorDesc = ActorsByGuid.FindChecked(Guid);
	return *ActorDesc->Get();
}

FWorldPartitionActorDesc& FActorDescList::GetActorDescChecked(const FGuid& Guid)
{
	TUniquePtr<FWorldPartitionActorDesc>* ActorDesc = ActorsByGuid.FindChecked(Guid);
	return *ActorDesc->Get();
}

const FWorldPartitionActorDesc* FActorDescList::GetActorDesc(const FString& ActorPath) const
{
	FString ActorName;
	FString ActorContext;
	if (!ActorPath.Split(TEXT("."), &ActorContext, &ActorName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		ActorName = ActorPath;
	}

	if (const TUniquePtr<FWorldPartitionActorDesc>* const* ActorDesc = ActorsByName.Find(*ActorName))
	{
		return (*ActorDesc)->Get();
	}

	return nullptr;
}

const FWorldPartitionActorDesc* FActorDescList::GetActorDesc(const FSoftObjectPath& ActorPath) const
{
	return GetActorDesc(ActorPath.ToString());
}

void FActorDescList::Empty()
{
	ActorsByGuid.Empty();
	ActorsByName.Empty();
	ActorDescList.Empty();
}

void FActorDescList::AddActorDescriptor(FWorldPartitionActorDesc* ActorDesc)
{
	check(ActorDesc);

	if (const TUniquePtr<FWorldPartitionActorDesc>* const* ExistingActorDesc = ActorsByGuid.Find(ActorDesc->GetGuid()))
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("Duplicated actor descriptor guid '%s' detected: `%s` (existing: `%s`)"), *ActorDesc->GetGuid().ToString(), *ActorDesc->GetActorName().ToString(), *(*ExistingActorDesc)->Get()->GetActorName().ToString());
	}
	else
	{
		TUniquePtr<FWorldPartitionActorDesc>* NewActorDesc = new(ActorDescList) TUniquePtr<FWorldPartitionActorDesc>(ActorDesc);
		ActorsByGuid.Add(ActorDesc->GetGuid(), NewActorDesc);
		ActorsByName.Add(ActorDesc->GetActorName(), NewActorDesc);
	}
}

void FActorDescList::RemoveActorDescriptor(FWorldPartitionActorDesc* ActorDesc)
{
	check(ActorDesc);
	verify(ActorsByGuid.Remove(ActorDesc->GetGuid()));
	verify(ActorsByName.Remove(ActorDesc->GetActorName()));
}

TUniquePtr<FWorldPartitionActorDesc>* FActorDescList::GetActorDescriptor(const FGuid& ActorGuid)
{
	return ActorsByGuid.FindRef(ActorGuid);
}
#endif