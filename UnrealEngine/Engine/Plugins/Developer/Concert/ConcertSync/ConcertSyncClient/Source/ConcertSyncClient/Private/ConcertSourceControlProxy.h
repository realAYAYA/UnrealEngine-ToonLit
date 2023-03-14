// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlState.h"
#include "ISourceControlProvider.h"

class FConcertClientWorkspace;

/**
 * Concert Source Control State Proxy
 * Manages underlying source control state while a Concert session is active.
 */
class FConcertSourceControlStateProxy : public ISourceControlState
{
public:
	explicit FConcertSourceControlStateProxy(FSourceControlStateRef InActualState);

	explicit FConcertSourceControlStateProxy(FString InFilename);

	//~ ISourceControlState implementation
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem(int32 HistoryIndex) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(int32 RevisionNumber) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision(const FString& InRevision) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetBaseRevForMerge() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = NULL) const override;
	virtual bool IsCheckedOutInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual bool IsModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual bool IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch = FString()) const override;
	virtual TArray<FString> GetCheckedOutBranches() const override;
	virtual FString GetOtherUserBranchCheckedOuts() const override;
	virtual bool GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const override;
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsLocal() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool CanDelete() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool IsConflicted() const override;
	virtual bool CanRevert() const override;

private:
	/** The underlying state we proxy through. */
	FSourceControlStatePtr ActualState;
	
	/** The name of the file we represent (only when ActualState is null) */
	FString CachedFilename;

	/** The timestamp of the file we represent (only when ActualState is null) */
	FDateTime CachedTimestamp;
};

class FConcertSourceControlChangelistStateProxy : public ISourceControlChangelistState
{
public:
	FConcertSourceControlChangelistStateProxy() = default;
	explicit FConcertSourceControlChangelistStateProxy(FSourceControlChangelistStateRef InActualState);

	virtual FName GetIconName() const override;
	virtual FName GetSmallIconName() const override;
	virtual FText GetDisplayText() const override;
	virtual FText GetDescriptionText() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual const TArray<FSourceControlStateRef>& GetFilesStates() const override;
	virtual const TArray<FSourceControlStateRef>& GetShelvedFilesStates() const override;
	virtual FSourceControlChangelistRef GetChangelist() const override;

private:
	/** The underlying state we proxy through. */
	FSourceControlChangelistStatePtr ActualState;
};

/**
 * Concert Source Control Provider Proxy
 * Manages underlying source control provider while a Concert session is active.
 */
class FConcertSourceControlProxy : public ISourceControlProvider
{
public:
	FConcertSourceControlProxy();

	/** Set the concert session workspace for the proxy. */
	void SetWorkspace(const TSharedPtr<FConcertClientWorkspace>& InWorkspace);
	
	//~ ISourceControlProvider implementation
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual bool QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest) override;
	virtual void RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRootIn) override;
	virtual int32 GetStateBranchIndex(const FString& BranchName) const override;
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray<FSourceControlStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual ECommandResult::Type GetState(const TArray<FSourceControlChangelistRef>& InFiles, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual void UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle ) override;
	virtual ECommandResult::Type Execute( const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
	virtual bool CanCancelOperation( const FSourceControlOperationRef& InOperation ) const override;
	virtual void CancelOperation( const FSourceControlOperationRef& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual bool UsesCheckout() const override;
	virtual bool UsesFileRevisions() const override;
	virtual TOptional<bool> IsAtLatestRevision() const override;
	virtual TOptional<int> GetNumLocalChanges() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
	virtual TArray<FSourceControlChangelistRef> GetChangelists(EStateCacheUsage::Type InStateCacheUsage) override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

private:

	/** Install the proxy as the currently provider. */
	void InstallProvider();

	/** Uninstall the proxy and restore the previously used provider. */
	void UninstallProvider();

	/** Delegate to handle provider change and change our underlying provider. */
	void HandleProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);

	/** Name used to register this provider to the source control module. */
	static const FName ConcertProviderName;

	/** Reentry guard when handling provider changes. */
	bool bHandlingProviderChanges;

	/** Delegate handle for provider changes. */
	FDelegateHandle ProviderChangedHandle;

	/** Active Workspace of the session we are representing the state of. */
	TSharedPtr<FConcertClientWorkspace> Workspace;

	/** The underlying Source Control provider we are going to submit the workspace through. */
	ISourceControlProvider* ActualProvider;
};
