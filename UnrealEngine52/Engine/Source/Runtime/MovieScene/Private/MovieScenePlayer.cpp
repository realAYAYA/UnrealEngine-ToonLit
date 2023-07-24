// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "Misc/ScopeRWLock.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"

namespace UE
{
namespace MovieScene
{

static FRWLock                          GGlobalPlayerRegistryLock;
static TSparseArray<IMovieScenePlayer*> GGlobalPlayerRegistry;

} // namespace MovieScene
} // namespace UE

IMovieScenePlayer::IMovieScenePlayer()
{
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	UniqueIndex = UE::MovieScene::GGlobalPlayerRegistry.Add(this);
}

IMovieScenePlayer::~IMovieScenePlayer()
{
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	UE::MovieScene::GGlobalPlayerRegistry.RemoveAt(UniqueIndex, 1);
}

IMovieScenePlayer* IMovieScenePlayer::Get(uint16 InUniqueIndex)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	check(UE::MovieScene::GGlobalPlayerRegistry.IsValidIndex(InUniqueIndex));
	return UE::MovieScene::GGlobalPlayerRegistry[InUniqueIndex];
}

void IMovieScenePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	Sequence.LocateBoundObjects(InBindingId, ResolutionContext, OutObjects);
}

void IMovieScenePlayer::InvalidateCachedData()
{
	FMovieSceneRootEvaluationTemplateInstance& Template = GetEvaluationTemplate();

	UE::MovieScene::FSequenceInstance* RootInstance = Template.FindInstance(MovieSceneSequenceID::Root);
	if (RootInstance)
	{
		RootInstance->InvalidateCachedData(Template.GetEntitySystemLinker());
	}
}