// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlState.h"
#include "Misc/DateTime.h"

class FSubversionSourceControlRevision;

namespace EWorkingCopyState
{
	enum Type
	{
		Unknown,
		Pristine,
		Added,
		Deleted,
		Modified,
		Replaced,
		Conflicted,
		External,
		Ignored,
		Incomplete,
		Merged,
		NotControlled,
		Obstructed,
		Missing,
		NotAWorkingCopy,
	};
}

namespace ELockState
{
	enum Type
	{
		Unknown,
		NotLocked,
		Locked,
		LockedOther,
	};
}

class FSubversionSourceControlState : public ISourceControlState
{
public:
	FSubversionSourceControlState( const FString& InLocalFilename )
		: LocalFilename(InLocalFilename)
		, LocalRevNumber(INVALID_REVISION)
		, PendingMergeBaseFileRevNumber(INVALID_REVISION)
		, bNewerVersionOnServer(false)
		, WorkingCopyState(EWorkingCopyState::Unknown)
		, LockState(ELockState::Unknown)
		, TimeStamp(0)
		, bCopied(false)
	{
	}

	FSubversionSourceControlState();
	FSubversionSourceControlState(const FSubversionSourceControlState& Other);
	FSubversionSourceControlState(FSubversionSourceControlState&& Other) noexcept;
	FSubversionSourceControlState& operator=(const FSubversionSourceControlState& Other);
	FSubversionSourceControlState& operator=(FSubversionSourceControlState&& Other) noexcept;

	/** ISourceControlState interface */
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem( int32 HistoryIndex ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( int32 RevisionNumber ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( const FString& InRevision ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetCurrentRevision() const override;
	virtual FResolveInfo GetResolveInfo() const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual FSlateIcon GetIcon() const override;
#endif //SOURCE_CONTROL_WITH_SLATE
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = NULL) const override;
	virtual bool IsCheckedOutInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return false; }
	virtual bool IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override { return IsCheckedOutInOtherBranch(CurrentBranch) || IsModifiedInOtherBranch(CurrentBranch); }
	virtual TArray<FString> GetCheckedOutBranches() const override { return TArray<FString>(); }
	virtual FString GetOtherUserBranchCheckedOuts() const override { return FString(); }
	virtual bool GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const override { return false;  }
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool CanDelete() const override;
	virtual bool CanRevert() const override;

public:
	/** History of the item, if any */
	TArray< TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> > History;

	/** Filename on disk */
	FString LocalFilename;

	/** Revision number currently synced */
	int LocalRevNumber;

	/** Pending rev info with which a file must be resolved, invalid if no resolve pending */
	FResolveInfo PendingResolveInfo;

	UE_DEPRECATED(5.3, "Use PendingResolveInfo.BaseRevisionNumber instead")
	int PendingMergeBaseFileRevNumber;

	/** Whether a newer version exists on the server */
	bool bNewerVersionOnServer;

	/** State of the working copy */
	EWorkingCopyState::Type WorkingCopyState;

	/** Lock state */
	ELockState::Type LockState;

	/** Name of other user who has file locked */
	FString LockUser;

	/** The timestamp of the last update */
	FDateTime TimeStamp;

	/** Flagged as a copy/branch */
	bool bCopied;
};
