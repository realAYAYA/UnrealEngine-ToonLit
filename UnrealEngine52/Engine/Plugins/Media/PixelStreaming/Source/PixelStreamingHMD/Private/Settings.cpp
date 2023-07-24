// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings.h"
#include "Misc/DefaultValueHelper.h"

namespace UE::PixelStreamingHMD::Settings
{
	void CommandLineParseOption(const TCHAR* Match, TAutoConsoleVariable<bool>& CVar)
	{
		FString ValueMatch(Match);
		ValueMatch.Append(TEXT("="));
		FString Value;
		if (FParse::Value(FCommandLine::Get(), *ValueMatch, Value))
		{
			if (Value.Equals(FString(TEXT("true")), ESearchCase::IgnoreCase))
			{
				CVar->Set(true, ECVF_SetByCommandline);
			}
			else if (Value.Equals(FString(TEXT("false")), ESearchCase::IgnoreCase))
			{
				CVar->Set(false, ECVF_SetByCommandline);
			}
		}
		else if (FParse::Param(FCommandLine::Get(), Match))
		{
			CVar->Set(true, ECVF_SetByCommandline);
		}
	}

	TAutoConsoleVariable<bool> CVarPixelStreamingEnableHMD(
			TEXT("PixelStreaming.HMD.Enable"),
			false,
			TEXT("Enables HMD specific functionality for Pixel Streaming. Namely input handling and stereoscopic rendering"),
			ECVF_Default);
	

	void InitialiseSettings()
	{
		CommandLineParseOption(TEXT("PixelStreamingEnableHMD"), CVarPixelStreamingEnableHMD);
	}
} // UE::PixelStreamingHMD::Settings