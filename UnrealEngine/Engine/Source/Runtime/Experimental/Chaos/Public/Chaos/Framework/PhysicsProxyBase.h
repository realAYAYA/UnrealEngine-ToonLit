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
	CharacterGroundConstraintType = 10,
	SingleParticleProxy,
	ClusterUnionProxy,
	Count
};

namespace Chaos
{
	class FPhysicsSolverBase;
}

struct FProxyTimestampBase
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

struct FSingleParticleProxyTimestamp: public FProxyTimestampBase
{
	int32 ObjectStateTimestamp = INDEX_NONE;
	TTimestampProperty<Chaos::FVec3> OverWriteX;
	TTimestampProperty<Chaos::FRotation3> OverWriteR;
	TTimestampProperty<Chaos::FVec3> OverWriteV;
	TTimestampProperty<Chaos::FVec3> OverWriteW;
};

struct FGeometryCollectionProxyTimestamp: public FProxyTimestampBase
{
	// nothing to add as Geometry collections are driven from the Physics thread only
	// ( including kinematic targeting )
};

struct FClusterUnionProxyTimestamp : public FProxyTimestampBase
{
	TTimestampProperty<Chaos::FVec3> OverWriteX;
	TTimestampProperty<Chaos::FRotation3> OverWriteR;
	TTimestampProperty<Chaos::FVec3> OverWriteV;
	TTimestampProperty<Chaos::FVec3> OverWriteW;
};

class IPhysicsProxyBase
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
	bool GetMarkedDeleted() const { return SyncTimestamp->bDeleted; }

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

	IPhysicsProxyBase* GetParentProxy() const { return ParentProxy; }
	void SetParentProxy(IPhysicsProxyBase* InProxy) { ParentProxy = InProxy; }

protected:
	// Ensures that derived classes can successfully call this destructor
	// but no one can delete using a IPhysicsProxyBase*
	CHAOS_API virtual ~IPhysicsProxyBase();

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
	IPhysicsProxyBase* ParentProxy = nullptr;
protected:
	/** Proxy type */
	EPhysicsProxyType Type;
	int32 InitializedOnStep = INDEX_NONE;
	int32 IgnoreDataOnStep_Internal = INDEX_NONE;

	CHAOS_API int32 GetSolverSyncTimestamp_External() const;
};

struct PhysicsProxyWrapper
{
	IPhysicsProxyBase* PhysicsProxy;
	EPhysicsProxyType Type;
};


struct FProxyInterpolationBase
{
	FProxyInterpolationBase()
		: PullDataInterpIdx_External(INDEX_NONE)
		, InterpChannel_External(0)
	{}

	int32 GetPullDataInterpIdx_External() const { return PullDataInterpIdx_External; }
	void SetPullDataInterpIdx_External(const int32 Idx) { PullDataInterpIdx_External = Idx; }

	int32 GetInterpChannel_External() const { return InterpChannel_External; }
	void SetInterpChannel_External(const int32 Channel) { InterpChannel_External = Channel; }

	virtual bool IsErrorSmoothing() { return false; }

protected:
	int32 PullDataInterpIdx_External;
	int32 InterpChannel_External;
};

// Render interpolation that can correct errors from resimulation / repositions through a linear decay over N simulation tick.
struct FProxyInterpolationError : FProxyInterpolationBase
{
	using Super = FProxyInterpolationBase;

	FProxyInterpolationError() : Super()
	{}

	virtual bool IsErrorSmoothing() override { return ErrorSmoothingCount > 0; }

	Chaos::FVec3 GetErrorX(const Chaos::FRealSingle Alpha) { return FMath::Lerp(ErrorXPrev, ErrorX, Alpha); } // Get the ErrorX based on current Alpha between GT and PT
	FQuat GetErrorR(const Chaos::FRealSingle Alpha) { return FMath::Lerp(ErrorRPrev, ErrorR, Alpha); } // Get the ErrorR based on current Alpha between GT and PT

	void AccumlateErrorXR(const Chaos::FVec3 X, const FQuat R, const int32 CurrentSimTick, const int32 ErrorSmoothDuration)
	{
		ErrorX += X;
		ErrorXPrev = ErrorX;
		ErrorR *= R;
		ErrorRPrev = ErrorR;
		ErrorSmoothingCount = ErrorSmoothDuration; // How many simulation ticks to correct error over
		LastSimTick = CurrentSimTick - 1; // Error is from the previous simulation tick, not the current
	}

	virtual bool UpdateError(const int32 CurrentSimTick, const Chaos::FReal AsyncFixedTimeStep)
	{
		// Cache how many simulation ticks have passed since last call
		SimTicks = CurrentSimTick - LastSimTick;
		LastSimTick = CurrentSimTick;

		if (IsErrorSmoothing() && SimTicks > 0)
		{
			DecayError();
			return true;
		}
		return false;
	}


protected:
	void DecayError()
	{
		// Linear decay
		// Example: If we want to decay an error of 100 over 10ticks (i.e. 10% each tick)
		// First step:  9/10 = 0.9   |  100 * 0.9  = 90 error
		// Second step: 8/9  = 0.888 |  90 * 0.888 = 80 error
		// Third step: 7/8  = 0.875 |  80 * 0.875 = 70 error
		// etc.
		Chaos::FRealSingle Alpha = Chaos::FRealSingle(ErrorSmoothingCount - SimTicks) / Chaos::FRealSingle(ErrorSmoothingCount);
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

		ErrorXPrev = ErrorX;
		ErrorX *= Alpha;

		ErrorRPrev = ErrorR;
		ErrorR = FMath::Lerp(FQuat::Identity, ErrorR, Alpha);

		ErrorSmoothingCount = FMath::Max(ErrorSmoothingCount - SimTicks, 0);
	}


protected:
	int32 LastSimTick = 0;
	int32 SimTicks = 0;

	Chaos::FVec3 ErrorX = { 0,0,0 };
	Chaos::FVec3 ErrorXPrev = { 0,0,0 };
	FQuat ErrorR = FQuat::Identity;
	FQuat ErrorRPrev = FQuat::Identity;
	int32 ErrorSmoothingCount = 0;

};


// Take incoming velocity into consideration when performing render interpolation, the correction will be more organic but might result in clipping and it's heavier for memory and CPU.
#ifndef RENDERINTERP_ERRORVELOCITYSMOOTHING
#define RENDERINTERP_ERRORVELOCITYSMOOTHING 0
#endif
// Render Interpolation that both perform the linear error correction from FProxyInterpolationError and takes incoming velocity into account to make a smoother and more organic correction of the error.
struct FProxyInterpolationErrorVelocity : FProxyInterpolationError
{
	using Super = FProxyInterpolationError;

	FProxyInterpolationErrorVelocity() : Super()
	{}

	bool IsErrorVelocitySmoothing() { return ErrorVelocitySmoothingCount > 0; }

	// Returns the Alpha of how much to take previous velocity into account, used to lerp from linear extrapolation to the predicted position based on previous velocity.
	Chaos::FRealSingle GetErrorVelocitySmoothingAlpha(const int32 ErrorVelocitySmoothDuration) { return Chaos::FRealSingle(ErrorVelocitySmoothingCount) / Chaos::FRealSingle(ErrorVelocitySmoothDuration); }
	Chaos::FVec3 GetErrorVelocitySmoothingX(const Chaos::FRealSingle Alpha) { return FMath::Lerp(ErrorVelocitySmoothingXPrev, ErrorVelocitySmoothingX, Alpha); } // Get the VelocityErrorX based on current Alpha between GT and PT

	bool UpdateError(const int32 CurrentSimTick, const Chaos::FReal AsyncFixedTimeStep) override
	{		
		if (Super::UpdateError(CurrentSimTick, AsyncFixedTimeStep))
		{
			StepErrorVelocitySmoothingData(AsyncFixedTimeStep);
			return true;
		}
		return false;
	}

	void SetVelocitySmoothing(const Chaos::FVec3 CurrV, const Chaos::FVec3 CurrX, const int32 ErrorVelocitySmoothDuration)
	{
		// Cache pre error velocity and position to be used when smoothing out error correction
		ErrorVelocitySmoothingV = CurrV;
		ErrorVelocitySmoothingX = CurrX;
		ErrorVelocitySmoothingXPrev = ErrorVelocitySmoothingX;
		ErrorVelocitySmoothingCount = ErrorVelocitySmoothDuration;
	}

protected:
	void StepErrorVelocitySmoothingData(const Chaos::FReal AsyncFixedTimeStep)
	{
		// Step the error velocity smoothing position forward along the previous velocity to have a new position to base smoothing on each tick
		if (IsErrorVelocitySmoothing())
		{
			const Chaos::FReal Time = AsyncFixedTimeStep * SimTicks;
			ErrorVelocitySmoothingXPrev = ErrorVelocitySmoothingX;
			ErrorVelocitySmoothingX += ErrorVelocitySmoothingV * Time;

			ErrorVelocitySmoothingCount = FMath::Max(ErrorVelocitySmoothingCount - SimTicks, 0);
		}
	}

	Chaos::FVec3 ErrorVelocitySmoothingV = { 0,0,0 };
	Chaos::FVec3 ErrorVelocitySmoothingX = { 0,0,0 };
	Chaos::FVec3 ErrorVelocitySmoothingXPrev = { 0,0,0 };
	int32 ErrorVelocitySmoothingCount = 0;
};
