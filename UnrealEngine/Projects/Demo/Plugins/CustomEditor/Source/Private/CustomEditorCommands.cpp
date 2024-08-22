// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomEditorCommands.h"

#define LOCTEXT_NAMESPACE "FDEditorModule"

void FCustomEditorCommands::RegisterCommands()
{
	UI_COMMAND(StartGameService, "Start GameServer", "Start Game Service", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopGameService, "Stop GameServer", "Stop Game Service", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowGddInFileExplorer, "打开 GDD 文件夹", "Show GDD In FileManager", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowExcelInFileExplorer, "打开 Excel 文件夹", "Show Excel In FileManager", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UpdateGdd, "Excel 打表", "Generate GDD data & Code", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReloadGdd, "重新加载 GDD 表格", "Reload GDD tables", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UpdatePb, "重新生成消息文件", "Compile .proto files", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GenerateWidgetTsFile, "生成 Widget 脚本文件", "Genrate Widget .TS files", EUserInterfaceActionType::Button, FInputChord());	
}

#undef LOCTEXT_NAMESPACE
