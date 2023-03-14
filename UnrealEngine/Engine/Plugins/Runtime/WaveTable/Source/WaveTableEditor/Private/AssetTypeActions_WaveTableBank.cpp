// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_WaveTableBank.h"

#include "Templates/SharedPointer.h"
#include "WaveTableBank.h"
#include "WaveTableBankEditor.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace WaveTable
{
	namespace Editor
	{
		UClass* FAssetTypeActions_WaveTableBank::GetSupportedClass() const
		{
			return UWaveTableBank::StaticClass();
		}

		const TArray<FText>& FAssetTypeActions_WaveTableBank::GetSubMenus() const
		{
			static const TArray<FText> SubMenus
			{
				LOCTEXT("AssetWaveTableSubMenu", "WaveTable")
			};

			return SubMenus;
		}

		void FAssetTypeActions_WaveTableBank::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost)
		{
			EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

			for (UObject* Object : InObjects)
			{
				if (UWaveTableBank* Patch = Cast<UWaveTableBank>(Object))
				{
					TSharedRef<FBankEditor> Editor = MakeShared<FBankEditor>();
					Editor->Init(Mode, ToolkitHost, Patch);
				}
			}
		}
	} // namespace Editor
} // namespace WaveTable
#undef LOCTEXT_NAMESPACE
