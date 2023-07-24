// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxCoreModule.h"

#include "Modules/ModuleManager.h"
#include "RivermaxInputStream.h"
#include "RivermaxManager.h"
#include "RivermaxOutputStream.h"


void FRivermaxCoreModule::StartupModule()
{
	RivermaxManager = MakeShared<UE::RivermaxCore::Private::FRivermaxManager>();
}

void FRivermaxCoreModule::ShutdownModule()
{

}

TUniquePtr<UE::RivermaxCore::IRivermaxInputStream> FRivermaxCoreModule::CreateInputStream()
{
	using UE::RivermaxCore::Private::FRivermaxInputStream;
	return MakeUnique<FRivermaxInputStream>();
}

TUniquePtr<UE::RivermaxCore::IRivermaxOutputStream> FRivermaxCoreModule::CreateOutputStream()
{
	using UE::RivermaxCore::Private::FRivermaxOutputStream;
	return MakeUnique<FRivermaxOutputStream>();
}

TSharedPtr<UE::RivermaxCore::IRivermaxManager> FRivermaxCoreModule::GetRivermaxManager()
{
	return RivermaxManager;
}

IMPLEMENT_MODULE(FRivermaxCoreModule, RivermaxCore);
