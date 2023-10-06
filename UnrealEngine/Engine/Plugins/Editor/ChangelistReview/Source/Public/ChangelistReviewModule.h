// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "SSourceControlReview.h"
#include "Modules/ModuleManager.h"

class SSourceControlReview;
class FSpawnTabArgs;
class SDockTab;
class SWidget;

class UContentBrowserAliasDataSource;

class CHANGELISTREVIEW_API FChangelistReviewModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void ShowReviewTab();
	bool CanShowReviewTab() const;
	TWeakPtr<SSourceControlReview> GetActiveReview();
	
	/**
	 * Opens review tool and loads the provided change list.
	 *
	 * @param[in] Changelist The change list to load.
	 * @return @c true if changelist review tool was opened and change list is put to be loaded, @c false otherwise.
	 */
	bool OpenChangelistReview(const FString& Changelist);

	static FChangelistReviewModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FChangelistReviewModule>("ChangelistReview");
	}
private:
	TSharedRef<SDockTab> CreateReviewTab(const FSpawnTabArgs& Args);
	TSharedPtr<SWidget> CreateReviewUI();
	
	TWeakPtr<SDockTab> ReviewTab;
	TWeakPtr<SSourceControlReview> ReviewWidget;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#endif
