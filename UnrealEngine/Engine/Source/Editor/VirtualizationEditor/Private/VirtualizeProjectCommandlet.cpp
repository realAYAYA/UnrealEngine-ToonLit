// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizeProjectCommandlet.h"

#include "CommandletUtils.h"
#include "Virtualization/VirtualizationSystem.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

UVirtualizeProjectCommandlet::UVirtualizeProjectCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

int32 UVirtualizeProjectCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVirtualizeProjectCommandlet);

	if (!ParseCmdline(Params))
	{
		UE_LOG(LogVirtualization, Error, TEXT("Failed to parse the command line correctly"));
		return -1;
	}

	UE_LOG(LogVirtualization, Display, TEXT("Scanning for project packages..."));
	TArray<FString> Packages = UE::Virtualization::DiscoverPackages(Params, UE::Virtualization::EFindPackageFlags::ExcludeEngineContent);
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

		if (bAutoCheckIn && !Result.CheckedOutPackages.IsEmpty())
		{
			UE_LOG(LogVirtualization, Display, TEXT("Attempting to check in modified packages to revision control..."));

			TArray<FString> CheckedOutPackages = SourceControlHelpers::PackageFilenames(Result.CheckedOutPackages);

			FTextBuilder DescriptionBuilder;
			DescriptionBuilder.AppendLine(FText::FromString(TEXT("Virtualizing project packages - automated submit from the VirtualizeProject commandlet")));

			for (const FText& Line : Result.DescriptionTags)
			{
				DescriptionBuilder.AppendLine(Line);
			}

			const FText CheckInDescription = DescriptionBuilder.ToText();

			TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
			CheckInOperation->SetDescription(CheckInDescription);

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			if (SourceControlProvider.Execute(CheckInOperation, CheckedOutPackages) == ECommandResult::Succeeded)
			{
				UE_LOG(LogVirtualization, Display, TEXT("Success! - %s"), *CheckInOperation->GetSuccessMessage().ToString());
			}
			else
			{
				// The FCheckIn operation tends to log error messages so we don't need to extract the result info
				// from the operation.
				UE_LOG(LogVirtualization, Error, TEXT("Failed to check in the package(s) see the above LogSourceControl errors"));
			}
		}

		return 0;
	}
	else
	{
		return -1;
	}
}

bool UVirtualizeProjectCommandlet::ParseCmdline(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	bAutoCheckIn = Switches.Contains(TEXT("AutoCheckIn"));

	return true;
}
