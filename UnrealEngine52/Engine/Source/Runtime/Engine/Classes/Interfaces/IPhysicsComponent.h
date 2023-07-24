// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/PhysicsObject.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "IPhysicsComponent.generated.h"

/** Interface for components that contains physics bodies. **/
UINTERFACE(MinimalApi, Experimental, meta = (CannotImplementInterfaceInBlueprint))
class UPhysicsComponent : public UInterface
{
	GENERATED_BODY()
};

class IPhysicsComponent
{
	GENERATED_BODY()

public:
	virtual Chaos::FPhysicsObject* GetPhysicsObjectById(int32 Id) const = 0;
	virtual Chaos::FPhysicsObject* GetPhysicsObjectByName(const FName& Name) const = 0;
	virtual TArray<Chaos::FPhysicsObject*> GetAllPhysicsObjects() const = 0;
};