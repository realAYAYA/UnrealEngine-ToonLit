// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHIShaderPlatform.h"
#include "RHIStrings.h"

RENDERCORE_API EShaderPlatform GetEditorShaderPlatform(EShaderPlatform ShaderPlatform);

template<typename Type>
struct FShaderPlatformCachedIniValue
{
	FShaderPlatformCachedIniValue(const TCHAR* InCVarName)
		: CVarName(InCVarName)
		, CVar(nullptr)
	{
	}

	FShaderPlatformCachedIniValue(IConsoleVariable* InCVar)
		: CVar(InCVar)
	{
	}

	Type Get(EShaderPlatform ShaderPlatform)
	{
		Type Value{};

		const EShaderPlatform ActualShaderPlatform = GetEditorShaderPlatform(ShaderPlatform);

		FName IniPlatformName = ShaderPlatformToPlatformName(ActualShaderPlatform);
		// find the cvar if needed
		if (CVar == nullptr)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
		}

		// if we are looking up our own platform, just use the current value, however
		// ShaderPlatformToPlatformName can return the wrong platform than expected - for instance, Linux Vulkan will return Windows
		// so instead of hitting an asser below, we detect that the request SP is the current SP, and use the CVar value that is set currently
		if (IniPlatformName == FPlatformProperties::IniPlatformName() || ActualShaderPlatform == GMaxRHIShaderPlatform)
		{
			checkf(CVar != nullptr, TEXT("Failed to find CVar %s when getting current value for FShaderPlatformCachedIniValue"), *CVarName);

			CVar->GetValue(Value);
			return Value;
		}

#if ALLOW_OTHER_PLATFORM_CONFIG
		// create a dummy cvar if needed
		if (CVar == nullptr)
		{
			// this could be a cvar that only exists on the target platform so create a dummy one
			CVar = IConsoleManager::Get().RegisterConsoleVariable(*CVarName, Type(), TEXT(""), ECVF_ReadOnly);
		}

		// now get the value from the platform that makes sense for this shader platform
		TSharedPtr<IConsoleVariable> OtherPlatformVar = CVar->GetPlatformValueVariable(IniPlatformName);
		ensureMsgf(OtherPlatformVar.IsValid(), TEXT("Failed to get another platform's version of a cvar (possible name: '%s'). It is probably an esoteric subclass that needs to implement GetPlatformValueVariable."), *CVarName);
		if (OtherPlatformVar.IsValid())
		{
			OtherPlatformVar->GetValue(Value);
		}
		else
		{
			// get this platform's value, even tho it could be wrong
			CVar->GetValue(Value);
		}
#else
		checkf(IniPlatformName == FName(FPlatformProperties::IniPlatformName()), TEXT("FShaderPlatformCachedIniValue can only look up the current platform when ALLOW_OTHER_PLATFORM_CONFIG is false"));
#endif
		return Value;
	}

private:
	FString CVarName;
	IConsoleVariable* CVar;
};
