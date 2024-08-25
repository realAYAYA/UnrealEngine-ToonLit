// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Evolution/SolverBody.h"
#include "Chaos/Particles.h"
#include "Containers/ArrayView.h"

namespace Chaos::Softs
{
template<typename ParticlesType, typename = typename TEnableIf<TIsDerivedFrom<ParticlesType, TParticles<FSolverReal, 3>>::IsDerived>::Type>
class TParticlesRange
{
public:

	TParticlesRange() = default;
	virtual ~TParticlesRange() = default;

	TParticlesRange(ParticlesType* InParticles, int32 InOffset, int32 InRangeSize)
		: Particles(InParticles)
		, Offset(InOffset)
		, RangeSize(InRangeSize)
	{
	}

	static TParticlesRange AddParticleRange(ParticlesType& InParticles, const int32 InRangeSize)
	{
		const int32 Offset = (int32)InParticles.Size();
		InParticles.AddParticles(InRangeSize);
		return TParticlesRange(&InParticles, Offset, InRangeSize);
	}

	bool IsValid() const
	{
		return Particles && Offset >= 0 && ((uint32)(Offset + RangeSize) <= Particles->Size());
	}

	template<typename T>
	TConstArrayView<T> GetConstArrayView(const TArray<T>& Array) const
	{
		check(Offset >= 0 && Offset + RangeSize <= Array.Num());
		return TConstArrayView<T>(Array.GetData() + Offset, RangeSize);
	}

	template<typename T>
	TArrayView<T> GetArrayView(TArray<T>& Array) const
	{
		check(Offset >= 0 && Offset + RangeSize <= Array.Num());
		return TArrayView<T>(Array.GetData() + Offset, RangeSize);
	}

	const ParticlesType& GetParticles() const { check(Particles);  return *Particles; }
	ParticlesType& GetParticles() { check(Particles); return *Particles; }
	int32 GetOffset() const { return Offset; }
	int32 GetRangeSize() const { return RangeSize; }
	int32 Size() const { return RangeSize; } // So this has same interface as Particles

protected:

	ParticlesType* Particles = nullptr;
	int32 Offset = INDEX_NONE;
	int32 RangeSize = 0;
};
}
