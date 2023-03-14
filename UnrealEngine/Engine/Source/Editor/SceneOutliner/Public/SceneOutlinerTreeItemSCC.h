// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneOutlinerPublicTypes.h"
#include "SourceControlHelpers.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerTreeItemSCC"

DECLARE_DELEGATE_OneParam(FSourceControlStateChangedDelegate, FSourceControlStatePtr);

class FSceneOutlinerTreeItemSCC : public TSharedFromThis<FSceneOutlinerTreeItemSCC>
{
public:
	FSceneOutlinerTreeItemSCC(FSceneOutlinerTreeItemPtr InTreeItemPtr);

	~FSceneOutlinerTreeItemSCC();

	FSourceControlStatePtr GetSourceControlState();

	FSourceControlStatePtr RefreshSourceControlState();

	bool IsExternalPackage() { return !ExternalPackageName.IsEmpty(); }

	FString GetPackageName() { return ExternalPackageName; }

	UPackage* GetPackage() { return ExternalPackage; }

	FSourceControlStateChangedDelegate OnSourceControlStateChanged;

private:

	void ConnectSourceControl();

	void DisconnectSourceControl();

	void HandleSourceControlStateChanged(EStateCacheUsage::Type CacheUsage);

	void HandleSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);

	void BroadcastNewState(FSourceControlStatePtr SourceControlState);

	/** The tree item we relate to */
	FSceneOutlinerTreeItemPtr TreeItemPtr;

	/** Cache the items external package name */
	FString ExternalPackageName;
	UPackage* ExternalPackage = nullptr;

	/** Source control state changed delegate handle */
	FDelegateHandle SourceControlStateChangedDelegateHandle;

	/** Source control provider changed delegate handle */
	FDelegateHandle SourceControlProviderChangedDelegateHandle;

	/** Actor packaging mode changed delegate handle */
	FDelegateHandle ActorPackingModeChangedDelegateHandle;
};

#undef LOCTEXT_NAMESPACE
