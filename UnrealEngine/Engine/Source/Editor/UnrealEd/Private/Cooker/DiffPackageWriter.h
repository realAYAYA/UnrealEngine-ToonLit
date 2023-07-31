// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetRegistryState.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/PackageWriter.h"

/** A CookedPackageWriter that diffs output from the current cook with the file that was saved in the previous cook. */
class FDiffPackageWriter : public ICookedPackageWriter
{
public:
	FDiffPackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner);

	// IPackageWriter
	virtual FCapabilities GetCapabilities() const override
	{
		FCapabilities Result = Inner->GetCapabilities();
		Result.bIgnoreHeaderDiffs = bIgnoreHeaderDiffs;
		return Result;
	}
	virtual void BeginPackage(const FBeginPackageInfo& Info) override;
	virtual void CommitPackage(FCommitPackageInfo&& Info) override;
	virtual void WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
		const TArray<FFileRegion>& FileRegions) override;
	virtual void WriteBulkData(const FBulkDataInfo& Info, const FIoBuffer& BulkData,
		const TArray<FFileRegion>& FileRegions) override
	{
		Inner->WriteBulkData(Info, BulkData, FileRegions);
	}
	virtual void WriteAdditionalFile(const FAdditionalFileInfo& Info, const FIoBuffer& FileData) override
	{
		Inner->WriteAdditionalFile(Info, FileData);
	}
	virtual void WriteLinkerAdditionalData(const FLinkerAdditionalDataInfo& Info, const FIoBuffer& Data,
		const TArray<FFileRegion>& FileRegions) override
	{
		Inner->WriteLinkerAdditionalData(Info, Data, FileRegions);
	}
	virtual void AddToExportsSize(int64& ExportsSize) override
	{
		Inner->AddToExportsSize(ExportsSize);
	}
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerArchive(FName PackageName, UObject* Asset) override;
	virtual bool IsPreSaveCompleted() const override
	{
		return bDiffCallstack;
	}

	// ICookedPackageWriter
	virtual FCookCapabilities GetCookCapabilities() const override
	{
		FCookCapabilities Result = Inner->GetCookCapabilities();
		Result.bDiffModeSupported = false; // DiffPackageWriter can not be an inner of another DiffPackageWriter
		return Result;
	}
	virtual FDateTime GetPreviousCookTime() const
	{
		return Inner->GetPreviousCookTime();
	}
	virtual void Initialize(const FCookInfo& Info) override
	{
		Inner->Initialize(Info);
	}
	virtual void BeginCook() override
	{
		Inner->BeginCook();
	}
	virtual void EndCook() override
	{
		Inner->EndCook();
	}
	virtual TUniquePtr<FAssetRegistryState> LoadPreviousAssetRegistry() override
	{
		return Inner->LoadPreviousAssetRegistry();
	}
	virtual FCbObject GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey) override
	{
		return Inner->GetOplogAttachment(PackageName, AttachmentKey);
	}
	virtual void RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove) override
	{
		Inner->RemoveCookedPackages(PackageNamesToRemove);
	}
	virtual void RemoveCookedPackages() override
	{
		Inner->RemoveCookedPackages();
	}
	virtual void MarkPackagesUpToDate(TArrayView<const FName> UpToDatePackages) override
	{
		Inner->MarkPackagesUpToDate(UpToDatePackages);
	}
	virtual void UpdateSaveArguments(FSavePackageArgs& SaveArgs) override
	{
		Inner->UpdateSaveArguments(SaveArgs);
	}
	virtual bool IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs) override;
	virtual TMap<FName, TRefCountPtr<FPackageHashes>>& GetPackageHashes() override
	{
		return Inner->GetPackageHashes();
	}
private:
	void ParseCmds();
	void ParseDumpObjList(FString InParams);
	void ParseDumpObjects(FString InParams);
	void RemoveParam(FString& InOutParams, const TCHAR* InParamToRemove);
	bool FilterPackageName(const FString& InWildcard);
	void ConditionallyDumpObjList();
	void ConditionallyDumpObjects();

	FArchiveDiffMap DiffMap;
	FBeginPackageInfo BeginInfo;
	TUniquePtr<ICookedPackageWriter> Inner;
	FString DumpObjListParams;
	FString PackageFilter;
	int32 MaxDiffsToLog = 5;
	bool bSaveForDiff = false;
	bool bIgnoreHeaderDiffs = false;
	bool bIsDifferent = false;
	bool bDiffCallstack = false;
	bool bHasStartedSecondSave = false;
	bool bDumpObjList = false;
	bool bDumpObjects = false;
	bool bDumpObjectsSorted = false;
};

/** A CookedPackageWriter that runs the save twice with different methods (Save1 vs Save2). */
class FLinkerDiffPackageWriter : public ICookedPackageWriter
{
public:
	FLinkerDiffPackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner);

	// IPackageWriter
	virtual FCapabilities GetCapabilities() const override
	{
		return Inner->GetCapabilities();
	}
	virtual void BeginPackage(const FBeginPackageInfo& Info) override;
	virtual void CommitPackage(FCommitPackageInfo&& Info) override
	{
		return Inner->CommitPackage(MoveTemp(Info));
	}
	virtual void WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
		const TArray<FFileRegion>& FileRegions) override
	{
		Inner->WritePackageData(Info, ExportsArchive, FileRegions);
	}
	virtual void WriteBulkData(const FBulkDataInfo& Info, const FIoBuffer& BulkData,
		const TArray<FFileRegion>& FileRegions) override
	{
		Inner->WriteBulkData(Info, BulkData, FileRegions);
	}
	virtual void WriteAdditionalFile(const FAdditionalFileInfo& Info, const FIoBuffer& FileData) override
	{
		Inner->WriteAdditionalFile(Info, FileData);
	}
	virtual void WriteLinkerAdditionalData(const FLinkerAdditionalDataInfo& Info, const FIoBuffer& Data,
		const TArray<FFileRegion>& FileRegions) override
	{
		Inner->WriteLinkerAdditionalData(Info, Data, FileRegions);
	}
	virtual void AddToExportsSize(int64& ExportsSize) override
	{
		Inner->AddToExportsSize(ExportsSize);
	}
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerArchive(FName PackageName, UObject* Asset) override
	{
		return Inner->CreateLinkerArchive(PackageName, Asset);
	}
	virtual bool IsPreSaveCompleted() const override
	{
		return Inner->IsPreSaveCompleted();
	}

	// ICookedPackageWriter
	virtual FCookCapabilities GetCookCapabilities() const override
	{
		FCookCapabilities Result = Inner->GetCookCapabilities();
		Result.bDiffModeSupported = false; // LinkerDiffPackageWriter can not be an inner of DiffPackageWriter
		return Result;
	}
	virtual FDateTime GetPreviousCookTime() const
	{
		return Inner->GetPreviousCookTime();
	}
	virtual void Initialize(const FCookInfo& Info) override
	{
		Inner->Initialize(Info);
	}
	virtual void BeginCook() override
	{
		Inner->BeginCook();
	}
	virtual void EndCook() override
	{
		Inner->EndCook();
	}
	virtual TUniquePtr<FAssetRegistryState> LoadPreviousAssetRegistry() override
	{
		return Inner->LoadPreviousAssetRegistry();
	}
	virtual FCbObject GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey) override
	{
		return Inner->GetOplogAttachment(PackageName, AttachmentKey);
	}
	virtual void RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove) override
	{
		Inner->RemoveCookedPackages(PackageNamesToRemove);
	}
	virtual void RemoveCookedPackages() override
	{
		Inner->RemoveCookedPackages();
	}
	virtual void MarkPackagesUpToDate(TArrayView<const FName> UpToDatePackages) override
	{
		Inner->MarkPackagesUpToDate(UpToDatePackages);
	}
	virtual void UpdateSaveArguments(FSavePackageArgs& SaveArgs) override;
	virtual bool IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs) override;
	virtual TMap<FName, TRefCountPtr<FPackageHashes>>& GetPackageHashes() override
	{
		return Inner->GetPackageHashes();
	}
private:
	enum class EDiffMode : uint8
	{
		LDM_Consistent,
	};

	void SetupOtherAlgorithm();
	void SetupCurrentAlgorithm();
	void CompareResults(FSavePackageResultStruct& CurrentResult);

	FSavePackageResultStruct OtherResult;
	FBeginPackageInfo BeginInfo;
	TUniquePtr<ICookedPackageWriter> Inner;
	EDiffMode DiffMode = EDiffMode::LDM_Consistent;
	bool bHasStartedSecondSave = false;
};