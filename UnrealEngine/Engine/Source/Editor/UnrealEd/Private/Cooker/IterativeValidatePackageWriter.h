// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiffPackageWriter.h"

/**
 * A CookedPackageWriter that diffs the cook results of iteratively-unmodified packages between their last cook
 * results and the current cook.
 */
class FIterativeValidatePackageWriter : public FDiffPackageWriter
{
public:
	using Super = FDiffPackageWriter;
	enum class EPhase
	{
		AllInOnePhase,
		Phase1,
		Phase2,
	};
	FIterativeValidatePackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner, EPhase InPhase,
		const FString& ResolvedMetadataPath);

	// IPackageWriter
	virtual void BeginPackage(const FBeginPackageInfo& Info) override;
	virtual void CommitPackage(FCommitPackageInfo&& Info) override;
	virtual void WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
		const TArray<FFileRegion>& FileRegions) override;
	virtual void WriteBulkData(const FBulkDataInfo& Info, const FIoBuffer& BulkData,
		const TArray<FFileRegion>& FileRegions) override;
	virtual void WriteAdditionalFile(const FAdditionalFileInfo& Info, const FIoBuffer& FileData) override;
	virtual void WriteLinkerAdditionalData(const FLinkerAdditionalDataInfo& Info, const FIoBuffer& Data,
		const TArray<FFileRegion>& FileRegions) override;
	virtual void WritePackageTrailer(const FPackageTrailerInfo& Info, const FIoBuffer& Data) override;
	virtual int64 GetExportsFooterSize() override;
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex) override;
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerExportsArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex) override;
	virtual bool IsPreSaveCompleted() const override;

	// ICookedPackageWriter
	virtual FCookCapabilities GetCookCapabilities() const override;
	virtual void Initialize(const FCookInfo& CookInfo) override;
	virtual void UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
		bool& bInOutShouldIterativelySkip) override;
	virtual void BeginCook(const FCookInfo& Info) override;
	virtual void EndCook(const FCookInfo& Info) override;
	virtual void UpdateSaveArguments(FSavePackageArgs& SaveArgs) override;
	virtual bool IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs) override;

protected:
	virtual void OnDiffWriterMessage(ELogVerbosity::Type Verbosity, FStringView Message) override;
	void LogIterativeDifferences();
	void Save();
	void Load();
	void Serialize(FArchive& Ar);
	FString GetIterativeValidatePath() const;

	enum class ESaveAction : uint8
	{
		CheckForDiffs,
		SaveToInner,
		IgnoreResults,
	};
	struct FMessage
	{
		FString Text;
		ELogVerbosity::Type Verbosity;
	};
	friend FArchive& operator<<(FArchive& Ar, FMessage& Message);

	/**
	 * Written during Phase1, read during Phase2. Read/Written during AllInOnePhase. Packages that we thought were iteratively
	 * unchanged and that were confirmed upon resave to be unchanged.
	 */
	TSet<FName> IterativeValidated;
	/**
	 * Written during Phase1, read during Phase2. Unused in AllInOnePhase. Packages that we thought were iteratively
	 * unchanged but in which we discovered differences. But the differences might be due to indeterminism. Phase2
	 * splits this container into IndeterminismFailed entries and IterativeFalseNegative entries.
	 */
	TMap<FName, TArray<FMessage>> IterativeFailed;
	/**
	 * Read during Phase2. Unused in AllInOnePhase. Packages that we thought were iteratively unchanged but that had
	 * differences when saved, but those differences turned out to be due to indeterminism.
	 */
	TSet<FName> IndeterminismFailed;
	/**
	 * Read during Phase2. Read/Written during AllInOnePhase. Packages that we thought were iteratively unchanged but that had
	 * differences when saved, and no indeterminism detected (AllInOnePhase does not search for indeterminism, so all packages
	 * with differences end up here), so they must be a bug in the iteratvely unchanged decision.
	 */
	TSet<FName> IterativeFalseNegative;
	/**
	 * Unused in Phase1,Phase2. In AllInOnePhase, this records packages that we think are iteratively unchanged.
	 * We do a 2-pass or 3-pass save for these: look for diffs and then save to disk. For packages not in this list
	 * we do a 1-pass save: save it to disk without looking for diffs.
	 */
	TSet<FName> IterativelyUnmodified;

	FString MetadataPath;
	int32 ModifiedCount = 0;
	EPhase Phase = EPhase::AllInOnePhase;
	ESaveAction SaveAction = ESaveAction::IgnoreResults;
	bool bPackageFirstPass = false;
	bool bReadOnly = true;
};
