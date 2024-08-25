// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEEditorModelDataEditorToolkit.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNEEditorRuntimesWidget.h"
#include "Widgets/Docking/SDockTab.h"

namespace UE::NNEEditor::Private
{

	void FModelDataEditorToolkit::InitEditor(UNNEModelData* InModelData)
	{
		ModelData = InModelData;

		const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("NNEModelDataEditorLayout")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->AddTab(TEXT("NNEModelDataTab"), ETabState::OpenedTab)
				)
			);

		FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, TEXT("NNEModelDataEditor"), Layout, true, true, ModelData);
	}

	void FModelDataEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("NNE Model Data Editor"));

		InTabManager->RegisterTabSpawner("NNEModelDataTab", FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs&)
			{
				return SNew(SDockTab)
				[
					SNew(SRuntimesWidget)
					.TargetRuntimes(this, &FModelDataEditorToolkit::GetTargetRuntimes)
					.OnTargetRuntimesChanged(this, &FModelDataEditorToolkit::SetTargetRuntimes)
				];
			}))
			.SetDisplayName(INVTEXT("NNE Model Data"))
			.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	}

	void FModelDataEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
		InTabManager->UnregisterTabSpawner("NNEModelDataTab");
	}

	TArrayView<const FString> FModelDataEditorToolkit::GetTargetRuntimes() const
	{
		check(ModelData != nullptr);
		return ModelData->GetTargetRuntimes();
	}

	void FModelDataEditorToolkit::SetTargetRuntimes(TArrayView<const FString> TargetRuntimes)
	{
		check(ModelData != nullptr);
		ModelData->Modify();
		ModelData->SetTargetRuntimes(TargetRuntimes);
	}

} // UE::NNEEditor::Private