// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsObjectPhysicsCoreInterface.h"

#include "Chaos/PhysicsObjectInterface.h"
#include "PBDRigidsSolver.h"

FChaosScene* PhysicsObjectPhysicsCoreInterface::GetScene(TArrayView<const Chaos::FConstPhysicsObjectHandle> InObjects)
{
	Chaos::FPBDRigidsSolver* Solver = Chaos::FPhysicsObjectInterface::GetSolver(InObjects);
	if (!Solver)
	{
		return nullptr;
	}
	return reinterpret_cast<FChaosScene*>(Solver->PhysSceneHack);
}

FChaosScene* PhysicsObjectPhysicsCoreInterface::GetScene(const Chaos::FConstPhysicsObjectHandle InObject)
{
	Chaos::FPBDRigidsSolver* Solver = Chaos::FPhysicsObjectInterface::GetSolver(InObject);
	if (!Solver)
	{
		return nullptr;
	}
	return reinterpret_cast<FChaosScene*>(Solver->PhysSceneHack);
}
