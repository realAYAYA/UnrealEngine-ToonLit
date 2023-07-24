// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundSubmix.h"

#include "AudioEditorModule.h"
#include "Containers/Set.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "IAudioEndpoint.h"
#include "ISoundfieldEndpoint.h"
#include "ISoundfieldFormat.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundSubmix.h"
#include "SoundSubmixEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"

class IToolkitHost;
class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundSubmix::GetSupportedClass() const
{
	return USoundSubmix::StaticClass();
}

void FAssetTypeActions_SoundSubmix::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Obj : InObjects)
	{
		if (USoundSubmixBase* SoundSubmix = Cast<USoundSubmixBase>(Obj))
		{
			IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
			AudioEditorModule->CreateSoundSubmixEditor(Mode, EditWithinLevelEditor, SoundSubmix);
		}
	}
}

bool FAssetTypeActions_SoundSubmix::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	TSet<USoundSubmixBase*> SubmixesToSelect;
	IAssetEditorInstance* Editor = nullptr;
	for (UObject* Obj : InObjects)
	{
		if (USoundSubmixBase* SubmixToSelect = Cast<USoundSubmixBase>(Obj))
		{
			if (!Editor)
			{
				Editor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Obj, true);
			}
			SubmixesToSelect.Add(SubmixToSelect);
		}
	}

	if (Editor)
	{
		static_cast<FSoundSubmixEditor*>(Editor)->SelectSubmixes(SubmixesToSelect);
		return true;
	}

	return false;
}

const TArray<FText>& FAssetTypeActions_SoundSubmix::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundMixSubMenu", "Mix")
	};

	return SubMenus;
}

UClass* FAssetTypeActions_SoundfieldSubmix::GetSupportedClass() const
{
	return USoundfieldSubmix::StaticClass();
}

UClass* FAssetTypeActions_EndpointSubmix::GetSupportedClass() const
{
	return UEndpointSubmix::StaticClass();
}

UClass* FAssetTypeActions_SoundfieldEndpointSubmix::GetSupportedClass() const
{
	return USoundfieldEndpointSubmix::StaticClass();
}

UClass* FAssetTypeActions_SoundfieldEncodingSettings::GetSupportedClass() const
{
	return USoundfieldEncodingSettingsBase::StaticClass();
}

UClass* FAssetTypeActions_SoundfieldEffectSettings::GetSupportedClass() const
{
	return USoundfieldEncodingSettingsBase::StaticClass();
}

UClass* FAssetTypeActions_SoundfieldEffect::GetSupportedClass() const
{
	return USoundfieldEffectBase::StaticClass();
}

UClass* FAssetTypeActions_AudioEndpointSettings::GetSupportedClass() const
{
	return UAudioEndpointSettingsBase::StaticClass();
}

UClass* FAssetTypeActions_SoundfieldEndpointSettings::GetSupportedClass() const
{
	return USoundfieldEndpointSettingsBase::StaticClass();
}

#undef LOCTEXT_NAMESPACE
