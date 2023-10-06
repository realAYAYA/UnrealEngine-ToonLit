// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterLibraryEditorMode.h"
#include "AnimNextParameterLibraryEditor.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Layout/SSpacer.h"
#include "Modules/ModuleManager.h"
#include "SParameterLibraryView.h"
#include "Param/AnimNextParameterLibrary.h"

#define LOCTEXT_NAMESPACE "ParameterLibraryEditorMode"

namespace UE::AnimNext::Editor
{

class FParameterLibraryDetailsTabSummoner : public FWorkflowTabFactory
{
public:
	FParameterLibraryDetailsTabSummoner(TSharedPtr<FParameterLibraryEditor> InHostingApp, FOnDetailsViewCreated InOnDetailsViewCreated)
		: FWorkflowTabFactory(ParameterLibraryTabs::Details, InHostingApp)
	{
		TabLabel = LOCTEXT("DetailsTabLabel", "Details");
		TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details");
		ViewMenuDescription = LOCTEXT("DetailsTabMenuDescription", "Details");
		ViewMenuTooltip = LOCTEXT("DetailsTabToolTip", "Shows the details tab for selected objects.");
		bIsSingleton = true;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		InOnDetailsViewCreated.ExecuteIfBound(DetailsView.ToSharedRef());
	}

	TSharedPtr<IDetailsView> GetDetailsView() const
	{
		return DetailsView;
	}

private:
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		return DetailsView.ToSharedRef();
	}
	
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return ViewMenuTooltip;
	}
	
	TSharedPtr<IDetailsView> DetailsView;
};

class FParameterLibraryTabSummoner : public FWorkflowTabFactory
{
public:
	FParameterLibraryTabSummoner(TSharedPtr<FParameterLibraryEditor> InHostingApp)
		: FWorkflowTabFactory(ParameterLibraryTabs::Parameters, InHostingApp)
	{
		TabLabel = LOCTEXT("ParameterLibraryTabLabel", "Parameter Library");
		TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details");
		ViewMenuDescription = LOCTEXT("ParameterLibraryTabMenuDescription", "Parameter Library");
		ViewMenuTooltip = LOCTEXT("ParameterLibraryTabToolTip", "Shows all parameters in this parameter library.");
		bIsSingleton = true;
	}

private:
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		return SNew(SParameterLibraryView, StaticCastSharedPtr<FParameterLibraryEditor>(HostingApp.Pin())->ParameterLibrary)
			.OnSelectionChanged(this, &FParameterLibraryTabSummoner::HandleSelectionChanged);
	}
	
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return ViewMenuTooltip;
	}

	void HandleSelectionChanged(const TArray<UObject*>& InObjects) const
	{
		TSharedPtr<FParameterLibraryEditor> ParameterLibraryEditor = StaticCastSharedPtr<FParameterLibraryEditor>(HostingApp.Pin());
		ParameterLibraryEditor->SetSelectedObjects(InObjects);
	}
};

FParameterLibraryEditorMode::FParameterLibraryEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp)
	: FApplicationMode(ParameterLibraryModes::ParameterLibraryEditor)
	, HostingAppPtr(InHostingApp)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FParameterLibraryEditor> ParameterLibraryEditor = StaticCastSharedRef<FParameterLibraryEditor>(InHostingApp);
	
	TabFactories.RegisterFactory(MakeShared<FParameterLibraryDetailsTabSummoner>(ParameterLibraryEditor, FOnDetailsViewCreated::CreateSP(&ParameterLibraryEditor.Get(), &FParameterLibraryEditor::HandleDetailsViewCreated)));
	TabFactories.RegisterFactory(MakeShared<FParameterLibraryTabSummoner>(ParameterLibraryEditor));

	TabLayout = FTabManager::NewLayout("Standalone_AnimNextParameterLibraryEditor_Layout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(1.0f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(false)
					->AddTab(ParameterLibraryTabs::Parameters, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(false)
					->AddTab(ParameterLibraryTabs::Details, ETabState::OpenedTab)
				)
			)
		);

	ParameterLibraryEditor->RegisterModeToolbarIfUnregistered(GetModeName());
}

void FParameterLibraryEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FParameterLibraryEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FParameterLibraryEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

}

#undef LOCTEXT_NAMESPACE