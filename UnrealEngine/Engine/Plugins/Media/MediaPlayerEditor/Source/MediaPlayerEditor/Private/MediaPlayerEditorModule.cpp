// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "IAssetTypeActions.h"
#include "PropertyEditorModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UnrealEdGlobals.h"

#include "BaseMediaSource.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "PlatformMediaSource.h"

#include "AssetTools/FileMediaSourceActions.h"
#include "AssetTools/MediaPlayerActions.h"
#include "AssetTools/MediaPlaylistActions.h"
#include "AssetTools/MediaSourceActions.h"
#include "AssetTools/MediaTextureActions.h"
#include "AssetTools/PlatformMediaSourceActions.h"
#include "AssetTools/StreamMediaSourceActions.h"

#include "Customizations/BaseMediaSourceCustomization.h"
#include "Customizations/FileMediaSourceCustomization.h"
#include "Customizations/MediaTextureCustomization.h"
#include "Customizations/PlatformMediaSourceCustomization.h"

#include "Models/MediaPlayerEditorCommands.h"
#include "Shared/MediaPlayerEditorStyle.h"
#include "Visualizers/MediaSoundComponentVisualizer.h"
#include "ToolMenus.h"
#include "Widgets/SMediaPlayerEditorMedia.h"

#include "MediaPlayerEditorLog.h"


#define LOCTEXT_NAMESPACE "FMediaPlayerEditorModule"

DEFINE_LOG_CATEGORY(LogMediaPlayerEditor);


/**
 * Implements the MediaPlayerEditor module.
 */
class FMediaPlayerEditorModule
	: public IHasMenuExtensibility
	, public IHasToolBarExtensibility
	, public IModuleInterface
{
public:

	//~ IHasMenuExtensibility interface

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override
	{
		return MenuExtensibilityManager;
	}

public:

	//~ IHasToolBarExtensibility interface

	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override
	{
		return ToolBarExtensibilityManager;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		BaseMediaSourceName = UBaseMediaSource::StaticClass()->GetFName();
		FileMediaSourceName = UFileMediaSource::StaticClass()->GetFName();
		MediaSoundComponentName = UMediaSoundComponent::StaticClass()->GetFName();
		MediaTextureName = UMediaTexture::StaticClass()->GetFName();
		PlatformMediaSourceName = UPlatformMediaSource::StaticClass()->GetFName();

		Style = MakeShareable(new FMediaPlayerEditorStyle());

		FMediaPlayerEditorCommands::Register();

		SMediaPlayerEditorMedia::RegisterMenus();

		RegisterAssetTools();
		RegisterCustomizations();
		RegisterEditorDelegates();
		RegisterMenuExtensions();
		RegisterThumbnailRenderers();
		RegisterVisualizers();
	}

	virtual void ShutdownModule() override
	{
		UnregisterAssetTools();
		UnregisterCustomizations();
		UnregisterEditorDelegates();
		UnregisterMenuExtensions();
		UnregisterThumbnailRenderers();
		UnregisterVisualizers();
	}

protected:

	/** Registers asset tool actions. */
	void RegisterAssetTools()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		RegisterAssetTypeAction(AssetTools, MakeShareable(new FFileMediaSourceActions(Style.ToSharedRef())));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FMediaPlayerActions(Style.ToSharedRef())));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FMediaPlaylistActions(Style.ToSharedRef())));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FMediaSourceActions));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FMediaTextureActions));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FPlatformMediaSourceActions(Style.ToSharedRef())));
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FStreamMediaSourceActions(Style.ToSharedRef())));
	}

	/**
	 * Registers a single asset type action.
	 *
	 * @param AssetTools The asset tools object to register with.
	 * @param Action The asset type action to register.
	 */
	void RegisterAssetTypeAction( IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action )
	{
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}

	/** Unregisters asset tool actions. */
	void UnregisterAssetTools()
	{
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

		if (AssetToolsModule != nullptr)
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();

			for (auto Action : RegisteredAssetTypeActions)
			{
				AssetTools.UnregisterAssetTypeActions(Action);
			}
		}
	}

protected:

	/** Register details view customizations. */
	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		{
			PropertyModule.RegisterCustomClassLayout(FileMediaSourceName, FOnGetDetailCustomizationInstance::CreateStatic(&FFileMediaSourceCustomization::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(MediaTextureName, FOnGetDetailCustomizationInstance::CreateStatic(&FMediaTextureCustomization::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(PlatformMediaSourceName, FOnGetDetailCustomizationInstance::CreateStatic(&FPlatformMediaSourceCustomization::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(BaseMediaSourceName, FOnGetDetailCustomizationInstance::CreateStatic(&FBaseMediaSourceCustomization::MakeInstance));
		}
	}

	/** Unregister details view customizations. */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		{
			PropertyModule.UnregisterCustomClassLayout(BaseMediaSourceName);
			PropertyModule.UnregisterCustomClassLayout(FileMediaSourceName);
			PropertyModule.UnregisterCustomClassLayout(MediaTextureName);
			PropertyModule.UnregisterCustomClassLayout(PlatformMediaSourceName);
		}
	}

protected:

	/** Register Editor delegates. */
	void RegisterEditorDelegates()
	{
		FEditorDelegates::BeginPIE.AddRaw(this, &FMediaPlayerEditorModule::HandleEditorBeginPIE);
		FEditorDelegates::EndPIE.AddRaw(this, &FMediaPlayerEditorModule::HandleEditorEndPIE);
		FEditorDelegates::PausePIE.AddRaw(this, &FMediaPlayerEditorModule::HandleEditorPausePIE);
		FEditorDelegates::ResumePIE.AddRaw(this, &FMediaPlayerEditorModule::HandleEditorResumePIE);
	}

	/** Unregister Editor delegates. */
	void UnregisterEditorDelegates()
	{
		FEditorDelegates::BeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);
		FEditorDelegates::PausePIE.RemoveAll(this);
		FEditorDelegates::ResumePIE.RemoveAll(this);
	}

protected:

	/** Registers main menu and tool bar menu extensions. */
	void RegisterMenuExtensions()
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Unregisters main menu and tool bar menu extensions. */
	void UnregisterMenuExtensions()
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

protected:

	/** Registers asset thumbnail renderers .*/
	void RegisterThumbnailRenderers()
	{
		UThumbnailManager::Get().RegisterCustomRenderer(UMediaTexture::StaticClass(), UTextureThumbnailRenderer::StaticClass());
	}

	/** Unregisters all asset thumbnail renderers. */
	void UnregisterThumbnailRenderers()
	{
		if (UObjectInitialized())
		{
			UThumbnailManager::Get().UnregisterCustomRenderer(UMediaTexture::StaticClass());
		}
	}

protected:

	/** Register all component visualizers. */
	void RegisterVisualizers()
	{
		if (GUnrealEd != nullptr)
		{
			RegisterVisualizer(*GUnrealEd, MediaSoundComponentName, MakeShared<FMediaSoundComponentVisualizer>());
		}
	}

	/**
	 * Register a component visualizer.
	 *
	 * @param ComponentClassName The name of the component class to register.
	 * @param Visualizer The component visualizer to register.
	 */
	void RegisterVisualizer(UUnrealEdEngine& UnrealEdEngine, const FName& ComponentClassName, const TSharedRef<FComponentVisualizer>& Visualizer)
	{
		UnrealEdEngine.RegisterComponentVisualizer(ComponentClassName, Visualizer);
		Visualizer->OnRegister();
	}

	/** Unregister all component visualizers. */
	void UnregisterVisualizers()
	{
		if (GUnrealEd != nullptr)
		{
			GUnrealEd->UnregisterComponentVisualizer(MediaSoundComponentName);
		}
	}

private:

	void HandleEditorBeginPIE(bool bIsSimulating)
	{
		for (TObjectIterator<UMediaPlayer> It; It; ++It)
		{
			UMediaPlayer* Player = *It;
			if (Player->AffectedByPIEHandling)
			{
				Player->Close();
			}
		}
	}

	void HandleEditorEndPIE(bool bIsSimulating)
	{
		for (TObjectIterator<UMediaPlayer> It; It; ++It)
		{
			UMediaPlayer* Player = *It;
			if (Player->AffectedByPIEHandling)
			{
				(*It)->Close();
			}
		}
	}

	void HandleEditorPausePIE(bool bIsSimulating)
	{
		for (TObjectIterator<UMediaPlayer> It; It; ++It)
		{
			UMediaPlayer* Player = *It;
			if (Player->AffectedByPIEHandling)
			{
				(*It)->PausePIE();
			}
		}
	}

	void HandleEditorResumePIE(bool bIsSimulating)
	{
		for (TObjectIterator<UMediaPlayer> It; It; ++It)
		{
			UMediaPlayer* Player = *It;
			if (Player->AffectedByPIEHandling)
			{
				(*It)->ResumePIE();
			}
		}
	}

private:

	/** Holds the menu extensibility manager. */
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;

	/** The collection of registered asset type actions. */
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** Holds the plug-ins style set. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds the tool bar extensibility manager. */
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Class names. */
	FName BaseMediaSourceName;
	FName FileMediaSourceName;
	FName MediaSoundComponentName;
	FName MediaTextureName;
	FName PlatformMediaSourceName;
};


IMPLEMENT_MODULE(FMediaPlayerEditorModule, MediaPlayerEditor);


#undef LOCTEXT_NAMESPACE
