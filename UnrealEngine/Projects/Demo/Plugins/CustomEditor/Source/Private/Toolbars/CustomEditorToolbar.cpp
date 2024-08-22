#include "CustomEditorToolbar.h"

#include "BaseWidgetBlueprint.h"
#include "CustomEditor.h"
#include "CustomEditorCommands.h"
#include "GameTables.h"
#include "GameTablesModule.h"
#include "GameServicesModuleInterface.h"
#include "Blueprint/UserWidget.h"


bool GeneratePerforceConfig()
{
	FString SCCProvider;
	FString P4Port;
	FString P4User;
	FString P4Client;

	bool bOk = false;
	do
	{
		FString IniFile;
		{
			// 配置项参见 Engine/Source/Developer/SourceControl/Private/SourceControlHelpers.cpp
			const FString SourceControlSettingsDir = FPaths::GeneratedConfigDir();
			FConfigCacheIni::LoadGlobalIniFile(
				IniFile, TEXT("SourceControlSettings"),
				nullptr, false, false, true, true,
				*SourceControlSettingsDir);
		}
		{
			// 配置项参见 Engine/Source/Developer/SourceControl/Private/SourceControlSettings.cpp
			static const FString SettingsSection = TEXT("SourceControl.SourceControlSettings");
			GConfig->GetString(*SettingsSection, TEXT("Provider"), SCCProvider, IniFile);
			if (SCCProvider != TEXT("Perforce"))
				break;
		}
		{
			// 配置项参见
			// Engine/Plugins/Developer/PerforceSourceControl/Source/PerforceSourceControl/Private/PerforceSourceControlSettings.cpp
			static const FString SettingsSection = TEXT("PerforceSourceControl.PerforceSourceControlSettings");
			if (!GConfig->GetString(*SettingsSection, TEXT("Port"), P4Port, IniFile))
				break;
			if (!GConfig->GetString(*SettingsSection, TEXT("UserName"), P4User, IniFile))
				break;
			if (!GConfig->GetString(*SettingsSection, TEXT("Workspace"), P4Client, IniFile))
				break;
		}
		bOk = true;
	}
	while (false);
	if (!bOk)
	{
		const FString Text = FString::Printf(TEXT("未正确配置 Perforce"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
		return false;
	}

	const FString P4ConfigFile = FPaths::EngineDir() + "../.p4config"; // 在工作区根目录
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (IFileHandle* FileHandle = PlatformFile.OpenWrite(*P4ConfigFile))
	{
		const auto Body = FString::Printf(
			TEXT("P4PORT=%s\nP4USER=%s\nP4CLIENT=%s\nP4CHARSET=utf8\n"), *P4Port, *P4User, *P4Client);
		FileHandle->Write(reinterpret_cast<const uint8*>(TCHAR_TO_ANSI(*Body)), Body.Len());

		delete FileHandle;
	}
	else
	{
		const FString Text = FString::Printf(TEXT("创建P4配置文件失败! %s"), *P4ConfigFile); 
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
		return false;
	}
	return true;
}

bool ExecuteShellScript(const FString& ScriptFile, const FString& WorkDir)
{
#if PLATFORM_WINDOWS
	FString ScriptExt = TEXT(".bat");
	FString CmdExe = TEXT("cmd.exe");
#elif PLATFORM_LINUX
	FString ScriptExt = TEXT(".sh");
	FString CmdExe = TEXT("/bin/bash");
#else
	FString ScriptExt = TEXT(".sh");
	FString CmdExe = TEXT("/bin/sh");
#endif

	int32 ReturnCode = 0;
	const FString Params(ScriptFile + ScriptExt);
	FProcHandle RunningProc = FPlatformProcess::CreateProc(*CmdExe, *Params, true, false, false, nullptr, 0,
	                                                       *WorkDir, nullptr);
	FPlatformProcess::WaitForProc(RunningProc);
	FPlatformProcess::GetProcReturnCode(RunningProc, &ReturnCode);
	FPlatformProcess::CloseProc(RunningProc);
	UE_LOG(LogTemp, Display, TEXT("Code=%d"), ReturnCode);

	return ReturnCode == 0;
}

bool ExecutePyScript(const FString& Params, const FString& WorkDir)
{
	// FString PlatformName = FPlatformProperties::PlatformName();
	const FString BinDirName = FPlatformProcess::GetBinariesSubdirectory();
	FString PyBin = FPaths::EngineDir() + TEXT("Binaries/ThirdParty/Python3/") + BinDirName;
	if (BinDirName == TEXT("Win64"))
	{
		PyBin += TEXT("/python");
	}
	else
	{
		PyBin += TEXT("/bin/python");
	}

	PyBin = FPaths::ConvertRelativePathToFull(PyBin);

	int32 ReturnCode = 0;
	FProcHandle RunningProc = FPlatformProcess::CreateProc(*PyBin, *Params, true, false, false, nullptr, 0,
	                                                       *WorkDir, nullptr);
	FPlatformProcess::WaitForProc(RunningProc);
	FPlatformProcess::GetProcReturnCode(RunningProc, &ReturnCode);
	FPlatformProcess::CloseProc(RunningProc);
	
	return ReturnCode == 0;
}


IGameServicesModule* GetGameServiceModule()
{
	EModuleLoadResult LoadResult;
	if (auto Mod = FModuleManager::Get().LoadModuleWithFailureReason(TEXT("MGameServices"), LoadResult))
	{
		return StaticCast<IGameServicesModule*>(Mod);
	}
	
	return nullptr;
}


FCustomEditorToolbar::~FCustomEditorToolbar()
{
}

void FCustomEditorToolbar::Initialize()
{
	BindCommands();
}

void FCustomEditorToolbar::BindCommands()
{
	FCustomEditorCommands::Register();

	CommandList->MapAction(
		FCustomEditorCommands::Get().StartGameService,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::StartGameService_Executed),
		FCanExecuteAction());
	CommandList->MapAction(
		FCustomEditorCommands::Get().StopGameService,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::StopGameService_Executed),
		FCanExecuteAction());

	CommandList->MapAction(
		FCustomEditorCommands::Get().ShowGddInFileExplorer,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::ShowGddInFileExplorer_Executed),
		FCanExecuteAction());
	
	CommandList->MapAction(
		FCustomEditorCommands::Get().ShowGddInFileExplorer,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::ShowGddInFileExplorer_Executed),
		FCanExecuteAction());

	CommandList->MapAction(
		FCustomEditorCommands::Get().ShowExcelInFileExplorer,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::ShowExcelInFileExplorer_Executed),
		FCanExecuteAction());

	CommandList->MapAction(
		FCustomEditorCommands::Get().UpdateGdd,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::UpdateGdd_Executed),
		FCanExecuteAction());
	
	CommandList->MapAction(
		FCustomEditorCommands::Get().ReloadGdd,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::ReloadGdd_Executed),
		FCanExecuteAction());
	
	CommandList->MapAction(
		FCustomEditorCommands::Get().UpdatePb,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::UpdatePb_Executed),
		FCanExecuteAction());

	CommandList->MapAction(
		FCustomEditorCommands::Get().GenerateWidgetTsFile,
		FExecuteAction::CreateRaw(this, &FCustomEditorToolbar::GenerateWidgetTsFile_Executed),
		FCanExecuteAction());
}

void FCustomEditorToolbar::StartGameService_Executed()
{
	if (const auto Mod = GetGameServiceModule())
	{
		Mod->Start();
	}
}

void FCustomEditorToolbar::StopGameService_Executed()
{
	if (const auto Mod = GetGameServiceModule())
	{
		Mod->Shutdown();
	}
}

void FCustomEditorToolbar::ShowGddInFileExplorer_Executed()
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + TEXT("GDD"));
	FPlatformProcess::ExploreFolder(*Dir);
}

void FCustomEditorToolbar::ShowExcelInFileExplorer_Executed()
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + TEXT("../GameDesignData/Excel"));
	FPlatformProcess::ExploreFolder(*Dir);
}

void FCustomEditorToolbar::UpdateGdd_Executed()
{
	if (!GeneratePerforceConfig()) // 重新生成P4配置文件
	{
		UE_LOG(LogCustomEditor, Error, TEXT("Perforce配置无效"));
		//return;// Todo 暂时没有版本控制
	}

	{
		const FString Text = TEXT("确定要打表？");
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Text)) != EAppReturnType::Yes)
			return;
	}

	const FString WorkDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() + TEXT("GameTables/"));
	const FString LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("make_gdd_err.log"));
	const FString Params = WorkDir + "make.py -log_file=" + LogFile;
	const bool bOk = ExecutePyScript(Params, WorkDir);

	FString Text;
	if (bOk)
	{
		Text = TEXT("成功");
	}
	else
	{
		Text = TEXT("失败!");

#if PLATFORM_WINDOWS
		{
			FProcHandle Proc = FPlatformProcess::CreateProc(
				TEXT("c:\\Windows\\notepad.exe"), *LogFile,
				true, false, false, nullptr, 0,
				*WorkDir, nullptr);
			if (!Proc.IsValid())
			{
				FPlatformProcess::CloseProc(Proc);
			}
		}
#endif		
		
		// IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		//
		// TSharedPtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*LogFile));
		// if (FileHandle.IsValid())
		// {
		// 	TArray<uint8> Buffer;
		// 	Buffer.AddUninitialized(FileHandle->Size());
		// 	if (FileHandle->Read(Buffer.GetData(), FileHandle->Size()))
		// 	{
		// 		FString Str = UTF8_TO_TCHAR(reinterpret_cast<char*>(Buffer.GetData()));
		// 		Text = Text + TEXT("\n") + Str;
		// 	}
		// }
	}

	
	if (bOk)
	{
		ReloadGdd_Executed();
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}
}

void FCustomEditorToolbar::ReloadGdd_Executed()
{
	const auto GameTables = FGameTablesModule::Get().GetGameTables();
	GameTables->Init(true);

	{
		const FString Text = TEXT("读表完成");
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}
}

void FCustomEditorToolbar::UpdatePb_Executed()
{
	if (!GeneratePerforceConfig()) // 重新生成P4配置文件
	{
		// 暂不考虑版本管理
		//return;
	}

	{
		const FString Text = TEXT("确定要重新生成 .proto 文件吗？");
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Text)) != EAppReturnType::Yes)
			return;
	}

	bool bOk;
	FString LogFile;
	do
	{
		FString WorkDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() + TEXT("MyProject/Shared/MProtocol/Source/"));
		LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("make_protocol_err.log"));
		FString Params = WorkDir + "make.py -log_file=" + LogFile;
		bOk = ExecutePyScript(Params, WorkDir);
		if (!bOk)
			break;

		WorkDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() + TEXT("MyProject/Shared/MRpc/Source/"));
		LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("make_rpc_err.log"));
		Params = WorkDir + "make.py -log_file=" + LogFile;
		bOk = ExecutePyScript(Params, WorkDir);
		if (!bOk)
			break;

		bOk = true;
	} while (false);
	
	FString Text;
	if (bOk)
	{
		Text = TEXT("成功");
	}
	else
	{
		Text = TEXT("失败!");

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		TSharedPtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*LogFile));
		if (FileHandle.IsValid())
		{
			TArray<uint8> Buffer;
			Buffer.AddUninitialized(FileHandle->Size());
			if (FileHandle->Read(Buffer.GetData(), FileHandle->Size()))
			{
				FString Str = UTF8_TO_TCHAR(reinterpret_cast<char*>(Buffer.GetData()));
				Text = Text + TEXT("\n") + Str;
			}
		}
	}
	
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
}

void FCustomEditorToolbar::GenerateWidgetTsFile_Executed()
{
	const auto* Blueprint = Cast<UBaseWidgetBlueprint>(ContextObject);
	if (!Blueprint)
	{
		UE_LOG(LogCustomEditor, Warning, TEXT("操作上下文空指针"));
		return;
	}

	const FString PathName = Blueprint->GetPathName();
	if (!PathName.Contains("WBP_") || !PathName.Contains("WidgetBP"))
	{
		UE_LOG(LogCustomEditor, Warning, TEXT("操作上下文不是蓝图"));
		return;
	}

	// '/Game/Blueprints/WidgetBP/WBP_Example.WBP_Example_C'
	FString WidgetAssetPath = PathName + TEXT("_C");

	// 提取出'WBP_Example_C'
	FString WidgetName;
	{
		TArray<FString> Data;
		PathName.ParseIntoArray(Data, TEXT("."));
		WidgetName = Data[Data.Num() - 1];
	}

	// 组装'UE.Game.Blueprints.WidgetBP.WBP_Login.WBP_Example_C'
	FString WidgetJsClassName = WidgetAssetPath.Replace(TEXT("/"), TEXT("."));
	WidgetJsClassName.RemoveAt(0);
	
	FString BasePath = FPaths::ProjectDir() / TEXT("TypeScript") / TEXT("widgets");
	
	FString TargetDir;
	{
		TArray<FString> Data;
		PathName.ParseIntoArray(Data, TEXT("/"));
		Data.RemoveAt(0);
		Data.RemoveAt(0);
		Data.RemoveAt(Data.Num() - 1);
		for (const auto& Dir : Data)
		{
			TargetDir /= Dir;
		}
	}

	BasePath /= TargetDir;
	const FString FullPath = FPaths::ConvertRelativePathToFull(BasePath);
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	PlatformFile.CreateDirectoryTree(*FullPath);  // 保证目录存在

	const FString TsFile = FullPath / WidgetName + TEXT(".ts");

	FString WorkDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools"));
	FString Params = FString::Printf(TEXT("generate_widget_ts.py -ts_dst_file \"%s\" -widget_name \"%s\" -class_name \"%s\" -asset_path \"%s\""), *TsFile, *WidgetName, *WidgetJsClassName, *WidgetAssetPath);
	FString Cmd = WorkDir / Params;
	
	if (ExecutePyScript(Cmd, WorkDir))
	{
		FPlatformProcess::ExploreFolder(*TsFile);		
	}
	else
	{
		const FString Text = FString::Printf(TEXT("生成失败\n==========\n工作目录\n%s\n==========\n执行指令\n%s"), *WorkDir, *Cmd);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}
}

/*
void FZEditorToolbar::UpdatePb_Executed()
{
	if (!GeneratePerforceConfig()) // 重新生成P4配置文件
		return;

	{
		FString Text = TEXT("你确定要重新处理 .proto 文件吗？");
		if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Text)) != EAppReturnType::Yes)
			return;
	}

	bool bOk = false;
	FString WorkDir;
	FString LogFile;
	FString Params;
	do
	{
		WorkDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() + TEXT("ZShared/ZProtocol/Source/"));
		LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("make_protocol_err.log"));
		Params = WorkDir + "make.py -log_file=" + LogFile;
		bOk = ExecutePyScript(Params, WorkDir);
		if (!bOk)
			break;

		WorkDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() + TEXT("ZShared/ZRpc/Source/"));
		LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("make_rpc_err.log"));
		Params = WorkDir + "make.py -log_file=" + LogFile;
		bOk = ExecutePyScript(Params, WorkDir);
		if (!bOk)
			break;

		bOk = true;
	} while (false);
	FString Text;
	if (bOk)
	{
		Text = TEXT("成功");
	}
	else
	{
		Text = TEXT("失败!");

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		TSharedPtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*LogFile));
		if (FileHandle.IsValid())
		{
			TArray<uint8> Buffer;
			Buffer.AddUninitialized(FileHandle->Size());
			if (FileHandle->Read(Buffer.GetData(), FileHandle->Size()))
			{
				FString Str = UTF8_TO_TCHAR(reinterpret_cast<char*>(Buffer.GetData()));
				Text = Text + TEXT("\n") + Str;
			}
		}
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
}*/

TSharedRef<SWidget> FCustomEditorToolbar::GenerateMenuContent(TSharedRef<FUICommandList> InCommandList, UObject* Context)
{
	const FCustomEditorCommands& Commands = FCustomEditorCommands::Get();
	FMenuBuilder MenuBuilder(true, InCommandList);

	// Server about
	{
		auto Mod = GetGameServiceModule();
		if (Mod && Mod->IsRunning())
		{
			MenuBuilder.AddMenuEntry(Commands.StopGameService);
		}
		else
		{
			MenuBuilder.AddMenuEntry(Commands.StartGameService);
		}
	}
				
	{
		MenuBuilder.AddSeparator(TEXT("SeparatorGDD"));
		MenuBuilder.AddMenuEntry(Commands.ShowExcelInFileExplorer);
		MenuBuilder.AddMenuEntry(Commands.ShowGddInFileExplorer);
		MenuBuilder.AddMenuEntry(Commands.UpdateGdd);
		MenuBuilder.AddMenuEntry(Commands.ReloadGdd);
	}

	{
		MenuBuilder.AddSeparator(TEXT("SeparatorPb"));
		MenuBuilder.AddMenuEntry(Commands.UpdatePb);
	}

	if (const auto* Blueprint = Cast<UBaseWidgetBlueprint>(Context))
	{
		if (Blueprint->ParentClass->IsChildOf(UUserWidget::StaticClass()))
		{
			MenuBuilder.AddSeparator(TEXT("SeparatorTs"));
			MenuBuilder.AddMenuEntry(Commands.GenerateWidgetTsFile);
		}
	}
			
	return MenuBuilder.MakeWidget();
}

#define LOCTEXT_NAMESPACE "FCustomEditorToolbar"

void FCustomEditorToolbar::BuildToolbar(FToolBarBuilder& ToolbarBuilder, UObject* InContextObject)
{
	// InContextObject 在关卡模式为 nullptr， 在蓝图模式下则为当前蓝图对象

	ToolbarBuilder.BeginSection(NAME_None);

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateLambda([&, InContextObject]()
		{
			ContextObject = InContextObject;
			
			return GenerateMenuContent(CommandList, ContextObject);
		}),
		LOCTEXT("CustomEditor_Label", "CustomEditor"),
		LOCTEXT("CustomEditor_ToolTip", "效率"),
		FSlateIcon("CustomEditorStyle", TEXT("CustomEditorStyle"))
	);

	ToolbarBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE