// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"

class SNaniteAudit;
class FNaniteAuditRegistry;
class FTabManager;
class FUICommandList;

struct FNaniteAuditTabs
{
	// Tab identifiers
	static const FName ErrorsViewID;
	static const FName OptimizeViewID;
};

class SNaniteTools : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNaniteTools)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);
	~SNaniteTools();

	TSharedPtr<FTabManager> GetTabManager() const { return TabManager; }

	void Audit();

private:
	FReply OnPerformAudit();

	TSharedRef<SDockTab> SpawnTab_ErrorsView(const FSpawnTabArgs& Args);
	void OnErrorsViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	TSharedRef<SDockTab> SpawnTab_OptimizeView(const FSpawnTabArgs& Args);
	void OnOptimizeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	void HighlightImportantTab();

private:
	TSharedPtr<SNaniteAudit> ErrorsView = nullptr;
	TSharedPtr<SNaniteAudit> OptimizeView = nullptr;
	TSharedPtr<FNaniteAuditRegistry> AuditRegistry = nullptr;

	TSharedPtr<SDockTab> ErrorsTab = nullptr;
	TSharedPtr<SDockTab> OptimizeTab = nullptr;

	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<FTabManager::FLayout> TabLayout;
	TSharedPtr<FUICommandList> CommandList;
};
