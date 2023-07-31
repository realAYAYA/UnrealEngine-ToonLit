// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneCompiledVolatilityManager.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "MovieSceneFwd.h"

namespace UE
{
namespace MovieScene
{

#if WITH_EDITOR

int32 GVolatileSequencesInEditor = 1;
FAutoConsoleVariableRef CVarVolatileSequencesInEditor(
	TEXT("Sequencer.VolatileSequencesInEditor"),
	GVolatileSequencesInEditor,
	TEXT("(Default: 1) When non-zero, all assets will be treated as volatile in editor. Can be disabled to bypass volatility checks in-editor for more representative runtime performance metrics.\n"),
	ECVF_Default
);

#endif


FORCEINLINE EMovieSceneSequenceFlags GetEditorVolatilityFlags()
{
#if WITH_EDITOR
	return GVolatileSequencesInEditor ? EMovieSceneSequenceFlags::Volatile : EMovieSceneSequenceFlags::None;
#else
	return EMovieSceneSequenceFlags::None;
#endif
}


TUniquePtr<FCompiledDataVolatilityManager> FCompiledDataVolatilityManager::Construct(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager)
{
	const FMovieSceneCompiledDataEntry& Entry = CompiledDataManager->GetEntryRef(RootDataID);
	EMovieSceneSequenceFlags SequenceFlags = Entry.AccumulatedFlags | GetEditorVolatilityFlags();
	if (!EnumHasAnyFlags(SequenceFlags, EMovieSceneSequenceFlags::Volatile))
	{
		// If the entry has a valid compiled signature, it is assumed to be non-volatile if it does
		// not explicitly have the volatile flag. Otherwise we assume this sequence was added to the
		// manager but never compiled, and as such must be volatile since it needs compiling.
		if (Entry.CompiledSignature.IsValid())
		{
			return nullptr;
		}
	}

	TUniquePtr<FCompiledDataVolatilityManager> VolatilityManager = MakeUnique<FCompiledDataVolatilityManager>();
	VolatilityManager->ConditionalRecompile(Player, RootDataID, CompiledDataManager);
	return VolatilityManager;
}

bool FCompiledDataVolatilityManager::HasBeenRecompiled(FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager) const
{
	if (HasSequenceBeenRecompiled(RootDataID, MovieSceneSequenceID::Root, CompiledDataManager))
	{
		return true;
	}

	if (const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(RootDataID))
	{
		for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& Pair : Hierarchy->AllSubSequenceData())
		{
			FMovieSceneCompiledDataID SubDataID = CompiledDataManager->GetSubDataID(RootDataID, Pair.Key);
			if (SubDataID.IsValid() && HasSequenceBeenRecompiled(SubDataID, Pair.Key, CompiledDataManager))
			{
				return true;
			}
		}
	}

	return false;
}

bool FCompiledDataVolatilityManager::HasSequenceBeenRecompiled(FMovieSceneCompiledDataID DataID, FMovieSceneSequenceID SequenceID, UMovieSceneCompiledDataManager* CompiledDataManager) const
{
	const FGuid* CachedSignature = CachedCompilationSignatures.Find(SequenceID);

	const FMovieSceneCompiledDataEntry& CompiledEntry = CompiledDataManager->GetEntryRef(DataID);
	return CachedSignature == nullptr || *CachedSignature != CompiledEntry.CompiledSignature;
}

bool FCompiledDataVolatilityManager::ConditionalRecompile(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager)
{
	bool bRecompiled = false;

	if (CompiledDataManager->IsDirty(RootDataID))
	{
		CompiledDataManager->Compile(RootDataID);
		bRecompiled = true;
	}
	else
	{
		bRecompiled = HasBeenRecompiled(RootDataID, CompiledDataManager);
	}

	if (bRecompiled)
	{
		UpdateCachedSignatures(Player, RootDataID, CompiledDataManager);
	}

	return bRecompiled;
}

void FCompiledDataVolatilityManager::UpdateCachedSignatures(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager)
{
	CachedCompilationSignatures.Reset();

	{
		const FMovieSceneCompiledDataEntry& RootEntry = CompiledDataManager->GetEntryRef(RootDataID);
		CachedCompilationSignatures.Add(MovieSceneSequenceID::Root, RootEntry.CompiledSignature);

		UMovieSceneSequence* RootSequence = RootEntry.GetSequence();
		if (RootSequence)
		{
			Player.State.AssignSequence(MovieSceneSequenceID::Root, *RootSequence, Player);
		}
	}

	if (const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(RootDataID))
	{
		for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& SubData : Hierarchy->AllSubSequenceData())
		{
			const FMovieSceneCompiledDataID     SubDataID = CompiledDataManager->GetSubDataID(RootDataID, SubData.Key);
			if (!SubDataID.IsValid())
			{
				UE_LOG(LogMovieScene, Error, TEXT("Invalid SubDataID for: %s"), *SubData.Value.Sequence.ToString());
				continue;
			}

			const FMovieSceneCompiledDataEntry& SubEntry  = CompiledDataManager->GetEntryRef(SubDataID);

			CachedCompilationSignatures.Add(SubData.Key, SubEntry.CompiledSignature);

			UMovieSceneSequence* Sequence = SubData.Value.GetSequence();
			if (Sequence)
			{
				Player.State.AssignSequence(SubData.Key, *Sequence, Player);
			}
		}
	}
}


} // namespace MovieScene
} // namespace UE
