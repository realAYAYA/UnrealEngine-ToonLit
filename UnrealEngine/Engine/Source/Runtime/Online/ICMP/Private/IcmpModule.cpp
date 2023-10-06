// Copyright Epic Games, Inc. All Rights Reserved.

#include "IcmpModule.h"
#include "Icmp.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_MODULE(FIcmpModule, Icmp);

DEFINE_LOG_CATEGORY(LogIcmp);

void FIcmpModule::StartupModule()
{
}

void FIcmpModule::ShutdownModule()
{
}


bool FIcmpModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Icmp
	if (FParse::Command(&Cmd, TEXT("Icmp")))
	{
		if (FParse::Command(&Cmd, TEXT("Ping")))
		{
			FString Url;
			FParse::Token(Cmd, Url, true);
			if (Url.IsEmpty())
			{
				UE_LOG(LogConsoleResponse, Warning, TEXT("Url is missing, console command is Icmp Ping URL, defaulting to 127.0.0.1"));
				Url = TEXT("127.0.0.1");
			}
			FIcmp::IcmpEcho(Url, 1.0f, [](FIcmpEchoResult Result)
				{
					if (Result.Status == EIcmpResponseStatus::Success)
					{
						const double CurPing = static_cast<double>(Result.Time) * 1000.0;
						UE_LOG(LogConsoleResponse, Display, TEXT("Ping success: Reply from %s time=%.02f ms"), *Result.ReplyFrom, CurPing);
					}
					else
					{
						UE_LOG(LogConsoleResponse, Display, TEXT("Ping failure"));
					}
				});
			return true;
		}
	}
	return false;
}
