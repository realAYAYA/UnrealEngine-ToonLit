// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


namespace UE::Online {

class IOnlineComponent
{
public:
	virtual ~IOnlineComponent() {}
	// Called after component has been constructed. It is not safe to reference other components at this time
	virtual void Initialize() = 0;
	// Called after all components have been initialized
	virtual void PostInitialize() = 0;
	// Called whenever we need to reload data from config
	virtual void UpdateConfig() = 0;
	// Called every Tick
	virtual void Tick(float DeltaSeconds) = 0;
	// Called before any component has been shutdown
	virtual void PreShutdown() = 0;
	// Called right before the component is destroyed. It is not safe to reference any other components at this time
	virtual void Shutdown() = 0;
};

/* UE::Online */ }

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Online/OnlineAsyncOp.h"
#include "Templates/SharedPointer.h"
#endif
