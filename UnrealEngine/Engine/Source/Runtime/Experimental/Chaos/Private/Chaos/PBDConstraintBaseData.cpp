// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDConstraintBaseData.h"

namespace Chaos
{
	FConstraintBase::FConstraintBase(EConstraintType InType)
		: Type(InType)
		, Proxy(nullptr)
	{
	}

	bool FConstraintBase::IsValid() const
	{
		return Proxy != nullptr;
	}

	void FConstraintBase::SetProxy(IPhysicsProxyBase* InProxy)
	{
		Proxy = InProxy;
		if (Proxy)
		{
			if (FPhysicsSolverBase* PhysicsSolverBase = Proxy->GetSolver<FPhysicsSolverBase>())
			{
				PhysicsSolverBase->AddDirtyProxy(Proxy);
			}
		}
	}

} // Chaos

