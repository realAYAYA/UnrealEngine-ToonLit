// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDCollisionSpringConstraintsBase.h"
#include "Chaos/Plane.h"
#include "Chaos/Triangle.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/PBDSoftsSolverParticles.h"

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/Framework/Parallel.h"
#include <atomic>

namespace Chaos::Softs {

FPBDCollisionSpringConstraintsBase::FPBDCollisionSpringConstraintsBase(
	const int32 InOffset,
	const int32 InNumParticles,
	const FTriangleMesh& InTriangleMesh,
	const TArray<FSolverVec3>* InReferencePositions,
	TSet<TVec2<int32>>&& InDisabledCollisionElements,
	const FSolverReal InThickness,
	const FSolverReal InStiffness,
	const FSolverReal InFrictionCoefficient)
	: TriangleMesh(InTriangleMesh)
	, Elements(InTriangleMesh.GetSurfaceElements())
	, ReferencePositions(InReferencePositions)
	, DisabledCollisionElements(InDisabledCollisionElements)
	, Offset(InOffset)
	, NumParticles(InNumParticles)
	, Thickness(InThickness)
	, Stiffness(InStiffness)
	, FrictionCoefficient(InFrictionCoefficient)
	, bGlobalIntersectionAnalysis(false)
{
}

void FPBDCollisionSpringConstraintsBase::Init(const FSolverParticles& Particles)
{
	if (!Elements.Num())
	{
		Constraints.Reset();
		Barys.Reset();
		FlipNormal.Reset();
		return;
	}

	FTriangleMesh::TBVHType<FSolverReal> BVH;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosPBDCollisionSpring_BuildBVH);
		TriangleMesh.BuildBVH(static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), BVH);
	}
	TArray<FPBDTriangleMeshCollisions::FGIAColor> EmptyGIAColors;
	return Init(Particles, BVH, static_cast<TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>>(EmptyGIAColors), EmptyGIAColors);
}

template<typename SpatialAccelerator>
void FPBDCollisionSpringConstraintsBase::Init(const FSolverParticles& Particles, const SpatialAccelerator& Spatial, 
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors)
{
	if (!Elements.Num())
	{
		Constraints.Reset();
		Barys.Reset();
		FlipNormal.Reset();
		return;
	}
	{
		bGlobalIntersectionAnalysis = VertexGIAColors.Num() == NumParticles + Offset && TriangleGIAColors.Num() == Elements.Num();
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosPBDCollisionSpring_ProximityQuery);

		// Preallocate enough space for all possible connections.
		constexpr int32 MaxConnectionsPerPoint = 3;
		Constraints.SetNum(NumParticles * MaxConnectionsPerPoint);
		Barys.SetNum(NumParticles * MaxConnectionsPerPoint);
		FlipNormal.SetNum(NumParticles * MaxConnectionsPerPoint);

		std::atomic<int32> ConstraintIndex(0);

		const FSolverReal HeightSq = FMath::Square(Thickness + Thickness);

		PhysicsParallelFor(NumParticles,
			[this, &Spatial, &Particles, &ConstraintIndex, HeightSq, MaxConnectionsPerPoint, &VertexGIAColors, &TriangleGIAColors](int32 i)
			{
				const int32 Index = i + Offset;
				constexpr FSolverReal ExtraThicknessMult = 1.5f;

				TArray< TTriangleCollisionPoint<FSolverReal> > Result;
				if (TriangleMesh.PointProximityQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.X(Index), Thickness * ExtraThicknessMult, Thickness * ExtraThicknessMult,
					[this, &VertexGIAColors, &TriangleGIAColors](const int32 PointIndex, const int32 TriangleIndex)->bool
					{
						const TVector<int32, 3>& Elem = Elements[TriangleIndex];
						if (bGlobalIntersectionAnalysis && VertexGIAColors[PointIndex].IsLoop() && (VertexGIAColors[Elem[0]].IsLoop() || VertexGIAColors[Elem[1]].IsLoop() || VertexGIAColors[Elem[2]].IsLoop() || TriangleGIAColors[TriangleIndex].IsLoop()))
						{
							return false;
						}

						if (DisabledCollisionElements.Contains({ PointIndex, Elem[0] }) ||
							DisabledCollisionElements.Contains({ PointIndex, Elem[1] }) ||
							DisabledCollisionElements.Contains({ PointIndex, Elem[2] }))
						{
							return false;
						}

						return true;
					},
					Result))
				{

					if (Result.Num() > MaxConnectionsPerPoint)
					{
						// TODO: once we have a PartialSort, use that instead here.
						Result.Sort(
							[](const TTriangleCollisionPoint<FSolverReal>& First, const TTriangleCollisionPoint<FSolverReal>& Second)->bool
							{
								return First.Phi < Second.Phi;
							}
						);
						Result.SetNum(MaxConnectionsPerPoint, false /*bAllowShrinking*/);
					}

					for (const TTriangleCollisionPoint<FSolverReal>& CollisionPoint : Result)
					{
						const TVector<int32, 3>& Elem = Elements[CollisionPoint.Indices[1]];
						if (ReferencePositions)
						{
							const FSolverVec3& RefP = (*ReferencePositions)[Index];
							const FSolverVec3& RefP0 = (*ReferencePositions)[Elem[0]];
							const FSolverVec3& RefP1 = (*ReferencePositions)[Elem[1]];
							const FSolverVec3& RefP2 = (*ReferencePositions)[Elem[2]];
							const FSolverVec3 RefDiff = RefP - CollisionPoint.Bary[1] * RefP0 - CollisionPoint.Bary[2] * RefP1 - CollisionPoint.Bary[3] * RefP2;
							if (RefDiff.SizeSquared() < HeightSq)
							{
								continue;
							}
						}

						// NOTE: CollisionPoint.Normal has already been flipped to point toward the Point, so need to recalculate here.
						const TTriangle<FSolverReal> Triangle(Particles.X(Elem[0]), Particles.X(Elem[1]), Particles.X(Elem[2]));
						bool bFlipNormal = (Particles.X(Index) - CollisionPoint.Location).Dot(Triangle.GetNormal()) < 0; // Is Point currently behind Triangle?

						// Doing a check against ANY (plus the TriangleGIAColors which captures sub-triangle intersections) seems to work better than checking against ALL vertex colors where the triangle must agree.
						// In particular, it's better at handling thin regions of intersection where a single vertex or line of vertices intersect through faces.
						if (bGlobalIntersectionAnalysis &&
							(FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[0]]) ||
								FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[1]]) ||
								FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[2]]) ||
								FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], TriangleGIAColors[CollisionPoint.Indices[1]])))
						{
							// Want Point to push to opposite side of triangle
							bFlipNormal = !bFlipNormal;
						}
						const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

						Constraints[IndexToWrite] = { Index, Elem[0], Elem[1], Elem[2] };
						Barys[IndexToWrite] = { CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
						FlipNormal[IndexToWrite] = bFlipNormal;
					}
				}
			}
		);

		// Shrink the arrays to the actual number of found constraints.
		const int32 ConstraintNum = ConstraintIndex.load();
		Constraints.SetNum(ConstraintNum, /*bAllowShrinking*/ true);
		Barys.SetNum(ConstraintNum, /*bAllowShrinking*/ true);
		FlipNormal.SetNum(ConstraintNum, /*bAllowShrinking*/ true);
	}
}
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TBVHType<FSolverReal>>(const FSolverParticles& Particles, const FTriangleMesh::TBVHType<FSolverReal>& Spatial, 
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TSpatialHashType<FSolverReal>>(const FSolverParticles& Particles, const FTriangleMesh::TSpatialHashType<FSolverReal>& Spatial, 
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);

FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const FSolverParticles& Particles, const int32 i) const
{
	const TVec4<int32>& Constraint = Constraints[i];
	const int32 i1 = Constraint[0];
	const int32 i2 = Constraint[1];
	const int32 i3 = Constraint[2];
	const int32 i4 = Constraint[3];

	const FSolverReal TrianglePointInvM =
		Particles.InvM(i2) * Barys[i][0] +
		Particles.InvM(i3) * Barys[i][1] +
		Particles.InvM(i4) * Barys[i][2];

	const FSolverReal CombinedMass = Particles.InvM(i1) + TrianglePointInvM;
	if (CombinedMass <= (FSolverReal)1e-7)
	{
		return FSolverVec3(0);
	}

	const FSolverVec3& P1 = Particles.P(i1);
	const FSolverVec3& P2 = Particles.P(i2);
	const FSolverVec3& P3 = Particles.P(i3);
	const FSolverVec3& P4 = Particles.P(i4);

	const FSolverReal Height = Thickness + Thickness;
	const FSolverVec3 P = Barys[i][0] * P2 + Barys[i][1] * P3 + Barys[i][2] * P4;
	const FSolverVec3 Difference = P1 - P;

	// Normal repulsion with friction
	const TTriangle<FSolverReal> Triangle(P2, P3, P4);
	const FSolverVec3 Normal = FlipNormal[i] ? -Triangle.GetNormal() : Triangle.GetNormal();

	const FSolverReal NormalDifference = Difference.Dot(Normal);
	if (NormalDifference > Height)
	{
		return FSolverVec3(0);
	}

	const FSolverReal NormalDelta = Height - NormalDifference;
	const FSolverVec3 RepulsionDelta = Stiffness * NormalDelta * Normal / CombinedMass;

	if (FrictionCoefficient > 0)
	{
		const FSolverVec3& X1 = Particles.X(i1);
		const FSolverVec3 X = Barys[i][0] * Particles.X(i2) + Barys[i][1] * Particles.X(i3) + Barys[i][2] * Particles.X(i4);
		const FSolverVec3 RelativeDisplacement = (P1 - X1) - (P - X) + (Particles.InvM(i1) - TrianglePointInvM) * RepulsionDelta;
		const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - RelativeDisplacement.Dot(Normal) * Normal;
		const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Length();
		const FSolverReal PositionCorrection = FMath::Min(NormalDelta * FrictionCoefficient, RelativeDisplacementTangentLength);
		const FSolverReal CorrectionRatio = RelativeDisplacementTangentLength < UE_SMALL_NUMBER ? 0.f : PositionCorrection / RelativeDisplacementTangentLength;
		const FSolverVec3 FrictionDelta = -CorrectionRatio * RelativeDisplacementTangent / CombinedMass;
		return RepulsionDelta + FrictionDelta;
	}
	else
	{
		return RepulsionDelta;
	}
}

}  // End namespace Chaos::Softs

#endif
