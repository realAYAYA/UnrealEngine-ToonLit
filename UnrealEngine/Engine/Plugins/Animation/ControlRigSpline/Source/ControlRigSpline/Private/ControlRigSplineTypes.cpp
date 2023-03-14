// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSplineTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSplineTypes)

#if !(USE_TINYSPLINE)

ControlRigBaseSpline::ControlRigBaseSpline(const TArrayView<const FVector>& InControlPoints, const uint16 InDegree)
: ControlPoints(InControlPoints)
, Degree(InDegree)
{
	
}

ControlRigBSpline::ControlRigBSpline(const TArrayView<const FVector>& InControlPoints, const uint16 InDegree, bool bInClamped)
: ControlRigBaseSpline(InControlPoints, InDegree)
{
	uint16 N = ControlPoints.Num() - 1;
	uint16 P = Degree;
	uint16 M = N + P + 1;
	KnotVector.SetNumZeroed(M+1);

	// Clamped means there is p+1 0s at the beginning and 1s at the end of the knot vector
	if (bInClamped)
	{
		for (int32 i=M-P; i<=M; ++i)
		{
			KnotVector[i] = 1.f;
		}

		float Dk = 1.f/(M-P - (P+1) +1);
		for (int32 i=P+1; i<M-P; ++i)
		{
			KnotVector[i] = (i-P)*Dk;
		}
	}
	else
	{
		float Dk = 1.f / M;
		for (int32 i = 0; i<M+1; ++i)
		{
			KnotVector[i] = i*Dk;
		}
	}
}

FVector ControlRigBSpline::GetPointAtParam(float Param)
{
	// DeBoor's Algorithm
	// https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES/spline/B-spline/de-Boor.html
	
	float ClampedU = FMath::Clamp(Param, 0.f, 1.f);

	uint16 N = ControlPoints.Num() - 1;
	uint16 P = Degree;
	uint16 M = N + P + 1;

	// Remap U value to KnotVector[P] : KnotVector[M+1 - (P+1)]
	float Min = KnotVector[P] + KINDA_SMALL_NUMBER;
	float Max = KnotVector[M-P] - KINDA_SMALL_NUMBER;
	float U = Min + ClampedU*(Max-Min);

	// Find [uk, uk+1) where U lives (or k == the first appearence of U)
	uint16 K = 0;
	{
		// Find [uk, uk+1) where U lives
		for (uint16 i=0; i<KnotVector.Num()-1; ++i)
		{
			if ((FMath::IsNearlyEqual(KnotVector[i], U)) ||
				(KnotVector[i] < U && KnotVector[i+1] > U))
			{
				K = i;
				break;
			}
		}
	}

	TArray<FVector> WorkingPoints;
	WorkingPoints.SetNum(P+1);
	for (int32 i=K-P; i<=K; ++i)
	{
		WorkingPoints[i-(K-P)] = ControlPoints[i];
	}
	
	for (uint16 R = 1; R <= Degree; ++R)
	{
		uint16 StartCPIndex = K-P+R;
		uint16 EndCPIndex = K;
		if (FMath::IsNearlyEqual(KnotVector[StartCPIndex], U))
		{
			continue;
		}

		uint16 Index = 0;
		for (uint16 i = StartCPIndex; i <= EndCPIndex; ++i, ++Index)
		{
			float Air = (U - KnotVector[i]) / (KnotVector[i+P-R+1] - KnotVector[i]);
			WorkingPoints[Index] = (1-Air) * WorkingPoints[Index] + Air * WorkingPoints[Index+1];
		}
	}

	return WorkingPoints[0];
}

ControlRigHermite::ControlRigHermite(const TArrayView<const FVector>& InControlPoints)
: ControlRigBaseSpline(InControlPoints, 3)
{
	SegmentPoints.Add(ControlPoints[0] + ControlPoints[0] - ControlPoints[1]);
	SegmentPoints.Append(ControlPoints);
	SegmentPoints.Add(ControlPoints.Last() + ControlPoints.Last() - ControlPoints[ControlPoints.Num()-2]);

	NumSegments = SegmentPoints.Num() - 3;
}

void ControlRigHermite::SetControlPoints(const TArrayView<const FVector>& InControlPoints)
{
	ControlRigBaseSpline::SetControlPoints(InControlPoints);

	SegmentPoints.Reset();
	SegmentPoints.Add(ControlPoints[0] + ControlPoints[0] - ControlPoints[1]);
	SegmentPoints.Append(ControlPoints);
	SegmentPoints.Add(ControlPoints.Last() + ControlPoints.Last() - ControlPoints[ControlPoints.Num()-2]);
}

static FVector Hermite(const FVector& P0, const FVector& P1, const FVector& M0, const FVector& M1, float Param)
{
	// https://en.wikipedia.org/wiki/Cubic_Hermite_spline#Catmull–Rom_spline

	const float T = Param;
	const float T2 = T*T;
	const float T3 = T2*T;
	
	float H00 = 2*T3 - 3*T2 + 1;
	float H10 = T3 - 2*T2 + T;
	float H01 = -2*T3 + 3*T2;
	float H11 = T3 - T2;

	return H00*P0 + H10*M0 + H01*P1 + H11*M1;
}

FVector ControlRigHermite::GetPointAtParam(float Param)
{
	float ClampedParam = FMath::Clamp(Param, 0.f, 1.f);	

	float ParamIntervals = 1.f / (float) NumSegments;
	uint16 SegmentIndex = FMath::Floor(ClampedParam / ParamIntervals);
	if (SegmentIndex >= NumSegments)
	{
		SegmentIndex = NumSegments-1;
	}

	uint16 P3Index = SegmentIndex + 3;
	P3Index = (P3Index < SegmentPoints.Num()) ? P3Index : SegmentPoints.Num()-1;
	uint16 P0Index = P3Index - 3;
	uint16 P1Index = P3Index - 2;
	uint16 P2Index = P3Index - 1;

	float StartSegmentParam = SegmentIndex * ParamIntervals;
	float T = FMath::Clamp((ClampedParam-StartSegmentParam)/ParamIntervals, 0.f, 1.f);

	const FVector& P0 = SegmentPoints[P0Index];
	const FVector& P1 = SegmentPoints[P1Index];
	const FVector& P2 = SegmentPoints[P2Index];
	const FVector& P3 = SegmentPoints[P3Index];

	// https://www.cs.cmu.edu/~fp/courses/graphics/asst5/catmullRom.pdf
	float Tension = 0.5f;
	FVector M1 = Tension * (P2 - P0);
	FVector M2 = Tension * (P3 - P1);
	return Hermite(P1, P2, M1, M2, T);
}
#endif

FControlRigSplineImpl::~FControlRigSplineImpl()
{
#if !(USE_TINYSPLINE)
	if (Spline)
	{
		delete Spline;
		Spline = nullptr;
	}
#endif
}

TArray<FVector>& FControlRigSplineImpl::GetControlPoints()
{
#if USE_TINYSPLINE
	return ControlPoints;
#else
	check(Spline);
	return Spline->GetControlPoints();	
#endif
}

FControlRigSpline::FControlRigSpline(const FControlRigSpline& InOther)
{
	SplineData = InOther.SplineData;
}

FControlRigSpline& FControlRigSpline::operator=(const FControlRigSpline& InOther)
{
	SplineData = InOther.SplineData;
	return *this;
}

void FControlRigSpline::SetControlPoints(const TArrayView<const FVector>& InPoints, const ESplineType SplineMode, const int32 SamplesPerSegment, const float Compression, const float Stretch)
{
	const int32 ControlPointsCount = InPoints.Num();
	if (ControlPointsCount < 4)
	{
		return;
	}

	if (!SplineData.IsValid())
	{
		SplineData = MakeShared<FControlRigSplineImpl>();
	}

#if USE_TINYSPLINE
	bool bControlPointsChanged = InPoints != SplineData->GetControlPoints();
	bool bNumControlPointsChanged = SplineData->GetControlPoints().Num() != ControlPointsCount;
#else
	bool bControlPointsChanged = (SplineData->Spline) ? InPoints != SplineData->GetControlPoints() : true;
	bool bNumControlPointsChanged = (SplineData->Spline) ? SplineData->GetControlPoints().Num() != ControlPointsCount : true;
#endif
	bool bSplineModeChanged = SplineMode != SplineData->SplineMode;
	bool bSamplesCountChanged = SamplesPerSegment != SplineData->SamplesPerSegment;
	bool bStretchChanged = Stretch != SplineData->Stretch || Compression != SplineData->Compression;
	if (!bSplineModeChanged && !bControlPointsChanged && !bSamplesCountChanged && !bStretchChanged)
	{
		return;
	}

#if USE_TINYSPLINE
	SplineData->ControlPoints = InPoints;
#endif
	SplineData->SplineMode = SplineMode;
	SplineData->SamplesPerSegment = SamplesPerSegment;
	SplineData->Compression = Compression;
	SplineData->Stretch = Stretch;

	// If we need to update the spline because the controls points have changed, or the spline mode has changed
	if (bControlPointsChanged || bSplineModeChanged)
	{	
#if USE_TINYSPLINE
		switch (SplineMode)
		{
			case ESplineType::BSpline:
			{
				if (bSplineModeChanged || bNumControlPointsChanged)
				{
					SplineData->Spline = tinyspline::BSpline(ControlPointsCount, 3);
				}

				// Update the positions of the control points

				// There's no guarantee that FVector is a tightly packed array of three floats. 
				// We have SIMD versions where we waste a dummy float to align it on a 16 byte boundary,
				// so we need to iterate updating the points one by one.
				for (int32 i = 0; i < ControlPointsCount; ++i)
				{
					FVector Point = InPoints[i];
					ts_bspline_set_control_point_at(SplineData->Spline.data(), i, &Point.X, nullptr);
				}

				break;
			}
			case ESplineType::Hermite:
			{
				break;
			}
			default:
			{
				checkNoEntry(); // Unknown Spline Mode
				break;
			}
		}

#else

		if (bNumControlPointsChanged || bSplineModeChanged)
		{
			// Delete previously created spline
			if (SplineData->Spline)
			{
				delete SplineData->Spline;
				SplineData->Spline = nullptr;
			}
			
			switch (SplineMode)
			{
				case ESplineType::BSpline:
				{
					SplineData->Spline = new ControlRigBSpline(InPoints, 3, true);
					break;
				}
				case ESplineType::Hermite:
				{
					
					SplineData->Spline = new ControlRigHermite(InPoints);
					break;
				}
				default:
				{
					checkNoEntry(); // Unknown Spline Mode
					break;
				}
			}
		}
		else if (bControlPointsChanged)
		{
			SplineData->Spline->SetControlPoints(InPoints);
		}
#endif
	}

	// If curve has changed, or sample count has changed, recompute the cache
	if (bControlPointsChanged || bSplineModeChanged || bSamplesCountChanged || bStretchChanged)
	{
#if USE_TINYSPLINE
		switch (SplineMode)
		{
			case ESplineType::BSpline:
			{
				// Cache sample positions of the spline
				FVector::FReal* SamplesPtr = nullptr;
				size_t ActualSamplesPerSegment = 0;
				ts_bspline_sample(SplineData->Spline.data(), (ControlPointsCount - 1) * SamplesPerSegment, &SamplesPtr, &ActualSamplesPerSegment, nullptr);
				SplineData->SamplesArray.SetNumUninitialized((ControlPointsCount - 1) * SamplesPerSegment, false);
				for (int32 i = 0; i < ControlPointsCount - 1; ++i)
				{
					for (int32 j = 0; j < SamplesPerSegment; ++j)
					{
						SplineData->SamplesArray[i * SamplesPerSegment + j].X = SamplesPtr[(i * SamplesPerSegment + j) * 3];
						SplineData->SamplesArray[i * SamplesPerSegment + j].Y = SamplesPtr[(i * SamplesPerSegment + j) * 3 + 1];
						SplineData->SamplesArray[i * SamplesPerSegment + j].Z = SamplesPtr[(i * SamplesPerSegment + j) * 3 + 2];
					}
				}

				// tinySpline will allocate the samples array, but does not free that memory. We need to take care of that.
				free(SamplesPtr);

				break;
			}
			case ESplineType::Hermite:
			{
				SplineData->SamplesArray.SetNumUninitialized((ControlPointsCount - 1) * SamplesPerSegment);
				for (int32 i = 0; i < ControlPointsCount - 1; ++i)
				{
					const FVector P0 = (i > 0) ? InPoints[i-1] : 2*InPoints[0] - InPoints[1];
					const FVector& P1 = InPoints[i];
					const FVector& P2 = InPoints[i+1];
					const FVector P3 = (i + 2 < ControlPointsCount) ? InPoints[i+2] : 2*InPoints.Last() - InPoints[ControlPointsCount-2];

					// https://www.cs.cmu.edu/~fp/courses/graphics/asst5/catmullRom.pdf
					float Tension = 0.5f;
					FVector M1 = Tension * (P2 - P0);
					FVector M2 = Tension * (P3 - P1);

					float Dt = 1.f / (float) SamplesPerSegment;
					if (i == ControlPointsCount - 2)
					{
						Dt = 1.f / (float) (SamplesPerSegment - 1);
					}
					for (int32 j = 0; j < SamplesPerSegment; ++j)
					{
						// https://en.wikipedia.org/wiki/Cubic_Hermite_spline#Catmull–Rom_spline
						const float T = j  * Dt;
						const float T2 = T*T;
						const float T3 = T2*T;
	
						float H00 = 2*T3 - 3*T2 + 1;
						float H10 = T3 - 2*T2 + T;
						float H01 = -2*T3 + 3*T2;
						float H11 = T3 - T2;

						SplineData->SamplesArray[i * SamplesPerSegment + j] = H00*P1 + H10*M1 + H01*P2 + H11*M2;						
					}
				}
				break;
			}
			default:
			{
				checkNoEntry(); // Unknown Spline Mode
				break;
			}
		}
#else
		// Cache sample positions of the spline
		int32 NumSamples = (ControlPointsCount-1) * SamplesPerSegment;
		SplineData->SamplesArray.SetNumUninitialized(NumSamples, false);
			
		float U = 0.f;
		float DeltaU = 1.f/(NumSamples-1);
		for (int32 i=0; i<NumSamples; ++i, U+=DeltaU)
		{
			SplineData->SamplesArray[i] = SplineData->Spline->GetPointAtParam(U); 
		}
#endif

		// Correct length of samples
		if (!bSplineModeChanged && !bSamplesCountChanged && !bNumControlPointsChanged)
		{
			if (SplineData->InitialLengths.Num() > 0)
			{
				TArray<FVector> SamplesBeforeCorrect = SplineData->SamplesArray;
				for (int32 i = 0; i < ControlPointsCount - 1; ++i)
				{
					for (int32 j = (i == 0) ? 1 : 0; j < SamplesPerSegment; ++j)
					{
						// Get direction from samples before correction
						FVector Dir = SamplesBeforeCorrect[i * SamplesPerSegment + j] - SamplesBeforeCorrect[i * SamplesPerSegment + j - 1];
						Dir.Normalize();

						float InitialLength = SplineData->InitialLengths[i * SamplesPerSegment + j - 1];
						// Current length as the projection on the Dir vector (might be negative)
						float CurrentLength = (SplineData->SamplesArray[i * SamplesPerSegment + j] - SplineData->SamplesArray[i * SamplesPerSegment + j - 1]).Dot(Dir);
						float FixedLength = FMath::Clamp(CurrentLength,
							(Compression > 0.f) ? InitialLength * Compression : CurrentLength,
							(Stretch > 0.f) ? InitialLength * Stretch : CurrentLength);

						SplineData->SamplesArray[i * SamplesPerSegment + j] = SplineData->SamplesArray[i * SamplesPerSegment + j - 1] + Dir * FixedLength;
					}
				}
			}
		}

		// Cache accumulated length at sample array
		{
			SplineData->AccumulatedLenth.SetNumUninitialized(SplineData->SamplesArray.Num(), false);
			SplineData->AccumulatedLenth[0] = 0.f;
			if (bSplineModeChanged || bSamplesCountChanged || bNumControlPointsChanged)
			{
				SplineData->InitialLengths.SetNumUninitialized(SplineData->SamplesArray.Num() - 1);
			}
			for (int32 i = 1; i < SplineData->SamplesArray.Num(); ++i)
			{
				float CurrentLength = FVector::Distance(SplineData->SamplesArray[i - 1], SplineData->SamplesArray[i]);
				if (bSplineModeChanged || bSamplesCountChanged || bNumControlPointsChanged)
				{
					SplineData->InitialLengths[i-1] = CurrentLength;
				}
				SplineData->AccumulatedLenth[i] = SplineData->AccumulatedLenth[i - 1] + CurrentLength;
			}
		}
	}	
}

FVector FControlRigSpline::PositionAtParam(const float InParam) const
{
	if (!SplineData.IsValid())
	{
		return FVector();
	}

	float ClampedU = FMath::Clamp<float>(InParam, 0.f, 1.f);

	const int32 LastIndex = SplineData->SamplesArray.Num() - 1;
	float fIndexPrev = ClampedU * LastIndex;	
	int32 IndexPrev = FMath::Floor<int32>(fIndexPrev);
	float ULocal = fIndexPrev - IndexPrev;
	int32 IndexNext = (IndexPrev < LastIndex) ? IndexPrev + 1 : IndexPrev;

	return SplineData->SamplesArray[IndexPrev] * (1.f - ULocal) + SplineData->SamplesArray[IndexNext] * ULocal;
}

FVector FControlRigSpline::TangentAtParam(const float InParam) const
{
	if (!SplineData.IsValid())
	{
		return FVector();
	}

	const float ClampedU = FMath::Clamp<float>(InParam, 0.f, 1.f);

	int32 IndexPrev = ClampedU * (SplineData->SamplesArray.Num()-2);
	return SplineData->SamplesArray[IndexPrev+1] - SplineData->SamplesArray[IndexPrev];
}

