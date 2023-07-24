// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsModule.h"

#include "SkeletalMeshModelingToolsCommands.h"
#include "SkeletalMeshModelingToolsEditorMode.h"
#include "SkeletalMeshModelingToolsMeshConverter.h"
#include "SkeletalMeshModelingToolsStyle.h"

#include "ContentBrowserMenuContexts.h"
#include "EditorModeManager.h"
#include "Engine/SkeletalMesh.h"
#include "ISkeletalMeshEditor.h"
#include "ISkeletalMeshEditorModule.h"
#include "Modules/ModuleManager.h"
#include "SkeletalMeshToolMenuContext.h"
#include "ToolMenus.h"
#include "Styling/SlateIconFinder.h"
#include "WorkflowOrientedApp/ApplicationMode.h"


#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsModule"

DEFINE_LOG_CATEGORY(LogSkeletalMeshModelingTools);

IMPLEMENT_MODULE(FSkeletalMeshModelingToolsModule, SkeletalMeshModelingTools);



void FSkeletalMeshModelingToolsModule::StartupModule()
{
	FSkeletalMeshModelingToolsStyle::Register();
	FSkeletalMeshModelingToolsCommands::Register();

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSkeletalMeshModelingToolsModule::RegisterMenusAndToolbars));

	ISkeletalMeshEditorModule& SkelMeshEditorModule = FModuleManager::Get().LoadModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender>& ToolbarExtenders = SkelMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();

	ToolbarExtenders.Add(ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender::CreateRaw(this, &FSkeletalMeshModelingToolsModule::ExtendSkelMeshEditorToolbar));
	SkelMeshEditorExtenderHandle = ToolbarExtenders.Last().GetHandle();
}

void FSkeletalMeshModelingToolsModule::ShutdownModule()
{
	if (ISkeletalMeshEditorModule* SkelMeshEditorModule = FModuleManager::GetModulePtr<ISkeletalMeshEditorModule>("SkeletalMeshEditor"))
	{
		TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender>& Extenders = SkelMeshEditorModule->GetAllSkeletalMeshEditorToolbarExtenders();

		Extenders.RemoveAll([=](const ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender& InDelegate) { return InDelegate.GetHandle() == SkelMeshEditorExtenderHandle; });
	}

	UToolMenus::UnregisterOwner(this);

	FSkeletalMeshModelingToolsCommands::Unregister();
	FSkeletalMeshModelingToolsStyle::Unregister();
}


void FSkeletalMeshModelingToolsModule::RegisterMenusAndToolbars()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu("AssetEditor.SkeletalMeshEditor.ToolBar"))
	{
		FToolMenuSection& Section = ToolMenu->FindOrAddSection("SkeletalMesh");
		Section.AddDynamicEntry("ToggleModelingToolsMode", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection) {
			const USkeletalMeshToolMenuContext* Context = InSection.FindContext<USkeletalMeshToolMenuContext>();
			if (Context && Context->SkeletalMeshEditor.IsValid())
			{
				InSection.AddEntry(FToolMenuEntry::InitToolBarButton(
				    FSkeletalMeshModelingToolsCommands::Get().ToggleModelingToolsMode,
					LOCTEXT("SkeletalMeshEditorModelingMode", "Modeling Tools"),
				    LOCTEXT("SkeletalMeshEditorModelingModeTooltip", "Opens the Modeling Tools palette that provides selected mesh modification tools."),
				    FSlateIcon("ModelingToolsStyle", "LevelEditor.ModelingToolsMode")));
			}
		}));
	}

	// Extend the asset browser for static meshes to include the conversion to skelmesh.
	if (UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh"))
	{
		FToolMenuSection& Section = ToolMenu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("ConvertToSkeletalMesh", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = LOCTEXT("ConvertToSkeletalMesh", "Convert to Skeletal Mesh");
			const TAttribute<FText> ToolTip = LOCTEXT("ConvertToSkeletalMeshToolTip", "Converts a non-Nanite static mesh to a skeletal mesh with an associated skeleton.");
			const FSlateIcon Icon = FSlateIconFinder::FindIconForClass(USkeletalMesh::StaticClass());
			const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InMenuContext)
			{
				if (const UContentBrowserAssetContextMenuContext* Context = InMenuContext.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					ConvertStaticMeshToSkeletalMeshInteractive(
						Context->GetSelectedAssetsOfType(UStaticMesh::StaticClass()));
				}
			});

			InSection.AddMenuEntry("ConvertToSkeletalMesh", Label, ToolTip, Icon, UIAction);
		}));
	}
}


TSharedRef<FExtender> FSkeletalMeshModelingToolsModule::ExtendSkelMeshEditorToolbar(const TSharedRef<FUICommandList> InCommandList, TSharedRef<ISkeletalMeshEditor> InSkeletalMeshEditor)
{
	// Add toolbar extender
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	TWeakPtr<ISkeletalMeshEditor> Ptr(InSkeletalMeshEditor);

	InCommandList->MapAction(FSkeletalMeshModelingToolsCommands::Get().ToggleModelingToolsMode,
	    FExecuteAction::CreateRaw(this, &FSkeletalMeshModelingToolsModule::OnToggleModelingToolsMode, Ptr),
	    FCanExecuteAction(),
	    FIsActionChecked::CreateRaw(this, &FSkeletalMeshModelingToolsModule::IsModelingToolModeActive, Ptr));

	return ToolbarExtender.ToSharedRef();
}


bool FSkeletalMeshModelingToolsModule::IsModelingToolModeActive(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor) const
{
	TSharedPtr<ISkeletalMeshEditor> SkeletalMeshEditor = InSkeletalMeshEditor.Pin();
	return SkeletalMeshEditor.IsValid() && SkeletalMeshEditor->GetEditorModeManager().IsModeActive(USkeletalMeshModelingToolsEditorMode::Id);
}


void FSkeletalMeshModelingToolsModule::OnToggleModelingToolsMode(TWeakPtr<ISkeletalMeshEditor> InSkeletalMeshEditor)
{
	TSharedPtr<ISkeletalMeshEditor> SkeletalMeshEditor = InSkeletalMeshEditor.Pin();
	if (SkeletalMeshEditor.IsValid())
	{
		if (!IsModelingToolModeActive(InSkeletalMeshEditor))
		{
			SkeletalMeshEditor->GetEditorModeManager().ActivateMode(USkeletalMeshModelingToolsEditorMode::Id, true);
		}
		else
		{
			SkeletalMeshEditor->GetEditorModeManager().ActivateDefaultMode();
		}
	}
}


#undef LOCTEXT_NAMESPACE
