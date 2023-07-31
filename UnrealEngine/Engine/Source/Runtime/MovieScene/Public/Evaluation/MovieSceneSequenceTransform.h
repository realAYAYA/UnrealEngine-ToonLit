// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/AllOf.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Math/Range.h"
#include "Math/RangeBound.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "MovieSceneTimeTransform.h"
#include "MovieSceneTimeWarping.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneSequenceTransform.generated.h"


/**
 * Converts a range from one type of bounds to another. The output bounds type must be implicitely 
 * constructable from the input bounds type.
 */
template<typename InBoundType, typename OutBoundType>
inline TRange<OutBoundType> ConvertRange(const TRange<InBoundType>& Range)
{
	const TRangeBound<InBoundType> SourceLower = Range.GetLowerBound();
	TRangeBound<OutBoundType> DestLower = SourceLower.IsOpen() ?
		TRangeBound<OutBoundType>() :
		SourceLower.IsInclusive() ?
			TRangeBound<OutBoundType>::Inclusive(SourceLower.GetValue()) :
			TRangeBound<OutBoundType>::Exclusive(SourceLower.GetValue());

	const TRangeBound<InBoundType> SourceUpper = Range.GetUpperBound();
	TRangeBound<OutBoundType> DestUpper = SourceUpper.IsOpen() ?
		TRangeBound<OutBoundType>() :
		SourceUpper.IsInclusive() ?
			TRangeBound<OutBoundType>::Inclusive(SourceUpper.GetValue()) :
			TRangeBound<OutBoundType>::Exclusive(SourceUpper.GetValue());

	return TRange<OutBoundType>(DestLower, DestUpper);
}

// Specialization of ConvertRange for round down FFrameTime to FFrameNumber.
template<>
inline TRange<FFrameNumber> ConvertRange(const TRange<FFrameTime>& Range)
{
	const TRangeBound<FFrameTime> SourceLower = Range.GetLowerBound();
	TRangeBound<FFrameNumber> DestLower = SourceLower.IsOpen() ?
		TRangeBound<FFrameNumber>() :
		SourceLower.IsInclusive() ?
			TRangeBound<FFrameNumber>::Inclusive(SourceLower.GetValue().FloorToFrame()) :
			TRangeBound<FFrameNumber>::Exclusive(SourceLower.GetValue().FloorToFrame());

	const TRangeBound<FFrameTime> SourceUpper = Range.GetUpperBound();
	TRangeBound<FFrameNumber> DestUpper = SourceUpper.IsOpen() ?
		TRangeBound<FFrameNumber>() :
		SourceUpper.IsInclusive() ?
			TRangeBound<FFrameNumber>::Inclusive(SourceUpper.GetValue().FloorToFrame()) :
			TRangeBound<FFrameNumber>::Exclusive(SourceUpper.GetValue().FloorToFrame());

	return TRange<FFrameNumber>(DestLower, DestUpper);
}

/**
 * Struct that tracks warp counts in a way that works with the FMovieSceneTimeWarping struct.
 */
USTRUCT()
struct FMovieSceneWarpCounter
{
	GENERATED_BODY()

	void AddWarpingLevel(uint32 WarpCount)
	{
		if (WarpCount != FMovieSceneTimeWarping::InvalidWarpCount)
		{
			WarpCounts.Add(WarpCount);
		}
		else if (WarpCounts.Num() > 0)
		{
			if (WarpCounts.Last() != FMovieSceneTimeWarping::InvalidWarpCount)
			{
				WarpCounts.Add(FMovieSceneTimeWarping::InvalidWarpCount);
			}
		}
		// else: ignore, non-warping sub-sequences have their transforms compressed into a single
		//       linear transform until the point we meet the first looping sub-sequence.
	}

	void AddNonWarpingLevel()
	{
		AddWarpingLevel(FMovieSceneTimeWarping::InvalidWarpCount);
	}

	int32 NumWarpCounts() const
	{
		return WarpCounts.Num();
	}

	uint32 LastWarpCount() const
	{
		return WarpCounts.Num() > 0 ? WarpCounts[WarpCounts.Num() - 1] : FMovieSceneTimeWarping::InvalidWarpCount;
	}

	friend bool operator==(const FMovieSceneWarpCounter& A, const FMovieSceneWarpCounter& B)
	{
		return A.WarpCounts == B.WarpCounts;
	}

	UPROPERTY()
	TArray<uint32> WarpCounts;
};

/**
 * Time transform information for a nested sequence.
 */
USTRUCT()
struct FMovieSceneNestedSequenceTransform
{
	GENERATED_BODY()

	FMovieSceneNestedSequenceTransform()
	{}

	FMovieSceneNestedSequenceTransform(FMovieSceneTimeTransform InLinearTransform)
		: LinearTransform(InLinearTransform)
	{}

	FMovieSceneNestedSequenceTransform(FMovieSceneTimeWarping InWarping)
		: Warping(InWarping)
	{}

	FMovieSceneNestedSequenceTransform(FMovieSceneTimeTransform InLinearTransform, FMovieSceneTimeWarping InWarping)
		: LinearTransform(InLinearTransform)
		, Warping(InWarping)
	{}

	/**
	 * Returns whether this transform is identity.
	 */
	bool IsIdentity() const { return LinearTransform.IsIdentity() && !IsWarping(); }

	/**
	 * Returns whether this transform is warping.
	 */
	bool IsWarping() const { return Warping.IsValid(); }

	/**
	 * Linear time transform for this sub-sequence.
	 */
	UPROPERTY()
	FMovieSceneTimeTransform LinearTransform;

	/**
	 * Time warping information for this sub-sequence.
	 */
	UPROPERTY()
	FMovieSceneTimeWarping Warping;

	friend bool operator==(const FMovieSceneNestedSequenceTransform& A, const FMovieSceneNestedSequenceTransform& B)
	{
		return A.LinearTransform == B.LinearTransform && A.Warping == B.Warping;
	}

	friend bool operator!=(const FMovieSceneNestedSequenceTransform& A, const FMovieSceneNestedSequenceTransform& B)
	{
		return A.LinearTransform != B.LinearTransform || A.Warping != B.Warping;
	}

	FMovieSceneTimeTransform InverseLinearOnly() const
	{
		return LinearTransform.Inverse();
	}

	FMovieSceneTimeTransform InverseFromWarp(uint32 WarpCount) const
	{
		return LinearTransform.Inverse() * Warping.InverseFromWarp(WarpCount);
	}
};

/**
 * Movie scene sequence transform class that transforms from one time-space to another.
 */
USTRUCT()
struct FMovieSceneSequenceTransform
{
	GENERATED_BODY()

	/**
	 * Default construction to the identity transform
	 */
	FMovieSceneSequenceTransform()
	{}

	/**
	 * Construction from an offset, and a scale
	 *
	 * @param InOffset 			The offset to translate by
	 * @param InTimeScale 		The timescale. For instance, if a sequence is playing twice as fast, pass 2.f
	 */
	explicit FMovieSceneSequenceTransform(FFrameTime InOffset, float InTimeScale = 1.f)
		: LinearTransform(InOffset, InTimeScale)
	{}

	/**
	 * Construction from a linear time transform.
	 *
	 * @param InLinearTransform	The linear transform
	 */
	FMovieSceneSequenceTransform(FMovieSceneTimeTransform InLinearTransform)
		: LinearTransform(InLinearTransform)
	{}

	friend bool operator==(const FMovieSceneSequenceTransform& A, const FMovieSceneSequenceTransform& B)
	{
		return A.LinearTransform == B.LinearTransform && A.NestedTransforms == B.NestedTransforms;
	}

	friend bool operator!=(const FMovieSceneSequenceTransform& A, const FMovieSceneSequenceTransform& B)
	{
		return A.LinearTransform != B.LinearTransform || A.NestedTransforms != B.NestedTransforms;
	}

	/**
	 * Returns whether this sequence transform is an identity transform (i.e. it doesn't change anything).
	 */
	bool IsIdentity() const
	{
		return LinearTransform.IsIdentity() && 
			(NestedTransforms.Num() == 0 || 
			 Algo::AllOf(NestedTransforms,
				 [](const FMovieSceneNestedSequenceTransform& A) -> bool
				 {
					return A.IsIdentity();
				 }));
	}

	/**
	 * Returns whether this sequence transform includes any time warping.
	 */
	bool IsWarping() const
	{
		// Since we compress any linear transforms, we know that there will be warping as soon as we
		// use the nested transform array.
		return NestedTransforms.Num() > 0;
	}

	/**
	 * Returns whether this sequence transform is purely linear (i.e. doesn't involve time warping).
	 */
	bool IsLinear() const
	{
		return NestedTransforms.Num() == 0;
	}

	/**
	 * Returns the total time-scale of this transform.
	 */
	float GetTimeScale() const
	{
		float TimeScale = LinearTransform.TimeScale;
		for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
		{
			if (NestedTransform.LinearTransform.TimeScale != 1.f)
			{
				TimeScale *= NestedTransform.LinearTransform.TimeScale;
			}
		}
		return TimeScale;
	}

	/**
	 * Transforms the given time, returning the transformed time.
	 */
	FFrameTime TransformTime(FFrameTime InTime) const
	{
		FFrameTime OutTime;
		FMovieSceneWarpCounter WarpCounter;
		TransformTime(InTime, OutTime, WarpCounter);
		return OutTime;
	}

	/**
	 * Transforms the given time, returning the transformed time along with a series of loop indices that
	 * indicate which loops each nested level is in.
	 */
	void TransformTime(FFrameTime InTime, FFrameTime& OutTime, FMovieSceneWarpCounter& OutWarpCounter) const
	{
		OutWarpCounter.WarpCounts.Reset();

		OutTime = InTime * LinearTransform;
		
		if (NestedTransforms.Num() > 0)
		{
			for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
			{
				OutTime = OutTime * NestedTransform.LinearTransform;

				if (NestedTransform.IsWarping())
				{
					uint32 WarpIndex;
					NestedTransform.Warping.TransformTime(OutTime, OutTime, WarpIndex);
					OutWarpCounter.WarpCounts.Add(WarpIndex);
				}
				else
				{
					OutWarpCounter.WarpCounts.Add(FMovieSceneTimeWarping::InvalidWarpCount);
				}
			}
		}
	}

	/**
	 * Transforms the given range using the "pure" warping transformation.
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameTime> TransformRangePure(const TRange<FFrameTime>& Range) const
	{
		TRange<FFrameTime> Result = Range * LinearTransform;
		for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
		{
			Result = Result * NestedTransform.LinearTransform;
			if (NestedTransform.IsWarping())
			{
				Result = NestedTransform.Warping.TransformRangePure(Result);
			}
		}
		return Result;
	}

	/**
	 * Transforms the given range using the "unwarped" warping transformation. 
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameTime> TransformRangeUnwarped(const TRange<FFrameTime>& Range) const
	{
		TRange<FFrameTime> Result = Range * LinearTransform;
		for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
		{
			Result = Result * NestedTransform.LinearTransform;
			if (NestedTransform.IsWarping())
			{
				Result = NestedTransform.Warping.TransformRangeUnwarped(Result);
			}
		}
		return Result;
	}

	/**
	 * Transforms the given range using the "constrained" warping transformation. 
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameTime> TransformRangeConstrained(const TRange<FFrameTime>& Range) const
	{
		TRange<FFrameTime> Result = Range * LinearTransform;
		for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
		{
			Result = Result * NestedTransform.LinearTransform;
			if (NestedTransform.IsWarping())
			{
				Result = NestedTransform.Warping.TransformRangeConstrained(Result);
			}
		}
		return Result;
	}

	/**
	 * Transforms the given range using the "pure" warping transformation.
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameNumber> TransformRangePure(const TRange<FFrameNumber>& Range) const
	{
		TRange<FFrameTime> TimeRange = ConvertRange<FFrameNumber, FFrameTime>(Range);
		TimeRange = TransformRangePure(TimeRange);
		return ConvertRange<FFrameTime, FFrameNumber>(TimeRange);
	}

	/**
	 * Transforms the given range using the "unwarped" warping transformation. 
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameNumber> TransformRangeUnwarped(const TRange<FFrameNumber>& Range) const
	{
		TRange<FFrameTime> TimeRange = ConvertRange<FFrameNumber, FFrameTime>(Range);
		TimeRange = TransformRangeUnwarped(TimeRange);
		return ConvertRange<FFrameTime, FFrameNumber>(TimeRange);
	}

	/**
	 * Transforms the given range using the "constrained" warping transformation. 
	 * See FMovieSceneTimeWarping for more information.
	 */
	TRange<FFrameNumber> TransformRangeConstrained(const TRange<FFrameNumber>& Range) const
	{
		TRange<FFrameTime> TimeRange = ConvertRange<FFrameNumber, FFrameTime>(Range);
		TimeRange = TransformRangeConstrained(TimeRange);
		return ConvertRange<FFrameTime, FFrameNumber>(TimeRange);
	}

	/**
	 * Retrieve the inverse of the linear part of this transform.
	 */
	FMovieSceneTimeTransform InverseLinearOnly() const
	{
		return LinearTransform.Inverse();
	}

	/**
	 * Retrieve the inverse of this transform assuming the local times would all belong to
	 * the first warp inside each nested time range.
	 */
	FMovieSceneTimeTransform InverseFromAllFirstWarps() const
	{
		const size_t NestedTransformsSize = NestedTransforms.Num();
		if (NestedTransformsSize == 0)
		{
			return LinearTransform.Inverse();
		}
		else
		{
			TArray<uint32> WarpCounts;
			WarpCounts.Reserve(NestedTransforms.Num());
			for (const FMovieSceneNestedSequenceTransform& NestedTransform : NestedTransforms)
			{
				WarpCounts.Add(NestedTransform.IsWarping() ? 0 : FMovieSceneTimeWarping::InvalidWarpCount);
			}
			return InverseFromWarp(WarpCounts);
		}
	}

	/**
	 * Retrieve the inverse of this transform assuming the local times would belong to the
	 * n'th warp inside the time range. As such, this method requires that the warp counts
	 * in the counter object is equal to the number of nested loops in this transform.
	 */
	FMovieSceneTimeTransform InverseFromWarp(const FMovieSceneWarpCounter& WarpCounter) const
	{
		return InverseFromWarp(WarpCounter.WarpCounts);
	}

	/**
	 * Retrieve the inverse of this transform assuming the local times would belong to the
	 * n'th warp inside the time range. As such, this method takes as parameter an array with
	 * as many items as there currently have nested loops in this transform.
	 */
	FMovieSceneTimeTransform InverseFromWarp(const TArrayView<const uint32>& WarpCounts) const
	{
		const size_t NestedTransformsSize = NestedTransforms.Num();
		check(WarpCounts.Num() == NestedTransformsSize);

		if (NestedTransformsSize > 0)
		{
			// Start accumulating the inverse transforms in reverse order.
			const FMovieSceneNestedSequenceTransform& LastNesting = NestedTransforms.Last();
			FMovieSceneTimeTransform Result = LastNesting.InverseFromWarp(WarpCounts.Last());

			size_t Index = NestedTransformsSize - 1;
			while (Index > 0)
			{
				--Index;
				
				const uint32 CurWarpCount = WarpCounts[Index];
				const FMovieSceneNestedSequenceTransform& CurNesting = NestedTransforms[Index];
				Result = CurNesting.InverseFromWarp(CurWarpCount) * Result;
			}

			// Add the inverse of the main linear transform.
			Result = LinearTransform.Inverse() * Result;

			return Result;
		}
		else
		{
			return FMovieSceneTimeTransform(LinearTransform.Inverse());
		}
	}

	UPROPERTY()
	FMovieSceneTimeTransform LinearTransform;

	UPROPERTY()
	TArray<FMovieSceneNestedSequenceTransform> NestedTransforms;
};

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline FFrameTime operator*(FFrameTime InTime, const FMovieSceneSequenceTransform& RHS)
{
	const uint32 NestedTransformsSize = RHS.NestedTransforms.Num();
	if (NestedTransformsSize == 0)
	{
		return InTime * RHS.LinearTransform;
	}
	else
	{
		FFrameTime OutTime = InTime * RHS.LinearTransform;
		for (const FMovieSceneNestedSequenceTransform& NestedTransform : RHS.NestedTransforms)
		{
			if (NestedTransform.IsWarping())
			{
				OutTime = OutTime * NestedTransform.LinearTransform * NestedTransform.Warping;
			}
			else
			{
				OutTime = OutTime * NestedTransform.LinearTransform;
			}
		}
		return OutTime;
	}
}

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline FFrameTime& operator*=(FFrameTime& InTime, const FMovieSceneSequenceTransform& RHS)
{
	InTime = InTime * RHS;
	return InTime;
}

/**
 * Transform a time range by a sequence transform
 *
 * @param InTime 			The time range to transform
 * @param RHS 				The transform
 */
template<typename T>
TRange<T>& operator*=(TRange<T>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	LHS = LHS * RHS;
	return LHS;
}

/**
 * Multiply 2 transforms together, resulting in a single transform that gets from RHS parent to LHS space
 * @note Transforms apply from right to left
 */
inline FMovieSceneSequenceTransform operator*(const FMovieSceneSequenceTransform& LHS, const FMovieSceneSequenceTransform& RHS)
{
	if (!LHS.IsWarping())
	{
		if (!RHS.IsWarping())
		{
			// None of the transforms are warping... we can combine them into another linear transform.
			return FMovieSceneSequenceTransform(LHS.LinearTransform * RHS.LinearTransform);
		}
		else
		{
			// LHS is linear, but RHS is warping. Since transforms are supposed to apply from right to left,
			// we need to append LHS at the "bottom" of RHS, i.e. add a new nested transform that's LHS. However
			// if LHS is identity, we have nothing to do, and if both LHS and RHS' deeper transform are linear,
			// we can combine both.
			FMovieSceneSequenceTransform Result(RHS);
			if (!LHS.LinearTransform.IsIdentity())
			{
				const int32 NumNestedTransforms = RHS.NestedTransforms.Num();
				check(NumNestedTransforms > 0);
				const FMovieSceneNestedSequenceTransform& LastNested = RHS.NestedTransforms[NumNestedTransforms - 1];
				if (LastNested.IsWarping())
				{
					Result.NestedTransforms.Add(FMovieSceneNestedSequenceTransform(LHS.LinearTransform));
				}
				else
				{
					Result.NestedTransforms[NumNestedTransforms - 1] = LHS.LinearTransform * LastNested.LinearTransform;
				}
			}
			return Result;
		}
	}
	else // LHS is warping
	{
		if (!RHS.IsWarping())
		{
			// RHS isn't warping, but LHS is, so we combine the linear transform parts and start looping
			// from there.
			FMovieSceneSequenceTransform Result;
			Result.LinearTransform = LHS.LinearTransform * RHS.LinearTransform;
			Result.NestedTransforms = LHS.NestedTransforms;
			return Result;
		}
		else
		{
			// Both are looping, we need to combine them. Usually, a warping transform doesn't use its linear part,
			// because whatever linear placement/scaling it has would be in the linear part of the nested transform
			// struct.
			FMovieSceneSequenceTransform Result(RHS);
			const bool bHasOnlyNested = LHS.LinearTransform.IsIdentity();
			if (!bHasOnlyNested)
			{
				Result.NestedTransforms.Add(FMovieSceneNestedSequenceTransform(LHS.LinearTransform));
			}
			Result.NestedTransforms.Append(LHS.NestedTransforms);
			return Result;
		}
	}
}

/** Convert a FMovieSceneSequenceTransform into a string */
FString LexToString(const FMovieSceneSequenceTransform& InTransform);

/** Convert a FMovieSceneWarpCounter into a string */
FString LexToString(const FMovieSceneWarpCounter& InCounter);

