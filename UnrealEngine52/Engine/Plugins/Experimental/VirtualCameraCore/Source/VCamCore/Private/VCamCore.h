// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IVCamCoreModule.h"
#include "Modules/ModuleManager.h"

namespace UE::VCamCore::WidgetSnapshotUtils::Private
{
	struct FWidgetSnapshotSettings;
}

namespace UE::VCamCore::Private
{
	class FVCamCoreModule : public IVCamCoreModule
	{
	public:

		static FVCamCoreModule& Get()
		{
			return FModuleManager::Get().GetModuleChecked<FVCamCoreModule>("VCamCore");
		}

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		WidgetSnapshotUtils::Private::FWidgetSnapshotSettings GetSnapshotSettings() const;
	};
}

