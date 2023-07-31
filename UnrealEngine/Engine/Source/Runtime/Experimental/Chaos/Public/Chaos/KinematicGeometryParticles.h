// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/KinematicTargets.h"

namespace Chaos
{

template<class T, int d, EGeometryParticlesSimType SimType>
class TKinematicGeometryParticlesImp : public TGeometryParticlesImp<T, d, SimType>
{
  public:
	TKinematicGeometryParticlesImp()
	    : TGeometryParticlesImp<T, d, SimType>()
	{
		this->MParticleType = EParticleType::Kinematic;
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MW);
		TArrayCollection::AddArray(&KinematicTargets);
	}
	TKinematicGeometryParticlesImp(const TKinematicGeometryParticlesImp<T, d, SimType>& Other) = delete;
	TKinematicGeometryParticlesImp(TKinematicGeometryParticlesImp<T, d, SimType>&& Other)
	    : TGeometryParticlesImp<T, d, SimType>(MoveTemp(Other)), MV(MoveTemp(Other.MV)), MW(MoveTemp(Other.MW)), KinematicTargets(MoveTemp(Other.KinematicTargets))
	{
		this->MParticleType = EParticleType::Kinematic;
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MW);
		TArrayCollection::AddArray(&KinematicTargets);
	}
	virtual ~TKinematicGeometryParticlesImp() {};

	const TVector<T, d>& V(const int32 Index) const { return MV[Index]; }
	TVector<T, d>& V(const int32 Index) { return MV[Index]; }

	const TVector<T, d>& W(const int32 Index) const { return MW[Index]; }
	TVector<T, d>& W(const int32 Index) { return MW[Index]; }

	const TKinematicTarget<T, d>& KinematicTarget(const int32 Index) const { return KinematicTargets[Index]; }
	TKinematicTarget<T, d>& KinematicTarget(const int32 Index) { return KinematicTargets[Index]; }

	FString ToString(int32 index) const
	{
		FString BaseString = TGeometryParticlesImp<T, d, SimType>::ToString(index);
		return FString::Printf(TEXT("%s, MV:%s, MW:%s"), *BaseString, *V(index).ToString(), *W(index).ToString());
	}

	typedef TKinematicGeometryParticleHandle<T, d> THandleType;
	const THandleType* Handle(int32 Index) const;

	//cannot be reference because double pointer would allow for badness, but still useful to have non const access to handle
	THandleType* Handle(int32 Index);

	virtual void Serialize(FChaosArchive& Ar) override
	{
		TGeometryParticlesImp<T, d, SimType>::Serialize(Ar);
		Ar << MV << MW;

		Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
		if(Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) >= FExternalPhysicsCustomObjectVersion::KinematicTargets)
		{
			Ar << KinematicTargets;
		}
	}

	FORCEINLINE TArray<TVector<T, d>>& AllV() { return MV; }
	FORCEINLINE TArray<TVector<T, d>>& AllW() { return MW; }
	FORCEINLINE TArray<TKinematicTarget<T, d>>& AllKinematicTargets() { return KinematicTargets; }

  private:
	TArrayCollectionArray<TVector<T, d>> MV;
	TArrayCollectionArray<TVector<T, d>> MW;
	TArrayCollectionArray<TKinematicTarget<T, d>> KinematicTargets;
};

template <typename T, int d, EGeometryParticlesSimType SimType>
FChaosArchive& operator<<(FChaosArchive& Ar, TKinematicGeometryParticlesImp<T, d, SimType>& Particles)
{
	Particles.Serialize(Ar);
	return Ar;
}



template <typename T, int d>
using TKinematicGeometryParticles = TKinematicGeometryParticlesImp<T, d, EGeometryParticlesSimType::RigidBodySim>;

using FKinematicGeometryParticles = TKinematicGeometryParticles<FReal, 3>;

template <typename T, int d>
using TKinematicGeometryClothParticles = TKinematicGeometryParticlesImp<T, d, EGeometryParticlesSimType::Other>;

using FKinematicGeometryClothParticles UE_DEPRECATED(5.0, "FKinematicGeometryClothParticles is deprecated, use Softs::FSolverRigidParticles instead.") = TKinematicGeometryClothParticles<FReal, 3>;
}

