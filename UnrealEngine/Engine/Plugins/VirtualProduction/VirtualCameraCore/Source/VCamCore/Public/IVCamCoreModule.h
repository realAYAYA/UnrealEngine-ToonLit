// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace UE::VCamCore
{
	class VCAMCORE_API IVCamCoreModule : public IModuleInterface
	{
	public:

		static IVCamCoreModule& Get()
		{
			return FModuleManager::Get().GetModuleChecked<IVCamCoreModule>("VCamCore");
		}
	};
}