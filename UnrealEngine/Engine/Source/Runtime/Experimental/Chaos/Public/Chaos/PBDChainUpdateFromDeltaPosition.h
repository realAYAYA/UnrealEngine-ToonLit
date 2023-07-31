// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/ParticleRule.h"

namespace Chaos
{
class FPBDChainUpdateFromDeltaPosition : public FParticleRule
{
  public:
	FPBDChainUpdateFromDeltaPosition(TArray<TArray<int32>>&& Constraints, const FReal Damping)
	    : MConstraints(MoveTemp(Constraints)), MDamping(Damping) {}
	virtual ~FPBDChainUpdateFromDeltaPosition() {}

	inline void Apply(FPBDParticles& InParticles, const FReal Dt) const override //-V762
	{
		PhysicsParallelFor(MConstraints.Num(), [&](int32 index) {
			{
				int32 i = MConstraints[index][0];
				InParticles.V(i) = (InParticles.P(i) - InParticles.X(i)) / Dt;
				InParticles.X(i) = InParticles.P(i);
			}
			for (int i = 2; i < MConstraints[index].Num(); ++i)
			{
				int32 p = MConstraints[index][i];
				int32 pm1 = MConstraints[index][i - 1];
				InParticles.V(pm1) = (InParticles.P(pm1) - InParticles.X(pm1)) / Dt - MDamping * (InParticles.P(p) - InParticles.X(p)) / Dt;
				InParticles.X(pm1) = InParticles.P(pm1);
			}
			{
				int32 i = MConstraints[index][MConstraints[index].Num() - 1];
				InParticles.V(i) = (InParticles.P(i) - InParticles.X(i)) / Dt;
				InParticles.X(i) = InParticles.P(i);
			}
		});
	}

  private:
	TArray<TArray<int32>> MConstraints;
	FReal MDamping;
};

template<class T, int d>
using TPBDChainUpdateFromDeltaPosition UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FPBDChainUpdateFromDeltaPosition instead") = FPBDChainUpdateFromDeltaPosition;
}
