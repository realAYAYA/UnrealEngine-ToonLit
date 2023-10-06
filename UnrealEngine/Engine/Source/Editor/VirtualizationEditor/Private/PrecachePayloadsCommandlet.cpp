// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrecachePayloadsCommandlet.h"

#include "CommandletUtils.h"
#include "UObject/PackageTrailer.h"
#include "Virtualization/VirtualizationSystem.h"

UPrecachePayloadsCommandlet::UPrecachePayloadsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UPrecachePayloadsCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPrecachePayloadsCommandlet);

	TArray<FString> PackageNames = UE::Virtualization::FindPackages(UE::Virtualization::EFindPackageFlags::ExcludeEngineContent);

	UE_LOG(LogVirtualization, Display, TEXT("Found %d packages"), PackageNames.Num());

	TArray<FIoHash> PayloadIds = UE::Virtualization::FindVirtualizedPayloads(PackageNames);

	if (PayloadIds.IsEmpty())
	{
		UE_LOG(LogVirtualization, Display, TEXT("No virtualized payloads found"));
		return 0;
	}

	UE_LOG(LogVirtualization, Display, TEXT("Found %d virtualized payloads to precache"), PayloadIds.Num());
	UE_LOG(LogVirtualization, Display, TEXT("Precaching payloads..."));
	
	const int32 BatchSize = 64;
	UE::Virtualization::PullPayloadsThreaded(PayloadIds, BatchSize, TEXT("Cached"), [](const UE::Virtualization::FPullRequest& Request)
		{
			if (!Request.IsSuccess())
			{
				UE_LOG(LogVirtualization, Error, TEXT("%s: Failed to precache payload"), *LexToString(Request.GetIdentifier()));
			}
		});

	UE_LOG(LogVirtualization, Display, TEXT("Precaching complete!"));

	UE::Virtualization::IVirtualizationSystem::Get().DumpStats();
	
	return  0;
}
