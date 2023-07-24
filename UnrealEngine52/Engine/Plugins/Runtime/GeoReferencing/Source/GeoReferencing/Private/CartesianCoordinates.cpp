// Copyright Epic Games, Inc. All Rights Reserved.

#include "CartesianCoordinates.h"

FCartesianCoordinates::FCartesianCoordinates()
	: X(0)
	, Y(0)
	, Z(0)
{

}

FCartesianCoordinates::FCartesianCoordinates(double InX, double InY, double InZ)
	: X(InX)
	, Y(InY)
	, Z(InZ)
{

}

FCartesianCoordinates::FCartesianCoordinates(const FVector& Coordinates)
	: X(Coordinates.X)
	, Y(Coordinates.Y)
	, Z(Coordinates.Z)
{
}

FCartesianCoordinates::FCartesianCoordinates(const FVector4d& Coordinates)
	: X(Coordinates.X)
	, Y(Coordinates.Y)
	, Z(Coordinates.Z)
{
}

FText FCartesianCoordinates::ToFullText(int32 IntegralDigits /*= 3*/)
{
	FNumberFormattingOptions NumberFormatOptions = FNumberFormattingOptions::DefaultNoGrouping();
	NumberFormatOptions.MinimumFractionalDigits = IntegralDigits;
	NumberFormatOptions.MaximumFractionalDigits = IntegralDigits;

	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), FText::AsNumber(X, &NumberFormatOptions));
	Args.Add(TEXT("Y"), FText::AsNumber(Y, &NumberFormatOptions));
	Args.Add(TEXT("Z"), FText::AsNumber(Z, &NumberFormatOptions));

	return FText::Format(NSLOCTEXT("GeoReferencing", "CartesianCoordinatesFull", "Easting={X} Northing={Y} Up={Z}"), Args);
}

FText FCartesianCoordinates::ToCompactText(int32 IntegralDigits /*= 3*/)
{
	FNumberFormattingOptions NumberFormatOptions = FNumberFormattingOptions::DefaultNoGrouping();
	NumberFormatOptions.MinimumFractionalDigits = IntegralDigits;
	NumberFormatOptions.MaximumFractionalDigits = IntegralDigits;

	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), FText::AsNumber(X, &NumberFormatOptions));
	Args.Add(TEXT("Y"), FText::AsNumber(Y, &NumberFormatOptions));
	Args.Add(TEXT("Z"), FText::AsNumber(Z, &NumberFormatOptions));

	return FText::Format(NSLOCTEXT("GeoReferencing", "CartesianCoordinatesCompact", "({X}, {Y}, {Z}m )"), Args);
}

void FCartesianCoordinates::ToSeparateTexts(FText& OutX, FText& OutY, FText& OutZ, int32 IntegralDigits /*= 3*/)
{
	FNumberFormattingOptions NumberFormatOptions = FNumberFormattingOptions::DefaultNoGrouping();
	NumberFormatOptions.MinimumFractionalDigits = IntegralDigits;
	NumberFormatOptions.MaximumFractionalDigits = IntegralDigits;

	OutX = FText::AsNumber(X, &NumberFormatOptions);
	OutY = FText::AsNumber(Y, &NumberFormatOptions);
	OutZ = FText::AsNumber(Z, &NumberFormatOptions);
}

void FCartesianCoordinates::ToFloatApproximation(float& OutX, float& OutY, float& OutZ)
{
	OutX = static_cast<float>(X);
	OutY = static_cast<float>(Y);
	OutZ = static_cast<float>(Z);
}

FVector FCartesianCoordinates::ToVector() const
{
	return FVector(X, Y, Z);
}
