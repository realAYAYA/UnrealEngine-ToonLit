// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSplineTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSplineTypes)

ControlRigBaseSpline::ControlRigBaseSpline(const TArrayView<const FVector>& InControlPoints, const uint16 InDegree, bool bInClosed)
: Degree(InDegree)
, bClosed(bInClosed)
{
	if (bClosed)
	{
		int32 NumPoints = InControlPoints.Num() + Degree;
		ControlPoints.Reserve(NumPoints);
		
		int32 Index = InControlPoints.Num() - FMath::RoundToPositiveInfinity((float)Degree/2);
		while (ControlPoints.Num() < NumPoints)
		{
			Index = (Index+1)%InControlPoints.Num();
			ControlPoints.Add(InControlPoints[Index]);
		}
	}
	else
	{
		ControlPoints = InControlPoints;
	}
}

TArray<FVector> ControlRigBaseSpline::GetControlPointsWithoutDuplicates()
{
	if (bClosed)
	{
		const int32 NumPoints = ControlPoints.Num() - Degree;
		const int32 NumInitPointsToIgnore = FMath::RoundToPositiveInfinity((float)Degree/2) - 1;
		
		TArray<FVector> ReducedControlPoints;
		ReducedControlPoints.Reserve(NumPoints);
		for (int32 i=0; i<NumPoints; ++i)
		{
			ReducedControlPoints.Add(ControlPoints[i+NumInitPointsToIgnore]);
		}
		return ReducedControlPoints;
	}

	return ControlPoints;
}

void ControlRigBaseSpline::SetControlPoints(const TArrayView<const FVector>& InControlPoints)
{
	if (bClosed)
	{
		int32 CurIndex = 0;
		int32 Index = InControlPoints.Num() - FMath::RoundToPositiveInfinity((float)Degree/2);
		while (CurIndex < ControlPoints.Num())
		{
			Index = (Index+1)%InControlPoints.Num();
			ControlPoints[CurIndex++] = InControlPoints[Index];
		}
	}
	else
	{
		ControlPoints = InControlPoints;
	}
}

ControlRigBSpline::ControlRigBSpline(const TArrayView<const FVector>& InControlPoints, const uint16 InDegree, const bool bInClosed, bool bInClamped)
: ControlRigBaseSpline(InControlPoints, InDegree, bInClosed)
{
	uint16 N = ControlPoints.Num() - 1;
	uint16 P = Degree;
	uint16 M = N + P + 1;
	KnotVector.SetNumZeroed(M+1);

	// Clamped means there is p+1 0s at the beginning and 1s at the end of the knot vector
	if (bInClamped && !bInClosed)
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

ControlRigHermite::ControlRigHermite(const TArrayView<const FVector>& InControlPoints, const bool bInClosed)
: ControlRigBaseSpline(InControlPoints, 4, bInClosed)
{
	if (bClosed)
	{
		SegmentPoints.Append(ControlPoints);
		NumSegments = SegmentPoints.Num() - 4;
	}
	else
	{
		SegmentPoints.Add(ControlPoints[0] + ControlPoints[0] - ControlPoints[1]);
		SegmentPoints.Append(ControlPoints);
		SegmentPoints.Add(ControlPoints.Last() + ControlPoints.Last() - ControlPoints[ControlPoints.Num()-2]);
		NumSegments = SegmentPoints.Num() - 3;
	}
}

void ControlRigHermite::SetControlPoints(const TArrayView<const FVector>& InControlPoints)
{
	ControlRigBaseSpline::SetControlPoints(InControlPoints);

	SegmentPoints.Reset();
	if (bClosed)
	{
		SegmentPoints.Append(ControlPoints);
	}
	else
	{
		SegmentPoints.Add(ControlPoints[0] + ControlPoints[0] - ControlPoints[1]);
		SegmentPoints.Append(ControlPoints);
		SegmentPoints.Add(ControlPoints.Last() + ControlPoints.Last() - ControlPoints[ControlPoints.Num()-2]);
	}
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

FControlRigSplineImpl::~FControlRigSplineImpl()
{
	if (Spline)
	{
		delete Spline;
		Spline = nullptr;
	}
}

TArray<FVector>& FControlRigSplineImpl::GetControlPoints()
{
	check(Spline);
	return Spline->GetControlPoints();	
}

TArray<FVector> FControlRigSplineImpl::GetControlPointsWithoutDuplicates()
{
	check(Spline);
	return Spline->GetControlPointsWithoutDuplicates();
}

uint8 FControlRigSplineImpl::GetDegree() const
{
	check(Spline);
	return Spline->GetDegree();
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

uint8 FControlRigSpline::GetDegree() const
{
	if (!SplineData.IsValid())
	{
		return 0;
	}
	return SplineData->GetDegree();
}

void FControlRigSpline::SetControlPoints(const TArrayView<const FVector>& InPoints, const ESplineType SplineMode, const bool bInClosed, const int32 SamplesPerSegment, const float Compression, const float Stretch)
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

	TArray<FVector> OldControlPoints;
	if (SplineData->Spline)
	{
		OldControlPoints = SplineData->GetControlPointsWithoutDuplicates();
	}

	bool bControlPointsChanged = (SplineData->Spline) ? InPoints != OldControlPoints : true;
	bool bNumControlPointsChanged = (SplineData->Spline) ? OldControlPoints.Num() != ControlPointsCount : true;
	bool bSplineModeChanged = SplineMode != SplineData->SplineMode;
	bool bSamplesCountChanged = SamplesPerSegment != SplineData->SamplesPerSegment;
	bool bStretchChanged = Stretch != SplineData->Stretch || Compression != SplineData->Compression;
	bool bClosedChanged = bInClosed != SplineData->bClosed;
	if (!bSplineModeChanged && !bControlPointsChanged && !bSamplesCountChanged && !bStretchChanged && !bClosedChanged)
	{
		return;
	}

	SplineData->SplineMode = SplineMode;
	SplineData->bClosed = bInClosed;
	SplineData->SamplesPerSegment = SamplesPerSegment;
	SplineData->Compression = Compression;
	SplineData->Stretch = Stretch;

	// If we need to update the spline because the controls points have changed, or the spline mode has changed
	if (bControlPointsChanged || bSplineModeChanged || bClosedChanged)
	{	
		if (bNumControlPointsChanged || bSplineModeChanged || bClosedChanged)
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
					SplineData->Spline = new ControlRigBSpline(InPoints, 3, bInClosed, true);
					break;
				}
				case ESplineType::Hermite:
				{
					
					SplineData->Spline = new ControlRigHermite(InPoints, bInClosed);
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
	}

	// If curve has changed, or sample count has changed, recompute the cache
	if (bControlPointsChanged || bSplineModeChanged || bSamplesCountChanged || bStretchChanged || bClosedChanged)
	{
		// Cache sample positions of the spline
		int32 NumSamples = (ControlPointsCount-1) * SamplesPerSegment;
		SplineData->SamplesArray.SetNumUninitialized(NumSamples, false);
			
		float U = 0.f;
		float DeltaU = 1.f/(NumSamples-1);
		for (int32 i=0; i<NumSamples; ++i, U+=DeltaU)
		{
			SplineData->SamplesArray[i] = SplineData->Spline->GetPointAtParam(U); 
		}

		// Correct length of samples
		if (!bSplineModeChanged && !bSamplesCountChanged && !bNumControlPointsChanged && !bClosedChanged)
		{
			if (SplineData->InitialLengths.Num() > 0)
			{
				TArray<FVector> SamplesBeforeCorrect = SplineData->SamplesArray;
				for (int32 i = 0; i < ControlPointsCount - 1; ++i)
				{
					for (int32 j = (i == 0) ? 1 : 0; j < SamplesPerSegment; ++j)
					{
						const int32 CurSampleIndex = i * SamplesPerSegment + j;
						const int32 PrevSampleIndex = CurSampleIndex - 1;
						if (!SplineData->InitialLengths.IsValidIndex(PrevSampleIndex))
						{
							break;
						}
						
						// Get direction from samples before correction
						FVector Dir = SamplesBeforeCorrect[CurSampleIndex] - SamplesBeforeCorrect[PrevSampleIndex];
						Dir.Normalize();

						float InitialLength = SplineData->InitialLengths[PrevSampleIndex];
						// Current length as the projection on the Dir vector (might be negative)
						float CurrentLength = (SplineData->SamplesArray[CurSampleIndex] - SplineData->SamplesArray[PrevSampleIndex]).Dot(Dir);
						float FixedLength = FMath::Clamp(CurrentLength,
							(Compression > 0.f) ? InitialLength * Compression : CurrentLength,
							(Stretch > 0.f) ? InitialLength * Stretch : CurrentLength);

						SplineData->SamplesArray[CurSampleIndex] = SplineData->SamplesArray[PrevSampleIndex] + Dir * FixedLength;
					}
				}
			}
		}

		// Cache accumulated length at sample array
		{
			SplineData->AccumulatedLenth.SetNumUninitialized(SplineData->SamplesArray.Num(), false);
			SplineData->AccumulatedLenth[0] = 0.f;
			if (bSplineModeChanged || bClosedChanged || bSamplesCountChanged || bNumControlPointsChanged)
			{
				SplineData->InitialLengths.SetNumUninitialized(SplineData->SamplesArray.Num() - 1);
			}
			for (int32 i = 1; i < SplineData->SamplesArray.Num(); ++i)
			{
				float CurrentLength = FVector::Distance(SplineData->SamplesArray[i - 1], SplineData->SamplesArray[i]);
				if (bSplineModeChanged || bClosedChanged || bSamplesCountChanged || bNumControlPointsChanged)
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

