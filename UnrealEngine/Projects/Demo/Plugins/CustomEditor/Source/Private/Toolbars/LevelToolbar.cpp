#include "LevelToolbar.h"

void FLevelToolbar::Initialize()
{
	FCustomEditorToolbar::Initialize();

	// UE4.27写法
	//FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	//const auto ToolbarExtender = GetExtender(nullptr);
	//LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);


#define LOCTEXT_NAMESPACE "FLevelToolbar"

	// 模仿引擎插件写法，UE官方提供了ExtendMenu方法给插件拓展Editor
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");

		FToolMenuEntry BlueprintEntry = FToolMenuEntry::InitComboButton(
					"CustomEditor",
					FUIAction(),
					FOnGetContent::CreateStatic(&FCustomEditorToolbar::GenerateMenuContent, CommandList, Cast<UObject>(ToolbarMenu)),
					LOCTEXT("CustomEditor_Label", "CustomEditor"),
					LOCTEXT("CustomEditor_ToolTip", "效率"),
					FSlateIcon("CustomEditor", TEXT("CustomEditor"))
				);

		BlueprintEntry.StyleNameOverride = "CustomEditorToolbar";
		
		FToolMenuSection& Section = ToolbarMenu->AddSection("CustomEditorToolbar");
		Section.AddEntry(BlueprintEntry);
	}
	
#undef LOCTEXT_NAMESPACE
}
