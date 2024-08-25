// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollectionManagerTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "FrontendFilterBase.h"
#include "HAL/Platform.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "Internationalization/Text.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "Misc/TextFilterUtils.h"
#include "SourceControlOperations.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"

class UPackage;
struct FAssetRenameData;
struct FCollectionNameType;
struct FContentBrowserDataFilter;

#define LOCTEXT_NAMESPACE "ContentBrowser"

class FMenuBuilder;
struct FAssetCompileData;

/** A filter for text search */
class CONTENTBROWSER_API FFrontendFilter_Text : public FFrontendFilter
{
public:
	FFrontendFilter_Text();
	~FFrontendFilter_Text();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("TextFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Text", "Text"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_TextTooltip", "Show only assets that match the input text"); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

public:
	/** Returns the unsanitized and unsplit filter terms */
	FText GetRawFilterText() const;

	/** Set the Text to be used as the Filter's restrictions */
	void SetRawFilterText(const FText& InFilterText);

	/** Get the last error returned from lexing or compiling the current filter text */
	FText GetFilterErrorText() const;

	/** If bIncludeClassName is true, the text filter will include an asset's class name in the search */
	void SetIncludeClassName(const bool InIncludeClassName);

	/** If bIncludeAssetPath is true, the text filter will match against full Asset path */
	void SetIncludeAssetPath(const bool InIncludeAssetPath);

	bool GetIncludeAssetPath() const;

	/** If bIncludeCollectionNames is true, the text filter will match against collection names as well */
	void SetIncludeCollectionNames(const bool InIncludeCollectionNames);

	bool GetIncludeCollectionNames() const;
private:
	/** Handles an on collection created event */
	void HandleCollectionCreated(const FCollectionNameType& Collection);

	/** Handles an on collection destroyed event */
	void HandleCollectionDestroyed(const FCollectionNameType& Collection);

	/** Handles an on collection renamed event */
	void HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection);

	/** Handles an on collection updated event */
	void HandleCollectionUpdated(const FCollectionNameType& Collection);

	/** Rebuild the array of dynamic collections that are being referenced by the current query */
	void RebuildReferencedDynamicCollections();

	/** An array of dynamic collections that are being referenced by the current query. These should be tested against each asset when it's looking for collections that contain it */
	TArray<FCollectionNameType> ReferencedDynamicCollections;

	/** Transient context data, used when calling PassesFilter. Kept around to minimize re-allocations between multiple calls to PassesFilter */
	TSharedRef<class FFrontendFilter_TextFilterExpressionContext> TextFilterExpressionContext;

	/** Expression evaluator that can be used to perform complex text filter queries */
	FTextFilterExpressionEvaluator TextFilterExpressionEvaluator;

	/** Delegate handles */
	FDelegateHandle OnCollectionCreatedHandle;
	FDelegateHandle OnCollectionDestroyedHandle;
	FDelegateHandle OnCollectionRenamedHandle;
	FDelegateHandle OnCollectionUpdatedHandle;
};

/** A filter that displays only checked out assets */
class CONTENTBROWSER_API FFrontendFilter_CheckedOut : public FFrontendFilter, public TSharedFromThis<FFrontendFilter_CheckedOut>
{
public:
	FFrontendFilter_CheckedOut(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("CheckedOut"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_CheckedOut", "Checked Out"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_CheckedOutTooltip", "Show only assets that you have checked out or pending for add."); }
	virtual void ActiveStateChanged(bool bActive) override;
	virtual void SetCurrentFilter(TArrayView<const FName> InSourcePaths, const FContentBrowserDataFilter& InBaseFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	
	/** Request the source control status for this filter */
	void RequestStatus();

	/** Callback when source control operation has completed */
	void SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	bool bSourceControlEnabled;
};

/** A filter that displays assets not tracked by source control */
class CONTENTBROWSER_API FFrontendFilter_NotSourceControlled : public FFrontendFilter, public TSharedFromThis<FFrontendFilter_NotSourceControlled>
{
public:
	FFrontendFilter_NotSourceControlled(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("NotSourceControlled"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_NotSourceControlled", "Not Revision Controlled"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_NotSourceControlledTooltip", "Show only assets that are not tracked by revision control."); }
	virtual void ActiveStateChanged(bool bActive) override;
	virtual void SetCurrentFilter(TArrayView<const FName> InSourcePaths, const FContentBrowserDataFilter& InBaseFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:

	/** Request the source control status for this filter */
	void RequestStatus();

	/** Callback when source control operation has completed */
	void SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	bool bSourceControlEnabled;
	bool bIsRequestStatusRunning;
	bool bInitialRequestCompleted;
};

/** A filter that displays only modified assets */
class CONTENTBROWSER_API FFrontendFilter_Modified : public FFrontendFilter
{
public:
	FFrontendFilter_Modified(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_Modified();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("Modified"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Modified", "Modified"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ModifiedTooltip", "Show only assets that have been modified and not yet saved."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:

	/** Handler for when a package's dirty state has changed */
	void OnPackageDirtyStateUpdated(UPackage* Package);

	bool bIsCurrentlyActive;
};

/** A filter that displays blueprints that have replicated properties */
class CONTENTBROWSER_API FFrontendFilter_ReplicatedBlueprint : public FFrontendFilter
{
public:
	FFrontendFilter_ReplicatedBlueprint(TSharedPtr<FFrontendFilterCategory> InCategory) : FFrontendFilter(InCategory) {}

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ReplicatedBlueprint"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_ReplicatedBlueprint", "Replicated Blueprints"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_ReplicatedBlueprintToolTip", "Show only blueprints with replicated properties."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
};

/** A filter that compares the value of an asset registry tag to a target value */
class CONTENTBROWSER_API FFrontendFilter_ArbitraryComparisonOperation : public FFrontendFilter
{
public:
	FFrontendFilter_ArbitraryComparisonOperation(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

protected:
	static FString ConvertOperationToString(ETextFilterComparisonOperation Op);
	
	void SetComparisonOperation(ETextFilterComparisonOperation NewOp);
	bool IsComparisonOperationEqualTo(ETextFilterComparisonOperation TestOp) const;

	FText GetKeyValueAsText() const;
	FText GetTargetValueAsText() const;
	void OnKeyValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType);
	void OnTargetValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

public:
	FName TagName;
	FString TargetTagValue;
	ETextFilterComparisonOperation ComparisonOp;
};

/** An inverse filter that allows display of content in developer folders that are not the current user's */
class CONTENTBROWSER_API FFrontendFilter_ShowOtherDevelopers : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowOtherDevelopers(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ShowOtherDevelopers"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_ShowOtherDevelopers", "Other Developers"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ShowOtherDevelopersTooltip", "Allow display of assets in developer folders that aren't yours."); }
	virtual bool IsInverseFilter() const override { return true; }
	virtual void SetCurrentFilter(TArrayView<const FName> InSourcePaths, const FContentBrowserDataFilter& InBaseFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

public:
	/** Sets if we should filter out assets from other developers */
	void SetShowOtherDeveloperAssets(bool bValue);

	/** Gets if we should filter out assets from other developers */
	bool GetShowOtherDeveloperAssets() const;

private:
	FString BaseDeveloperPath;
	TArray<ANSICHAR> BaseDeveloperPathAnsi;
	FString UserDeveloperPath;
	bool bIsOnlyOneDeveloperPathSelected;
	bool bShowOtherDeveloperAssets;
};

/** An inverse filter that allows display of object redirectors */
class CONTENTBROWSER_API FFrontendFilter_ShowRedirectors : public FFrontendFilter
{
public:
	/** Constructor */
	FFrontendFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("ShowRedirectors"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_ShowRedirectors", "Show Redirectors"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_ShowRedirectorsToolTip", "Allow display of Redirectors."); }
	virtual bool IsInverseFilter() const override { return true; }
	virtual void SetCurrentFilter(TArrayView<const FName> InSourcePaths, const FContentBrowserDataFilter& InBaseFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	bool bAreRedirectorsInBaseFilter;
	FString RedirectorClassName;
};

/** A filter that only displays assets used by loaded levels */
class CONTENTBROWSER_API FFrontendFilter_InUseByLoadedLevels : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_InUseByLoadedLevels(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_InUseByLoadedLevels() override;

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("InUseByLoadedLevels"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_InUseByLoadedLevels", "In Use By Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_InUseByLoadedLevelsToolTip", "Show only assets that are currently in use by any loaded level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

	/** Handler for when maps change in the editor */
	void OnEditorMapChange( uint32 MapChangeFlags );

	/** Handler for when an asset is renamed */
	void OnAssetPostRename(const TArray<FAssetRenameData>& AssetsAndNames);

	/** Handler for when assets are finished compiling */
	void OnAssetPostCompile(const TArray<FAssetCompileData>& CompiledAssets);

private:
	void Refresh();
	void RegisterDelayedRefresh(float DelayInSeconds);
	void UnregisterDelayedRefresh();
	FTSTicker::FDelegateHandle DelayedRefreshHandle;
	bool bIsDirty = false;
	bool bIsCurrentlyActive = false;
};

/** A filter that only displays assets used by any level */
class CONTENTBROWSER_API FFrontendFilter_UsedInAnyLevel: public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_UsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("UsedInAnyLevel"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_UsedInAnyLevel", "Used In Any Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_UsedInAnyLevelTooltip", "Show only assets that are used in any level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	class IAssetRegistry* AssetRegistry;
	TSet<FName> LevelsDependencies;
};

/** A filter that only displays assets not used by any level */
class CONTENTBROWSER_API FFrontendFilter_NotUsedInAnyLevel : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_NotUsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("NotUsedInAnyLevel"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyLevel", "Not Used In Any Level"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyLevelTooltip", "Show only assets that are not used in any level."); }
	virtual void ActiveStateChanged(bool bActive) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	class IAssetRegistry* AssetRegistry;
	TSet<FName> LevelsDependencies;
};

/** A filter that only displays assets not used in another asset (Note It does not update itself automatically) */
class CONTENTBROWSER_API FFrontendFilter_NotUsedInAnyAsset : public FFrontendFilter
{
public:
	/** Constructor/Destructor */
	FFrontendFilter_NotUsedInAnyAsset(TSharedPtr<FFrontendFilterCategory> InCategory);

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("NotUsedInAnyAsset"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyAsset", "Not Used In Any Asset"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FFrontendFilter_NotUsedInAnyAssetTooltip", "Show only the assets that aren't used by another asset."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
	class IAssetRegistry* AssetRegistry = nullptr;
};

/** A filter that displays recently opened assets */
class CONTENTBROWSER_API FFrontendFilter_Recent : public FFrontendFilter
{
public:
	FFrontendFilter_Recent(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_Recent();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("RecentlyOpened"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Recent", "Recently Opened"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_RecentTooltip", "Show only recently opened assets."); }
	virtual void ActiveStateChanged(bool bActive) override;
	virtual void SetCurrentFilter(TArrayView<const FName> InSourcePaths, const FContentBrowserDataFilter& InBaseFilter) override;

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

	void ResetFilter(FName InName);

private:
	void RefreshRecentPackagePaths();

	TSet<FName> RecentPackagePaths;
	bool bIsCurrentlyActive;
};

/** A filter that displays only assets that are not read only */
class CONTENTBROWSER_API FFrontendFilter_Writable : public FFrontendFilter
{
public:
	FFrontendFilter_Writable(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_Writable();

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("Writable"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Writable", "Writable"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_WritableTooltip", "Show only assets that are not read only."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;

private:
};

/** A filter that displays only packages that contain virtualized data  */
class CONTENTBROWSER_API FFrontendFilter_VirtualizedData : public FFrontendFilter
{
public:
	FFrontendFilter_VirtualizedData(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_VirtualizedData() = default;

private:
	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("VirtualizedData"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_VirtualizedData", "Virtualized Data"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_VirtualizedDataTooltip", "Show only package that contain virtualized data."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
};

/** A filter that displays only assets the are mark as unsupported */
class CONTENTBROWSER_API FFrontendFilter_Unsupported : public FFrontendFilter
{
public:
	FFrontendFilter_Unsupported(TSharedPtr<FFrontendFilterCategory> InCategory);
	~FFrontendFilter_Unsupported() = default;

	// FFrontendFilter implementation
	virtual FString GetName() const override { return TEXT("Unsupported"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("FrontendFilter_Unsupported", "Unsupported"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("FrontendFilter_UnsupportedTooltip", "Show only assets that are not supported by this project."); }

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
};

#undef LOCTEXT_NAMESPACE
