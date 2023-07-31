// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/RemoteControlPresetActions.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "RemoteControlPreset.h"
#include "AssetEditor/RemoteControlPresetEditorToolkit.h"
#include "IRemoteControlUIModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FRemoteControlPresetActions::FRemoteControlPresetActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }

uint32 FRemoteControlPresetActions::GetCategories()
{
	return IRemoteControlUIModule::Get().GetRemoteControlAssetCategory();
}

FText FRemoteControlPresetActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_RemoteControlPreset", "Remote Control Preset");
}

UClass* FRemoteControlPresetActions::GetSupportedClass() const
{
	return URemoteControlPreset::StaticClass();
}

FColor FRemoteControlPresetActions::GetTypeColor() const
{
	return FColor(200, 80, 80);
}

void FRemoteControlPresetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Obj : InObjects)
	{
		if (URemoteControlPreset* Asset = Cast<URemoteControlPreset>(Obj))
		{
			FRemoteControlPresetEditorToolkit::CreateEditor(EditWithinLevelEditor ? EToolkitMode::WorldCentric : EToolkitMode::Standalone,  EditWithinLevelEditor, Asset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
