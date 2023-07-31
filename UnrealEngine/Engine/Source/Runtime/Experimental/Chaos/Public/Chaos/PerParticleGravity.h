// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/DynamicParticles.h"
#include "Chaos/PerParticleRule.h"
#include "Chaos/ParticleHandle.h"

namespace Chaos
{
	class FPerParticleGravity : public FPerParticleRule
	{
	public:
		FPerParticleGravity()
			: MAcceleration(FVec3(0, 0, (FReal)-980.665)) {}
		FPerParticleGravity(const FVec3& G)
			: MAcceleration(G) {}
		FPerParticleGravity(const FVec3& Direction, const FReal Magnitude)
			: MAcceleration(Direction * Magnitude) {}
		virtual ~FPerParticleGravity() {}

		// TODO: Remove this - we should no longer be using indices directly.
		//       This has been kept around for cloth, which uses it in
		//       PBDEvolution.
		template<class T_PARTICLES>
		inline void ApplyHelper(T_PARTICLES& InParticles, const FReal Dt, const int Index) const
		{
			InParticles.Acceleration(Index) += MAcceleration;
		}
		inline void Apply(FPBDParticles& InParticles, const FReal Dt, const int Index) const override //-V762
		{
			ApplyHelper(InParticles, Dt, Index);
		}

		void Apply(TTransientPBDRigidParticleHandle<FReal, 3>& Particle, const FReal Dt) const override //-V762
		{
			if(Particle.GravityEnabled())
			{
				Particle.Acceleration() += MAcceleration;
			}
		}

		void SetAcceleration(const FVec3& Acceleration)
		{ MAcceleration = Acceleration; }

		const FVec3& GetAcceleration() const
		{ return MAcceleration; }

	private:
		FVec3 MAcceleration;
	};

	template<class T, int d>
	using TPerParticleGravity UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FPerParticleGravity instead") = FPerParticleGravity;
}
