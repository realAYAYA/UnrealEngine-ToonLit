// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector2D.h"

namespace Audio
{
	// A simple curve class that takes FVector2D points
	// Required to decouple from Engine module
	class SIGNALPROCESSING_API FLinearCurve
	{
	public:
		FLinearCurve() = default;
		FLinearCurve(const TArray<FVector2D>& InCurve);
		FLinearCurve(TArray<FVector2D>&& InCurve);

		virtual ~FLinearCurve() = default;

		// Sets curve array
		void SetCurvePoints(const TArray<FVector2D>& InPoints);
		void SetCurvePoints(TArray<FVector2D>&& InPoints);

		// Clears the points array
		void ClearPoints();

		// Returns the number of points in the curve
		int32 Num() const { return Points.Num(); }

		// Adds individual points
		void AddPoint(const FVector2D& InPoint);
		void AddPoints(const TArray<FVector2D>& InPoints);

		// Retrieves curve array
		const TArray<FVector2D>& GetCurvePoints() const;

		// Evaluates the curve with the given domain. 
		// Returns false if the curve was incapable of evaluation (i.e. empty).
		// Will clamp the output to the values of the domain
		bool Eval(float InDomain, float& OutValue) const;

	private:

		// Will sort the curve according to increasing domain values
		void Sort();
		TArray<FVector2D> Points;
	};
}