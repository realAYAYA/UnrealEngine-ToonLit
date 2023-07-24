// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_UserDefinedStruct.h"

#include "BlueprintEditorModule.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_UserDefinedStruct"

EAssetCommandResult UAssetDefinition_UserDefinedStruct::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	if (FStructureEditorUtils::UserDefinedStructEnabled())
	{
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );

		for (UUserDefinedStruct* UDStruct : OpenArgs.LoadObjects<UUserDefinedStruct>())
		{
			BlueprintEditorModule.CreateUserDefinedStructEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, UDStruct);
		}
	}

	return EAssetCommandResult::Handled;
}

FText UAssetDefinition_UserDefinedStruct::GetAssetDescription(const FAssetData& AssetData) const
{
	FString Description = AssetData.GetTagValueRef<FString>("Tooltip");
	if (!Description.IsEmpty())
	{
		Description.ReplaceInline(TEXT("\\n"), TEXT("\n"));
		return FText::FromString(MoveTemp(Description));
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
