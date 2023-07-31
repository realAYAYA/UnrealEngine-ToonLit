// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaModule.h"

#include "Modules/ModuleManager.h"
#include "RivermaxMediaPlayer.h"



void FRivermaxMediaModule::StartupModule()
{
}

void FRivermaxMediaModule::ShutdownModule()
{

}

TSharedPtr<IMediaPlayer> FRivermaxMediaModule::CreatePlayer(IMediaEventSink& EventSink)
{
	return MakeShared<UE::RivermaxMedia::FRivermaxMediaPlayer>(EventSink);
}

IMPLEMENT_MODULE(FRivermaxMediaModule, RivermaxMedia);
