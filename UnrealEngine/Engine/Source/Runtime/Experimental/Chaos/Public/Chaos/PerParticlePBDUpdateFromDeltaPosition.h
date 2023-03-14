// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
class FPerParticlePBDUpdateFromDeltaPosition : public FPerParticleRule
{
  public:
	  FPerParticlePBDUpdateFromDeltaPosition() {}
	virtual ~FPerParticlePBDUpdateFromDeltaPosition() {}

	template<class T_PARTICLES>
	inline void ApplyHelper(T_PARTICLES& InParticles, const FReal Dt, const int32 Index) const
	{
		InParticles.V(Index) = (InParticles.P(Index) - InParticles.X(Index)) / Dt;
		//InParticles.X(Index) = InParticles.P(Index);
	}

	inline void Apply(FPBDParticles& InParticles, const FReal Dt, const int32 Index) const override //-V762
	{
		InParticles.V(Index) = (InParticles.P(Index) - InParticles.X(Index)) / Dt;
		InParticles.X(Index) = InParticles.P(Index);
	}

	inline void Apply(TPBDRigidParticles<FReal, 3>& InParticles, const FReal Dt, const int32 Index) const override //-V762
	{
		ApplyHelper(InParticles, Dt, Index);
		InParticles.W(Index) = FRotation3::CalculateAngularVelocity(InParticles.R(Index), InParticles.Q(Index), Dt);
	}

	inline void Apply(FPBDRigidParticleHandle* Particle, const FReal Dt) const override //-V762
	{
#if CHAOS_PARTICLE_ACTORTRANSFORM
		const FVec3& CenterOfMass = Particle->CenterOfMass();
		const FVec3 CenteredX = Particle->X() + Particle->R().RotateVector(CenterOfMass);
		const FVec3 CenteredP = Particle->P() + Particle->Q().RotateVector(CenterOfMass);
		Particle->V() = FVec3::CalculateVelocity(CenteredX, CenteredP, Dt);
#else
		Particle->V() = FVec3::CalculateVelocity(Particle->X(), Particle->P(), Dt);
#endif
		Particle->W() = FRotation3::CalculateAngularVelocity(Particle->R(), Particle->Q(), Dt);
	}

	inline void Apply(TTransientPBDRigidParticleHandle<FReal, 3>& Particle, const FReal Dt) const override //-V762
	{
#if CHAOS_PARTICLE_ACTORTRANSFORM
		const FVec3& CenterOfMass = Particle.CenterOfMass();
		const FVec3 CenteredX = Particle.X() + Particle.R().RotateVector(CenterOfMass);
		const FVec3 CenteredP = Particle.P() + Particle.Q().RotateVector(CenterOfMass);
		Particle.V() = FVec3::CalculateVelocity(CenteredX, CenteredP, Dt);
#else
		Particle.V() = FVec3::CalculateVelocity(Particle.X(), Particle.P(), Dt);
#endif
		Particle.W() = FRotation3::CalculateAngularVelocity(Particle.R(), Particle.Q(), Dt);
	}
};

template<class T, int d>
using TPerParticlePBDUpdateFromDeltaPosition UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FPerParticlePBDUpdateFromDeltaPosition instead") = FPerParticlePBDUpdateFromDeltaPosition;
}
