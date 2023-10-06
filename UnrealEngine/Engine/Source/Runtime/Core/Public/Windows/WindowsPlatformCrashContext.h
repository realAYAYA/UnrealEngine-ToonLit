// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Microsoft/MicrosoftPlatformCrashContext.h"


struct FWindowsPlatformCrashContext : public FMicrosoftPlatformCrashContext
{
	static CORE_API const TCHAR* const UEGPUAftermathMinidumpName;
	
	FWindowsPlatformCrashContext(ECrashContextType InType, const TCHAR* InErrorMessage)
		: FMicrosoftPlatformCrashContext(InType, InErrorMessage)
	{
	}

	
	CORE_API virtual void AddPlatformSpecificProperties() const override;
	CORE_API virtual void CopyPlatformSpecificFiles(const TCHAR* OutputDirectory, void* Context) override;
};

typedef FWindowsPlatformCrashContext FPlatformCrashContext;

