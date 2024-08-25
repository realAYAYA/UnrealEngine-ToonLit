// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/SavePackage.h"

#if UE_WITH_SAVEPACKAGE
#include "Misc/Optional.h"
#include "UObject/ArchiveCookContext.h"

#if WITH_EDITOR

COREUOBJECT_API extern bool GOutputCookingWarnings;

#endif

FSavePackageResultStruct UPackage::Save(UPackage* InOuter, UObject* InAsset, const TCHAR* Filename,
	const FSavePackageArgs& SaveArgs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackage::Save);

	return UPackage::Save2(InOuter, InAsset, Filename, SaveArgs);
}

bool UPackage::SavePackage(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
	FOutputDevice* Error, FLinkerNull* Conform, bool bForceByteSwapping, bool bWarnOfLongFilename, uint32 SaveFlags,
	const ITargetPlatform* TargetPlatform, const FDateTime& FinalTimeStamp, bool bSlowTask)
{
	// CookData should only be nonzero if we are cooking.
	TOptional<FArchiveCookContext> CookContext;
	TOptional<FArchiveCookData> CookData;
	if (TargetPlatform != nullptr)
	{
		CookContext.Emplace(InOuter, UE::Cook::ECookType::Unknown,
			UE::Cook::ECookingDLC::Unknown, TargetPlatform);
		CookData.Emplace(*TargetPlatform, *CookContext);
	}
	FSavePackageArgs SaveArgs = { nullptr /* deprecated target platform */, CookData.GetPtrOrNull(), TopLevelFlags, SaveFlags, bForceByteSwapping,
		bWarnOfLongFilename, bSlowTask, FinalTimeStamp, Error };
	return SavePackage(InOuter, Base, Filename, SaveArgs);
}

bool UPackage::SavePackage(UPackage* InOuter, UObject* InAsset, const TCHAR* Filename, const FSavePackageArgs& SaveArgs)
{
	const FSavePackageResultStruct Result = Save(InOuter, InAsset, Filename, SaveArgs);
	return Result == ESavePackageResult::Success;
}

FSavePackageContext::~FSavePackageContext()
{
	delete PackageWriter;
}

#endif	// UE_WITH_SAVEPACKAGE
