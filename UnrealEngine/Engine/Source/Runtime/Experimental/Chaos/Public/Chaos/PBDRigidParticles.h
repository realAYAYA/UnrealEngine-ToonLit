// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/RigidParticles.h"
#include "Chaos/Rotation.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4946)
#endif

namespace Chaos
{
template<class T, int d>
class TPBDRigidsEvolution;

CHAOS_API void EnsureSleepingObjectState(EObjectStateType ObjectState);

template<class T, int d>
class TPBDRigidParticles : public TRigidParticles<T, d>
{
	friend class TPBDRigidsEvolution<T, d>;

  public:
    using TRigidParticles<T, d>::Sleeping;

	CHAOS_API TPBDRigidParticles()
	    : TRigidParticles<T, d>()
	{
		this->MParticleType = EParticleType::Rigid;
		TArrayCollection::AddArray(&MP);
		TArrayCollection::AddArray(&MQ);
		TArrayCollection::AddArray(&MPreV);
		TArrayCollection::AddArray(&MPreW);
		TArrayCollection::AddArray(&MSolverBodyIndex);
	}
	TPBDRigidParticles(const TPBDRigidParticles<T, d>& Other) = delete;
	CHAOS_API TPBDRigidParticles(TPBDRigidParticles<T, d>&& Other)
	    : TRigidParticles<T, d>(MoveTemp(Other))
		, MP(MoveTemp(Other.MP))
		, MQ(MoveTemp(Other.MQ))
		, MPreV(MoveTemp(Other.MPreV))
		, MPreW(MoveTemp(Other.MPreW))
		, MSolverBodyIndex(MoveTemp(Other.MSolverBodyIndex))
	{
		this->MParticleType = EParticleType::Rigid;
		TArrayCollection::AddArray(&MP);
		TArrayCollection::AddArray(&MQ);
		TArrayCollection::AddArray(&MPreV);
		TArrayCollection::AddArray(&MPreW);
		TArrayCollection::AddArray(&MSolverBodyIndex);
	}

	CHAOS_API virtual ~TPBDRigidParticles()
	{}

	FORCEINLINE const TVector<T, d>& P(const int32 index) const { return MP[index]; }
	FORCEINLINE TVector<T, d>& P(const int32 index) { return MP[index]; }
	
	FORCEINLINE const TRotation<T, d>& Q(const int32 index) const { return MQ[index]; }
	FORCEINLINE TRotation<T, d>& Q(const int32 index) { return MQ[index]; }

	CHAOS_API const TVector<T, d>& PreV(const int32 index) const { return MPreV[index]; }
	CHAOS_API TVector<T, d>& PreV(const int32 index) { return MPreV[index]; }

	CHAOS_API const TVector<T, d>& PreW(const int32 index) const { return MPreW[index]; }
	CHAOS_API TVector<T, d>& PreW(const int32 index) { return MPreW[index]; }

	// The index into an FSolverBodyContainer (for dynamic particles only), or INDEX_NONE.
	// \see FSolverBodyContainer
	CHAOS_API int32 SolverBodyIndex(const int32 index) const { return MSolverBodyIndex[index]; }
	CHAOS_API void SetSolverBodyIndex(const int32 index, const int32 InSolverBodyIndex) { MSolverBodyIndex[index] = InSolverBodyIndex; }

    // Must be reinterpret cast instead of static_cast as it's a forward declare
	typedef TPBDRigidParticleHandle<T, d> THandleType;
	const THandleType* Handle(int32 Index) const { return reinterpret_cast<const THandleType*>(TGeometryParticles<T,d>::Handle(Index)); }

	//cannot be reference because double pointer would allow for badness, but still useful to have non const access to handle
	THandleType* Handle(int32 Index) { return reinterpret_cast<THandleType*>(TGeometryParticles<T, d>::Handle(Index)); }

	CHAOS_API void SetSleeping(int32 Index, bool bSleeping)
	{
		if (Sleeping(Index) && bSleeping == false)
		{
			PreV(Index) = this->V(Index);
			PreW(Index) = this->W(Index);
		}
		else if(bSleeping)
		{
			//being put to sleep, so zero out velocities
			this->V(Index) = FVec3(0);
			this->W(Index) = FVec3(0);
		}

		bool CurrentlySleeping = this->ObjectState(Index) == EObjectStateType::Sleeping;
		if (CurrentlySleeping != bSleeping)
		{
			TGeometryParticleHandle<T, d>* Particle = reinterpret_cast<TGeometryParticleHandle<T, d>*>(this->Handle(Index));
			this->AddSleepData(Particle, bSleeping);
		}

		// Dynamic -> Sleeping or Sleeping -> Dynamic
		if (this->ObjectState(Index) == EObjectStateType::Dynamic || this->ObjectState(Index) == EObjectStateType::Sleeping)
		{
			this->ObjectState(Index) = bSleeping ? EObjectStateType::Sleeping : EObjectStateType::Dynamic;
		}

		// Possible for code to set a Dynamic to Kinematic State then to Sleeping State
		if (this->ObjectState(Index) == EObjectStateType::Kinematic && bSleeping)
		{
			this->ObjectState(Index) =  EObjectStateType::Sleeping;
		}

		if (bSleeping)
		{
			EnsureSleepingObjectState(this->ObjectState(Index));
		}
	}

	CHAOS_API void SetObjectState(int32 Index, EObjectStateType InObjectState)
	{
		const EObjectStateType CurrentState = this->ObjectState(Index);

		if (CurrentState == EObjectStateType::Uninitialized)
		{
			// When the state is first initialized, treat it like a static.
			this->InvM(Index) = 0.0f;
			this->InvI(Index) = TVec3<FRealSingle>(0);
		}

		if ((CurrentState == EObjectStateType::Dynamic || CurrentState == EObjectStateType::Sleeping) && (InObjectState == EObjectStateType::Kinematic || InObjectState == EObjectStateType::Static))
		{
			// Transitioning from dynamic to static or kinematic, set inverse mass and inertia tensor to zero.
			this->InvM(Index) = 0.0f;
			this->InvI(Index) = TVec3<FRealSingle>(0);
		}
		else if ((CurrentState == EObjectStateType::Kinematic || CurrentState == EObjectStateType::Static || CurrentState == EObjectStateType::Uninitialized) && (InObjectState == EObjectStateType::Dynamic || InObjectState == EObjectStateType::Sleeping))
		{
			// Transitioning from kinematic or static to dynamic, compute the inverses.
			checkSlow(this->M(Index) != 0.0);
			checkSlow(this->I(Index)[0] != 0.0);
			checkSlow(this->I(Index)[1] != 0.0);
			checkSlow(this->I(Index)[2] != 0.0);
			this->InvM(Index) = 1.f / this->M(Index);
			this->InvI(Index) = TVec3<FRealSingle>(
				1.f / this->I(Index)[0], 
				1.f / this->I(Index)[1],
				1.f / this->I(Index)[2]);

			this->P(Index) = this->X(Index);
			this->Q(Index) = this->R(Index);
		}
		else if (InObjectState == EObjectStateType::Sleeping)
		{
			SetSleeping(Index, true);
			return;
		}
		
		const bool bCurrentSleeping = this->ObjectState(Index) == EObjectStateType::Sleeping;
		const bool bNewSleeping = InObjectState == EObjectStateType::Sleeping;
		if(bCurrentSleeping != bNewSleeping)
		{
			TGeometryParticleHandle<T, d>* Particle = reinterpret_cast<TGeometryParticleHandle<T, d>*>(this->Handle(Index));
 			this->AddSleepData(Particle, bNewSleeping);
		}

		this->ObjectState(Index) = InObjectState;
	}

	CHAOS_API void SetSleepType(int32 Index, ESleepType InSleepType)
	{
		if (InSleepType == ESleepType::NeverSleep && this->ObjectState(Index) == EObjectStateType::Sleeping)
		{
			SetObjectState(Index, EObjectStateType::Dynamic);
		}

		this->SleepType(Index) = InSleepType;
	}

	FString ToString(int32 index) const
	{
		FString BaseString = TRigidParticles<T, d>::ToString(index);
		return FString::Printf(TEXT("%s, MP:%s, MQ:%s, MPreV:%s, MPreW:%s"), *BaseString, *P(index).ToString(), *Q(index).ToString(), *PreV(index).ToString(), *PreW(index).ToString());
	}

	CHAOS_API virtual void Serialize(FChaosArchive& Ar) override
	{
		TRigidParticles<T, d>::Serialize(Ar);
		Ar << MP << MQ << MPreV << MPreW;
	}

	FORCEINLINE TArray<TVector<T, d>>& AllP() { return MP; }
	FORCEINLINE TArray<TRotation<T, d>>& AllQ() { return MQ; }
	FORCEINLINE TArray<TVector<T, d>>& AllPreV() { return MPreV; }
	FORCEINLINE TArray<TVector<T, d>>& AllPreW() { return MPreW; }

  private:
	TArrayCollectionArray<TVector<T, d>> MP;
	TArrayCollectionArray<TRotation<T, d>> MQ;
	TArrayCollectionArray<TVector<T, d>> MPreV;
	TArrayCollectionArray<TVector<T, d>> MPreW;
	TArrayCollectionArray<int32> MSolverBodyIndex;	// Transient for use in constraint solver
};

using FPBDRigidParticles = TPBDRigidParticles<FReal, 3>;

template <typename T, int d>
FChaosArchive& operator<<(FChaosArchive& Ar, TPBDRigidParticles<T, d>& Particles)
{
	Particles.Serialize(Ar);
	return Ar;
}
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
