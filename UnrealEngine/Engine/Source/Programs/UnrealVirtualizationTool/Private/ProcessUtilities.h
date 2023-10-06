// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "UnrealVirtualizationTool.h"

namespace UE::Virtualization
{

/** Wrapper around stdin and stdout pipes created by a call to FPlatformProcess::CreatePipe */
struct FProcessPipes
{
	FProcessPipes()
	{
		verify(FPlatformProcess::CreatePipe(StdOut, StdIn));
	}

	~FProcessPipes()
	{
		FPlatformProcess::ClosePipe(StdOut, StdIn);
	}

	void* GetStdIn() const
	{
		return StdIn;
	}

	void* GetStdOut() const
	{
		return StdOut;
	}

	void ProcessStdOut()
	{
		check(StdOut != nullptr);

		FString Output = FPlatformProcess::ReadPipe(StdOut);

		while (!Output.IsEmpty())
		{
			TArray<FString> Lines;
			Output.ParseIntoArray(Lines, LINE_TERMINATOR);

			for (const FString& Line : Lines)
			{
				UE_LOG(LogVirtualizationTool, Display, TEXT("Child Process-> %s"), *Line);
			}

			Output = FPlatformProcess::ReadPipe(StdOut);
		}
	}

private:
	void* StdIn = nullptr;
	void* StdOut = nullptr;
};

} // namespace UE::Virtualization