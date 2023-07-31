// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerPublicTypes.h"
#include "ToolMenu.h"
#include "SceneOutlinerTreeItemSCC.h"

class FSceneOutlinerSCCHandler : public TSharedFromThis<class FSceneOutlinerSCCHandler>
{
public:

	bool AddSourceControlMenuOptions(UToolMenu* Menu, TArray<FSceneOutlinerTreeItemPtr> InSelectedItems);

	TSharedPtr<FSceneOutlinerTreeItemSCC> GetItemSourceControl(const FSceneOutlinerTreeItemPtr& InItem) const;

private:

	bool CanExecuteSourceControlActions() const;
	void CacheCanExecuteVars();
	bool CanExecuteSCC() const;
	bool CanExecuteSCCCheckOut() const;
	bool CanExecuteSCCCheckIn() const;
	bool CanExecuteSCCHistory() const;
	bool CanExecuteSCCRefresh() const;
	void FillSourceControlSubMenu(UToolMenu* Menu);
	void GetSelectedPackageNames(TArray<FString>& OutPackageNames) const;
	void GetSelectedPackages(TArray<UPackage*>& OutPackages) const;
	void ExecuteSCCRefresh();
	void ExecuteSCCCheckOut();
	void ExecuteSCCCheckIn();
	void ExecuteSCCHistory();
	void ExecuteSCCShowInChangelist();

	TArray<FSceneOutlinerTreeItemPtr> SelectedItems;

	// todo: Sync and Revert functionality is not enabled currently
	// due to needing to unload/reload the actor and having to deal
	// with any potential dependencies. You can read more about this
	// decision here: https://github.com/EpicGames/UnrealEngine/pull/9310#issuecomment-1171505628
	bool bCanExecuteSCC = false;
	bool bCanExecuteSCCCheckOut = false;
	bool bCanExecuteSCCCheckIn = false;
	bool bCanExecuteSCCHistory = false;

	mutable TMap<FSceneOutlinerTreeItemPtr, TSharedPtr<FSceneOutlinerTreeItemSCC>> ItemSourceControls;
};
