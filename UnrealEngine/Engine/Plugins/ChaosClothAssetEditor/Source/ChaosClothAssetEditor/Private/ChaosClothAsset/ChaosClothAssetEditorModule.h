// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModule.h"

namespace UE::Chaos::ClothAsset
{
	class FChaosClothAssetEditorModule final : public FBaseCharacterFXEditorModule
	{
	public:
		/** IModuleInterface implementation */
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
	};
} // namespace UE::Chaos::ClothAsset
