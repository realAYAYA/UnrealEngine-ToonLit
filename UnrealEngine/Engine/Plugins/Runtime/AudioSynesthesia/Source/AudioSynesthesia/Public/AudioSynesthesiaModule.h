// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "LoudnessNRTFactory.h"
#include "ConstantQNRTFactory.h"
#include "OnsetNRTFactory.h"

namespace Audio
{
	class AUDIOSYNESTHESIA_API FAudioSynesthesiaModule : public IModuleInterface
	{
	public:

		// IModuleInterface interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		FLoudnessNRTFactory LoudnessFactory;
		FConstantQNRTFactory ConstantQFactory;
		FOnsetNRTFactory OnsetFactory;
	};
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#endif
