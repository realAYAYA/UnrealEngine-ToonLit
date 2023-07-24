// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Real.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/DynamicParticles.h"

namespace Chaos
{
template<class T, int d>
class TPBDParticles : public TDynamicParticles<T, d>
{
  public:
	TPBDParticles()
	    : TDynamicParticles<T, d>()
	{
		TArrayCollection::AddArray(&MP);
	}
	TPBDParticles(const TPBDParticles<T, d>& Other) = delete;
	TPBDParticles(TPBDParticles<T, d>&& Other)
	    : TDynamicParticles<T, d>(MoveTemp(Other)), MP(MoveTemp(Other.MP))
	{
		TArrayCollection::AddArray(&MP);
	}
	~TPBDParticles() {}

	const TVector<T, d>& P(const int32 index) const { return MP[index]; }
	TVector<T, d>& P(const int32 index) { return MP[index]; }
	TArrayCollectionArray<TVector<T, d>>& GetP() { return MP; }

  private:
	TArrayCollectionArray<TVector<T, d>> MP;
};

using FPBDParticles = TPBDParticles<FReal, 3>;
}
