// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlState.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "SubversionSourceControlRevision.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl.State"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSubversionSourceControlState::FSubversionSourceControlState() = default;
FSubversionSourceControlState::FSubversionSourceControlState(const FSubversionSourceControlState& Other) = default;
FSubversionSourceControlState::FSubversionSourceControlState(FSubversionSourceControlState&& Other) noexcept = default;
FSubversionSourceControlState& FSubversionSourceControlState::operator=(const FSubversionSourceControlState& Other) = default;
FSubversionSourceControlState& FSubversionSourceControlState::operator=(FSubversionSourceControlState&& Other) noexcept = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

int32 FSubversionSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::FindHistoryRevision( int32 RevisionNumber ) const
{
	for(auto Iter(History.CreateConstIterator()); Iter; Iter++)
	{
		if((*Iter)->GetRevisionNumber() == RevisionNumber)
		{
			return *Iter;
		}
	}

	return NULL;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for(const auto& Revision : History)
	{
		if(Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return NULL;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlState::GetCurrentRevision() const
{
	return FindHistoryRevision(LocalRevNumber);
}

ISourceControlState::FResolveInfo FSubversionSourceControlState::GetResolveInfo() const
{
	return PendingResolveInfo;
}

#if SOURCE_CONTROL_WITH_SLATE

FSlateIcon FSubversionSourceControlState::GetIcon() const
{
	if (LockState == ELockState::Locked)
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOut");
	}
	else if (LockState == ELockState::LockedOther)
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.CheckedOutByOtherUser", NAME_None, "RevisionControl.CheckedOutByOtherUserBadge");
	}
	else if (!IsCurrent())
	{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotAtHeadRevision");
	}

	switch (WorkingCopyState)
	{
	case EWorkingCopyState::Added:
		if (bCopied)
		{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.Branched");
		}
		else
		{
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.OpenForAdd");
		}
	case EWorkingCopyState::NotControlled:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.NotInDepot");
	case EWorkingCopyState::Deleted:
		return FSlateIcon(FRevisionControlStyleManager::GetStyleSetName(), "RevisionControl.MarkedForDelete");
	}

	return FSlateIcon();
}

#endif //SOURCE_CONTROL_WITH_SLATE

FText FSubversionSourceControlState::GetDisplayName() const
{
	if(LockState == ELockState::Locked)
	{
		return LOCTEXT("Locked", "Locked For Editing");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FText::Format( LOCTEXT("LockedOther", "Locked by "), FText::FromString(LockUser) );
	}

	switch(WorkingCopyState) //-V719
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown", "Unknown");
	case EWorkingCopyState::Pristine:
		return LOCTEXT("Pristine", "Pristine");
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return LOCTEXT("AddedWithHistory", "Added With History");
		}
		else
		{
			return LOCTEXT("Added", "Added");
		}
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted", "Deleted");
	case EWorkingCopyState::Modified:
		return LOCTEXT("Modified", "Modified");
	case EWorkingCopyState::Replaced:
		return LOCTEXT("Replaced", "Replaced");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict", "Contents Conflict");
	case EWorkingCopyState::External:
		return LOCTEXT("External", "External");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored", "Ignored");
	case EWorkingCopyState::Incomplete:
		return LOCTEXT("Incomplete", "Incomplete");
	case EWorkingCopyState::Merged:
		return LOCTEXT("Merged", "Merged");
	case EWorkingCopyState::NotControlled:
		return LOCTEXT("NotControlled", "Not Under Revision Control");
	case EWorkingCopyState::Obstructed:
		return LOCTEXT("Obstructed", "Obstructed By Other Type");
	case EWorkingCopyState::Missing:
		return LOCTEXT("Missing", "Missing");
	}

	return FText();
}

FText FSubversionSourceControlState::GetDisplayTooltip() const
{
	if(LockState == ELockState::Locked)
	{
		return LOCTEXT("Locked_Tooltip", "Locked for editing by current user");
	}
	else if(LockState == ELockState::LockedOther)
	{
		return FText::Format( LOCTEXT("LockedOther_Tooltip", "Locked for editing by: {0}"), FText::FromString(LockUser) );
	}

	switch(WorkingCopyState) //-V719
	{
	case EWorkingCopyState::Unknown:
		return LOCTEXT("Unknown_Tooltip", "Unknown revision control state");
	case EWorkingCopyState::Pristine:
		return LOCTEXT("Pristine_Tooltip", "There are no modifications");
	case EWorkingCopyState::Added:
		if(bCopied)
		{
			return LOCTEXT("AddedWithHistory_Tooltip", "Item is scheduled for addition with history");
		}
		else
		{
			return LOCTEXT("Added_Tooltip", "Item is scheduled for addition");
		}
	case EWorkingCopyState::Deleted:
		return LOCTEXT("Deleted_Tooltip", "Item is scheduled for deletion");
	case EWorkingCopyState::Modified:
		return LOCTEXT("Modified_Tooltip", "Item has been modified");
	case EWorkingCopyState::Replaced:
		return LOCTEXT("Replaced_Tooltip", "Item has been replaced in this working copy. This means the file was scheduled for deletion, and then a new file with the same name was scheduled for addition in its place.");
	case EWorkingCopyState::Conflicted:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents (as opposed to the properties) of the item conflict with updates received from the repository.");
	case EWorkingCopyState::External:
		return LOCTEXT("External_Tooltip", "Item is present because of an externals definition.");
	case EWorkingCopyState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EWorkingCopyState::Merged:
		return LOCTEXT("Merged_Tooltip", "Item has been merged.");
	case EWorkingCopyState::NotControlled:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	case EWorkingCopyState::Obstructed:
		return LOCTEXT("ReplacedOther_Tooltip", "Item is versioned as one kind of object (file, directory, link), but has been replaced by a different kind of object.");
	case EWorkingCopyState::Missing:
		return LOCTEXT("Missing_Tooltip", "Item is missing (e.g., you moved or deleted it without using svn). This also indicates that a directory is incomplete (a checkout or update was interrupted).");
	}

	return FText();
}

const FString& FSubversionSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FSubversionSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

bool FSubversionSourceControlState::CanCheckIn() const
{
	return ( (LockState == ELockState::Locked) || (WorkingCopyState == EWorkingCopyState::Added) ) && !IsConflicted() && IsCurrent();
}

bool FSubversionSourceControlState::CanCheckout() const
{
	return (WorkingCopyState == EWorkingCopyState::Pristine || WorkingCopyState == EWorkingCopyState::Modified) && LockState == ELockState::NotLocked;
}

bool FSubversionSourceControlState::IsCheckedOut() const
{
	return LockState == ELockState::Locked;
}

bool FSubversionSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if(Who != NULL)
	{
		*Who = LockUser;
	}
	return LockState == ELockState::LockedOther;
}

bool FSubversionSourceControlState::IsCurrent() const
{
	return !bNewerVersionOnServer;
}

bool FSubversionSourceControlState::IsSourceControlled() const
{
	return WorkingCopyState != EWorkingCopyState::NotControlled && WorkingCopyState != EWorkingCopyState::Unknown && WorkingCopyState != EWorkingCopyState::NotAWorkingCopy;
}

bool FSubversionSourceControlState::IsAdded() const
{
	return WorkingCopyState == EWorkingCopyState::Added;
}

bool FSubversionSourceControlState::IsDeleted() const
{
	return WorkingCopyState == EWorkingCopyState::Deleted;
}

bool FSubversionSourceControlState::IsIgnored() const
{
	return WorkingCopyState == EWorkingCopyState::Ignored;
}

bool FSubversionSourceControlState::CanEdit() const
{
	return LockState == ELockState::Locked || WorkingCopyState == EWorkingCopyState::Added;
}

bool FSubversionSourceControlState::CanDelete() const
{
	return !IsCheckedOutOther() && IsSourceControlled() && IsCurrent();
}

bool FSubversionSourceControlState::IsUnknown() const
{
	return WorkingCopyState == EWorkingCopyState::Unknown;
}

bool FSubversionSourceControlState::IsModified() const
{
	return WorkingCopyState == EWorkingCopyState::Modified || WorkingCopyState == EWorkingCopyState::Merged || WorkingCopyState == EWorkingCopyState::Obstructed || WorkingCopyState == EWorkingCopyState::Conflicted;
}

bool FSubversionSourceControlState::CanAdd() const
{
	return WorkingCopyState == EWorkingCopyState::NotControlled;
}

bool FSubversionSourceControlState::CanRevert() const
{
	return CanCheckIn();
}

#undef LOCTEXT_NAMESPACE
