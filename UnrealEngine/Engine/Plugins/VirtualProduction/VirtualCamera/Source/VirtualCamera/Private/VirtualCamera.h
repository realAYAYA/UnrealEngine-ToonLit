// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVirtualCamera, Log, Log);

namespace UE::VirtualCamera
{
	class FVirtualCameraModuleImpl : public IModuleInterface
	{
	public:

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

	private:

		void RegisterSettings();
		void UnregisterSettings();
	};
}
