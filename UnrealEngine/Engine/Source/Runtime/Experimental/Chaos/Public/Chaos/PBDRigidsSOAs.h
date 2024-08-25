// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ParticleHandle.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "UObject/ExternalPhysicsCustomObjectVersion.h"

namespace Chaos
{
namespace CVars
{
	extern CHAOS_API bool bRemoveParticleFromMovingKinematicsOnDisable;
	extern CHAOS_API bool bChaosSolverCheckParticleViews;
}

// A helper function to get the debug name for a particle or "UNKNOWN" when debug names are unavailable
template<typename TParticleType>
const FString& GetParticleDebugName(const TParticleType& Particle)
{
#if CHAOS_DEBUG_NAME
	return *Particle.DebugName();
#else
	static const FString SNoDebugNames = TEXT("<UNKNOWN>");
	return SNoDebugNames;
#endif
}

class IParticleUniqueIndices
{
public:
	virtual ~IParticleUniqueIndices() = default;
	virtual FUniqueIdx GenerateUniqueIdx() = 0;
	virtual void ReleaseIdx(FUniqueIdx Unique) = 0;
};

class FParticleUniqueIndicesMultithreaded: public IParticleUniqueIndices
{
public:
	FParticleUniqueIndicesMultithreaded()
		: Block(0)
	{
		//Note: tune this so that all allocation is done at initialization
		AddPageAndAcquireNextId(/*bAcquireNextId = */ false);
	}

	FUniqueIdx GenerateUniqueIdx() override
	{
		while(true)
		{
			if(FUniqueIdx* Idx = FreeIndices.Pop())
			{
				return *Idx;
			}

			//nothing available so try to add some

			if(FPlatformAtomics::InterlockedCompareExchange(&Block,1,0) == 0)
			{
				//we got here first so add a new page
				FUniqueIdx RetIdx = FUniqueIdx(AddPageAndAcquireNextId(/*bAcquireNextId =*/ true));

				//release blocker. Note: I don't think this can ever fail, but no real harm in the while loop
				while(FPlatformAtomics::InterlockedCompareExchange(&Block,0,1) != 1)
				{
				}

				return RetIdx;
			}
		}
	}

	void ReleaseIdx(FUniqueIdx Unique) override
	{
		ensure(Unique.IsValid());
		int32 PageIdx = Unique.Idx / IndicesPerPage;
		int32 Entry = Unique.Idx % IndicesPerPage;
		FUniqueIdxPage& Page = *Pages[PageIdx];
		FreeIndices.Push(&Page.Indices[Entry]);
	}

	~FParticleUniqueIndicesMultithreaded()
	{
		//Make sure queue is empty, memory management of actual pages is handled automatically by TUniquePtr
		while(FreeIndices.Pop())
		{
		}
	}

private:

	int32 AddPageAndAcquireNextId(bool bAcquireNextIdx)
	{
		//Note: this should never really be called post initialization
		TUniquePtr<FUniqueIdxPage> Page = MakeUnique<FUniqueIdxPage>();
		const int32 PageIdx = Pages.Num();
		int32 FirstIdxInPage = PageIdx * IndicesPerPage;
		Page->Indices[0] = FUniqueIdx(FirstIdxInPage);

		//If we acquire next id we avoid pushing it into the queue
		for(int32 Count = bAcquireNextIdx ? 1 : 0; Count < IndicesPerPage; Count++)
		{
			Page->Indices[Count] = FUniqueIdx(FirstIdxInPage + Count);
			FreeIndices.Push(&Page->Indices[Count]);
		}

		Pages.Emplace(MoveTemp(Page));
		return bAcquireNextIdx ? FirstIdxInPage : INDEX_NONE;
	}

	static constexpr int32 IndicesPerPage = 1024;
	struct FUniqueIdxPage
	{
		FUniqueIdx Indices[IndicesPerPage];
	};
	TArray<TUniquePtr<FUniqueIdxPage>> Pages;
	TLockFreePointerListFIFO<FUniqueIdx,0> FreeIndices;
	volatile int8 Block;
};

using FParticleUniqueIndices = FParticleUniqueIndicesMultithreaded;

/**
* A list of particles held in a array.
* Also contains a particle-index map for faster find/removal with large lists.
* @see TParticleArray
*/
template <typename TParticleType>
class TParticleMapArray
{
public:
	TParticleMapArray()
		: ContainerListMask(EGeometryParticleListMask::None)
	{
	}

	void Reset()
	{
		ParticleToIndex.Reset();
		ParticleArray.Reset();
	}
	
	bool Contains(const TParticleType* Particle) const
	{
		return ParticleToIndex.Contains(Particle);
	}

	template <typename TParticle1>
	void Insert(const TArray<TParticle1*>& ParticlesToInsert)
	{
		TArray<bool> Contains;
		Contains.AddZeroed(ParticlesToInsert.Num());

		// TODO: Compile time check ensuring TParticle2 is derived from TParticle1?
		int32 NextIdx = ParticleArray.Num();
		for (int32 Idx = 0; Idx < ParticlesToInsert.Num(); ++Idx)
		{
			auto Particle = ParticlesToInsert[Idx];
			Contains[Idx] = ParticleToIndex.Contains(Particle);
			if (!Contains[Idx])
			{
				ParticleToIndex.Add(Particle, NextIdx++);
			}
		}
		ParticleArray.Reserve(ParticleArray.Num() + NextIdx - ParticleArray.Num());
		for (int32 Idx = 0; Idx < ParticlesToInsert.Num(); ++Idx)
		{
			if (!Contains[Idx])
			{
				auto Particle = ParticlesToInsert[Idx];
				ParticleArray.Add(Particle);
			}
		}
	}

	void Insert(TParticleType* Particle)
	{
		if (ParticleToIndex.Contains(Particle) == false)
		{
			ParticleToIndex.Add(Particle, ParticleArray.Num());
			ParticleArray.Add(Particle);
		}
	}

	void Remove(TParticleType* Particle)
	{
		if (int32* IdxPtr = ParticleToIndex.Find(Particle))
		{
			int32 Idx = *IdxPtr;
			ParticleArray.RemoveAtSwap(Idx);
			if (Idx < ParticleArray.Num())
			{
				//update swapped element with new index
				ParticleToIndex[ParticleArray[Idx]] = Idx;
			}
			ParticleToIndex.Remove(Particle);
		}
	}

	void Serialize(FChaosArchive& Ar)
	{
		TArray<TSerializablePtr<TParticleType>>& SerializableArray = AsAlwaysSerializableArray(ParticleArray);
		Ar << SerializableArray;

		int32 Idx = 0;
		for (auto Particle : ParticleArray)
		{
			ParticleToIndex.Add(Particle, Idx++);
		}
	}

	const TArray<TParticleType*>& GetArray() const { return ParticleArray;	}
	TArray<TParticleType*>& GetArray() { return ParticleArray; }

	void SetContainerListMask(const EGeometryParticleListMask InMask) { ContainerListMask = InMask; }
	EGeometryParticleListMask GetContainerListMask() const { return ContainerListMask; }

private:
	TMap<TParticleType*, int32> ParticleToIndex;
	TArray<TParticleType*> ParticleArray;
	EGeometryParticleListMask ContainerListMask;
};

/**
* A list of particles held in a array.
* O(N) find/remove so only for use on small lists. 
* @see TParticleArrayMap
*/
template <typename TParticleType>
class TParticleArray
{
public:
	TParticleArray()
		: ContainerListMask(EGeometryParticleListMask::None)
	{
	}

	void Reset()
	{
		ParticleArray.Reset();
	}

	bool Contains(const TParticleType* Particle) const
	{
		return ParticleArray.Contains(Particle);
	}

	void Insert(TParticleType* Particle)
	{
		ParticleArray.Add(Particle);
	}

	void Remove(TParticleType* Particle)
	{
		ParticleArray.Remove(Particle);
	}

	const TArray<TParticleType*>& GetArray() const { return ParticleArray; }
	TArray<TParticleType*>& GetArray() { return ParticleArray; }

	void SetContainerListMask(const EGeometryParticleListMask InMask) { ContainerListMask = InMask; }
	EGeometryParticleListMask GetContainerListMask() const { return ContainerListMask; }

private:
	TArray<TParticleType*> ParticleArray;
	EGeometryParticleListMask ContainerListMask;
};

class FPBDRigidsSOAs
{
public:
	FPBDRigidsSOAs(IParticleUniqueIndices& InUniqueIndices)
		: UniqueIndices(InUniqueIndices)
	{
#if CHAOS_DETERMINISTIC
		BiggestParticleID = 0;
#endif

		StaticParticles = MakeUnique<FGeometryParticles>();
		StaticDisabledParticles = MakeUnique<FGeometryParticles>();

		KinematicParticles = MakeUnique<FKinematicGeometryParticles>();
		KinematicDisabledParticles = MakeUnique<FKinematicGeometryParticles>();

		DynamicDisabledParticles = MakeUnique<FPBDRigidParticles>();
		DynamicParticles = MakeUnique<FPBDRigidParticles>();
		DynamicKinematicParticles = MakeUnique<FPBDRigidParticles>();

		ClusteredParticles = MakeUnique<FPBDRigidClusteredParticles>();

		GeometryCollectionParticles = MakeUnique<TPBDGeometryCollectionParticles<FReal, 3>>();

		SetContainerListMasks();

		UpdateViews();
	}

	FPBDRigidsSOAs(const FPBDRigidsSOAs&) = delete;
	FPBDRigidsSOAs(FPBDRigidsSOAs&& Other) = delete;

	~FPBDRigidsSOAs()
	{
		// Abandonning the particles, don't worry about ordering anymore.
		ClusteredParticles->RemoveParticleBehavior() = ERemoveParticleBehavior::RemoveAtSwap;
		GeometryCollectionParticles->RemoveParticleBehavior() = ERemoveParticleBehavior::RemoveAtSwap;
	}

	void Reset()
	{
		check(0);
	}

	void ShrinkArrays(const float MaxSlackFraction, const int32 MinSlack)
	{
		StaticParticles->ShrinkArrays(MaxSlackFraction, MinSlack);
		StaticDisabledParticles->ShrinkArrays(MaxSlackFraction, MinSlack);

		KinematicParticles->ShrinkArrays(MaxSlackFraction, MinSlack);
		KinematicDisabledParticles->ShrinkArrays(MaxSlackFraction, MinSlack);

		DynamicDisabledParticles->ShrinkArrays(MaxSlackFraction, MinSlack);
		DynamicParticles->ShrinkArrays(MaxSlackFraction, MinSlack);
		DynamicKinematicParticles->ShrinkArrays(MaxSlackFraction, MinSlack);

		ClusteredParticles->ShrinkArrays(MaxSlackFraction, MinSlack);

		GeometryCollectionParticles->ShrinkArrays(MaxSlackFraction, MinSlack);
	}

	void UpdateDirtyViews()
	{
		// @todo(chaos): this should only refresh views that may have changed
		UpdateViews();
	}
	
	TArray<FGeometryParticleHandle*> CreateStaticParticles(int32 NumParticles, const FUniqueIdx* ExistingIndices = nullptr, const FGeometryParticleParameters& Params = FGeometryParticleParameters())
	{
		LLM_SCOPE_BYNAME(TEXT("Physics/StaticParticles"));

		auto Results = CreateParticlesHelper<FGeometryParticleHandle>(NumParticles, ExistingIndices, Params.bDisabled ? StaticDisabledParticles : StaticParticles, Params);
		UpdateViews();
		return Results;
	}
	TArray<FKinematicGeometryParticleHandle*> CreateKinematicParticles(int32 NumParticles, const FUniqueIdx* ExistingIndices = nullptr,  const FKinematicGeometryParticleParameters& Params = FKinematicGeometryParticleParameters())
	{
		LLM_SCOPE_BYNAME(TEXT("Physics/KinematicParticles"));

		auto Results = CreateParticlesHelper<FKinematicGeometryParticleHandle>(NumParticles, ExistingIndices, Params.bDisabled ? KinematicDisabledParticles : KinematicParticles, Params);
		UpdateViews();
		return Results;
	}
	TArray<FPBDRigidParticleHandle*> CreateDynamicParticles(int32 NumParticles, const FUniqueIdx* ExistingIndices = nullptr,  const FPBDRigidParticleParameters& Params = FPBDRigidParticleParameters())
	{
		LLM_SCOPE_BYNAME(TEXT("Physics/DynamicParticles"));

		auto Results = CreateParticlesHelper<FPBDRigidParticleHandle>(NumParticles, ExistingIndices, Params.bDisabled ? DynamicDisabledParticles : DynamicParticles, Params);;

		if (!Params.bStartSleeping)
		{
			AddToActiveArray(Results);
		}
		UpdateViews();
		return Results;
	}
	TArray<TPBDGeometryCollectionParticleHandle<FReal, 3>*> CreateGeometryCollectionParticles(int32 NumParticles, const FUniqueIdx* ExistingIndices = nullptr, const FPBDRigidParticleParameters& Params = FPBDRigidParticleParameters())
	{
		LLM_SCOPE_BYNAME(TEXT("Physics/GeometryCollectionParticles"));

		TArray<TPBDGeometryCollectionParticleHandle<FReal, 3>*> Results = CreateParticlesHelper<TPBDGeometryCollectionParticleHandle<FReal, 3>>(
			NumParticles, ExistingIndices, GeometryCollectionParticles, Params);
		for (auto* Handle : Results)
		{
			if (Params.bStartSleeping)
			{
				Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Sleeping);
				Handle->SetSleeping(true);
			}
			else
			{
				Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Dynamic);
				Handle->SetSleeping(false);
			}
			if (!Params.bDisabled)
			{
				InsertGeometryCollectionParticle(Handle);
			}
		}
		UpdateViews();
		
		return Results;
	}

	/** Used specifically by PBDRigidClustering. These have special properties for maintaining relative order, efficiently switching from kinematic to dynamic, disable to enable, etc... */
	TArray<FPBDRigidClusteredParticleHandle*> CreateClusteredParticles(int32 NumParticles, const FUniqueIdx* ExistingIndices = nullptr,  const FPBDRigidParticleParameters& Params = FPBDRigidParticleParameters())
	{
		LLM_SCOPE_BYNAME(TEXT("Physics/ClusteredParticles"));

		auto NewClustered = CreateParticlesHelper<FPBDRigidClusteredParticleHandle>(NumParticles, ExistingIndices, ClusteredParticles, Params);
		
		if (!Params.bDisabled)
		{
			InsertClusteredParticles(NewClustered);
		}

		if (!Params.bStartSleeping)
		{
			AddToActiveArray(reinterpret_cast<TArray<FPBDRigidParticleHandle*>&>(NewClustered));
		}

		UpdateViews();
		
		return NewClustered;
	}
	
	void ClearTransientDirty()
	{
		TransientDirtyMapArray.Reset();
	}

	// @todo(chaos): keep track of which views may be dirty and lazily update them
	// rather than calling UpdateViews all the time
	void MarkTransientDirtyParticle(FGeometryParticleHandle* Particle, const bool bUpdateViews = true)
	{
		if(FPBDRigidParticleHandle* Rigid =  Particle->CastToRigidParticle())
		{
			//if active it's already in the dirty view
			if (!ActiveParticlesMapArray.Contains(Rigid) && !MovingKinematicsMapArray.Contains(Rigid))
			{
				AddToList(Rigid, TransientDirtyMapArray);
		
				CheckParticleViewMask(Particle);

				if (bUpdateViews)
				{
					UpdateViews();
				}
			}
		}
	}

	void DestroyParticle(FGeometryParticleHandle* Particle)
	{
		if (bResimulating)
		{
			RemoveFromList(Particle, ResimStaticParticles);
			FKinematicGeometryParticleHandle* Kinematic = Particle->CastToKinematicParticle();
			if (Kinematic)
			{
				RemoveFromList(Kinematic, ResimKinematicParticles);
			}
		}

		CVD_TRACE_PARTICLE_DESTROYED(Particle);

		auto PBDRigid = Particle->CastToRigidParticle();
		if(PBDRigid)
		{
			if (bResimulating)
			{
				RemoveFromList(PBDRigid, ResimDynamicParticles);
				RemoveFromList(PBDRigid, ResimDynamicKinematicParticles);
			}

			RemoveFromActiveArray(PBDRigid, /*bStillDirty=*/ false);
			RemoveFromList(PBDRigid, MovingKinematicsMapArray);

			if (TPBDGeometryCollectionParticleHandle<FReal, 3>* GCHandle = Particle->CastToGeometryCollection())
			{
				RemoveGeometryCollectionParticle(GCHandle);
			}
			else if (FPBDRigidClusteredParticleHandle* PBDRigidClustered = Particle->CastToClustered())
			{
				RemoveClusteredParticle(PBDRigidClustered);
			}

			// Check for sleep events referencing this particle
			// TODO think about this case more
			GetDynamicParticles().GetSleepDataLock().WriteLock();
			TArray<TSleepData<FReal, 3>>& SleepData = GetDynamicParticles().GetSleepData();

			SleepData.RemoveAllSwap([Particle](TSleepData<FReal, 3>& Entry) 
			{
				return Entry.Particle == Particle;
			});

			GetDynamicParticles().GetSleepDataLock().WriteUnlock();
		}

		ParticleHandles.DestroyHandleSwap(Particle);

		UpdateViews();
	}

	/**
	 * A disabled particle is ignored by the solver.
	 */
	void DisableParticle(FGeometryParticleHandle* Particle)
	{
		// Rigid particles express their disabled state with a boolean.
		// Disabled kinematic and static particles get shuffled to different SOAs.

		if (auto PBDRigid = Particle->CastToRigidParticle())
		{
			PBDRigid->SetDisabled(true);
			PBDRigid->SetVf(FVec3f(0));
			PBDRigid->SetWf(FVec3f(0));

			if (TPBDGeometryCollectionParticleHandle<FReal, 3>* PBDRigidGC = Particle->CastToGeometryCollection())
			{
				RemoveGeometryCollectionParticle(PBDRigidGC);
			}
			else if (FPBDRigidClusteredParticleHandle* PBDRigidClustered = Particle->CastToClustered())
			{
				RemoveClusteredParticle(PBDRigidClustered);
			}
			else
			{
				SetDynamicParticleSOA(PBDRigid);
			}

			// All active particles RIGID particles
			{
				RemoveFromActiveArray(PBDRigid, /*bStillDirty=*/false);
				if (CVars::bRemoveParticleFromMovingKinematicsOnDisable)
				{
					RemoveFromList(PBDRigid, MovingKinematicsMapArray);
				}
			}
		}
		else if (Particle->CastToKinematicParticle())
		{
			Particle->MoveToSOA(*KinematicDisabledParticles);
		}
		else
		{
			Particle->MoveToSOA(*StaticDisabledParticles);
		}

		CheckParticleViewMask(Particle);

		UpdateViews();
	}

	void EnableParticle(FGeometryParticleHandle* Particle)
	{
		// Rigid particles express their disabled state with a boolean.
		// Disabled kinematic and static particles get shuffled to differnt SOAs.

		if (auto PBDRigid = Particle->CastToRigidParticle())
		{
			PBDRigid->SetDisabled(false);
			// DisableParticle() zeros V and W.  We do nothing here and assume the client
			// sets appropriate values.

			if (TPBDGeometryCollectionParticleHandle<FReal, 3>* PBDRigidGC = Particle->CastToGeometryCollection())
			{
				RemoveGeometryCollectionParticle(PBDRigidGC);
				InsertGeometryCollectionParticle(PBDRigidGC);
			}
			else if (FPBDRigidClusteredParticleHandle* PBDRigidClustered = Particle->CastToClustered())
			{
				InsertClusteredParticle(PBDRigidClustered);
			}
			else
			{
				SetDynamicParticleSOA(PBDRigid);
			}

			if (!PBDRigid->Sleeping() && Particle->ObjectState() == EObjectStateType::Dynamic)
			{
				AddToActiveArray(PBDRigid);
			}
		}
		else if (Particle->CastToKinematicParticle())
		{
			Particle->MoveToSOA(*KinematicParticles);
		}
		else
		{
			Particle->MoveToSOA(*StaticParticles);
		}

		CheckParticleViewMask(Particle);

		UpdateViews();
	}

	/**
	 * Wake a sleeping dynamic non-disabled particle.
	 */
	void ActivateParticle(FGeometryParticleHandle* Particle, const bool DeferUpdateViews=false)
	{
		if (auto PBDRigid = Particle->CastToRigidParticle())
		{
			if (PBDRigid->ObjectState() == EObjectStateType::Sleeping ||
				PBDRigid->ObjectState() == EObjectStateType::Dynamic)
			{
				if (ensure(!PBDRigid->Disabled()))
				{
					// Sleeping state is currently expressed in 2 places...
					PBDRigid->SetSleeping(false);
					PBDRigid->SetObjectStateLowLevel(EObjectStateType::Dynamic);
		
					if (TPBDGeometryCollectionParticleHandle<FReal, 3>* PBDRigidGC = Particle->CastToGeometryCollection())
					{
						RemoveGeometryCollectionParticle(PBDRigidGC);
						InsertGeometryCollectionParticle(PBDRigidGC);
					}
					check(PBDRigid->ObjectState() == EObjectStateType::Dynamic);
					AddToActiveArray(PBDRigid);

					CheckParticleViewMask(Particle);

					if(!DeferUpdateViews)
					{
						UpdateViews();
					}
				}
			}
		}
	}

	/**
	 * Wake multiple dynamic non-disabled particles.
	 */
	void ActivateParticles(const TArray<FGeometryParticleHandle*>& Particles)
	{
		for (auto Particle : Particles)
		{
			ActivateParticle(Particle, true);
		}
		
		UpdateViews();
	}

	/**
	 * Put a non-disabled dynamic particle to sleep.
	 *
	 * If \p DeferUpdateViews is \c true, then it's assumed this function
	 * is being called in a loop and it won't update the SOA view arrays.
	 */
	void DeactivateParticle(
		FGeometryParticleHandle* Particle,
		const bool DeferUpdateViews=false)
	{
		if(auto PBDRigid = Particle->CastToRigidParticle())
		{
			if (PBDRigid->ObjectState() == EObjectStateType::Dynamic ||
				PBDRigid->ObjectState() == EObjectStateType::Sleeping)
			{
				if (ensure(!PBDRigid->Disabled()))
				{
					// Sleeping state is currently expressed in 2 places...
					PBDRigid->SetSleeping(true);
					PBDRigid->SetObjectStateLowLevel(EObjectStateType::Sleeping);

					if (TPBDGeometryCollectionParticleHandle<FReal, 3>* PBDRigidGC = Particle->CastToGeometryCollection())
					{
						RemoveGeometryCollectionParticle(PBDRigidGC);
						InsertGeometryCollectionParticle(PBDRigidGC);
					}
					RemoveFromActiveArray(PBDRigid, /*bStillDirty=*/true);

					CheckParticleViewMask(Particle);

					if (!DeferUpdateViews)
					{
						UpdateViews();
					}
				}
			}
		}
	}

	/**
	 * Put multiple dynamic non-disabled particles to sleep.
	 */
	void DeactivateParticles(const TArray<FGeometryParticleHandle*>& Particles)
	{
		for (auto Particle : Particles)
		{
			DeactivateParticle(Particle, true);
		}
		UpdateViews();
	}
	
	/**
	* Rebuild views if necessary.
	*/
	void RebuildViews()
	{
		UpdateViews();
	}

	void SetDynamicParticleSOA(FPBDRigidParticleHandle* Particle)
	{
		const EObjectStateType State = Particle->ObjectState();

		if (Particle->Disabled())
		{
			Particle->MoveToSOA(*DynamicDisabledParticles);
			RemoveFromActiveArray(Particle->CastToRigidParticle(), /*bStillDirty=*/ false);
			if (CVars::bRemoveParticleFromMovingKinematicsOnDisable)
			{
				RemoveFromList(Particle, MovingKinematicsMapArray);
			}
		}
		else
		{
			if (Particle->ObjectState() != EObjectStateType::Dynamic)
			{
				RemoveFromActiveArray(Particle->CastToRigidParticle(), /*bStillDirty=*/true);
			}
			else
			{
				AddToActiveArray(Particle->CastToRigidParticle());
			}

			if (Particle->ObjectState() != EObjectStateType::Kinematic)
			{
				RemoveFromList(Particle, MovingKinematicsMapArray);
			}

			if (Particle->Type == EParticleType::Clustered)
			{
				Particle->MoveToSOA(*ClusteredParticles);
				if (bResimulating)
				{
					RemoveFromList(Particle, ResimDynamicKinematicParticles);
					AddToList(Particle, ResimDynamicParticles);
				}
			}
			else if (Particle->Type == EParticleType::GeometryCollection)
			{
				Particle->MoveToSOA(*GeometryCollectionParticles);
				if (bResimulating)
				{
					RemoveFromList(Particle, ResimDynamicKinematicParticles);
					AddToList(Particle, ResimDynamicParticles);
				}
			}
			else
			{
				// Move to appropriate dynamic SOA
				switch (State)
				{
				case EObjectStateType::Kinematic:
					Particle->MoveToSOA(*DynamicKinematicParticles);
					if (bResimulating)
					{
						RemoveFromList(Particle, ResimDynamicParticles);
						AddToList(Particle, ResimDynamicKinematicParticles);
					}
					break;

				case EObjectStateType::Dynamic:
					Particle->MoveToSOA(*DynamicParticles);
					if (bResimulating)
					{
						RemoveFromList(Particle, ResimDynamicKinematicParticles);
						AddToList(Particle, ResimDynamicParticles);
					}
					break;

				default:
					// TODO: Special SOAs for sleeping and static particles?
					Particle->MoveToSOA(*DynamicParticles);
					if (bResimulating)
					{
						RemoveFromList(Particle, ResimDynamicKinematicParticles);
						AddToList(Particle, ResimDynamicParticles);
					}
					break;
				}
			}
		}

		CheckParticleViewMask(Particle);

		UpdateViews();
	}

	void SetClusteredParticleSOA(FPBDRigidClusteredParticleHandle* ClusteredParticle)
	{
		if (ClusteredParticle->ObjectState() != EObjectStateType::Kinematic)
		{
			RemoveFromList(ClusteredParticle, MovingKinematicsMapArray);
		}

		if (TPBDGeometryCollectionParticleHandle<FReal, 3>* GCParticle = ClusteredParticle->CastToGeometryCollection())
		{
			// Geometry collection particles have their own arrays which are also included in the active view
			RemoveGeometryCollectionParticle(GCParticle);
			InsertGeometryCollectionParticle(GCParticle);
		}
		else 
		{ 
			check(ClusteredParticle->GetParticleType() != Chaos::EParticleType::GeometryCollection);
			RemoveClusteredParticle(ClusteredParticle);
			InsertClusteredParticle(ClusteredParticle);

			// Cluster particle must go in the active array because the Cluster arrays are not in the active view
			if (ClusteredParticle->ObjectState() != EObjectStateType::Dynamic || ClusteredParticle->Disabled())
			{
				RemoveFromActiveArray(ClusteredParticle->CastToRigidParticle(), /*bStillDirty=*/true);
			}
			else
			{
				AddToActiveArray(ClusteredParticle->CastToRigidParticle());
			}
		}

		CheckParticleViewMask(ClusteredParticle);
		
		UpdateViews();
	}

	void MarkMovingKinematic(FKinematicGeometryParticleHandle* Particle)
	{
		if (FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle())
		{
			AddToList(Rigid, MovingKinematicsMapArray);
			// Remove from TransientDirtyMapArray to be sure not have have particle in both lists
			RemoveFromList(Rigid, TransientDirtyMapArray);

			CheckParticleViewMask(Particle);
		}
	}

	void UpdateAllMovingKinematic()
	{
		// moving kinematics are going to a 'reset' mode then a 'none' mode
		// 'reset' ones need to stay for another frame 
		// 'none' can be safely removed from the map/array 
		int32 Index = 0;
		while (Index < MovingKinematicsMapArray.GetArray().Num())
		{
			if (MovingKinematicsMapArray.GetArray()[Index]->KinematicTarget().GetMode() == EKinematicTargetMode::None)
			{
				// remove and do not increment Index
				FPBDRigidParticleHandle* Rigid = MovingKinematicsMapArray.GetArray()[Index];
				RemoveFromList(Rigid, MovingKinematicsMapArray);

				// Particle may not be moving, but its velocity was just changed (to zero), so it
				// needs to be in a dirty list. Since it are no longer in MovingKinematicsMapArray
				// we must put it the transient dirty list
				MarkTransientDirtyParticle(Rigid);

				CheckParticleViewMask(Rigid);
			}
			else
			{
				++Index;
			}
		}
	}

	void Serialize(FChaosArchive& Ar)
	{
		static const FName SOAsName = TEXT("PBDRigidsSOAs");
		FChaosArchiveScopedMemory ScopedMemory(Ar, SOAsName, false);

		ParticleHandles.Serialize(Ar);

		Ar << StaticParticles;
		Ar << StaticDisabledParticles;
		Ar << KinematicParticles;
		Ar << KinematicDisabledParticles;
		Ar << DynamicParticles;
		Ar << DynamicDisabledParticles;
		// TODO: Add an entry in UObject/ExternalPhysicsCustomObjectVersion.h when adding these back in:
		//Ar << ClusteredParticles;
		//Ar << GeometryCollectionParticles;

		Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
		if (Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) >= FExternalPhysicsCustomObjectVersion::AddDynamicKinematicSOA)
		{
			Ar << DynamicKinematicParticles;
		}

		{
			//need to assign indices to everything
			auto AssignIdxHelper = [&](const auto& Particles)
			{
				for(uint32 ParticleIdx = 0; ParticleIdx < Particles->Size(); ++ParticleIdx)
				{
					FUniqueIdx Unique = UniqueIndices.GenerateUniqueIdx();
					Particles->UniqueIdx(ParticleIdx) = Unique;
					Particles->GTGeometryParticle(ParticleIdx)->SetUniqueIdx(Unique);
				}
			};

			AssignIdxHelper(StaticParticles);
			AssignIdxHelper(StaticDisabledParticles);
			AssignIdxHelper(KinematicParticles);
			AssignIdxHelper(DynamicParticles);
			AssignIdxHelper(DynamicParticles);
			AssignIdxHelper(DynamicDisabledParticles);
			//AssignIdxHelper(ClusteredParticles);
			//AssignIdxHelper(GeometryCollectionParticles);
		}

		ensure(ClusteredParticles->Size() == 0);	//not supported yet
		//Ar << ClusteredParticles;
		Ar << GeometryCollectionParticles;

		ActiveParticlesMapArray.Serialize(Ar);
		//DynamicClusteredMapArray.Serialize(Ar);

		//todo: update deterministic ID

		// We do not serialize the list masks to simplify maintenance
		if (Ar.IsLoading())
		{
			SetContainerListMasks();
		}

		//if (!GeometryCollectionParticles || !GeometryCollectionParticles->Size())
			UpdateViews();
		//else
		//	UpdateGeometryCollectionViews();
	}


	const TParticleView<FGeometryParticles>& GetNonDisabledView() const {  return NonDisabledView; }

	const TParticleView<FPBDRigidParticles>& GetNonDisabledDynamicView() const { return NonDisabledDynamicView; }

	const TParticleView<FPBDRigidClusteredParticles>& GetNonDisabledClusteredView() const { return NonDisabledClusteredView; }

	const TParticleView<FPBDRigidParticles>& GetActiveParticlesView() const {  return ActiveParticlesView; }
	TParticleView<FPBDRigidParticles>& GetActiveParticlesView() { return ActiveParticlesView; }

	const TArray<FPBDRigidParticleHandle*>& GetActiveParticlesArray() const { return ActiveParticlesMapArray.GetArray(); }
	
	const TParticleView<FPBDRigidParticles>& GetDirtyParticlesView() const { return DirtyParticlesView; }
	TParticleView<FPBDRigidParticles>& GetDirtyParticlesView() { return DirtyParticlesView; }

	const TParticleView<FGeometryParticles>& GetAllParticlesView() const { return AllParticlesView; }


	const TParticleView<FKinematicGeometryParticles>& GetActiveKinematicParticlesView() const { return ActiveKinematicParticlesView; }
	TParticleView<FKinematicGeometryParticles>& GetActiveKinematicParticlesView() { return ActiveKinematicParticlesView; }

	const TParticleView<FPBDRigidParticles>& GetActiveMovingKinematicParticlesView() const { return ActiveMovingKinematicParticlesView; }
	TParticleView<FPBDRigidParticles>& GetActiveMovingKinematicParticlesView() { return ActiveMovingKinematicParticlesView; }

	const TParticleView<FGeometryParticles>& GetActiveStaticParticlesView() const { return ActiveStaticParticlesView; }
	TParticleView<FGeometryParticles>& GetActiveStaticParticlesView() { return ActiveStaticParticlesView; }

	const TParticleView<FPBDRigidParticles>& GetActiveDynamicMovingKinematicParticlesView() const { return ActiveDynamicMovingKinematicParticlesView; }
	TParticleView<FPBDRigidParticles>& GetActiveDynamicMovingKinematicParticlesView() { return ActiveDynamicMovingKinematicParticlesView; }

	const TGeometryParticleHandles<FReal, 3>& GetParticleHandles() const { return ParticleHandles; }
	TGeometryParticleHandles<FReal, 3>& GetParticleHandles() { return ParticleHandles; }

	const FPBDRigidParticles& GetDynamicParticles() const { return *DynamicParticles; }
	FPBDRigidParticles& GetDynamicParticles() { return *DynamicParticles; }

	const FPBDRigidParticles& GetDynamicKinematicParticles() const { return *DynamicKinematicParticles; }
	FPBDRigidParticles& GetDynamicKinematicParticles() { return *DynamicKinematicParticles; }

	// Disabled Dynamic and DynamicKinematic Particles
	const FPBDRigidParticles& GetDynamicDisabledParticles() const { return *DynamicDisabledParticles; }
	FPBDRigidParticles& GetDynamicDisabledParticles() { return *DynamicDisabledParticles; }

	const FGeometryParticles& GetNonDisabledStaticParticles() const { return *StaticParticles; }
	FGeometryParticles& GetNonDisabledStaticParticles() { return *StaticParticles; }

	const TPBDGeometryCollectionParticles<FReal, 3>& GetGeometryCollectionParticles() const { return *GeometryCollectionParticles; }
	TPBDGeometryCollectionParticles<FReal, 3>& GetGeometryCollectionParticles() { return *GeometryCollectionParticles; }

	const TArray<FPBDGeometryCollectionParticleHandle*>& GetSleepingGeometryCollectionArray() const { return SleepingGeometryCollectionArray.GetArray(); }
	const TArray<FPBDGeometryCollectionParticleHandle*>& GetDynamicGeometryCollectionArray() const { return DynamicGeometryCollectionArray.GetArray(); }

	void InsertGeometryCollectionParticle(TPBDGeometryCollectionParticleHandle<FReal, 3>* GCParticle)
	{
		if (!GCParticle->Disabled())
		{
			const Chaos::EObjectStateType State = GCParticle->Sleeping() ? Chaos::EObjectStateType::Sleeping : GCParticle->ObjectState();
			switch (State)
			{
			case EObjectStateType::Uninitialized:
				ensure(false); // we should probably not be here 
				break;
			case EObjectStateType::Static:
				AddToList(GCParticle, StaticGeometryCollectionArray);
				break;
			case EObjectStateType::Kinematic:
				AddToList(GCParticle, KinematicGeometryCollectionArray);
				break;
			case EObjectStateType::Dynamic:
				AddToList(GCParticle, DynamicGeometryCollectionArray);
				break;
			case EObjectStateType::Sleeping:
				AddToList(GCParticle, SleepingGeometryCollectionArray);
				break;
			}
		}
	}

	void RemoveGeometryCollectionParticle(TPBDGeometryCollectionParticleHandle<FReal, 3>* GCParticle)
	{
		RemoveFromList(GCParticle, StaticGeometryCollectionArray);
		RemoveFromList(GCParticle, KinematicGeometryCollectionArray);
		RemoveFromList(GCParticle, SleepingGeometryCollectionArray);
		RemoveFromList(GCParticle, DynamicGeometryCollectionArray);
	}

	const auto& GetClusteredParticles() const { return *ClusteredParticles; }
	auto& GetClusteredParticles() { return *ClusteredParticles; }

	auto& GetUniqueIndices() { return UniqueIndices; }

	// Check that the particles in each of the particle lists have their ListMask sett appropriately
	CHAOS_API void CheckListMasks();
	
	// Check that no particles are in multiple lists that contribute to a single view
	CHAOS_API void CheckViewMasks();

private:

	//
	// List Tracking System: 
	// every particle stores the set of SOAs/ParticleMapArrays/ParticleArrays 
	// that the particle is in. This information is used to check for invalid 
	// combinations (some list memberships are mutually exclusive) and to 
	// reduce the cost of checking whether a particle is in a particular list.
	//

	// Initialize the list mask for all particle containers and lists
	CHAOS_API void SetContainerListMasks();

	// Add a particle to the specified SOA/List and update its ListMask
	template<typename TListType, typename TParticleHandleType>
	void AddToList(TParticleHandleType* Particle, TListType& List)
	{
		//if (!Particle->IsInAnyList(List.GetContainerListMask()))
		{
			Particle->AddToLists(List.GetContainerListMask());
			List.Insert(Particle);
		}
	}

	// Add a set of particles to the specified SOA/List and update their ListMasks
	template<typename TListType, typename TParticleHandleType>
	void AddToList(const TArray<TParticleHandleType*> Particles, TListType& List)
	{
		check(List.GetContainerListMask() != EGeometryParticleListMask::None);

		for (TParticleHandleType* Particle : Particles)
		{
			Particle->AddToLists(List.GetContainerListMask());
		}
		List.Insert(Particles);
	}

	// Remove a particle from the specified SOA/List and update its ListMask
	template<typename TListType, typename TParticleHandleType>
	void RemoveFromList(TParticleHandleType* Particle, TListType& List)
	{
		check(List.GetContainerListMask() != EGeometryParticleListMask::None);

		//if (Particle->IsInAnyList(List.GetContainerListMask()))
		{
			Particle->RemoveFromLists(List.GetContainerListMask());
			List.Remove(Particle);
		}
	}

	// Check that the particles in an SOA have the appropriate bit set in their ListMask
	template<typename TSOAType>
	void CheckSOAMasks(TSOAType* SOA)
	{
		TArray<TSOAView<FGeometryParticles>> SOAViewArray = { SOA };
		auto View = MakeConstParticleView(MoveTemp(SOAViewArray));
		for (const auto& Particle : View)
		{
			ensureAlwaysMsgf(Particle.IsInAnyList(SOA->GetContainerListMask()), TEXT("Particle %s ListMask missing expected SOA bit 0x%x"), *GetParticleDebugName(Particle), SOA->GetContainerListMask());
		}
	}

	// Check that the particles in a List have the appropriate bit set in their ListMask
	template<typename TListType>
	void CheckListMasks(TListType& List)
	{
		TSOAView<FGeometryParticles> SOAView(&List.GetArray());
		TArray<TSOAView<FGeometryParticles>> SOAViewArray = { SOAView };
		auto View = MakeConstParticleView(MoveTemp(SOAViewArray));
		for (const auto& Particle : View)
		{
			ensureAlwaysMsgf(Particle.IsInAnyList(List.GetContainerListMask()), TEXT("Particle %s ListMask missing expected List bit 0x%x"), *GetParticleDebugName(Particle), List.GetContainerListMask());
		}
	}

	// Make sure the particle is not in a set of lists that would cause it to appear in any single view multiple times
	void CheckParticleViewMask(const FGeometryParticleHandle* Particle) const
	{
		// This is called after every change to a particle's SOA/List for validation. 
		// It should be cheap when the cvar is disabled but we fully remove it in shipping anyway
#if !UE_BUILD_SHIPPING
		if (CVars::bChaosSolverCheckParticleViews)
		{
			CheckParticleViewMaskImpl(Particle);
		}
#endif
	}

	// Make sure the particle is not in a set of lists that would cause it to appear in any single view multiple times
	CHAOS_API void CheckParticleViewMaskImpl(const FGeometryParticleHandle* Particle) const;


	template <typename TParticleHandleType, typename TParticles>
	TArray<TParticleHandleType*> CreateParticlesHelper(int32 NumParticles, const FUniqueIdx* ExistingIndices,  TUniquePtr<TParticles>& Particles, const FGeometryParticleParameters& Params)
	{
		const int32 ParticlesStartIdx = Particles->Size();
		Particles->AddParticles(NumParticles);
		TArray<TParticleHandleType*> ReturnHandles;
		ReturnHandles.AddUninitialized(NumParticles);

		const int32 HandlesStartIdx = ParticleHandles.Size();
		ParticleHandles.AddHandles(NumParticles);

		for (int32 Count = 0; Count < NumParticles; ++Count)
		{
			const int32 ParticleIdx = Count + ParticlesStartIdx;
			const int32 HandleIdx = Count + HandlesStartIdx;

			TUniquePtr<TParticleHandleType> NewParticleHandle = TParticleHandleType::CreateParticleHandle(MakeSerializable(Particles), ParticleIdx, HandleIdx);
			NewParticleHandle->SetParticleID(FParticleID{ INDEX_NONE, BiggestParticleID++ });
			NewParticleHandle->AddToLists(Particles->GetContainerListMask());
			ReturnHandles[Count] = NewParticleHandle.Get();
			//If unique indices are null it means there is no GT particle that already registered an ID, so create one
			if(ExistingIndices)
			{
				ReturnHandles[Count]->SetUniqueIdx(ExistingIndices[Count]);
			}
			else
			{
				ReturnHandles[Count]->SetUniqueIdx(UniqueIndices.GenerateUniqueIdx());
			}
			ParticleHandles.Handle(HandleIdx) = MoveTemp(NewParticleHandle);
			Particles->HasCollision(ParticleIdx) = true;	//todo: find a better place for this
		}

		return ReturnHandles;
	}
	
	void AddToActiveArray(const TArray<FPBDRigidParticleHandle*>& Particles)
	{
		AddToList(Particles, ActiveParticlesMapArray);

		if(bResimulating)
		{
			AddToList(Particles, ResimActiveParticlesMapArray);
		}
		
		//dirty contains Active so make sure no duplicates
		for(FPBDRigidParticleHandle* Particle : Particles)
		{
			RemoveFromList(Particle, TransientDirtyMapArray);
		}
	}

	void AddToActiveArray(FPBDRigidParticleHandle* Particle)
	{
		AddToList(Particle, ActiveParticlesMapArray);

		if (bResimulating)
		{
			AddToList(Particle, ResimActiveParticlesMapArray);
		}

		//dirty contains Active so make sure no duplicates
		RemoveFromList(Particle, TransientDirtyMapArray);
	}

	void RemoveFromActiveArray(FPBDRigidParticleHandle* Particle, bool bStillDirty)
	{
		RemoveFromList(Particle, ActiveParticlesMapArray);

		if (bResimulating)
		{
			RemoveFromList(Particle, ResimActiveParticlesMapArray);
		}

		if(bStillDirty)
		{
			if(!MovingKinematicsMapArray.Contains(Particle))
			{ 
				//no longer active, but still dirty
				AddToList(Particle, TransientDirtyMapArray);
			}
		}
		else
		{
			//might have already been removed from active from a previous call
			//but now removing and don't want it dirty either
			RemoveFromList(Particle, TransientDirtyMapArray);
		}
	}
	
	//should be called whenever particles are added / removed / reordered
	CHAOS_API void UpdateViews();

	void InsertClusteredParticle(FPBDRigidClusteredParticleHandle* ClusteredParticle)
	{
		if (!ClusteredParticle->Disabled())
		{
			switch (ClusteredParticle->ObjectState())
			{
			case EObjectStateType::Uninitialized:
				ensure(false); // we should probably not be here 
				break;
			case EObjectStateType::Static:
				AddToList(ClusteredParticle, StaticClusteredMapArray);
				break;
			case EObjectStateType::Kinematic:
				AddToList(ClusteredParticle, KinematicClusteredMapArray);
				break;
			case EObjectStateType::Dynamic:
			case EObjectStateType::Sleeping:
				AddToList(ClusteredParticle, DynamicClusteredMapArray);
				break;
			}
		}
	}

	void InsertClusteredParticles(const TArray<FPBDRigidClusteredParticleHandle*>& ClusteredParticleArray)
	{
		for (FPBDRigidClusteredParticleHandle* ClusteredParticle : ClusteredParticleArray)
		{
			InsertClusteredParticle(ClusteredParticle);
		}
	}

	void RemoveClusteredParticle(FPBDRigidClusteredParticleHandle* ClusteredParticle)
	{
		RemoveFromList(ClusteredParticle, StaticClusteredMapArray);
		RemoveFromList(ClusteredParticle, KinematicClusteredMapArray);
		RemoveFromList(ClusteredParticle, DynamicClusteredMapArray);
	}

	//Organized by SOA type
	TUniquePtr<FGeometryParticles> StaticParticles;
	TUniquePtr<FGeometryParticles> StaticDisabledParticles;

	TUniquePtr<FKinematicGeometryParticles> KinematicParticles;
	TUniquePtr<FKinematicGeometryParticles> KinematicDisabledParticles;

	TUniquePtr<FPBDRigidParticles> DynamicParticles;
	TUniquePtr<FPBDRigidParticles> DynamicKinematicParticles;
	TUniquePtr<FPBDRigidParticles> DynamicDisabledParticles;

	TUniquePtr<FPBDRigidClusteredParticles> ClusteredParticles;

	TUniquePtr<TPBDGeometryCollectionParticles<FReal, 3>> GeometryCollectionParticles;

	//
	// NOTE: The member here are enumerated in EGeometryParticleListMask which must
	// be kept up to date with changes here. Also see SetContainerListMasks() and CheckListMasks()
	//

	// Geometry collection particle state is controlled via their disabled state and assigned 
	// EObjectStateType, and are shuffled into these corresponding arrays in UpdateGeometryCollectionViews().
	TParticleMapArray<FPBDGeometryCollectionParticleHandle> StaticGeometryCollectionArray;
	TParticleMapArray<FPBDGeometryCollectionParticleHandle> KinematicGeometryCollectionArray;
	TParticleMapArray<FPBDGeometryCollectionParticleHandle> SleepingGeometryCollectionArray;
	TParticleMapArray<FPBDGeometryCollectionParticleHandle> DynamicGeometryCollectionArray;

	// Utility structures for maintaining an Active particles view
	TParticleMapArray<FPBDRigidParticleHandle> ActiveParticlesMapArray;
	TParticleMapArray<FPBDRigidParticleHandle> TransientDirtyMapArray;
	
	// Keep track of kinematic that have their kinematic target set for this current frame
	TParticleMapArray<FPBDRigidParticleHandle> MovingKinematicsMapArray;

	// Structures for maintaining a subset view during a resim (TParticleMapArray used when we need to dynamically add/remove during resim)
	TParticleMapArray<FPBDRigidParticleHandle> ResimActiveParticlesMapArray;
	TParticleMapArray<FPBDRigidParticleHandle> ResimDynamicParticles;
	TParticleMapArray<FPBDRigidParticleHandle> ResimDynamicKinematicParticles;
	TParticleArray<FGeometryParticleHandle> ResimStaticParticles;
	TParticleArray<FKinematicGeometryParticleHandle> ResimKinematicParticles;
	bool bResimulating = false;

	// NonDisabled clustered particle arrays
	TParticleMapArray<FPBDRigidClusteredParticleHandle> StaticClusteredMapArray;
	TParticleMapArray<FPBDRigidClusteredParticleHandle> KinematicClusteredMapArray;
	TParticleMapArray<FPBDRigidClusteredParticleHandle> DynamicClusteredMapArray;

	// Particle Views
	TParticleView<FGeometryParticles> NonDisabledView;								//all particles that are not disabled
	TParticleView<FPBDRigidParticles> NonDisabledDynamicView;						//all dynamic particles that are not disabled
	TParticleView<FPBDRigidClusteredParticles> NonDisabledClusteredView;			//all clustered particles that are not disabled
	TParticleView<FPBDRigidParticles> ActiveParticlesView;							//all particles that are active
	TParticleView<FPBDRigidParticles> DirtyParticlesView;							//all particles that are active + any that were put to sleep this frame
	TParticleView<FGeometryParticles> AllParticlesView;								//all particles
	TParticleView<FKinematicGeometryParticles> ActiveKinematicParticlesView;		//all kinematic particles that are not disabled
	TParticleView<FPBDRigidParticles> ActiveMovingKinematicParticlesView;			//all moving kinematic particles that are not disabled
	TParticleView<FPBDRigidParticles> ActiveDynamicMovingKinematicParticlesView;	//all moving kinematic particles that are not disabled + all dynamic particles
	TParticleView<FGeometryParticles> ActiveStaticParticlesView;					//all static particles that are not disabled
	TParticleView<TPBDGeometryCollectionParticles<FReal, 3>> ActiveGeometryCollectionParticlesView; // all geom collection particles that are not disabled

	// Auxiliary data synced with particle handles
	TGeometryParticleHandles<FReal, 3> ParticleHandles;

	IParticleUniqueIndices& UniqueIndices;

#if CHAOS_DETERMINISTIC
	int32 BiggestParticleID;
#endif
};

template <typename T, int d>
using TPBDRigidsSOAs UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FPBDRigidsSOAs instead") = FPBDRigidsSOAs;

}
