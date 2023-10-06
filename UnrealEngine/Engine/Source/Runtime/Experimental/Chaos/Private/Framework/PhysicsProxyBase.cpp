// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/Framework/PhysicsSolverBase.h"

IPhysicsProxyBase::~IPhysicsProxyBase()
{
	if(GetSolver<Chaos::FPhysicsSolverBase>())
	{
		GetSolver<Chaos::FPhysicsSolverBase>()->RemoveDirtyProxy(this);
	}
}

int32 IPhysicsProxyBase::GetSolverSyncTimestamp_External() const
{
	if (Chaos::FPhysicsSolverBase* SolverBase = GetSolverBase())
	{
		return SolverBase->GetMarshallingManager().GetExternalTimestamp_External();
	}

	return INDEX_NONE;
}