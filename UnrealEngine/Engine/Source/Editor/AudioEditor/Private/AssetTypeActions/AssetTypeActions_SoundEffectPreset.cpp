// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundEffectPreset.h"

#include "Audio/AudioWidgetSubsystem.h"
#include "Audio/SoundEffectPresetWidgetInterface.h"
#include "Blueprint/UserWidget.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editors/SoundEffectPresetEditor.h"
#include "Engine/Engine.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

class UWorld;

#define LOCTEXT_NAMESPACE "AssetTypeActions"


namespace EffectPresets
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetEffectSubMenu", "Effects"))
	};
} // namespace EffectPresets

FAssetTypeActions_SoundEffectPreset::FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset)
	: EffectPreset(InEffectPreset)
{
}

FText FAssetTypeActions_SoundEffectPreset::GetName() const
{
	FText AssetActionName = EffectPreset->GetAssetActionName();
	if (AssetActionName.IsEmpty())
	{
		FString ClassName;
		EffectPreset->GetClass()->GetName(ClassName);
		ensureMsgf(false, TEXT("U%sGetAssetActionName not implemented. Please check that EFFECT_PRESET_METHODS(EffectClassName) is at the top of the declaration of %s."), *ClassName, *ClassName);
		FString DefaultName = ClassName + FString(TEXT(" (Error: EFFECT_PRESET_METHODS() Not Used in Class Declaration)"));
		return FText::FromString(DefaultName);
	}
	else
	{
		return EffectPreset->GetAssetActionName();
	}
}

UClass* FAssetTypeActions_SoundEffectPreset::GetSupportedClass() const
{
	UClass* SupportedClass = EffectPreset->GetSupportedClass();
	if (SupportedClass == nullptr)
	{
		FString ClassName;
		EffectPreset->GetClass()->GetName(ClassName);
		ensureMsgf(false, TEXT("U%s::GetSupportedClass not implemented. Please check that EFFECT_PRESET_METHODS(EffectClassName) is at the top of the declaration of %s."), *ClassName, *ClassName);
		return EffectPreset->GetClass();
	}
	else
	{
		return SupportedClass;
	}
}

const TArray<FText>& FAssetTypeActions_SoundEffectSubmixPreset::GetSubMenus() const
{
	return EffectPresets::SubMenus;
}

const TArray<FText>& FAssetTypeActions_SoundEffectPreset::GetSubMenus() const
{
	return EffectPresets::SubMenus;
}

const TArray<FText>& FAssetTypeActions_SoundEffectSourcePresetChain::GetSubMenus() const
{
	return EffectPresets::SubMenus;
}

const TArray<FText>& FAssetTypeActions_SoundEffectSourcePreset::GetSubMenus() const
{
	return EffectPresets::SubMenus;
}

void FAssetTypeActions_SoundEffectPreset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost)
{
	EToolkitMode::Type Mode = ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	TArray<UObject*> Objects = InObjects;
	if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
	{
		if (UAudioWidgetSubsystem* WidgetSubsystem = GEngine ? GEngine->GetEngineSubsystem<UAudioWidgetSubsystem>() : nullptr)
		{
			for (UObject* Object : InObjects)
			{
				USoundEffectPreset* Preset = Cast<USoundEffectPreset>(Object);
				if (!Preset)
				{
					continue;
				}

				auto FilterFunction = [InPresetClass = Object->GetClass()](UUserWidget* UserWidget)
				{
					TSubclassOf<USoundEffectPreset> PresetClass = ISoundEffectPresetWidgetInterface::Execute_GetClass(UserWidget);
					while (PresetClass)
					{
						if (PresetClass == InPresetClass)
						{
							return true;
						}

						PresetClass = PresetClass->GetSuperClass();
					}

					return false;
				};

				TArray<UUserWidget*> UserWidgets = WidgetSubsystem->CreateUserWidgets(*World, USoundEffectPresetWidgetInterface::StaticClass(), FilterFunction);
				if (!UserWidgets.IsEmpty())
				{
					TSharedRef<FSoundEffectPresetEditor> PresetEditor = MakeShared<FSoundEffectPresetEditor>();
					PresetEditor->Init(Mode, ToolkitHost, Preset, UserWidgets);
					Objects.Remove(Object);
				}
			}
		}
	}

	if (!Objects.IsEmpty())
	{
		FAssetTypeActions_Base::OpenAssetEditor(Objects, ToolkitHost);
	}
}
#undef LOCTEXT_NAMESPACE
