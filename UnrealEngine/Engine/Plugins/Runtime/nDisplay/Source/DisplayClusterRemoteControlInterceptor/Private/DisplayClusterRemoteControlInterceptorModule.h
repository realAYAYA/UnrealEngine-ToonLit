// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/UniquePtr.h"

class IRemoteControlInterceptionFeatureInterceptor;


/**
 * Remote control interceptor module
 */
class FDisplayClusterRemoteControlInterceptorModule :
	public IModuleInterface
{
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IModuleInterface
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// RC Interceptor feature instance
	TUniquePtr<IRemoteControlInterceptionFeatureInterceptor> Interceptor;
};
