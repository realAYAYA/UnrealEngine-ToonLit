// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDSoftBodyCollisionConstraint.h"
#include "Chaos/Levelset.h"
#include "Chaos/SoftsEvolutionLinearSystem.h"
#include "Chaos/WeightedLatticeImplicitObject.h"
#include "ChaosStats.h"
#include "HAL/IConsoleManager.h"
#include "Chaos/Framework/Parallel.h"
#include "Misc/ScopeLock.h"
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
bool bChaos_SoftBodyCollision_ISPC_Enabled = CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarChaosSoftBodyCollisionISPCEnabled(TEXT("p.Chaos.SoftBodyCollision.ISPC"), bChaos_SoftBodyCollision_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in per particle collisions"));
#endif
#endif

static int32 Chaos_SoftBodyCollision_ISPC_ParallelBatchSize = 128;
#if !UE_BUILD_SHIPPING
FAutoConsoleVariableRef CVarChaosSoftBodyCollisionISPCParallelBatchSize(TEXT("p.Chaos.SoftBodyCollision.ISPC.ParallelBatchSize"), Chaos_SoftBodyCollision_ISPC_ParallelBatchSize, TEXT("Parallel batch size for ISPC"));
#endif

namespace Chaos::Softs {

// Helper function to call PhiWithNormal and return data to ISPC
extern "C" void GetPhiWithNormalCollisionParticleRange(const uint8 * CollisionParticlesRange, const FSolverReal * InV, FSolverReal * Normal, FSolverReal * Phi, const int32 i, const int32 ProgramCount, const int32 Mask)
{
	const FSolverCollisionParticlesRange& C = *(const FSolverCollisionParticlesRange*)CollisionParticlesRange;

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
extern "C" void GetPhiWithNormalAndVelocityBoneCollisionParticleRange(const uint8 * CollisionParticlesRange, const FSolverReal * InV, FSolverReal * Normal, FSolverReal * Phi, int32 * VelocityBone, const int32 i, const FSolverReal Thickness, const int32 ProgramCount, const int32 Mask)
{
	const FSolverCollisionParticlesRange& C = *(const FSolverCollisionParticlesRange*)CollisionParticlesRange;

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


void FPBDSoftBodyCollisionConstraintBase::Apply(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPBDSoftBodyCollisionConstraint);
	const bool bLockAndWriteContacts = bWriteDebugContacts && CollisionParticleCollided && Contacts && Normals && Phis;
	const bool bWithFriction = FrictionCoefficient > (FSolverReal)UE_KINDA_SMALL_NUMBER;

	if (CollisionParticles.IsEmpty())
	{
		return;
	}

	if (bUseCCD)
	{
		if (bWithFriction)
		{
			if (bLockAndWriteContacts)
			{
				ApplyInternalCCD<true, true>(Particles, Dt, CollisionParticles);
			}
			else
			{
				ApplyInternalCCD<true, false>(Particles, Dt, CollisionParticles);
			}
		}
		else
		{
			if (bLockAndWriteContacts)
			{
				ApplyInternalCCD<false, true>(Particles, Dt, CollisionParticles);
			}
			else
			{
				ApplyInternalCCD<false, false>(Particles, Dt, CollisionParticles);
			}
		}
	}
	else
	{
		if (bWithFriction)
		{
			if (bRealTypeCompatibleWithISPC && bChaos_SoftBodyCollision_ISPC_Enabled)
			{
				ApplyInternalISPC<true>(Particles, Dt, CollisionParticles);
			}
			else if (bLockAndWriteContacts)
			{
				ApplyInternal<true, true>(Particles, Dt, CollisionParticles);
			}
			else
			{
				ApplyInternal<true, false>(Particles, Dt, CollisionParticles);
			}
		}
		else
		{
			if (bRealTypeCompatibleWithISPC && bChaos_SoftBodyCollision_ISPC_Enabled)
			{
				ApplyInternalISPC<false>(Particles, Dt, CollisionParticles);
			}
			else if (bLockAndWriteContacts)
			{
				ApplyInternal<false, true>(Particles, Dt, CollisionParticles);
			}
			else
			{
				ApplyInternal<false, false>(Particles, Dt, CollisionParticles);
			}
		}
	}
}


template<bool bLockAndWriteContacts, bool bWithFriction>
void FPBDSoftBodyCollisionConstraintBase::ApplyInternal(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const
{
	FPAndInvM* const PAndInvM = Particles.GetPAndInvM().GetData();
	const FSolverVec3* const X = Particles.XArray().GetData();
	PhysicsParallelFor(Particles.GetRangeSize(), [this, Dt, &PAndInvM, &X, &CollisionParticles](int32 Index)
	{
		if (PAndInvM[Index].InvM == (FSolverReal)0.)
		{
			return;
		}

		for (const FSolverCollisionParticlesRange& CollisionParticlesRange : CollisionParticles)
		{
			for(int32 CollisionIndex = 0; CollisionIndex < CollisionParticlesRange.GetRangeSize(); ++CollisionIndex)
			{
				if (CollisionParticlesRange.GetGeometry(CollisionIndex)->GetType() == Chaos::ImplicitObjectType::WeightedLatticeBone)
				{
					continue;
				}

				const FSolverRigidTransform3 Frame(CollisionParticlesRange.GetX(CollisionIndex), CollisionParticlesRange.R(CollisionIndex));
				const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(PAndInvM[Index].P));  // PhiWithNormal requires FReal based arguments
				FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
				FSolverReal Phi;
				int32 VelocityBone = CollisionIndex;
				FSolverReal Penetration;
				if (const TWeightedLatticeImplicitObject<FLevelSet>* LevelSet = CollisionParticlesRange.GetGeometry(CollisionIndex)->GetObject< TWeightedLatticeImplicitObject<FLevelSet> >())
				{
					FWeightedLatticeImplicitObject::FEmbeddingCoordinate SurfaceCoord;
					Phi = (FSolverReal)LevelSet->PhiWithNormalAndSurfacePoint(RigidSpacePosition, ImplicitNormal, SurfaceCoord);
					Penetration = CollisionThickness - Phi; // This is related to the Normal impulse
					if (bWithFriction && Penetration > (FSolverReal)0.)
					{
						const int32 StrongestBone = SurfaceCoord.GreatestInfluenceBone(LevelSet->GetBoneData());
						if (StrongestBone != INDEX_NONE)
						{
							VelocityBone = LevelSet->GetSolverBoneIndices()[StrongestBone];
						}
					}
				}
				else
				{
					Phi = (FSolverReal)CollisionParticlesRange.GetGeometry(CollisionIndex)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
					Penetration = CollisionThickness - Phi; // This is related to the Normal impulse
				}
				const FSolverVec3 Normal(ImplicitNormal);

				if (Penetration > (FSolverReal)0.)
				{
					const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);

					if constexpr (bLockAndWriteContacts)
					{
						checkSlow(Contacts);
						checkSlow(Normals);
						checkSlow(Phis);
						FScopeLock Lock(&DebugMutex);
						CollisionParticlesRange.GetArrayView(*CollisionParticleCollided)[CollisionIndex] = true;
						Contacts->Emplace(PAndInvM[Index].P);
						Normals->Emplace(NormalWorld);
						Phis->Emplace(Phi);
					}

					PAndInvM[Index].P += Penetration * NormalWorld;

					if constexpr (bWithFriction)
					{
						const FSolverVec3 VectorToPoint = PAndInvM[Index].P - CollisionParticlesRange.GetX(VelocityBone);
						const FSolverVec3 RelativeDisplacement = (PAndInvM[Index].P - X[Index]) - (CollisionParticlesRange.V(VelocityBone) + FSolverVec3::CrossProduct(CollisionParticlesRange.W(VelocityBone), VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
						const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - FSolverVec3::DotProduct(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
						const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Size();
						if (RelativeDisplacementTangentLength >= UE_SMALL_NUMBER)
						{
							const FSolverReal PositionCorrection = FMath::Min<FSolverReal>(Penetration * FrictionCoefficient, RelativeDisplacementTangentLength);
							const FSolverReal CorrectionRatio = PositionCorrection / RelativeDisplacementTangentLength;
							PAndInvM[Index].P -= CorrectionRatio * RelativeDisplacementTangent;
						}
					}
				}
			}
		}
	});
}

template<bool bLockAndWriteContacts, bool bWithFriction>
void FPBDSoftBodyCollisionConstraintBase::ApplyInternalCCD(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const
{
	FPAndInvM* const PAndInvM = Particles.GetPAndInvM().GetData();
	const FSolverVec3* const X = Particles.XArray().GetData();
	PhysicsParallelFor(Particles.GetRangeSize(), [this, Dt, &PAndInvM, &X, &CollisionParticles](int32 Index)
	{
		if (PAndInvM[Index].InvM == (FSolverReal)0.)
		{
			return;
		}

		for (const FSolverCollisionParticlesRange& CollisionParticlesRange : CollisionParticles)
		{
			const TConstArrayView<FSolverRigidTransform3> CollisionTransforms = CollisionParticlesRange.GetConstArrayView(LastCollisionTransforms);

			for (int32 CollisionIndex = 0; CollisionIndex < CollisionParticlesRange.GetRangeSize(); ++CollisionIndex)
			{
				if (CollisionParticlesRange.GetGeometry(CollisionIndex)->GetType() == Chaos::ImplicitObjectType::WeightedLatticeBone)
				{
					continue;
				}

				const FSolverRigidTransform3 Frame(CollisionParticlesRange.GetX(CollisionIndex), CollisionParticlesRange.R(CollisionIndex));

				const Pair<FVec3, bool> PointPair = CollisionParticlesRange.GetGeometry(CollisionIndex)->FindClosestIntersection(  // Geometry operates in FReal
					FVec3(CollisionTransforms[CollisionIndex].InverseTransformPositionNoScale(X[Index])),        // hence the back and forth
					FVec3(Frame.InverseTransformPositionNoScale(PAndInvM[Index].P)), (FReal)CollisionThickness);                   // FVec3/FReal conversions

				if (PointPair.Second)
				{
					const FSolverVec3 Normal = FSolverVec3(CollisionParticlesRange.GetGeometry(CollisionIndex)->Normal(PointPair.First));
					const FSolverVec3 NormalWorld = Frame.TransformVectorNoScale(Normal);
					const FSolverVec3 ContactWorld = Frame.TransformPositionNoScale(UE::Math::TVector<FSolverReal>(PointPair.First));

					if constexpr (bLockAndWriteContacts)
					{
						checkSlow(Contacts);
						checkSlow(Normals);
						FScopeLock Lock(&DebugMutex);
						CollisionParticlesRange.GetArrayView(*CollisionParticleCollided)[CollisionIndex] = true;
						Contacts->Emplace(ContactWorld);
						Normals->Emplace(NormalWorld);
					}
					const FSolverVec3 Direction = ContactWorld - PAndInvM[Index].P;
					const FSolverReal Penetration = FMath::Max((FSolverReal)0., FSolverVec3::DotProduct(NormalWorld, Direction)) + (FSolverReal)UE_THRESH_POINT_ON_PLANE;

					PAndInvM[Index].P += Penetration * NormalWorld;
					
					if constexpr (bWithFriction)
					{
						// Friction
						int32 VelocityBone = CollisionIndex;
						if (const TWeightedLatticeImplicitObject<FLevelSet>* LevelSet = CollisionParticlesRange.GetGeometry(CollisionIndex)->GetObject< TWeightedLatticeImplicitObject<FLevelSet> >())
						{
							TArray<FWeightedLatticeImplicitObject::FEmbeddingCoordinate> Coordinates;
							LevelSet->GetEmbeddingCoordinates(PointPair.First, Coordinates, false);
							int32 ClosestCoordIndex = INDEX_NONE;
							double ClosestCoordPhi = UE_BIG_NUMBER;
							for (int32 CoordIndex = 0; CoordIndex < Coordinates.Num(); ++CoordIndex)
							{
								FVec3 NormalUnused;
								const double CoordPhi = FMath::Abs(LevelSet->GetEmbeddedObject()->PhiWithNormal(Coordinates[CoordIndex].UndeformedPosition(LevelSet->GetGrid()), NormalUnused));
								if (CoordPhi < ClosestCoordPhi)
								{
									ClosestCoordIndex = CoordIndex;
									ClosestCoordPhi = CoordPhi;
								}
							}
							if (ClosestCoordIndex != INDEX_NONE)
							{
								const int32 StrongestBone = Coordinates[ClosestCoordIndex].GreatestInfluenceBone(LevelSet->GetBoneData());
								if (StrongestBone != INDEX_NONE)
								{
									VelocityBone = LevelSet->GetSolverBoneIndices()[StrongestBone];
								}
							}
						}

						const FSolverVec3 VectorToPoint = PAndInvM[Index].P - CollisionParticlesRange.GetX(VelocityBone);
						const FSolverVec3 RelativeDisplacement = (PAndInvM[Index].P - X[Index]) - (CollisionParticlesRange.V(VelocityBone) + FSolverVec3::CrossProduct(CollisionParticlesRange.W(VelocityBone), VectorToPoint)) * Dt;  // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
						const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - FSolverVec3::DotProduct(RelativeDisplacement, NormalWorld) * NormalWorld;  // Project displacement into the tangential plane
						const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Size();
						if (RelativeDisplacementTangentLength >= UE_SMALL_NUMBER)
						{
							const FSolverReal PositionCorrection = FMath::Min<FSolverReal>(Penetration * FrictionCoefficient, RelativeDisplacementTangentLength);
							const FSolverReal CorrectionRatio = PositionCorrection / RelativeDisplacementTangentLength;
							PAndInvM[Index].P -= CorrectionRatio * RelativeDisplacementTangent;
						}
					}
				}
			}
		}
	});
}

template<bool bWithFriction>
void FPBDSoftBodyCollisionConstraintBase::ApplyInternalISPC(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const
{
#if INTEL_ISPC
	check(bRealTypeCompatibleWithISPC);
	const int32 NumBatches = FMath::CeilToInt((FSolverReal)(Particles.GetRangeSize()) / (FSolverReal)Chaos_SoftBodyCollision_ISPC_ParallelBatchSize);
	PhysicsParallelFor(NumBatches, [this, &Particles, Dt, &CollisionParticles](int32 BatchNumber)
	{
		const int32 BatchBegin = (Chaos_SoftBodyCollision_ISPC_ParallelBatchSize * BatchNumber);
		const int32 BatchEnd = FMath::Min(Particles.GetRangeSize(), BatchBegin + Chaos_SoftBodyCollision_ISPC_ParallelBatchSize);

		for (const FSolverCollisionParticlesRange& CollisionParticlesRange : CollisionParticles)
		{
			if constexpr (bWithFriction)
			{
				ispc::ApplyPerParticleCollisionFastFrictionNoGroupCheck(
					(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
					(const ispc::FVector3f*)Particles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.GetV().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.GetW().GetData(),
					(const ispc::FVector4f*)CollisionParticlesRange.GetR().GetData(),
					FrictionCoefficient,
					CollisionThickness,
					(const uint8*)&CollisionParticlesRange,
					(const uint8*)CollisionParticlesRange.GetAllGeometry().GetData(),
					sizeof(FImplicitObject),
					FImplicitObject::GetOffsetOfType(),
					FImplicitObject::GetOffsetOfMargin(),
					Dt,
					CollisionParticlesRange.GetRangeSize(),
					BatchBegin,
					BatchEnd);
			}
			else
			{
				ispc::ApplyPerParticleCollisionNoFrictionNoGroupCheck(
					(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
					(const ispc::FVector3f*)Particles.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.GetV().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.XArray().GetData(),
					(const ispc::FVector3f*)CollisionParticlesRange.GetW().GetData(),
					(const ispc::FVector4f*)CollisionParticlesRange.GetR().GetData(),
					CollisionThickness,
					(const uint8*)&CollisionParticlesRange,
					(const uint8*)CollisionParticlesRange.GetAllGeometry().GetData(),
					sizeof(FImplicitObject),
					FImplicitObject::GetOffsetOfType(),
					FImplicitObject::GetOffsetOfMargin(),
					Dt,
					CollisionParticlesRange.GetRangeSize(),
					BatchBegin,
					BatchEnd);
			}
		}
	});
#endif  // #if INTEL_ISPC
}

void FPBDSoftBodyCollisionConstraintBase::UpdateLinearSystem(const FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles, FEvolutionLinearSystem& LinearSystem) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPBDSoftBodyCollisionConstraint_UpdateLinearSystem);

	if (CollisionParticles.IsEmpty() || ProximityStiffness == (FSolverReal)0.f)
	{
		return;
	}

	// Just going to allocate enough space for all possible collisions. 
	LinearSystem.ReserveForParallelAdd(Particles.GetRangeSize(), 0);

	// Just proximity forces for now
	const FPAndInvM* const PAndInvM = Particles.GetPAndInvM().GetData();
	const FSolverReal ClampedFriction = FMath::Clamp(FrictionCoefficient, (FSolverReal)0., (FSolverReal)1.);
	PhysicsParallelFor(Particles.GetRangeSize(), [this, Dt, ClampedFriction, &Particles, &PAndInvM, &CollisionParticles, &LinearSystem](int32 Index)
	{
		if (PAndInvM[Index].InvM == (FSolverReal)0.)
		{
			return;
		}

		bool bAddForce = false;
		FSolverVec3 Force(0.f);
		FSolverMatrix33 DfDx(0.f);
		for (const FSolverCollisionParticlesRange& CollisionParticlesRange : CollisionParticles)
		{
			for (int32 CollisionIndex = 0; CollisionIndex < CollisionParticlesRange.GetRangeSize(); ++CollisionIndex)
			{
				if (CollisionParticlesRange.GetGeometry(CollisionIndex)->GetType() == Chaos::ImplicitObjectType::WeightedLatticeBone)
				{
					continue;
				}

				const FSolverRigidTransform3 Frame(CollisionParticlesRange.GetX(CollisionIndex), CollisionParticlesRange.R(CollisionIndex));
				const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(PAndInvM[Index].P));  // PhiWithNormal requires FReal based arguments
				FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
				const FSolverReal Phi = (FSolverReal)CollisionParticlesRange.GetGeometry(CollisionIndex)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
				const FSolverReal Penetration = CollisionThickness - Phi; // This is related to the Normal impulse
				const FSolverVec3 Normal(ImplicitNormal);

				if (Penetration > (FSolverReal)0.)
				{
					bAddForce = true;

					const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);

					// Repulsion force
					Force += ProximityStiffness * Penetration * NormalWorld;

					// Blend between a zero-length spring (stiction) and repulsion force based on friction
					// DfDx = -ProximityStiffness * ((1-FrictionCoefficient)*OuterProduct(N,N) + FrictionCoefficient * Identity)
					// Nothing here to match velocities... not sure if it's necessary, but this is a very stable force at least unlike any velocity-based thing.

					DfDx += -ProximityStiffness * (((FSolverReal)1. - ClampedFriction) * FSolverMatrix33::OuterProduct(NormalWorld, NormalWorld) + FSolverMatrix33(ClampedFriction, ClampedFriction, ClampedFriction));
				}
			}
		}

		if (bAddForce)
		{
			LinearSystem.AddForce(Particles, Force, Index, Dt);
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDx, nullptr, Index, Index, Dt);
		}
	});
}

void FPBDSoftBodyCollisionConstraint::SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
{
	if (IsCollisionThicknessMutable(PropertyCollection))
	{
		CollisionThickness = MeshScale * GetCollisionThickness(PropertyCollection);
	}
	if (IsFrictionCoefficientMutable(PropertyCollection))
	{
		FrictionCoefficient = GetFrictionCoefficient(PropertyCollection);
	}
	if (IsUseCCDMutable(PropertyCollection))
	{
		bUseCCD = GetUseCCD(PropertyCollection);
	}
	if (IsProximityStiffnessMutable(PropertyCollection))
	{
		ProximityStiffness = GetProximityStiffness(PropertyCollection);
	}
}

}  // End namespace Chaos::Softs
