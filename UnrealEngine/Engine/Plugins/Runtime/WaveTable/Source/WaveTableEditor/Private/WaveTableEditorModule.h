// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetTypeActions_Base.h"
#include "Containers/Array.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWaveTableEditor, Log, All);

namespace WaveTable
{
	namespace Editor
	{
		class FModule : public IModuleInterface
		{
		public:
			FModule() = default;

			virtual void StartupModule() override;

			virtual void ShutdownModule() override;

		private:
			TArray<TSharedPtr<FAssetTypeActions_Base>> AssetActions;
		};
	} // namespace Editor
} // namespace WaveTable
