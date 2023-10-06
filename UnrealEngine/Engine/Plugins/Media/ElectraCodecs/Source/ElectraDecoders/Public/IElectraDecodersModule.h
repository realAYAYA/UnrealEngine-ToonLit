// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "IElectraCodecRegistry.h"

/**
 * Interface for the `ElectraDecodersModule` module.
 */
class IElectraDecodersModule : public IModuleInterface
{
public:
	virtual ~IElectraDecodersModule() = default;

	virtual void RegisterDecodersWithCodecFactory(IElectraCodecRegistry* InCodecFactoryToRegisterWith) = 0;
};
