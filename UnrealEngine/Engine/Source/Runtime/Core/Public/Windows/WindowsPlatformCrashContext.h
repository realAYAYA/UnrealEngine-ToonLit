// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Microsoft/MicrosoftPlatformCrashContext.h"

#if !defined(WINDOWS_CRASHCONTEXT_WITH_CUSTOM_HANDLERS)
	#define WINDOWS_CRASHCONTEXT_WITH_CUSTOM_HANDLERS 0
#endif


struct CORE_API FWindowsPlatformCrashContext : public FMicrosoftPlatformCrashContext
{
	static const TCHAR* const UEGPUAftermathMinidumpName;
	
	FWindowsPlatformCrashContext(ECrashContextType InType, const TCHAR* InErrorMessage)
		: FMicrosoftPlatformCrashContext(InType, InErrorMessage)
	{
	}

	
	virtual void AddPlatformSpecificProperties() const override;
	virtual void CopyPlatformSpecificFiles(const TCHAR* OutputDirectory, void* Context) override;
};

typedef FWindowsPlatformCrashContext FPlatformCrashContext;

