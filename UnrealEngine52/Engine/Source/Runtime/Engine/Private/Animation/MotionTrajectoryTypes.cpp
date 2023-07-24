// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MotionTrajectoryTypes.h"
#include "Algo/AllOf.h"
#include "Animation/AnimTypes.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimTypes.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringFormatArg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MotionTrajectoryTypes)

#if ENABLE_ANIM_DEBUG
static constexpr int32 DebugTrajectorySampleDisable = 0;
static constexpr int32 DebugTrajectorySampleCount = 1;
static constexpr int32 DebugTrajectorySampleTime = 2;
static constexpr int32 DebugTrajectorySamplePosition = 3;
static constexpr int32 DebugTrajectorySampleVelocity = 4;
static const FVector DebugSampleTypeOffset(0.f, 0.f, 50.f);
static const FVector DebugSampleOffset(0.f, 0.f, 10.f);

TAutoConsoleVariable<int32> CVarMotionTrajectoryDebug(TEXT("a.MotionTrajectory.Debug"), 0, TEXT("Turn on debug drawing for motion trajectory"));
TAutoConsoleVariable<int32> CVarMotionTrajectoryDebugStride(TEXT("a.MotionTrajectory.Stride"), 1, TEXT("Configure the sample stride when displaying information"));
TAutoConsoleVariable<int32> CVarMotionTrajectoryDebugOptions(TEXT("a.MotionTrajectory.Options"), 0, TEXT("Toggle motion trajectory sample information:\n 0. Disable Text\n 1. Index\n2. Accumulated Time\n 3. Position\n 4. Velocity\n 5. Acceleration"));
#endif

namespace
{
	template<class U> static inline U CubicCRSplineInterpSafe(const U& P0, const U& P1, const U& P2, const U& P3, const float I, const float A = 0.5f)
	{
		float D1;
		float D2;
		float D3;

		if constexpr (TIsFloatingPoint<U>::Value)
		{
			D1 = FMath::Abs(P1 - P0);
			D2 = FMath::Abs(P2 - P1);
			D3 = FMath::Abs(P3 - P2);
		}
		else
		{
			D1 = static_cast<float>(FVector::Distance(P0, P1));
			D2 = static_cast<float>(FVector::Distance(P2, P1));
			D3 = static_cast<float>(FVector::Distance(P3, P2));
		}

		const float T0 = 0.f;
		const float T1 = T0 + FMath::Pow(D1, A);
		const float T2 = T1 + FMath::Pow(D2, A);
		const float T3 = T2 + FMath::Pow(D3, A);

		return FMath::CubicCRSplineInterpSafe(P0, P1, P2, P3, T0, T1, T2, T3, FMath::Lerp(T1, T2, I));
	}
}

bool FTrajectorySample::IsZeroSample() const
{
	// AccumulatedTime is specifically omitted here to allow for the zero sample semantic across an entire trajectory range
	return LinearVelocity.IsNearlyZero()
		&& Transform.GetTranslation().IsNearlyZero()
		&& Transform.GetRotation().IsIdentity();
}

FTrajectorySample FTrajectorySample::Lerp(const FTrajectorySample& Sample, float Alpha) const
{
	FTrajectorySample Interp;
	Interp.AccumulatedSeconds = FMath::Lerp(AccumulatedSeconds, Sample.AccumulatedSeconds, Alpha);
	Interp.LinearVelocity = FMath::Lerp(LinearVelocity, Sample.LinearVelocity, Alpha);

	Interp.Transform.Blend(Transform, Sample.Transform, Alpha);
	
	return Interp;
}

FTrajectorySample FTrajectorySample::SmoothInterp(const FTrajectorySample& PrevSample
	, const FTrajectorySample& Sample
	, const FTrajectorySample& NextSample
	, float Alpha) const
{
	FTrajectorySample Interp;
	Interp.AccumulatedSeconds = CubicCRSplineInterpSafe(PrevSample.AccumulatedSeconds, AccumulatedSeconds, Sample.AccumulatedSeconds, NextSample.AccumulatedSeconds, Alpha);
	Interp.LinearVelocity = CubicCRSplineInterpSafe(PrevSample.LinearVelocity, LinearVelocity, Sample.LinearVelocity, NextSample.LinearVelocity, Alpha);

	Interp.Transform.SetLocation(CubicCRSplineInterpSafe(
		PrevSample.Transform.GetLocation(),
		Transform.GetLocation(),
		Sample.Transform.GetLocation(),
		NextSample.Transform.GetLocation(),
		Alpha));
	FQuat Q0 = PrevSample.Transform.GetRotation().W >= 0.0f ? 
		PrevSample.Transform.GetRotation() : -PrevSample.Transform.GetRotation();
	FQuat Q1 = Transform.GetRotation().W >= 0.0f ? 
		Transform.GetRotation() : -Transform.GetRotation();
	FQuat Q2 = Sample.Transform.GetRotation().W >= 0.0f ? 
		Sample.Transform.GetRotation() : -Sample.Transform.GetRotation();
	FQuat Q3 = NextSample.Transform.GetRotation().W >= 0.0f ? 
		NextSample.Transform.GetRotation() : -NextSample.Transform.GetRotation();

	FQuat T0, T1;
	FQuat::CalcTangents(Q0, Q1, Q2, 0.0f, T0);
	FQuat::CalcTangents(Q1, Q2, Q3, 0.0f, T1);

	Interp.Transform.SetRotation(FQuat::Squad(Q1, T0, Q2, T1, Alpha));

	return Interp;
}

void FTrajectorySample::PrependOffset(const FTransform DeltaTransform, float DeltaSeconds)
{
	AccumulatedSeconds += DeltaSeconds;
	Transform *= DeltaTransform;
	LinearVelocity = DeltaTransform.TransformVectorNoScale(LinearVelocity);
}

void FTrajectorySample::TransformReferenceFrame(const FTransform DeltaTransform)
{
	Transform = DeltaTransform.Inverse() * Transform * DeltaTransform;
	LinearVelocity = DeltaTransform.TransformVectorNoScale(LinearVelocity);
}

bool FTrajectorySampleRange::HasSamples() const
{
	return !Samples.IsEmpty();
}

bool FTrajectorySampleRange::HasOnlyZeroSamples() const
{
	return Algo::AllOf(Samples, [](const FTrajectorySample& Sample)
		{
			return Sample.IsZeroSample();
		});
}

void FTrajectorySampleRange::RemoveHistory()
{
	Samples.RemoveAll([](const FTrajectorySample& Sample)
		{
			return Sample.AccumulatedSeconds < 0.f;
		});
}

void FTrajectorySampleRange::Rotate(const FQuat& Rotation)
{
	for (auto& Sample : Samples)
	{
		Sample.PrependOffset(FTransform(Rotation), 0.0f);
	}
}

void FTrajectorySampleRange::TransformReferenceFrame(const FTransform& Transform)
{
	for (auto& Sample : Samples)
	{
		Sample.TransformReferenceFrame(Transform);
	}
}

FTrajectorySample FTrajectorySampleRange::GetSampleAtTime(float Time, bool bExtrapolate) const
{
	const int32 Num = Samples.Num();
	if (Num > 1)
	{
		const int32 LowerBoundIdx = Algo::LowerBound(Samples, Time, [](const FTrajectorySample& TrajectorySample, float Value)
			{
				return Value > TrajectorySample.AccumulatedSeconds;
			});

		const int32 NextIdx = FMath::Clamp(LowerBoundIdx, 1, Samples.Num() - 1);
		const int32 PrevIdx = NextIdx - 1;

		const float Denominator = Samples[NextIdx].AccumulatedSeconds - Samples[PrevIdx].AccumulatedSeconds;
		if (!FMath::IsNearlyZero(Denominator))
		{
			const float Numerator = Time - Samples[PrevIdx].AccumulatedSeconds;
			const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
			return Samples[PrevIdx].Lerp(Samples[NextIdx], LerpValue);
		}

		return Samples[PrevIdx];
	}

	if (Num > 0)
	{
		return Samples[0];
	}

	return FTrajectorySample();
}

void FTrajectorySampleRange::DebugDrawTrajectory(bool bEnable
	, const UWorld* World
	, const FTransform& WorldTransform
	, const FLinearColor PredictionColor
	, const FLinearColor HistoryColor
	, float TransformScale
	, float TransformThickness
	, float VelArrowScale
	, float VelArrowSize
	, float VelArrowThickness) const
{
	if (bEnable
#if ENABLE_ANIM_DEBUG
		|| CVarMotionTrajectoryDebug.GetValueOnAnyThread()
#endif
		)
	{
		if (World)
		{
#if ENABLE_ANIM_DEBUG
			const int32 DebugSampleStride = CVarMotionTrajectoryDebugStride.GetValueOnAnyThread();
			const int32 DebugSampleOptions = CVarMotionTrajectoryDebugOptions.GetValueOnAnyThread();
#endif
			for (int32 Idx = 0, Num = Samples.Num(); Idx < Num; Idx++)
			{
				const FTransform SampleTransformWS = Samples[Idx].Transform * WorldTransform;
				const FVector SamplePositionWS = SampleTransformWS.GetTranslation();

				const FVector WorldVelocity =
					(WorldTransform.TransformVector(Samples[Idx].LinearVelocity) * VelArrowScale) + SamplePositionWS;

				// Interpolate the history and prediction color over the entire trajectory range
				const float ColorLerp = static_cast<float>(Idx) / static_cast<float>(Num);
				const FLinearColor Color = FLinearColor::LerpUsingHSV(PredictionColor, HistoryColor, ColorLerp);

				if (VelArrowScale > 0.0f)
				{
					DrawDebugDirectionalArrow(
						World, SamplePositionWS, WorldVelocity, 
						VelArrowSize, Color.ToFColor(true), false, 0.f, 0, VelArrowThickness);
				}

				if (TransformScale > 0.0f)
				{
					DrawDebugCoordinateSystem(
						World, SampleTransformWS.GetLocation(), SampleTransformWS.Rotator(),
						TransformScale, false, 0.f, 0, TransformThickness);

					if (VelArrowScale == 0.0f)
					{
						DrawDebugSphere(
							World, SamplePositionWS,										
							TransformScale * 0.5f, 4, Color.ToFColor(true), false, 0.f, 0, TransformThickness * 0.5f);
					}
				}

#if ENABLE_ANIM_DEBUG
				FString DebugString;
				FString DebugSampleString;
				switch (DebugSampleOptions)
				{
				case DebugTrajectorySampleCount: // Sample Index
					DebugString = "Sample Index:";
					DebugSampleString = DebugSampleString.Format(TEXT("{0}"), { Idx });
;					break;
				case DebugTrajectorySampleTime: // Sample Accumulated Time
					DebugString = "Sample Time:";
					DebugSampleString = DebugSampleString.Format(TEXT("{0}"), { Samples[Idx].AccumulatedSeconds });
					break;
				case DebugTrajectorySamplePosition: // Sample Position
					DebugString = "Sample Position:";
					DebugSampleString = DebugSampleString.Format(TEXT("{0}"), { Samples[Idx].Transform.GetLocation().ToCompactString() });
					break;
				case DebugTrajectorySampleVelocity: // Sample Velocity
					DebugString = "Sample Velocity:";
					DebugSampleString = DebugSampleString.Format(TEXT("{0}"), { Samples[Idx].LinearVelocity.ToCompactString() });
					break;
				default:
					break;
				}

				// Conditionally display per-sample information against a specified stride
				if (!DebugSampleString.IsEmpty() && !!DebugSampleStride && (Idx % DebugSampleStride == 0))
				{
					// One time debug drawing of the per-sample type description
					if (!DebugString.IsEmpty() && Idx == 0)
					{
						DrawDebugString(World, WorldTransform.GetLocation() + DebugSampleTypeOffset, DebugString, nullptr, FColor::White, 0.f, false, 1.f);
					}

					DrawDebugString(World, WorldVelocity + DebugSampleOffset, DebugSampleString, nullptr, FColor::White, 0.f, false, 1.f);
				}
#endif
			}
		}
	}
}
