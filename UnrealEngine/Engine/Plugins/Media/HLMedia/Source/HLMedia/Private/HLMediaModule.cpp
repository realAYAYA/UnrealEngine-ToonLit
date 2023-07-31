// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLMediaModule.h"
#include "HLMediaPrivate.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MessageDialog.h"

#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "HLMediaPlayer.h"

DEFINE_LOG_CATEGORY(LogHLMediaModule);

#define LOCTEXT_NAMESPACE "HLMediaModule"

FHLMediaModule::FHLMediaModule()
	: LibraryHandle(nullptr)
{
}

void FHLMediaModule::StartupModule()
{
	if (LibraryHandle != nullptr)
	{
		return;
	}

	const FString LibraryName = "HLMediaLibrary";
	
	const FString DllName = FString::Printf(TEXT("%s.dll"), *LibraryName);
#if UE_BUILD_DEBUG
	const FString ConfigName = "Debug";
#else
	const FString ConfigName = "Release";
#endif

	const FString ThirdPartyDir = FPaths::EngineDir() / "Binaries/ThirdParty" / LibraryName / FPlatformProperties::IniPlatformName() / ConfigName / TARGET_ARCH;

	// HoloLens needs the full path to load the dll.
	LibraryHandle = FPlatformProcess::GetDllHandle(*(ThirdPartyDir / DllName));
	if (LibraryHandle == nullptr)
	{
		FText Message = FText::Format(LOCTEXT("HLMediaModule", "Failed to load native library:\n\t{0}"), FText::FromString(ThirdPartyDir));
		FText DialogTitle = FText::FromString(TEXT("Cannot find dll"));
		FMessageDialog::Open(EAppMsgType::Ok, Message, &DialogTitle);
	}
}

void FHLMediaModule::ShutdownModule()
{
	if (LibraryHandle == nullptr)
	{
		return;
	}

	FPlatformProcess::FreeDllHandle(LibraryHandle);
	LibraryHandle = nullptr;
}

TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> FHLMediaModule::CreatePlayer(IMediaEventSink& EventSink)
{
	return MakeShared<FHLMediaPlayer, ESPMode::ThreadSafe>(EventSink);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHLMediaModule, HLMedia)
