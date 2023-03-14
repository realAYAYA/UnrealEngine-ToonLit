// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSourceControlProxy.h"
#include "ISourceControlModule.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"

#if SOURCE_CONTROL_WITH_SLATE
	#include "Widgets/SNullWidget.h"
#endif

#include "ConcertClientWorkspace.h"

const FName FConcertSourceControlProxy::ConcertProviderName("Multi-User");

#define LOCTEXT_NAMESPACE "ConcertSourceControl"

FConcertSourceControlStateProxy::FConcertSourceControlStateProxy(FSourceControlStateRef InActualState)
	: ActualState(InActualState)
	, CachedFilename()
	, CachedTimestamp(0)
{
}

FConcertSourceControlStateProxy::FConcertSourceControlStateProxy(FString InFilename)
	: ActualState(nullptr)
	, CachedFilename(MoveTemp(InFilename))
	, CachedTimestamp(IFileManager::Get().GetTimeStamp(*CachedFilename))
{
}

int32 FConcertSourceControlStateProxy::GetHistorySize() const
{
	return ActualState.IsValid() ? ActualState->GetHistorySize() : 0;
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FConcertSourceControlStateProxy::GetHistoryItem(int32 HistoryIndex) const
{
	return ActualState.IsValid() ? ActualState->GetHistoryItem(HistoryIndex) : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FConcertSourceControlStateProxy::FindHistoryRevision(int32 RevisionNumber) const
{
	return ActualState.IsValid() ? ActualState->FindHistoryRevision(RevisionNumber) : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FConcertSourceControlStateProxy::FindHistoryRevision(const FString& InRevision) const
{
	return ActualState.IsValid() ? ActualState->FindHistoryRevision(InRevision) : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FConcertSourceControlStateProxy::GetBaseRevForMerge() const
{
	return ActualState.IsValid() ? ActualState->GetBaseRevForMerge() : TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

FSlateIcon FConcertSourceControlStateProxy::GetIcon() const
{
	return ActualState.IsValid() ? ActualState->GetIcon() : FSlateIcon();
}

FText FConcertSourceControlStateProxy::GetDisplayName() const
{
	return ActualState.IsValid() ? ActualState->GetDisplayName() : FText::GetEmpty();
}

FText FConcertSourceControlStateProxy::GetDisplayTooltip() const
{
	return ActualState.IsValid() ? ActualState->GetDisplayTooltip() : FText::GetEmpty();
}

const FString& FConcertSourceControlStateProxy::GetFilename() const
{
	return ActualState.IsValid() ? ActualState->GetFilename() : CachedFilename;
}

const FDateTime& FConcertSourceControlStateProxy::GetTimeStamp() const
{
	return ActualState.IsValid() ? ActualState->GetTimeStamp() : CachedTimestamp;
}

bool FConcertSourceControlStateProxy::CanCheckIn() const
{
	return ActualState.IsValid() && ActualState->CanCheckIn();
}

bool FConcertSourceControlStateProxy::CanCheckout() const
{
	return ActualState.IsValid() && ActualState->CanCheckout();
}

bool FConcertSourceControlStateProxy::IsCheckedOut() const
{
	return ActualState.IsValid() && ActualState->IsCheckedOut();
}

bool FConcertSourceControlStateProxy::IsCheckedOutOther(FString* Who) const
{
	return ActualState.IsValid() && ActualState->IsCheckedOutOther(Who);
}

bool FConcertSourceControlStateProxy::IsCheckedOutInOtherBranch(const FString& CurrentBranch) const
{
	return ActualState.IsValid() && ActualState->IsCheckedOutInOtherBranch(CurrentBranch);
}

bool FConcertSourceControlStateProxy::IsModifiedInOtherBranch(const FString& CurrentBranch) const
{
	return ActualState.IsValid() && ActualState->IsModifiedInOtherBranch(CurrentBranch);
}

bool FConcertSourceControlStateProxy::IsCheckedOutOrModifiedInOtherBranch(const FString& CurrentBranch) const
{
	return ActualState.IsValid() && ActualState->IsCheckedOutOrModifiedInOtherBranch(CurrentBranch);
}

TArray<FString> FConcertSourceControlStateProxy::GetCheckedOutBranches() const
{
	return ActualState.IsValid() ? ActualState->GetCheckedOutBranches() : TArray<FString>();
}

FString FConcertSourceControlStateProxy::GetOtherUserBranchCheckedOuts() const
{
	return ActualState.IsValid() ? ActualState->GetOtherUserBranchCheckedOuts() : FString();
}

bool FConcertSourceControlStateProxy::GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const
{
	return ActualState.IsValid() && ActualState->GetOtherBranchHeadModification(HeadBranchOut, ActionOut, HeadChangeListOut);
}

bool FConcertSourceControlStateProxy::IsCurrent() const
{
	return ActualState.IsValid() && ActualState->IsCurrent();
}

bool FConcertSourceControlStateProxy::IsSourceControlled() const
{
	return ActualState.IsValid() && ActualState->IsSourceControlled();
}

bool FConcertSourceControlStateProxy::IsLocal() const
{
	// Concert live propagates assets to other clients, so no assets can be local
	// This function is used to determine whether redirector assets are left when things are renamed
	return false;
}

bool FConcertSourceControlStateProxy::IsAdded() const
{
	return ActualState.IsValid() && ActualState->IsAdded();
}

bool FConcertSourceControlStateProxy::IsDeleted() const
{
	return ActualState.IsValid() && ActualState->IsDeleted();
}

bool FConcertSourceControlStateProxy::IsIgnored() const
{
	return ActualState.IsValid() && ActualState->IsIgnored();
}

bool FConcertSourceControlStateProxy::CanEdit() const
{
	return ActualState.IsValid() && ActualState->CanEdit();
}

bool FConcertSourceControlStateProxy::CanDelete() const
{
	return ActualState.IsValid() && ActualState->CanDelete();
}

bool FConcertSourceControlStateProxy::IsUnknown() const
{
	return ActualState.IsValid() && ActualState->IsUnknown();
}

bool FConcertSourceControlStateProxy::IsModified() const
{
	return ActualState.IsValid() && ActualState->IsModified();
}

bool FConcertSourceControlStateProxy::CanAdd() const
{
	return ActualState.IsValid() && ActualState->CanAdd();
}

bool FConcertSourceControlStateProxy::IsConflicted() const
{
	return ActualState.IsValid() && ActualState->IsConflicted();
}

bool FConcertSourceControlStateProxy::CanRevert() const
{
	return ActualState.IsValid() && ActualState->CanRevert();
}


FConcertSourceControlChangelistStateProxy::FConcertSourceControlChangelistStateProxy(FSourceControlChangelistStateRef InActualState)
	: ActualState(InActualState)
{
}

FName FConcertSourceControlChangelistStateProxy::GetIconName() const
{
	return ActualState.IsValid() ? ActualState->GetIconName() : NAME_None;
}

FName FConcertSourceControlChangelistStateProxy::GetSmallIconName() const
{
	return ActualState.IsValid() ? ActualState->GetSmallIconName() : NAME_None;
}

FText FConcertSourceControlChangelistStateProxy::GetDisplayText() const
{
	return ActualState.IsValid() ? ActualState->GetDisplayText() : FText::GetEmpty();
}

FText FConcertSourceControlChangelistStateProxy::GetDescriptionText() const
{
	return ActualState.IsValid() ? ActualState->GetDescriptionText() : FText::GetEmpty();
}

FText FConcertSourceControlChangelistStateProxy::GetDisplayTooltip() const
{
	return ActualState.IsValid() ? ActualState->GetDisplayTooltip() : FText::GetEmpty();
}

const FDateTime& FConcertSourceControlChangelistStateProxy::GetTimeStamp() const
{
	static const FDateTime DummyTime;
	return ActualState.IsValid() ? ActualState->GetTimeStamp() : DummyTime;
}

const TArray<FSourceControlStateRef>& FConcertSourceControlChangelistStateProxy::GetFilesStates() const
{
	static const TArray<FSourceControlStateRef> DummyResult;
	return ActualState.IsValid() ? ActualState->GetFilesStates() : DummyResult;
}

const TArray<FSourceControlStateRef>& FConcertSourceControlChangelistStateProxy::GetShelvedFilesStates() const
{
	static const TArray<FSourceControlStateRef> DummyResult;
	return ActualState.IsValid() ? ActualState->GetShelvedFilesStates() : DummyResult;
}

FSourceControlChangelistRef FConcertSourceControlChangelistStateProxy::GetChangelist() const
{
	static ISourceControlChangelist DummyCL;
	return ActualState.IsValid() ? ActualState->GetChangelist() : DummyCL.AsShared();
}

FConcertSourceControlProxy::FConcertSourceControlProxy()
	: bHandlingProviderChanges(false)
	, ActualProvider(nullptr)
{
}

void FConcertSourceControlProxy::SetWorkspace(const TSharedPtr<FConcertClientWorkspace>& InWorkspace)
{
	Workspace = InWorkspace;

	if (Workspace.IsValid())
	{
		InstallProvider();
	}
	else
	{
		UninstallProvider();
	}
}

void FConcertSourceControlProxy::Init(bool bForceConnection)
{
	if (ActualProvider)
	{
		ActualProvider->Init(bForceConnection);
	}
}

void FConcertSourceControlProxy::Close()
{
	if (ActualProvider)
	{
		ActualProvider->Close();
	}
}

FText FConcertSourceControlProxy::GetStatusText() const
{
	if (ActualProvider)
	{
		ActualProvider->GetStatusText();
	}
	return FText();
}

bool FConcertSourceControlProxy::IsEnabled() const
{
	if (ActualProvider)
	{
		return ActualProvider->IsEnabled();
	}
	return true;
}

bool FConcertSourceControlProxy::IsAvailable() const
{
	return true;
}

const FName& FConcertSourceControlProxy::GetName() const
{
	return ConcertProviderName;
}

bool FConcertSourceControlProxy::QueryStateBranchConfig(const FString& ConfigSrc, const FString& ConfigDest)
{
	if (ActualProvider)
	{
		ActualProvider->QueryStateBranchConfig(ConfigSrc, ConfigDest);
	}
	return false;
}

void FConcertSourceControlProxy::RegisterStateBranches(const TArray<FString>& BranchNames, const FString& ContentRootIn)
{
	if (ActualProvider)
	{
		ActualProvider->RegisterStateBranches(BranchNames, ContentRootIn);
	}
}

int32 FConcertSourceControlProxy::GetStateBranchIndex(const FString& BranchName) const
{
	if (ActualProvider)
	{
		ActualProvider->GetStateBranchIndex(BranchName);
	}
	return -1;
}

ECommandResult::Type FConcertSourceControlProxy::GetState(const TArray<FString>& InFiles, TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	if (ActualProvider)
	{
		Result = ActualProvider->GetState(InFiles, OutState, InStateCacheUsage);
	}

	if (Result == ECommandResult::Failed)
	{
		// We always return dummy state entries if the underlying provider fails
		Result = ECommandResult::Succeeded;

		OutState.Reset();
		for (const FString& File : InFiles)
		{
			OutState.Add(MakeShared<FConcertSourceControlStateProxy, ESPMode::ThreadSafe>(File));
		}
	}
	else
	{
		for (auto& State : OutState)
		{
			State = MakeShared<FConcertSourceControlStateProxy, ESPMode::ThreadSafe>(State);
		}
	}

	return Result;
}


ECommandResult::Type FConcertSourceControlProxy::GetState(const TArray<FSourceControlChangelistRef>& InChangelists, TArray<FSourceControlChangelistStateRef>& OutState, EStateCacheUsage::Type InStateCacheUsage)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	if (ActualProvider)
	{
		Result = ActualProvider->GetState(InChangelists, OutState, InStateCacheUsage);
	}

	if (Result == ECommandResult::Failed)
	{
		// We always return dummy state entries if the underlying provider fails
		Result = ECommandResult::Succeeded;

		OutState.Reset();
		for (const FSourceControlChangelistRef& Changelist : InChangelists)
		{
			OutState.Add(MakeShared<FConcertSourceControlChangelistStateProxy, ESPMode::ThreadSafe>());
		}
 	}
	else
	{
		for (auto& State : OutState)
		{
			State = MakeShared<FConcertSourceControlChangelistStateProxy, ESPMode::ThreadSafe>(State);
		}
	}

	return Result;
}

TArray<FSourceControlStateRef> FConcertSourceControlProxy::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	if (ActualProvider)
	{
		return ActualProvider->GetCachedStateByPredicate(Predicate);
	}

	return TArray<FSourceControlStateRef>();
}

FDelegateHandle FConcertSourceControlProxy::RegisterSourceControlStateChanged_Handle(const FSourceControlStateChanged::FDelegate& SourceControlStateChanged)
{
	if (ActualProvider)
	{
		return ActualProvider->RegisterSourceControlStateChanged_Handle(SourceControlStateChanged);
	}

	return FDelegateHandle();
}

void FConcertSourceControlProxy::UnregisterSourceControlStateChanged_Handle(FDelegateHandle Handle)
{
	if (ActualProvider)
	{
		return ActualProvider->UnregisterSourceControlStateChanged_Handle(Handle);
	}
}

ECommandResult::Type FConcertSourceControlProxy::Execute(const FSourceControlOperationRef& InOperation, FSourceControlChangelistPtr InChangelist, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate)
{
	if (ActualProvider)
	{
		// We do not want to allow a P4 add when we operating within a sandbox. Perforce is the only SCS that will allow you to add a file that does
		// not yet exist.
		//
		if (!IFileManager::Get().IsSandboxEnabled() || InOperation->GetName() != FName(TEXT("MarkForAdd")))
		{
			return ActualProvider->Execute(InOperation, InChangelist, InFiles, InConcurrency, InOperationCompleteDelegate);
		}
	}

	return ECommandResult::Failed;
}

bool FConcertSourceControlProxy::CanCancelOperation(const FSourceControlOperationRef& InOperation) const
{
	if (ActualProvider)
	{
		return ActualProvider->CanCancelOperation(InOperation);
	}

	return false;
}

void FConcertSourceControlProxy::CancelOperation(const FSourceControlOperationRef& InOperation)
{
	if (ActualProvider)
	{
		return ActualProvider->CancelOperation(InOperation);
	}
}

bool FConcertSourceControlProxy::UsesLocalReadOnlyState() const
{
	if (ActualProvider)
	{
		return ActualProvider->UsesLocalReadOnlyState();
	}
	return false;
}

bool FConcertSourceControlProxy::UsesChangelists() const
{
	if (ActualProvider)
	{
		return ActualProvider->UsesChangelists();
	}
	return false;
}

bool FConcertSourceControlProxy::UsesCheckout() const
{
	if (ActualProvider)
	{
		return ActualProvider->UsesCheckout();
	}
	return false;
}

bool FConcertSourceControlProxy::UsesFileRevisions() const
{
	if (ActualProvider)
	{
		return ActualProvider->UsesFileRevisions();
	}
	return false;
}

TOptional<bool> FConcertSourceControlProxy::IsAtLatestRevision() const
{
	return TOptional<bool>();
}

TOptional<int> FConcertSourceControlProxy::GetNumLocalChanges() const
{
	return TOptional<int>();
}

void FConcertSourceControlProxy::Tick()
{
	if (ActualProvider)
	{
		return ActualProvider->Tick();
	}
}

TArray<TSharedRef<class ISourceControlLabel>> FConcertSourceControlProxy::GetLabels(const FString& InMatchingSpec) const
{
	if (ActualProvider)
	{
		return ActualProvider->GetLabels(InMatchingSpec);
	}
	return TArray<TSharedRef<class ISourceControlLabel>>();
}

TArray<FSourceControlChangelistRef> FConcertSourceControlProxy::GetChangelists(EStateCacheUsage::Type InStateCacheUsage)
{
	if (ActualProvider)
	{
		return ActualProvider->GetChangelists(InStateCacheUsage);
	}
	return TArray<FSourceControlChangelistRef>();
}

TSharedRef<class SWidget> FConcertSourceControlProxy::MakeSettingsWidget() const
{
	if (ActualProvider)
	{
		return ActualProvider->MakeSettingsWidget();
	}

	return SNullWidget::NullWidget;
}

void FConcertSourceControlProxy::InstallProvider()
{
	// if we have a valid handle, we are already installed
	if (ProviderChangedHandle.IsValid())
	{
		return;
	}

	ISourceControlModule& SourceControl = ISourceControlModule::Get();

	// Get the Actual Source Control Provider
	ActualProvider = &SourceControl.GetProvider();

	// Register Concert Proxy modular feature
	IModularFeatures::Get().RegisterModularFeature("SourceControl", this);

	// Set the proxy as the current provider
	SourceControl.SetProvider(ConcertProviderName);

	// Register Provider changes so we can override them
	ProviderChangedHandle = SourceControl.RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateRaw(this, &FConcertSourceControlProxy::HandleProviderChanged));
}

void FConcertSourceControlProxy::UninstallProvider()
{
	if (!ProviderChangedHandle.IsValid())
	{
		return;
	}

	ISourceControlModule& SourceControl = ISourceControlModule::Get();

	// Unregister provider changes
	SourceControl.UnregisterProviderChanged(ProviderChangedHandle);
	ProviderChangedHandle.Reset();

	// Set back the old provider
	SourceControl.SetProvider(ActualProvider->GetName());

	// Unregister the modular feature
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", this);
}

void FConcertSourceControlProxy::HandleProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	// if we aren't currently setting ourselves as the provider
	if (bHandlingProviderChanges)
	{
		return;
	}
	TGuardValue<bool> ReentrancyGuard(bHandlingProviderChanges, true);

	{
		// if we receive this event we should be installed as the current provider, if we aren't resetting ourselves
		check(&OldProvider == this);
		ActualProvider = &NewProvider;
		ISourceControlModule::Get().SetProvider(ConcertProviderName);
	}
}

#undef LOCTEXT_NAMESPACE

