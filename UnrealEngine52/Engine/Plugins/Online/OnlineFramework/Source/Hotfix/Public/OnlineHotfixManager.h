// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/ConfigCacheIni.h"
#include "Interfaces/OnlineTitleFileInterface.h"
#include "OnlineHotfixManager.generated.h"

struct FCloudFileHeader;

HOTFIX_API DECLARE_LOG_CATEGORY_EXTERN(LogHotfixManager, Display, All);

class FAsyncLoadingFlushContext;

UENUM()
enum class EHotfixResult : uint8
{
	/** Failed to apply the hotfix */
	Failed,
	/** Hotfix succeeded and is ready to go */
	Success,
	/** Hotfix process succeeded but there were no changes applied */
	SuccessNoChange,
	/** Hotfix succeeded and requires the current level to be reloaded to take effect */
	SuccessNeedsReload,
	/** Hotfix succeeded and requires the process restarted to take effect */
	SuccessNeedsRelaunch
};

/**
 * Delegate fired when a check for hotfix files (but not application) completes
 *
 * @param EHotfixResult status on what happened
 */
DECLARE_DELEGATE_OneParam(FOnHotfixAvailableComplete, EHotfixResult);

/**
 * Delegate fired when the hotfix process has completed
 *
 * @param EHotfixResult status on what happened
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHotfixComplete, EHotfixResult);
typedef FOnHotfixComplete::FDelegate FOnHotfixCompleteDelegate;

/**
 * Delegate fired as progress of hotfix file reading happens
 *
 * @param NumDownloaded the number of files downloaded so far
 * @param TotalFiles the total number of files part of the hotfix
 * @param NumBytes the number of bytes processed so far
 * @param TotalBytes the total size of the hotfix data
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnHotfixProgress, uint32, uint32, uint64, uint64);
typedef FOnHotfixProgress::FDelegate FOnHotfixProgressDelegate;

/**
 * Delegate fired as each file is applied
 *
 * @param FriendlyName the human readable version of the file name (DefaultEngine.ini)
 * @param CachedFileName the full path to the file on disk
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHotfixProcessedFile, const FString&, const FString&);
typedef FOnHotfixProcessedFile::FDelegate FOnHotfixProcessedFileDelegate;

/**
 * This class manages the downloading and application of hotfix data
 * Hotfix data is a set of non-executable files downloaded and applied to the game.
 * The base implementation knows how to handle INI, PAK, and locres files.
 * NOTE: Each INI/PAK file must be prefixed by the platform name they are targeted at
 */
UCLASS(Config=Engine)
class HOTFIX_API UOnlineHotfixManager :
	public UObject
{
	GENERATED_BODY()

protected:
	/** The online interface to use for downloading the hotfix files */
	IOnlineTitleFilePtr OnlineTitleFile;

	/** Callbacks for when the title file interface is done */
	FOnEnumerateFilesCompleteDelegate OnEnumerateFilesCompleteDelegate;
	FOnReadFileProgressDelegate OnReadFileProgressDelegate;
	FOnReadFileCompleteDelegate OnReadFileCompleteDelegate;
	FDelegateHandle OnEnumerateFilesCompleteDelegateHandle;
	FDelegateHandle OnEnumerateFilesForAvailabilityCompleteDelegateHandle;
	FDelegateHandle OnReadFileProgressDelegateHandle;
	FDelegateHandle OnReadFileCompleteDelegateHandle;

	/**
	 * Delegate fired when the hotfix process has completed
	 *
	 * @param status of the hotfix process
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnHotfixComplete, EHotfixResult);

	/**
	 * Delegate fired as the hotfix files are read
	 */
	DEFINE_ONLINE_DELEGATE_FOUR_PARAM(OnHotfixProgress, uint32, uint32, uint64, uint64);

	/**
	 * Delegate fired as the hotfix files are applied
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnHotfixProcessedFile, const FString&, const FString&);

	struct FPendingFileDLProgress
	{
		uint64 Progress;

		FPendingFileDLProgress()
		{
			Progress = 0;
		}
	};

	struct FConfigFileBackup
	{
		/** Name of the ini file backed up*/
		FString IniName;
		/** Previous ini data backed up */
		FConfigFile ConfigData;
		/** UClasses reloaded as a result of the current ini */
		TArray<FString> ClassesReloaded;
	};

	/** Holds which files are pending download */
	TMap<FString, FPendingFileDLProgress> PendingHotfixFiles;
	/** The filtered list of files that are part of the hotfix */
	TArray<FCloudFileHeader> HotfixFileList;
	/** The last set of hotfix files that was applied so we can determine whether we are up to date or not */
	TArray<FCloudFileHeader> LastHotfixFileList;
	/** The set of hotfix files that have changed from the last time we applied them */
	TArray<FCloudFileHeader> ChangedHotfixFileList;
	/** The set of hotfix files that have been removed from the last time we applied them */
	TArray<FCloudFileHeader> RemovedHotfixFileList;
	/** Holds which files have been mounted for unmounting */
	TArray<FString> MountedPakFiles;
	/** Backup copies of INI files that change during hotfixing so they can be undone afterward */
	TArray<FConfigFileBackup> IniBackups;
	/** Used to match any PAK files for this platform */
	FString PlatformPrefix;
	/** Used to match any server-only hotfixes */
	FString ServerPrefix;
	/** Normally will be "Default" but could be different if we have a debug prefix */
	FString DefaultPrefix;
	/** Holds a chunk of string that will be swapped for Game during processing pak files (MyGame/Content/Maps -> /Game/Maps) */
	FString GameContentPath;
	/** Tracks how many files are being processed as part of the hotfix */
	uint32 TotalFiles;
	uint32 NumDownloaded;
	/** Tracks the size of the files being processed as part of the hotfix */
	uint64 TotalBytes;
	uint64 NumBytes;
	/** Some title file interfaces aren't re-entrant so handle it ourselves */
	bool bHotfixingInProgress;
	/** Asynchronously flush async loading before starting the hotfixing process. */
	TUniquePtr<FAsyncLoadingFlushContext> AsyncFlushContext;
	/** Set to true if any PAK file contains an update to a level that is currently loaded */
	bool bHotfixNeedsMapReload;
#if !UE_BUILD_SHIPPING
	/** Whether we want to log all of the files that are in a mounted pak file or not */
	bool bLogMountedPakContents;
#endif
	/**
	 * If we have removed or changed a currently mounted PAK file, then we'll need to restart the app
	 * because there's no simple undo for objects that were loaded and possibly rooted
	 */
	uint32 ChangedOrRemovedPakCount;
	/** Our passed-in World */
	TWeakObjectPtr<UWorld> OwnerWorld;

	virtual void Init();
	virtual void Cleanup();
	/** Looks at each file returned via the hotfix and processes them */
	EHotfixResult ApplyHotfix();
	/** Cleans up and fires the delegate indicating it's done */
	void TriggerHotfixComplete(EHotfixResult HotfixResult);
	/** Checks each file listed to see if it is a hotfix file to process */
	void FilterHotfixFiles();
	/** Starts the async reading process for the hotfix files */
	void ReadHotfixFiles();
	/** Unmounts any changed PAK files so they can be re-mounted after downloading */
	void UnmountHotfixFiles();
	/** Stores off the INI file for restoration later */
	FConfigFileBackup& BackupIniFile(const FString& IniName, const FConfigFile* ConfigFile);
	/** Restores any changed INI files to their default loaded state */
	void RestoreBackupIniFiles();
	/** Builds the list of files that are different between two runs of the hotfix process */
	void BuildHotfixFileListDeltas();

	/** Called once the list of hotfix files has been retrieved */
	void OnEnumerateFilesComplete(bool bWasSuccessful, const FString& ErrorStr);
	/** Called once the list of hotfix files has been retrieved and we only want to see if a hotfix is necessary */
	void OnEnumerateFilesForAvailabilityComplete(bool bWasSuccessful, const FString& ErrorStr, FOnHotfixAvailableComplete InCompletionDelegate);
	/** Called as files are downloaded to determine when to apply the hotfix data */
	void OnReadFileComplete(bool bWasSuccessful, const FString& FileName);
	/** Called as files are downloaded to provide progress notifications */
	void OnReadFileProgress(const FString& FileName, uint64 BytesRead);

	/** @return the config file entry for the ini file name in question */
	FConfigFile* GetConfigFile(const FString& IniName);

	/** @return the config cache key used to associate ini file entries within the config cache */
	FString BuildConfigCacheKey(const FString& IniName);
	/** @return the config file name after stripping any extra info (platform, debug prefix, etc.) */
	virtual FString GetStrippedConfigFileName(const FString& IniName);

	/** @return the human readable name of the file */
	const FString GetFriendlyNameFromDLName(const FString& DLName) const;

	virtual void PostInitProperties() override;

	bool IsMapLoaded(const FString& MapName);

	/** @return our current world */
	UWorld* GetWorld() const override;

protected:

	/**
	 * Is this hotfix file compatible with the current build
	 * If the file has version information it is compared with compatibility
	 * If the file has NO version information it is assumed compatible
	 *
	 * @param InFilename name of the file to check
	 * @param OutFilename name of file with version information stripped
	 *
	 * @return true if file is compatible, false otherwise
	 */
	bool IsCompatibleHotfixFile(const FString& InFilename, FString& OutFilename);

	/**
	 * Override this method to look at the file information for any game specific hotfix processing
	 * NOTE: Make sure to call Super to get default handling of files
	 *
	 * @param FileHeader - the information about the file to determine if it needs custom processing
	 *
	 * @return true if the file needs some kind of processing, false to have hotfixing ignore the file
	 */
	virtual bool WantsHotfixProcessing(const FCloudFileHeader& FileHeader);
	/**
	 * Called when a file needs custom processing (see above). Override this to provide your own processing methods
	 *
	 * @param FileHeader - the header information for the file in question
	 *
	 * @return whether the file was successfully processed
	 */
	virtual bool ApplyHotfixProcessing(const FCloudFileHeader& FileHeader);
	/**
	* Called prior to reading the file data.
	*
	* @param FileData - byte data of the hotfix file
	*
	* @return whether the file was successfully preprocessed
	*/
	virtual bool PreProcessDownloadedFileData(TArray<uint8>& FileData) const { return true;	}
	/**
	 * Override this to change the default INI file handling (merge delta INI changes into the config cache)
	 *
	 * @param FileName - the name of the INI being merged into the config cache
	 * @param IniData - the contents of the INI file (expected to be delta contents not a whole file)
	 *
	 * @return whether the merging was successful or not
	 */
	virtual bool HotfixIniFile(const FString& FileName, const FString& IniData);
	/**
	 * Override this to change the default PAK file handling:
	 *		- mount PAK file immediately
	 *		- Scan for any INI files contained within the PAK file and merge those in
	 *
	 * @param FileName - the name of the PAK file being mounted
	 *
	 * @return whether the mounting of the PAK file was successful or not
	 */
	virtual bool HotfixPakFile(const FCloudFileHeader& FileHeader);
	/**
	 * Override this to change the default INI file handling (merge whole INI files into the config cache)
	 *
	 * @param FileName - the name of the INI being merged into the config cache
	 *
	 * @return whether the merging was successful or not
	 */
	virtual bool HotfixPakIniFile(const FString& FileName);

	/**
	 * Override this to change the default caching directory
	 *
	 * @return the default caching directory
	 */
	virtual FString GetCachedDirectory()
	{
		return FPaths::ProjectPersistentDownloadDir();
	}

	/** Notify used by CheckAvailability() */
	virtual void OnHotfixAvailablityCheck(const TArray<FCloudFileHeader>& PendingChangedFiles, const TArray<FCloudFileHeader>& PendingRemoveFiles);

	/** Finds the header associated with the file name */
	FCloudFileHeader* GetFileHeaderFromDLName(const FString& FileName);

	/** Fires the progress delegate with our updated progress */
	void UpdateProgress(uint32 FileCount, uint64 UpdateSize);

	virtual bool ShouldWarnAboutMissingWhenPatchingFromIni(const FString& AssetPath) const { return true; }

	/** Called after any hotfixes are applied to apply last-second changes to certain asset types from .ini file data */
	virtual void PatchAssetsFromIniFiles();
	
	/** Used in PatchAssetsFromIniFiles to hotfix only a row in a table. 
	 *  If ChangedTables is not null then HandleDataTableChanged will not be called and the caller should call it on the data tables in ChangedTables when they're ready to
	 */
	void HotfixRowUpdate(UObject* Asset, const FString& AssetPath, const FString& RowName, const FString& ColumnName, const FString& NewValue, TArray<FString>& ProblemStrings, TSet<class UDataTable*>* ChangedTables = nullptr);
	
	/** Used in PatchAssetsFromIniFiles to hotfix an entire table. */
	void HotfixTableUpdate(UObject* Asset, const FString& AssetPath, const FString& JsonData, TArray<FString>& ProblemStrings);

	/** Called after modifying table values by HotfixRowUpdate() */
	virtual void OnHotfixTableValueInt64(UObject& Asset, const FString& RowName, const FString& ColumnName, const int64& OldValue, const int64& NewValue) { }
	virtual void OnHotfixTableValueDouble(UObject& Asset, const FString& RowName, const FString& ColumnName, const double& OldValue, const double& NewValue) { }
	virtual void OnHotfixTableValueFloat(UObject& Asset, const FString& RowName, const FString& ColumnName, const float& OldValue, const float& NewValue) { }
	virtual void OnHotfixTableValueString(UObject& Asset, const FString& RowName, const FString& ColumnName, const FString& OldValue, const FString& NewValue) { }
	virtual void OnHotfixTableValueName(UObject& Asset, const FString& RowName, const FString& ColumnName, const FName& OldValue, const FName& NewValue) { }
	virtual void OnHotfixTableValueObject(UObject& Asset, const FString& RowName, const FString& ColumnName, const UObject* OldValue, const UObject* NewValue) { }
	virtual void OnHotfixTableValueSoftObject(UObject& Asset, const FString& RowName, const FString& ColumnName, const FSoftObjectPtr& OldValue, const FSoftObjectPtr& NewValue) { }

	virtual bool ShouldPerformHotfix();

	/** Allow the application to override the dedicated server filename prefix. */
	virtual FString GetDedicatedServerPrefix() const;

public:
	UOnlineHotfixManager();
	UOnlineHotfixManager(FVTableHelper& Helper);
	virtual ~UOnlineHotfixManager();

	/** Tells the hotfix manager which OSS to use. Uses the default if empty */
	UPROPERTY(Config)
	FString OSSName;

	/** Tells the factory method which class to contruct */
	UPROPERTY(Config)
	FString HotfixManagerClassName;

	/** Used to prevent development work from interfering with playtests, etc. */
	UPROPERTY(Config)
	FString DebugPrefix;

	/** Starts the fetching of hotfix data from the OnlineTitleFileInterface that is registered for this game */
	UFUNCTION(BlueprintCallable, Category="Hotfix")
	virtual void StartHotfixProcess();

	/** Array of objects that we're forcing to remain resident because we've applied live hotfixes and won't get an
	    opportunity to reapply changes if the object is evicted from memory. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> AssetsHotfixedFromIniFiles;
	

	/** 
	 * Check for available hotfix files (but do not apply them) 
	 *
	 * @param InCompletionDelegate delegate to fire when the check is complete
	 */
	virtual void CheckAvailability(FOnHotfixAvailableComplete& InCompletionDelegate);

	/** Factory method that returns the configured hotfix manager */
	static UOnlineHotfixManager* Get(UWorld* World);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
