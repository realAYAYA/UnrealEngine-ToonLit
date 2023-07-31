// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntityLedger.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/BuiltInComponentTypes.h"

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

void FEntityLedger::UnlinkEverything(UMovieSceneEntitySystemLinker* Linker)
{
	FComponentMask FinishedMask = FBuiltInComponentTypes::Get()->FinishedMask;

	for (TPair<FMovieSceneEvaluationFieldEntityKey, FImportedEntityData>& Pair : ImportedEntities)
	{
		if (Pair.Value.EntityID)
		{
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
			OneShotEntities.RemoveAtSwap(Index, 1, false);
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
	FComponentTypeID NeedsUnlink = FBuiltInComponentTypes::Get()->Tags.NeedsUnlink;

	for (auto It = ImportedEntities.CreateIterator(); It; ++It)
	{
		if (It.Key().EntityOwner == nullptr)
		{
			if (It.Value().EntityID)
			{
				Linker->EntityManager.AddComponent(It.Value().EntityID, NeedsUnlink, EEntityRecursion::Full);
			}
			It.RemoveCurrent();
		}
	}
}

} // namespace MovieScene
} // namespace UE
