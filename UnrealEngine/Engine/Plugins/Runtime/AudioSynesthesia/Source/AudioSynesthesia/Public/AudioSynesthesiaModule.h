// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
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
