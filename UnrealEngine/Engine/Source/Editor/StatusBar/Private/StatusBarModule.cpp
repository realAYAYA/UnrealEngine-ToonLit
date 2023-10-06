// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatusBarModule.h"


class FStatusBarModule : public IStatusBarModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FStatusBarModule, StatusBar )



void FStatusBarModule::StartupModule()
{

}


void FStatusBarModule::ShutdownModule()
{

}



