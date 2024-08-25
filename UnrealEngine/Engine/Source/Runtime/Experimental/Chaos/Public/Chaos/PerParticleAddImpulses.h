// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PerParticleRule.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/Utilities.h"

namespace Chaos
{
	class FPerParticleAddImpulses : public FPerParticleRule
	{
	public:
		FPerParticleAddImpulses() {}
		virtual ~FPerParticleAddImpulses() {}

		template<class T_PARTICLES>
		inline void ApplyHelper(T_PARTICLES& InParticles, const FReal Dt, const int32 Index) const
		{
			InParticles.SetV(Index, InParticles.GetV(Index) + InParticles.LinearImpulseVelocity(Index));
		}

		inline void Apply(FDynamicParticles& InParticles, const FReal Dt, const int32 Index) const override //-V762
		{
			// @todo(mlentine): Is this something we want to support?
			ensure(false);
		}

		inline void Apply(TRigidParticles<FReal, 3>& InParticles, const FReal Dt, const int32 Index) const override //-V762
		{
			if (InParticles.InvM(Index) == 0 || InParticles.Disabled(Index) || InParticles.Sleeping(Index))
				return;
			ApplyHelper(InParticles, Dt, Index);

			//
			// TODO: This is the first-order approximation.
			//       If needed, we might eventually want to do a second order Euler's Equation,
			//       but if we do that we'll need to do a transform into a rotating reference frame.
			//       Just using W += InvI * (Torque - W x (I * W)) * dt is not correct, since Torque
			//		 and W are in an inertial frame.
			//

			InParticles.SetW(Index, InParticles.GetW(Index) + InParticles.AngularImpulseVelocity(Index));
			InParticles.LinearImpulseVelocity(Index) = FVec3(0);
			InParticles.AngularImpulseVelocity(Index) = FVec3(0);
		}

		inline void Apply(TTransientPBDRigidParticleHandle<FReal, 3>& Particle, const FReal Dt) const override //-V762
		{
			Particle.SetV(Particle.GetV() + Particle.LinearImpulseVelocity());
			Particle.SetW(Particle.GetW() + Particle.AngularImpulseVelocity());
			Particle.LinearImpulseVelocity() = FVec3(0);
			Particle.AngularImpulseVelocity() = FVec3(0);
		}
	};

	template<class T, int d>
	using TPerParticleAddImpulses UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FPerParticleAddImpulses instead") = FPerParticleAddImpulses;
}
