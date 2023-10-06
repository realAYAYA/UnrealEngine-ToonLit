// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Mesh/Meshers/IsoTriangulator/IsoSegment.h"
#include "CADKernel/Math/Boundary.h"
#include "CADKernel/UI/Visu.h"

namespace UE::CADKernel
{
class FGrid;
class FIsoNode;
class FPoint2D;

namespace IntersectionIsoSegmentTool
{

class FIntersectionIsoSegment
{
private:
	const FPoint2D& Point0;
	const FPoint2D& Point1;
	double IsoCoordinate;
	double MinCoordinate;
	double MaxCoordinate;

public:
	
	FIntersectionIsoSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint, const double InIsoCoordinate, const double InStartCoordinate, const double InEndCoordinate)
		: Point0(StartPoint)
		, Point1(EndPoint)
		, IsoCoordinate(InIsoCoordinate)
		, MinCoordinate(FMath::Min(InStartCoordinate, InEndCoordinate))
		, MaxCoordinate(FMath::Max(InStartCoordinate, InEndCoordinate))
	{}

	virtual ~FIntersectionIsoSegment()
	{}

	virtual FPoint2D GetMinPoint() const = 0;
	virtual FPoint2D GetMaxPoint() const = 0;

	double GetIsoCoordinate() const 
	{
		return IsoCoordinate;
	}

	double GetMinCoordinate() const
	{
		return MinCoordinate;
	}

	double GetMaxCoordinate() const
	{
		return MaxCoordinate;
	}

	FSegment2D GetSegment2D() const
	{
		return FSegment2D(Point0, Point1);
	}

	friend bool operator<(const FIntersectionIsoSegment& A, const FIntersectionIsoSegment& B)
	{
		if (FMath::IsNearlyEqual(A.IsoCoordinate, B.IsoCoordinate))
		{
			return A.MinCoordinate < B.MinCoordinate;
		}
		else
		{
			return A.IsoCoordinate < B.IsoCoordinate;
		}
	}
};

class FIsoUSegment : public FIntersectionIsoSegment
{
public:
	FIsoUSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
		: FIntersectionIsoSegment(StartPoint, EndPoint, StartPoint.U, StartPoint.V, EndPoint.V)
	{}

	virtual FPoint2D GetMinPoint() const override
	{
		return FPoint2D(GetIsoCoordinate(), GetMinCoordinate());
	}
	virtual FPoint2D GetMaxPoint() const override
	{
		return FPoint2D(GetIsoCoordinate(), GetMaxCoordinate());
	}
};

class FIsoVSegment : public FIntersectionIsoSegment
{
public:
	FIsoVSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint)
		: FIntersectionIsoSegment(StartPoint, EndPoint, StartPoint.V, StartPoint.U, EndPoint.U)
	{}

	virtual FPoint2D GetMinPoint() const override
	{
		return FPoint2D(GetMinCoordinate(), GetIsoCoordinate());
	}

	virtual FPoint2D GetMaxPoint() const override
	{
		return FPoint2D(GetMaxCoordinate(), GetIsoCoordinate());
	}

};

}

class FIntersectionIsoSegmentTool
{
private:
	const FGrid& Grid;

	TArray<TPair<double, int32>> CoordToIndex[2];
	
	TArray<IntersectionIsoSegmentTool::FIsoUSegment> USegments;
	TArray<IntersectionIsoSegmentTool::FIsoVSegment> VSegments;

	bool bIsSorted = false;

public:
	FIntersectionIsoSegmentTool(const FGrid& InGrid, const double Tolerance);

	void AddIsoSegment(const FPoint2D& StartPoint, const FPoint2D& EndPoint, const ESegmentType InType);

	bool DoesIntersect(const FIsoNode& StartNode, const FIsoNode& EndNode) const;

	int32 CountIntersections(const FIsoNode& StartNode, const FIsoNode& EndNode) const;

	void Sort();

private:
	int32 GetStartIndex(EIso Iso, double Min) const;
	int32 GetStartIndex(EIso Iso, const FSurfacicBoundary& Boundary) const;

public:
#ifdef CADKERNEL_DEBUG
	void Display(bool bDisplay, const TCHAR* Message, EVisuProperty Property = EVisuProperty::BlueCurve) const;
#endif
};

} // namespace UE::CADKernel

