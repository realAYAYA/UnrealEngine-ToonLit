// Copyright Epic Games, Inc. All Rights Reserved.

#include "Horde.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_EDITOR

FString FHorde::GetTemplateName()
{
	static FString TemplateName;

	if (TemplateName.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeTemplateName="), TemplateName))
		{
			TemplateName = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_TEMPLATENAME"));
		}
	}
	return TemplateName;
}

FString FHorde::GetTemplateId()
{
	static FString TemplateId;

	if (TemplateId.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeTemplateId="), TemplateId))
		{
			TemplateId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_TEMPLATEID"));
		}
	}

	return TemplateId;
}

FString FHorde::GetJobId()
{
	static FString JobId;

	if (JobId.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeJobId="), JobId))
		{
			JobId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_JOBID"));
		}
	}

	return JobId;
}

FString FHorde::GetJobURL()
{
	return FString::Printf(TEXT("https://horde.devtools.epicgames.com/job/%s"), *GetJobId());
}

FString FHorde::GetStepId()
{
	static FString StepId;

	if (StepId.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeStepId="), StepId))
		{
			StepId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_STEPID"));
		}
	}
	return StepId;
}

FString FHorde::GetStepURL()
{
	return FString::Printf(TEXT("%s?step=%s"), *GetJobURL(), *GetStepId());
}

FString FHorde::GetStepName()
{
	FString StepName;

	if (StepName.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeStepName="), StepName))
		{
			StepName = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_STEPNAME"));
		}
	}
	return StepName;
}


FString FHorde::GetBatchId()
{
	FString BatchId;
	if (BatchId.IsEmpty())
	{
		if (false == FParse::Value(FCommandLine::Get(), TEXT("HordeBatchId="), BatchId))
		{
			BatchId = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_HORDE_BATCHID"));
		}
	}
	return BatchId;
} 

#endif