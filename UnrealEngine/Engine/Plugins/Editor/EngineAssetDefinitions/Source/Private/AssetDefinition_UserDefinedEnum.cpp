// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_UserDefinedEnum.h"
#include "BlueprintEditorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_UserDefinedEnum"

FText UAssetDefinition_UserDefinedEnum::GetAssetDescription(const FAssetData& AssetData) const
{
	return AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UUserDefinedEnum, EnumDescription));
}

EAssetCommandResult UAssetDefinition_UserDefinedEnum::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );

	for (UUserDefinedEnum* UDEnum : OpenArgs.LoadObjects<UUserDefinedEnum>())
	{
		BlueprintEditorModule.CreateUserDefinedEnumEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, UDEnum);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
