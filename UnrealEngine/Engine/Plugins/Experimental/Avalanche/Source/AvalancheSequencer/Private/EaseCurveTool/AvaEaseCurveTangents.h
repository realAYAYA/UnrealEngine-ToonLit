// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "AvaEaseCurveTangents.generated.h"

class FString;
class FText;
struct FFrameRate;

/** Represents a curve constructed of two tangents and their weights, one for each end of the curve. */
USTRUCT()
struct FAvaEaseCurveTangents
{
	GENERATED_BODY()

public:
	static bool FromString(const FString& InString, FAvaEaseCurveTangents& OutTangents);
	static bool CanParseString(const FString& InString);

	static FNumberFormattingOptions DefaultNumberFormattingOptions();

	static FAvaEaseCurveTangents Average(const TArray<FAvaEaseCurveTangents>& InTangentArray);

	FAvaEaseCurveTangents() {}
	FAvaEaseCurveTangents(const double InStart, const double InStartWeight, const double InEnd, const double InEndWeight)
		: Start(InStart), StartWeight(InStartWeight)
		, End(InEnd), EndWeight(InEndWeight)
	{}
	FAvaEaseCurveTangents(const FRichCurveKey& InRichCurveKey)
		: Start(InRichCurveKey.LeaveTangent), StartWeight(InRichCurveKey.LeaveTangentWeight)
		, End(InRichCurveKey.ArriveTangent), EndWeight(InRichCurveKey.ArriveTangentWeight)
	{}
	FAvaEaseCurveTangents(const FRichCurveKey& InStartRichCurveKey, const FRichCurveKey& InEndRichCurveKey)
		: Start(InStartRichCurveKey.LeaveTangent), StartWeight(InStartRichCurveKey.LeaveTangentWeight)
		, End(InEndRichCurveKey.ArriveTangent), EndWeight(InEndRichCurveKey.ArriveTangentWeight)
	{}
	FAvaEaseCurveTangents(const FMovieSceneDoubleValue& InMovieSceneDoubleValue)
		: Start(InMovieSceneDoubleValue.Tangent.LeaveTangent), StartWeight(InMovieSceneDoubleValue.Tangent.LeaveTangentWeight)
		, End(InMovieSceneDoubleValue.Tangent.ArriveTangent), EndWeight(InMovieSceneDoubleValue.Tangent.ArriveTangentWeight)
	{}
	FAvaEaseCurveTangents(const FMovieSceneDoubleValue& InStartMovieSceneDoubleValue, const FMovieSceneDoubleValue& InEndMovieSceneDoubleValue)
		: Start(InStartMovieSceneDoubleValue.Tangent.LeaveTangent), StartWeight(InStartMovieSceneDoubleValue.Tangent.LeaveTangentWeight)
		, End(InEndMovieSceneDoubleValue.Tangent.ArriveTangent), EndWeight(InEndMovieSceneDoubleValue.Tangent.ArriveTangentWeight)
	{}
	/** Constructor from string consisting of cubic bezier points. Ex. "0.45, 0.34, 0.0, 1.00" */
	explicit FAvaEaseCurveTangents(const FString& InTangentsString);

	FORCEINLINE bool operator==(const FAvaEaseCurveTangents& InRhs) const
	{
		return Start == InRhs.Start && StartWeight == InRhs.StartWeight && End == InRhs.End && EndWeight == InRhs.EndWeight;
	}
	FORCEINLINE bool operator!=(const FAvaEaseCurveTangents& InRhs) const
	{
		return !(*this == InRhs);
	}

	FText ToDisplayText() const;
	FString ToDisplayString() const;

	FString ToJson() const;

	FText GetStartTangentText() const;
	FText GetEndTangentText() const;
	FText GetCubicBezierText() const;

	bool FromCubicBezier(const TArray<double>& InPoints);
	bool ToCubicBezier(TArray<double>& OutPoints) const;

	void Normalize(const FFrameNumber& InFrameNumber, const double InValue
		, const FFrameNumber& InNextFrameNumber, const double InNextValue
		, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution);

	void ScaleUp(const FFrameNumber& InFrameNumber, const double InValue
		, const FFrameNumber& InNextFrameNumber, const double InNextValue
		, const FFrameRate& InDisplayRate, const FFrameRate& InTickResolution);

	/** Calculates the length of this curve. The higher the sample count, the higher the calculation precision. */
	double CalculateCurveLength(const int32 InSampleCount = 10) const;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTangents")
	double Start = 0.0;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTangents")
	double StartWeight = 0.0;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTangents")
	double End = 0.0;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurveTangents")
	double EndWeight = 0.0;
};
