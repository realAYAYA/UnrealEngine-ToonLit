// Copyright Epic Games, Inc. All Rights Reserved.

#include "Curve/PolygonOffsetUtils.h"

#include "Curve/ClipperUtils.h"
THIRD_PARTY_INCLUDES_START
#include "ThirdParty/clipper/clipper.h"
THIRD_PARTY_INCLUDES_END

using namespace UE::Geometry;

namespace UE::Geometry::Private
{
	template <typename RealType>
	static bool	Offset(
		const TArray<TArrayView<TVector2<RealType>>>& InPolygons,
		const EPolygonOffsetJoinType& InJoinType,
		const EPolygonOffsetEndType& InEndType,
		const RealType& InOffset,
		const RealType& InMiterLimit,
		TArray<UE::Geometry::TGeneralPolygon2<RealType>>& OutResult)
	{
		// Get combined bounds (min, max) of points 
		UE::Math::TBox2<RealType> InputBounds(EForceInit::ForceInitToZero);
		for(const TArrayView<TVector2<RealType>>& Polygon : InPolygons) 
		{
			if(!Polygon.IsEmpty())
			{
				UE::Math::TBox2<RealType> PolygonBounds(&Polygon[0], Polygon.Num());
				InputBounds += PolygonBounds;
			}
		}

		Clipper2Lib::ClipperOffset ClipperOffset;
		ClipperOffset.ReverseSolution(FMath::Sign(InOffset) < 0);

		InputBounds.ExpandBy(FMath::Max(0, InOffset * InOffset)); // pad to allow for offset geometry
		RealType InputRange = InputBounds.GetSize().GetMax();
		
		const Clipper2Lib::Paths<IntegralType> Paths = ConvertPolygonsToPaths<RealType, IntegralType>(InPolygons, InputBounds.Min, InputRange);

		ClipperOffset.MergeGroups(false); // This disables union clipping so we can perform it later to get a polytree
		ClipperOffset.AddPaths(Paths, static_cast<Clipper2Lib::JoinType>(InJoinType), static_cast<Clipper2Lib::EndType>(InEndType));
		ClipperOffset.MiterLimit(InMiterLimit);

		RealType ScaledOffset = FMath::Floor(static_cast<RealType>((InOffset / InputRange) * static_cast<RealType>(IntRange))); // scale to account for value normalization
		Clipper2Lib::Paths64 OffsetResultPaths = ClipperOffset.Execute(FMath::Abs(ScaledOffset));

		if(OffsetResultPaths.size() <= 0)
		{
			return false;
		}

		bool bExecuteResult = true;
		if(OffsetResultPaths.size() > 1)
		{
			// ...then union to merge and get polytree
			Clipper2Lib::Clipper64 Clipper;
			Clipper.PreserveCollinear = false;
			Clipper.AddSubject(OffsetResultPaths);
		
			Clipper2Lib::PolyTree64 UnionResultPolyTree;
			Clipper2Lib::Paths64 UnionResultOpenPaths;
			bExecuteResult = Clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, UnionResultPolyTree, UnionResultOpenPaths);

			if(bExecuteResult)
			{
				ConvertPolyTreeToPolygons<IntegralType, RealType>(&UnionResultPolyTree, OutResult, InputBounds.Min, InputRange);
			}
		}
		else
		{
			OutResult.Add(ConvertPathToPolygon(OffsetResultPaths.back(), InputBounds.Min, InputRange));
		}

		return bExecuteResult;
	}
}; 

template <typename GeometryType, typename RealType>
TOffsetPolygon2<GeometryType, RealType>::TOffsetPolygon2(
	const TArray<GeometryType>& InPolygons)
		: Polygons(InPolygons)
		, Result()
{
}

template <typename GeometryType, typename RealType>
bool TOffsetPolygon2<GeometryType, RealType>::ComputeResult()
{
	MiterLimit = FMath::Max<RealType>(static_cast<RealType>(2.0), MiterLimit); // Clamps lower value to 2.0
	return Private::Offset<RealType>(Polygons, JoinType, EndType, Offset, MiterLimit, Result);
}

namespace UE::Geometry
{
	template class GEOMETRYALGORITHMS_API TOffsetPolygon2<TArrayView<TVector2<float>>, float>;
	template class GEOMETRYALGORITHMS_API TOffsetPolygon2<TArrayView<TVector2<double>>, double>;
}
