// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "SSourceControlReview.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;
class SWidget;

class UContentBrowserAliasDataSource;

class FChangelistReviewModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void ShowReviewTab();
	bool CanShowReviewTab() const;
private:
	TSharedRef<SDockTab> CreateReviewTab(const FSpawnTabArgs& Args);
	TSharedPtr<SWidget> CreateReviewUI();
	
	TWeakPtr<SDockTab> ReviewTab;
	//TWeakPtr<SSourceControlReview> ReviewWidget;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#endif
