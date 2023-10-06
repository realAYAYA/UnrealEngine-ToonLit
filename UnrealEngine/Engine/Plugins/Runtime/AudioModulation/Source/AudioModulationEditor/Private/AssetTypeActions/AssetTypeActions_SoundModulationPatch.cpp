// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_SoundModulationPatch.h"

#include "Editors/ModulationPatchEditor.h"
#include "SoundModulationPatch.h"
#include "Templates/SharedPointer.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundModulationPatch::GetSupportedClass() const
{
	return USoundModulationPatch::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundModulationPatch::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundModulationSubMenu", "Modulation")
	};

	return SubMenus;
}

void FAssetTypeActions_SoundModulationPatch::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost)
{
	EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (USoundModulationPatch* Patch = Cast<USoundModulationPatch>(Object))
		{
			TSharedRef<FModulationPatchEditor> PatchEditor = MakeShared<FModulationPatchEditor>();
			PatchEditor->Init(Mode, ToolkitHost, Patch);
		}
	}
}

#undef LOCTEXT_NAMESPACE
