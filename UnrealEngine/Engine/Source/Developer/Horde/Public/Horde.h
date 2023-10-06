// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

#if WITH_EDITOR

struct FHorde
{
	HORDE_API static FString GetTemplateName();
	HORDE_API static FString GetTemplateId();
	HORDE_API static FString GetJobId();
	HORDE_API static FString GetJobURL();
	HORDE_API static FString GetStepId();
	HORDE_API static FString GetStepURL();
	HORDE_API static FString GetStepName();
	HORDE_API static FString GetBatchId();
};

#endif
