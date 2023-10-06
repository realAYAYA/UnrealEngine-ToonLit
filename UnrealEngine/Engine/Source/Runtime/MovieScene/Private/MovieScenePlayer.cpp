// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "Misc/ScopeRWLock.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"

namespace UE
{
namespace MovieScene
{

static FRWLock                          GGlobalPlayerRegistryLock;
static TSparseArray<IMovieScenePlayer*> GGlobalPlayerRegistry;
static TBitArray<> GGlobalPlayerUpdateFlags;

} // namespace MovieScene
} // namespace UE

IMovieScenePlayer::IMovieScenePlayer()
{
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerRegistry.Shrink();
	UniqueIndex = UE::MovieScene::GGlobalPlayerRegistry.Add(this);

	UE::MovieScene::GGlobalPlayerUpdateFlags.PadToNum(UniqueIndex + 1, false);
	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
}

IMovieScenePlayer::~IMovieScenePlayer()
{	
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
	UE::MovieScene::GGlobalPlayerRegistry.RemoveAt(UniqueIndex, 1);
}

IMovieScenePlayer* IMovieScenePlayer::Get(uint16 InUniqueIndex)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	check(UE::MovieScene::GGlobalPlayerRegistry.IsValidIndex(InUniqueIndex));
	return UE::MovieScene::GGlobalPlayerRegistry[InUniqueIndex];
}

void IMovieScenePlayer::Get(TArray<IMovieScenePlayer*>& OutPlayers, bool bOnlyUnstoppedPlayers)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	for (auto It = UE::MovieScene::GGlobalPlayerRegistry.CreateIterator(); It; ++It)
	{
		if (IMovieScenePlayer* Player = *It)
		{
			if (!bOnlyUnstoppedPlayers || Player->GetPlaybackStatus() != EMovieScenePlayerStatus::Stopped)
			{
				OutPlayers.Add(*It);
			}
		}
	}
}

void IMovieScenePlayer::SetIsEvaluatingFlag(uint16 InUniqueIndex, bool bIsUpdating)
{
	check(UE::MovieScene::GGlobalPlayerUpdateFlags.IsValidIndex(InUniqueIndex));
	UE::MovieScene::GGlobalPlayerUpdateFlags[InUniqueIndex] = bIsUpdating;
}

bool IMovieScenePlayer::IsEvaluating() const
{
	return UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex];
}

void IMovieScenePlayer::PopulateUpdateFlags(UE::MovieScene::ESequenceInstanceUpdateFlags& OutFlags)
{
	using namespace UE::MovieScene;

	OutFlags |= ESequenceInstanceUpdateFlags::NeedsPreEvaluation | ESequenceInstanceUpdateFlags::NeedsPostEvaluation;
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
