// Copyright Epic Games, Inc. All Rights Reserved.

#include "RehydrateProjectCommandlet.h"

#include "CommandletUtils.h"
#include "Virtualization/VirtualizationSystem.h"

URehydrateProjectCommandlet::URehydrateProjectCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

int32 URehydrateProjectCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URehydrateProjectCommandlet);

	UE_LOG(LogVirtualization, Display, TEXT("Scanning for project packages..."));
	TArray<FString> Packages = UE::Virtualization::FindAllPackages();
	
	UE_LOG(LogVirtualization, Display, TEXT("Rehydrating packages..."));

	UE::Virtualization::IVirtualizationSystem& System = UE::Virtualization::IVirtualizationSystem::Get();
	UE::Virtualization::FRehydrationResult Result = System.TryRehydratePackages(Packages, UE::Virtualization::ERehydrationOptions::Checkout);
	if (Result.WasSuccessful())
	{
		UE_LOG(LogVirtualization, Display, TEXT("Rehydrated %d package(s)"), Result.RehydratedPackages.Num());
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
