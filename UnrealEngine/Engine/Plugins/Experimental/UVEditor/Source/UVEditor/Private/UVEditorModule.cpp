// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorModule.h"

#include "Features/IModularFeatures.h"
#include "ContentBrowserMenuContexts.h"
#include "EditorModeRegistry.h"
#include "LevelEditorMenuContext.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "UVEditor.h"
#include "UVEditorCommands.h"
#include "UVEditorMode.h"
#include "UVEditorStyle.h"
#include "UVEditorSubsystem.h"
#include "UVSelectTool.h"

#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FUVEditorModule"

void FUVEditorModule::StartupModule()
{
	UVEditorAssetEditor = MakeShared<UE::Geometry::FUVEditorAssetEditorImpl>();

	if (UVEditorAssetEditor.IsValid())
	{
		IModularFeatures::Get().RegisterModularFeature(IGeometryProcessing_UVEditorAssetEditor::GetModularFeatureName(), UVEditorAssetEditor.Get());
	}

	FUVEditorStyle::Get(); // Causes the constructor to be called
	FUVEditorCommands::Register();

	// Menus need to be registered in a callback to make sure the system is ready for them.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUVEditorModule::RegisterMenus));

	// Register details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassesToUnregisterOnShutdown.Reset();
	// Customizations get registered like this:
	//PropertyModule.RegisterCustomClassLayout(USelectToolActionPropertySet::StaticClass()->GetFName(), 
	//	FOnGetDetailCustomizationInstance::CreateStatic(&FUVSelectToolActionPropertySetDetails::MakeInstance));
	//ClassesToUnregisterOnShutdown.Add(USelectToolActionPropertySet::StaticClass()->GetFName());
}

void FUVEditorModule::ShutdownModule()
{
	if (UVEditorAssetEditor.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(IGeometryProcessing_UVEditorAssetEditor::GetModularFeatureName(), UVEditorAssetEditor.Get());
		UVEditorAssetEditor = nullptr;
	}

	// Clean up menu things
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FUVEditorCommands::Unregister();

	FEditorModeRegistry::Get().UnregisterMode(UUVEditorMode::EM_UVEditorModeId);

	// Unregister customizations
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
	}
}

void FUVEditorModule::RegisterMenus()
{
	// Allows cleanup when module unloads.
	FToolMenuOwnerScoped OwnerScoped(this);

	// Extend the content browser context menu for static meshes and skeletal meshes
	auto AddToContextMenuSection = [this](FToolMenuSection& Section)
	{
		Section.AddDynamicEntry("OpenUVEditor", FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& Section)
			{
				// We'll need to get the target objects out of the context
				if (UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					TArray<TObjectPtr<UObject>> AssetsToEdit;
					AssetsToEdit.Append(Context->GetSelectedObjects());

					UUVEditorSubsystem* UVSubsystem = GEditor->GetEditorSubsystem<UUVEditorSubsystem>();
					check(UVSubsystem);

					if (UVSubsystem->AreObjectsValidTargets(AssetsToEdit))
					{
						TSharedPtr<class FUICommandList> CommandListToBind = MakeShared<FUICommandList>();
						CommandListToBind->MapAction(
							FUVEditorCommands::Get().OpenUVEditor,
							FExecuteAction::CreateUObject(UVSubsystem, &UUVEditorSubsystem::StartUVEditor, AssetsToEdit));

						Section.AddMenuEntryWithCommandList(FUVEditorCommands::Get().OpenUVEditor, CommandListToBind, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FUVEditorStyle::Get().GetStyleSetName(), "UVEditor.OpenUVEditor"));
					}
				}
			}));
	};
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		AddToContextMenuSection(Section);
	}
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SkeletalMesh");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		AddToContextMenuSection(Section);
	}

	// Extend the level editor menus

	// Helper struct to collect all necessary inputs for a menu entry.
	struct FMenuEntryParameters
	{
		TArray<TObjectPtr<UObject>> TargetObjects;
		UUVEditorSubsystem* UVSubsystem;
		bool bValidTargets;
	};

	// Sets up all parameters for a menu entry.
	auto SetupMenuEntryParameters = []
	{
		FMenuEntryParameters Result;

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			// We are interested in the (unique) assets backing the actor, or else the actor
			// itself if it is not asset backed (such as UDynamicMesh).
			const AActor* Actor = static_cast<AActor*>(*It);
			TArray<UObject*> ActorAssets;
			Actor->GetReferencedContentObjects(ActorAssets);

			if (ActorAssets.Num() > 0)
			{
				for (UObject* Asset : ActorAssets)
				{
					Result.TargetObjects.AddUnique(Asset);
				}
			}
			else
			{
				// Need to transform actors to components here because this is what our tool targets expect
				TInlineComponentArray<UActorComponent*> ActorComponents;
				Actor->GetComponents(ActorComponents);
				Result.TargetObjects.Append(ActorComponents);
			}
		}

		Result.UVSubsystem = GEditor->GetEditorSubsystem<UUVEditorSubsystem>();
		check(Result.UVSubsystem);

		Result.bValidTargets = Result.UVSubsystem->AreObjectsValidTargets(Result.TargetObjects);

		return Result;
	};

	// Adds a menu entry for the given parameters.
	auto AddMenuEntry = [](FToolMenuSection& Section, const FMenuEntryParameters& MenuEntryParameters)
	{
		const bool bValidTargets = MenuEntryParameters.bValidTargets;

		const TSharedPtr<class FUICommandList> CommandListToBind = MakeShared<FUICommandList>();
		CommandListToBind->MapAction(
			FUVEditorCommands::Get().OpenUVEditor,
			FExecuteAction::CreateUObject(MenuEntryParameters.UVSubsystem, &UUVEditorSubsystem::StartUVEditor, MenuEntryParameters.TargetObjects),
			FCanExecuteAction::CreateLambda([bValidTargets]() { return bValidTargets; }));

		Section.AddMenuEntryWithCommandList(FUVEditorCommands::Get().OpenUVEditor, CommandListToBind);
	};

	// Add UV Editor to actor context menu.
	{
		UToolMenu* ActorContextMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		FToolMenuSection& ActorTypeToolsSection = ActorContextMenu->FindOrAddSection("ActorTypeTools");
		ActorTypeToolsSection.AddDynamicEntry("OpenUVEditor", FNewToolMenuSectionDelegate::CreateLambda(
			                                      [&SetupMenuEntryParameters, &AddMenuEntry](FToolMenuSection& Section)
			                                      {
				                                      const FMenuEntryParameters MenuEntryParameters = SetupMenuEntryParameters();
				                                      if (MenuEntryParameters.bValidTargets)
				                                      {
					                                      AddMenuEntry(Section, MenuEntryParameters);
				                                      }
			                                      }));
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUVEditorModule, UVEditor)