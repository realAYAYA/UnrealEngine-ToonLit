// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDActiveView.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Chaos/KinematicGeometryParticles.h"
#endif
#include "Chaos/SoftsSolverCollisionParticles.h"
#include "Chaos/WeightedLatticeImplicitObject.h"
#include "Chaos/Levelset.h"
#include "HAL/PlatformMath.h"

#if !defined(CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT)
#define CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
const bool bChaos_PerParticleCollision_ISPC_Enabled = INTEL_ISPC && CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_PerParticleCollision_ISPC_Enabled;
#endif

namespace Chaos::Softs
{

class FPerParticlePBDCollisionConstraint final
{
	struct FVelocityConstraint
	{
		FSolverVec3 Velocity;
		FSolverVec3 Normal;
	};

public:
	FPerParticlePBDCollisionConstraint(const TPBDActiveView<FSolverCollisionParticles>& InParticlesActiveView, TArray<bool>& Collided, TArray<uint32>& DynamicGroupIds, TArray<uint32>& KinematicGroupIds, const TArray<FSolverReal>& PerGroupThickness, const TArray<FSolverReal>& PerGroupFriction)
	: bFastPositionBasedFriction(true)
	, MCollisionParticlesActiveView(InParticlesActiveView)
	, MCollided(Collided), MDynamicGroupIds(DynamicGroupIds)
	, MKinematicGroupIds(KinematicGroupIds), MPerGroupThickness(PerGroupThickness)
	, MPerGroupFriction(PerGroupFriction) {}

	FPerParticlePBDCollisionConstraint(const TPBDActiveView<FSolverCollisionParticles>& InParticlesActiveView, TArray<bool>& Collided,
		TArray<FSolverVec3>& InContacts,
		TArray<FSolverVec3>& InNormals, 
		TArray<FSolverReal>& InPhis,
		TArray<uint32>& DynamicGroupIds, TArray<uint32>& KinematicGroupIds, const TArray<FSolverReal>& PerGroupThickness, const TArray<FSolverReal>& PerGroupFriction,
		bool bWriteCCDContacts)
	    : bFastPositionBasedFriction(true), MCollisionParticlesActiveView(InParticlesActiveView), MCollided(Collided)
		, Contacts(&InContacts)
		, Normals(&InNormals)
		, Phis(&InPhis)
		, MDynamicGroupIds(DynamicGroupIds), MKinematicGroupIds(KinematicGroupIds), MPerGroupThickness(PerGroupThickness), MPerGroupFriction(PerGroupFriction)
		, Mutex(bWriteCCDContacts ? new FCriticalSection : nullptr) {}

	~FPerParticlePBDCollisionConstraint() 
	{
		delete Mutex;
	}

	inline void ApplyRange(FSolverParticles& Particles, const FSolverReal Dt, const int32 Offset, const int32 Range) const
	{
		// NOTE: currently using ISPC with TWeightedLatticeImplicitObject<FLevelSet> is significantly slower than not using ISPC (largely because it has not been fully implemented)
		if (bRealTypeCompatibleWithISPC && bChaos_PerParticleCollision_ISPC_Enabled && bFastPositionBasedFriction )
		{
			ApplyHelperISPC(Particles, Dt, Offset, Range);
		}
		else
		{
			if (Mutex)
			{
				ApplyHelper<true>(Particles, Dt, Offset, Range);
			}
			else
			{
				ApplyHelper<false>(Particles, Dt, Offset, Range);
			}
		}
	}

	void ApplyFriction(FSolverParticles& Particles, const FSolverReal Dt, const int32 Index) const
	{
		check(!bFastPositionBasedFriction);  // Do not call this function if this is setup to run with fast PB friction

		if (!MVelocityConstraints.Contains(Index))
		{
			return;
		}
		const FSolverReal VN = FSolverVec3::DotProduct(Particles.V(Index), MVelocityConstraints[Index].Normal);
		const FSolverReal VNBody = FSolverVec3::DotProduct(MVelocityConstraints[Index].Velocity, MVelocityConstraints[Index].Normal);
		const FSolverVec3 VTBody = MVelocityConstraints[Index].Velocity - VNBody * MVelocityConstraints[Index].Normal;
		const FSolverVec3 VTRelative = Particles.V(Index) - VN * MVelocityConstraints[Index].Normal - VTBody;
		const FSolverReal VTRelativeSize = VTRelative.Size();
		const FSolverReal VNMax = FMath::Max(VN, VNBody);
		const FSolverReal VNDelta = VNMax - VN;
		const FSolverReal CoefficientOfFriction = MPerGroupFriction[MDynamicGroupIds[Index]];
		check(CoefficientOfFriction > 0);
		const FSolverReal Friction = CoefficientOfFriction * VNDelta < VTRelativeSize ? CoefficientOfFriction * VNDelta / VTRelativeSize : 1;
		Particles.V(Index) = VNMax * MVelocityConstraints[Index].Normal + VTBody + VTRelative * (1 - Friction);
	}

private:
	template<bool bLockAndWriteContacts>
	inline void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 Offset, const int32 Range) const
	{
		const uint32 DynamicGroupId = MDynamicGroupIds[Offset];  // Particle group Id, must be the same across the entire range
		const FSolverReal PerGroupFriction = MPerGroupFriction[DynamicGroupId];
		const FSolverReal PerGroupThickness = MPerGroupThickness[DynamicGroupId];

		if (PerGroupFriction > (FSolverReal)UE_KINDA_SMALL_NUMBER)
		{
			PhysicsParallelFor(Range - Offset, [this, &Particles, Dt, Offset, DynamicGroupId, PerGroupFriction, PerGroupThickness](int32 i)
			{
				const int32 Index = Offset + i;

				if (Particles.InvM(Index) == (FSolverReal)0.)
				{
					return;  // Continue
				}

				MCollisionParticlesActiveView.SequentialFor([this, &Particles, &Dt, &Index, DynamicGroupId, PerGroupFriction, PerGroupThickness](FSolverCollisionParticles& CollisionParticles, int32 i)
				{
					const uint32 KinematicGroupId = MKinematicGroupIds[i];  // Collision group Id

					if ((KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId ) || CollisionParticles.GetGeometry(i)->GetType() == Chaos::ImplicitObjectType::WeightedLatticeBone)
					{
						return; // Bail out if the collision groups doesn't match the particle group id, or use INDEX_NONE (= global collision that affects all particle)
					}
					const FSolverRigidTransform3 Frame(CollisionParticles.GetX(i), CollisionParticles.GetR(i));
					const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(Particles.P(Index)));  // PhiWithNormal requires FReal based arguments
					FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
					FSolverReal Phi;
					int32 VelocityBone = i;
					FSolverReal Penetration;
					if (const TWeightedLatticeImplicitObject<FLevelSet>* LevelSet = CollisionParticles.GetGeometry(i)->GetObject< TWeightedLatticeImplicitObject<FLevelSet> >())
					{
						FWeightedLatticeImplicitObject::FEmbeddingCoordinate SurfaceCoord;
						Phi = (FSolverReal)LevelSet->PhiWithNormalAndSurfacePoint(RigidSpacePosition, ImplicitNormal, SurfaceCoord);
						Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
						if (Penetration > (FSolverReal)0.)
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
						Phi = (FSolverReal)CollisionParticles.GetGeometry(i)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
						Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
					}
					const FSolverVec3 Normal(ImplicitNormal);

					if (Penetration > (FSolverReal)0.)
					{
						const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);

						if (bLockAndWriteContacts)
						{
							check(Mutex);
							checkSlow(Contacts);
							checkSlow(Normals);
							checkSlow(Phis);
							FScopeLock Lock(Mutex);
							Contacts->Emplace(Particles.P(Index));
							Normals->Emplace(NormalWorld);
							Phis->Emplace(Phi);
						}

						Particles.P(Index) += Penetration * NormalWorld;

						if (bFastPositionBasedFriction)
						{
							FSolverVec3 VectorToPoint = Particles.GetP(Index) - CollisionParticles.GetX(VelocityBone);
							const FSolverVec3 RelativeDisplacement = (Particles.GetP(Index) - Particles.GetX(Index)) - (CollisionParticles.V(VelocityBone) + FSolverVec3::CrossProduct(CollisionParticles.W(VelocityBone), VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
							const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - FSolverVec3::DotProduct(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
							const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Size();
							if (RelativeDisplacementTangentLength >= UE_SMALL_NUMBER)
							{
								const FSolverReal PositionCorrection = FMath::Min<FSolverReal>(Penetration * PerGroupFriction, RelativeDisplacementTangentLength);
								const FSolverReal CorrectionRatio = PositionCorrection / RelativeDisplacementTangentLength;
								Particles.P(Index) -= CorrectionRatio * RelativeDisplacementTangent;
							}
						}
						else
						{
							// Note, to fix: Only use fast position based friction for now, since adding to TMaps here is not thread safe when calling Apply on multiple threads (will cause crash)
							FVelocityConstraint Constraint;
							FSolverVec3 VectorToPoint = Particles.GetP(Index) - CollisionParticles.GetX(VelocityBone);
							Constraint.Velocity = CollisionParticles.V(VelocityBone) + FSolverVec3::CrossProduct(CollisionParticles.W(VelocityBone), VectorToPoint);
							Constraint.Normal = Frame.TransformVector(Normal);
						
							MVelocityConstraints.Add(Index, Constraint);
						}
					}
				});
			});
		}
		else
		{
			PhysicsParallelFor(Range - Offset, [this, &Particles, Dt, Offset, DynamicGroupId, PerGroupFriction, PerGroupThickness](int32 i)
			{
				const int32 Index = Offset + i;

				if (Particles.InvM(Index) == 0)
				{
					return;  // Continue
				}

				MCollisionParticlesActiveView.SequentialFor([this, &Particles, &Dt, &Index, DynamicGroupId, PerGroupFriction, PerGroupThickness](FSolverCollisionParticles& CollisionParticles, int32 i)
				{
					const uint32 KinematicGroupId = MKinematicGroupIds[i];  // Collision group Id

					if ((KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId) || CollisionParticles.GetGeometry(i)->GetType() == Chaos::ImplicitObjectType::WeightedLatticeBone)
					{
						return; // Bail out if the collision groups doesn't match the particle group id, or use INDEX_NONE (= global collision that affects all particle)
					}
					const FSolverRigidTransform3 Frame(CollisionParticles.GetX(i), CollisionParticles.GetR(i));
					const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(Particles.P(Index)));  // PhiWithNormal requires FReal based arguments
					FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
					const FSolverReal Phi = (FSolverReal)CollisionParticles.GetGeometry(i)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
					const FSolverVec3 Normal(ImplicitNormal);

					const FSolverReal Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
					if (Penetration > (FSolverReal)0.)
					{
						const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);
						if (bLockAndWriteContacts)
						{
							check(Mutex);
							checkSlow(Contacts);
							checkSlow(Normals);
							checkSlow(Phis);
							FScopeLock Lock(Mutex);
							Contacts->Emplace(Particles.P(Index));
							Normals->Emplace(NormalWorld);
							Phis->Emplace(Phi);
						}

						Particles.P(Index) += Penetration * NormalWorld;
					}
				});
			});
		}
	}

	CHAOS_API void ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal Dt, const int32 Offset, const int32 Range) const;

private:
	bool bFastPositionBasedFriction;
	// TODO(mlentine): Need a bb hierarchy
	const TPBDActiveView<FSolverCollisionParticles>& MCollisionParticlesActiveView;
	TArray<bool>& MCollided;
	TArray<FSolverVec3>* const Contacts = nullptr;
	TArray<FSolverVec3>* const Normals = nullptr;
	TArray<FSolverReal>* const Phis = nullptr;
	const TArray<uint32>& MDynamicGroupIds;
	const TArray<uint32>& MKinematicGroupIds;
	mutable TMap<int32, FVelocityConstraint> MVelocityConstraints;
	const TArray<FSolverReal>& MPerGroupThickness;
	const TArray<FSolverReal>& MPerGroupFriction;
	FCriticalSection* const Mutex = nullptr;
};

}  // End namespace Chaos::Softs
