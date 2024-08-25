// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangePipelineConfigurationGeneric.h"

#include "Framework/Application/SlateApplication.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Interfaces/IMainFrameModule.h"
#include "SInterchangePipelineConfigurationDialog.h"
#include "Widgets/SWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePipelineConfigurationGeneric)

EInterchangePipelineConfigurationDialogResult UInterchangePipelineConfigurationGeneric::ShowPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
	, TArray<UInterchangePipelineBase*>& OutPipelines
	, TWeakObjectPtr<UInterchangeSourceData> SourceData
	, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
	, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer)
{
	//Create and show the graph inspector UI dialog
	TSharedPtr<SWindow> ParentWindow;
	if(IMainFrameModule* MainFrame = FModuleManager::LoadModulePtr<IMainFrameModule>("MainFrame"))
	{
		ParentWindow = MainFrame->GetParentWindow();
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(1000.f, 650.f))
		.Title(NSLOCTEXT("Interchange", "PipelineConfigurationGenericTitleContent", "Interchange Pipeline Configuration (Import Content)"));
	TSharedPtr<SInterchangePipelineConfigurationDialog> InterchangePipelineConfigurationDialog;

	Window->SetContent
	(
		SAssignNew(InterchangePipelineConfigurationDialog, SInterchangePipelineConfigurationDialog)
		.OwnerWindow(Window)
		.SourceData(SourceData)
		.bSceneImport(false)
		.bReimport(false)
		.PipelineStacks(PipelineStacks)
		.OutPipelines(&OutPipelines)
		.BaseNodeContainer(BaseNodeContainer)
		.Translator(Translator)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	
	if (InterchangePipelineConfigurationDialog->IsCanceled())
	{
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}
	
	if (InterchangePipelineConfigurationDialog->IsImportAll())
	{
		return EInterchangePipelineConfigurationDialogResult::ImportAll;
	}

	return EInterchangePipelineConfigurationDialogResult::Import;
}

EInterchangePipelineConfigurationDialogResult UInterchangePipelineConfigurationGeneric::ShowScenePipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
	, TArray<UInterchangePipelineBase*>& OutPipelines
	, TWeakObjectPtr<UInterchangeSourceData> SourceData
	, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
	, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer)
{
	//Create and show the graph inspector UI dialog
	TSharedPtr<SWindow> ParentWindow;
	if (IMainFrameModule* MainFrame = FModuleManager::LoadModulePtr<IMainFrameModule>("MainFrame"))
	{
		ParentWindow = MainFrame->GetParentWindow();
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(1000.f, 650.f))
		.Title(NSLOCTEXT("Interchange", "PipelineConfigurationGenericTitleScene", "Interchange Pipeline Configuration (Import Scene)"));
	TSharedPtr<SInterchangePipelineConfigurationDialog> InterchangePipelineConfigurationDialog;

	Window->SetContent
	(
		SAssignNew(InterchangePipelineConfigurationDialog, SInterchangePipelineConfigurationDialog)
		.OwnerWindow(Window)
		.SourceData(SourceData)
		.bSceneImport(true)
		.bReimport(false)
		.PipelineStacks(PipelineStacks)
		.OutPipelines(&OutPipelines)
		.BaseNodeContainer(BaseNodeContainer)
		.Translator(Translator)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (InterchangePipelineConfigurationDialog->IsCanceled())
	{
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	if (InterchangePipelineConfigurationDialog->IsImportAll())
	{
		return EInterchangePipelineConfigurationDialogResult::ImportAll;
	}

	return EInterchangePipelineConfigurationDialogResult::Import;
}

EInterchangePipelineConfigurationDialogResult UInterchangePipelineConfigurationGeneric::ShowReimportPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
	, TArray<UInterchangePipelineBase*>& OutPipelines
	, TWeakObjectPtr<UInterchangeSourceData> SourceData
	, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
	, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer
	, TWeakObjectPtr <UObject> ReimportAsset)
{
	//Create and show the graph inspector UI dialog
	TSharedPtr<SWindow> ParentWindow;
	if (IMainFrameModule* MainFrame = FModuleManager::LoadModulePtr<IMainFrameModule>("MainFrame"))
	{
		ParentWindow = MainFrame->GetParentWindow();
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(1000.f, 650.f))
		.Title(NSLOCTEXT("Interchange", "PipelineConfigurationGenericTitleReimportContent", "Interchange Pipeline Configuration (Reimport Content)"));
	TSharedPtr<SInterchangePipelineConfigurationDialog> InterchangePipelineConfigurationDialog;

	Window->SetContent
	(
		SAssignNew(InterchangePipelineConfigurationDialog, SInterchangePipelineConfigurationDialog)
		.OwnerWindow(Window)
		.SourceData(SourceData)
		.bSceneImport(false)
		.bReimport(true)
		.PipelineStacks(PipelineStacks)
		.OutPipelines(&OutPipelines)
		.BaseNodeContainer(BaseNodeContainer)
		.ReimportObject(ReimportAsset)
		.Translator(Translator)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (InterchangePipelineConfigurationDialog->IsCanceled())
	{
		return EInterchangePipelineConfigurationDialogResult::Cancel;
	}

	if (InterchangePipelineConfigurationDialog->IsImportAll())
	{
		return EInterchangePipelineConfigurationDialogResult::ImportAll;
	}

	return EInterchangePipelineConfigurationDialogResult::Import;
}
