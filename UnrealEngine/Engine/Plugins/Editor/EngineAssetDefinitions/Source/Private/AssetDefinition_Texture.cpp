// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_Texture.h"

#include "AssetRegistry/AssetIdentifier.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Factories/MaterialFactoryNew.h"
#include "Interfaces/ITextureEditorModule.h"

#include "ToolMenu.h"
#include "ToolMenus.h"
#include "IAssetTools.h"
#include "ToolMenuSection.h"
#include "VirtualTexturingEditorModule.h"
#include "Algo/AnyOf.h"
#include "AssetRegistry/AssetRegistryHelpers.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_Texture"

EAssetCommandResult UAssetDefinition_Texture::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UTexture* Texture : OpenArgs.LoadObjects<UTexture>())
	{
		ITextureEditorModule* TextureEditorModule = &FModuleManager::LoadModuleChecked<ITextureEditorModule>("TextureEditor");
		TextureEditorModule->CreateTextureEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Texture);
	}
	
	return EAssetCommandResult::Handled;
}

// Menu Extensions
//--------------------------------------------------------------------

namespace MenuExtension_Texture
{
	void ExecuteCreateMaterial(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

		IAssetTools::Get().CreateAssetsFrom<UTexture>(
			CBContext->LoadSelectedObjects<UTexture>(), UMaterial::StaticClass(), TEXT("_Mat"), [](UTexture* SourceObject)
			{
				UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
				Factory->InitialTexture = SourceObject;
				return Factory;
			}
		);
	}

	static void ExecuteConvertToVirtualTexture(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			IVirtualTexturingEditorModule* Module = FModuleManager::Get().GetModulePtr<IVirtualTexturingEditorModule>("VirtualTexturingEditor");
			Module->ConvertVirtualTexturesWithDialog(CBContext->LoadSelectedObjects<UTexture2D>(), false);
		}
	}

	static void ExecuteConvertToRegularTexture(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			IVirtualTexturingEditorModule* Module = FModuleManager::Get().GetModulePtr<IVirtualTexturingEditorModule>("VirtualTexturingEditor");
			Module->ConvertVirtualTexturesWithDialog(CBContext->LoadSelectedObjects<UTexture2D>(), true);
		}
	}

	static void ExecuteFindMaterials(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			TArray<FAssetData> MaterialsUsingTexture;

			// This finds "Material like" objects. It's called material in the UI string but this generally
			// seems more useful
			if ( const FAssetData* TextureAsset = CBContext->GetSingleSelectedAssetOfType(UTexture::StaticClass()) )
			{
				UAssetRegistryHelpers::FindReferencersOfAssetOfClass(TextureAsset->PackageName, { UMaterialInterface::StaticClass(), UMaterialFunction::StaticClass() }, MaterialsUsingTexture);
			}

			if (MaterialsUsingTexture.Num() > 0)
			{
				IAssetTools::Get().SyncBrowserToAssets(MaterialsUsingTexture);
			}
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UTexture::StaticClass());
		
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
				{
					if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
					{
						{
							const TAttribute<FText> Label = LOCTEXT("Texture_CreateMaterial", "Create Material");
							const TAttribute<FText> ToolTip = LOCTEXT("Texture_CreateMaterialTooltip", "Creates a new material using this texture.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material");
							const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateMaterial);
							InSection.AddMenuEntry("Texture_CreateMaterial", Label, ToolTip, Icon, UIAction);
						}
											
						static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures"));
						check(CVarVirtualTexturesEnabled);
						
						bool bVTEnabled = !! CVarVirtualTexturesEnabled->GetValueOnAnyThread();
						
						static const auto CVarVirtualTexturesMenuRestricted = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VT.MenuRestricted"));
						check(CVarVirtualTexturesMenuRestricted);
						
						bool bVTMenuRestricted = !! CVarVirtualTexturesMenuRestricted->GetValueOnAnyThread();

						if ( bVTEnabled && ! bVTMenuRestricted )
						{
							const bool bHasVirtualTextures =
								Algo::AnyOf(Context->SelectedAssets, [](const FAssetData& AssetData){ 
									bool VirtualTextured = false;
									AssetData.GetTagValue<bool>("VirtualTextureStreaming", VirtualTextured);
									return VirtualTextured;
								});

							if (bHasVirtualTextures)
							{
								const TAttribute<FText> Label = LOCTEXT("Texture_ConvertToRegular", "Convert to Regular Texture");
								const TAttribute<FText> ToolTip = LOCTEXT("Texture_ConvertToRegularTooltip", "Converts this texture to a regular 2D texture if it is a virtual texture.");
								const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");
								const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteConvertToRegularTexture);
								InSection.AddMenuEntry("Texture_ConvertToVT", Label, ToolTip, Icon, UIAction);
							}
						
							if (!bHasVirtualTextures)
							{
								const TAttribute<FText> Label = LOCTEXT("Texture_ConvertToVT", "Convert to Virtual Texture");
								const TAttribute<FText> ToolTip = LOCTEXT("Texture_ConvertToVTTooltip", "Converts this texture to a virtual texture if it fits the size limit imposed in the texture importer settings.");
								const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");
								const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteConvertToVirtualTexture);
								InSection.AddMenuEntry("Texture_ConvertToVT", Label, ToolTip, Icon, UIAction);
							}
						}
						
						if ( Context->SelectedAssets.Num() == 1 )
						{
							const TAttribute<FText> Label = LOCTEXT("Texture_FindMaterials", "Find Materials Using This");
							const TAttribute<FText> ToolTip = LOCTEXT("Texture_FindMaterialsTooltip", "Finds all materials that use this material in the content browser.");
							const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Find");
							const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteFindMaterials);
							InSection.AddMenuEntry("Texture_FindMaterials", Label, ToolTip, Icon, UIAction);
						}
					}
				}));
		}));
	});
}

#undef LOCTEXT_NAMESPACE
