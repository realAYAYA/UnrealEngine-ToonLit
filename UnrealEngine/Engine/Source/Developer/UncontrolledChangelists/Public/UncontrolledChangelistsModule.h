// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UncontrolledChangelistState.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "Async/AsyncWork.h"

struct FAssetData;

/**
 * Interface for talking to Uncontrolled Changelists
 */
class UNCONTROLLEDCHANGELISTS_API FUncontrolledChangelistsModule : public IModuleInterface
{
	typedef TMap<FUncontrolledChangelist, FUncontrolledChangelistStateRef> FUncontrolledChangelistsStateCache;

public:	
	static constexpr const TCHAR* VERSION_NAME = TEXT("version");
	static constexpr const TCHAR* CHANGELISTS_NAME = TEXT("changelists");
	static constexpr uint32 VERSION_NUMBER = 1;

	/** Callback called when the state of the Uncontrolled Changelist Module (or any Uncontrolled Changelist) changed */
	DECLARE_MULTICAST_DELEGATE(FOnUncontrolledChangelistModuleChanged);
	FOnUncontrolledChangelistModuleChanged OnUncontrolledChangelistModuleChanged;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Check whether uncontrolled changelist module is enabled.
	 */
	bool IsEnabled() const;

	/**
	 * Get the changelist state of each cached Uncontrolled Changelist.
	 */
	 TArray<FUncontrolledChangelistStateRef> GetChangelistStates() const;

	/**
	 * Called when file has been made writable. Adds the file to the reconcile list because we don't know yet if it will change.
	 * @param	InFilename		The file to be reconciled.
	 * @return True if the file have been handled by the Uncontrolled module.
	 */
	bool OnMakeWritable(const FString& InFilename);
	 	 
	/**
	 * Called when file has been saved without an available Provider. Adds the file to the Default Uncontrolled Changelist
	 * @param	InFilename		The file to be added.
	 * @return	True if the file have been handled by the Uncontrolled module.
	 */
	bool OnSaveWritable(const FString& InFilename);

	/**
	 * Called when file has been deleted without an available Provider. Adds the file to the Default Uncontrolled Changelist
	 * @param	InFilename		The file to be added.
	 * @return	True if the file have been handled by the Uncontrolled module.
	 */
	bool OnDeleteWritable(const FString& InFilename);

	/**
	 * Called when files should have been marked for add without an available Provider. Adds the files to the Default Uncontrolled Changelist
	 * @param	InFilenames		The files to be added.
	 * @return	True if the files have been handled by the Uncontrolled module.
	 */
	bool OnNewFilesAdded(const TArray<FString>& InFilenames);

	/**
	 * Updates the status of Uncontrolled Changelists and files.
	 */
	void UpdateStatus();

	/**
	 * Gets a reference to the UncontrolledChangelists module
	 * @return A reference to the UncontrolledChangelists module.
	 */
	static inline FUncontrolledChangelistsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUncontrolledChangelistsModule>(GetModuleName());
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( GetModuleName() );
	}

	static FName GetModuleName()
	{
		static FName UncontrolledChangelistsModuleName("UncontrolledChangelists");
		return UncontrolledChangelistsModuleName;
	}

	/**
	 * Gets a message indicating the status of SCC coherence.
	 * @return 	A text representing the status of SCC.
	 */
	FText GetReconcileStatus() const;

	/** Called when "Reconcile assets" button is clicked. Checks for uncontrolled modifications in previously added assets.
	 *	Adds modified files to Uncontrolled Changelists
	 *  @return True if new modifications found
	 */
	bool OnReconcileAssets();

	/**
	 * Delegate callback called when assets are added to AssetRegistry.
	 * @param 	AssetData 	The asset just added.
	 */
	void OnAssetAdded(const FAssetData& AssetData);

	/**
	 * Delegate callback called when an asset is loaded.
	 * @param 	InAsset 	The loaded asset.
	 */
	void OnAssetLoaded(UObject* InAsset);

	/** Called when "Revert files" button is clicked. Reverts modified files and deletes new ones.
	 *  @param	InFilenames		The files to be reverted
	 *	@return true if the provided files were reverted.
	 */
	bool OnRevert(const TArray<FString>& InFilenames);
	
	/**
	 * Delegate callback called before an asset has been written to disk.
	 * @param 	InObject 			The saved object.
	 * @param 	InPreSaveContext 	Interface used to access saved parameters.
	 */
	void OnObjectPreSaved(UObject* InObject, const FObjectPreSaveContext& InPreSaveContext);

	/**
	 * Moves files to an Uncontrolled Changelist.
	 * @param 	InControlledFileStates 		The Controlled files to move.
	 * @param 	InUncontrolledFileStates 	The Uncontrolled files to move.
	 * @param 	InUncontrolledChangelist 	The Uncontrolled Changelist where to move the files.
	 */
	void MoveFilesToUncontrolledChangelist(const TArray<FSourceControlStateRef>& InControlledFileStates, const TArray<FSourceControlStateRef>& InUncontrolledFileStates, const FUncontrolledChangelist& InUncontrolledChangelist);

	/**
	* Moves files to an Uncontrolled Changelist.
	* @param 	InControlledFileStates 		The Controlled files to move.
	* @param 	InUncontrolledChangelist 	The Uncontrolled Changelist where to move the files.
	*/
	void MoveFilesToUncontrolledChangelist(const TArray<FString>& InControlledFiles, const FUncontrolledChangelist& InUncontrolledChangelist);

	/**
	 * Moves files to a Controlled Changelist.
	 * @param 	InUncontrolledFileStates 	The files to move.
	 * @param 	InChangelist 				The Controlled Changelist where to move the files.
	 * @param 	InOpenConflictDialog 		A callback to be used by the method when file conflicts are detected. The callback should display the files and ask the user if they should proceed.
	 */
	void MoveFilesToControlledChangelist(const TArray<FSourceControlStateRef>& InUncontrolledFileStates, const FSourceControlChangelistPtr& InChangelist, TFunctionRef<bool(const TArray<FSourceControlStateRef>&)> InOpenConflictDialog);
	
	/**
	 * Moves files to a Controlled Changelist.
	 * @param 	InUncontrolledFiles 	The files to move.
	 * @param 	InChangelist 			The Controlled Changelist where to move the files.
	 * @param 	InOpenConflictDialog 	A callback to be used by the method when file conflicts are detected. The callback should display the files and ask the user if they should proceed.
	 */
	void MoveFilesToControlledChangelist(const TArray<FString>& InUncontrolledFiles, const FSourceControlChangelistPtr& InChangelist, TFunctionRef<bool(const TArray<FSourceControlStateRef>&)> InOpenConflictDialog);

	/**
	 * Creates a new Uncontrolled Changelist.
	 * @param	InDescription	The description of the newly created Uncontrolled Changelist.
	 * return	TOptional<FUncontrolledChangelist> set with the new Uncontrolled Changelist key if succeeded.
	 */
	TOptional<FUncontrolledChangelist> CreateUncontrolledChangelist(const FText& InDescription);
	
	/**
     * Edits an Uncontrolled Changelist's description
	 * @param	InUncontrolledChangelist	The Uncontrolled Changelist to modify. Should not be the default Uncontrolled Changelist.
	 * @param	InNewDescription			The description to set.
	 */
	void EditUncontrolledChangelist(const FUncontrolledChangelist& InUncontrolledChangelist, const FText& InNewDescription);
	
	/**
	 * Deletes an Uncontrolled Changelist.
	 * @param	InUncontrolledChangelist	The Uncontrolled Changelist to delete. Should not be the default Uncontrolled Changelist and should not contain files.
	 */
	void DeleteUncontrolledChangelist(const FUncontrolledChangelist& InUncontrolledChangelist);

private:
	/**
	 * Helper use by Startup task and OnAssetLoaded delegate.
	 * @param 	InAssetData 		The asset just added.
	 * @param  	InAddedAssetsCache 	The cache to add the asset to.
	 * @param	bInStartupTask		If true, this asset was added from the startup task.
	 * 
	 */
	void OnAssetAddedInternal(const FAssetData& InAssetData, TSet<FString>& InAddedAssetsCache, bool bInStartupTask);

	/**
	 * Add files to Uncontrolled Changelist. Also adds them to files to reconcile.
	 */
	bool AddToUncontrolledChangelist(const TArray<FString>& InFilenames);

	/**
	 * Saves the state of UncontrolledChangelists to Json for persistency.
	 */
	void SaveState() const;
	
	/**
	 * Restores the previously saved state from Json.
	 */
	void LoadState();
		
	/**
	 * Called on End of frame. Calls SaveState if needed.
	 */
	void OnEndFrame();

	/**
	 * Helper returning the location of the file used for persistency.
	 * @return 	A string containing the filepath.
	 */
	FString GetPersistentFilePath() const;

	/**
	 * Helper returning the package path where an UObject is located.
	 * @param 	InObject 	The object used to locate the package.
	 * @return 	A String containing the filepath of the package.
	 */
	FString GetUObjectPackageFullpath(const UObject* InObject) const;

	/** Called when a state changed either in the module or an Uncontrolled Changelist. */
	void OnStateChanged();

	/** Removes from asset caches files already present in Uncontrolled Changelists */
	void CleanAssetsCaches();

	/**
	 * Try to add the provided filenames to the default Uncontrolled Changelist.
	 * @param 	InFilenames 	The files to add.
	 * @param 	InCheckFlags 	The required checks to check the file against before adding.
	 * @return 	True file have been added.
	 */
	bool AddFilesToDefaultUncontrolledChangelist(const TArray<FString>& InFilenames, const FUncontrolledChangelistState::ECheckFlags InCheckFlags);

	/** Returns the default Uncontrolled Changelist state, creates it if it does not exist. */
	FUncontrolledChangelistStateRef GetDefaultUncontrolledChangelistState();

private:
	class FStartupTask : public FNonAbandonableTask
	{
	public:
		
		FStartupTask(FUncontrolledChangelistsModule* InOwner)
			: Owner(InOwner)
		{}
		~FStartupTask() {}
		
		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FStartupTask, STATGROUP_ThreadPoolAsyncTasks);
		}
		
		void DoWork();
		const TSet<FString>& GetAddedAssetsCache() const { return AddedAssetsCache; }
	
	private:
		FUncontrolledChangelistsModule* Owner;
		TSet<FString> AddedAssetsCache;
	};

	// Used to determine if the initial Asset Registry scan was completed or the module was shutdown
	struct FInitialScanEvent : public TSharedFromThis<FInitialScanEvent> {};
	TSharedPtr<FInitialScanEvent> InitialScanEvent;

	TUniquePtr<FAsyncTask<FStartupTask>> StartupTask;
	FUncontrolledChangelistsStateCache	UncontrolledChangelistsStateCache;
	TSet<FString>						AddedAssetsCache;
	FDelegateHandle						OnAssetAddedDelegateHandle;
	FDelegateHandle						OnObjectPreSavedDelegateHandle;
	FDelegateHandle						OnEndFrameDelegateHandle;
	bool								bIsEnabled = false;
	bool								bIsStateDirty = false;
};
