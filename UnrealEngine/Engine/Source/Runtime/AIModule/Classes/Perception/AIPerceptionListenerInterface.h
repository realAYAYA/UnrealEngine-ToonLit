// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AIPerceptionListenerInterface.generated.h"

class UAIPerceptionComponent;

UINTERFACE(MinimalAPI)
class UAIPerceptionListenerInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IAIPerceptionListenerInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual UAIPerceptionComponent* GetPerceptionComponent() { return nullptr; }
};

