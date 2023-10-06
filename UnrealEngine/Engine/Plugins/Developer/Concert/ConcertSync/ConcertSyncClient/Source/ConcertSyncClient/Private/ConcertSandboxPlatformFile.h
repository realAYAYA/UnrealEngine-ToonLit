// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "ConcertClientPersistData.h"

class ISourceControlProvider;

struct FFileChangeData;

class FConcertSandboxPlatformFilePath
{
public:
	explicit FConcertSandboxPlatformFilePath(FString&& InNonSandboxPath)
		: SandboxPath()
		, NonSandboxPath(MoveTemp(InNonSandboxPath))
	{
	}

	FConcertSandboxPlatformFilePath(FString&& InNonSandboxPath, FString&& InSandboxPath)
		: SandboxPath(MoveTemp(InSandboxPath))
		, NonSandboxPath(MoveTemp(InNonSandboxPath))
	{
	}

	static FConcertSandboxPlatformFilePath CreateSandboxPath(FString&& InNonSandboxPath, const FConcertSandboxPlatformFilePath& InRootPath)
	{
		checkf(InRootPath.HasSandboxPath(), TEXT("Root '%s' had no sandbox path set!"), *InRootPath.GetNonSandboxPath());
		return CreateSandboxPath(MoveTemp(InNonSandboxPath), InRootPath.GetSandboxPath(), InRootPath.GetNonSandboxPath());
	}

	static FConcertSandboxPlatformFilePath CreateSandboxPath(FString&& InNonSandboxPath, const FString& InRootSandboxPath, const FString& InRootNonSandboxPath)
	{
		// Mount point are stored with a trailing slash to prevent matching mount point with similar names -> (/Bla/Content, /Bla/ContentSupreme)
		// An extra slash is appended here to make sure with can match mount point directly -> (/Bla/Content match /Bla/Content/)
		FString ResolvedSandboxPath = InNonSandboxPath + TEXT("/");
		checkf(ResolvedSandboxPath.StartsWith(InRootNonSandboxPath), TEXT("Path '%s' was not under the root '%s'!"), *InNonSandboxPath, *InRootNonSandboxPath);
		ResolvedSandboxPath.ReplaceInline(*InRootNonSandboxPath, *InRootSandboxPath);
		ResolvedSandboxPath.RemoveAt(ResolvedSandboxPath.Len() - 1, 1, false);
		return FConcertSandboxPlatformFilePath(MoveTemp(InNonSandboxPath), MoveTemp(ResolvedSandboxPath));
	}

	static FConcertSandboxPlatformFilePath CreateNonSandboxPath(FString&& InSandboxPath, const FConcertSandboxPlatformFilePath& InRootPath)
	{
		checkf(InRootPath.HasSandboxPath(), TEXT("Root '%s' had no sandbox path set!"), *InRootPath.GetNonSandboxPath());
		return CreateNonSandboxPath(MoveTemp(InSandboxPath), InRootPath.GetSandboxPath(), InRootPath.GetNonSandboxPath());
	}

	static FConcertSandboxPlatformFilePath CreateNonSandboxPath(FString&& InSandboxPath, const FString& InRootSandboxPath, const FString& InRootNonSandboxPath)
	{
		// Mount point are stored with a trailing slash to prevent matching mount point with similar names -> (/Bla/Content, /Bla/ContentSupreme)
		// An extra slash is appended here to make sure with can match mount point directly -> (/Bla/Content match /Bla/Content/)
		FString ResolvedNonSandboxPath = InSandboxPath + TEXT("/");
		checkf(ResolvedNonSandboxPath.StartsWith(InRootSandboxPath), TEXT("Path '%s' was not under the root '%s'!"), *InRootSandboxPath, *InRootSandboxPath);
		ResolvedNonSandboxPath.ReplaceInline(*InRootSandboxPath, *InRootNonSandboxPath);
		ResolvedNonSandboxPath.RemoveAt(ResolvedNonSandboxPath.Len() - 1, 1, false);
		return FConcertSandboxPlatformFilePath(MoveTemp(ResolvedNonSandboxPath), MoveTemp(InSandboxPath));
	}

	/** Copyable */
	FConcertSandboxPlatformFilePath(const FConcertSandboxPlatformFilePath&) = default;
	FConcertSandboxPlatformFilePath& operator=(const FConcertSandboxPlatformFilePath&) = default;

	/** Movable */
	FConcertSandboxPlatformFilePath(FConcertSandboxPlatformFilePath&&) = default;
	FConcertSandboxPlatformFilePath& operator=(FConcertSandboxPlatformFilePath&&) = default;

	bool operator==(const FConcertSandboxPlatformFilePath& Rhs) const
	{
		return GetSandboxPath() == Rhs.GetSandboxPath() && GetNonSandboxPath() == Rhs.GetNonSandboxPath();
	}

	/** Do we have a sandbox path set? */
	bool HasSandboxPath() const
	{
		return SandboxPath.Len() > 0;
	}

	/** Get the absolute sandbox path */
	const FString& GetSandboxPath() const
	{
		return SandboxPath;
	}

	/** Get the absolute non-sandbox path */
	const FString& GetNonSandboxPath() const
	{
		return NonSandboxPath;
	}

private:
	/** Absolute sandbox path */
	FString SandboxPath;

	/** Absolute non-sandbox path */
	FString NonSandboxPath;
};

// Using only the non sandbox path for our hash should be enough
FORCEINLINE uint32 GetTypeHash(const FConcertSandboxPlatformFilePath& SandboxFilePath)
{
	return GetTypeHash(SandboxFilePath.GetNonSandboxPath());
}

class FConcertSandboxPlatformFile : public IPlatformFile
{
public:
	explicit FConcertSandboxPlatformFile(const FString& InSandboxRootPath);
	virtual ~FConcertSandboxPlatformFile();

	static const TCHAR* GetTypeName()
	{
		return TEXT("ConcertSandboxFile");
	}

	//~ For visibility of overloads we don't override
	using IPlatformFile::IterateDirectory;
	using IPlatformFile::IterateDirectoryStat;

	//~ IPlatformFile overrides
	virtual void SetSandboxEnabled(bool bInEnabled) override;
	virtual bool IsSandboxEnabled() const override;
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual void Tick() override;
	virtual IPlatformFile* GetLowerLevel() override;
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override;
	virtual const TCHAR* GetName() const override;
	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override;
	virtual void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags MinPriority) override;
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) override;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) override;

	/**
	 *	Persist the file list from the sandbox state onto the real files Will mark files for which the operation was
	 *	succesful as persisted.
	 *
	 *	@param InFiles	List of files names to persist as full paths.
	 *	@param InParams	Parameters controlling the persist options specified by the user.
	 *	@return Returns a persist result object that contains result status.
	 */
	FPersistResult PersistSandbox(TArrayView<const FString> InFiles, FPersistParameters InParams);

	/** 
	 *	Discard the sandbox state
	 *	This will trigger directory watcher notifications for files that are restored.
	 *	This will also gather packages that need to be hot reloaded or purge from memory
	 *	@param OutPackagePendingHotReload	Array to be filled with package name that need to be hot reloaded
	 *	@param OutPackagesPendingPurge		Array to be filled with package name that need to be purged
	 */
	void DiscardSandbox(TArray<FName>& OutPackagesPendingHotReload, TArray<FName>& OutPackagesPendingPurge);

	/**
	 *	Add a list of files as persisted from the sandbox.
	 *	Persisted files will will ignored when returning changed files 
	 *	from the sandbox if the modified date match.
	 *
	 *	@param InFiles		List of files to add to the persisted list.
	 */
	void AddFilesAsPersisted(TArrayView<const FString> InFiles);

	/**
	 *	Remove a list of files as persisted from the sandbox.
	 *	@see AddFilesAsPersisted, PersistSandbox
	 *
	 *	@param InFiles		Array of files to remove
	 */
	void RemoveFilesAsPersisted(TArrayView<const FString> InFiles);

	/**
	 *	Return the list of files currently in the persisted list.
	 *	@see AddFilesAsPersisted, PersistSandbox
	 */
	const TMap<FString, FDateTime>& GetPersistedFiles() const;

	/** 
	 *	Gather all files changes that are currently in the sandbox
	 *	while ignoring files marked as persisted.
	 *	@return Array of filepaths in not particular order.
	 */
	TArray<FString> GatherSandboxChangedFilenames() const;

	/**
	 * Returns true if the given package file exists on non sandbox path.
	 */
	bool DeletedPackageExistsInNonSandbox(FString InFilename) const;

private:
	struct FDirectoryItem
	{
		FString Path;
		FFileStatData StatData;
	};

	struct FSandboxMountPoint
	{
		/** Sandbox path */
		FConcertSandboxPlatformFilePath Path;

		/** Sandbox directory watcher delegate handle (if any) */
		FDelegateHandle OnDirectoryChangedHandle;
	};

	/** Callback when a new content path is mounted */
	void OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath);

	/** Callback when an existing content path is unmounted */
	void OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath);

	/** Register a mount path from a source content path */
	void RegisterContentMountPath(const FString& InContentPath);

	/** Unregister a mount path from a source content path */
	void UnregisterContentMountPath(const FString& InContentPath);

	/** Resolve the given path to its sandbox path (if any) */
	FConcertSandboxPlatformFilePath ToSandboxPath(FString InFilename, const bool bEvenIfDisabled = false) const;

	/** Resolve the given path to its sandbox path (if any) from an absolute filename */
	FConcertSandboxPlatformFilePath ToSandboxPath_Absolute(FString InFilename, const bool bEvenIfDisabled = false) const;

	/** Resolve the given path to its non-sandbox path (if any) */
	FConcertSandboxPlatformFilePath FromSandboxPath(FString InFilename) const;

	/** Resolve the given path to its non-sandbox path (if any) from an absolute filename */
	FConcertSandboxPlatformFilePath FromSandboxPath_Absolute(FString InFilename) const;

	/** Check whether the given absolute sandbox path has been explicitly deleted from the sandbox */
	bool IsPathDeleted(const FConcertSandboxPlatformFilePath& InPath) const;

	/** Set whether the given absolute sandbox path has been explicitly deleted from the sandbox */
	void SetPathDeleted(const FConcertSandboxPlatformFilePath& InPath, const bool bIsDeleted);

	/** Notify that a file has been explicitly deleted from the sandbox */
	void NotifyFileDeleted(const FConcertSandboxPlatformFilePath& InPath);

	/** Helper function to ensure that a sandbox contains a copy of the non-sandbox file (eg, prior to opening an existing file for writing) - does nothing if the sandbox already has the file, or if there is no non-sandbox file to copy */
	void MigrateFileToSandbox(const FConcertSandboxPlatformFilePath& InPath) const;

	/** Helper function to get the contents of a directory, taking into account the sandbox state - paths are returned relative to InDirBase */
	TArray<FDirectoryItem> GetDirectoryContents(const FConcertSandboxPlatformFilePath& InPath, const TCHAR* InDirBase) const;

#if WITH_EDITOR
	/** Called when a file in a sandbox directory changes on disk */
	void OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges, FConcertSandboxPlatformFilePath MountPath);
#endif

	/** Copy file using source control (if enabled) */
	bool CopyFileWithSCC(const TCHAR* InToFilename, const TCHAR* InFromFilename, const FPersistParameters& InParam, FPersistResult& OutResult);

	/** Delete file using source control (if enabled) */
	bool DeleteFileWithSCC(const TCHAR* InFilename, const FPersistParameters& InParam, FPersistResult& OutResult);

	/** Root path of this sandbox */
	const FString SandboxRootPath;

	/** Underlying platform file that we're wrapping */
	IPlatformFile* LowerLevel;

	/** Is this sandbox currently enabled? */
	TAtomic<bool> bSandboxEnabled;

	/** Map of (files -> modified date) to ignore when returning sandbox changed files if the modified date match. */
	TMap<FString, FDateTime> PersistedFiles;

	/** Array of sandbox mount points */
	TArray<FSandboxMountPoint> SandboxMountPoints;
	/** Critical section protecting concurrent access to SandboxMountPoints */
	mutable FCriticalSection SandboxMountPointsCS;

	/** Set of absolute sandbox paths that have been explicitly deleted from the sandbox and shouldn't fallback to the non-sandbox items */
	TSet<FConcertSandboxPlatformFilePath> DeletedSandboxPaths;
	/** Critical section protecting concurrent access to DeletedSandboxPaths */
	mutable FCriticalSection DeletedSandboxPathsCS;
};
