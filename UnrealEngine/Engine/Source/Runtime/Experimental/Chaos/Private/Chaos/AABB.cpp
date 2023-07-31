// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/AABB.h"
#include "Chaos/Real.h"
#include "Chaos/Sphere.h"
#include "Chaos/Capsule.h"
#if INTEL_ISPC
#include "AABB.ispc.generated.h"

static_assert(sizeof(ispc::FTransform) == sizeof(FTransform), "sizeof(ispc::FTransform) != sizeof(FTransform)");
static_assert(sizeof(ispc::FVector) == sizeof(Chaos::TVector<Chaos::FReal, 3>), "sizeof(ispc::FVector) != sizeof(Chaos::TVector<Chaos::FReal, 3>)");
#endif

#if !defined(CHAOS_AABB_TRANSFORM_ISPC_ENABLED_DEFAULT)
#define CHAOS_AABB_TRANSFORM_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_AABBTransform_ISPC_Enabled = INTEL_ISPC && CHAOS_AABB_TRANSFORM_ISPC_ENABLED_DEFAULT;
#else
static bool bChaos_AABBTransform_ISPC_Enabled = CHAOS_AABB_TRANSFORM_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarChaosAABBTransformISPCEnabled(TEXT("p.Chaos.AABBTransform.ISPC"), bChaos_AABBTransform_ISPC_Enabled, TEXT("Whether to use ISPC optimizations when computing AABB transforms"));
#endif

namespace Chaos
{

template <typename T, int d>
TVector<T, d> TAABB<T, d>::FindClosestPoint(const TVector<T, d>& StartPoint, const T Thickness) const
{
	TVector<T, d> Result(0);

	// clamp exterior to surface
	bool bIsExterior = false;
	for (int i = 0; i < 3; i++)
	{
		T v = StartPoint[i];
		if (v < MMin[i])
		{
			v = MMin[i];
			bIsExterior = true;
		}
		if (v > MMax[i])
		{
			v = MMax[i];
			bIsExterior = true;
		}
		Result[i] = v;
	}

	if (!bIsExterior)
	{
		TArray<Pair<T, TVector<T, d>>> Intersections;

		// sum interior direction to surface
		for (int32 i = 0; i < d; ++i)
		{
			auto PlaneIntersection = TPlane<T, d>(MMin - Thickness, -TVector<T, d>::AxisVector(i)).FindClosestPoint(Result, 0);
			Intersections.Add(MakePair((T)(PlaneIntersection - Result).Size(), -TVector<T, d>::AxisVector(i)));
			PlaneIntersection = TPlane<T, d>(MMax + Thickness, TVector<T, d>::AxisVector(i)).FindClosestPoint(Result, 0);
			Intersections.Add(MakePair((T)(PlaneIntersection - Result).Size(), TVector<T, d>::AxisVector(i)));
		}
		Intersections.Sort([](const Pair<T, TVector<T, d>>& Elem1, const Pair<T, TVector<T, d>>& Elem2) { return Elem1.First < Elem2.First; });

		if (!FMath::IsNearlyEqual(Intersections[0].First, (T)0.))
		{
			T SmallestDistance = Intersections[0].First;
			Result += Intersections[0].Second * Intersections[0].First;
			for (int32 i = 1; i < 3 && FMath::IsNearlyEqual(SmallestDistance, Intersections[i].First); ++i)
			{
				Result += Intersections[i].Second * Intersections[i].First;
			}
		}
	}
	return Result;
}

template <typename T, int d>
Pair<TVector<FReal, d>, bool> TAABB<T, d>::FindClosestIntersectionImp(const TVector<FReal, d>& StartPoint, const TVector<FReal, d>& EndPoint, const FReal Thickness) const
{
	TArray<Pair<FReal, TVector<FReal, d>>> Intersections;
	for (int32 i = 0; i < d; ++i)
	{
		auto PlaneIntersection = TPlane<FReal, d>(TVector<FReal, d>(MMin) - Thickness, -TVector<FReal, d>::AxisVector(i)).FindClosestIntersection(StartPoint, EndPoint, 0);
		if (PlaneIntersection.Second)
			Intersections.Add(MakePair((FReal)(PlaneIntersection.First - StartPoint).Size(), PlaneIntersection.First));
		PlaneIntersection = TPlane<FReal, d>(TVector<FReal, d>(MMax) + Thickness, TVector<FReal, d>::AxisVector(i)).FindClosestIntersection(StartPoint, EndPoint, 0);
		if (PlaneIntersection.Second)
			Intersections.Add(MakePair((FReal)(PlaneIntersection.First - StartPoint).Size(), PlaneIntersection.First));
	}
	Intersections.Sort([](const Pair<FReal, TVector<FReal, d>>& Elem1, const Pair<FReal, TVector<FReal, d>>& Elem2) { return Elem1.First < Elem2.First; });
	for (const auto& Elem : Intersections)
	{
		if (SignedDistance(Elem.Second) < (Thickness + 1e-4))
		{
			return MakePair(Elem.Second, true);
		}
	}
	return MakePair(TVector<FReal, d>(0), false);
}


template <typename T, int d>
bool TAABB<T, d>::Raycast(const TVector<FReal, d>& StartPoint, const TVector<FReal, d>& Dir, const FReal Length, const FReal Thickness, FReal& OutTime, TVector<FReal, d>& OutPosition, TVector<FReal, d>& OutNormal, int32& OutFaceIndex) const
{
	ensure(Length > 0);
	ensure(FMath::IsNearlyEqual(Dir.SizeSquared(), (FReal)1, (FReal)UE_KINDA_SMALL_NUMBER));

	OutFaceIndex = INDEX_NONE;
	const TVector<FReal, d> MinInflated = TVector<FReal, d>(MMin) - Thickness;
	const TVector<FReal,d> StartToMin = MinInflated - StartPoint;
	
	const TVector<FReal, d> MaxInflated = TVector<FReal, d>(MMax) + Thickness;
	const TVector<FReal, d> StartToMax = MaxInflated - StartPoint;

	//For each axis record the start and end time when ray is in the box. If the intervals overlap the ray is inside the box
	FReal LatestStartTime = 0;
	FReal EarliestEndTime = TNumericLimits<FReal>::Max();
	TVector<FReal, d> Normal(0);	//not needed but fixes compiler warning

	for (int Axis = 0; Axis < d; ++Axis)
	{
		const bool bParallel = FMath::IsNearlyZero(Dir[Axis]);
		FReal Time1, Time2;
		if (bParallel)
		{
			if (StartToMin[Axis] > 0 || StartToMax[Axis] < 0)
			{
				return false;	//parallel and outside
			}
			else
			{
				Time1 = 0;
				Time2 = FLT_MAX;
			}
		}
		else
		{
			const FReal InvDir = (FReal)1 / Dir[Axis];
			Time1 = StartToMin[Axis] * InvDir;
			Time2 = StartToMax[Axis] * InvDir;
		}

		TVector<FReal, d> CurNormal = TVector<FReal, d>::AxisVector(Axis);

		if (Time1 > Time2)
		{
			//going from max to min direction
			std::swap(Time1, Time2);
		}
		else
		{
			//hit negative plane first
			CurNormal[Axis] = -1;
		}

		if (Time1 > LatestStartTime)
		{
			//last plane to enter so save its normal
			Normal = CurNormal;
		}
		LatestStartTime = FMath::Max(LatestStartTime, Time1);
		EarliestEndTime = FMath::Min(EarliestEndTime, Time2);

		if (LatestStartTime > EarliestEndTime)
		{
			return false;	//Outside of slab before entering another
		}
	}

	//infinite ray intersects with inflated box
	if (LatestStartTime > Length || EarliestEndTime < 0)
	{
		//outside of line segment given
		return false;
	}
	
	const TVector<FReal, d> BoxIntersection = StartPoint + LatestStartTime * Dir;

	//If the box is rounded we have to consider corners and edges.
	//Break the box into voronoi regions based on features (corner, edge, face) and see which region the raycast hit

	if (Thickness != (FReal)0)
	{
		check(d == 3);
		TVector<FReal, d> GeomStart;
		TVector<FReal, d> GeomEnd;
		int32 NumAxes = 0;

		for (int Axis = 0; Axis < d; ++Axis)
		{
			if (BoxIntersection[Axis] < (FReal)MMin[Axis])
			{
				GeomStart[Axis] = (FReal)MMin[Axis];
				GeomEnd[Axis] = (FReal)MMin[Axis];
				++NumAxes;
			}
			else if (BoxIntersection[Axis] > (FReal)MMax[Axis])
			{
				GeomStart[Axis] = (FReal)MMax[Axis];
				GeomEnd[Axis] = (FReal)MMax[Axis];
				++NumAxes;
			}
			else
			{
				GeomStart[Axis] = (FReal)MMin[Axis];
				GeomEnd[Axis] = (FReal)MMax[Axis];
			}
		}

		if (NumAxes >= 2)
		{
			bool bHit = false;
			if (NumAxes == 3)
			{
				//hit a corner. For now just use 3 capsules, there's likely a better way to determine which capsule is needed
				FReal CornerTimes[3];
				TVector<FReal, d> CornerPositions[3];
				TVector<FReal, d> CornerNormals[3];
				int32 HitIdx = INDEX_NONE;
				FReal MinTime = 0;	//initialization just here for compiler warning
				for (int CurIdx = 0; CurIdx < 3; ++CurIdx)
				{
					TVector<FReal, d> End = GeomStart;
					End[CurIdx] = End[CurIdx] == MMin[CurIdx] ? MMax[CurIdx] : MMin[CurIdx];
					FCapsule Capsule(GeomStart, End, Thickness);
					if (Capsule.Raycast(StartPoint, Dir, Length, 0, CornerTimes[CurIdx], CornerPositions[CurIdx], CornerNormals[CurIdx], OutFaceIndex))
					{
						if (HitIdx == INDEX_NONE || CornerTimes[CurIdx] < MinTime)
						{
							MinTime = CornerTimes[CurIdx];
							HitIdx = CurIdx;

							if (MinTime == 0)
							{
								OutTime = 0;	//initial overlap so just exit
								return true;
							}
						}
					}
				}

				if (HitIdx != INDEX_NONE)
				{
					OutPosition = CornerPositions[HitIdx];
					OutTime = MinTime;
					OutNormal = CornerNormals[HitIdx];
					bHit = true;
				}
			}
			else
			{
				//capsule: todo(use a cylinder which is cheaper. Our current cylinder raycast implementation doesn't quite work for this setup)
				FCapsule CapsuleBorder(GeomStart, GeomEnd, Thickness);
				bHit = CapsuleBorder.Raycast(StartPoint, Dir, Length, 0, OutTime, OutPosition, OutNormal, OutFaceIndex);
			}

			if (bHit && OutTime > 0)
			{
				OutPosition -= OutNormal * Thickness;
			}
			return bHit;
		}
	}
	
	// didn't hit any rounded parts so just use the box intersection
	OutTime = LatestStartTime;
	OutNormal = Normal;
	OutPosition = BoxIntersection - Thickness * Normal;
	return true;
}

template<typename T>
inline TAABB<T, 3> TransformedAABBHelper(const TAABB<T, 3>& AABB, const FMatrix44& SpaceTransform)
{
	// Initialize to center
	FVec3 Translation(SpaceTransform.M[3][0], SpaceTransform.M[3][1], SpaceTransform.M[3][2]);
	FVec3 Min = Translation;
	FVec3 Max = Translation;

	// Compute extents per axis
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)	
		{
			FReal A = SpaceTransform.M[j][i] * AABB.Min()[j];
			FReal B = SpaceTransform.M[j][i] * AABB.Max()[j];
			if (A < B)
			{
				Min[i] += A;
				Max[i] += B;
			}
			else 
			{
				Min[i] += B;
				Max[i] += A;
			}
		}
	}

	return TAABB<T, 3>(Min, Max);
}

inline TAABB<FReal, 3> TransformedAABBHelperISPC(const TAABB<FReal, 3>& AABB, const FTransform& SpaceTransform)
{
	check(bRealTypeCompatibleWithISPC);
#if INTEL_ISPC
	TVector<Chaos::FReal, 3> NewMin, NewMax;
	ispc::TransformedAABB((const ispc::FTransform&)SpaceTransform, (const ispc::FVector&)AABB.Min(), (const ispc::FVector&)AABB.Max(), (ispc::FVector&)NewMin, (ispc::FVector&)NewMax);

	TAABB<FReal, 3> NewAABB(NewMin, NewMax);
	return NewAABB;
#else
	check(false);
	return TAABB<FReal, 3>::EmptyAABB();
#endif
}

inline TAABB<Chaos::FRealSingle, 3> TransformedAABBHelperISPC(const TAABB<Chaos::FRealSingle, 3>& AABB, const FTransform& SpaceTransform)
{
	check(bRealTypeCompatibleWithISPC);
#if INTEL_ISPC
	static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::TVector<Chaos::FRealSingle, 3>), "sizeof(ispc::FVector3f) != sizeof(Chaos::TVector<Chaos::FRealSingle, 3>)");

	TVector<Chaos::FRealSingle, 3> NewMin, NewMax;
	ispc::TransformedAABBMixed((const ispc::FTransform&)SpaceTransform, (const ispc::FVector3f&)AABB.Min(), (const ispc::FVector3f&)AABB.Max(), (ispc::FVector3f&)NewMin, (ispc::FVector3f&)NewMax);

	TAABB<Chaos::FRealSingle, 3> NewAABB(NewMin, NewMax);
	return NewAABB;
#else
	check(false);
	return TAABB<Chaos::FRealSingle, 3>::EmptyAABB();
#endif
}

template<typename T, int d>
TAABB<T, d> TAABB<T, d>::TransformedAABB(const Chaos::TRigidTransform<FReal, 3>& SpaceTransform) const
{
	if (bRealTypeCompatibleWithISPC && INTEL_ISPC && bChaos_AABBTransform_ISPC_Enabled)
	{
		return TransformedAABBHelperISPC(*this, SpaceTransform);
	}
	else
	{
		return TransformedAABBHelper<T>(*this, SpaceTransform.ToMatrixWithScale());
	}
}

template<typename T, int d>
TAABB<T, d> TAABB<T, d>::TransformedAABB(const FMatrix& SpaceTransform) const
{
	return TransformedAABBHelper<T>(*this, SpaceTransform);
}


template<typename T, int d>
TAABB<T, d> TAABB<T, d>::TransformedAABB(const Chaos::PMatrix<FReal, 4, 4>& SpaceTransform) const
{
	return TransformedAABBHelper<T>(*this, SpaceTransform);
}

template<typename T, int d>
TAABB<T, d> TAABB<T, d>::TransformedAABB(const FTransform& SpaceTransform) const
{
	if (bRealTypeCompatibleWithISPC && INTEL_ISPC && bChaos_AABBTransform_ISPC_Enabled)
	{
		return TransformedAABBHelperISPC(*this, SpaceTransform);
	}
	else
	{
		return TransformedAABBHelper<T>(*this, SpaceTransform.ToMatrixWithScale());
	}
}

template<typename T, int d>
inline TAABB<T, d> TAABB<T, d>::InverseTransformedAABB(const FRigidTransform3& SpaceTransform) const
{
	TVector<T, d> CurrentExtents = Extents();
	int32 Idx = 0;
	const TVector<T, d> MinToNewSpace = SpaceTransform.InverseTransformPosition(FVector(Min()));
	TAABB<T, d> NewAABB(MinToNewSpace, MinToNewSpace);
	NewAABB.GrowToInclude(SpaceTransform.InverseTransformPosition(FVector(Max())));

	for (int32 j = 0; j < d; ++j)
	{
		NewAABB.GrowToInclude(SpaceTransform.InverseTransformPosition(FVector(Min() + TVector<T, d>::AxisVector(j) * CurrentExtents)));
		NewAABB.GrowToInclude(SpaceTransform.InverseTransformPosition(FVector(Max() - TVector<T, d>::AxisVector(j) * CurrentExtents)));
	}

	return NewAABB;
}
}

template class Chaos::TAABB<Chaos::FRealSingle, 3>;
template class Chaos::TAABB<Chaos::FRealDouble, 3>;
