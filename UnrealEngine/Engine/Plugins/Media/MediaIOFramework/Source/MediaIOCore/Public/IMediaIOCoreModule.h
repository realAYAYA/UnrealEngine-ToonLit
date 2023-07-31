// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


class IMediaIOCoreDeviceProvider;


/**
 * Definition the MediaIOCore module.
 */
class MEDIAIOCORE_API IMediaIOCoreModule : public IModuleInterface
{
public:
	static bool IsAvailable();
	static IMediaIOCoreModule& Get();

public:
	virtual void RegisterDeviceProvider(IMediaIOCoreDeviceProvider* InProvider) = 0;
	virtual void UnregisterDeviceProvider(IMediaIOCoreDeviceProvider* InProvider) = 0;
	virtual IMediaIOCoreDeviceProvider* GetDeviceProvider(FName InProviderName) = 0;
	virtual TConstArrayView<IMediaIOCoreDeviceProvider*> GetDeviceProviders() const = 0;
};
