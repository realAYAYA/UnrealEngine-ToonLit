#include "CustomFileWatcher.h"

#include "CustomEditor.h"

#if WITH_EDITOR

#include "DirectoryWatcherModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"

//#include "TypeScriptDeclarationGenerator.h"
//#include "ZProtocolModule.h"

FCustomFileWatcher::FCustomFileWatcher()
{
}

FCustomFileWatcher::~FCustomFileWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule =
		FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

	FScopeLock ScopeLock(&SourceFileWatcherCritical);
	for (auto& T : WatchedDirs)
	{
		DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(T.Key, T.Value);
	}	
}

void FCustomFileWatcher::WatchDir(const FString& InPath)
{
	FString Dir = FPaths::GetPath(InPath);

	FScopeLock ScopeLock(&SourceFileWatcherCritical);
	if (!WatchedDirs.Contains(Dir))
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		FDelegateHandle DelegateHandle;
		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(Dir,
			IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FCustomFileWatcher::OnDirectoryChanged), DelegateHandle);
		WatchedDirs.Emplace(Dir, DelegateHandle);
	}
}

void FCustomFileWatcher::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	FScopeLock ScopeLock(&SourceFileWatcherCritical);
	
	bool bChanged = false;
	for (auto Change : FileChanges)
	{
		if (Change.Filename.EndsWith(TEXT(".js")))
		{
			bChanged = true;
			break;
		}
	}
	
	if (bChanged)
	{
		//FZProtocolModule::Get().RebuildPbTypes();// Todo 对应模块做出反应
		this->GenProtocolTsDeclaration();
	}
}

void FCustomFileWatcher::GenProtocolTsDeclaration()
{
	/*UE_LOG(LogCustomEditor, Log, TEXT("GenProtocolTsDeclaration begin..."));
	
	FTypeScriptDeclarationGenerator TypeScriptDeclarationGenerator;
	TypeScriptDeclarationGenerator.bManualGenerate = true;
	TypeScriptDeclarationGenerator.Begin();

	// Todo 重新生成脚本
	/*
	FZProtocolModule::Get().ForeachTypes([&](const FString& TypeName, UObject* TypeObject) -> bool
	{
		TypeScriptDeclarationGenerator.Gen(TypeObject);
		return true;
	});
	#1#
	
	TypeScriptDeclarationGenerator.End();

	const FString UEDeclarationFilePath = FPaths::ProjectDir() / TEXT("Typing/ue/zprotocol.d.ts");

	// #ifdef PUERTS_WITH_SOURCE_CONTROL
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*UEDeclarationFilePath))
	{
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*UEDeclarationFilePath, false);
	}
	// #endif

	FFileHelper::SaveStringToFile(TypeScriptDeclarationGenerator.ToString(), *UEDeclarationFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogCustomEditor, Log, TEXT("GenProtocolTsDeclaration end. (%s)"), *UEDeclarationFilePath);*/
}


#endif
