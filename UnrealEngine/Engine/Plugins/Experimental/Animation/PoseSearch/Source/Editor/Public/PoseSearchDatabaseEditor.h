// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Misc/NotifyHook.h"
#include "SPoseSearchDatabaseAssetList.h"

class IDetailsView;
class FToolBarBuilder;
class UPoseSearchDatabaseSelectionReflection;

namespace UE::PoseSearch
{
	class SDatabasePreview;
	class FDatabasePreviewScene;
	class FDatabaseViewModel;

	class FDatabaseEditor : public FAssetEditorToolkit, public FNotifyHook
	{
	public:

		FDatabaseEditor();
		virtual ~FDatabaseEditor();

		void InitAssetEditor(
			const EToolkitMode::Type Mode,
			const TSharedPtr<IToolkitHost>& InitToolkitHost,
			UPoseSearchDatabase* DatabaseAsset);

		virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
		virtual FName GetToolkitFName() const override;
		virtual FText GetBaseToolkitName() const override;
		virtual FText GetToolkitName() const override;
		virtual FLinearColor GetWorldCentricTabColorScale() const override;
		virtual FString GetWorldCentricTabPrefix() const override;

		const UPoseSearchDatabase* GetPoseSearchDatabase() const;
		UPoseSearchDatabase* GetPoseSearchDatabase();
		FDatabaseViewModel* GetViewModel() const { return ViewModel.Get(); }
		TSharedPtr<FDatabaseViewModel> GetViewModelSharedPtr() const { return ViewModel; }

		void BuildSearchIndex();

		void PreviewBackwardEnd();
		void PreviewBackwardStep();
		void PreviewBackward();
		void PreviewPause();
		void PreviewForward();
		void PreviewForwardStep();
		void PreviewForwardEnd();

	private:

		TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
		TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
		TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);
		TSharedRef<SDockTab> SpawnTab_AssetTreeView(const FSpawnTabArgs& Args);
		TSharedRef<SDockTab> SpawnTab_SelectionDetails(const FSpawnTabArgs& Args);

		void BindCommands();
		void ExtendToolbar();
		void FillToolbar(FToolBarBuilder& ToolbarBuilder);

		void OnFinishedChangingSelectionProperties(const FPropertyChangedEvent& PropertyChangedEvent);
		void OnAssetTreeSelectionChanged(
			const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& SelectedItems,
			ESelectInfo::Type SelectionType);

		TSharedPtr<SDatabasePreview> PreviewWidget;

		TSharedPtr<SDatabaseAssetTree> AssetTreeWidget;

		TSharedPtr<IDetailsView> EditingAssetWidget;

		TArray<TSharedPtr<IDetailsView>> SelectionWidgets;

		TSharedPtr<FDatabasePreviewScene> PreviewScene;

		TSharedPtr<FDatabaseViewModel> ViewModel;
	};
}

