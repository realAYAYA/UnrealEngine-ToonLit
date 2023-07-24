// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizeProjectCommandlet.h"

#include "CommandletUtils.h"
#include "Virtualization/VirtualizationSystem.h"

UVirtualizeProjectCommandlet::UVirtualizeProjectCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

int32 UVirtualizeProjectCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVirtualizeProjectCommandlet);

	UE_LOG(LogVirtualization, Display, TEXT("Scanning for project packages..."));
	TArray<FString> Packages = UE::Virtualization::FindAllPackages();

	UE_LOG(LogVirtualization, Display, TEXT("Virtualizing packages..."));

	UE::Virtualization::IVirtualizationSystem& System = UE::Virtualization::IVirtualizationSystem::Get();
	UE::Virtualization::FVirtualizationResult Result = System.TryVirtualizePackages(Packages, UE::Virtualization::EVirtualizationOptions::Checkout);
	if (Result.WasSuccessful())
	{
		UE_LOG(LogVirtualization, Display, TEXT("Virtualized %d package(s)"), Result.VirtualizedPackages.Num());
		if (!Result.CheckedOutPackages.IsEmpty())
		{
			UE_LOG(LogVirtualization, Display, TEXT("Checked out %d package(s) from revision control"), Result.CheckedOutPackages.Num());
		}

		return 0;
	}
	else
	{
		return -1;
	}
}
