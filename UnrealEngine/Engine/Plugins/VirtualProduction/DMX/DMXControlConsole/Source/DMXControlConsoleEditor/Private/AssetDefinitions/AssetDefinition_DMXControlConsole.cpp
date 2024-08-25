// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_DMXControlConsole.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleEditorModule.h"
#include "Toolkits/DMXControlConsoleEditorToolkit.h"
#include "Style/DMXControlConsoleEditorStyle.h"


#define LOCTEXT_NAMESPACE "AssetDefinition_DMXControlConsole"

FText UAssetDefinition_DMXControlConsole::GetAssetDisplayName() const
{ 
	return LOCTEXT("AssetDefinition_DMXControlConsole", "DMX Control Console"); 
}

FLinearColor UAssetDefinition_DMXControlConsole::GetAssetColor() const
{ 
	return FLinearColor(FColor(62, 140, 35)); 
}

TSoftClassPtr<UObject> UAssetDefinition_DMXControlConsole::GetAssetClass() const
{ 
	return UDMXControlConsole::StaticClass(); 
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DMXControlConsole::GetAssetCategories() const
{
	static const auto Categories = { FDMXControlConsoleEditorModule::Get().GetControlConsoleCategory() };
	return Categories;
}

EAssetCommandResult UAssetDefinition_DMXControlConsole::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UDMXControlConsole* DMXControlConsole : OpenArgs.LoadObjects<UDMXControlConsole>())
	{
		using namespace UE::DMX::Private;
		TSharedRef<FDMXControlConsoleEditorToolkit> NewEditor(MakeShared<FDMXControlConsoleEditorToolkit>());
		NewEditor->InitControlConsoleEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, DMXControlConsole);
	}

	return EAssetCommandResult::Handled;
}

const FSlateBrush* UAssetDefinition_DMXControlConsole::GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.TabIcon");
}

const FSlateBrush* UAssetDefinition_DMXControlConsole::GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const
{
	return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.TabIcon");
}

#undef LOCTEXT_NAMESPACE
