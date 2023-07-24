// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Chaos/PhysicsObject.h"

class FChaosScene;

class PHYSICSCORE_API PhysicsObjectPhysicsCoreInterface
{
public:
	static FChaosScene* GetScene(TArrayView<Chaos::FPhysicsObjectHandle> InObjects);
};