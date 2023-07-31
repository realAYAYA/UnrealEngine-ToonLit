// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/SortedMap.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Templates/UniquePtr.h"

class IMovieScenePlayer;
class UMovieSceneCompiledDataManager;
struct FGuid;
struct FMovieSceneCompiledDataID;
struct FMovieSceneSequenceID;

namespace UE
{
namespace MovieScene
{

struct FCompiledDataVolatilityManager
{
	static TUniquePtr<FCompiledDataVolatilityManager> Construct(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager);

	bool ConditionalRecompile(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager);

private:

	bool HasBeenRecompiled(FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager) const;

	bool HasSequenceBeenRecompiled(FMovieSceneCompiledDataID DataID, FMovieSceneSequenceID SequenceID, UMovieSceneCompiledDataManager* CompiledDataManager) const;

	void UpdateCachedSignatures(IMovieScenePlayer& Player, FMovieSceneCompiledDataID RootDataID, UMovieSceneCompiledDataManager* CompiledDataManager);

private:

	TSortedMap<FMovieSceneSequenceID, FGuid, TInlineAllocator<16>> CachedCompilationSignatures;
};

} // namespace MovieScene
} // namespace UE