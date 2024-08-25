// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/IterativeValidatePackageWriter.h"

#include "CookOnTheSide/CookLog.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogIterativeValidate, Log, All);

constexpr FStringView IterativeValidateFilename(TEXTVIEW("IterativeValidate.bin"));

FIterativeValidatePackageWriter::FIterativeValidatePackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner,
	EPhase InPhase, const FString& ResolvedMetadataPath)
	: FDiffPackageWriter(MoveTemp(InInner))
	, MetadataPath(ResolvedMetadataPath)
	, Phase(InPhase)
{
	Indent = FCString::Spc(FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Warning,
		LogIterativeValidate.GetCategoryName(), TEXT(""), GPrintLogTimes).Len());
}

void FIterativeValidatePackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	bPackageFirstPass = true;
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		if (IterativelyUnmodified.Contains(Info.PackageName))
		{
			// Save to memory and look for diffs before saving it out to disk
			SaveAction = ESaveAction::CheckForDiffs;
			Super::BeginPackage(Info);
		}
		else
		{
			// Not iteratively skippable, so we expect it to change. No need to look for diffs, just save to disk.
			++ModifiedCount;
			if (bReadOnly)
			{
				SaveAction = ESaveAction::IgnoreResults;
			}
			else
			{
				SaveAction = ESaveAction::SaveToInner;
				Inner->BeginPackage(Info);
			}
		}
		break;
	case EPhase::Phase1:
		SaveAction = ESaveAction::CheckForDiffs;
		Super::BeginPackage(Info);
		break;
	case EPhase::Phase2:
		if (IterativeFailed.Contains(Info.PackageName))
		{
			SaveAction = ESaveAction::CheckForDiffs;
			Super::BeginPackage(Info);
		}
		else
		{
			// This is not an IterativeValidated package (because we don't save those; UpdatePackageModificationStatus
			// prevents saving it). And it is not an IterativeFailed package (checked above), so it is an
			// IterativeModified package. It was found during Phase1 to be modified and would in a
			// normal iterative cook be resaved rather than iteratively skipped. Resave it as normal.
			SaveAction = ESaveAction::SaveToInner;
			Inner->BeginPackage(Info);
		}
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::CommitPackage(FCommitPackageInfo&& Info)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::CommitPackage(MoveTemp(Info));
		break;
	case ESaveAction::SaveToInner:
		Inner->CommitPackage(MoveTemp(Info));
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
	const TArray<FFileRegion>& FileRegions)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::WritePackageData(Info, ExportsArchive, FileRegions);
		break;
	case ESaveAction::SaveToInner:
		Inner->WritePackageData(Info, ExportsArchive, FileRegions);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::WriteBulkData(const FBulkDataInfo& Info, const FIoBuffer& BulkData,
	const TArray<FFileRegion>& FileRegions)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::WriteBulkData(Info, BulkData, FileRegions);
		break;
	case ESaveAction::SaveToInner:
		Inner->WriteBulkData(Info, BulkData, FileRegions);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::WriteAdditionalFile(const FAdditionalFileInfo& Info, const FIoBuffer& FileData)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::WriteAdditionalFile(Info, FileData);
		break;
	case ESaveAction::SaveToInner:
		Inner->WriteAdditionalFile(Info, FileData);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}
void FIterativeValidatePackageWriter::WriteLinkerAdditionalData(const FLinkerAdditionalDataInfo& Info,
	 const FIoBuffer& Data, const TArray<FFileRegion>& FileRegions)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::WriteLinkerAdditionalData(Info, Data, FileRegions);
		break;
	case ESaveAction::SaveToInner:
		Inner->WriteLinkerAdditionalData(Info, Data, FileRegions);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::WritePackageTrailer(const FPackageTrailerInfo& Info, const FIoBuffer& Data)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::WritePackageTrailer(Info, Data);
		break;
	case ESaveAction::SaveToInner:
		Inner->WritePackageTrailer(Info, Data);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

int64 FIterativeValidatePackageWriter::GetExportsFooterSize()
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		return Super::GetExportsFooterSize();
	case ESaveAction::SaveToInner:
		return Inner->GetExportsFooterSize();
	case ESaveAction::IgnoreResults:
		return 0;
	default:
		checkNoEntry();
		return 0;
	}
}

TUniquePtr<FLargeMemoryWriter> FIterativeValidatePackageWriter::CreateLinkerArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		return Super::CreateLinkerArchive(PackageName, Asset, MultiOutputIndex);
	case ESaveAction::SaveToInner:
		return Inner->CreateLinkerArchive(PackageName, Asset, MultiOutputIndex);
	case ESaveAction::IgnoreResults:
		return MakeUnique<FLargeMemoryWriter>();
	default:
		checkNoEntry();
		return TUniquePtr<FLargeMemoryWriter>();
	}
}

TUniquePtr<FLargeMemoryWriter> FIterativeValidatePackageWriter::CreateLinkerExportsArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		return Super::CreateLinkerExportsArchive(PackageName, Asset, MultiOutputIndex);
	case ESaveAction::SaveToInner:
		return Inner->CreateLinkerExportsArchive(PackageName, Asset, MultiOutputIndex);
	case ESaveAction::IgnoreResults:
		return MakeUnique<FLargeMemoryWriter>();
	default:
		checkNoEntry();
		return TUniquePtr<FLargeMemoryWriter>();
	}
}

bool FIterativeValidatePackageWriter::IsPreSaveCompleted() const
{
	return !bPackageFirstPass;
}

ICookedPackageWriter::FCookCapabilities FIterativeValidatePackageWriter::GetCookCapabilities() const
{
	FCookCapabilities Result = Super::GetCookCapabilities();
	Result.bReadOnly = bReadOnly;
	return Result;
}

void FIterativeValidatePackageWriter::Initialize(const FCookInfo& CookInfo)
{
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		if (CookInfo.bFullBuild)
		{
			UE_LOG(LogIterativeValidate, Display,
				TEXT("The cook is running non-iteratively. All packages are reported \"modified\" and will be resaved as in a normal cook."));
			bReadOnly = false;
		}
		else
		{
			bReadOnly = !FParse::Param(FCommandLine::Get(), TEXT("iterativevalidateallowwrite"));
		}
		break;
	case EPhase::Phase1:
		if (CookInfo.bFullBuild)
		{
			UE_LOG(LogIterativeValidate, Display,
				TEXT("The cook is running non-iteratively. All packages are reported \"modified\" and will be resaved during the final IterativeValidate phase."));
		}
		bReadOnly = false;
		break;
	case EPhase::Phase2:
		if (CookInfo.bFullBuild)
		{
			UE_LOG(LogIterativeValidate, Display,
				TEXT("The cook is running non-iteratively. Packages that were iteratively skipped and found valid will be resaved anyway."));
		}
		bReadOnly = false;
		break;
	default:
		checkNoEntry();
		break;
	}
	Super::Initialize(CookInfo);
}

void FIterativeValidatePackageWriter::UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
	bool& bInOutShouldIterativelySkip)
{
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		// Save the input value for bIterativelyUnmodified, and report skippable if and only if !unmodified and we're readonly.
		if (bIterativelyUnmodified)
		{
			IterativelyUnmodified.Add(PackageName);
			bInOutShouldIterativelySkip = false;
		}
		else
		{
			if (bReadOnly)
			{
				bInOutShouldIterativelySkip = true;
				++ModifiedCount; // Only increment here if we're skipping it. Otherwise it is incremented in BeginPackage
			}
		}
		break;
	case EPhase::Phase1:
		// Invert what gets skipped: save the iteratively skipped files to record their diffs, but skip the regular files
		bInOutShouldIterativelySkip = !bIterativelyUnmodified;
		if (!bIterativelyUnmodified)
		{
			++ModifiedCount;
		}
		break;
	case EPhase::Phase2:
		// Ignore the Unmodified flag from this cook phase. Skip only the packages that were found to 
		// be IterativeValidated from Phase1.
		bInOutShouldIterativelySkip = IterativeValidated.Contains(PackageName);
		break;
	default:
		checkNoEntry();
		break;
	}

	bool bInnerIterativelyUnmodified = bInOutShouldIterativelySkip;
	bool bInnerInOutShouldIterativelySkip = bInOutShouldIterativelySkip;
	Inner->UpdatePackageModificationStatus(PackageName, bInnerIterativelyUnmodified, bInnerInOutShouldIterativelySkip);
	checkf(bInnerInOutShouldIterativelySkip == bInnerIterativelyUnmodified,
		TEXT("IterativeValidatePackageWriter is not supported with an Inner that modifies bInOutShouldIterativelySkip."));
}

void FIterativeValidatePackageWriter::BeginCook(const FCookInfo& Info)
{
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		if (bReadOnly)
		{
			UE_LOG(LogIterativeValidate, Display,
				TEXT("-IterativeValidateAllowWrite not present, read-only mode. Running -diffonly on all packages that were found to be iteratively unmodified."));
		}
		else
		{
			UE_LOG(LogIterativeValidate, Display,
				TEXT("-IterativeValidateAllowWrite is present, writable mode. Resaving packages as in a normal cook, but also running -diffonly on all packages that were found to be iteratively unmodified."));
		}
		break;
	case EPhase::Phase1:
		UE_LOG(LogIterativeValidate, Display,
			TEXT("Phase1: running -diffonly and a resave on all packages discovered to be iteratively unmodified."));
		break;
	case EPhase::Phase2:
		Load();
		UE_LOG(LogIterativeValidate, Display,
			TEXT("Phase2: %d packages were found during Phase1 to be iteratively unmodified but had differences. Running -diffonly on them again to check whether the differences are due to indeterminism or to IterativeFalsePositives."), IterativeFailed.Num());
		UE_LOG(LogIterativeValidate, Display,
			TEXT("%d packages were found during Phase1 to be modified or new and will be resaved."),
			ModifiedCount);
		break;
	default:
		checkNoEntry();
		break;
	}
	Super::BeginCook(Info);
}

void FIterativeValidatePackageWriter::EndCook(const FCookInfo& Info)
{
	Super::EndCook(Info);
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
	{
		UE_LOG(LogIterativeValidate, Display,
			TEXT("Modified: %d. DetectedUnmodified: %d. ValidatedUnmodified: %d. IterativeFalseNegative: %d."),
			ModifiedCount, IterativeValidated.Num() + IterativeFalseNegative.Num(), IterativeValidated.Num(), IterativeFalseNegative.Num());
		FString Message = FString::Printf(TEXT("IterativeFalseNegative: %d."), IterativeFailed.Num());
		if (IterativeFalseNegative.Num())
		{
			UE_LOG(LogIterativeValidate, Error, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogIterativeValidate, Display, TEXT("%s"), *Message);
		}
		break;
	}
	case EPhase::Phase1:
		UE_LOG(LogIterativeValidate, Display,
			TEXT("Modified: %d. DetectedUnmodified: %d. ValidatedUnmodified: %d. IterativeFalseNegativeOrIndeterminism: %d."),
			ModifiedCount, IterativeValidated.Num() + IterativeFailed.Num(), IterativeValidated.Num(), IterativeFailed.Num());
		Save();
		break;
	case EPhase::Phase2:
	{
		UE_LOG(LogIterativeValidate, Display,
			TEXT("Modified: %d. DetectedUnmodified: %d. ValidatedUnmodified: %d. Indeterminism: %d."),
			ModifiedCount, IterativeValidated.Num() + IterativeFailed.Num(), IterativeValidated.Num(), IndeterminismFailed.Num());
		FString Message = FString::Printf(TEXT("IterativeFalseNegative: %d."), IterativeFalseNegative.Num());
		if (IterativeFalseNegative.Num())
		{
			UE_LOG(LogIterativeValidate, Error, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogIterativeValidate, Display, TEXT("%s"), *Message);
		}
		break;
	}
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::UpdateSaveArguments(FSavePackageArgs& SaveArgs)
{
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		Super::UpdateSaveArguments(SaveArgs);
		break;
	case ESaveAction::SaveToInner:
		Inner->UpdateSaveArguments(SaveArgs);
		break;
	case ESaveAction::IgnoreResults:
		break;
	default:
		checkNoEntry();
		break;
	}
}

bool FIterativeValidatePackageWriter::IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs)
{
	bPackageFirstPass = false;
	checkf(!Inner->IsAnotherSaveNeeded(PreviousResult, SaveArgs),
		TEXT("IterativeValidatePackageWriter does not support an Inner that needs multiple saves."));
	if (PreviousResult == ESavePackageResult::Timeout)
	{
		return false;
	}
	switch (SaveAction)
	{
	case ESaveAction::CheckForDiffs:
		break;
	case ESaveAction::SaveToInner:
		// The SaveToInner pass, if present, is the last pass in a phase
		return false;
	case ESaveAction::IgnoreResults:
		// The IgnoreResults pass, if present, is the last pass in a phase
		return false;
	default:
		checkNoEntry();
		break;
	}

	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		check(IterativelyUnmodified.Contains(BeginInfo.PackageName)); // If !Contains, then we would have set SaveAction=SaveToInner or IgnoreResults and early exited above
		if (Super::IsAnotherSaveNeeded(PreviousResult, SaveArgs))
		{
			return true;
		}
		else
		{
			// Once our superclass has finished looking for differences, finish it off and start a SaveToInner pass
			FCommitPackageInfo CommitInfo;
			CommitInfo.Status = IPackageWriter::ECommitStatus::Success;
			CommitInfo.PackageName = BeginInfo.PackageName;
			CommitInfo.WriteOptions = EWriteOptions::None;
			Super::CommitPackage(MoveTemp(CommitInfo));

			if (bIsDifferent && !bNewPackage)
			{
				IterativeFalseNegative.Add(BeginInfo.PackageName);
			}
			else if (!bNewPackage)
			{
				IterativeValidated.Add(BeginInfo.PackageName);
			}
			else
			{
				++ModifiedCount;
			}

			if (bReadOnly)
			{
				SaveAction = ESaveAction::IgnoreResults;
				return false;
			}
			else
			{
				Inner->BeginPackage(BeginInfo);
				SaveAction = ESaveAction::SaveToInner;
				return true;
			}
		}
	case EPhase::Phase1:
		if (Super::IsAnotherSaveNeeded(PreviousResult, SaveArgs))
		{
			return true;
		}
		else if (bIsDifferent && !bNewPackage)
		{
			// If our superclass FDiffPackageWriter found differences, when it finishes the saves it wants to do,
			// Finish it off and start a SaveToInner pass
			FCommitPackageInfo CommitInfo;
			CommitInfo.Status = IPackageWriter::ECommitStatus::Success;
			CommitInfo.PackageName = BeginInfo.PackageName;
			CommitInfo.WriteOptions = EWriteOptions::None;
			Super::CommitPackage(MoveTemp(CommitInfo));
			Inner->BeginPackage(BeginInfo);
			SaveAction = ESaveAction::SaveToInner;

			// Mark that the iterative validation failed if it was not already marked by log or warning messages.
			// We need to record it for an indeterminism test
			IterativeFailed.FindOrAdd(BeginInfo.PackageName);
			return true;
		}
		else if (!bNewPackage)
		{
			// No differences found, so finish off the superclass's save during CommitPackage, without doing a
			// SaveToInner pass
			// Mark that the iterative validation passed
			IterativeValidated.Add(BeginInfo.PackageName);
			TArray<FMessage> Messages;
			IterativeFailed.RemoveAndCopyValue(BeginInfo.PackageName, Messages);
			for (FMessage& Message : Messages)
			{
				// If no differences were detected, we should not have logged any warning or error messages
				check(Message.Verbosity > ELogVerbosity::Warning);
			}
			return false;
		}
		else
		{
			// New packages need to be resaved in Phase2; for our purposes they are equivalent
			// to a package that iteration detected as modified.
			// Do not add an entry for it in our results for iterative packages, and do not resave it in this pass
			++ModifiedCount;
			return false;
		}
	case EPhase::Phase2:
		LogIterativeDifferences();
		// No need to do the Super's second diff pass to find callstacks - just knowing whether differences exist is enough.
		// No need to save package to disk; for these packages (packages found to be iteratively unmodified during
		// Phase1) they were already resaved during Phase1
		return false;
	default:
		checkNoEntry();
		return false;
	}
}

void FIterativeValidatePackageWriter::OnDiffWriterMessage(ELogVerbosity::Type Verbosity, FStringView Message)
{
	switch (Phase)
	{
	case EPhase::AllInOnePhase:
		FMsg::Logf(__FILE__, __LINE__, LogIterativeValidate.GetCategoryName(), Verbosity,
			TEXT("%s"), *ResolveText(Message));
		break;
	case EPhase::Phase1:
		IterativeFailed.FindOrAdd(BeginInfo.PackageName).Add(FMessage{ FString(Message), Verbosity });
		break;
	case EPhase::Phase2:
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FIterativeValidatePackageWriter::LogIterativeDifferences()
{
	// This function is called during Phase2 for packages that had differences during Phase1.
	// It is called immediately after first-pass package save that is run by our super class FDiffPackageWriter, which
	// compared it against the version that was written to disk during Phase1. If there are differences
	// now from Phase1, then this package has a determinism issue. We log that information at display rather
	// than Warning because this cookmode only logs warnings for IterativeFalseNegatives.
	bool bHasDeterminismIssue = bIsDifferent;
	if (bHasDeterminismIssue)
	{
		UE_LOG(LogIterativeValidate, Display, TEXT("Could not validate %s because it has a non-deterministic save."),
			*BeginInfo.PackageName.ToString());
		IndeterminismFailed.Add(BeginInfo.PackageName);
		return;
	}

	// Otherwise, no determinism issues, so the differences indicate a bug in Diff Package
	IterativeFalseNegative.Add(BeginInfo.PackageName);
	FMsg::Logf(__FILE__, __LINE__, LogIterativeValidate.GetCategoryName(), ELogVerbosity::Warning,
		TEXT("IterativeFalseNegative package %s."), *BeginInfo.PackageName.ToString());
	TArray<FMessage>& Messages = IterativeFailed.FindOrAdd(BeginInfo.PackageName);
	for (const FMessage& Message : Messages)
	{
		FMsg::Logf(__FILE__, __LINE__, LogIterativeValidate.GetCategoryName(), Message.Verbosity,
			TEXT("%s"), *ResolveText(Message.Text));
	}
}

void FIterativeValidatePackageWriter::Save()
{
	FString IterativeValidatePath = GetIterativeValidatePath();
	TUniquePtr<FArchive> DiskArchive(IFileManager::Get().CreateFileWriter(*IterativeValidatePath));

	if (!DiskArchive)
	{
		UE_LOG(LogIterativeValidate, Error,
			TEXT("Could not write to file %s. This file is needed to store results for the -IterativeValidate cook."),
			*IterativeValidatePath);
		return;
	}
	FNameAsStringProxyArchive Ar(*DiskArchive);
	Serialize(Ar);
}

void FIterativeValidatePackageWriter::Load()
{
	FString IterativeValidatePath = GetIterativeValidatePath();
	TUniquePtr<FArchive> DiskArchive(IFileManager::Get().CreateFileReader(*IterativeValidatePath));
	if (!DiskArchive)
	{
		UE_LOG(LogIterativeValidate, Fatal,
			TEXT("Could not load file %s. This file is required and should have been written by the -IterativeValidatePhase1 cook."),
			*IterativeValidatePath);
		return;
	}
	FNameAsStringProxyArchive Ar(*DiskArchive);
	Serialize(Ar);
	if (Ar.IsError())
	{
		UE_LOG(LogIterativeValidate, Fatal, TEXT("Corrupt file %s"), *IterativeValidatePath);
	}
}

void FIterativeValidatePackageWriter::Serialize(FArchive& Ar)
{
	constexpr int32 LatestVersion = 0;
	int32 Version = LatestVersion;
	Ar << Version;
	if (Ar.IsLoading() && Version != LatestVersion)
	{
		Ar.SetError();
		return;
	}
	Ar << IterativeValidated;
	Ar << IterativeFailed;
	Ar << ModifiedCount;
}

FArchive& operator<<(FArchive& Ar, FIterativeValidatePackageWriter::FMessage& Message)
{
	uint8 Verbosity = static_cast<uint8>(Message.Verbosity);
	Ar << Verbosity << Message.Text;
	if (Ar.IsLoading())
	{
		Message.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity);
	}
	return Ar;
}

FString FIterativeValidatePackageWriter::GetIterativeValidatePath() const
{
	return FPaths::Combine(MetadataPath, FString(IterativeValidateFilename));
}
