// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSequenceTransform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSequenceTransform)

FString LexToString(const FMovieSceneSequenceTransform& InTransform)
{
	TStringBuilder<256> Builder;

	Builder.Append(LexToString(InTransform.LinearTransform));

	int32 NestedIndex = 0;
	for (const FMovieSceneNestedSequenceTransform& Nested : InTransform.NestedTransforms)
	{
		const bool bHasLinear = !Nested.LinearTransform.IsIdentity();
		const bool bHasWarping = Nested.Warping.IsValid();

		if (bHasLinear || bHasWarping)
		{
			Builder.Appendf(TEXT(" [ %d = "), NestedIndex);
		}
		if (bHasLinear)
		{
			Builder.Append(LexToString(Nested.LinearTransform));
		}
		if (bHasWarping)
		{
			if (bHasLinear)
			{
				Builder.Append(TEXT(" | "));
			}
			Builder.Append(LexToString(Nested.Warping));
		}
		if (bHasLinear || bHasWarping)
		{
			Builder.Append(TEXT(" ]"));
		}

		++NestedIndex;
	}

	return Builder.ToString();
}

FString LexToString(const FMovieSceneWarpCounter& InCounter)
{
	if (InCounter.WarpCounts.Num() == 0)
	{
		return FString(TEXT("[]"));
	}

	TStringBuilder<256> Builder;

	Builder.Append(TEXT("["));
	int32 Index = 0;
	for (uint16 Loop : InCounter.WarpCounts)
	{
		if (Index > 0)
		{
			Builder.Append(TEXT(","));
		}
		Builder.Appendf(TEXT("%u"), (uint32)Loop);
		++Index;
	}
	Builder.Append(TEXT("]"));

	FString OutString = Builder.ToString();
	return OutString;
}


