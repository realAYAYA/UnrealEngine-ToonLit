// Copyright Epic Games, Inc. All Rights Reserved.

#include "IKRigLogger.h"
#include "Logging/MessageLog.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#include "MessageLogModule.h"
#endif

DEFINE_LOG_CATEGORY(LogIKRig);

void FIKRigLogger::SetLogTarget(const FName InLogName, const FText& LogLabel)
{
	LogName = InLogName;

#if WITH_EDITOR
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	if (!MessageLogModule.IsRegisteredLogListing(InLogName))
	{
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowFilters = false;
		InitOptions.bShowPages = false;
		InitOptions.bAllowClear = false;
		InitOptions.bShowInLogWindow = false;
		InitOptions.bDiscardDuplicates = true;
		MessageLogModule.RegisterLogListing(InLogName, LogLabel, InitOptions);
	}
#endif
	
}

FName FIKRigLogger::GetLogTarget() const
{
	return LogName;
}
	
void FIKRigLogger::LogError(const FText& Message) const
{
	FMessageLog(LogName).SuppressLoggingToOutputLog(true).Error(Message);
	Errors.Add(Message);
}

void FIKRigLogger::LogWarning(const FText& Message) const
{
	FMessageLog(LogName).SuppressLoggingToOutputLog(true).Warning(Message);
	Warnings.Add(Message);
}

void FIKRigLogger::LogInfo(const FText& Message) const
{
	FMessageLog(LogName).SuppressLoggingToOutputLog(true).Info(Message);
	Messages.Add(Message);
}

void FIKRigLogger::Clear() const
{
	Errors.Empty();
	Warnings.Empty();
	Messages.Empty();
}
