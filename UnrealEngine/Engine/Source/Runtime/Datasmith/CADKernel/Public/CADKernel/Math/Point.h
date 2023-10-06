// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/MathConst.h"

// this will eventually be reconciled with what the Geometry team is using.
namespace UE::CADKernel
{

class FPoint2D;

class CADKERNEL_API FPoint
{
public:
	union
	{
		struct
		{
			double X;
			double Y;
			double Z;
		};

		UE_DEPRECATED(all, "For internal use only")
		double XYZ[3];
	};

public:

	/** A zero point (0,0,0)*/
	static const FPoint ZeroPoint;
	static const FPoint UnitPoint;
	static const FPoint FarawayPoint;
	static const int32 Dimension;

	explicit FPoint(double InCoordX = 0., double InCoordY = 0., double InCoordZ = 0.)
	{
		X = InCoordX; Y = InCoordY; Z = InCoordZ;
	}

	FPoint(const FPoint2D& InPoint2D);

	FPoint(const double* const InPoint)
		: X(InPoint[0])
		, Y(InPoint[1])
		, Z(InPoint[2])
	{
	}

	FPoint(const FPoint& Point)
		: X(Point.X)
		, Y(Point.Y)
		, Z(Point.Z)
	{
	}

	constexpr double& operator[](int32 Index)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	constexpr double operator[](int32 Index) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool operator==(const FPoint& Point) const
	{
		return FMath::IsNearlyEqual(X, Point.X) && FMath::IsNearlyEqual(Y, Point.Y) && FMath::IsNearlyEqual(Z, Point.Z);
	}

	bool operator!=(const FPoint& Point) const
	{
		return !(*this == Point);
	};

	FPoint operator+(const FPoint& Point) const
	{
		return FPoint(X + Point.X, Y + Point.Y, Z + Point.Z);
	}

	FPoint operator+(const FVector& Point) const
	{
		return FPoint(X + Point.X, Y + Point.Y, Z + Point.Z);
	}

	FPoint operator+(const FVector3f& Point) const
	{
		return FPoint(X + Point.X, Y + Point.Y, Z + Point.Z);
	}

	FPoint operator-(const FPoint& Point) const
	{
		return FPoint(X - Point.X, Y - Point.Y, Z - Point.Z);
	}

	FPoint operator^(const FPoint& Point) const
	{
		return FPoint(Y * Point.Z - Z * Point.Y, Z * Point.X - X * Point.Z, X * Point.Y - Y * Point.X);
	}

	double operator*(const FPoint& Point) const
	{
		return X * Point.X + Y * Point.Y + Z * Point.Z;
	}

	FPoint operator*(double Scale) const
	{
		return FPoint(X * Scale, Y * Scale, Z * Scale);
	}

	FPoint operator/(double InvScale) const
	{
		if (FMath::IsNearlyZero(InvScale))
		{
			return *this;
		}
		return FPoint(X / InvScale, Y / InvScale, Z / InvScale);
	}

	FPoint operator-() const
	{
		return FPoint(-X, -Y, -Z);
	}

	FPoint& operator=(const FPoint& Point)
	{
		X = Point.X;
		Y = Point.Y;
		Z = Point.Z;
		return *this;
	}

	FPoint& operator+=(const FPoint& Point)
	{
		X += Point.X;
		Y += Point.Y;
		Z += Point.Z;
		return *this;
	}

	FPoint& operator+=(const FVector& Point)
	{
		X += Point.X;
		Y += Point.Y;
		Z += Point.Z;
		return *this;
	}

	FPoint& operator+=(const FVector3f& Point)
	{
		X += Point.X;
		Y += Point.Y;
		Z += Point.Z;
		return *this;
	}

	FPoint& operator-=(const FPoint& Point)
	{
		X -= Point.X;
		Y -= Point.Y;
		Z -= Point.Z;
		return *this;
	}

	FPoint& operator*=(double Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		return *this;
	}

	void NonUniformScale(const FPoint& Scale)
	{
		X *= Scale.X;
		Y *= Scale.Y;
		Z *= Scale.Z;
	}

	FPoint& operator/=(double InvScale)
	{
		if (FMath::IsNearlyZero(InvScale))
		{
			return *this;
		}

		X /= InvScale;
		Y /= InvScale;
		Z /= InvScale;
		return *this;
	}

	bool operator<(const FPoint& Other) const
	{
		int32 Compare = RealCompare(X, Other.X);
		if (Compare < 0)
		{
			return true;
		}
		if (Compare > 0)
		{
			return false;
		}

		Compare = RealCompare(Y, Other.Y);
		if (Compare < 0)
		{
			return true;
		}
		if (Compare > 0)
		{
			return false;
		}

		Compare = RealCompare(Z, Other.Z);
		return (Compare < 0);
	}

	operator FVector() const
	{
		return FVector(X, Y, Z);
	}

	operator FVector3f() const
	{
		return FVector3f((float)X, (float)Y, (float)Z);
	}

	void SetMin(const FPoint& Point)
	{
		X = FMath::Min(X, Point.X);
		Y = FMath::Min(Y, Point.Y);
		Z = FMath::Min(Z, Point.Z);
	}

	void SetMax(const FPoint& Point)
	{
		X = FMath::Max(X, Point.X);
		Y = FMath::Max(Y, Point.Y);
		Z = FMath::Max(Z, Point.Z);
	}

	void Set(double InCoordX = 0., double InCoordY = 0., double InCoordZ = 0.)
	{
		X = InCoordX;
		Y = InCoordY;
		Z = InCoordZ;
	}

	void Set(double* InCoordinates)
	{
		X = InCoordinates[0];
		Y = InCoordinates[1];
		Z = InCoordinates[2];
	}

	double Length() const
	{
		return FMath::Sqrt((X * X) + (Y * Y) + (Z * Z));
	}

	double SquareLength() const
	{
		return (X * X) + (Y * Y) + (Z * Z);
	}

	FPoint& Normalize()
	{
		double Norm = Length();
		if (FMath::IsNearlyZero(Norm))
		{
			*this = FPoint::UnitPoint.Normalize();
		}
		else
		{
			*this /= Norm;
		}
		return *this;
	}

	FPoint Normalize() const 
	{
		double Norm = Length();
		if (FMath::IsNearlyZero(Norm))
		{
			return FPoint::UnitPoint.Normalize();
		}
		else
		{
			return *this / Norm;
		}
	}

	FPoint Middle(const FPoint& Point) const
	{
		return FPoint(0.5 * (X + Point.X), 0.5 * (Y + Point.Y), 0.5 * (Z + Point.Z));
	}

	double Distance(const FPoint& Point) const
	{
		return FMath::Sqrt(FMath::Square(Point.X - X) + FMath::Square(Point.Y - Y) + FMath::Square(Point.Z - Z));
	}

	double SquareDistance(const FPoint& Point) const
	{
		return FMath::Square(Point.X - X) + FMath::Square(Point.Y - Y) + FMath::Square(Point.Z - Z);
	}

	double ComputeCosinus(const FPoint& OtherVector) const
	{
		FPoint ThisNormalized = *this;
		FPoint OtherNormalized = OtherVector;

		ThisNormalized.Normalize();
		OtherNormalized.Normalize();

		double Cosinus = ThisNormalized * OtherNormalized;

		return FMath::Max(-1.0, FMath::Min(Cosinus, 1.0));
	}

	double ComputeSinus(const FPoint& OtherVector) const
	{
		FPoint ThisNormalized = *this;
		FPoint OtherNormalized = OtherVector;

		ThisNormalized.Normalize();
		OtherNormalized.Normalize();

		FPoint SinusPoint = ThisNormalized ^ OtherNormalized;
		double Sinus = SinusPoint.Length();
		return FMath::Min(Sinus, 1.0);
	}

	double ComputeAngle(const FPoint& OtherVector) const
	{
		double CosAngle = ComputeCosinus(OtherVector);
		return acos(CosAngle);
	}

	double SignedAngle(const FPoint& Vector2, const FPoint& Normal) const;

	/**
	 * Return the projection of the point on the diagonal axis (of vector (1,1,1))
	 * i.e. return X + Y + Z
	 */
	double DiagonalAxisCoordinate() const
	{
		return X + Y + Z;
	}

	static double MixedTripleProduct(const FPoint& VectorA, const FPoint& VectorB, const FPoint& VectorC)
	{
		return VectorA * (VectorB ^ VectorC);
	}

	friend FArchive& operator<<(FArchive& Ar, FPoint& Point)
	{
		Ar.Serialize(&Point, sizeof(FPoint));
		return Ar;
	}

	friend FPoint Abs(const FPoint& Point)
	{
		double PointX = FMath::Abs(Point.X);
		double PointY = FMath::Abs(Point.Y);
		double PointZ = FMath::Abs(Point.Z);
		return FPoint(PointX, PointY, PointZ);
	}

};

class CADKERNEL_API FPointH
{
public:
	union
	{
		struct
		{
			double X;
			double Y;
			double Z;
			double W;
		};

		UE_DEPRECATED(all, "For internal use only")
		double XYZW[4];
	};

public:
	/** A zero point (0,0,0)*/
	static const FPointH ZeroPoint;
	static const FPointH FarawayPoint;
	static const int32 Dimension;

	FPointH()
		: X(0.)
		, Y(0.)
		, Z(0.)
		, W(1.)
	{
	}

	FPointH(const FPoint& Point, double Weight)
		: X(Point.X* Weight)
		, Y(Point.Y* Weight)
		, Z(Point.Z* Weight)
		, W(Weight)
	{
	}

	FPointH(const double InX, const double InY, const double InZ, const double InW)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	constexpr double& operator[](int32 Index)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	constexpr double operator[](int32 Index) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	FPointH operator+(const FPointH& Point) const
	{
		return FPointH(X + Point.X, Y + Point.Y, Z + Point.Z, W + Point.W);
	}

	FPointH operator-(const FPointH& Point) const
	{
		return FPointH(X - Point.X, Y - Point.Y, Z - Point.Z, W - Point.W);
	}

	FPointH operator*(double Factor) const
	{
		return FPointH(X * Factor, Y * Factor, Z * Factor, W * Factor);
	}

	operator FPoint() const
	{
		return FPoint(X / W, Y / W, Z / W);
	}

	friend FArchive& operator<<(FArchive& Ar, FPointH& Point)
	{
		Ar.Serialize(&Point, sizeof(FPointH));
		return Ar;
	}
};

class CADKERNEL_API FPoint2D
{
public:
	union
	{
		struct
		{
			double U;
			double V;
		};

		UE_DEPRECATED(all, "For internal use only")
		double UV[2];
	};


public:

	/** A zero point (0,0,0)*/
	static const FPoint2D ZeroPoint;
	static const FPoint2D FarawayPoint;
	static const int32 Dimension;

	FPoint2D()
	{
		U = V = 0.0;
	}

	FPoint2D(double NewU, double NewV)
		: U(NewU)
		, V(NewV)
	{
	}

	FPoint2D(const FPoint& Point)
		: U(Point.X)
		, V(Point.Y)
	{
	}

	FPoint2D(const FPoint2D& Point)
		: U(Point.U)
		, V(Point.V)
	{
	}

	FPoint2D& operator=(const FPoint2D& Point)
	{
		U = Point.U;
		V = Point.V;
		return *this;
	}

	FPoint2D Rotate(double Theta)
	{
		return FPoint2D(U * cos(Theta) - V * sin(Theta), U * sin(Theta) + V * cos(Theta));
	}

	FPoint2D Middle(const FPoint2D& Point) const
	{
		return FPoint(0.5 * (U + Point.U), 0.5 * (V + Point.V));
	}

	double Distance(const FPoint2D& Point) const
	{
		return FMath::Sqrt(SquareDistance(Point));
	}

	double SquareDistance(const FPoint2D& Point) const
	{
		return FMath::Square(Point.U - U) + FMath::Square(Point.V - V);
	}

	double Length() const
	{
		return FMath::Sqrt(SquareLength());
	}

	double SquareLength() const
	{
		return FMath::Square(U) + FMath::Square(V);
	}

	FPoint2D& Normalize(double& Norm)
	{
		Norm = Length();
		if (FMath::IsNearlyZero(Norm))
		{
			U = V = 0.0;
		}
		else
		{
			U /= Norm;
			V /= Norm;
		}
		return *this;
	}

	FPoint2D& Normalize()
	{
		double Norm = 0;
		return Normalize(Norm);
	}

	constexpr double& operator[](int32 Index)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return UV[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	constexpr double operator[](int32 Index) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return UV[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool operator==(const FPoint2D& Point) const
	{
		return FMath::IsNearlyEqual(U, Point.U) && FMath::IsNearlyEqual(V, Point.V);
	}

	bool operator<(const FPoint2D& other) const
	{
		int32 cmp = RealCompare(U, other.U);
		if (cmp < 0)
		{
			return true;
		}
		if (cmp > 0)
		{
			return false;
		}

		cmp = RealCompare(V, other.V);
		return (cmp < 0);
	}

	void Set(double NewU, double NewV = 0.)
	{
		U = NewU;
		V = NewV;
	}

	void Set(double* NewCoordinate)
	{
		U = NewCoordinate[0];
		V = NewCoordinate[1];
	}

	FPoint2D operator+(const FPoint2D& Point) const
	{
		return FPoint2D(U + Point.U, V + Point.V);
	}

	FPoint2D operator-(const FPoint2D& Point) const
	{
		return FPoint2D(U - Point.U, V - Point.V);
	}

	double operator*(const FPoint2D& Point) const
	{
		return U * Point.U + V * Point.V;
	}

	double operator^(const FPoint2D& Point) const
	{
		return (U * Point.V - V * Point.U);
	}

	FPoint2D operator*(double Scale) const
	{
		return FPoint2D(U * Scale, V * Scale);
	}

	FPoint2D operator/(double InvScale) const
	{
		if (FMath::IsNearlyZero(InvScale))
		{
			return *this;
		}

		return FPoint2D(U / InvScale, V / InvScale);
	}

	FPoint2D operator-() const
	{
		return FPoint2D(-U, -V);
	}

	FPoint2D& operator+=(const FPoint2D& Point)
	{
		U += Point.U;
		V += Point.V;
		return *this;
	}

	FPoint2D& operator-=(const FPoint2D& Point)
	{
		U -= Point.U;
		V -= Point.V;
		return *this;
	}

	FPoint2D& operator*=(double Scale)
	{
		U *= Scale;
		V *= Scale;
		return *this;
	}

	FPoint2D& operator/=(double InvScale)
	{
		if (FMath::IsNearlyZero(InvScale))
		{
			return *this;
		}

		U /= InvScale;
		V /= InvScale;
		return *this;
	}

	double ComputeCosinus(const FPoint2D& OtherVector) const
	{
		FPoint2D ThisNormalized = *this;
		FPoint2D OtherNormalized = OtherVector;

		ThisNormalized.Normalize();
		OtherNormalized.Normalize();

		double Cosinus = ThisNormalized * OtherNormalized;

		return FMath::Max(-1.0, FMath::Min(Cosinus, 1.0));
	}

	FPoint2D GetPerpendicularVector()
	{
		return FPoint2D(-V, U);
	}

	/**
	 * Return the projection of the point on the diagonal axis (of vector (1,1))
	 * i.e. return U + V
	 */
	double DiagonalAxisCoordinate() const
	{
		return U + V;
	}

	friend FArchive& operator<<(FArchive& Ar, FPoint2D& Point)
	{
		Ar.Serialize(&Point, sizeof(FPoint2D));
		return Ar;
	}

	friend FPoint2D Abs(const FPoint2D& Point)
	{
		double PointU = FMath::Abs(Point.U);
		double PointV = FMath::Abs(Point.V);
		return { PointU, PointV };
	}

	friend FPoint2D Max(const FPoint2D& PointA, const FPoint2D& PointB)
	{
		return { FMath::Max(PointA.U, PointB.U), FMath::Max(PointA.V, PointB.V) };
	}
};

inline FPoint::FPoint(const FPoint2D& Point)
{
	X = Point.U; Y = Point.V; Z = 0.0;
}

typedef FPoint2D FSurfacicTolerance;

class CADKERNEL_API FFPoint
{
public:
	union
	{
		struct
		{
			float X;
			float Y;
			float Z;
		};

		UE_DEPRECATED(all, "For internal use only")
		float XYZ[3];
	};

	/** A zero point (0,0,0)*/
	static const FFPoint ZeroPoint;
	static const FFPoint FarawayPoint;
	static const int32 Dimension;

	FFPoint(float InX = 0., float InY = 0., float InZ = 0.)
	{
		X = InX;
		Y = InY;
		Z = InZ;
	}

	FFPoint(const FPoint& Point)
	{
		X = (float)Point.X;
		Y = (float)Point.Y;
		Z = (float)Point.Z;
	}

	constexpr float& operator[](int32 Index)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	constexpr float operator[](int32 Index) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[Index];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	FFPoint& operator=(const FPoint& Point)
	{
		X = (float)Point.X; Y = (float)Point.Y; Z = (float)Point.Z; return *this;
	}

	friend FArchive& operator<<(FArchive& Ar, FFPoint& Point)
	{
		Ar.Serialize(&Point, sizeof(FFPoint));
		return Ar;
	}
};

inline FPoint operator*(double Scale, const FPoint& Point)
{
	return FPoint(Point.X * Scale, Point.Y * Scale, Point.Z * Scale);
}

inline FPoint operator*(float Scale, const FPoint& Point)
{
	double ScaleD = static_cast<double>(Scale);
	return FPoint(Point.X * ScaleD, Point.Y * ScaleD, Point.Z * ScaleD);
}

inline FPoint2D operator*(double Scale, const FPoint2D& Point)
{
	return FPoint2D(Point.U * Scale, Point.V * Scale);
}

inline FPoint2D operator*(float Scale, const FPoint2D& Point)
{
	double ScaleD = static_cast<double>(Scale);
	return FPoint2D(Point.U * ScaleD, Point.V * ScaleD);
}

}

inline uint32 GetTypeHash(const UE::CADKernel::FPoint& Point)
{
	return HashCombine(GetTypeHash(Point.X), HashCombine(GetTypeHash(Point.Y), GetTypeHash(Point.Z)));
}

inline uint32 GetTypeHash(const UE::CADKernel::FPoint2D& Point)
{
	return HashCombine(GetTypeHash(Point.U), GetTypeHash(Point.V));
}

inline uint32 GetTypeHash(const UE::CADKernel::FPointH& Point)
{
	return HashCombine(GetTypeHash(Point.X), HashCombine(GetTypeHash(Point.Y), HashCombine(GetTypeHash(Point.Z), GetTypeHash(Point.W))));
}

