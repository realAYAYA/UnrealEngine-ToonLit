// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "MovieSceneSequenceTransform.h"
#include "MovieSceneTimeTransform.h"
#include "MovieSceneTimeWarping.h"

/** Entry struct for the TMovieSceneTimeArray class */
template<typename DataType>
struct TMovieSceneTimeArrayEntry
{
	FFrameTime RootTime;
	DataType Datum;
};

/**
 * A utility class that lets you store a collection of timestamped data originating from various time bases.
 *
 * All of the data is stored in "root" time space. That is: as you add timestamped data, these timestamps are
 * converted back to "root times" using the inverse of the current time transform. Pushing and popping time
 * transforms, and incrementing loop counts, makes it possible to change what's considered "local time", 
 * which affects this inverse transformation.
 */
template<typename DataType>
struct TMovieSceneTimeArray
{
	/** Starts a scope where local times are offset and scaled as specified */
	void PushTransform(FFrameNumber FrameOffset, float TimeScale = 1.f)
	{
		PushTransform(FMovieSceneTimeTransform(FrameOffset, TimeScale));
	}

	/** Starts a scope where local times are transformed as specified */
	void PushTransform(const FMovieSceneTimeTransform& InTransform)
	{
		FTransformStep Step;
		Step.LinearTransform = InTransform;
		TransformStack.Push(Step);

		UpdateCachedInverseTransform();
	}

	/** Starts a scope where local times are looped and offset as specified */
	void PushTransform(FFrameNumber FirstLoopStartFrameOffset, FFrameNumber LoopStart, FFrameNumber LoopEnd, float TimeScale = 1.f)
	{
		PushTransform(
				FMovieSceneTimeTransform(FirstLoopStartFrameOffset, TimeScale),
				FMovieSceneTimeWarping(LoopStart, LoopEnd));
	}

	/** Starts a scope where local times are transformed as specified */
	void PushTransform(const FMovieSceneTimeWarping& InWarping)
	{
		PushTransform(FMovieSceneTimeTransform(), InWarping);
	}

	/** Starts a scope where local times are transformed as specified */
	void PushTransform(const FMovieSceneTimeTransform& InTransform, const FMovieSceneTimeWarping& InWarping)
	{
		FTransformStep Step;
		Step.LinearTransform = InTransform;
		Step.Warping = InWarping;
		Step.WarpCounter = InWarping.IsValid() ? 0 : FMovieSceneTimeWarping::InvalidWarpCount;
		TransformStack.Push(Step);

		UpdateCachedInverseTransform();
	}

	/** Incremenets the warp counter for the local time space, i.e. all local times will be considered as being in a further loop */
	void IncrementWarpCounter()
	{
		if (ensure(TransformStack.Num() > 0))
		{
			FTransformStep& LastStep = TransformStack.Last();
			if (ensure(LastStep.Warping.IsValid() && LastStep.WarpCounter != FMovieSceneTimeWarping::InvalidWarpCount))
			{
				++LastStep.WarpCounter;
			}
		}

		UpdateCachedInverseTransform();
	}

	/** Removes the last pushed local time transform */
	void PopTransform()
	{
		TransformStack.Pop();

		UpdateCachedInverseTransform();
	}

	/** Adds a new piece of data, timestamped to the given local time */
	void Add(FFrameNumber LocalTime, const DataType& Datum)
	{
		const FFrameTime RootTime = LocalTime * CachedInverseTransform;

		// Insert the new entry by keeping the array sorted by time.
		int32 NextEntry = Entries.IndexOfByPredicate([RootTime](const FEntry& Entry) { return RootTime < Entry.RootTime; });
		if (NextEntry != INDEX_NONE)
		{
			Entries.Insert(FEntry{ RootTime, Datum }, NextEntry);
		}
		else
		{
			Entries.Add(FEntry{ RootTime, Datum });
		}
	}

	/** Clears all entries */
	void Clear()
	{
		Entries.Clear();
	}

	/** Gets the current list of entries in the array */
	TArrayView<const TMovieSceneTimeArrayEntry<DataType>> GetEntries() const
	{
		return TArrayView<const FEntry>(Entries);
	}

private:

	void UpdateCachedInverseTransform()
	{
		// Build the total transform from root to local, and keep track of the loop indices.
		FMovieSceneWarpCounter LoopCounter;
		FMovieSceneSequenceTransform RootToLocalTransform;

		// TODO: It seems a bit silly to store this as a TransformStack when we need to do the same work to inverse it.

		for (int32 Index = 0; Index < TransformStack.Num(); ++Index)
		{
			RootToLocalTransform.NestedTransforms.Add(FMovieSceneNestedSequenceTransform(TransformStack[Index].LinearTransform, TransformStack[Index].Warping));
			if (TransformStack[Index].Warping.IsValid())
			{
				LoopCounter.AddWarpingLevel(TransformStack[Index].WarpCounter);
			}
			else
			{
				LoopCounter.AddNonWarpingLevel();
			}
		}

		CachedInverseTransform = RootToLocalTransform.InverseFromLoop(LoopCounter);
	}

private:

	using FEntry = TMovieSceneTimeArrayEntry<DataType>;

	TArray<FEntry> Entries;

	struct FTransformStep
	{
		FMovieSceneTimeTransform LinearTransform;
		FMovieSceneTimeWarping Warping;
		uint32 WarpCounter = FMovieSceneTimeWarping::InvalidWarpCount;
	};
	TArray<FTransformStep> TransformStack;

	FMovieSceneSequenceTransform CachedInverseTransform;
};

template<typename DataType>
struct TMovieSceneTimeArrayTransformScope
{
	TMovieSceneTimeArrayTransformScope(TMovieSceneTimeArray<DataType>& InArray, FFrameNumber Offset, float TimeScale = 1.f)
		: Array(&InArray)
	{
		Array->PushTransform(Offset, TimeScale);
	}

	TMovieSceneTimeArrayTransformScope(TMovieSceneTimeArray<DataType>& InArray, FMovieSceneTimeTransform&& InTransform)
		: Array(&InArray)
	{
		Array->PushTransform(InTransform);
	}

	TMovieSceneTimeArrayTransformScope(TMovieSceneTimeArray<DataType>& InArray, FFrameNumber FirstLoopStartFrameOffset, FFrameNumber LoopStart, FFrameNumber LoopEnd, float TimeScale = 1.f)
		: Array(&InArray)
	{
		Array->PushTransform(FirstLoopStartFrameOffset, LoopStart, LoopEnd, TimeScale);
	}

	TMovieSceneTimeArrayTransformScope(TMovieSceneTimeArray<DataType>& InArray, FMovieSceneTimeTransform&& InTransform, FMovieSceneTimeWarping&& InWarping)
		: Array(&InArray)
	{
		Array->PushTransform(InTransform, InWarping);
	}

	TMovieSceneTimeArrayTransformScope(TMovieSceneTimeArrayTransformScope&& Other)
		: Array(Other.Array)
	{
		Other.Array = nullptr;
	}

	~TMovieSceneTimeArrayTransformScope()
	{
		if (Array)
		{
			Array->PopTransform();
		}
	}

private:
	TMovieSceneTimeArray<DataType>* Array = nullptr;
};

