// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_Material.h"
#include "MaterialEditorModule.h"

EAssetCommandResult UAssetDefinition_Material::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UMaterial* Material : OpenArgs.LoadObjects<UMaterial>())
	{
		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
		MaterialEditorModule->CreateMaterialEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Material);
	}

	return EAssetCommandResult::Handled;
}
