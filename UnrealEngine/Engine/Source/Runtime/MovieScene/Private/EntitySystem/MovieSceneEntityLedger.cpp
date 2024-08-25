// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntityLedger.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntityMutations.h"

#include "Evaluation/MovieSceneEvaluationField.h"

#include "MovieSceneSection.h"

namespace UE
{
namespace MovieScene
{


void FEntityLedger::UpdateEntities(UMovieSceneEntitySystemLinker* Linker, const FEntityImportSequenceParams& ImportParams, const FMovieSceneEntityComponentField* EntityField, const FMovieSceneEvaluationFieldEntitySet& NewEntities)
{
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();
	if (NewEntities.Num() != 0)
	{
		// Destroy any entities that are no longer relevant
		if (ImportedEntities.Num() != 0)
		{
			FComponentMask FinishedMask = FBuiltInComponentTypes::Get()->FinishedMask;

			for (auto It = ImportedEntities.CreateIterator(); It; ++It)
			{
				if (!NewEntities.Contains(It.Key()))
				{
					if (It.Value().EntityID)
					{
						Linker->EntityManager.AddComponents(It.Value().EntityID, FinishedMask, EEntityRecursion::Full);
					}
					It.RemoveCurrent();
				}
			}
		}

		// If we've invalidated or we haven't imported anything yet, we can simply (re)import everything
		if (ImportedEntities.Num() == 0 || bInvalidated)
		{
			for (const FMovieSceneEvaluationFieldEntityQuery& Query : NewEntities)
			{
				ImportEntity(Linker, ImportParams, EntityField, Query);
			}
		}
		else for (const FMovieSceneEvaluationFieldEntityQuery& Query : NewEntities)
		{
			FImportedEntityData Existing = ImportedEntities.FindRef(Query.Entity.Key);
			if (!Existing.EntityID || Existing.MetaDataIndex != Query.MetaDataIndex)
			{
				ImportEntity(Linker, ImportParams, EntityField, Query);
			}
		}
	}
	else
	{
		UnlinkEverything(Linker);
	}

	// Nothing is invalidated now
	bInvalidated = false;
}

void FEntityLedger::UpdateOneShotEntities(UMovieSceneEntitySystemLinker* Linker, const FEntityImportSequenceParams& ImportParams, const FMovieSceneEntityComponentField* EntityField, const FMovieSceneEvaluationFieldEntitySet& NewEntities)
{
	checkf(OneShotEntities.Num() == 0, TEXT("One shot entities should not be updated multiple times per-evaluation. They must not have gotten cleaned up correctly."));
	if (NewEntities.Num() == 0)
	{
		return;
	}

	FEntityImportParams Params;
	Params.Sequence = ImportParams;

	for (const FMovieSceneEvaluationFieldEntityQuery& Query : NewEntities)
	{
		UObject* EntityOwner = Query.Entity.Key.EntityOwner.Get();
		IMovieSceneEntityProvider* Provider = Cast<IMovieSceneEntityProvider>(EntityOwner);
		if (!Provider)
		{
			continue;
		}

		Params.EntityID = Query.Entity.Key.EntityID;
		Params.EntityMetaData = EntityField->FindMetaData(Query);
		Params.SharedMetaData = EntityField->FindSharedMetaData(Query);

		if (ImportParams.bPreRoll && (Params.EntityMetaData == nullptr || Params.EntityMetaData->bEvaluateInSequencePreRoll == false))
		{
			return;
		}
		if (ImportParams.bPostRoll && (Params.EntityMetaData == nullptr || Params.EntityMetaData->bEvaluateInSequencePostRoll == false))
		{
			return;
		}

		FImportedEntity ImportedEntity;
		Provider->ImportEntity(Linker, Params, &ImportedEntity);

		if (!ImportedEntity.IsEmpty())
		{
			if (UMovieSceneSection* Section = Cast<UMovieSceneSection>(EntityOwner))
			{
				Section->BuildDefaultComponents(Linker, Params, &ImportedEntity);
			}

			FMovieSceneEntityID NewEntityID = ImportedEntity.Manufacture(Params, &Linker->EntityManager);
			OneShotEntities.Add(NewEntityID);
		}
	}
}

void FEntityLedger::Invalidate()
{
	bInvalidated = true;
}

bool FEntityLedger::IsEmpty() const
{
	return ImportedEntities.Num() == 0;
}

bool FEntityLedger::HasImportedEntity(const FMovieSceneEvaluationFieldEntityKey& EntityKey) const
{
	return ImportedEntities.Contains(EntityKey);
}

FMovieSceneEntityID FEntityLedger::FindImportedEntity(const FMovieSceneEvaluationFieldEntityKey& EntityKey) const
{
	return ImportedEntities.FindRef(EntityKey).EntityID;
}

void FEntityLedger::FindImportedEntities(TWeakObjectPtr<UObject> EntityOwner, TArray<FMovieSceneEntityID>& OutEntityIDs) const
{
	for (const TPair<FMovieSceneEvaluationFieldEntityKey, FImportedEntityData>& Pair : ImportedEntities)
	{
		if (Pair.Key.EntityOwner == EntityOwner)
		{
			OutEntityIDs.Add(Pair.Value.EntityID);
		}
	}
}

void FEntityLedger::ImportEntity(UMovieSceneEntitySystemLinker* Linker, const FEntityImportSequenceParams& ImportParams, const FMovieSceneEntityComponentField* EntityField, const FMovieSceneEvaluationFieldEntityQuery& Query)
{
	// We always add an entry even if no entity was imported by the provider to ensure that we do not repeatedly try and import the same entity every frame
	FImportedEntityData& EntityData = ImportedEntities.FindOrAdd(Query.Entity.Key);
	EntityData.MetaDataIndex = Query.MetaDataIndex;

	UObject* EntityOwner = Query.Entity.Key.EntityOwner.Get();
	IMovieSceneEntityProvider* Provider = Cast<IMovieSceneEntityProvider>(EntityOwner);
	if (!Provider)
	{
		return;
	}

	FEntityImportParams Params;
	Params.Sequence = ImportParams;
	Params.EntityID = Query.Entity.Key.EntityID;
	Params.EntityMetaData = EntityField->FindMetaData(Query);
	Params.SharedMetaData = EntityField->FindSharedMetaData(Query);

	if (ImportParams.bPreRoll && (Params.EntityMetaData == nullptr || Params.EntityMetaData->bEvaluateInSequencePreRoll == false))
	{
		return;
	}
	if (ImportParams.bPostRoll && (Params.EntityMetaData == nullptr || Params.EntityMetaData->bEvaluateInSequencePostRoll == false))
	{
		return;
	}

	FImportedEntity ImportedEntity;
	Provider->ImportEntity(Linker, Params, &ImportedEntity);

	if (!ImportedEntity.IsEmpty())
	{
		if (UMovieSceneSection* Section = Cast<UMovieSceneSection>(EntityOwner))
		{
			Section->BuildDefaultComponents(Linker, Params, &ImportedEntity);
		}

		FMovieSceneEntityID NewEntityID = ImportedEntity.Manufacture(Params, &Linker->EntityManager);

		Linker->EntityManager.ReplaceEntityID(EntityData.EntityID, NewEntityID);
	}
}

void FEntityLedger::UnlinkEverything(UMovieSceneEntitySystemLinker* Linker, EUnlinkEverythingMode UnlinkMode)
{
	FComponentTypeID NeedsLink = FBuiltInComponentTypes::Get()->Tags.NeedsLink;
	FComponentMask FinishedMask = FBuiltInComponentTypes::Get()->FinishedMask;

	for (TPair<FMovieSceneEvaluationFieldEntityKey, FImportedEntityData>& Pair : ImportedEntities)
	{
		if (Pair.Value.EntityID)
		{
			if (UnlinkMode == EUnlinkEverythingMode::CleanGarbage)
			{
				Linker->EntityManager.RemoveComponent(Pair.Value.EntityID, NeedsLink, EEntityRecursion::Full);
			}
			Linker->EntityManager.AddComponents(Pair.Value.EntityID, FinishedMask, EEntityRecursion::Full);
		}
	}
	ImportedEntities.Empty();
}

void FEntityLedger::UnlinkOneShots(UMovieSceneEntitySystemLinker* Linker)
{
	FComponentMask FinishedMask = FBuiltInComponentTypes::Get()->FinishedMask;

	for (FMovieSceneEntityID Entity : OneShotEntities)
	{
		Linker->EntityManager.AddComponents(Entity, FinishedMask, EEntityRecursion::Full);
	}
	OneShotEntities.Empty();
}

void FEntityLedger::CleanupLinkerEntities(const TSet<FMovieSceneEntityID>& LinkerEntities)
{
	for (int32 Index = OneShotEntities.Num()-1; Index >= 0; --Index)
	{
		if (LinkerEntities.Contains(OneShotEntities[Index]))
		{
			OneShotEntities.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	for (auto It = ImportedEntities.CreateIterator(); It; ++It)
	{
		FMovieSceneEntityID EntityID = It.Value().EntityID;
		if (EntityID && LinkerEntities.Contains(EntityID))
		{
			It.RemoveCurrent();
		}
	}
}

void FEntityLedger::TagGarbage(UMovieSceneEntitySystemLinker* Linker)
{
	FComponentTypeID NeedsLink = FBuiltInComponentTypes::Get()->Tags.NeedsLink;
	FComponentTypeID NeedsUnlink = FBuiltInComponentTypes::Get()->Tags.NeedsUnlink;

	for (auto It = ImportedEntities.CreateIterator(); It; ++It)
	{
		if (!It.Key().EntityOwner.IsValid())
		{
			if (It.Value().EntityID)
			{
				Linker->EntityManager.RemoveComponent(It.Value().EntityID, NeedsLink, EEntityRecursion::Full);
				Linker->EntityManager.AddComponent(It.Value().EntityID, NeedsUnlink, EEntityRecursion::Full);
			}
			It.RemoveCurrent();
		}
	}
}

bool FEntityLedger::Contains(UMovieSceneEntitySystemLinker* Linker, const FEntityComponentFilter& Filter) const
{
	bool bResult = false;

	auto Visit = [&Filter, &bResult, Linker](FMovieSceneEntityID EntityID)
	{
		bResult = Filter.Match(Linker->EntityManager.GetEntityType(EntityID));
	};

	for (FMovieSceneEntityID EntityID : OneShotEntities)
	{
		Visit(EntityID);
		Linker->EntityManager.IterateChildren_ParentFirst(EntityID, Visit);

		if (bResult)
		{
			return true;
		}
	}

	for (const TPair<FMovieSceneEvaluationFieldEntityKey, FImportedEntityData>& Pair : ImportedEntities)
	{
		Visit(Pair.Value.EntityID);
		Linker->EntityManager.IterateChildren_ParentFirst(Pair.Value.EntityID, Visit);

		if (bResult)
		{
			return true;
		}
	}

	return bResult;
}

void FEntityLedger::MutateAll(UMovieSceneEntitySystemLinker* Linker, const FEntityComponentFilter& Filter, const IMovieScenePerEntityMutation& Mutation) const
{
	auto Visit = [&Filter, &Mutation, Linker](FMovieSceneEntityID EntityID)
	{
		const FComponentMask& ExistingType = Linker->EntityManager.GetEntityType(EntityID);
		if (Filter.Match(ExistingType))
		{
			FComponentMask NewType = ExistingType;
			Mutation.CreateMutation(&Linker->EntityManager, &NewType);

			if (!NewType.CompareSetBits(ExistingType))
			{
				Linker->EntityManager.ChangeEntityType(EntityID, NewType);

				FEntityInfo EntityInfo = Linker->EntityManager.GetEntity(EntityID);
				Mutation.InitializeEntities(EntityInfo.Data.AsRange(), NewType);
			}
		}
	};

	for (FMovieSceneEntityID EntityID : OneShotEntities)
	{
		Visit(EntityID);
		Linker->EntityManager.IterateChildren_ParentFirst(EntityID, Visit);
	}

	for (const TPair<FMovieSceneEvaluationFieldEntityKey, FImportedEntityData>& Pair : ImportedEntities)
	{
		Visit(Pair.Value.EntityID);
		Linker->EntityManager.IterateChildren_ParentFirst(Pair.Value.EntityID, Visit);
	}
}

} // namespace MovieScene
} // namespace UE
