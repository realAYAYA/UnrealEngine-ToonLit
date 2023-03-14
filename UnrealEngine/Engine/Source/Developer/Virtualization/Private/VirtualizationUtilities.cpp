// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizationUtilities.h"

#include "IO/IoHash.h"
#include "Misc/StringBuilder.h"

namespace UE::Virtualization::Utils
{

void PayloadIdToPath(const FIoHash& Id, FStringBuilderBase& OutPath)
{
	OutPath.Reset();
	OutPath << Id;

	TStringBuilder<10> Directory;
	Directory << OutPath.ToView().Left(2) << TEXT("/");
	Directory << OutPath.ToView().Mid(2, 2) << TEXT("/");
	Directory << OutPath.ToView().Mid(4, 2) << TEXT("/");

	OutPath.ReplaceAt(0, 6, Directory);

	OutPath << TEXT(".upayload");
}

FString PayloadIdToPath(const FIoHash& Id)
{
	TStringBuilder<52> Path;
	PayloadIdToPath(Id, Path);

	return FString(Path);
}

void GetFormattedSystemError(FStringBuilderBase& SystemErrorMessage)
{
	SystemErrorMessage.Reset();

	const uint32 SystemError = FPlatformMisc::GetLastError();
	// If we have a system error we can give a more informative error message but don't output it if the error is zero as 
	// this can lead to very confusing error messages.
	if (SystemError != 0)
	{
		TCHAR SystemErrorMsg[MAX_SPRINTF] = { 0 };
		FPlatformMisc::GetSystemErrorMessage(SystemErrorMsg, sizeof(SystemErrorMsg), SystemError);

		SystemErrorMessage.Appendf(TEXT("'%s' (%d)"), SystemErrorMsg, SystemError);
	}
	else
	{
		SystemErrorMessage << TEXT("'unknown reason' (0)");
	}
}

} // namespace UE::Virtualization::Utils
