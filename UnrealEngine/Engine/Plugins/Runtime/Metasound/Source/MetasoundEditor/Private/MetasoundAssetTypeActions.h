// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetTypeActions_Base.h"


// Forward Declarations
class IToolkitHost;

namespace Metasound
{
	namespace Editor
	{
		class FAssetTypeActions_MetaSoundPatch : public FAssetTypeActions_Base
		{
		public:
			// IAssetTypeActions Implementation
			virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MetaSoundPatch", "MetaSound Patch"); }
			virtual FColor GetTypeColor() const override;
			virtual UClass* GetSupportedClass() const override;
			virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
			virtual const TArray<FText>& GetSubMenus() const override;
			virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost) override;

			virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;
			virtual const FSlateBrush* GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const override;

			static void RegisterMenuActions();
		};

		class FAssetTypeActions_MetaSoundSource : public FAssetTypeActions_Base
		{
		public:
			// IAssetTypeActions Implementation
			virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MetaSoundSource", "MetaSound Source"); }
			virtual FColor GetTypeColor() const override;
			virtual UClass* GetSupportedClass() const override;
			virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
			virtual const TArray<FText>& GetSubMenus() const override;
			virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost) override;

			virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;
			virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
			virtual const FSlateBrush* GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const override;

			static void RegisterMenuActions();
		};
	} // namespace Editor
} // namespace Metasound
