// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "MotionTrajectoryTypes.generated.h"

// Enumeration for signaling which "Accumulated" domain values to respect when determining past and future sampling horizons
UENUM(BlueprintType, Category="Motion Trajectory", meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ETrajectorySampleDomain : uint8
{
	None = 0,
	Time = 1 << 0, // Seconds
	Distance = 1 << 1 // Centimeters (Unreal units)
};
ENUM_CLASS_FLAGS(ETrajectorySampleDomain);

// A motion trajectory sample associated with a specific time or distance domain value
USTRUCT(BlueprintType, Category="Motion Trajectory")
struct ENGINE_API FTrajectorySample
{
	GENERATED_BODY()

	// The relative accumulated time that this sample is associated with
	// Zero value for instantaneous, negative values for the past, and positive values for the future
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	float AccumulatedSeconds = 0.f;

	// The relative accumulated distance that this trajectory sample is associated with
	// Zero value for instantaneous, negative values for the past, and positive values for the future
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	float AccumulatedDistance = 0.f;

	// Position relative to the sampled in-motion object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	FTransform Transform = FTransform::Identity;

	// Linear velocity relative to the sampled in-motion object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	FVector LinearVelocity = FVector::ZeroVector;

	// Linear interpolation of all parameters of two trajectory samples
	FTrajectorySample Lerp(const FTrajectorySample& Sample, float Alpha) const;

	// Centripetal Catmull–Rom spline interpolation of all parameters of two trajectory samples
	FTrajectorySample SmoothInterp(const FTrajectorySample& PrevSample
		, const FTrajectorySample& Sample
		, const FTrajectorySample& NextSample
		, float Alpha) const;

	// Concatenates DeltaTransform before the current transform is applied and shifts the accumulated time by 
	// DeltaSeconds
	void PrependOffset(const FTransform DeltaTransform, float DeltaSeconds);

	void TransformReferenceFrame(const FTransform DeltaTransform);

	// Determines if all sample properties are zeroed
	bool IsZeroSample() const;
};

// A container of ordered trajectory samples and associated sampling rate
USTRUCT(BlueprintType, Category="Motion Trajectory")
struct ENGINE_API FTrajectorySampleRange
{
	GENERATED_BODY()

	// Debug rendering contants
	static constexpr FLinearColor DebugDefaultPredictionColor{ 0.f, 1.f, 0.f };
	static constexpr FLinearColor DebugDefaultHistoryColor{ 0.f, 0.f, 1.f };
	static constexpr float DebugDefaultTransformScale = 10.f;
	static constexpr float DebugDefaultTransformThickness = 2.f;
	static constexpr float DebugDefaultArrowScale = 0.025f;
	static constexpr float DebugDefaultArrowSize = 40.f;
	static constexpr float DebugDefaultArrowThickness = 2.f;

	// Linearly ordered (Time or Distance domain) container for past, present, and future trajectory samples
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	TArray<FTrajectorySample> Samples;

	// Per-second sample rate of the trajectory sample container
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	int32 SampleRate = 30;

	// Constructors
	FTrajectorySampleRange() = default;

	FTrajectorySampleRange(int32 Rate) : Samples(), SampleRate(Rate) { }

	// Removes history samples from trajectory (retains present and future)
	void RemoveHistory();

	// Rotates all samples in the trajectory
	void Rotate(const FQuat& Rotation);

	// Interpolates transform over time
	void TransformOverTime(const FTransform& Transform, float StartTime, float DeltaTime);

	// Rotates all samples in the trajectory
	void TransformReferenceFrame(const FTransform& Transform);

	// Determine if any trajectory samples are present
	bool HasSamples() const;

	// Determine if all trajectory samples are default values
	bool HasOnlyZeroSamples() const;

	// Debug draw in-world trajectory samples and optional corresponding information
	void DebugDrawTrajectory(bool bEnable
		, const UWorld* World
		, const FTransform& WorldTransform
		, const FLinearColor PredictionColor = DebugDefaultPredictionColor
		, const FLinearColor HistoryColor = DebugDefaultHistoryColor
		, float TransformScale = DebugDefaultTransformScale
		, float TransformThickness = DebugDefaultTransformThickness
		, float VelArrowScale = DebugDefaultArrowScale
		, float VelArrowSize = DebugDefaultArrowSize
		, float VelArrowThickness = DebugDefaultArrowThickness) const;

	// Iterator for precise subsampling of the trajectory
	template <typename Container> static FTrajectorySample IterSampleTrajectory(const Container& Samples, ETrajectorySampleDomain DomainType, float DomainValue, int32& InitialIdx, bool bSmoothInterp = false)
	{
		auto SafeNextIndex = [](float Idx, float Num)
		{
			return FMath::Min(Idx + 1, Num - 1);
		};

		auto SafePrevIndex = [](float Idx)
		{
			return Idx > 0 ? Idx - 1 : 0;
		};

		// Iterative invocations of this function should use the NextIdx output parameter as a method of short circuiting a full linear traversal of the range
		for (int32 Idx = InitialIdx, Num = Samples.Num(); Idx < Num; ++Idx)
		{
			const FTrajectorySample& NextSample = Samples[Idx];
			const float NextDomainValue = DomainType == ETrajectorySampleDomain::Time ? NextSample.AccumulatedSeconds : NextSample.AccumulatedDistance;

			// Continue traversing the samples until an appropriate "right-hand" side domain value is found for interpolation 
			if (NextDomainValue < DomainValue)
			{
				continue;
			}
			else
			{
				// Range check to disallow a right-hand 0 index from using -1 as a left index
				InitialIdx = SafePrevIndex(Idx);

				// Find the interpolation factor of P between: [P1 .. P .. P2] by mapping the domain values to [0 ... 1]
				// Note: NextIdx is biased to equal P1 rather than P2 to account for cases where subsequent trajectory iterations may require subsampling between [P1 ... P2] again
				const FTrajectorySample& InitialSample = Samples[InitialIdx];
				const float PrevDomainValue = DomainType == ETrajectorySampleDomain::Time ? InitialSample.AccumulatedSeconds : InitialSample.AccumulatedDistance;
				const float Alpha = FMath::GetMappedRangeValueUnclamped(FVector2f(PrevDomainValue, NextDomainValue), FVector2f(0.f, 1.f), DomainValue);

				FTrajectorySample InterpSample;
				if (bSmoothInterp)
				{
					// Apply Centripetal Catmull–Rom spline interpolation when four valid control points can be established during sub-sampling
					const int32 PrevIdx = SafePrevIndex(InitialIdx);
					const int32 NextIdx = SafeNextIndex(Idx, Num);

					if (PrevIdx != InitialIdx && InitialIdx != Idx && Idx != NextIdx)
					{
						InterpSample = InitialSample.SmoothInterp(Samples[PrevIdx], NextSample, Samples[NextIdx], Alpha);
						return InterpSample;
					}
				}

				// For now, we will by default defer to linear interpolation
				InterpSample = InitialSample.Lerp(NextSample, Alpha);
				return InterpSample;
			}
		}

		// Sampling beyond the available range will extrapolate as the last sample
		return !Samples.IsEmpty() ? Samples.Last() : FTrajectorySample();
	}
};