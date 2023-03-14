// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformCrt.h"
#include "ISourceControlChangelist.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Memory/SharedBuffer.h"
#include "SourceControlOperationBase.h"
#include "SourceControlPreferences.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "SourceControl"

/**
 * Operation used to connect (or test a connection) to source control
 */
class FConnect : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Connect";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Connecting", "Connecting to source control...");
	}

	const FString& GetPassword() const
	{
		return Password;
	}

	void SetPassword(const FString& InPassword)
	{
		Password = InPassword;
	}

	const FText& GetErrorText() const
	{
		return OutErrorText;
	}

	void SetErrorText(const FText& InErrorText)
	{
		OutErrorText = InErrorText;
	}

protected:
	/** Password we use for this operation */
	FString Password;

	/** Error text for easy diagnosis */
	FText OutErrorText;
};

/**
 * Operation used to check files into source control
 */
class FCheckIn : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "CheckIn";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_CheckIn", "Checking file(s) into Source Control...");
	}

	void SetDescription( const FText& InDescription )
	{
		Description = InDescription;
	}

	const FText& GetDescription() const
	{
		return Description;
	}

	void SetSuccessMessage( const FText& InSuccessMessage )
	{
		SuccessMessage = InSuccessMessage;
	}

	const FText& GetSuccessMessage() const
	{
		return SuccessMessage;
	}

	void SetKeepCheckedOut( const bool bInKeepCheckedOut )
	{
		bKeepCheckedOut = bInKeepCheckedOut;
	}

	bool GetKeepCheckedOut()
	{
		return bKeepCheckedOut;
	}

protected:
	/** Description of the checkin */
	FText Description;

	/** A short message listing changelist/revision we submitted, if successful */
	FText SuccessMessage;

	/** Keep files checked-out after checking in */
	bool bKeepCheckedOut = false;
};

/**
 * Operation used to check files out of source control
 */
class FCheckOut : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "CheckOut";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_CheckOut", "Checking file(s) out of Source Control...");
	}
};

/**
 * Operation used to get the file list of a folder out of source control
 */
class FGetFileList : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "GetFileList";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_GetFileList", "Getting file list out of Source Control...");
	}

	void SetIncludeDeleted( const bool bInIncludeDeleted )
	{
		bIncludeDeleted = bInIncludeDeleted;
	}

	bool GetIncludeDeleted()
	{
		return bIncludeDeleted;
	}

	const TArray<FString>& GetFilesList() const
	{
		return FilesList;
	}

	void SetFilesList(TArray<FString>&& InFilesList)
	{
		FilesList = MoveTemp(InFilesList);
	}

protected:
	/** Include deleted files in the list. */
	bool bIncludeDeleted = false;

	/** Stored result of the operation */
	TArray<FString> FilesList;
};

/**
 * Operation used to mark files for add in source control
 */
class FMarkForAdd : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "MarkForAdd";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Add", "Adding file(s) to Source Control...");
	}
};

/**
 * Operation used to mark files for delete in source control
 */
class FDelete : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Delete";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Delete", "Deleting file(s) from Source Control...");
	}
};

/**
 * Operation used to revert changes made back to the state they are in source control
 */
class FRevert : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Revert";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Revert", "Reverting file(s) in Source Control...");
	}

	void SetSoftRevert(const bool bInSoftRevert)
	{
		bIsSoftRevert = bInSoftRevert;
	}

	bool IsSoftRevert() const
	{
		return bIsSoftRevert;
	}

	bool ShouldDeleteNewFiles() const
	{
		return USourceControlPreferences::ShouldDeleteNewFilesOnRevert();
	}

	const TArray<FString>& GetDeletedFiles() const { return DeletedFiles; }

	void AddDeletedFile(const FString& InDeletedFile)
	{
		DeletedFiles.Add(InDeletedFile);
	}

protected:
	bool				bIsSoftRevert = false;
	TArray<FString>		DeletedFiles;
};

/**
 * Operation used to sync files to the state they are in source control
 */
class FSync : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Sync";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Sync", "Syncing file(s) from source control...");
	}

	UE_DEPRECATED(4.26, "FSync::SetRevisionNumber(int32) has been deprecated. Please update to Fsync::SetRevision(const FString&).")
	void SetRevisionNumber(int32 InRevisionNumber)
	{
		SetRevision(FString::Printf(TEXT("%d"), InRevisionNumber));
	}
	void SetRevision( const FString& InRevision )
	{
		Revision = InRevision;
	}

	const FString& GetRevision() const
	{
		return Revision;
	}

	void SetHeadRevisionFlag(const bool bInHeadRevision)
	{
		bHeadRevision = bInHeadRevision;
	}

	bool IsHeadRevisionFlagSet() const
	{
		return bHeadRevision;
	}

	void SetForce(const bool bInForce)
	{
		bForce = bInForce;
	}

	bool IsForced() const
	{
		return bForce;
	}

	void SetLastSyncedFlag(const bool bInLastSynced)
	{
		bLastSynced = bInLastSynced;
	}

	bool IsLastSyncedFlagSet() const
	{
		return bLastSynced;
	}

protected:
	/** Revision to sync to */
	FString Revision;

	/** Flag abstracting if the operation aims to sync to head */
	bool bHeadRevision = false;

	/** Flag abstracting if the operation aims to force sync to last synced revision */
	bool bLastSynced = false;

	/** Forces operation, even if the file is already at the wanted revision. */
	bool bForce = false;
};

/**
 * Operation used to update the source control status of files
 */
class FUpdateStatus : public FSourceControlOperationBase
{
public:
	FUpdateStatus()
		: bUpdateHistory(false)
		, bGetOpenedOnly(false)
		, bUpdateModifiedState(false)
		, bUpdateModifiedStateToLocalRevision(false)
		, bCheckingAllFiles(false)
		, bForceQuiet(false)
		, bForceUpdate(false)
		, bSetRequireDirPathEndWithSeparator(false)
	{
	}

	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "UpdateStatus";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Update", "Updating file(s) source control status...");
	}

	void SetUpdateHistory( bool bInUpdateHistory )
	{
		bUpdateHistory = bInUpdateHistory;
	}

	void SetGetOpenedOnly( bool bInGetOpenedOnly )
	{
		bGetOpenedOnly = bInGetOpenedOnly;
	}

	void SetUpdateModifiedState( bool bInUpdateModifiedState )
	{
		bUpdateModifiedState = bInUpdateModifiedState;
	}

	void SetUpdateModifiedStateToLocalRevision(bool bInUpdateModifiedStateToLocalRevision)
	{
		bUpdateModifiedStateToLocalRevision = bInUpdateModifiedStateToLocalRevision;
	}

	void SetCheckingAllFiles( bool bInCheckingAllFiles )
	{
		bCheckingAllFiles = bInCheckingAllFiles;
	}

	void SetQuiet(bool bInQuiet)
	{
		bForceQuiet = bInQuiet;
	}

	/** 
	 * Sets the method that the operation will use to determine if a path
	 * references a file or a directory. For more details @see IsDirectoryPath()
	 * 
	 * @param bFlag When true the operation will check the path and assume that it
	 *				is a directory if the path ends with '/' or '\' and a file if it 
	 *				does not.
	 *				When false (the default) the operation will poll the file system
	 *				with the path to see if it is a file or a directory.
	 * 
	 */
	void SetRequireDirPathEndWithSeparator(bool bFlag)
	{
		bSetRequireDirPathEndWithSeparator = bFlag;
	}

	void SetForceUpdate(const bool bInForceUpdate)
	{
		bForceUpdate = bInForceUpdate;
	}

	bool ShouldUpdateHistory() const
	{
		return bUpdateHistory;
	}

	bool ShouldGetOpenedOnly() const
	{
		return bGetOpenedOnly;
	}

	bool ShouldUpdateModifiedState() const
	{
		return bUpdateModifiedState;
	}

	bool ShouldUpdateModifiedStateToLocalRevision() const
	{
		return bUpdateModifiedStateToLocalRevision;
	}

	bool ShouldCheckAllFiles() const
	{
		return bCheckingAllFiles;
	}

	bool ShouldBeQuiet() const
	{
		return bForceQuiet;
	}

	bool ShouldForceUpdate() const
	{
		return bForceUpdate;
	}

	/** 
	 * Returns if the given path should be considered a directory or not.
	 * If bSetRequireDirPathEndWithSeparator is not set (the default) then
	 * we will poll the file system. However in some cases this can be very
	 * slow, in which case bSetRequireDirPathEndWithSeparator can be set 
	 * to true and we require that any directory path must be terminated
	 * by a path separator (/ or \) so that we can tell by simply looking at
	 * the path itself.
	 * This is opt in behavior to avoid breaking existing 3rd party code that
	 * relies on the default behavior.
	 */
	SOURCECONTROL_API bool IsDirectoryPath(const FString& Path) const;

protected:
	/** Whether to update history */
	bool bUpdateHistory;

	/** Whether to just get files that are opened/edited */
	bool bGetOpenedOnly;

	/** Whether to update the modified state - expensive */
	bool bUpdateModifiedState;

	/** Whether to update the modified state against local revision - only used when bUpdateModifiedState is true */
	bool bUpdateModifiedStateToLocalRevision;

	/** Hint that we are intending on checking all files in the project - some providers can optimize for this */
	bool bCheckingAllFiles;

	/** Controls whether the operation will trigger an update or not */
	bool bForceQuiet;

	/** Forces the verification for provided files - providers can ignore files not opened/edited without it */
	bool bForceUpdate;

	/** If we should assume paths ending in a separator are directory paths or do we need to check with the file system? */
	bool bSetRequireDirPathEndWithSeparator;
};

/**
 * Operation used to copy a file or directory from one location to another
 */
class FCopy : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Copy";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Copy", "Copying file(s) in Source Control...");
	}

	void SetDestination(const FString& InDestination)
	{
		Destination = InDestination;
	}

	void SetDestination(FString&& InDestination)
	{
		Destination = MoveTemp(InDestination);
	}

	const FString& GetDestination() const
	{
		return Destination;
	}

	enum class ECopyMethod
	{
		Branch,  // The new file is branched from the original file
		Add      // The new file has no relation to the original file
	};

	/** Whether a relationship to the original file should be maintained */
	ECopyMethod CopyMethod = ECopyMethod::Branch; 

protected:
	/** Destination path of the copy operation */
	FString Destination;
};

/**
 * Operation used to resolve a file that is in a conflicted state.
 */
class FResolve : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		  return "Resolve";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Resolve", "Resolving file(s) in Source Control...");
	}
};

/**
 * Operation used to retrieve pending changelist(s).
 */
class FGetPendingChangelists : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "GetPendingChangelists";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_GetPendingChangelists", "Retrieving pending changelist(s) from Source Control...");
	}
};

/**
 * Operation used to retrieve submitted changelist(s).
 */
class FGetSubmittedChangelists : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "GetSubmittedChangelists"; 
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_GetSubmittedChangelists", "Retrieving submitted changelist(s) from Source Control...");
	}
};

/**
 * This operations query the source control to extract all the details available for a given changelist. The operations returns a collection of key/value corresponding to the details available. 
 * The list of key/value is specific to the source control implementation.
 */
class FGetChangelistDetails : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "GetChangelistDetails";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_GetChangelistDetails", "Retrieving changelist details from Source Control...");
	}

	const FString& GetChangelistNumber() { return ChangelistNumber; }

	const TArray<TMap<FString, FString>>& GetChangelistDetails() { return OutChangelistDetails; }
	
	void SetChangelistNumber(const FString& InChangelistNumber)
	{
		ChangelistNumber = InChangelistNumber;
	}

	void SetChangelistDetails(TArray<TMap<FString, FString>>&& InChangelistDetails)
	{
		OutChangelistDetails = MoveTemp(InChangelistDetails);
	}

private:
	FString ChangelistNumber;
	TArray<TMap<FString, FString>> OutChangelistDetails;
};

/**
 * Operation used to update the source control status of changelist(s)
 */
class FUpdatePendingChangelistsStatus : public FSourceControlOperationBase
{
public:
	void SetUpdateFilesStates(bool bInUpdateFilesStates)
	{
		bUpdateFilesStates = bInUpdateFilesStates;
	}

	bool ShouldUpdateFilesStates() const
	{
		return bUpdateFilesStates;
	}

	void SetUpdateShelvedFilesStates(bool bInUpdateShelvedFilesStates)
	{
		bUpdateShelvedFilesStates = bInUpdateShelvedFilesStates;
	}

	bool ShouldUpdateShelvedFilesStates() const
	{
		return bUpdateShelvedFilesStates;
	}

	void SetUpdateAllChangelists(bool bInUpdateAllChangelists)
	{
		bUpdateAllChangelists = bInUpdateAllChangelists;
		ChangelistsToUpdate.Empty();
	}

	bool ShouldUpdateAllChangelists() const
	{
		return bUpdateAllChangelists;
	}

	void SetChangelistsToUpdate(const TArray<FSourceControlChangelistRef>& InChangelistsToUpdate)
	{
		ChangelistsToUpdate = InChangelistsToUpdate;
		bUpdateAllChangelists = false;
	}

	void SetChangelistsToUpdate(const TArrayView<FSourceControlChangelistRef>& InChangelistsToUpdate)
	{
		ChangelistsToUpdate = InChangelistsToUpdate;
		bUpdateAllChangelists = false;
	}

	const TArray<FSourceControlChangelistRef>& GetChangelistsToUpdate() const
	{
		return ChangelistsToUpdate;
	}

	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "UpdateChangelistsStatus";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_UpdateChangelistsStatus", "Updating changelist(s) status from Source Control...");
	}

private:
	bool bUpdateFilesStates = false;
	bool bUpdateShelvedFilesStates = false;
	bool bUpdateAllChangelists = false;

	TArray<FSourceControlChangelistRef> ChangelistsToUpdate;
};

/**
* Operation used to create a new changelist
*/
class FNewChangelist : public FSourceControlOperationBase
{
public:
	virtual FName GetName() const override
	{
		return "NewChangelist";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_NewChangelist", "Creating new changelist from Source Control...");
	}

	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
	}

	const FText& GetDescription() const
	{
		return Description;
	}

	void SetNewChangelist(FSourceControlChangelistPtr InNewChangelist)
	{
		NewChangelist = InNewChangelist;
	}

	const FSourceControlChangelistPtr& GetNewChangelist() const
	{
		return NewChangelist;
	}

protected:
	/** Description of the changelist */
	FText Description;
	FSourceControlChangelistPtr NewChangelist;
};

/**
 * Operation used to delete an empty changelist
 */
class FDeleteChangelist : public FSourceControlOperationBase
{
public:
	virtual FName GetName() const override
	{
		return "DeleteChangelist";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_DeleteChangelist", "Deleting a changelist from Source Control...");
	}
};

/**
 * Operation to change the description of a changelist
 */
class FEditChangelist : public FSourceControlOperationBase
{
public:
	virtual FName GetName() const override
	{
		return "EditChangelist";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_EditChangelist", "Editing a changelist from Source Control...");
	}

	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
	}

	const FText& GetDescription() const
	{
		return Description;
	}

protected:
	/** Description of the changelist */
	FText Description;
};

/**
 * Operation to revert unchanged file(s) or all unchanged files in a changelist
 */
class FRevertUnchanged : public FSourceControlOperationBase
{
public:
	virtual FName GetName() const override
	{
		return "RevertUnchanged";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_RevertUnchanged", "Reverting unchanged files from Source Control...");
	}
};

/**
 * Operation used to move files between changelists
 */
class FMoveToChangelist : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "MoveToChangelist";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_MoveToChangelist", "Moving files to target changelist...");
	}
};

/**
 * Operation used to shelve files in a changelist
 */
class FShelve : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Shelve";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_ShelveOperation", "Shelving files in changelist...");
	}

	void SetDescription(const FText& InDescription)
	{
		Description = InDescription;
	}

	const FText& GetDescription() const
	{
		return Description;
	}

private:
	/** Description of the changelist, will be used only to create a new changelist when needed */
	FText Description;
};

/**
 * Operation used to unshelve files from a changelist
 */
class FUnshelve : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override 
	{
		return "Unshelve";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_UnshelveOperation", "Unshelving files from changelist...");
	}
};

/**
 * Operation used to delete shelved files from a changelist
 */
class FDeleteShelved : public FSourceControlOperationBase
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "DeleteShelved";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_DeleteShelvedOperation", "Deleting shelved files from changelist...");
	}
};

/**
 * Operation used to download a file from the source control server directly rather
 * than sync it. This should not change the state of anything locally on the client.
 */
class FDownloadFile : public FSourceControlOperationBase
{
public:
	enum class EVerbosity
	{
		/** No logging when the command is run. */
		None,
		/** Log the full cmdline when the command is run. */
		Full
	};

	/** 
	 * This constructor will download the files and keep them in memory, 
	 * which can then be accessed by calling XXX.
	 */
	FDownloadFile() = default;

	FDownloadFile( EVerbosity InVerbosity)
		: Verbosity(InVerbosity)
	{}
	
	/** This constructor will download the files to the given directory */
	SOURCECONTROL_API FDownloadFile(FStringView InTargetDirectory, EVerbosity InVerbosity);

	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "DownloadFile";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_PrintOperation", "Downloading file from server...");
	}

	virtual bool CanBeCalledFromBackgroundThreads() const
	{
		return true;
	}

	FString GetTargetDirectory() const
	{
		return TargetDirectory;
	}

	void AddFileData(const FString& Filename, FSharedBuffer FileData)
	{
		FileDataMap.Add(Filename, FileData);
	}

	FSharedBuffer GetFileData(const FStringView& Filename)
	{
		const uint32 Hash = GetTypeHash(Filename);
		FSharedBuffer* Buffer = FileDataMap.FindByHash(Hash, Filename);
		if (Buffer != nullptr)
		{
			return *Buffer;
		}
		else
		{
			return FSharedBuffer();
		}
	}

	bool ShouldLogToStdOutput() const
	{
		return Verbosity == EVerbosity::Full;
	}
private:
	EVerbosity Verbosity = EVerbosity::Full;

	FString TargetDirectory;
	TMap<FString, FSharedBuffer> FileDataMap;
};

/** 
 * Operation used to create a new workspace if the source control system supports this functionality.
 */
class FCreateWorkspace : public FSourceControlOperationBase
{
public:
	enum class EType
	{
		/* Default */
		Writeable = 0,
		/* Has a short life span and can only query/read/sync files */
		ReadOnly,
		/* Has a short life span but is allowed full functionality */
		Partitioned
	};

	using FClientViewMapping = TPair<FString,FString>;

	FCreateWorkspace() = delete;

	/**
	 * Constructor
	 * 
	 * @param WorkspaceName	The name of the workspace to create
	 * @param WorkspaceRoom	The file path to the workspace root (can be relative to the project)
	 */
	SOURCECONTROL_API FCreateWorkspace(FStringView WorkspaceName, FStringView WorkspaceRoot);
	
	/** Set the description to be used by the workspace, if left unset then the default description will be used */
	void SetDescription(FStringView Desciption)
	{
		WorkspaceDescription = Desciption;
	}

	/** Set the stream to be used by the workspace, if left unset then a classic depot with ClientView will be used */
	void SetStream(FStringView Stream)
	{
		WorkspaceStream = Stream;
	}

	/**
	 * Add a new mapping for the client spec in the native format of the current source control provider.
	 * These will be written to the client spec in the order that they are added.
	 * 
	 * @param DepotPath		The path in the source control depot to map from
	 * @param ClientPath	The path on the local client to map too
	 */
	void AddNativeClientViewMapping(FStringView DepotPath, FStringView ClientPath)
	{
		ClientView.Emplace(DepotPath, ClientPath);
	}

	/** Remove all currently set client-view mappings */
	void ClearClientViewMappings()
	{
		ClientView.Empty();
	}

	/** Set the type of workspace to be created. @see FCreateWorkspace::EType */
	void SetType(EType InType)
	{
		Type = InType;
	}

	const FString& GetWorkspaceName() const
	{
		return WorkspaceName;
	}

	const FString& GetWorkspaceRoot() const
	{
		return WorkspaceRoot;
	}

	const FString& GetWorkspaceDescription() const
	{
		return WorkspaceDescription;
	}

	const FString& GetWorkspaceStream() const
	{
		return WorkspaceStream;
	}

	const TArray<FClientViewMapping> GetClientView() const
	{
		return ClientView;
	}

	EType GetType() const
	{
		return Type;
	}

	// ISourceControlOperation interface

	virtual FName GetName() const override
	{
		return "CreateWorkspace";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_CreateWorkspaceOperation", "Creating a workspace...");
	}

protected:

	FString WorkspaceName;
	FString WorkspaceRoot;

	FString WorkspaceDescription;
	FString WorkspaceStream;

	EType Type = EType::Writeable;

	TArray<FClientViewMapping> ClientView;
};

/** Operation used to delete a workspace */
class FDeleteWorkspace : public FSourceControlOperationBase
{
public:
	FDeleteWorkspace() = delete;

	/** 
	 * Constructor
	 * 
	 * @param InWorkspaceName The name of the workspace to be deleted
	 */
	FDeleteWorkspace(FStringView InWorkspaceName)
		: WorkspaceName(InWorkspaceName)
	{

	}

	const FString& GetWorkspaceName() const
	{
		return WorkspaceName;
	}

	// ISourceControlOperation interface

	virtual FName GetName() const override
	{
		return "DeleteWorkspace";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_DeleteWorkspaceOperation", "Deleting a workspace...");
	}

protected:

	FString WorkspaceName;
};

/**
 * This operation uses p4v print command to get file from specified depot by shelved changelist or file revision returns a package filename
 */
class FGetFile : public FSourceControlOperationBase
{
public:

	FGetFile(const FString& InChangelistNumber, const FString& InRevisionNumber, const FString& InDepotFilePath, bool bInIsShelve = false)
		: ChangelistNumber(InChangelistNumber)
		, RevisionNumber(InRevisionNumber)
		, DepotFilePath(InDepotFilePath)
		, bIsShelve(bInIsShelve)
	{}

	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "GetFile";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_GetFile", "Retrieving file from source control...");
	}

	const FString& GetChangelistNumber() const { return ChangelistNumber; }

	const FString& GetRevisionNumber() const { return RevisionNumber; }

	const FString& GetDepotFilePath() const { return DepotFilePath; }

	bool IsShelve() const { return bIsShelve; }

	const FString& GetOutPackageFilename() const { return OutPackageFilename; }

	void SetOutPackageFilename(const FString& InOutPackageFilename)
	{
		OutPackageFilename = InOutPackageFilename;
	}

private:

	FString ChangelistNumber;
	FString RevisionNumber;
	FString DepotFilePath;
	bool bIsShelve;

	FString OutPackageFilename;
};

#undef LOCTEXT_NAMESPACE
