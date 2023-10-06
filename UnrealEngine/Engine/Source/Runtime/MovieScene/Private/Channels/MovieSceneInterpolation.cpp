// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneInterpolation.h"
#include "Curves/CurveEvaluation.h"

namespace UE::MovieScene::Interpolation
{

FCachedInterpolationRange FCachedInterpolationRange::Empty()
{
	return FCachedInterpolationRange{ 0, -1 };
}
FCachedInterpolationRange FCachedInterpolationRange::Finite(FFrameNumber InStart, FFrameNumber InEnd)
{
	return FCachedInterpolationRange{ InStart, InEnd };
}
FCachedInterpolationRange FCachedInterpolationRange::Infinite()
{
	return FCachedInterpolationRange{ TNumericLimits<FFrameNumber>::Lowest(), TNumericLimits<FFrameNumber>::Max() };
}
FCachedInterpolationRange FCachedInterpolationRange::Only(FFrameNumber InTime)
{
	const FFrameNumber EndTime = InTime < TNumericLimits<FFrameNumber>::Max()
		? InTime + 1
		: InTime;

	return FCachedInterpolationRange{ InTime, EndTime };
}
FCachedInterpolationRange FCachedInterpolationRange::From(FFrameNumber InStart)
{
	return FCachedInterpolationRange{ InStart, TNumericLimits<FFrameNumber>::Max() };
}
FCachedInterpolationRange FCachedInterpolationRange::Until(FFrameNumber InEnd) 
{
	return FCachedInterpolationRange{ TNumericLimits<FFrameNumber>::Lowest(), InEnd };
}
bool FCachedInterpolationRange::Contains(FFrameNumber FrameNumber) const
{
	return FrameNumber >= Start && (FrameNumber < End || End == TNumericLimits<FFrameNumber>::Max());
}

FCachedInterpolation::FCachedInterpolation()
	: Data(TInPlaceType<FInvalidValue>())
	, Range(FCachedInterpolationRange::Empty())
{
}

FCachedInterpolation::FCachedInterpolation(const FCachedInterpolationRange& InRange, const FConstantValue& Constant)
	: Data(TInPlaceType<FConstantValue>(), Constant)
	, Range(InRange)
{
}

FCachedInterpolation::FCachedInterpolation(const FCachedInterpolationRange& InRange, const FLinearInterpolation& Linear)
	: Data(TInPlaceType<FLinearInterpolation>(), Linear)
	, Range(InRange)
{
}

FCachedInterpolation::FCachedInterpolation(const FCachedInterpolationRange& InRange, const FCubicInterpolation& Cubic)
	: Data(TInPlaceType<FCubicInterpolation>(), Cubic)
	, Range(InRange)
{
}

FCachedInterpolation::FCachedInterpolation(const FCachedInterpolationRange& InRange, const FWeightedCubicInterpolation& WeightedCubic)
	: Data(TInPlaceType<FWeightedCubicInterpolation>(), WeightedCubic)
	, Range(InRange)
{
}

bool FCachedInterpolation::IsCacheValidForTime(FFrameNumber FrameNumber) const
{
	return Range.Contains(FrameNumber);
}

bool FCachedInterpolation::Evaluate(FFrameTime Time, double& OutResult) const
{
	if (const FConstantValue* Constant = Data.TryGet<FConstantValue>())
	{
		OutResult = Constant->Value;
		return true;
	}
	else if (const FLinearInterpolation* Linear = Data.TryGet<FLinearInterpolation>())
	{
		OutResult = Linear->Evaluate(Time);
		return true;
	}
	else if (const FCubicInterpolation* Cubic = Data.TryGet<FCubicInterpolation>())
	{
		OutResult = Cubic->Evaluate(Time);
		return true;
	}
	else if (const FWeightedCubicInterpolation* WeightedCubic = Data.TryGet<FWeightedCubicInterpolation>())
	{
		OutResult = WeightedCubic->Evaluate(Time);
		return true;
	}

	return false;
}

FCubicInterpolation::FCubicInterpolation(FFrameNumber InOrigin, double InDX, double InStartValue, double InEndValue, double InStartTangent, double InEndTangent)
	: DX(InDX)
	, Origin(InOrigin)
{
	constexpr double OneThird = 1.0 / 3.0;

	P0 = InStartValue;
	P1 = P0 + (InStartTangent * DX * OneThird);
	P3 = InEndValue;
	P2 = P3 - (InEndTangent * DX * OneThird);
}

double FCubicInterpolation::Evaluate(FFrameTime InTime) const
{
	const float Interp = static_cast<float>((InTime - Origin).AsDecimal() / DX);
	return UE::Curves::BezierInterp(P0, P1, P2, P3, Interp);
}

FWeightedCubicInterpolation::FWeightedCubicInterpolation(
	FFrameRate TickResolution,
	FFrameNumber InOrigin,

	FFrameNumber StartTime,
	double StartValue,
	double StartTangent,
	double StartTangentWeight,
	bool bStartIsWeighted,

	FFrameNumber EndTime,
	double EndValue,
	double EndTangent,
	double EndTangentWeight,
	bool bEndIsWeighted)
{
	constexpr double OneThird = 1.0 / 3.0;

	const float TimeInterval = TickResolution.AsInterval();
	const float ToSeconds = 1.0f / TimeInterval;

	const double Time1 = TickResolution.AsSeconds(StartTime);
	const double Time2 = TickResolution.AsSeconds(EndTime);
	const double DXInSeconds = Time2 - Time1;

	Origin = InOrigin;
	DX = (EndTime - StartTime).Value;
	StartKeyValue = StartValue;
	EndKeyValue = EndValue;

	double CosAngle, SinAngle, Angle;

	// ---------------------------------------------------------------------------------
	// Initialize the start key parameters
	Angle = FMath::Atan(StartTangent * ToSeconds);
	FMath::SinCos(&SinAngle, &CosAngle, Angle);

	if (bStartIsWeighted)
	{
		StartWeight = StartTangentWeight;
	}
	else
	{
		const double LeaveTangentNormalized = StartTangent / TimeInterval;
		const double DY = LeaveTangentNormalized * DXInSeconds;
		StartWeight = FMath::Sqrt(DXInSeconds*DXInSeconds + DY*DY) * OneThird;
	}

	const double StartKeyTanX = CosAngle * StartWeight + Time1;
	StartKeyTanY              = SinAngle * StartWeight + StartValue;
	NormalizedStartTanDX = (StartKeyTanX - Time1) / DXInSeconds;

	// ---------------------------------------------------------------------------------
	// Initialize the end key parameters
	Angle = FMath::Atan(EndTangent * ToSeconds);
	FMath::SinCos(&SinAngle, &CosAngle, Angle);

	if (bEndIsWeighted)
	{
		EndWeight =  EndTangentWeight;
	}
	else
	{
		const double ArriveTangentNormalized = EndTangent / TimeInterval;
		const double DY = ArriveTangentNormalized * DXInSeconds;
		EndWeight = FMath::Sqrt(DXInSeconds*DXInSeconds + DY*DY) * OneThird;
	}

	const double EndKeyTanX = -CosAngle * EndWeight + Time2;
	EndKeyTanY              = -SinAngle * EndWeight + EndValue;

	NormalizedEndTanDX = (EndKeyTanX - Time1) / DXInSeconds;
}

double FWeightedCubicInterpolation::Evaluate(FFrameTime InTime) const
{
	const double Interp = (InTime - Origin).AsDecimal() / DX;

	double Coeff[4];
	double Results[3];

	//Convert Bezier to Power basis, also float to double for precision for root finding.
	UE::Curves::BezierToPower(
		0.0, NormalizedStartTanDX, NormalizedEndTanDX, 1.0,
		&(Coeff[3]), &(Coeff[2]), &(Coeff[1]), &(Coeff[0])
	);

	Coeff[0] = Coeff[0] - Interp;

	const int32 NumResults = UE::Curves::SolveCubic(Coeff, Results);
	double NewInterp = Interp;
	if (NumResults == 1)
	{
		NewInterp = Results[0];
	}
	else
	{
		NewInterp = TNumericLimits<double>::Lowest(); //just need to be out of range
		for (double Result : Results)
		{
			if ((Result >= 0.0) && (Result <= 1.0))
			{
				if (NewInterp < 0.0 || Result > NewInterp)
				{
					NewInterp = Result;
				}
			}
		}

		if (NewInterp == TNumericLimits<double>::Lowest())
		{
			NewInterp = 0.0;
		}
	}

	//now use NewInterp and adjusted tangents plugged into the Y (Value) part of the graph.
	const double P0 = StartKeyValue;
	const double P1 = StartKeyTanY;
	const double P3 = EndKeyValue;
	const double P2 = EndKeyTanY;

	return UE::Curves::BezierInterp(P0, P1, P2, P3, static_cast<float>(NewInterp));
}

} // namespace UE::MovieScene
