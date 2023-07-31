// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetTypeActions_Base.h"

namespace WaveTable
{
	namespace Editor
	{
		class FAssetTypeActions_WaveTableBank : public FAssetTypeActions_Base
		{
		public:
			// IAssetTypeActions Implementation
			virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_WaveTableBank", "WaveTable Bank"); }
			virtual FColor GetTypeColor() const override { return FColor(232, 122, 0, 255); }
			virtual const TArray<FText>& GetSubMenus() const override;
			virtual UClass* GetSupportedClass() const override;
			virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }

			virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost) override;
		};
	} // namespace Editor
} // namespace WaveTable
