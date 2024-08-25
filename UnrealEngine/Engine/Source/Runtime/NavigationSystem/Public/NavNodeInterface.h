// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "AI/Navigation/NavigationTypes.h"
#endif
#include "NavNodeInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavNodeInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavNodeInterface
{
	GENERATED_IINTERFACE_BODY()


	/**
	 *	Retrieves pointer to implementation's UNavigationGraphNodeComponent
	 */
	virtual class UNavigationGraphNodeComponent* GetNavNodeComponent() PURE_VIRTUAL(FNavAgentProperties::GetNavNodeComponent,return NULL;);

};
