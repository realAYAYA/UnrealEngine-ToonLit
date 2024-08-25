// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Framework/Docking/TabManager.h"
#include "Templates/SharedPointer.h"
#include "Toolkits/IToolkitHost.h"
#include "Widgets/SCompoundWidget.h"

#include "SChaosVDMainTab.generated.h"

class FComponentVisualizer;
class FChaosVDEditorModeTools;
class FChaosVDTabSpawnerBase;
class FChaosVDEditorVisualizationSettingsTab;
class FChaosVDSolversTracksTab;
class FChaosVDEngine;
class FChaosVDOutputLogTab;
class FChaosVDPlaybackViewportTab;
class FChaosVDObjectDetailsTab;
class FChaosVDWorldOutlinerTab;
class SButton;
class SDockTab;

UCLASS()
class CHAOSVD_API UChaosVDMainToolbarMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<class SChaosVDMainTab> MainTab;
};

/** The main widget containing the Chaos Visual Debugger interface */
class SChaosVDMainTab : public SCompoundWidget, public IToolkitHost
{
public:
	
	SLATE_BEGIN_ARGS(SChaosVDMainTab) {}
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, OwnerTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FChaosVDEngine> InChaosVDEngine);

	TSharedRef<FChaosVDEngine> GetChaosVDEngineInstance() const { return ChaosVDEngine.ToSharedRef(); }

	template<typename TabType>
	TWeakPtr<TabType> GetTabSpawnerInstance(FName TabID);

	// BEGIN ITOOLKITHOST Interface
	virtual TSharedRef<SWidget> GetParentWidget() override { return AsShared(); }
	virtual void BringToFront() override;
	virtual TSharedPtr<FTabManager> GetTabManager() const override { return TabManager; };
	virtual void OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit) override;
	virtual UWorld* GetWorld() const override;
	virtual FEditorModeTools& GetEditorModeManager() const override;

	virtual UTypedElementCommonActions* GetCommonActions() const override { return nullptr; }
	virtual FName GetStatusBarName() const override { return StatusBarID; };
	virtual FOnActiveViewportChanged& OnActiveViewportChanged() override { return ViewportChangedDelegate;};
	// END ITOOLKITHOST Interface

	TSharedPtr<FComponentVisualizer> FindComponentVisualizer(UClass* ClassPtr);
	TSharedPtr<FComponentVisualizer> FindComponentVisualizer(FName ClassName);

	TConstArrayView<TSharedPtr<FComponentVisualizer>> GetAllComponentVisualizers() { return ComponentVisualizers; }
	
	bool ConnectToLiveSession(int32 SessionID, FString SessionAddress) const;

private:

	void RegisterMainTabMenu();

	template<typename TabType>
	void RegisterTabSpawner(FName TabID);

	void RegisterComponentVisualizer(FName ClassName, const TSharedPtr<FComponentVisualizer>& Visualizer);

	void HandleTabSpawned(TSharedRef<SDockTab> Tab, FName TabID);
	void HandleTabDestroyed(TSharedRef<SDockTab> Tab, FName TabID);

	TSharedRef<FTabManager::FLayout> GenerateMainLayout();

	void GenerateMainWindowMenu();

	FReply BrowseAndOpenChaosVDRecording();

	TSharedRef<SButton> CreateSimpleButton(TFunction<FText()>&& GetTextDelegate, TFunction<FText()>&& ToolTipTextDelegate, const FSlateBrush* ButtonIcon, const UChaosVDMainToolbarMenuContext* MenuContext, const FOnClicked& InButtonClickedCallback);

	TSharedRef<SWidget> GenerateMainToolbarWidget();

	void BrowseChaosVDRecordingFromFolder(FStringView FolderPath = TEXT(""));

	void BrowseLiveSessionsFromTraceStore() const;

	TSharedPtr<FChaosVDEngine> ChaosVDEngine;

	FName StatusBarID;

	TSharedPtr<FTabManager> TabManager;
	TWeakPtr<SDockTab> OwnerTab;
	TSharedPtr<FChaosVDEditorModeTools> EditorModeTools;
	
	TMap<FName, TSharedPtr<FChaosVDTabSpawnerBase>> TabSpawnersByIDMap;

	TMap<FName, TSharedPtr<FComponentVisualizer>> ComponentVisualizersMap;
	TArray<TSharedPtr<FComponentVisualizer>> ComponentVisualizers;

	TMap<FName, TWeakPtr<SDockTab>> ActiveTabsByID;

	FOnActiveViewportChanged ViewportChangedDelegate;

	FReply HandleSessionConnectionClicked();
	FText GetConnectButtonText() const;
	FText GetConnectButtonTooltipText() const;

	static inline const FName MainToolBarName = FName("ChaosVD.MainToolBar");
};


template <typename TabType>
TWeakPtr<TabType> SChaosVDMainTab::GetTabSpawnerInstance(FName TabID)
{
	if (TSharedPtr<FChaosVDTabSpawnerBase>* TabSpawnerPtrPtr = TabSpawnersByIDMap.Find(TabID))
	{
		return StaticCastSharedPtr<TabType>(*TabSpawnerPtrPtr);
	}

	return nullptr;
}

template <typename TabType>
void SChaosVDMainTab::RegisterTabSpawner(FName TabID)
{
	static_assert(std::is_base_of_v<FChaosVDTabSpawnerBase, TabType> , "SChaosVDMainTab::RegisterTabSpawner Only supports FChaosVDTabSpawnerBase based spawners");

	if (!TabSpawnersByIDMap.Contains(TabID))
	{
		TSharedPtr<TabType> TabSpawner = MakeShared<TabType>(TabID, TabManager, StaticCastWeakPtr<SChaosVDMainTab>(AsWeak()));
		TabSpawner->OnTabSpawned().AddRaw(this, &SChaosVDMainTab::HandleTabSpawned, TabID);
		TabSpawner->OnTabDestroyed().AddRaw(this, &SChaosVDMainTab::HandleTabDestroyed, TabID);
		TabSpawnersByIDMap.Add(TabID, TabSpawner);
	}
}
