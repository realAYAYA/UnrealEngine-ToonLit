// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "Toolkits/IToolkit.h"

#include "Containers/Ticker.h"
#include "Tools/Modes.h"
#include "Misc/NamePermissionList.h"
#include "AssetEditorSubsystem.generated.h"

class UAssetEditor;
class UEdMode;
class UObject;
class UClass;
struct FAssetEditorRequestOpenAsset;
class FEditorModeTools;
class IMessageContext;

/**
 * This class keeps track of a currently open asset editor; allowing it to be
 * brought into focus, closed, etc..., without concern for how the editor was
 * implemented.
 */
class UNREALED_API IAssetEditorInstance
{
public:

	virtual FName GetEditorName() const = 0;
	virtual void FocusWindow(UObject* ObjectToFocusOn = nullptr) = 0;
	virtual bool CloseWindow() = 0;
	virtual bool IsPrimaryEditor() const = 0;
	virtual void InvokeTab(const struct FTabId& TabId) = 0;
	UE_DEPRECATED(5.0, "Toolbar tab no longer exists and tab ID will return None; do not add it to layouts")
	virtual FName GetToolbarTabId() const { return NAME_None; }
	virtual TSharedPtr<class FTabManager> GetAssociatedTabManager() = 0;
	virtual double GetLastActivationTime() = 0;
	virtual void RemoveEditingAsset(UObject* Asset) = 0;
};

/** The way that editors were requested to close */
enum class EAssetEditorCloseReason : uint8
{
	CloseAllEditorsForAsset,
	CloseOtherEditors,
	RemoveAssetFromAllEditors,
	CloseAllAssetEditors,
};

struct UNREALED_API FRegisteredModeInfo
{
	TWeakObjectPtr<UClass> ModeClass;
	FEditorModeInfo ModeInfo;
};
using RegisteredModeInfoMap = TMap<FEditorModeID, FRegisteredModeInfo>;

/**
 * UAssetEditorSubsystem
 */
UCLASS()
class UNREALED_API UAssetEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UAssetEditorSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsAssetEditable(const UObject* Asset);

	/** Opens an asset by path */
	void OpenEditorForAsset(const FString& AssetPath);
	void OpenEditorForAsset(const FSoftObjectPath& AssetPath);

	/**
	 * Tries to open an editor for the specified asset.  Returns true if the asset is open in an editor.
	 * If the file is already open in an editor, it will not create another editor window but instead bring it to front
	 */
	bool OpenEditorForAsset(UObject* Asset, const EToolkitMode::Type ToolkitMode = EToolkitMode::Standalone, TSharedPtr<IToolkitHost> OpenedFromLevelEditor = TSharedPtr<IToolkitHost>(), const bool bShowProgressWindow = true);
	
	template<typename ObjectType>
	bool OpenEditorForAsset(TObjectPtr<ObjectType> Asset, const EToolkitMode::Type ToolkitMode = EToolkitMode::Standalone, TSharedPtr<IToolkitHost> OpenedFromLevelEditor = TSharedPtr<IToolkitHost>(), const bool bShowProgressWindow = true)
	{
		return OpenEditorForAsset(ToRawPtr(Asset), ToolkitMode, OpenedFromLevelEditor, bShowProgressWindow);
	}

	/**
	 * Tries to open an editor for all of the specified assets.
	 * If any of the assets are already open, it will not create a new editor for them.
	 * If all assets are of the same type, the supporting AssetTypeAction (if it exists) is responsible for the details of how to handle opening multiple assets at once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	bool OpenEditorForAssets(const TArray<UObject*>& Assets);
	bool OpenEditorForAssets_Advanced(const TArray<UObject*>& Assets, const EToolkitMode::Type ToolkitMode = EToolkitMode::Standalone, TSharedPtr<IToolkitHost> OpenedFromLevelEditor = TSharedPtr<IToolkitHost>());

	/** Opens editors for the supplied assets (via OpenEditorForAsset) */
	void OpenEditorsForAssets(const TArray<FString>& AssetsToOpen);
	void OpenEditorsForAssets(const TArray<FName>& AssetsToOpen);
	void OpenEditorsForAssets(const TArray<FSoftObjectPath>& AssetsToOpen);

	/** Returns the primary editor if one is already open for the specified asset.
	 * If there is one open and bFocusIfOpen is true, that editor will be brought to the foreground and focused if possible.
	 */
	IAssetEditorInstance* FindEditorForAsset(UObject* Asset, bool bFocusIfOpen);

	/** Returns all editors currently opened for the specified asset */
	TArray<IAssetEditorInstance*> FindEditorsForAsset(UObject* Asset);

	/** Returns all editors currently opened for the specified asset or any of its subobjects */
	TArray<IAssetEditorInstance*> FindEditorsForAssetAndSubObjects(UObject* Asset);

	/** Close all active editors for the supplied asset and return the number of asset editors that were closed */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	int32 CloseAllEditorsForAsset(UObject* Asset);

	/** Close any editor which is not this one */
	void CloseOtherEditors(UObject* Asset, IAssetEditorInstance* OnlyEditor);

	/** Remove given asset from all open editors */
	void RemoveAssetFromAllEditors(UObject* Asset);

	/** Event called when CloseAllEditorsForAsset/RemoveAssetFromAllEditors is called */
	DECLARE_EVENT_TwoParams(UAssetEditorSubsystem, FAssetEditorRequestCloseEvent, UObject*, EAssetEditorCloseReason);
	virtual FAssetEditorRequestCloseEvent& OnAssetEditorRequestClose() { return AssetEditorRequestCloseEvent; }

	/** Get all assets currently being tracked with open editors */
	TArray<UObject*> GetAllEditedAssets();

	/** Notify the asset editor manager that an asset was opened */
	void NotifyAssetOpened(UObject* Asset, IAssetEditorInstance* Instance);
	void NotifyAssetsOpened(const TArray< UObject* >& Assets, IAssetEditorInstance* Instance);

	/** Called when an asset has been opened in an editor */
	DECLARE_EVENT_TwoParams(UAssetEditorSubsystem, FOnAssetOpenedInEditorEvent, UObject*, IAssetEditorInstance*);
	virtual FOnAssetOpenedInEditorEvent& OnAssetOpenedInEditor() { return AssetOpenedInEditorEvent; }

	/** Notify the asset editor manager that an asset editor is being opened and before widgets are constructed */
	void NotifyEditorOpeningPreWidgets(const TArray< UObject* >& Assets, IAssetEditorInstance* Instance);

	/** Called when an asset editor is opening and before widgets are constructed */
	DECLARE_EVENT_TwoParams(UAssetEditorSubsystem, FOnAssetsOpenedInEditorEvent, const TArray<UObject*>&, IAssetEditorInstance*);
	virtual FOnAssetsOpenedInEditorEvent& OnEditorOpeningPreWidgets() { return EditorOpeningPreWidgetsEvent; }

	/** Notify the asset editor manager that an asset editor is done editing an asset */
	void NotifyAssetClosed(UObject* Asset, IAssetEditorInstance* Instance);

	/** Notify the asset editor manager that an asset was closed */
	void NotifyEditorClosed(IAssetEditorInstance* Instance);

	/** Close all open asset editors */
	bool CloseAllAssetEditors();

	/** Called when an asset editor is requested to be opened */
	DECLARE_EVENT_OneParam(UAssetEditorSubsystem, FAssetEditorRequestOpenEvent, UObject*);
	virtual FAssetEditorRequestOpenEvent& OnAssetEditorRequestedOpen() { return AssetEditorRequestOpenEvent; }

	/** Called when an asset editor is actually opened */
	DECLARE_EVENT_OneParam(UAssetEditorSubsystem, FAssetEditorOpenEvent, UObject*);
	FAssetEditorOpenEvent& OnAssetEditorOpened() { return AssetEditorOpenedEvent; }

	/** Request notification to restore the assets that were previously open when the editor was last closed */
	void RequestRestorePreviouslyOpenAssets();

	
	void RegisterUAssetEditor(UAssetEditor* NewAssetEditor);
	void UnregisterUAssetEditor(UAssetEditor* RemovedAssetEditor);
	
	/**
	 * Creates a scriptable editor mode based on ID name, which will be owned by the given Owner, if that name exists in the map of editor modes found at system startup.
	 * @param ModeID	ID of the mode to create.
	 * @param Owner		The tools ownership context that the mode should be created under.
	 *
	 * @return 			A pointer to the created UEdMode or nullptr, if the given ModeID does not exist in the set of known modes.
	 */
	UEdMode* CreateEditorModeWithToolsOwner(FEditorModeID ModeID, FEditorModeTools& Owner);
	
	/**
	 * Returns information about an editor mode, based on the given ID.
	 * @param ModeID		ID of the editor mode.
	 * @param OutModeInfo	The out struct where the mode information should be stored.
	 *
	 * @return 				True if OutModeInfo was filled out successfully, otherwise false.
	 */
	bool FindEditorModeInfo(const FEditorModeID& InModeID, FEditorModeInfo& OutModeInfo) const;
	
	/**
	 * Creates an array of all known FEditorModeInfos, sorted by their priority, from greatest to least.
	 *
	 * @return 			The sorted array of FEditorModeInfos.
	 */
	TArray<FEditorModeInfo> GetEditorModeInfoOrderedByPriority() const;

	/**
	 * Event that is triggered whenever a mode is registered or unregistered
	 */
	FRegisteredModesChangedEvent& OnEditorModesChanged();

	/**
	 * Event that is triggered whenever a mode is registered
	 */
	FOnModeRegistered& OnEditorModeRegistered();

	/**
	 * Event that is triggered whenever a mode is unregistered
	 */
	FOnModeUnregistered& OnEditorModeUnregistered();

	/** Get the permission list that controls which editor modes are exposed */
	FNamePermissionList& GetAllowedEditorModes();

private:

	/** Handles FAssetEditorRequestOpenAsset messages. */
	void HandleRequestOpenAssetMessage(const FAssetEditorRequestOpenAsset& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles ticks from the ticker. */
	bool HandleTicker(float DeltaTime);

	/** Spawn a notification asking the user if they want to restore their previously open assets */
	void SpawnRestorePreviouslyOpenAssetsNotification(const bool bCleanShutdown, const TArray<FString>& AssetsToOpen);

	/** Handler for when the "Restore Now" button is clicked on the RestorePreviouslyOpenAssets notification */
	void OnConfirmRestorePreviouslyOpenAssets(TArray<FString> AssetsToOpen);

	/** Handler for when the "Don't Restore" button is clicked on the RestorePreviouslyOpenAssets notification */
	void OnCancelRestorePreviouslyOpenAssets();

public:

	/**
	 * Saves a list of open asset editors so they can be restored on editor restart.
	 * @param bOnShutdown If true, this is handled as if the engine is shutting down right now.
	 */

	void SaveOpenAssetEditors(const bool bOnShutdown);
	
	/**
	 * Saves a list of open asset editors so they can be restored on editor restart.
	 * @param bOnShutdown If true, this is handled as if the engine is shutting down right now.
	 * @param bCancelIfDebugger If true, don't save a list of assets to restore if we are running under a debugger.
	 */
	UE_DEPRECATED(5.0, "Please use the version of SaveOpenAssetEditors with only one argument, bOnShutdown.")
	void SaveOpenAssetEditors(const bool bOnShutdown, const bool bCancelIfDebugger);

	/** Restore the assets that were previously open when the editor was last closed. */
	void RestorePreviouslyOpenAssets();

	/** Sets bAutoRestoreAndDisableSaving and sets bRequestRestorePreviouslyOpenAssets to false to avoid running RestorePreviouslyOpenAssets() twice. */
	void SetAutoRestoreAndDisableSaving(const bool bInAutoRestoreAndDisableSaving);

private:

	/** Handles a package being reloaded */
	void HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

	/** Callback for when the Editor closes, before Slate shuts down all the windows. */
	void OnEditorClose();

	void RegisterEditorModes();
	void UnregisterEditorModes();
	void OnSMInstanceElementsEnabled();

	bool IsEditorModeAllowed(const FName ModeId) const;

private:

	/** struct used by OpenedEditorTimes map to store editor names and times */
	struct FOpenedEditorTime
	{
		FName EditorName;
		FDateTime OpenedTime;
	};

	/** struct used to track total time and # of invocations during an overall UnrealEd session */
	struct FAssetEditorAnalyticInfo
	{
		FTimespan SumDuration;
		int32 NumTimesOpened;

		FAssetEditorAnalyticInfo()
			: SumDuration(0)
			, NumTimesOpened(0)
		{
		}
	};

	/**
	 * Utility struct for assets book keeping.
	 * We'll index assets using the raw pointer, as it should be possible to clear an entry after an asset has been garbage collected
	 * but any access to the UObject should go through the weak object pointer.
	 */
	struct FAssetEntry
	{
		// Implicit constructor
		FAssetEntry(UObject* InRawPtr)
			: RawPtr(InRawPtr)
			, ObjectPtr(InRawPtr)
		{
		}

		UObject* RawPtr;
		FWeakObjectPtr ObjectPtr;

		bool operator==(const FAssetEntry& Other) const { return RawPtr == Other.RawPtr; }

		inline friend uint32 GetTypeHash(const FAssetEntry& AssetEntry)
		{
			return GetTypeHash(AssetEntry.RawPtr);
		}
	};

	/** Holds the opened assets. */
	TMultiMap<FAssetEntry, IAssetEditorInstance*> OpenedAssets;

	/** Holds the opened editors. */
	TMultiMap<IAssetEditorInstance*, FAssetEntry> OpenedEditors;

	/** Holds the times that editors were opened. */
	TMap<IAssetEditorInstance*, FOpenedEditorTime> OpenedEditorTimes;

	/** Holds the cumulative time editors have been open by type. */
	TMap<FName, FAssetEditorAnalyticInfo> EditorUsageAnalytics;

private:
	/** Holds a delegate to be invoked when the widget ticks. */
	FTickerDelegate TickDelegate;

	/** Call to request closing editors for an asset */
	FAssetEditorRequestCloseEvent AssetEditorRequestCloseEvent;

	/** Called when an asset has been opened in an editor */
	FOnAssetOpenedInEditorEvent AssetOpenedInEditorEvent;

	/** Called when editor is opening and before widgets are constructed */
	FOnAssetsOpenedInEditorEvent EditorOpeningPreWidgetsEvent;

	/** Multicast delegate executed when an asset editor is requested to be opened */
	FAssetEditorRequestOpenEvent AssetEditorRequestOpenEvent;

	/** Multicast delegate executed when an asset editor is actually opened */
	FAssetEditorOpenEvent AssetEditorOpenedEvent;

	/** Flag whether we are currently shutting down */
	bool bSavingOnShutdown;
	
	/**
	 * Flag whether to disable SaveOpenAssetEditors() and enable auto-restore on RestorePreviouslyOpenAssets().
	 * Useful e.g., to allow LayoutsMenu.cpp re-load layouts on-the-fly and reload the previously opened assets.
	 * If true, SaveOpenAssetEditors() will not save any asset editor and RestorePreviouslyOpenAssets() will automatically open them without asking the user.
	 * If false, default behavior of both SaveOpenAssetEditors() and RestorePreviouslyOpenAssets().
	 */
	bool bAutoRestoreAndDisableSaving;

	/** Flag whether there has been a request to notify whether to restore previously open assets */
	bool bRequestRestorePreviouslyOpenAssets;

	/** A pointer to the notification used by RestorePreviouslyOpenAssets */
	TWeakPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotificationPtr;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAssetEditor>> OwnedAssetEditors;
	
	/** Map of FEditorModeId to EditorModeInfo for all known UEdModes when the subsystem initialized */
	RegisteredModeInfoMap EditorModes;

	/** Event that is triggered whenever a mode is unregistered */
	FRegisteredModesChangedEvent OnEditorModesChangedEvent;

	/** Event that is triggered whenever a mode is unregistered */
	FOnModeRegistered OnEditorModeRegisteredEvent;

	/** Event that is triggered whenever a mode is unregistered */
	FOnModeUnregistered OnEditorModeUnregisteredEvent;
	
	/**
	 * Which FEditorModeInfo data should be returned when queried, filtered by the mode's ID.
	 * Note that this does not disable or unregister disallowed modes, it simply removes them from the query results.
	 */
	FNamePermissionList AllowedEditorModes;
};
