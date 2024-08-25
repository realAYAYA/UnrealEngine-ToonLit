// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PerParticlePBDCollisionConstraint.h"
#include "ChaosStats.h"
#include "HAL/IConsoleManager.h"
#if INTEL_ISPC
#include "PerParticlePBDCollisionConstraint.ispc.generated.h"

// These includes are used for the static_asserts below
#include "Chaos/Capsule.h"
#include "Chaos/Convex.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Sphere.h"
#include "Chaos/TaperedCapsule.h"
#include "Chaos/TaperedCylinder.h"
#endif

#if INTEL_ISPC
static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::Softs::FSolverVec3), "sizeof(ispc::FVector3f) != sizeof(Chaos::Softs::FSolverVec3)");
static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FSolverRotation3), "sizeof(ispc::FVector4f) != sizeof(Chaos::Softs::FSolverRotation3)");
static_assert(ispc::ImplicitObjectType::Sphere == Chaos::ImplicitObjectType::Sphere);
static_assert(ispc::ImplicitObjectType::Capsule == Chaos::ImplicitObjectType::Capsule);
static_assert(ispc::ImplicitObjectType::Union == Chaos::ImplicitObjectType::Union);
static_assert(ispc::ImplicitObjectType::TaperedCapsule == Chaos::ImplicitObjectType::TaperedCapsule);
static_assert(ispc::ImplicitObjectType::Convex == Chaos::ImplicitObjectType::Convex);
static_assert(ispc::ImplicitObjectType::WeightedLatticeBone == Chaos::ImplicitObjectType::WeightedLatticeBone);
static_assert(ispc::ImplicitObjectType::IsWeightedLattice == Chaos::ImplicitObjectType::IsWeightedLattice);
static_assert(ispc::ImplicitObjectType::WeightedLatticeLevelSetType == (Chaos::ImplicitObjectType::IsWeightedLattice | Chaos::ImplicitObjectType::LevelSet));
static_assert(sizeof(ispc::TArray) == sizeof(TArray<int>));
// Sphere
static_assert(sizeof(Chaos::FImplicitObject) == Chaos::FSphere::FISPCDataVerifier::OffsetOfCenter());
static_assert(sizeof(ispc::FVector) == Chaos::FSphere::FISPCDataVerifier::SizeOfCenter());
// Capsule
static_assert(sizeof(Chaos::FImplicitObject) == Chaos::FCapsule::FISPCDataVerifier::OffsetOfMSegment());
static_assert(sizeof(ispc::Segment) == Chaos::FCapsule::FISPCDataVerifier::SizeOfMSegment());
// Union (only specific case of MObjects = [FTaperedCylinder, Sphere, Sphere] is used here.
static_assert(sizeof(Chaos::FImplicitObject) == Chaos::FImplicitObjectUnion::FISPCDataVerifier::OffsetOfMObjects());
static_assert(sizeof(ispc::TArray) == Chaos::FImplicitObjectUnion::FISPCDataVerifier::SizeOfMObjects());
// TaperedCylinder
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCylinder, MPlane1) == Chaos::FTaperedCylinder::FISPCDataVerifier::OffsetOfMPlane1());
static_assert(sizeof(ispc::FTaperedCylinder::MPlane1) == Chaos::FTaperedCylinder::FISPCDataVerifier::SizeOfMPlane1());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCylinder, MPlane2) == Chaos::FTaperedCylinder::FISPCDataVerifier::OffsetOfMPlane2());
static_assert(sizeof(ispc::FTaperedCylinder::MPlane2) == Chaos::FTaperedCylinder::FISPCDataVerifier::SizeOfMPlane2());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCylinder, Height) == Chaos::FTaperedCylinder::FISPCDataVerifier::OffsetOfMHeight());
static_assert(sizeof(ispc::FTaperedCylinder::Height) == Chaos::FTaperedCylinder::FISPCDataVerifier::SizeOfMHeight());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCylinder, Radius1) == Chaos::FTaperedCylinder::FISPCDataVerifier::OffsetOfMRadius1());
static_assert(sizeof(ispc::FTaperedCylinder::Radius1) == Chaos::FTaperedCylinder::FISPCDataVerifier::SizeOfMRadius1());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCylinder, Radius2) == Chaos::FTaperedCylinder::FISPCDataVerifier::OffsetOfMRadius2());
static_assert(sizeof(ispc::FTaperedCylinder::Radius2) == Chaos::FTaperedCylinder::FISPCDataVerifier::SizeOfMRadius2());
// TaperedCapsule
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCapsule, Origin) == Chaos::FTaperedCapsule::FISPCDataVerifier::OffsetOfOrigin());
static_assert(sizeof(ispc::FTaperedCapsule::Origin) == Chaos::FTaperedCapsule::FISPCDataVerifier::SizeOfOrigin());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCapsule, Axis) == Chaos::FTaperedCapsule::FISPCDataVerifier::OffsetOfAxis());
static_assert(sizeof(ispc::FTaperedCapsule::Axis) == Chaos::FTaperedCapsule::FISPCDataVerifier::SizeOfAxis());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCapsule, Height) == Chaos::FTaperedCapsule::FISPCDataVerifier::OffsetOfHeight());
static_assert(sizeof(ispc::FTaperedCapsule::Height) == Chaos::FTaperedCapsule::FISPCDataVerifier::SizeOfHeight());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCapsule, Radius1) == Chaos::FTaperedCapsule::FISPCDataVerifier::OffsetOfRadius1());
static_assert(sizeof(ispc::FTaperedCapsule::Radius1) == Chaos::FTaperedCapsule::FISPCDataVerifier::SizeOfRadius1());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FTaperedCapsule, Radius2) == Chaos::FTaperedCapsule::FISPCDataVerifier::OffsetOfRadius2());
static_assert(sizeof(ispc::FTaperedCapsule::Radius2) == Chaos::FTaperedCapsule::FISPCDataVerifier::SizeOfRadius2());
// Convex
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FConvex, Planes) == Chaos::FConvex::FISPCDataVerifier::OffsetOfPlanes());
static_assert(sizeof(ispc::FConvex::Planes) == Chaos::FConvex::FISPCDataVerifier::SizeOfPlanes());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FConvex, Vertices) == Chaos::FConvex::FISPCDataVerifier::OffsetOfVertices());
static_assert(sizeof(ispc::FConvex::Vertices) == Chaos::FConvex::FISPCDataVerifier::SizeOfVertices());
static_assert(sizeof(Chaos::FImplicitObject) + offsetof(ispc::FConvex, StructureData) == Chaos::FConvex::FISPCDataVerifier::OffsetOfStructureData());
static_assert(sizeof(ispc::FConvex::StructureData) == Chaos::FConvex::FISPCDataVerifier::SizeOfStructureData());

static_assert(sizeof(ispc::FPlaneConcrete3f) == sizeof(Chaos::FConvex::FPlaneType));
static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::FConvex::FVec3Type)); // Vertices array

static_assert(offsetof(ispc::FConvexStructureData, Data) == Chaos::FConvexStructureData::FISPCDataVerifier::OffsetOfData());
static_assert(sizeof(ispc::FConvexStructureData::Data) == Chaos::FConvexStructureData::FISPCDataVerifier::SizeOfData());
static_assert(offsetof(ispc::FConvexStructureData, IndexType) == Chaos::FConvexStructureData::FISPCDataVerifier::OffsetOfIndexType());
static_assert(sizeof(ispc::FConvexStructureData::IndexType) == Chaos::FConvexStructureData::FISPCDataVerifier::SizeOfIndexType());

static_assert(offsetof(ispc::FConvexStructureDataImp, Planes) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::OffsetOfPlanes());
static_assert(offsetof(ispc::FConvexStructureDataImp, Planes) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::OffsetOfPlanes());
static_assert(offsetof(ispc::FConvexStructureDataImp, Planes) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::OffsetOfPlanes());
static_assert(sizeof(ispc::FConvexStructureDataImp::Planes) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::SizeOfPlanes());
static_assert(sizeof(ispc::FConvexStructureDataImp::Planes) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::SizeOfPlanes());
static_assert(sizeof(ispc::FConvexStructureDataImp::Planes) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::SizeOfPlanes());

static_assert(offsetof(ispc::FConvexStructureDataImp, HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::OffsetOfHalfEdges());
static_assert(offsetof(ispc::FConvexStructureDataImp, HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::OffsetOfHalfEdges());
static_assert(offsetof(ispc::FConvexStructureDataImp, HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::OffsetOfHalfEdges());
static_assert(sizeof(ispc::FConvexStructureDataImp::HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::SizeOfHalfEdges());
static_assert(sizeof(ispc::FConvexStructureDataImp::HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::SizeOfHalfEdges());
static_assert(sizeof(ispc::FConvexStructureDataImp::HalfEdges) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::SizeOfHalfEdges());

static_assert(offsetof(ispc::FConvexStructureDataImp, Vertices) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::OffsetOfVertices());
static_assert(offsetof(ispc::FConvexStructureDataImp, Vertices) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::OffsetOfVertices());
static_assert(offsetof(ispc::FConvexStructureDataImp, Vertices) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::OffsetOfVertices());
static_assert(sizeof(ispc::FConvexStructureDataImp::Vertices) == Chaos::FConvexStructureData::FConvexStructureDataSmall::FISPCDataVerifier::SizeOfVertices());
static_assert(sizeof(ispc::FConvexStructureDataImp::Vertices) == Chaos::FConvexStructureData::FConvexStructureDataMedium::FISPCDataVerifier::SizeOfVertices());
static_assert(sizeof(ispc::FConvexStructureDataImp::Vertices) == Chaos::FConvexStructureData::FConvexStructureDataLarge::FISPCDataVerifier::SizeOfVertices());

static_assert(sizeof(ispc::PlanesS) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataSmall::FPlaneData));
static_assert(sizeof(ispc::PlanesM) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataMedium::FPlaneData));
static_assert(sizeof(ispc::PlanesL) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataLarge::FPlaneData));
static_assert(sizeof(ispc::HalfEdgesS) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataSmall::FHalfEdgeData));
static_assert(sizeof(ispc::HalfEdgesM) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataMedium::FHalfEdgeData));
static_assert(sizeof(ispc::HalfEdgesL) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataLarge::FHalfEdgeData));
static_assert(sizeof(ispc::VerticesS) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataSmall::FVertexData));
static_assert(sizeof(ispc::VerticesM) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataMedium::FVertexData));
static_assert(sizeof(ispc::VerticesL) == sizeof(Chaos::FConvexStructureData::FConvexStructureDataLarge::FVertexData));

#if !UE_BUILD_SHIPPING
bool bChaos_PerParticleCollision_ISPC_Enabled = CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarChaosPerParticleCollisionISPCEnabled(TEXT("p.Chaos.PerParticleCollision.ISPC"), bChaos_PerParticleCollision_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in per particle collisions"));
#endif
#endif

static int32 Chaos_PerParticleCollision_ISPC_ParallelBatchSize = 128;
#if !UE_BUILD_SHIPPING
FAutoConsoleVariableRef CVarChaosPerParticleCollisionISPCParallelBatchSize(TEXT("p.Chaos.PerParticleCollision.ISPC.ParallelBatchSize"), Chaos_PerParticleCollision_ISPC_ParallelBatchSize, TEXT("Parallel batch size for ISPC"));
#endif

namespace Chaos::Softs {

// Helper function to call PhiWithNormal and return data to ISPC
extern "C" void GetPhiWithNormal(const uint8* CollisionParticles, const FSolverReal* InV, FSolverReal* Normal, FSolverReal* Phi, const int32 i, const int32 ProgramCount, const int32 Mask)
{
	const FSolverCollisionParticles& C = *(const FSolverCollisionParticles*)CollisionParticles;
	
	for (int32 Index = 0; Index < ProgramCount; ++Index)
	{
		if (Mask & (1 << Index))
		{
			FSolverVec3 V;

			// aos_to_soa3
			V.X = InV[Index];
			V.Y = InV[Index + ProgramCount];
			V.Z = InV[Index + 2 * ProgramCount];

			FVec3 ImplicitNormal;
			Phi[Index] = (FSolverReal)C.GetGeometry(i)->PhiWithNormal(FVec3(V), ImplicitNormal);
			FSolverVec3 Norm(ImplicitNormal);

			// aos_to_soa3
			Normal[Index] = Norm.X;
			Normal[Index + ProgramCount] = Norm.Y;
			Normal[Index + 2 * ProgramCount] = Norm.Z;
		}
	}
}

// Helper function to call PhiWithNormal and return data to ISPC
extern "C" void GetPhiWithNormalAndVelocityBone(const uint8 * CollisionParticles, const FSolverReal * InV, FSolverReal * Normal, FSolverReal * Phi, int32 * VelocityBone, const int32 i, const FSolverReal Thickness, const int32 ProgramCount, const int32 Mask)
{
	const FSolverCollisionParticles& C = *(const FSolverCollisionParticles*)CollisionParticles;

	for (int32 Index = 0; Index < ProgramCount; ++Index)
	{
		if (Mask & (1 << Index))
		{
			FSolverVec3 V;

			// aos_to_soa3
			V.X = InV[Index];
			V.Y = InV[Index + ProgramCount];
			V.Z = InV[Index + 2 * ProgramCount];

			VelocityBone[Index] = i;

			FVec3 ImplicitNormal;
			if (const TWeightedLatticeImplicitObject<FLevelSet>* LevelSet = C.GetGeometry(i)->GetObject< TWeightedLatticeImplicitObject<FLevelSet> >())
			{
				FWeightedLatticeImplicitObject::FEmbeddingCoordinate SurfaceCoord;
				Phi[Index] = (FSolverReal)LevelSet->PhiWithNormalAndSurfacePoint(FVec3(V), ImplicitNormal, SurfaceCoord);
				const FSolverReal Penetration = Thickness - Phi[Index]; // This is related to the Normal impulse
				if (Penetration > (FSolverReal)0.)
				{
					const int32 StrongestBone = SurfaceCoord.GreatestInfluenceBone(LevelSet->GetBoneData());
					if (StrongestBone != INDEX_NONE)
					{
						VelocityBone[Index] = LevelSet->GetSolverBoneIndices()[StrongestBone];
					}
				}
			}
			else
			{
				Phi[Index] = (FSolverReal)C.GetGeometry(i)->PhiWithNormal(FVec3(V), ImplicitNormal);
			}
			FSolverVec3 Norm(ImplicitNormal);

			// aos_to_soa3
			Normal[Index] = Norm.X;
			Normal[Index + ProgramCount] = Norm.Y;
			Normal[Index + 2 * ProgramCount] = Norm.Z;
		}
	}
}

void FPerParticlePBDCollisionConstraint::ApplyHelperISPC(FSolverParticles& InParticles, const FSolverReal Dt, const int32 Offset, const int32 Range) const
{
	check(bRealTypeCompatibleWithISPC);
	check(bFastPositionBasedFriction);

	const uint32 DynamicGroupId = MDynamicGroupIds[Offset];
	const FSolverReal PerGroupFriction = MPerGroupFriction[DynamicGroupId];
	const FSolverReal PerGroupThickness = MPerGroupThickness[DynamicGroupId];

	const int32 NumBatches = FMath::CeilToInt((FSolverReal)(Range - Offset) / (FSolverReal)Chaos_PerParticleCollision_ISPC_ParallelBatchSize);

	if (PerGroupFriction > UE_KINDA_SMALL_NUMBER)  // Fast friction
	{
		PhysicsParallelFor(NumBatches, [this, &InParticles, Dt, Offset, Range, DynamicGroupId, PerGroupFriction, PerGroupThickness](int32 BatchNumber)
			{
				const int32 BatchBegin = Offset + (Chaos_PerParticleCollision_ISPC_ParallelBatchSize * BatchNumber);
		const int32 BatchEnd = FMath::Min(Range, BatchBegin + Chaos_PerParticleCollision_ISPC_ParallelBatchSize);

#if INTEL_ISPC
		MCollisionParticlesActiveView.RangeFor(
			[this, &InParticles, Dt, BatchBegin, BatchEnd, DynamicGroupId, PerGroupFriction, PerGroupThickness](FSolverCollisionParticles& CollisionParticles, int32 CollisionOffset, int32 CollisionRange)
			{
				ispc::ApplyPerParticleCollisionFastFriction(
					(ispc::FVector4f*)InParticles.GetPAndInvM().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticles.AllV().GetData(),
					(const ispc::FVector3f*)CollisionParticles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticles.AllW().GetData(),
					(const ispc::FVector4f*)CollisionParticles.AllR().GetData(),
					DynamicGroupId,
					MKinematicGroupIds.GetData(),
					PerGroupFriction,
					PerGroupThickness,
					(const uint8*)&CollisionParticles,
					(const uint8*)CollisionParticles.GetAllGeometry().GetData(),
					sizeof(FImplicitObject),
					FImplicitObject::GetOffsetOfType(),
					FImplicitObject::GetOffsetOfMargin(),
					Dt,
					CollisionOffset,
					CollisionRange,
					BatchBegin,
					BatchEnd);
			});
#endif  // #if INTEL_ISPC
			});
	}
	else  // No friction
	{
		PhysicsParallelFor(NumBatches, [this, &InParticles, Dt, Offset, Range, DynamicGroupId, PerGroupFriction, PerGroupThickness](int32 BatchNumber)
			{
				const int32 BatchBegin = Offset + (Chaos_PerParticleCollision_ISPC_ParallelBatchSize * BatchNumber);
		const int32 BatchEnd = FMath::Min(Range, BatchBegin + Chaos_PerParticleCollision_ISPC_ParallelBatchSize);

#if INTEL_ISPC
		MCollisionParticlesActiveView.RangeFor(
			[this, &InParticles, Dt, BatchBegin, BatchEnd, DynamicGroupId, PerGroupThickness](FSolverCollisionParticles& CollisionParticles, int32 CollisionOffset, int32 CollisionRange)
			{
				ispc::ApplyPerParticleCollisionNoFriction(
					(ispc::FVector4f*)InParticles.GetPAndInvM().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticles.AllV().GetData(),
					(const ispc::FVector3f*)CollisionParticles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticles.AllW().GetData(),
					(const ispc::FVector4f*)CollisionParticles.AllR().GetData(),
					DynamicGroupId,
					MKinematicGroupIds.GetData(),
					PerGroupThickness,
					(const uint8*)&CollisionParticles,
					(const uint8*)CollisionParticles.GetAllGeometry().GetData(),
					sizeof(FImplicitObject),
					FImplicitObject::GetOffsetOfType(),
					FImplicitObject::GetOffsetOfMargin(),
					Dt,
					CollisionOffset,
					CollisionRange,
					BatchBegin,
					BatchEnd);
			});
#endif  // #if INTEL_ISPC
			});

	}
}

}  // End namespace Chaos::Softs
