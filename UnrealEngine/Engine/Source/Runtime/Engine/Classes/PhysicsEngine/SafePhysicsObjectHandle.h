// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/PhysicsObject.h"
#include "Interfaces/IPhysicsComponent.h"
#include "UObject/WeakInterfacePtr.h"

struct FSafePhysicsObjectHandle
{
	TWeakInterfacePtr<IPhysicsComponent> Component;
	Chaos::FPhysicsObjectId BoneId;

	Chaos::FPhysicsObjectHandle Get() const
	{
		if (IPhysicsComponent* ComponentPtr = Component.Get())
		{
			return ComponentPtr->GetPhysicsObjectById(BoneId);
		}

		return nullptr;
	}
};
