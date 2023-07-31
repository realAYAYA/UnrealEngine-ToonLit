// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/HaveStates.h"
#include "CADKernel/Core/Factory.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/Math/Point.h"
#include "CADKernel/Mesh/Structure/EdgeSegment.h"

//#define DEBUG_THIN_ZONES
//#define DISPLAY_THIN_ZONES // to display found thin zone

//#define DEBUG_BORDER_THIN_ZONES
//#define DEBUG_REMOVE_THIN_ZONE
//#define DEBUG_REMOVABLE
//#define DEBUG_BEST_VERTEX

namespace UE::CADKernel
{
	class FTopologicalLoop;
	class FTopologicalEdge;
	class FGrid;
	class FThinZone2D;
	class FThinZone2DFinder;

	struct FThinZoneChronos
	{
		FDuration BuildLoopSegmentsTime;
		FDuration DisplayBoundarySegmentTime;
		FDuration FindClosedSegmentTime;
		FDuration CheckClosedSegmentTime;
		FDuration LinkClosedSegmentTime;
		FDuration BuildThinZoneTime;
	};

	enum class EThinZone2DType : uint8
	{
		Undefined = 0,
		Global,			// a Surface globally thin
		PeakStart,		// an extremity of a Surface that is fine
		PeakEnd,		// an extremity of a Surface that is fine
		Butterfly,		// the outter loop that is like a bow tie (Butterfly)
		BetweenLoops    // a bow tie between two different loops
	};

	class FThinZoneSide : public FHaveStates
	{
	private:
		TArray<FEdgeSegment*> Segments;
		FThinZoneSide& FrontSide;
		double Length;

	public:

		FThinZoneSide(FThinZoneSide* InFrontSide, const TArray<FEdgeSegment*>& InSegments, bool bInIsFirstSide);
		virtual ~FThinZoneSide() = default;

		bool IsClosed() const
		{
			if (GetFirst() == nullptr)
			{
				return false;
			}
			return GetFirst()->GetPrevious() == GetLast();
		}

		FEdgeSegment* GetFirst() const
		{
			return Segments.Num() > 0 ? Segments[0] : nullptr;
		}

		FEdgeSegment* GetLast() const
		{
			return Segments.Num() > 0 ? Segments.Last() : nullptr;
		}

		void SetEdgesAsThinZone();

		const TArray<FEdgeSegment*>& GetSegments() const
		{
			return Segments;
		}

		bool IsPartiallyMeshed() const;
		double GetMeshedLength() const;

		double GetLength() const
		{
			return Length;
		}

		bool IsInner() const
		{
			if (Segments.Num() == 0)
			{
				return true;
			}
			return Segments[0]->IsInner();
		}

		bool IsFirstSide() const
		{
			return ((States & EHaveStates::IsFirstSide) == EHaveStates::IsFirstSide);
		}

		void SetFirstSide() const
		{
			States |= EHaveStates::IsFirstSide;
		}

		void ResetFirstSide() const
		{
			States &= ~EHaveStates::IsFirstSide;
		}
	};

	class FThinZone2D : public FHaveStates
	{
	private:

		FThinZoneSide FirstSide;
		FThinZoneSide SecondSide;

		EThinZone2DType Category;

		TArray<TSharedPtr<FTopologicalEdge>> PeakEdges[2];

		double Thickness;

	public:

		FThinZone2D(const TArray<FEdgeSegment*>& InFirstSideSegments, bool bInIsClosed1, const TArray<FEdgeSegment*>& InSecondSideSegments, bool bInIsClosed2, double ZoneThickness)
			: FirstSide(&SecondSide, InFirstSideSegments, true)
			, SecondSide(&FirstSide, InSecondSideSegments, false)
			, Category(EThinZone2DType::Undefined)
			, Thickness(ZoneThickness)
		{
			if (bInIsClosed1)
			{
				SetFirstSideClosed();
			}

			if (bInIsClosed2)
			{
				SetFirstSideClosed();
			}
		}

		virtual ~FThinZone2D() = default;

		const double GetThickness()
		{
			return Thickness;
		};

		const TArray<TSharedPtr<FTopologicalEdge>>* GetPeakEdges() const
		{
			return PeakEdges;
		}

		TArray<TSharedPtr<FTopologicalEdge>>* GetPeakEdges()
		{
			return PeakEdges;
		}

		const FThinZoneSide& GetFirstSide() const
		{
			return FirstSide;
		}

		const FThinZoneSide& GetSecondSide() const
		{
			return SecondSide;
		}

		EThinZone2DType GetCategory() const
		{
			return Category;
		}

		void SetCategory(EThinZone2DType InType)
		{
			Category = InType;
		}


		void SetEdgesAsThinZone();

		double GetMaxLength() const
		{
			return FMath::Max(FirstSide.GetLength(), SecondSide.GetLength());
		}

		bool IsRemoved() const
		{
			return ((States & EHaveStates::IsRemoved) == EHaveStates::IsRemoved);
		}

		void SetRemoved() const
		{
			States |= EHaveStates::IsRemoved;
		}

		void ResetRemoved() const
		{
			States &= ~EHaveStates::IsRemoved;
		}

		bool IsFirstSideClosed() const
		{
			return ((States & EHaveStates::FirstSideClosed) == EHaveStates::FirstSideClosed);
		}

		void SetFirstSideClosed() const
		{
			States |= EHaveStates::FirstSideClosed;
		}

		void ResetFirstSideClosed() const
		{
			States &= ~EHaveStates::FirstSideClosed;
		}

		bool IsSecondSideClosed() const
		{
			return ((States & EHaveStates::SecondSideClosed) == EHaveStates::SecondSideClosed);
		}

		void SetSecondSideClosed() const
		{
			States |= EHaveStates::SecondSideClosed;
		}

		void ResetSecondSideClosed() const
		{
			States &= ~EHaveStates::SecondSideClosed;
		}

		bool HasClosedSide() const 
		{
			constexpr EHaveStates ATLeastOneClosed = EHaveStates::FirstSideClosed | EHaveStates::SecondSideClosed;
			return EnumHasAnyFlags(States, ATLeastOneClosed);
		}

	};

	class FThinZone2DFinder
	{
		friend class FThinZone2D;

	public:
		FThinZoneChronos Chronos;

	protected:
		FGrid& Grid;

		double Tolerance;
		double SquareTolerance;
		double MinFeatureSize;

		TArray<FThinZone2D> ThinZones;

		TArray<FEdgeSegment*> LoopSegments;
		TArray<FEdgeSegment*> SortedLoopSegments;

		double FirstLoopLength = 0.0;

		TFactory<FEdgeSegment> SegmentFatory;

	public:

		FThinZone2DFinder(FGrid& InGrid)
			: Grid(InGrid)
			, SegmentFatory()
		{
		}

		void Set(double InTolerance)
		{
			Tolerance = InTolerance;
			SquareTolerance = FMath::Square(Tolerance);
		}

		void SearchThinZones();

		const TArray<FThinZone2D>& GetThinZones() const
		{
			return ThinZones;
		}

	private:

		void BuildLoopSegments();
		void FindClosedSegments();
		void CheckClosedSegments();
		void LinkClosedSegments();
		void BuildThinZone();

		void DisplayClosedSegments();
		void DisplayLoopSegments();
	};
}
