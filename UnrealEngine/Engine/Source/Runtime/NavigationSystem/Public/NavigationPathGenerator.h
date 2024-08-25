// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavigationPathGenerator.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavigationPathGenerator : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavigationPathGenerator
{
	GENERATED_IINTERFACE_BODY()


	/** 
	 *	Retrieved path generated for specified navigation Agent
	 */
	virtual FNavPathSharedPtr GetGeneratedPath(class INavAgentInterface* Agent) PURE_VIRTUAL(INavigationPathGenerator::GetGeneratedPath, return FNavPathSharedPtr(NULL););
};
