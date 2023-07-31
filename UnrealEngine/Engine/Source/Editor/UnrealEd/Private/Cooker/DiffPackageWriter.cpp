// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/DiffPackageWriter.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CookOnTheSide/CookLog.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CString.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Parse.h"
#include "Misc/OutputDevice.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/LinkerDiff.h"
#include "UObject/LinkerSave.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

FDiffPackageWriter::FDiffPackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner)
	: Inner(MoveTemp(InInner))
{
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxDiffsToLog"), MaxDiffsToLog, GEditorIni);
	// Command line override for MaxDiffsToLog
	FParse::Value(FCommandLine::Get(), TEXT("MaxDiffstoLog="), MaxDiffsToLog);

	bSaveForDiff = FParse::Param(FCommandLine::Get(), TEXT("SaveForDiff"));

	GConfig->GetBool(TEXT("CookSettings"), TEXT("IgnoreHeaderDiffs"), bIgnoreHeaderDiffs, GEditorIni);
	// Command line override for IgnoreHeaderDiffs
	if (bIgnoreHeaderDiffs)
	{
		bIgnoreHeaderDiffs = !FParse::Param(FCommandLine::Get(), TEXT("HeaderDiffs"));
	}
	else
	{
		bIgnoreHeaderDiffs = FParse::Param(FCommandLine::Get(), TEXT("IgnoreHeaderDiffs"));
	}

	ParseCmds();
}

void FDiffPackageWriter::ParseCmds()
{
	const TCHAR* DumpObjListParam = TEXT("dumpobjlist");
	const TCHAR* DumpObjectsParam = TEXT("dumpobjects");

	FString CmdsText;
	if (FParse::Value(FCommandLine::Get(), TEXT("-diffcmds="), CmdsText, false))
	{
		CmdsText = CmdsText.TrimQuotes();
		TArray<FString> CmdsList;
		CmdsText.ParseIntoArray(CmdsList, TEXT(","));
		for (const FString& Cmd : CmdsList)
		{
			if (Cmd.StartsWith(DumpObjListParam))
			{
				bDumpObjList = true;
				ParseDumpObjList(*Cmd + FCString::Strlen(DumpObjListParam));
			}
			else if (Cmd.StartsWith(DumpObjectsParam))
			{
				bDumpObjects = true;
				ParseDumpObjects(*Cmd + FCString::Strlen(DumpObjectsParam));
			}
		}
	}
}

void FDiffPackageWriter::ParseDumpObjList(FString InParams)
{
	const TCHAR* PackageFilterParam = TEXT("-packagefilter=");
	FParse::Value(*InParams, PackageFilterParam, PackageFilter);
	RemoveParam(InParams, PackageFilterParam);

	// Add support for more parameters here
	// After all parameters have been parsed and removed, pass the remaining string as objlist params
	DumpObjListParams = InParams;
}

void FDiffPackageWriter::ParseDumpObjects(FString InParams)
{
	const TCHAR* PackageFilterParam = TEXT("-packagefilter=");
	FParse::Value(*InParams, PackageFilterParam, PackageFilter);
	RemoveParam(InParams, PackageFilterParam);

	const TCHAR* SortParam = TEXT("sort");
	bDumpObjectsSorted = FParse::Param(*InParams, SortParam);
	RemoveParam(InParams, SortParam);
}

void FDiffPackageWriter::RemoveParam(FString& InOutParams, const TCHAR* InParamToRemove)
{
	int32 ParamIndex = InOutParams.Find(InParamToRemove);
	if (ParamIndex >= 0)
	{
		int32 NextParamIndex = InOutParams.Find(TEXT(" -"),
			ESearchCase::CaseSensitive, ESearchDir::FromStart, ParamIndex + 1);
		if (NextParamIndex < ParamIndex)
		{
			NextParamIndex = InOutParams.Len();
		}
		InOutParams = InOutParams.Mid(0, ParamIndex) + InOutParams.Mid(NextParamIndex);
	}
}

void FDiffPackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	bIsDifferent = false;
	bDiffCallstack = false;
	bHasStartedSecondSave = false;
	DiffMap.Reset();

	BeginInfo = Info;
	ConditionallyDumpObjList();
	ConditionallyDumpObjects();
	Inner->BeginPackage(Info);
}

void FDiffPackageWriter::CommitPackage(FCommitPackageInfo&& Info)
{
	if (bDiffCallstack && bSaveForDiff)
	{
		// Write the package to _ForDiff, but do not write any sidecars
		EnumRemoveFlags(Info.WriteOptions, EWriteOptions::WriteSidecars);
		EnumAddFlags(Info.WriteOptions, EWriteOptions::SaveForDiff);
	}
	else
	{
		EnumRemoveFlags(Info.WriteOptions, EWriteOptions::Write);
	}
	return Inner->CommitPackage(MoveTemp(Info));
}

void FDiffPackageWriter::WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
	const TArray<FFileRegion>& FileRegions)
{
	Inner->CompleteExportsArchiveForDiff(Info, ExportsArchive);

	FArchiveStackTrace& Writer = static_cast<FArchiveStackTrace&>(ExportsArchive);
	ICookedPackageWriter::FPreviousCookedBytesData PreviousInnerData;
	Inner->GetPreviousCookedBytes(Info, PreviousInnerData);

	FArchiveStackTrace::FPackageData PreviousPackageData;
	PreviousPackageData.Data = PreviousInnerData.Data.Get();
	PreviousPackageData.Size = PreviousInnerData.Size;
	PreviousPackageData.HeaderSize = PreviousInnerData.HeaderSize;
	PreviousPackageData.StartOffset = PreviousInnerData.StartOffset;

	if (bDiffCallstack)
	{
		TMap<FName, FArchiveDiffStats> PackageDiffStats;
		const TCHAR* CutoffString = TEXT("UEditorEngine::Save()");
		Writer.CompareWith(PreviousPackageData, *Info.LooseFilePath, Info.HeaderSize, CutoffString,
			MaxDiffsToLog, PackageDiffStats);

		//COOK_STAT(FSavePackageStats::NumberOfDifferentPackages++);
		//COOK_STAT(FSavePackageStats::MergeStats(PackageDiffStats));
	}
	else
	{
		bIsDifferent = !Writer.GenerateDiffMap(PreviousPackageData, Info.HeaderSize, MaxDiffsToLog, DiffMap);
	}

	Inner->WritePackageData(Info, ExportsArchive, FileRegions);
}

TUniquePtr<FLargeMemoryWriter> FDiffPackageWriter::CreateLinkerArchive(FName PackageName, UObject* Asset)
{
	// The entire package will be serialized to memory and then compared against package on disk.
	if (bDiffCallstack)
	{
		// Each difference will be logged with its Serialize call stack trace
		return TUniquePtr<FLargeMemoryWriter>(new FArchiveStackTrace(Asset, *PackageName.ToString(),
			true /* bInCollectCallstacks */, &DiffMap));
	}
	else
	{
		return TUniquePtr<FLargeMemoryWriter>(new FArchiveStackTrace(Asset, *PackageName.ToString(),
			false /* bInCollectCallstacks */));
	}
}

bool FDiffPackageWriter::IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs)
{
	checkf(!Inner->IsAnotherSaveNeeded(PreviousResult, SaveArgs),
		TEXT("DiffPackageWriter does not support an Inner that needs multiple saves."));
	if (PreviousResult == ESavePackageResult::Timeout)
	{
		return false;
	}

	// When looking for deterministic cook issues, first serialize the package to memory and do a simple diff with the
	// existing package. If the simple memory diff was not identical, collect callstacks for all Serialize calls and
	// dump differences to log
	if (!bHasStartedSecondSave)
	{
		bHasStartedSecondSave = true;
		if (PreviousResult.Result == ESavePackageResult::Success && bIsDifferent)
		{
			bDiffCallstack = true;

			// The contract with the Inner is that Begin is paired with a single commit;
			// send the old commit and the new begin
			FCommitPackageInfo CommitInfo;
			CommitInfo.Status = IPackageWriter::ECommitStatus::Success;
			CommitInfo.PackageName = BeginInfo.PackageName;
			CommitInfo.WriteOptions = EWriteOptions::None;
			Inner->CommitPackage(MoveTemp(CommitInfo));
			Inner->BeginPackage(BeginInfo);

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool FDiffPackageWriter::FilterPackageName(const FString& InWildcard)
{
	bool bInclude = false;
	FString PackageName = BeginInfo.PackageName.ToString();
	if (PackageName.MatchesWildcard(InWildcard))
	{
		bInclude = true;
	}
	else if (FPackageName::GetShortName(PackageName).MatchesWildcard(InWildcard))
	{
		bInclude = true;
	}
	else
	{
		const FString& Filename = BeginInfo.LooseFilePath;
		bInclude = Filename.MatchesWildcard(InWildcard);
	}
	return bInclude;
}

void FDiffPackageWriter::ConditionallyDumpObjList()
{
	if (bDumpObjList)
	{
		if (FilterPackageName(PackageFilter))
		{
			FString ObjListExec = TEXT("OBJ LIST ");
			ObjListExec += DumpObjListParams;

			TGuardValue<ELogTimes::Type> GuardLogTimes(GPrintLogTimes, ELogTimes::None);
			TGuardValue<bool> GuardLogCategory(GPrintLogCategory, false);
			TGuardValue<bool> GuardPrintLogVerbosity(GPrintLogVerbosity, false);

			GEngine->Exec(nullptr, *ObjListExec);
		}
	}
}

void FDiffPackageWriter::ConditionallyDumpObjects()
{
	if (bDumpObjects)
	{
		if (FilterPackageName(PackageFilter))
		{
			TArray<FString> AllObjects;
			for (FThreadSafeObjectIterator It; It; ++It)
			{
				AllObjects.Add(*It->GetFullName());
			}
			if (bDumpObjectsSorted)
			{
				AllObjects.Sort();
			}

			TGuardValue<ELogTimes::Type> GuardLogTimes(GPrintLogTimes, ELogTimes::None);
			TGuardValue<bool> GuardLogCategory(GPrintLogCategory, false);
			TGuardValue<bool> GuardPrintLogVerbosity(GPrintLogVerbosity, false);

			for (const FString& Obj : AllObjects)
			{
				UE_LOG(LogCook, Display, TEXT("%s"), *Obj);
			}
		}
	}
}

FLinkerDiffPackageWriter::FLinkerDiffPackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner)
	: Inner(MoveTemp(InInner))
{
	FString DiffModeText;
	FParse::Value(FCommandLine::Get(), TEXT("-LINKERDIFF="), DiffModeText);
	DiffMode = EDiffMode::LDM_Consistent; // (DiffModeText == TEXT("2") || DiffModeText == TEXT("consistent"))
}

void FLinkerDiffPackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	bHasStartedSecondSave = false;
	BeginInfo = Info;
	Inner->BeginPackage(Info);
	SetupOtherAlgorithm();
}

void FLinkerDiffPackageWriter::UpdateSaveArguments(FSavePackageArgs& SaveArgs)
{
	SaveArgs.SaveFlags |= SAVE_CompareLinker;
	Inner->UpdateSaveArguments(SaveArgs);
}

bool FLinkerDiffPackageWriter::IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult,
	FSavePackageArgs& SaveArgs)
{
	checkf(!Inner->IsAnotherSaveNeeded(PreviousResult, SaveArgs),
		TEXT("LinkerDiffPackageWriter does not support an Inner that needs multiple saves."));
	if (PreviousResult == ESavePackageResult::Timeout)
	{
		OtherResult.LinkerSave.Reset();
		PreviousResult.LinkerSave.Reset();
		return false;
	}

	if (!bHasStartedSecondSave)
	{
		bHasStartedSecondSave = true;
		OtherResult = MoveTemp(PreviousResult);

		// Resave the package with the current save algorithm.
		SetupCurrentAlgorithm();

		// The contract with the Inner is that every Begin is paired with a single commit.
		// Send the old commit and the new begin.
		IPackageWriter::FCommitPackageInfo CommitInfo;
		if (OtherResult == ESavePackageResult::Success)
		{
			CommitInfo.Status = IPackageWriter::ECommitStatus::Success;
		}
		else
		{
			CommitInfo.Status = IPackageWriter::ECommitStatus::Error;
		}
		CommitInfo.PackageName = BeginInfo.PackageName;
		CommitInfo.WriteOptions = IPackageWriter::EWriteOptions::None;
		Inner->CommitPackage(MoveTemp(CommitInfo));
		Inner->BeginPackage(BeginInfo);
		return true;
	}
	else
	{
		CompareResults(PreviousResult);

		OtherResult.LinkerSave.Reset();
		PreviousResult.LinkerSave.Reset();

		return false;
	}
}

void FLinkerDiffPackageWriter::SetupOtherAlgorithm()
{
}

void FLinkerDiffPackageWriter::SetupCurrentAlgorithm()
{
}

void FLinkerDiffPackageWriter::CompareResults(FSavePackageResultStruct& CurrentResult)
{
	if (OtherResult.LinkerSave && CurrentResult.LinkerSave)
	{
		FLinkerDiff LinkerDiff =
			FLinkerDiff::CompareLinkers(OtherResult.LinkerSave.Get(), CurrentResult.LinkerSave.Get());
		LinkerDiff.PrintDiff(*GWarn);
	}
}
