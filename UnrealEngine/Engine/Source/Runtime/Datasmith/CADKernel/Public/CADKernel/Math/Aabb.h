// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Math/Point.h"


enum EAABBBoundary : uint32
{
	XMax = 0x00000000u,
	YMax = 0x00000000u,
	ZMax = 0x00000000u,
	XMin = 0x00000001u,
	YMin = 0x00000002u,
	ZMin = 0x00000004u,
};

ENUM_CLASS_FLAGS(EAABBBoundary);

namespace UE::CADKernel
{
template<class PointType>
class TAABB
{

protected:
	PointType MinCorner;
	PointType MaxCorner;

public:
	TAABB()
		: MinCorner(PointType::FarawayPoint)
		, MaxCorner(-PointType::FarawayPoint)
	{
	}

	TAABB(const PointType& InMinCorner, const PointType& InMaxCorner)
		: MinCorner(InMinCorner)
		, MaxCorner(InMaxCorner)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, TAABB& AABB)
	{
		Ar << AABB.MinCorner;
		Ar << AABB.MaxCorner;
		return Ar;
	}

	bool IsValid() const
	{
		for (int32 Axis = 0; Axis < PointType::Dimension; Axis++)
		{
			if (MinCorner[Axis] > MaxCorner[Axis])
			{
				return false;
			}
		}
		return true;
	}

	void Empty()
	{
		MinCorner = PointType::FarawayPoint;
		MaxCorner = -PointType::FarawayPoint;
	}

	bool Contains(const PointType& Point) const
	{
		for (int32 Axis = 0; Axis < PointType::Dimension; Axis++)
		{
			if ((Point[Axis] < MinCorner[Axis]) || (Point[Axis] > MaxCorner[Axis]))
			{
				return false;
			}
		}
		return true;
	}

	void SetMinSize(double MinSize)
	{
		for (int32 Axis = 0; Axis < PointType::Dimension; Axis++)
		{
			double AxisSize = GetSize(Axis);
			if (AxisSize < MinSize)
			{
				double Offset = (MinSize - AxisSize) / 2;
				MinCorner[Axis] -= Offset;
				MaxCorner[Axis] += Offset;
			}
		}
	}

	double GetMaxSize() const
	{
		double MaxSideSize = 0;
		for (int32 Index = 0; Index < PointType::Dimension; Index++)
		{
			double Size = GetSize(Index);
			if (Size > MaxSideSize)
			{
				MaxSideSize = Size;
			}
		}
		return MaxSideSize;
	}

	double GetSize(int32 Axis) const
	{
		return MaxCorner[Axis] - MinCorner[Axis];
	}

	double DiagonalLength() const
	{
		return MaxCorner.Distance(MinCorner);
	}

	PointType Diagonal() const
	{
		return MaxCorner - MinCorner;
	}

	bool Contains(const TAABB& Aabb) const
	{
		return IsValid() && Aabb.IsValid() && Contains(Aabb.MinCorner) && Contains(Aabb.MaxCorner);
	}

	const PointType& GetMin() const
	{
		return MinCorner;
	}

	const PointType& GetMax() const
	{
		return MaxCorner;
	}

	TAABB& operator+= (const double* Point)
	{
		for (int32 Index = 0; Index < PointType::Dimension; Index++)
		{
			if (Point[Index] < MinCorner[Index])
			{
				MinCorner[Index] = Point[Index];
			}
			if (Point[Index] > MaxCorner[Index])
			{
				MaxCorner[Index] = Point[Index];
			}
		}
		return *this;
	}

	TAABB& operator+= (const PointType& Point)
	{
		for (int32 Index = 0; Index < PointType::Dimension; Index++)
		{
			if (Point[Index] < MinCorner[Index])
			{
				MinCorner[Index] = Point[Index];
			}
			if (Point[Index] > MaxCorner[Index])
			{
				MaxCorner[Index] = Point[Index];
			}
		}
		return *this;
	}


	TAABB& operator+= (const TArray<PointType>& Points)
	{
		for (const PointType& Point : Points)
		{
			*this += Point;
		}
		return *this;
	}


	void Offset(double Offset)
	{
		for (int32 Index = 0; Index < PointType::Dimension; Index++)
		{
			MinCorner[Index] -= Offset;
			MaxCorner[Index] += Offset;
		}
	}

	TAABB& operator+= (const TAABB& aabb)
	{
		*this += aabb.MinCorner;
		*this += aabb.MaxCorner;
		return *this;
	}

	TAABB operator+ (const PointType& Point) const
	{
		TAABB Other = *this;
		Other += Point;
		return Other;
	}


	TAABB operator+ (const TAABB& Aabb) const
	{
		TAABB Other = *this;
		Other += Aabb;
		return Other;
	}
};

class FAABB : public TAABB<FPoint>
{

public:
	FAABB()
		: TAABB<FPoint>()
	{
	}

	FAABB(const FPoint& InMinCorner, const FPoint& InMaxCorner)
		: TAABB<FPoint>(InMinCorner, InMaxCorner)
	{
	}

	FPoint GetCorner(int32 Corner) const
	{
		return FPoint(
			Corner & EAABBBoundary::XMin ? MinCorner[0] : MaxCorner[0],
			Corner & EAABBBoundary::YMin ? MinCorner[1] : MaxCorner[1],
			Corner & EAABBBoundary::ZMin ? MinCorner[2] : MaxCorner[2]
		);
	}
};

class CADKERNEL_API FAABB2D : public TAABB<FPoint2D>
{
public:
	FAABB2D()
		: TAABB<FPoint2D>()
	{
	}

	FAABB2D(const FPoint& InMinCorner, const FPoint& InMaxCorner)
		: TAABB<FPoint2D>(InMinCorner, InMaxCorner)
	{
	}

	FPoint2D GetCorner(int32 CornerIndex) const
	{
		return FPoint2D(
			CornerIndex & EAABBBoundary::XMin ? MinCorner[0] : MaxCorner[0],
			CornerIndex & EAABBBoundary::YMin ? MinCorner[1] : MaxCorner[1]
		);
	}

};

} // namespace UE::CADKernel

