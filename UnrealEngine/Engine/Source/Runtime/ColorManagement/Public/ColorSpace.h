// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ColorManagementDefines.h"
#include "Containers/StaticArray.h"
#include "CoreTypes.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/Archive.h"
#include "Templates/UnrealTypeTraits.h"

namespace UE { namespace Color {

/**
 * Convert coordinate to CIE Yxy with a luminance value.
 *
 * @return FVector3d
 */
FORCEINLINE FVector3d ToYxy(double LuminanceY, const FVector2d& Coordinate)
{
	return FVector3d(LuminanceY, Coordinate.X, Coordinate.Y);
}

/**
 * Convert coordinate to CIE Yxy with a default luminance of 1.0.
 *
 * @return FVector3d
 */
FORCEINLINE FVector3d ToYxy(const FVector2d& Coordinate)
{
	return ToYxy(1.0, Coordinate);
}

/**
 * Convert coordinate to CIE XYZ tristimulus values with a luminance value.
 * 
 * @return FVector3d
 */
FORCEINLINE FVector3d ToXYZ(double LuminanceY, const FVector2d& Coordinate)
{
	const FVector3d Yxy = ToYxy(LuminanceY, Coordinate);
	return FVector3d(
		Yxy[1] * Yxy[0] / FMath::Max(Yxy[2], 1e-10),
		Yxy[0],
		(1.0 - Yxy[1] - Yxy[2]) * Yxy[0] / FMath::Max(Yxy[2], 1e-10)
	);
}

/**
 * Convert coordinate to CIE XYZ tristimulus values with a default luminance of 1.0.
 *
 * @return FVector3d
 */
FORCEINLINE FVector3d ToXYZ(const FVector2d& Coordinate)
{
	return ToXYZ(1.0, Coordinate);
}

/**
 * Convenience function to get the transposed matrix, i.e. for pre-multiplied shader matrices.
 *
 * @return TMatrix<T>
 */
template<typename T>
FORCEINLINE UE::Math::TMatrix<T> Transpose(const FMatrix44d& Transform)
{
	if constexpr (TIsSame<T, double>::Value)
	{
		return Transform.GetTransposed();
	}

	return UE::Math::TMatrix<T>(Transform).GetTransposed();
}

/**
 * Get standard white point coordinates.
 *
 * @param InWhitePoint White point type.
 * @return FVector2d Chromaticity coordinates.
 */
FORCEINLINE FVector2d GetWhitePoint(EWhitePoint InWhitePoint)
{
	switch (InWhitePoint)
	{
	case UE::Color::EWhitePoint::CIE1931_D65:
		return FVector2d(0.3127, 0.3290);
	case UE::Color::EWhitePoint::ACES_D60:
		return FVector2d(0.32168, 0.33767);
	case UE::Color::EWhitePoint::DCI_CalibrationWhite:
		return FVector2d(0.314, 0.351);

	default:
		checkNoEntry();
		return FVector2d();
	}
}


/** Color space definition as 4 chromaticity coordinates, in double precision internally. */
class COLORMANAGEMENT_API FColorSpace
{
public:
	/**
	 * Get the global engine working color space (as a singleton).
	 * 
	 * @return FColorSpace working color space
	 */
	static const FColorSpace& GetWorking();

	/**
	 * Set the global engine working color space (as a singleton).
	 * 
	 * @param ColorSpace working color space
	 */
	static void SetWorking(FColorSpace ColorSpace);


	/** Constructor */
	FColorSpace() {}

	/**
	* Constructor
	*
	* @param InRed Chromaticity 2D coordinates for the red color.
	* @param InGreen Chromaticity 2D coordinates for the green color.
	* @param InBlue Chromaticity 2D coordinates for the blue color.
	* @param InWhite Chromaticity 2D coordinates for the white point.
	*/
	explicit FColorSpace(const FVector2d& InRed, const FVector2d& InGreen, const FVector2d& InBlue, const FVector2d& InWhite);

	/**
	* Constructor
	*
	* @param ColorSpaceType Color space type.
	*/
	explicit FColorSpace(UE::Color::EColorSpace ColorSpaceType);

	FColorSpace(FColorSpace&&) = default;
	FColorSpace(const FColorSpace&) = default;
	FColorSpace& operator=(FColorSpace&&) = default;
	FColorSpace& operator=(const FColorSpace&) = default;

	/**
	* Getter for the color space chromaticity coordinates.
	* 
	* @param OutRed FVector2d for the red color chromaticity coordinate.
	* @param OutGreen FVector2d for the green color chromaticity coordinate.
	* @param OutBlue FVector2d for the blue color chromaticity coordinate.
	* @param OutWhite FVector2d for the white color chromaticity coordinate.
	*/
	template<typename T>
	FORCEINLINE void GetChromaticities(UE::Math::TVector2<T>& OutRed, UE::Math::TVector2<T>& OutGreen, UE::Math::TVector2<T>& OutBlue, UE::Math::TVector2<T>& OutWhite) const
	{
		OutRed		= Chromaticities[0];
		OutGreen	= Chromaticities[1];
		OutBlue		= Chromaticities[2];
		OutWhite	= Chromaticities[3];
	}

	/**
	* Gets the color space's red chromaticity coordinates.
	*
	* @return FVector2d xy coordinates.
	*/
	FORCEINLINE const FVector2d& GetRedChromaticity() const
	{
		return Chromaticities[0];
	}

	/**
	* Gets the color space's green chromaticity coordinates.
	*
	* @return FVector2d xy coordinates.
	*/
	FORCEINLINE const FVector2d& GetGreenChromaticity() const
	{
		return Chromaticities[1];
	}

	/**
	* Gets the color space's blue chromaticity coordinates.
	*
	* @return FVector2d xy coordinates.
	*/
	FORCEINLINE const FVector2d& GetBlueChromaticity() const
	{
		return Chromaticities[2];
	}

	/**
	* Gets the color space's white point chromaticity coordinates.
	*
	* @return FVector2d xy coordinates.
	*/
	FORCEINLINE const FVector2d& GetWhiteChromaticity() const
	{
		return Chromaticities[3];
	}

	/**
	* Gets the RGB-to-XYZ conversion matrix.
	*
	* @return FMatrix conversion matrix.
	*/
	FORCEINLINE const FMatrix44d& GetRgbToXYZ() const
	{
		return RgbToXYZ;
	}

	/**
	* Gets the XYZ-to-RGB conversion matrix.
	*
	* @return FMatrix conversion matrix.
	*/
	FORCEINLINE const FMatrix44d& GetXYZToRgb() const
	{
		return XYZToRgb;
	}

	/**
	 * Check against another vector for equality.
	 *
	 * @param V The vector to check against.
	 * @return true if the vectors are equal, false otherwise.
	 */
	FORCEINLINE bool operator==(const FColorSpace& CS) const
	{
		return Chromaticities == CS.Chromaticities;
	}

	/**
	 * Check against another colorspace for inequality.
	 *
	 * @param V The vector to check against.
	 * @return true if the vectors are not equal, false otherwise.
	 */
	FORCEINLINE bool operator!=(const FColorSpace& CS) const
	{
		return Chromaticities != CS.Chromaticities;
	}

	/**
	 * Check against another colorspace for equality, within specified error limits.
	 *
	 * @param V The vector to check against.
	 * @param Tolerance Error tolerance.
	 * @return true if the vectors are equal within tolerance limits, false otherwise.
	 */
	bool Equals(const FColorSpace& CS, double Tolerance = 1.e-7) const;

	/**
	 * Convenience function to verify if the color space matches the engine's default sRGB chromaticities.
	 *
	 * @return true if sRGB.
	 */
	bool IsSRGB() const;

private:

	FMatrix44d CalcRgbToXYZ() const;

	/** Red, green, blue, white chromaticities, in order. */
	TStaticArray<FVector2d, 4> Chromaticities;

	FMatrix44d RgbToXYZ;
	FMatrix44d XYZToRgb;

	bool bIsSRGB;

public:

	/**
	 * Serializer.
	 *
	 * @param Ar The Serialization Archive.
	 * @param CS The Color Space being serialized.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FColorSpace& CS)
	{
		return Ar << CS.Chromaticities;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}
};


struct FColorSpaceTransform : FMatrix44d
{
	/**
	* Constructor: create a color space transformation matrix from a source to a target color space using the RGB-XYZ-RGB conversions.
	*
	* @param Source Source color space.
	* @param Target Target color space.
	* @param Method Chromatic adapation method.
	*/
	COLORMANAGEMENT_API explicit FColorSpaceTransform(const FColorSpace& Src, const FColorSpace& Dst, EChromaticAdaptationMethod Method);

	/**
	* Constructor: create a color space transformation matrix from a raw matrix.
	*
	* @param Matrix Color space transformation matrix.
	*/
	COLORMANAGEMENT_API explicit FColorSpaceTransform(FMatrix44d Matrix);

	/**
	* Apply color space transform to FLinearColor.
	* 
	* @param Color Color to transform.
	*/
	COLORMANAGEMENT_API FLinearColor Apply(const FLinearColor& Color) const;

	/**
	* Calculate the chromatic adaptation matrix using the specific method.
	*
	* @param SourceXYZ Source color in XYZ space.
	* @param TargetXYZ Target color in XYZ space.
	* @param Method Adaptation method (None, Bradford, CAT02).
	*/
	COLORMANAGEMENT_API static FMatrix44d CalcChromaticAdaptionMatrix(FVector3d SourceXYZ, FVector3d TargetXYZ, EChromaticAdaptationMethod Method);
};

} }  // end namespace UE::Color
