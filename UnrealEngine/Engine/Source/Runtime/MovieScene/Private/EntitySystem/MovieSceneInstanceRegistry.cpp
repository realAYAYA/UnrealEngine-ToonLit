// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/BuiltInComponentTypes.h"

#include "Compilation/MovieSceneCompiledDataManager.h"

#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/Instances/MovieSceneTrackEvaluator.h"

#include "Engine/World.h"

#include "IMovieScenePlayer.h"

namespace UE
{
namespace MovieScene
{


FInstanceRegistry::FInstanceRegistry(UMovieSceneEntitySystemLinker* InLinker)
	: Linker(InLinker)
	, InstanceSerialNumber(0)
{
}

FInstanceRegistry::~FInstanceRegistry()
{
}

FInstanceHandle FInstanceRegistry::FindRelatedInstanceHandle(FInstanceHandle InstanceHandle, FMovieSceneSequenceID SequenceID) const
{
	checkfSlow(IsHandleValid(InstanceHandle), TEXT("Given instance handle is not valid."));
	checkfSlow(SequenceID.IsValid(), TEXT("Given sequence ID is not valid."));

	const FSequenceInstance* RootInstance = &GetInstance(InstanceHandle);

	if (SequenceID == MovieSceneSequenceID::Root)
	{
		return RootInstance->GetRootInstanceHandle();
	}

	if (!RootInstance->IsRootSequence())
	{
		RootInstance = &GetInstance(RootInstance->GetRootInstanceHandle());
	}
	return RootInstance->FindSubInstance(SequenceID);
}

FRootInstanceHandle FInstanceRegistry::AllocateRootInstance(IMovieScenePlayer* Player)
{
	check(Instances.Num() < 65535);

	const uint16 InstanceSerial = InstanceSerialNumber++;

	FSparseArrayAllocationInfo NewAllocation = Instances.AddUninitialized();
	FRootInstanceHandle InstanceHandle { (uint16)NewAllocation.Index, InstanceSerial };

	new (NewAllocation) FSequenceInstance(Linker, Player, InstanceHandle);

	return InstanceHandle;
}

FInstanceHandle FInstanceRegistry::AllocateSubInstance(IMovieScenePlayer* Player, FMovieSceneSequenceID SequenceID, FRootInstanceHandle RootInstanceHandle)
{
	check(Instances.Num() < 65535 && SequenceID != MovieSceneSequenceID::Root);

	const FMovieSceneRootEvaluationTemplateInstance& Template = Player->GetEvaluationTemplate();
	const FMovieSceneSequenceHierarchy*              Hierarchy = Template.GetCompiledDataManager()->FindHierarchy(Template.GetCompiledDataID());

	checkf(Hierarchy, TEXT("Attempting to construct a new sub sequence instance without a hierarchy"));

	const FMovieSceneSubSequenceData* SubData = Hierarchy->FindSubData(SequenceID);
	checkf(SubData, TEXT("Attempting to construct a new sub sequence instance with a sub sequence ID that does not exist in the hierarchy"));

	FMovieSceneCompiledDataID CompiledDataID = Template.GetCompiledDataManager()->GetDataID(SubData->GetSequence());


	const uint16 InstanceSerial = InstanceSerialNumber++;
	FSparseArrayAllocationInfo NewAllocation = Instances.AddUninitialized();
	FInstanceHandle InstanceHandle { (uint16)NewAllocation.Index, InstanceSerial };

	new (NewAllocation) FSequenceInstance(Linker, Player, InstanceHandle, RootInstanceHandle, SequenceID, CompiledDataID);

	return InstanceHandle;
}

void FInstanceRegistry::DestroyInstance(FInstanceHandle InstanceHandle)
{
	if (ensureMsgf(Instances.IsValidIndex(InstanceHandle.InstanceID) && Instances[InstanceHandle.InstanceID].GetSerialNumber() == InstanceHandle.InstanceSerial, TEXT("Attempting to destroy an instance an invalid instance handle.")))
	{
		FSequenceInstance& Instance = Instances[InstanceHandle.InstanceID];
		const bool bHasFinished = (GExitPurge || Instance.HasFinished());
		if (!bHasFinished)
		{
			UE_LOG(LogMovieSceneECS, Verbose, TEXT("Instance being destroyed without finishing evaluation."));
		}
		Instance.DestroyImmediately(Linker);
		Instances.RemoveAt(InstanceHandle.InstanceID);
	}
}

void FInstanceRegistry::PostInstantation()
{
	InvalidatedObjectBindings.Empty();
	Instances.Shrink();
}

void FInstanceRegistry::TagGarbage()
{
	for (FSequenceInstance& Instance : Instances)
	{
		Instance.Ledger.TagGarbage(Linker);
	}
}

void FInstanceRegistry::WorldCleanup(UWorld* World)
{
	auto Iter = [World](FMovieSceneEntityID EntityID, UObject*& BoundObject)
	{
		if (BoundObject && BoundObject->IsIn(World))
		{
			BoundObject = nullptr;
		}
	};

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Write(FBuiltInComponentTypes::Get()->BoundObject)
	.Iterate_PerEntity(&Linker->EntityManager, Iter);
}

void FInstanceRegistry::CleanupLinkerEntities(const TSet<FMovieSceneEntityID>& ExpiredBoundObjects)
{
	if (ExpiredBoundObjects.Num() != 0)
	{
		for (FSequenceInstance& Instance : Instances)
		{
			Instance.Ledger.CleanupLinkerEntities(ExpiredBoundObjects);
		}
	}
}

} // namespace MovieScene
} // namespace UE
