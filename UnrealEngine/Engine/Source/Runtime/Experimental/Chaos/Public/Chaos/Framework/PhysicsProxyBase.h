// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Declares.h"
#include "UObject/GCObject.h"
#include "Chaos/Core.h"

enum class EPhysicsProxyType : uint32
{
	NoneType = 0,
	StaticMeshType = 1,
	GeometryCollectionType = 2,
	FieldType = 3,
	SkeletalMeshType = 4,
	JointConstraintType = 8,	//left gap when removed some types in case these numbers actually matter to someone, should remove
	SuspensionConstraintType = 9,
	SingleParticleProxy,
	Count
};

namespace Chaos
{
	class FPhysicsSolverBase;
}

struct CHAOS_API FProxyTimestampBase
{
	bool bDeleted = false;
};

template <typename TPropertyType>
struct TTimestampProperty
{
	FORCEINLINE_DEBUGGABLE void Set(int32 InTimestamp, const TPropertyType& InValue)
	{
		Value = InValue;
		Timestamp = InTimestamp;
	}
	
	TPropertyType Value;
	int32 Timestamp = INDEX_NONE;
};

struct CHAOS_API FSingleParticleProxyTimestamp: public FProxyTimestampBase
{
	int32 ObjectStateTimestamp = INDEX_NONE;
	TTimestampProperty<Chaos::FVec3> OverWriteX;
	TTimestampProperty<Chaos::FRotation3> OverWriteR;
	TTimestampProperty<Chaos::FVec3> OverWriteV;
	TTimestampProperty<Chaos::FVec3> OverWriteW;
};

struct CHAOS_API FGeometryCollectionProxyTimestamp: public FProxyTimestampBase
{
	// nothing to add as Geometry collections are driven from the Physics thread only
	// ( including kinematic targeting )
};


class CHAOS_API IPhysicsProxyBase
{
public:
	IPhysicsProxyBase(EPhysicsProxyType InType, UObject* InOwner, TSharedPtr<FProxyTimestampBase,ESPMode::ThreadSafe> InProxyTimeStamp)
		: Solver(nullptr)
		, Owner(InOwner)
		, DirtyIdx(INDEX_NONE)
		, SyncTimestamp(InProxyTimeStamp)
		, Type(InType)
	{}

	UObject* GetOwner() const { return Owner; }

	template< class SOLVER_TYPE>
	SOLVER_TYPE* GetSolver() const { return static_cast<SOLVER_TYPE*>(Solver); }

	Chaos::FPhysicsSolverBase* GetSolverBase() const { return Solver; }

	//Should this be in the public API? probably not
	template< class SOLVER_TYPE = Chaos::FPhysicsSolver>
	void SetSolver(SOLVER_TYPE* InSolver) { Solver = InSolver; }

	EPhysicsProxyType GetType() const { return Type; }

	//todo: remove this
	virtual void* GetHandleUnsafe() const { check(false); return nullptr; }

	int32 GetDirtyIdx() const { return DirtyIdx; }
	void SetDirtyIdx(const int32 Idx) { DirtyIdx = Idx; }
	void ResetDirtyIdx() { DirtyIdx = INDEX_NONE; }

	void MarkDeleted() { SyncTimestamp->bDeleted = true; }

	TSharedPtr<FProxyTimestampBase,ESPMode::ThreadSafe> GetSyncTimestamp() const { return SyncTimestamp; }

	bool IsInitialized() const { return InitializedOnStep != INDEX_NONE; }
	void SetInitialized(const int32 InitializeStep)
	{
		//If changed initialization, ignore the very first initialization push data
		if(InitializedOnStep != InitializeStep && InitializedOnStep != INDEX_NONE)
		{
			IgnoreDataOnStep_Internal = InitializedOnStep;
		}

		InitializedOnStep = InitializeStep;
	}
	int32 GetInitializedStep() const { return InitializedOnStep; }

	int32 GetIgnoreDataOnStep_Internal() const { return IgnoreDataOnStep_Internal; }

protected:
	// Ensures that derived classes can successfully call this destructor
	// but no one can delete using a IPhysicsProxyBase*
	virtual ~IPhysicsProxyBase();

	template<typename TProxyTimeStamp>
	FORCEINLINE_DEBUGGABLE TProxyTimeStamp& GetSyncTimestampAs()
	{
		return static_cast<TProxyTimeStamp&>(*GetSyncTimestamp());
	}
	
	/** The solver that owns the solver object */
	Chaos::FPhysicsSolverBase* Solver;
	UObject* Owner;

private:
	int32 DirtyIdx;
	TSharedPtr<FProxyTimestampBase,ESPMode::ThreadSafe> SyncTimestamp;
protected:
	/** Proxy type */
	EPhysicsProxyType Type;
	int32 InitializedOnStep = INDEX_NONE;
	int32 IgnoreDataOnStep_Internal = INDEX_NONE;
};

struct PhysicsProxyWrapper
{
	IPhysicsProxyBase* PhysicsProxy;
	EPhysicsProxyType Type;
};

// Data ise used by interpolation code
struct FProxyInterpolationData
{
public:
	FProxyInterpolationData()
		: PullDataInterpIdx_External(INDEX_NONE)
		, InterpChannel_External(0)
		, bResimSmoothing(false)
	{}

	int32 GetPullDataInterpIdx_External() const { return PullDataInterpIdx_External; }
	void SetPullDataInterpIdx_External(const int32 Idx) { PullDataInterpIdx_External = Idx;	}

	int32 GetInterpChannel_External() const { return InterpChannel_External; }
	void SetInterpChannel_External(int32 Channel) { InterpChannel_External = Channel; }

	void SetResimSmoothing(bool ResimSmoothing) { bResimSmoothing = ResimSmoothing; }
	bool IsResimSmoothing() const { return bResimSmoothing; }
	
private:
	int32 PullDataInterpIdx_External;
	int32 InterpChannel_External;
	bool bResimSmoothing;
};