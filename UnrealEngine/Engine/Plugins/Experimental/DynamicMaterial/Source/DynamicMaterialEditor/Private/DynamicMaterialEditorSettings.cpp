// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialEditorSettings.h"
#include "AssetRegistry/AssetData.h"
#include "Components/DMMaterialComponent.h"
#include "Engine/AssetManager.h"
#include "GenericPlatform/GenericApplication.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerSettings"

UDynamicMaterialEditorSettings::UDynamicMaterialEditorSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Material Designer");

	bFollowSelection = true;

	ResetAllLayoutSettings();

	DefaultRGBTexture = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("Texture2D'/DynamicMaterial/T_Default_Texture.T_Default_Texture'")));
	DefaultOpaqueTexture = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("Texture2D'/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture'")));
	DefaultPreviewCanvasTexture = nullptr;
	DefaultOpacitySlotMask = TSoftObjectPtr<UTexture>(FSoftObjectPath(TEXT("/Script/Engine.Texture2D'/DynamicMaterial/Textures/T_DM_HorizontalGradient.T_DM_HorizontalGradient'")));
}

UDynamicMaterialEditorSettings* UDynamicMaterialEditorSettings::Get()
{
	UDynamicMaterialEditorSettings* DefaultSettings = GetMutableDefault<UDynamicMaterialEditorSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}

void UDynamicMaterialEditorSettings::PostInitProperties()
{
	Super::PostInitProperties();

}

void UDynamicMaterialEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SpinBoxValueMultiplier_Default)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SpinBoxValueMultiplier_Shift)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SpinBoxValueMultiplier_AltShift)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SpinBoxValueMultiplier_Cmd)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SpinBoxValueMultiplier_AltCmd)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SplitterLocation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, LayerPreviewSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SlotPreviewSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, DetailsPreviewSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, bShowTooltipPreview)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, TooltipTextureSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, bPreviewImagesUseTextureUVs))
	{
		OnSettingsChanged.Broadcast(InPropertyChangedEvent);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, bPreviewImagesUseTextureUVs))
	{
		for (UDMMaterialComponent* Component : TObjectRange<UDMMaterialComponent>())
		{
			Component->MarkComponentDirty();
		}
	}
}

void UDynamicMaterialEditorSettings::OpenEditorSettingsWindow() const
{
	static ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.ShowViewer(GetContainerName(), CategoryName, SectionName);
}

void UDynamicMaterialEditorSettings::ResetAllLayoutSettings()
{
	SpinBoxValueMultiplier_AltShift = 1.0f; // Normal Sensitivity
	SpinBoxValueMultiplier_Shift = 0.25f;   //   5x Default
	SpinBoxValueMultiplier_Default = 0.05f; //  20x Sensitivity
	SpinBoxValueMultiplier_Cmd = 0.01f;		//  1/5 Default
	SpinBoxValueMultiplier_AltCmd = 0.001f; // 1/50 Default, 1/10 Cmd
	SplitterLocation = 240;
	LayerPreviewSize = 40;
	SlotPreviewSize = 96;
	DetailsPreviewSize = 96;
	bShowTooltipPreview = false;
	TooltipTextureSize = 512;
	MaxFloatSliderWidth = 200.f;
	bPreviewImagesUseTextureUVs = true;
}

float UDynamicMaterialEditorSettings::GetSpinboxValueChangeMultiplier(const FModifierKeysState& InModifierKeys) const
{
	const bool bIsShiftDown = InModifierKeys.IsLeftShiftDown() || InModifierKeys.IsRightShiftDown();
	const bool bIsAltDown = InModifierKeys.IsLeftAltDown() || InModifierKeys.IsRightAltDown();
	const bool bIsCmdDown = InModifierKeys.IsLeftControlDown() || InModifierKeys.IsLeftCommandDown() || InModifierKeys.IsRightControlDown() || InModifierKeys.IsRightCommandDown();
	
	if (InModifierKeys.IsShiftDown())
	{
		return InModifierKeys.IsAltDown() ? SpinBoxValueMultiplier_AltShift : SpinBoxValueMultiplier_Shift;
	}

	if (InModifierKeys.IsCommandDown() || InModifierKeys.IsControlDown())
	{
		return InModifierKeys.IsAltDown() ? SpinBoxValueMultiplier_AltCmd : SpinBoxValueMultiplier_Cmd;
	}

	return SpinBoxValueMultiplier_Default;
}

TArray<FDMMaterialEffectList> UDynamicMaterialEditorSettings::GetEffectList() const
{
	TArray<FDMMaterialEffectList> Effects = {};

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();

	if (!AssetRegistry)
	{
		return Effects;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);

	if (!Plugin.IsValid())
	{
		return Effects;
	}

	const FString PluginEffectPath = Plugin->GetMountedAssetPath() / "MaterialFunctions" / "Effects";

	TArray<FName> AssetPaths = {*PluginEffectPath};
	AssetPaths.Append(UDynamicMaterialEditorSettings::Get()->CustomEffectsFolders);

	TArray<FAssetData> Assets;
	AssetRegistry->GetAssetsByPaths(AssetPaths, Assets, /* bRecursive */ true, /* Only Assets on Disk */ true);

	TArray<FString> AssetPathStrings;
	AssetPathStrings.Reserve(AssetPaths.Num());
	Algo::Transform(AssetPaths, AssetPathStrings, [](const FName& InElement) { return InElement.ToString(); });

	for (FString& AssetPathString : AssetPathStrings)
	{
		if (AssetPathString.EndsWith(TEXT("/")) || AssetPathString.EndsWith(TEXT("\\")))
		{
			AssetPathString = AssetPathString.LeftChop(1);
		}
	}

	auto FindBasePath = [&AssetPathStrings](const FString& InPath)
		{
			for (const FString& AssetPathString : AssetPathStrings)
			{
				if (InPath.StartsWith(AssetPathString))
				{
					return AssetPathString;
				}
			}

			return FString("");
		};

	for (const FAssetData& Asset : Assets)
	{
		UClass* AssetClass = Asset.GetClass(EResolveClass::Yes);

		if (!AssetClass || !AssetClass->IsChildOf(UMaterialFunctionInterface::StaticClass()))
		{
			continue;
		}

		const FString AssetPath = Asset.GetObjectPathString();
		const FString AssetBasePath = FindBasePath(AssetPath);

		FString Path = AssetPath;
		FString Category = "";

		// Reduce from /BasePath/Category/Random/OtherPaths/Asset.Asset to /BasePath and Category
		while (true)
		{
			const FString ParentPath = FPaths::GetPath(Path);

			if (ParentPath == AssetBasePath)
			{
				Category = Path.Mid(AssetBasePath.Len() + 1);
				break;
			}
			else if (ParentPath.IsEmpty())
			{
				break;
			}

			Path = ParentPath;
		}

		if (Category.IsEmpty())
		{
			continue;
		}

		FDMMaterialEffectList* EffectList = Effects.FindByPredicate(
			[Category](const FDMMaterialEffectList& InElement)
			{
				return InElement.Name == Category;
			});

		if (!EffectList)
		{
			Effects.Add({Category, {}});
			EffectList = &Effects.Last();
		}

		TSoftObjectPtr<UMaterialFunctionInterface> SoftPtr;
		SoftPtr = Asset.GetSoftObjectPath();

		EffectList->Effects.Add(SoftPtr);
	}

	return Effects;
}

#undef LOCTEXT_NAMESPACE
