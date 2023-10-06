// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Chaos/Core.h"
#include "Chaos/Transform.h"
#include "Chaos/Vector.h"

#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Properties.h"

namespace Chaos
{

enum class EConstraintType
{
	NoneType = 0,
	JointConstraintType = 1,
	SpringConstraintType = 2,
	SuspensionConstraintType = 3,
	CharacterGroundConstraintType = 4,
};

#define CONSTRAINT_JOINT_PROPERPETY_IMPL(TYPE, FNAME, ENAME, VNAME)\
	void Set##FNAME(TYPE InValue){ if (InValue != VNAME){VNAME = InValue;MDirtyFlags.MarkDirty(ENAME);SetProxy(Proxy);}}\
	TYPE Get##FNAME() const{return VNAME;}\

#define CONSTRAINT_JOINT_PROPERPETY_IMPL2(TYPE, FNAME, PROP, VNAME)\
	void Set##FNAME(TYPE InValue){ PROP.Modify(/*bInvalidate=*/true, DirtyFlags, Proxy, [&InValue](auto& Data) { Data.VNAME = InValue; }); }\
	TYPE Get##FNAME() const{return PROP.Read().VNAME;}\

class CHAOS_API FConstraintBase
{
public:
	virtual ~FConstraintBase() {}

	typedef TVector<TGeometryParticleHandle<FReal, 3>*, 2> FParticleHandlePair;

	FConstraintBase(EConstraintType InType);

	EConstraintType GetType() const { return Type; }
	bool IsType(EConstraintType InType) { return (Type == InType); }
	bool IsValid() const;

	bool IsDirty() const { return DirtyFlags.IsDirty(); }
	bool IsDirty(const EChaosPropertyFlags CheckBits) const { return DirtyFlags.IsDirty(CheckBits); }
	void ClearDirtyFlags() { DirtyFlags.Clear(); }
	const FDirtyChaosPropertyFlags& GetDirtyFlags() const { return DirtyFlags; }

	//template <typename CastType>
	//static CastType CastTo<CastType>(EConstraintType InType) { (InType == Type) ? static_cast<CastType*> : nullptr; }

	template<typename T = IPhysicsProxyBase> T* GetProxy() { return static_cast<T*>(Proxy); }

	void SetProxy(IPhysicsProxyBase* InProxy);

	void SyncRemoteData(FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData)
	{
		RemoteData.SetFlags(DirtyFlags);
		SyncRemoteDataImp(Manager, DataIdx, RemoteData);
		ClearDirtyFlags();
	}

protected:
	EConstraintType Type;
	class IPhysicsProxyBase* Proxy;
	FDirtyChaosPropertyFlags DirtyFlags;

	virtual void SyncRemoteDataImp(FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData) = 0;
};

} // Chaos

