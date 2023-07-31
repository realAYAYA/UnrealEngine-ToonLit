// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"

struct ICompressedAnimData;

/* Encapsulates decompression related data used by bone compression codecs. */
struct FAnimSequenceDecompressionContext
{
	UE_DEPRECATED(5.1, "This constructor is deprecated. Use the other constructor by passing the ref pose and track to skeleton map to ensure safe usage with all codecs")
	FAnimSequenceDecompressionContext(float SequenceLength_, EAnimInterpolationType Interpolation_, const FName& AnimName_, const ICompressedAnimData& CompressedAnimData_)
			:
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SequenceLength(SequenceLength_),
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Interpolation(Interpolation_), AnimName(AnimName_), CompressedAnimData(CompressedAnimData_),
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Time(0.f), RelativePos(0.f)
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
	}

	FAnimSequenceDecompressionContext(float SequenceLength_, EAnimInterpolationType Interpolation_, const FName& AnimName_, const ICompressedAnimData& CompressedAnimData_,
		const TArray<FTransform>& RefPoses_, const TArray<FTrackToSkeletonMap>& TrackToSkeletonMap_)
		:
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SequenceLength(SequenceLength_),
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Interpolation(Interpolation_), AnimName(AnimName_), CompressedAnimData(CompressedAnimData_),
		RefPoses(RefPoses_.GetData(), RefPoses_.Num()), TrackToSkeletonMap(TrackToSkeletonMap_.GetData(), TrackToSkeletonMap_.Num()),
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Time(0.f), RelativePos(0.f)
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
	}

	// Anim info
	UE_DEPRECATED(5.1, "Direct access to SequenceLength has been deprecated use GetPlayableLength instead")
	float SequenceLength;
	EAnimInterpolationType Interpolation;
	FName AnimName;

	const ICompressedAnimData& CompressedAnimData;

private:
	// Immutable reference/bind pose, maps a bone index to its reference transform
	TArrayView<const FTransform> RefPoses;

	// Immutable array that maps from a track index to a bone index
	TArrayView<const FTrackToSkeletonMap> TrackToSkeletonMap;

public:
	UE_DEPRECATED(5.1, "Direct access to Time has been deprecated use GetEvaluationTime or GetInterpolatedEvaluationTime instead")
	float Time;
	
	UE_DEPRECATED(5.1, "Direct access to RelativePos has been deprecated use GetRelativePosition instead")
	float RelativePos;

	float GetPlayableLength() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return SequenceLength;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	float GetEvaluationTime() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Time;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	float GetRelativePosition() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RelativePos;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	const TArrayView<const FTransform>& GetRefLocalPoses() const
	{
		return RefPoses;
	}

	const TArrayView<const FTrackToSkeletonMap>& GetTrackToSkeletonMap() const
	{
		return TrackToSkeletonMap;
	}

	void Seek(float SampleAtTime)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Time = SampleAtTime;
		RelativePos = SequenceLength != 0.f ? SampleAtTime / SequenceLength : 0.f;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
};

