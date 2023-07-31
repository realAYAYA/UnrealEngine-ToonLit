// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTree.h"
#include "MovieSceneSection.h"

bool FMovieSceneAdditiveCameraTrackBlender::SortByStartTime(const FMovieSceneSectionData& A, const FMovieSceneSectionData& B)
{
	return TRangeBound<FFrameNumber>::MinLower(A.Section->GetRange().GetLowerBound(), B.Section->GetRange().GetLowerBound()) == A.Section->GetRange().GetLowerBound();
}
