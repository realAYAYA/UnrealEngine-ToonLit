// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavPathObserverInterface.h"
#include "AI/Navigation/NavEdgeProviderInterface.h"
#include "AI/RVOAvoidanceInterface.h"
#include "AI/Navigation/NavigationDataInterface.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"

URVOAvoidanceInterface::URVOAvoidanceInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavAgentInterface::UNavAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavRelevantInterface::UNavRelevantInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavEdgeProviderInterface::UNavEdgeProviderInterface(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
}

UNavPathObserverInterface::UNavPathObserverInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavigationDataInterface::UNavigationDataInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPathFollowingAgentInterface::UPathFollowingAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
