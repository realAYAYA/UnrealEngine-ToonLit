// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveTangents.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveEditor.h"
#include "Internationalization/Text.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"

#define LOCTEXT_NAMESPACE "AvaEaseCurveTangents"

bool FAvaEaseCurveTangents::FromString(const FString& InString, FAvaEaseCurveTangents& OutTangents)
{
	const FString TangentsString = InString.Replace(TEXT(" "), TEXT(""));

	TArray<FString> ValueStrings;
	if (TangentsString.ParseIntoArray(ValueStrings, TEXT(",")) != 4)
	{
		return false;
	}

	// Convert strings to doubles
	TArray<double> Values = { 0.0, 0.0, 0.0, 0.0 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!FDefaultValueHelper::ParseDouble(ValueStrings[Index], Values[Index]))
		{
			return false;
		}
	}

	// Convert four cubic bezier points to two points/tangents
	OutTangents.FromCubicBezier(Values);

	return true;
}

bool FAvaEaseCurveTangents::CanParseString(const FString& InString)
{
	FAvaEaseCurveTangents Tangents;
	return FromString(InString, Tangents);
}

FNumberFormattingOptions FAvaEaseCurveTangents::DefaultNumberFormattingOptions()
{
	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumIntegralDigits = 1;
	NumberFormat.MinimumFractionalDigits = 2;
	NumberFormat.MaximumFractionalDigits = 2;
	NumberFormat.UseGrouping = false;
	return NumberFormat;
}

FAvaEaseCurveTangents::FAvaEaseCurveTangents(const FString& InTangentsString)
{
	FAvaEaseCurveTangents::FromString(InTangentsString, *this);
}

FText FAvaEaseCurveTangents::ToDisplayText() const
{
	const FNumberFormattingOptions NumberFormat = DefaultNumberFormattingOptions();
	const FText StartTangentText = FText::AsNumber(Start, &NumberFormat);
	const FText StartTangentWeightText = FText::AsNumber(StartWeight, &NumberFormat);
	const FText EndTangentText = FText::AsNumber(End, &NumberFormat);
	const FText EndTangentWeightText = FText::AsNumber(EndWeight, &NumberFormat);
	return FText::Format(LOCTEXT("TangentText", "{0}, {1} - {2}, {3}")
		, { StartTangentText, StartTangentWeightText, EndTangentText, EndTangentWeightText });
}

FString FAvaEaseCurveTangents::ToDisplayString() const
{
	return ToDisplayText().ToString();
}

FString FAvaEaseCurveTangents::ToJson() const
{
	// Convert two points/tangents to four cubic bezier points 
	TArray<double> CubicBezierPoints;
	ToCubicBezier(CubicBezierPoints);

	const FNumberFormattingOptions NumberFormat = DefaultNumberFormattingOptions();
	const FText PointAText = FText::AsNumber(CubicBezierPoints[0], &NumberFormat);
	const FText PointBText = FText::AsNumber(CubicBezierPoints[1], &NumberFormat);
	const FText PointCText = FText::AsNumber(CubicBezierPoints[2], &NumberFormat);
	const FText PointDText = FText::AsNumber(CubicBezierPoints[3], &NumberFormat);

	return FString::Printf(TEXT("%s, %s, %s, %s")
		, *PointAText.ToString(), *PointBText.ToString(), *PointCText.ToString(), *PointDText.ToString());
}

FText FAvaEaseCurveTangents::GetStartTangentText() const
{
	const FNumberFormattingOptions NumberFormat = DefaultNumberFormattingOptions();
	const FText StartTangentText = FText::AsNumber(Start, &NumberFormat);
	const FText StartTangentWeightText = FText::AsNumber(StartWeight, &NumberFormat);
	return FText::Format(LOCTEXT("StartTangentText", "Start: {0}, {1}"), { StartTangentText, StartTangentWeightText });
}

FText FAvaEaseCurveTangents::GetEndTangentText() const
{
	const FNumberFormattingOptions NumberFormat = DefaultNumberFormattingOptions();
	const FText EndTangentText = FText::AsNumber(End, &NumberFormat);
	const FText EndTangentWeightText = FText::AsNumber(EndWeight, &NumberFormat);
	return FText::Format(LOCTEXT("EndTangentText", "End: {0}, {1}"), { EndTangentText, EndTangentWeightText });
}

FText FAvaEaseCurveTangents::GetCubicBezierText() const
{
	return FText::FromString(ToJson());
}

bool FAvaEaseCurveTangents::FromCubicBezier(const TArray<double>& InPoints)
{
	if (InPoints.Num() != 4)
	{
		return false;
	}

	const FVector2d StartDir(InPoints[0], InPoints[1]);
	const FVector2d EndDir(1.0 - InPoints[2], 1.0 - InPoints[3]);

	Start = SAvaEaseCurveEditor::CalcTangent(StartDir);
	End = SAvaEaseCurveEditor::CalcTangent(EndDir);

	StartWeight = StartDir.Size();
	EndWeight = EndDir.Size();

	return true;
}

bool FAvaEaseCurveTangents::ToCubicBezier(TArray<double>& OutPoints) const
{
	const FVector2d StartDir = SAvaEaseCurveEditor::CalcTangentDir(Start) * StartWeight;
	const FVector2d EndDir = SAvaEaseCurveEditor::CalcTangentDir(End) * EndWeight;

	OutPoints.Reset(4);
	OutPoints.Add(StartDir.X);
	OutPoints.Add(StartDir.Y);
	OutPoints.Add(1.0 - EndDir.X);
	OutPoints.Add(1.0 - EndDir.Y);

	return true;
}

void FAvaEaseCurveTangents::Normalize(const FFrameNumber& InFrameNumber, const double InValue
	, const FFrameNumber& InNextFrameNumber, const double InNextValue
	, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution)
{
	// Convert frame time from tick resolution to display rate
	const FFrameTime FrameTimeDifference = InNextFrameNumber - InFrameNumber;
	const FQualifiedFrameTime QualifiedFrameTime(FrameTimeDifference, InTickResolution);
	const FFrameTime ConvertedFrameTime = QualifiedFrameTime.ConvertTo(InDisplayRate);

	// Create time/value range scale factor
	FVector2d TimeValueRange;
	TimeValueRange.X = InDisplayRate.AsSeconds(ConvertedFrameTime);
	TimeValueRange.Y = FMath::Abs(InNextValue - InValue);

	const double ScaleFactor = InTickResolution.AsDecimal();

	// Convert tangent angles to grid coordinates
	FVector2d StartDir = SAvaEaseCurveEditor::CalcTangentDir(Start * ScaleFactor);
	FVector2d EndDir = SAvaEaseCurveEditor::CalcTangentDir(End * ScaleFactor);

	StartDir *= StartWeight;
	EndDir *= EndWeight;

	// Scale down tangent grid coordinates to normalized range of InTimeValueRange
	auto SafeDivide = [](const double InNumerator, const double InDenominator)
		{
			return FMath::IsNearlyZero(InDenominator) ? InNumerator : (InNumerator / InDenominator);
		};
	StartDir.X = SafeDivide(StartDir.X, TimeValueRange.X);
	StartDir.Y = SafeDivide(StartDir.Y, TimeValueRange.Y);
	EndDir.X = SafeDivide(EndDir.X, TimeValueRange.X);
	EndDir.Y = SafeDivide(EndDir.Y, TimeValueRange.Y);

	// Set new weights and tangents
	StartWeight = StartDir.Size();
	EndWeight = EndDir.Size();

	StartDir.Normalize();
	EndDir.Normalize();

	Start = SAvaEaseCurveEditor::CalcTangent(StartDir);
	End = SAvaEaseCurveEditor::CalcTangent(EndDir);
}

void FAvaEaseCurveTangents::ScaleUp(const FFrameNumber& InFrameNumber, const double InValue
	, const FFrameNumber& InNextFrameNumber, const double InNextValue
	, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution)
{
	// Convert frame time from tick resolution to display rate
	const FFrameTime FrameTimeDifference = InNextFrameNumber - InFrameNumber;
	const FQualifiedFrameTime QualifiedFrameTime(FrameTimeDifference, InTickResolution);
	const FFrameTime ConvertedFrameTime = QualifiedFrameTime.ConvertTo(InDisplayRate);

	// Create time/value range scale factor
	FVector2d TimeValueRange;
	TimeValueRange.X = InDisplayRate.AsSeconds(ConvertedFrameTime);
	TimeValueRange.Y = FMath::Abs(InNextValue - InValue);

	FVector2d StartDir = SAvaEaseCurveEditor::CalcTangentDir(Start);
	FVector2d EndDir = SAvaEaseCurveEditor::CalcTangentDir(End);

	StartDir *= StartWeight;
	EndDir *= EndWeight;

	StartDir *= TimeValueRange;
	EndDir *= TimeValueRange;

	// Set new weights
	StartWeight = StartDir.Size();
	EndWeight = EndDir.Size();

	// Normalize vector and set new tangents
	FVector2d NormalizedStartDir = StartDir;
	FVector2d NormalizedEndDir = EndDir;

	const double ScaleFactor = InTickResolution.AsDecimal();
	NormalizedStartDir.X *= ScaleFactor;
	NormalizedEndDir.X *= ScaleFactor;

	NormalizedStartDir.Normalize();
	NormalizedEndDir.Normalize();

	Start = SAvaEaseCurveEditor::CalcTangent(NormalizedStartDir);
	End = SAvaEaseCurveEditor::CalcTangent(NormalizedEndDir);
}

FAvaEaseCurveTangents FAvaEaseCurveTangents::Average(const TArray<FAvaEaseCurveTangents>& InTangentArray)
{
	const int32 KeySetTangentsCount = InTangentArray.Num();
	if (KeySetTangentsCount == 0)
	{
		return FAvaEaseCurveTangents();
	}

	double TotalStartTangent = 0.0;
	double TotalStartWeight = 0.0;
	double TotalEndTangent = 0.0;
	double TotalEndWeight = 0.0;

	for (const FAvaEaseCurveTangents& Tangent : InTangentArray)
	{
		TotalStartTangent += Tangent.Start;
		TotalStartWeight += Tangent.StartWeight;
		TotalEndTangent += Tangent.End;
		TotalEndWeight += Tangent.EndWeight;
	}

	FAvaEaseCurveTangents AverageTangents;
	AverageTangents.Start = TotalStartTangent / KeySetTangentsCount;
	AverageTangents.StartWeight = TotalStartWeight / KeySetTangentsCount;
	AverageTangents.End = TotalEndTangent / KeySetTangentsCount;
	AverageTangents.EndWeight = TotalEndWeight / KeySetTangentsCount;

	return AverageTangents;
}

double FAvaEaseCurveTangents::CalculateCurveLength(const int32 SampleCount) const
{
	TArray<double> CubicBezierPoints;
	ToCubicBezier(CubicBezierPoints);

	const TArray<FVector2d> Points = { 
			FVector2d(0.0, 0.0), 
			FVector2d(CubicBezierPoints[0], CubicBezierPoints[1]), 
			FVector2d(CubicBezierPoints[2], CubicBezierPoints[3]), 
			FVector2d(1.0, 1.0)
		};

	auto EvaluatePoint = [](const double InTime, double InStart, double InControlA, double InControlB, double InEnd) -> double
		{
			return InStart * (1.0 - InTime) * (1.0 - InTime) * (1.0 - InTime)
				+ 3.0 * InControlA * (1.0 - InTime) * (1.0 - InTime) * InTime
				+ 3.0 * InControlB * (1.0 - InTime) * InTime * InTime
				+ InEnd * InTime * InTime * InTime;
		};

	// Sample points along the curve and use those points to calculate the length of the curve
	FVector2d SamplePoint;
	FVector2d PreviousSamplePoint;
	double Length = 0.0;

	for (int32 Index = 0; Index <= SampleCount; Index++)
	{
		const double Time = (double)Index / (double)SampleCount;
				
		SamplePoint.X = EvaluatePoint(Time, Points[0].X, Points[1].X, Points[2].X, Points[3].X);
		SamplePoint.Y = EvaluatePoint(Time, Points[0].Y, Points[1].Y, Points[2].Y, Points[3].Y);

		if (Index > 0)
		{
			const double DifferenceX = SamplePoint.X - PreviousSamplePoint.X;
			const double DifferenceY = SamplePoint.Y - PreviousSamplePoint.Y;
			Length += FMath::Sqrt(DifferenceX * DifferenceX + DifferenceY * DifferenceY);
		}

		PreviousSamplePoint = SamplePoint;
	}

	return Length;
}

#undef LOCTEXT_NAMESPACE
