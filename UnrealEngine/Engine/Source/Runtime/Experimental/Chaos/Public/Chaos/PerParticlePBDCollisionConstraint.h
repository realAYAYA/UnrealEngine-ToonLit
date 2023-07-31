// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDActiveView.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/KinematicGeometryParticles.h"
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

class CHAOS_API FPerParticlePBDCollisionConstraint final
{
	struct FVelocityConstraint
	{
		FSolverVec3 Velocity;
		FSolverVec3 Normal;
	};

public:
	FPerParticlePBDCollisionConstraint(const TPBDActiveView<FSolverRigidParticles>& InParticlesActiveView, TArray<bool>& Collided, TArray<uint32>& DynamicGroupIds, TArray<uint32>& KinematicGroupIds, const TArray<FSolverReal>& PerGroupThickness, const TArray<FSolverReal>& PerGroupFriction)
	    : bFastPositionBasedFriction(true), MCollisionParticlesActiveView(InParticlesActiveView), MCollided(Collided), MDynamicGroupIds(DynamicGroupIds), MKinematicGroupIds(KinematicGroupIds), MPerGroupThickness(PerGroupThickness), MPerGroupFriction(PerGroupFriction) {}

	~FPerParticlePBDCollisionConstraint() {}

	inline void ApplyRange(FSolverParticles& Particles, const FSolverReal Dt, const int32 Offset, const int32 Range) const
	{
		if (bRealTypeCompatibleWithISPC && bChaos_PerParticleCollision_ISPC_Enabled && bFastPositionBasedFriction )
		{
			ApplyHelperISPC(Particles, Dt, Offset, Range);
		}
		else
		{
			ApplyHelper(Particles, Dt, Offset, Range);
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

				MCollisionParticlesActiveView.SequentialFor([this, &Particles, &Dt, &Index, DynamicGroupId, PerGroupFriction, PerGroupThickness](FSolverRigidParticles& CollisionParticles, int32 i)
				{
					const uint32 KinematicGroupId = MKinematicGroupIds[i];  // Collision group Id

					if (KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId)
					{
						return; // Bail out if the collision groups doesn't match the particle group id, or use INDEX_NONE (= global collision that affects all particle)
					}
					const FSolverRigidTransform3 Frame(CollisionParticles.X(i), CollisionParticles.R(i));
					const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(Particles.P(Index)));  // PhiWithNormal requires FReal based arguments
					FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
					const FSolverReal Phi = (FSolverReal)CollisionParticles.Geometry(i)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
					const FSolverVec3 Normal(ImplicitNormal);

					const FSolverReal Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
					if (Penetration > (FSolverReal)0.)
					{
						const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);
						Particles.P(Index) += Penetration * NormalWorld;

						if (bFastPositionBasedFriction)
						{
							FSolverVec3 VectorToPoint = Particles.P(Index) - CollisionParticles.X(i);
							const FSolverVec3 RelativeDisplacement = (Particles.P(Index) - Particles.X(Index)) - (CollisionParticles.V(i) + FSolverVec3::CrossProduct(CollisionParticles.W(i), VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
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
							FSolverVec3 VectorToPoint = Particles.P(Index) - CollisionParticles.X(i);
							Constraint.Velocity = CollisionParticles.V(i) + FSolverVec3::CrossProduct(CollisionParticles.W(i), VectorToPoint);
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

				MCollisionParticlesActiveView.SequentialFor([this, &Particles, &Dt, &Index, DynamicGroupId, PerGroupFriction, PerGroupThickness](FSolverRigidParticles& CollisionParticles, int32 i)
				{
					const uint32 KinematicGroupId = MKinematicGroupIds[i];  // Collision group Id

					if (KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId)
					{
						return; // Bail out if the collision groups doesn't match the particle group id, or use INDEX_NONE (= global collision that affects all particle)
					}
					const FSolverRigidTransform3 Frame(CollisionParticles.X(i), CollisionParticles.R(i));
					const FVec3 RigidSpacePosition(Frame.InverseTransformPosition(Particles.P(Index)));  // PhiWithNormal requires FReal based arguments
					FVec3 ImplicitNormal;                                                                // since implicits don't use FSolverReal
					const FSolverReal Phi = (FSolverReal)CollisionParticles.Geometry(i)->PhiWithNormal(RigidSpacePosition, ImplicitNormal);
					const FSolverVec3 Normal(ImplicitNormal);

					const FSolverReal Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
					if (Penetration > (FSolverReal)0.)
					{
						const FSolverVec3 NormalWorld = Frame.TransformVector(Normal);
						Particles.P(Index) += Penetration * NormalWorld;
					}
				});
			});
		}
	}

	void ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal Dt, const int32 Offset, const int32 Range) const;

private:
	bool bFastPositionBasedFriction;
	// TODO(mlentine): Need a bb hierarchy
	const TPBDActiveView<FSolverRigidParticles>& MCollisionParticlesActiveView;
	TArray<bool>& MCollided;
	const TArray<uint32>& MDynamicGroupIds;
	const TArray<uint32>& MKinematicGroupIds;
	mutable TMap<int32, FVelocityConstraint> MVelocityConstraints;
	const TArray<FSolverReal>& MPerGroupThickness;
	const TArray<FSolverReal>& MPerGroupFriction;
};

}  // End namespace Chaos::Softs
