// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Chaos/Core.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Containers/CircularBuffer.h"
#include "Chaos/ResimCacheBase.h"
#include "Chaos/PBDJointConstraints.h"

#ifndef VALIDATE_REWIND_DATA
#define VALIDATE_REWIND_DATA 0
#endif

namespace Chaos
{

struct FFrameAndPhase
{
	enum EParticleHistoryPhase : uint8
	{
		//The particle state before PushData, server state update, or any sim callbacks are processed 
		//This is the results of the previous frame before any GT modifications are made in this frame
		PrePushData = 0,

		//The particle state after PushData is applied, but before any server state is applied
		//This is what the server state should be compared against
		//This is what we rewind to before a resim
		PostPushData,

		//The particle state after sim callbacks are applied.
		//This is used to detect desync of particles before simulation itself is run (these desyncs can come from server state or the sim callback itself)
		PostCallbacks,

		NumPhases
	};

	int32 Frame : 30;
	uint32 Phase : 2;

	bool operator<(const FFrameAndPhase& Other) const
	{
		return Frame < Other.Frame || (Frame == Other.Frame && Phase < Other.Phase);
	}

	bool operator<=(const FFrameAndPhase& Other) const
	{
		return Frame < Other.Frame || (Frame == Other.Frame && Phase <= Other.Phase);
	}

	bool operator==(const FFrameAndPhase& Other) const
	{
		return Frame == Other.Frame && Phase == Other.Phase;
	}
};

template <typename THandle, typename T, bool bNoEntryIsHead>
struct NoEntryInSync
{
	static bool Helper(const THandle& Handle)
	{
		//nothing written so we're pointing to the particle which means it's in sync
		return true;
	}
};

template <typename THandle, typename T>
struct NoEntryInSync<THandle, T, false>
{
	static bool Helper(const THandle& Handle)
	{
		//nothing written so compare to zero
		T HeadVal;
		HeadVal.CopyFrom(Handle);
		return HeadVal == T::ZeroValue();
	}
};

struct FPropertyInterval
{
	FPropertyIdx Ref;
	FFrameAndPhase FrameAndPhase;
};

template <typename TData, typename TObj>
void CopyDataFromObject(TData& Data, const TObj& Obj)
{
	Data.CopyFrom(Obj);
}

inline void CopyDataFromObject(FPBDJointSettings& Data, const FPBDJointConstraintHandle& Joint)
{
	Data = Joint.GetSettings();
}

template <typename T, EChaosProperty PropName, bool bNoEntryIsHead = true>
class TParticlePropertyBuffer
{
public:
	explicit TParticlePropertyBuffer(int32 InCapacity)
	: Next(0)
	, NumValid(0)
	, Capacity(InCapacity)
	{
	}

	TParticlePropertyBuffer(TParticlePropertyBuffer<T, PropName>&& Other)
	: Next(Other.Next)
	, NumValid(Other.NumValid)
	, Capacity(Other.Capacity)
	, Buffer(MoveTemp(Other.Buffer))
	{
		Other.NumValid = 0;
		Other.Next = 0;
	}

	TParticlePropertyBuffer(const TParticlePropertyBuffer<T, PropName>& Other) = delete;

	~TParticlePropertyBuffer()
	{
		//Need to explicitly cleanup before destruction using Release (release back into the pool)
		ensure(Buffer.Num() == 0);
	}

	//Gets access into buffer in monotonically increasing FrameAndPhase order: x_{n+1} > x_n
	T& WriteAccessMonotonic(const FFrameAndPhase FrameAndPhase, FDirtyPropertiesPool& Manager)
	{
		return *WriteAccessImp<true>(FrameAndPhase, Manager);
	}

	//Gets access into buffer in non-decreasing FrameAndPhase order: x_{n+1} >= x_n
	//If x_{n+1} == x_n we return null to inform the user (usefull when a single phase can have multiple writes)
	T* WriteAccessNonDecreasing(const FFrameAndPhase FrameAndPhase, FDirtyPropertiesPool& Manager)
	{
		return WriteAccessImp<false>(FrameAndPhase, Manager);
	}

	//Searches in reverse order for interval that contains FrameAndPhase
	const T* Read(const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Manager) const
	{
		const int32 Idx = FindIdx(FrameAndPhase);
		return Idx != INDEX_NONE ? &GetPool(Manager).GetElement(Buffer[Idx].Ref) : nullptr;
	}

	//Releases data back into the pool
	void Release(FDirtyPropertiesPool& Manager)
	{
		TPropertyPool<T>& Pool = GetPool(Manager);
		for(FPropertyInterval& Interval : Buffer)
	{
			Pool.RemoveElement(Interval.Ref);
	}

		Buffer.Empty();
		NumValid = 0;
	}

	void Reset()
	{
		NumValid = 0;
	}

	void ClearEntryAndFuture(const FFrameAndPhase FrameAndPhase)
	{
		//Move next backwards until FrameAndPhase and anything more future than it is gone
		while(NumValid)
		{
			const int32 PotentialNext = Next - 1 >= 0 ? Next - 1 : Buffer.Num() - 1;

			if(Buffer[PotentialNext].FrameAndPhase < FrameAndPhase)
	{
				break;
			}

			Next = PotentialNext;
			--NumValid;
		}
	}

	bool IsClean(const FFrameAndPhase FrameAndPhase) const
	{
		return FindIdx(FrameAndPhase) == INDEX_NONE;
	}

	template <typename THandle>
	bool IsInSync(const THandle& Handle, const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Pool) const
	{
		if (const T* Val = Read(FrameAndPhase, Pool))
		{
			T HeadVal;
			CopyDataFromObject(HeadVal, Handle);
			return *Val == HeadVal;
		}

		return NoEntryInSync<THandle, T, bNoEntryIsHead>::Helper(Handle);
	}

private:

	const int32 FindIdx(const FFrameAndPhase FrameAndPhase) const
	{
		int32 Cur = Next;	//go in reverse order because hopefully we don't rewind too far back
		int32 Result = INDEX_NONE;
		for(int32 Count = 0; Count < NumValid; ++Count)
		{
			--Cur;
			if (Cur < 0) { Cur = Buffer.Num() - 1; }

			const FPropertyInterval& Interval = Buffer[Cur];
			if(Interval.FrameAndPhase < FrameAndPhase)
			{
				//no reason to keep searching, frame is bigger than everything before this
				break;
			}
			else
	{
				Result = Cur;
			}
		}

		if(bNoEntryIsHead || Result == INDEX_NONE)
		{
			//in this mode we consider the entire interval as one entry
			return Result;
		}
		else
		{
			//in this mode each interval just represents the frame the property was dirtied on
			//so in that case we have to check for equality
			return Buffer[Result].FrameAndPhase == FrameAndPhase ? Result : INDEX_NONE;
		}
	}

	TPropertyPool<T>& GetPool(FDirtyPropertiesPool& Manager) { return Manager.GetPool<T, PropName>(); }
	const TPropertyPool<T>& GetPool(const FDirtyPropertiesPool& Manager) const { return Manager.GetPool<T, PropName>(); }

	//Gets access into buffer in FrameAndPhase order.
	//It's assumed FrameAndPhase is monotonically increasing: x_{n+1} > x_n
	//If bEnsureMonotonic is true we will always return a valid access (unless assert fires)
	//If bEnsureMonotonic is false we will ensure x_{n+1} >= x_n. If x_{n+1} == x_n we return null to inform the user (can be useful when multiple writes happen in same phase)
	template <bool bEnsureMonotonic>
	T* WriteAccessImp(const FFrameAndPhase FrameAndPhase, FDirtyPropertiesPool& Manager)
	{
		if (NumValid)
		{
			const int32 Prev = Next == 0 ? Buffer.Num() - 1 : Next - 1;
			const FFrameAndPhase& LatestFrameAndPhase = Buffer[Prev].FrameAndPhase;
			if (bEnsureMonotonic)
			{
				ensure(LatestFrameAndPhase < FrameAndPhase);	//Must write in monotonic growing order so that x_{n+1} > x_n
			}
			else
			{
				ensure(LatestFrameAndPhase <= FrameAndPhase);	//Must write in growing order so that x_{n+1} >= x_n
				if (LatestFrameAndPhase == FrameAndPhase)
	{
					//Already wrote once for this FrameAndPhase so skip
					return nullptr;
				}
			}

			ValidateOrder();
	}

		T* Result;

		if (Next < Buffer.Num())
	{
			//reuse
			FPropertyInterval& Interval = Buffer[Next];
			Interval.FrameAndPhase = FrameAndPhase;
			Result = &GetPool(Manager).GetElement(Interval.Ref);
		}
		else
		{
			//no reuse yet so can just push
			FPropertyIdx NewIdx;
			Result = &GetPool(Manager).AddElement(NewIdx);
			Buffer.Add({NewIdx, FrameAndPhase });
		}

		++Next;
		if (Next == Capacity) { Next = 0; }

		NumValid = FMath::Min(++NumValid, Capacity);

		return Result;
	}

	void ValidateOrder()
	{
#if VALIDATE_REWIND_DATA
		int32 Val = Next;
		FFrameAndPhase PrevVal;
		for(int32 Count = 0; Count < NumValid; ++Count)
	{
			--Val;
			if (Val < 0) { Val = Buffer.Num() - 1; }
			if (Count == 0)
		{
				PrevVal = Buffer[Val].FrameAndPhase;
		}
		else
		{
				ensure(Buffer[Val].FrameAndPhase < PrevVal);
				PrevVal = Buffer[Val].FrameAndPhase;
			}
		}
#endif
	}

private:
	int32 Next;
	int32 NumValid;
	int32 Capacity;
	TArray<FPropertyInterval> Buffer;
};


enum EDesyncResult
{
	InSync, //both have entries and are identical, or both have no entries
	Desync, //both have entries but they are different
	NeedInfo //one of the entries is missing. Need more context to determine whether desynced
};

// Wraps FDirtyPropertiesManager and its DataIdx to avoid confusion between Source and offset Dest indices
struct FDirtyPropData
{
	FDirtyPropData(FDirtyPropertiesManager* InManager, int32 InDataIdx)
		: Ptr(InManager), DataIdx(InDataIdx) { }

	FDirtyPropertiesManager* Ptr;
	int32 DataIdx;
};

struct FConstDirtyPropData
{
	FConstDirtyPropData(const FDirtyPropertiesManager* InManager, int32 InDataIdx)
		: Ptr(InManager), DataIdx(InDataIdx) { }

	const FDirtyPropertiesManager* Ptr;
	int32 DataIdx;
};

template <typename T, EShapeProperty PropName>
class TPerShapeDataStateProperty
{
public:
	const T& Read() const
	{
		check(bSet);
		return Val;
	}

	void Write(const T& InVal)
	{
		bSet = true;
		Val = InVal;
	}

	bool IsSet() const
	{
		return bSet;
	}

private:
	T Val;
	bool bSet = false;
};

struct FPerShapeDataStateBase
{
	TPerShapeDataStateProperty<FCollisionData, EShapeProperty::CollisionData> CollisionData;
	TPerShapeDataStateProperty<FMaterialData, EShapeProperty::Materials> MaterialData;

	//helper functions for shape API
	template <typename TParticle>
	static const FCollisionFilterData& GetQueryData(const FPerShapeDataStateBase* State, const TParticle& Particle, int32 ShapeIdx) { return State && State->CollisionData.IsSet() ? State->CollisionData.Read().QueryData : Particle.ShapesArray()[ShapeIdx]->GetQueryData(); }
};

class FPerShapeDataState
{
public:
	FPerShapeDataState(const FPerShapeDataStateBase* InState, const FGeometryParticleHandle& InParticle, const int32 InShapeIdx)
	: State(InState)
	, Particle(InParticle)
	, ShapeIdx(InShapeIdx)
	{
	}

	const FCollisionFilterData& GetQueryData() const { return FPerShapeDataStateBase::GetQueryData(State, Particle, ShapeIdx); }
private:
	const FPerShapeDataStateBase* State;
	const FGeometryParticleHandle& Particle;
	const int32 ShapeIdx;

};

struct FShapesArrayStateBase
{
	TArray<FPerShapeDataStateBase> PerShapeData;

	FPerShapeDataStateBase& FindOrAdd(const int32 ShapeIdx)
	{
		if(ShapeIdx >= PerShapeData.Num())
		{
			const int32 NumNeededToAdd = ShapeIdx + 1 - PerShapeData.Num();
			PerShapeData.AddDefaulted(NumNeededToAdd);
		}
		return PerShapeData[ShapeIdx];

	}
};

template <typename T>
FString ToStringHelper(const T& Val)
{
	return Val.ToString();
}

template <typename T>
FString ToStringHelper(const TVector<T, 2>& Val)
{
	return FString::Printf(TEXT("(%s, %s)"), *Val[0].ToString(), *Val[1].ToString());
}

inline FString ToStringHelper(void* Val)
{
	// We don't print pointers because they will always be different in diff, need this function so we will compile
	// when using property .inl macros.
	return FString();
}

inline FString ToStringHelper(const FReal Val)
{
	return FString::Printf(TEXT("%f"), Val);
}

inline FString ToStringHelper(const EObjectStateType Val)
{
	return FString::Printf(TEXT("%d"), Val);
}

inline FString ToStringHelper(const EPlasticityType Val)
{
	return FString::Printf(TEXT("%d"), Val);
}

inline FString ToStringHelper(const EJointForceMode Val)
{
	return FString::Printf(TEXT("%d"), Val);
}

inline FString ToStringHelper(const EJointMotionType Val)
{
	return FString::Printf(TEXT("%d"), Val);
}

inline FString ToStringHelper(const bool Val)
{
	return FString::Printf(TEXT("%d"), Val);
}

inline FString ToStringHelper(const int32 Val)
{
	return FString::Printf(TEXT("%d"), Val);
}


template <typename TParticle>
class TShapesArrayState
{
public:
	TShapesArrayState(const TParticle& InParticle, const FShapesArrayStateBase* InState)
		: Particle(InParticle)
		, State(InState)
	{}

	FPerShapeDataState operator[](const int32 ShapeIdx) const { return FPerShapeDataState{ State && ShapeIdx < State->PerShapeData.Num() ? &State->PerShapeData[ShapeIdx] : nullptr, Particle, ShapeIdx }; }
private:
	const TParticle& Particle;
	const FShapesArrayStateBase* State;
};

#define REWIND_CHAOS_PARTICLE_PROPERTY(PROP, NAME)\
		const auto Data = State ? State->PROP.Read(FrameAndPhase, Pool) : nullptr;\
		return Data ? Data->NAME() : Head.NAME();\

#define REWIND_CHAOS_ZERO_PARTICLE_PROPERTY(PROP, NAME)\
		const auto Data = State ? State->PROP.Read(FrameAndPhase, Pool) : nullptr;\
		return Data ? Data->NAME() : ZeroVector;\

#define REWIND_PARTICLE_STATIC_PROPERTY(PROP, NAME)\
	decltype(auto) NAME() const\
	{\
		auto& Head = Particle;\
		REWIND_CHAOS_PARTICLE_PROPERTY(PROP, NAME);\
	}\

#define REWIND_PARTICLE_KINEMATIC_PROPERTY(PROP, NAME)\
	decltype(auto) NAME() const\
	{\
		auto& Head = *Particle.CastToKinematicParticle();\
		REWIND_CHAOS_PARTICLE_PROPERTY(PROP, NAME);\
	}\

#define REWIND_PARTICLE_RIGID_PROPERTY(PROP, NAME)\
	decltype(auto) NAME() const\
	{\
		auto& Head = *Particle.CastToRigidParticle();\
		REWIND_CHAOS_PARTICLE_PROPERTY(PROP, NAME);\
	}\

#define REWIND_PARTICLE_ZERO_PROPERTY(PROP, NAME)\
	decltype(auto) NAME() const\
	{\
		auto& Head = *Particle.CastToRigidParticle();\
		REWIND_CHAOS_ZERO_PARTICLE_PROPERTY(PROP, NAME);\
	}\

#define REWIND_JOINT_PROPERTY(PROP, FUNC_NAME, NAME)\
	decltype(auto) Get##FUNC_NAME() const\
	{\
		const auto Data = State ? State->PROP.Read(FrameAndPhase, Pool) : nullptr;\
		return Data ? Data->NAME : Head.Get##PROP().NAME;\
	}\

inline int32 ComputeCircularSize(int32 NumFrames) { return NumFrames * FFrameAndPhase::NumPhases; }

struct FGeometryParticleStateBase
{
	explicit FGeometryParticleStateBase(int32 NumFrames)
	: ParticlePositionRotation(ComputeCircularSize(NumFrames))
	, NonFrequentData(ComputeCircularSize(NumFrames))
	, Velocities(ComputeCircularSize(NumFrames))
	, Dynamics(ComputeCircularSize(NumFrames))
	, DynamicsMisc(ComputeCircularSize(NumFrames))
	, MassProps(ComputeCircularSize(NumFrames))
	, KinematicTarget(ComputeCircularSize(NumFrames))
	{

	}

	FGeometryParticleStateBase(const FGeometryParticleStateBase& Other) = delete;
	FGeometryParticleStateBase(FGeometryParticleStateBase&& Other) = default;
	~FGeometryParticleStateBase() = default;

	void Release(FDirtyPropertiesPool& Manager)
	{
		ParticlePositionRotation.Release(Manager);
		NonFrequentData.Release(Manager);
		Velocities.Release(Manager);
		Dynamics.Release(Manager);
		DynamicsMisc.Release(Manager);
		MassProps.Release(Manager);
		KinematicTarget.Release(Manager);
	}

	void Reset()
	{
		ParticlePositionRotation.Reset();
		NonFrequentData.Reset();
		Velocities.Reset();
		Dynamics.Reset();
		DynamicsMisc.Reset();
		MassProps.Reset();
		KinematicTarget.Reset();
	}

	void ClearEntryAndFuture(const FFrameAndPhase FrameAndPhase)
	{
		ParticlePositionRotation.ClearEntryAndFuture(FrameAndPhase);
		NonFrequentData.ClearEntryAndFuture(FrameAndPhase);
		Velocities.ClearEntryAndFuture(FrameAndPhase);
		Dynamics.ClearEntryAndFuture(FrameAndPhase);
		DynamicsMisc.ClearEntryAndFuture(FrameAndPhase);
		MassProps.ClearEntryAndFuture(FrameAndPhase);
		KinematicTarget.ClearEntryAndFuture(FrameAndPhase);
	}

	bool IsClean(const FFrameAndPhase FrameAndPhase) const
	{
		return IsCleanExcludingDynamics(FrameAndPhase) && Dynamics.IsClean(FrameAndPhase);
	}

	bool IsCleanExcludingDynamics(const FFrameAndPhase FrameAndPhase) const
	{
		return ParticlePositionRotation.IsClean(FrameAndPhase) &&
			NonFrequentData.IsClean(FrameAndPhase) &&
			Velocities.IsClean(FrameAndPhase) &&
			DynamicsMisc.IsClean(FrameAndPhase) &&
			MassProps.IsClean(FrameAndPhase) &&
			KinematicTarget.IsClean(FrameAndPhase);
	}

	template <bool bSkipDynamics = false>
	bool IsInSync(const FGeometryParticleHandle& Handle, const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Pool) const;
	
	template <typename TParticle>
	static TShapesArrayState<TParticle> ShapesArray(const FGeometryParticleStateBase* State, const TParticle& Particle)
	{
		return TShapesArrayState<TParticle>{ Particle, State ? &State->ShapesArrayState : nullptr };
	}

	void SyncSimWritablePropsFromSim(FDirtyPropData Manager,const TPBDRigidParticleHandle<FReal,3>& Rigid);
	void SyncDirtyDynamics(FDirtyPropData& DestManager,const FDirtyChaosProperties& Dirty,const FConstDirtyPropData& SrcManager);
	
	TParticlePropertyBuffer<FParticlePositionRotation,EChaosProperty::XR> ParticlePositionRotation;
	TParticlePropertyBuffer<FParticleNonFrequentData,EChaosProperty::NonFrequentData> NonFrequentData;
	TParticlePropertyBuffer<FParticleVelocities,EChaosProperty::Velocities> Velocities;
	TParticlePropertyBuffer<FParticleDynamics,EChaosProperty::Dynamics, /*bNoEntryIsHead=*/false> Dynamics;
	TParticlePropertyBuffer<FParticleDynamicMisc,EChaosProperty::DynamicMisc> DynamicsMisc;
	TParticlePropertyBuffer<FParticleMassProps,EChaosProperty::MassProps> MassProps;
	TParticlePropertyBuffer<FKinematicTarget, EChaosProperty::KinematicTarget> KinematicTarget;

	FShapesArrayStateBase ShapesArrayState;
};

class FGeometryParticleState
{
public:

	FGeometryParticleState(const FGeometryParticleHandle& InParticle, const FDirtyPropertiesPool& InPool)
	: Particle(InParticle)
	, Pool(InPool)
	, FrameAndPhase{0,0}
	{
	}

	FGeometryParticleState(const FGeometryParticleStateBase* InState, const FGeometryParticleHandle& InParticle, const FDirtyPropertiesPool& InPool, const FFrameAndPhase InFrameAndPhase)
	: Particle(InParticle)
	, Pool(InPool)
	, State(InState)
	, FrameAndPhase(InFrameAndPhase)
	{
	}


	REWIND_PARTICLE_STATIC_PROPERTY(ParticlePositionRotation, X)
	REWIND_PARTICLE_STATIC_PROPERTY(ParticlePositionRotation, R)

	REWIND_PARTICLE_KINEMATIC_PROPERTY(Velocities, V)
	REWIND_PARTICLE_KINEMATIC_PROPERTY(Velocities, W)

	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, LinearEtherDrag)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, AngularEtherDrag)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, MaxLinearSpeedSq)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, MaxAngularSpeedSq)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, ObjectState)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, CollisionGroup)
	REWIND_PARTICLE_RIGID_PROPERTY(DynamicsMisc, ControlFlags)

	REWIND_PARTICLE_RIGID_PROPERTY(MassProps, CenterOfMass)
	REWIND_PARTICLE_RIGID_PROPERTY(MassProps, RotationOfMass)
	REWIND_PARTICLE_RIGID_PROPERTY(MassProps, I)
	REWIND_PARTICLE_RIGID_PROPERTY(MassProps, M)
	REWIND_PARTICLE_RIGID_PROPERTY(MassProps, InvM)

	REWIND_PARTICLE_STATIC_PROPERTY(NonFrequentData, Geometry)
	REWIND_PARTICLE_STATIC_PROPERTY(NonFrequentData, UniqueIdx)
	REWIND_PARTICLE_STATIC_PROPERTY(NonFrequentData, SpatialIdx)
#if CHAOS_DEBUG_NAME
	REWIND_PARTICLE_STATIC_PROPERTY(NonFrequentData, DebugName)
#endif

	REWIND_PARTICLE_ZERO_PROPERTY(Dynamics, Acceleration)
	REWIND_PARTICLE_ZERO_PROPERTY(Dynamics, AngularAcceleration)
	REWIND_PARTICLE_ZERO_PROPERTY(Dynamics, LinearImpulseVelocity)
	REWIND_PARTICLE_ZERO_PROPERTY(Dynamics, AngularImpulseVelocity)

	TShapesArrayState<FGeometryParticleHandle> ShapesArray() const
	{
		return FGeometryParticleStateBase::ShapesArray(State, Particle);
	}

	const FGeometryParticleHandle& GetHandle() const
	{
		return Particle;
	}

	void SetState(const FGeometryParticleStateBase* InState)
	{
		State = InState;
	}

	FString ToString() const
	{
#undef REWIND_PARTICLE_TO_STR
#define REWIND_PARTICLE_TO_STR(PropName) Out += FString::Printf(TEXT(#PropName":%s\n"), *ToStringHelper(PropName()));
		//TODO: use macro to define api and the to string
		FString Out = FString::Printf(TEXT("ParticleID:[Global: %d Local: %d]\n"), Particle.ParticleID().GlobalID, Particle.ParticleID().LocalID);

		REWIND_PARTICLE_TO_STR(X)
		REWIND_PARTICLE_TO_STR(R)
		//REWIND_PARTICLE_TO_STR(Geometry)
		//REWIND_PARTICLE_TO_STR(UniqueIdx)
		//REWIND_PARTICLE_TO_STR(SpatialIdx)

		if(Particle.CastToKinematicParticle())
		{
			REWIND_PARTICLE_TO_STR(V)
			REWIND_PARTICLE_TO_STR(W)
		}

		if(Particle.CastToRigidParticle())
		{
			REWIND_PARTICLE_TO_STR(LinearEtherDrag)
			REWIND_PARTICLE_TO_STR(AngularEtherDrag)
			REWIND_PARTICLE_TO_STR(MaxLinearSpeedSq)
			REWIND_PARTICLE_TO_STR(MaxAngularSpeedSq)
			REWIND_PARTICLE_TO_STR(ObjectState)
			REWIND_PARTICLE_TO_STR(CollisionGroup)
			REWIND_PARTICLE_TO_STR(ControlFlags)

			REWIND_PARTICLE_TO_STR(CenterOfMass)
			REWIND_PARTICLE_TO_STR(RotationOfMass)
			REWIND_PARTICLE_TO_STR(I)
			REWIND_PARTICLE_TO_STR(M)
			REWIND_PARTICLE_TO_STR(InvM)

			REWIND_PARTICLE_TO_STR(Acceleration)
			REWIND_PARTICLE_TO_STR(AngularAcceleration)
			REWIND_PARTICLE_TO_STR(LinearImpulseVelocity)
			REWIND_PARTICLE_TO_STR(AngularImpulseVelocity)
		}

		return Out;
	}

private:
	const FGeometryParticleHandle& Particle;
	const FDirtyPropertiesPool& Pool;
	const FGeometryParticleStateBase* State = nullptr;
	const FFrameAndPhase FrameAndPhase;

	CHAOS_API static FVec3 ZeroVector;

	};


struct FJointStateBase
{
	explicit FJointStateBase(int32 NumFrames)
		: JointSettings(ComputeCircularSize(NumFrames))
		, JointProxies(ComputeCircularSize(NumFrames))
	{
	}

	FJointStateBase(const FJointStateBase& Other) = delete;
	FJointStateBase(FJointStateBase&& Other) = default;
	~FJointStateBase() = default;

	void Release(FDirtyPropertiesPool& Manager)
	{
		JointSettings.Release(Manager);
		JointProxies.Release(Manager);
	}

	void Reset()
	{
		JointSettings.Reset();
		JointProxies.Reset();
	}

	void ClearEntryAndFuture(const FFrameAndPhase FrameAndPhase)
	{
		JointSettings.ClearEntryAndFuture(FrameAndPhase);
		JointProxies.ClearEntryAndFuture(FrameAndPhase);
	}

	bool IsClean(const FFrameAndPhase FrameAndPhase) const
	{
		return JointSettings.IsClean(FrameAndPhase) && JointProxies.IsClean(FrameAndPhase);
	}

	template <bool bSkipDynamics>
	bool IsInSync(const FPBDJointConstraintHandle& Handle, const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Pool) const;

	TParticlePropertyBuffer<FPBDJointSettings, EChaosProperty::JointSettings> JointSettings;
	TParticlePropertyBuffer<FProxyBasePairProperty, EChaosProperty::JointParticleProxies> JointProxies;
};

class FJointState
{
public:
	FJointState(const FPBDJointConstraintHandle& InJoint, const FDirtyPropertiesPool& InPool)
	: Head(InJoint)
	, Pool(InPool)
	{
	}

	FJointState(const FJointStateBase* InState, const FPBDJointConstraintHandle& InJoint, const FDirtyPropertiesPool& InPool, const FFrameAndPhase InFrameAndPhase)
	: Head(InJoint)
	, Pool(InPool)
	, State(InState)
	, FrameAndPhase(InFrameAndPhase)
	{
	}

	//See JointProperties for API
	//Each CHAOS_INNER_JOINT_PROPERTY entry will have a Get*
#define CHAOS_INNER_JOINT_PROPERTY(OuterProp, FuncName, Inner, InnerType) REWIND_JOINT_PROPERTY(OuterProp, FuncName, Inner);
#include "Chaos/JointProperties.inl"


	FString ToString() const
	{
		TVector<FGeometryParticleHandle*, 2> Particles = Head.GetConstrainedParticles();
		FString Out = FString::Printf(TEXT("Joint: Particle0 ID:[Global: %d Local: %d] Particle1 ID:[Global: %d Local: %d]\n"), Particles[0]->ParticleID().GlobalID, Particles[0]->ParticleID().LocalID, Particles[1]->ParticleID().GlobalID, Particles[1]->ParticleID().LocalID);

#define CHAOS_INNER_JOINT_PROPERTY(OuterProp, FuncName, Inner, InnerType) Out += FString::Printf(TEXT(#FuncName":%s\n"), *ToStringHelper(Get##FuncName()));
#include "Chaos/JointProperties.inl"
#undef CHAOS_INNER_JOINT_PROPERTY

		return Out;
	}

private:
	const FPBDJointConstraintHandle& Head;
	const FDirtyPropertiesPool& Pool;
	const FJointStateBase* State = nullptr;
	const FFrameAndPhase FrameAndPhase = { 0,0 };
};

template <typename T> 
const T* ConstifyHelper(T* Ptr) { return Ptr; }

template <typename T>
T NoRefHelper(const T& Ref) { return Ref; }

extern CHAOS_API int32 EnableResimCache;

template <typename TVal>
class TDirtyObjects
{
public:
	using TKey = decltype(ConstifyHelper(
		((TVal*)0)->GetObjectPtr()
	));

	TVal& Add(const TKey Key, TVal&& Val)
	{
		if(int32* ExistingIdx = KeyToIdx.Find(Key))
		{
			ensure(false);	//Item alread exists, shouldn't be adding again
			return DenseVals[*ExistingIdx];
		}
		else
		{
			const int32 Idx = DenseVals.Emplace(MoveTemp(Val));
			KeyToIdx.Add(Key, Idx);
			return DenseVals[Idx];
		}
	}

	const TVal& FindChecked(const TKey Key) const
	{
		const int32 Idx = KeyToIdx.FindChecked(Key);
		return DenseVals[Idx];
	}

	TVal& FindChecked(const TKey Key)
	{
		const int32 Idx = KeyToIdx.FindChecked(Key);
		return DenseVals[Idx];
	}

	const TVal* Find(const TKey Key) const
	{
		if (const int32* Idx = KeyToIdx.Find(Key))
		{
			return &DenseVals[*Idx];
		}

		return nullptr;
	}

	TVal* Find(const TKey Key)
	{
		if (const int32* Idx = KeyToIdx.Find(Key))
		{
			return &DenseVals[*Idx];
		}

		return nullptr;
	}

	void Remove(const TKey Key)
	{
		if (const int32* Idx = KeyToIdx.Find(Key))
		{
			DenseVals.RemoveAtSwap(*Idx);

			if(*Idx < DenseVals.Num())
			{
				const TKey SwappedKey = DenseVals[*Idx].GetObjectPtr();
				KeyToIdx.FindChecked(SwappedKey) = *Idx;
			}

			KeyToIdx.Remove(Key);
		}
	}

	int32 Num() const { return DenseVals.Num(); }

	auto begin() { return DenseVals.begin(); }
	auto end() { return DenseVals.end(); }

	auto cbegin() const { return DenseVals.begin(); }
	auto cend() const { return DenseVals.end(); }

	const TVal& GetDenseAt(const int32 Idx) const { return DenseVals[Idx]; }
	TVal& GetDenseAt(const int32 Idx) { return DenseVals[Idx]; }

private:
	TMap<TKey, int32> KeyToIdx;
	TArray<TVal> DenseVals;
};

extern CHAOS_API int32 SkipDesyncTest;

class FPBDRigidsSolver;

class FRewindData
{
public:
	FRewindData(FPBDRigidsSolver* InSolver, int32 NumFrames, bool InResimOptimization, int32 InCurrentFrame)
	: Managers(NumFrames+1)	//give 1 extra for saving at head
	, Solver(InSolver)
	, CurFrame(InCurrentFrame)
	, LatestFrame(-1)
	, FramesSaved(0)
	, DataIdxOffset(0)
	, bNeedsSave(false)
	, bResimOptimization(InResimOptimization)
	{
	}

	int32 Capacity() const { return Managers.Capacity(); }
	int32 CurrentFrame() const { return CurFrame; }
	int32 GetFramesSaved() const { return FramesSaved; }

	FReal GetDeltaTimeForFrame(int32 Frame) const
	{
		ensure(Managers[Frame].FrameCreatedFor == Frame);
		return Managers[Frame].DeltaTime;
	}

	void CHAOS_API RemoveObject(const FGeometryParticleHandle* Particle)
	{
		DirtyParticles.Remove(Particle);
	}

	void CHAOS_API RemoveObject(const FPBDJointConstraintHandle* Joint)
	{
		DirtyJoints.Remove(Joint);
	}

	int32 CHAOS_API GetEarliestFrame_Internal() const { return CurFrame - FramesSaved; }

	/* Query the state of particles from the past. Can only be used when not already resimming*/
	FGeometryParticleState CHAOS_API GetPastStateAtFrame(const FGeometryParticleHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase = FFrameAndPhase::EParticleHistoryPhase::PostPushData) const;

	/* Query the state of joints from the past. Can only be used when not already resimming*/
	FJointState CHAOS_API GetPastJointStateAtFrame(const FPBDJointConstraintHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase = FFrameAndPhase::EParticleHistoryPhase::PostPushData) const;

	IResimCacheBase* GetCurrentStepResimCache() const
	{
		return !!EnableResimCache && bResimOptimization ? Managers[CurFrame].ExternalResimCache.Get() : nullptr;
	}

	void CHAOS_API DumpHistory_Internal(const int32 FramePrintOffset, const FString& Filename = FString(TEXT("Dump")));

	template <typename CreateCache>
	void AdvanceFrame(FReal DeltaTime, const CreateCache& CreateCacheFunc)
	{
		QUICK_SCOPE_CYCLE_COUNTER(RewindDataAdvance);
		Managers[CurFrame].DeltaTime = DeltaTime;
		Managers[CurFrame].FrameCreatedFor = CurFrame;
		TUniquePtr<IResimCacheBase>& ResimCache = Managers[CurFrame].ExternalResimCache;

		if(bResimOptimization)
		{
			if(IsResim())
			{
				if(ResimCache)
				{
					ResimCache->SetResimming(true);
				}
			}
			else
			{
				if(ResimCache)
				{
					ResimCache->ResetCache();
				} else
				{
					ResimCache = CreateCacheFunc();
				}
				ResimCache->SetResimming(false);
			}
		}
		else
		{
			ResimCache.Reset();
		}

		AdvanceFrameImp(ResimCache.Get());
	}

	void FinishFrame();

	bool IsResim() const
	{
		return CurFrame < LatestFrame;
	}

	bool IsFinalResim() const
	{
		return (CurFrame + 1) == LatestFrame;
	}

	//Number of particles that we're currently storing history for
	int32 GetNumDirtyParticles() const { return DirtyParticles.Num(); }

	void PushGTDirtyData(const FDirtyPropertiesManager& SrcManager,const int32 SrcDataIdx,const FDirtyProxy& Dirty, const FShapeDirtyData* ShapeDirtyData);

	void PushPTDirtyData(TPBDRigidParticleHandle<FReal,3>& Rigid,const int32 SrcDataIdx);

	void CHAOS_API MarkDirtyFromPT(FGeometryParticleHandle& Handle);
	void CHAOS_API MarkDirtyJointFromPT(FPBDJointConstraintHandle& Handle);

	void CHAOS_API SpawnProxyIfNeeded(FSingleParticlePhysicsProxy& Proxy);

private:

	friend class FPBDRigidsSolver;

	void CHAOS_API AdvanceFrameImp(IResimCacheBase* ResimCache);

	struct FFrameManagerInfo
	{
		TUniquePtr<IResimCacheBase> ExternalResimCache;

		//Note that this is not exactly the same as which frame this manager represents. 
		//A manager can have data for two frames at once, the important part is just knowing which frame it was created on so we know whether the physics data can rely on it
		//Consider the case where nothing is dirty from GT and then an object moves from the simulation, in that case it needs a manager to record the data into
		int32 FrameCreatedFor = INDEX_NONE;
		FReal DeltaTime;
	};

	template <typename THistoryType, typename TObj>
	struct TDirtyObjectInfo
	{
	private:
		THistoryType History;
		TObj* ObjPtr;
		FDirtyPropertiesPool* PropertiesPool;
	public:
		int32 DirtyDynamics = INDEX_NONE;	//Only used by particles, indicates the dirty properties was written to.
		int32 LastDirtyFrame;	//Track how recently this was made dirty
		int32 InitializedOnStep = INDEX_NONE;	//if not INDEX_NONE, it indicates we saw initialization during rewind history window
		UE_DEPRECATED(5.1, "bResimAsSlave is deprecated - please use bResimAsFollower")
		bool bResimAsSlave = true;
		bool bResimAsFollower = true;	//Indicates the particle will always resim in the exact same way from game thread data

		TDirtyObjectInfo(FDirtyPropertiesPool& InPropertiesPool, TObj& InObj, const int32 CurFrame, const int32 NumFrames)
			: History(NumFrames)
			, ObjPtr(&InObj)
			, PropertiesPool(&InPropertiesPool)
			, LastDirtyFrame(CurFrame)
		{
		}

		TDirtyObjectInfo(TDirtyObjectInfo&& Other)
			: History(MoveTemp(Other.History))
			, ObjPtr(Other.ObjPtr)
			, PropertiesPool(Other.PropertiesPool)
			, LastDirtyFrame(Other.LastDirtyFrame)
			, InitializedOnStep(Other.InitializedOnStep)
			, bResimAsFollower(Other.bResimAsFollower)
		{
			Other.PropertiesPool = nullptr;
		}

		~TDirtyObjectInfo()
		{
			if (PropertiesPool)
			{
				History.Release(*PropertiesPool);
			}
		}

		TDirtyObjectInfo(const TDirtyObjectInfo& Other) = delete;

		TObj* GetObjectPtr() const { return ObjPtr; }

		THistoryType& AddFrame(const int32 Frame)
		{
			LastDirtyFrame = Frame;
			return History;
		}

		void ClearPhaseAndFuture(const FFrameAndPhase FrameAndPhase)
		{
			History.ClearEntryAndFuture(FrameAndPhase);
		}

		const THistoryType& GetHistory() const	//For non-const access use AddFrame
		{
			return History;
		}
	};

	using FDirtyParticleInfo = TDirtyObjectInfo<FGeometryParticleStateBase, FGeometryParticleHandle>;
	using FDirtyJointInfo = TDirtyObjectInfo<FJointStateBase, FPBDJointConstraintHandle>;

	template <typename TDirtyObjs, typename TObj>
	auto& FindOrAddDirtyObjImp(TDirtyObjs& DirtyObjs, TObj& Handle, const int32 InitializedOnFrame = INDEX_NONE)
	{
		if (auto Info = DirtyObjs.Find(&Handle))
		{
			return *Info;
		}

		using TDirtyObj = decltype(NoRefHelper(DirtyObjs.GetDenseAt(0)));
		TDirtyObj& Info = DirtyObjs.Add(&Handle, TDirtyObj(PropertiesPool, Handle, CurFrame, Managers.Capacity()));
		Info.InitializedOnStep = InitializedOnFrame;
		return Info;
	}

	FDirtyParticleInfo& FindOrAddDirtyObj(FGeometryParticleHandle& Handle, const int32 InitializedOnFrame = INDEX_NONE)
	{
		return FindOrAddDirtyObjImp(DirtyParticles, Handle, InitializedOnFrame);
	}

	FDirtyJointInfo& FindOrAddDirtyObj(FPBDJointConstraintHandle& Handle, const int32 InitializedOnFrame = INDEX_NONE)
	{
		return FindOrAddDirtyObjImp(DirtyJoints, Handle, InitializedOnFrame);
	}

	template <typename TObjState, typename TDirtyObjs, typename TObj>
	auto GetPastStateAtFrameImp(const TDirtyObjs& DirtyObjs, const TObj& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase) const
	{
		ensure(!IsResim());
		ensure(Frame >= GetEarliestFrame_Internal());	//can't get state from before the frame we rewound to

		const auto* Info = DirtyObjs.Find(&Handle);
		const auto* State = Info ? &Info->GetHistory() : nullptr;
		return TObjState(State, Handle, PropertiesPool, { Frame, Phase });
	}

	bool RewindToFrame(int32 Frame);

	template <typename TDirtyInfo>
	static void DesyncObject(TDirtyInfo& Info, const FFrameAndPhase FrameAndPhase)
	{
		Info.ClearPhaseAndFuture(FrameAndPhase);
		Info.GetObjectPtr()->SetSyncState(ESyncState::HardDesync);
	}

	TCircularBuffer<FFrameManagerInfo> Managers;
	FDirtyPropertiesPool PropertiesPool;	//must come before DirtyParticles since it relies on it (and used in destruction)

	TDirtyObjects<FDirtyParticleInfo> DirtyParticles;
	TDirtyObjects<FDirtyJointInfo> DirtyJoints;

	FPBDRigidsSolver* Solver;
	int32 CurFrame;
	int32 LatestFrame;
	int32 FramesSaved;
	int32 DataIdxOffset;
	bool bNeedsSave;	//Indicates that some data is pointing at head and requires saving before a rewind
	bool bResimOptimization;

	template <typename TObj>
	bool IsResimAndInSync(const TObj& Handle) const { return IsResim() && Handle.SyncState() == ESyncState::InSync; }

	template <bool bSkipDynamics, typename TDirtyInfo>
	void DesyncIfNecessary(TDirtyInfo& Info, const FFrameAndPhase FrameAndPhase);
};

struct FResimDebugInfo
{
	double ResimTime = 0.0;
};

/** Used by user code to determine when rewind should occur and gives it the opportunity to record any additional data */
class IRewindCallback
{
public:
	virtual ~IRewindCallback() = default;
	/** Called before any sim callbacks are triggered but after physics data has marshalled over
	*   This means brand new physics particles are already created for example, and any pending game thread modifications have happened
	*   See ISimCallbackObject for recording inputs to callbacks associated with this PhysicsStep */
	virtual void ProcessInputs_Internal(int32 PhysicsStep, const TArray<FSimCallbackInputAndObject>& SimCallbackInputs){}

	/** Called before any inputs are marshalled over to the physics thread.
	*	The physics state has not been applied yet, and cannot be inspected anyway because this is triggered from the external thread (game thread)
	*	Gives user the ability to modify inputs or record them - this can help with reducing latency if you want to act on inputs immediately
	*/
	virtual void ProcessInputs_External(int32 PhysicsStep, const TArray<FSimCallbackInputAndObject>& SimCallbackInputs) {}

	/** Called before inputs are split into potential sub-steps and marshalled over to the physics thread.
	*	The physics state has not been applied yet, and cannot be inspected anyway because this is triggered from the external thread (game thread)
	*	Gives user the ability to call GetProducerInputData_External one last time.
	*	Input data is shared amongst sub-steps. If NumSteps > 1 it means any input data injected will be shared for all sub-steps generated
	*/
	virtual void InjectInputs_External(int32 PhysicsStep, int32 NumSteps){}

	/** Called after sim step to give the option to rewind. Any pending inputs for the next frame will remain in the queue
	*   Return the PhysicsStep to start resimulating from. Resim will run up until latest step passed into RecordInputs (i.e. latest physics sim simulated so far)
	*   Return INDEX_NONE to indicate no rewind
	*/
	virtual int32 TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted) { return INDEX_NONE; }

	/** Called before each rewind step. This is to give user code the opportunity to trigger other code before each rewind step
	*   Usually to simulate external systems that ran in lock step with the physics sim
	*/
	virtual void PreResimStep_Internal(int32 PhysicsStep, bool bFirstStep){}

	/** Called after each rewind step. This is to give user code the opportunity to trigger other code after each rewind step
	*   Usually to simulate external systems that ran in lock step with the physics sim
	*/
	virtual void PostResimStep_Internal(int32 PhysicsStep){}

	virtual void RegisterRewindableSimCallback_Internal(ISimCallbackObject* Callback) { ensure(false); }

	/** Called When resim is finished with debug information about the resim */
	virtual void SetResimDebugInfo_Internal(const FResimDebugInfo& ResimDebugInfo){}
};
}
