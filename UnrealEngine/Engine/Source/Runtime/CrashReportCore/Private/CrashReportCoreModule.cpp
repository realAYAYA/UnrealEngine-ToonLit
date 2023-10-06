// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrashReportCoreModule.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FCrashReportCoreModule, CrashReportCore);
DEFINE_LOG_CATEGORY(CrashReportCoreLog);

void FCrashReportCoreModule::StartupModule()
{
}

void FCrashReportCoreModule::ShutdownModule()
{
}
