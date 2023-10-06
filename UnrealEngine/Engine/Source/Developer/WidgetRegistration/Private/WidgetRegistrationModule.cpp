// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetRegistrationModule.h"
#include "ToolkitStyle.h"


void FWidgetRegistrationModule::StartupModule()
{
	FToolkitStyle::Initialize();
}
	
void FWidgetRegistrationModule::ShutdownModule()
{
	FToolkitStyle::Shutdown();
}


