// Copyright Epic Games, Inc. All Rights Reserved.

#include "IKRigLogger.h"

#include "Logging/MessageLog.h"

DEFINE_LOG_CATEGORY(LogIKRig);

void FIKRigLogger::SetLogTarget(const FName InLogName)
{
	LogName = InLogName;
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
