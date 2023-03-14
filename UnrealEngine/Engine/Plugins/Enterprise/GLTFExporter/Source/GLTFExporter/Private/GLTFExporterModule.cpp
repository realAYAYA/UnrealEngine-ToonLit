// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFExporterModule.h"
#include "Json/GLTFJsonEnums.h"
#include "Converters/GLTFObjectArrayScopeGuard.h"
#include "Utilities/GLTFProxyMaterialUtilities.h"
#include "Actions/GLTFProxyAssetActions.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#if WITH_EDITOR
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AssetToolsModule.h"

class FAssetTypeActions_Base;
#endif

DEFINE_LOG_CATEGORY(LogGLTFExporter);

class FGLTFExporterModule final : public IGLTFExporterModule
{
public:

	virtual void StartupModule() override
	{
		// TODO: shaders should be moved into its own module
		const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("GLTFExporter"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/GLTFExporter"), PluginShaderDir);

#if WITH_EDITOR
		// TODO: UI and editor-only functions should be moved (as much as possible) into its own module
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FGLTFExporterModule::PostEngineInit);
#endif
	}

#if WITH_EDITOR
	void PostEngineInit()
	{
		AddProxyAssetActions();
	}

	void AddProxyAssetActions()
	{
		if (FAssetToolsModule::IsModuleLoaded())
		{
			IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

			// TODO: add support for when GetAssetTypeActionsForClass returns null pointer
			AssetTypeActionsArray.Add(MakeShared<FGLTFProxyAssetActions>(AssetTools.GetAssetTypeActionsForClass(UMaterial::StaticClass()).Pin().ToSharedRef()));
			AssetTypeActionsArray.Add(MakeShared<FGLTFProxyAssetActions>(AssetTools.GetAssetTypeActionsForClass(UMaterialInstanceConstant::StaticClass()).Pin().ToSharedRef()));
			AssetTypeActionsArray.Add(MakeShared<FGLTFProxyAssetActions>(AssetTools.GetAssetTypeActionsForClass(UMaterialInstanceDynamic::StaticClass()).Pin().ToSharedRef()));
			AssetTypeActionsArray.Add(MakeShared<FGLTFProxyAssetActions>(AssetTools.GetAssetTypeActionsForClass(UMaterialInterface::StaticClass()).Pin().ToSharedRef()));

			for (TSharedRef<IAssetTypeActions>& AssetTypeActions : AssetTypeActionsArray)
			{
				AssetTools.RegisterAssetTypeActions(AssetTypeActions);
			}
		}
	}

	void RemoveProxyAssetActions()
	{
		if (FAssetToolsModule::IsModuleLoaded())
		{
			IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
			for (TSharedRef<IAssetTypeActions>& AssetTypeActions : AssetTypeActionsArray)
			{
				AssetTools.UnregisterAssetTypeActions(AssetTypeActions);
			}
		}

		AssetTypeActionsArray.Empty();
	}
#endif

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		RemoveProxyAssetActions();
#endif
	}

private:

#if WITH_EDITOR
	TArray<TSharedRef<IAssetTypeActions>> AssetTypeActionsArray;
#endif
};

IMPLEMENT_MODULE(FGLTFExporterModule, GLTFExporter);
