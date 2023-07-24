// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelViewViewModelEditorModule.h"

#include "BlueprintModes/WidgetBlueprintApplicationMode.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "Customizations/MVVMPropertyBindingExtension.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MessageLogModule.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorCommands.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Styling/MVVMEditorStyle.h"
#include "Tabs/MVVMBindingSummoner.h"
#include "Tabs/MVVMViewModelSummoner.h"
#include "UMGEditorModule.h"
#include "ViewModel/AssetTypeActions_ViewModelBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "WidgetDrawerConfig.h"

#define LOCTEXT_NAMESPACE "ModelViewViewModelModule"

/** Rather than expose and depend on private include which won't have _API, we just duplicate anim tab string here */
const FName AnimationTabSummonerTabID(TEXT("Animations"));

void FModelViewViewModelEditorModule::StartupModule()
{
	FMVVMEditorStyle::CreateInstance();

	IUMGEditorModule& UMGEditorModule = FModuleManager::LoadModuleChecked<IUMGEditorModule>("UMGEditor");
	UMGEditorModule.OnRegisterTabsForEditor().AddRaw(this, &FModelViewViewModelEditorModule::HandleRegisterBlueprintEditorTab);

	// Register asset types
	{
#if UE_MVVM_WITH_VIEWMODEL_EDITOR
		// Only remove what is related to the viewmodel editor, not the UMG extention for view.
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		ViewModelBlueprintActions = MakeShared<UE::MVVM::FAssetTypeActions_ViewModelBlueprint>();
		AssetTools.RegisterAssetTypeActions(ViewModelBlueprintActions.ToSharedRef());
#endif
	}

	PropertyBindingExtension = MakeShared<FMVVMPropertyBindingExtension>();
	UMGEditorModule.GetPropertyBindingExtensibilityManager()->AddExtension(PropertyBindingExtension.ToSharedRef());

	FBlueprintEditorUtils::OnRenameVariableReferencesEvent.AddRaw(this, &FModelViewViewModelEditorModule::HandleRenameVariableReferences);

	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowFilters = true;
		InitOptions.bShowPages = false;
		InitOptions.bAllowClear = true;
		MessageLogModule.RegisterLogListing("Model View Viewmodel", LOCTEXT("MVVMLog", "Model View Viewmodel"), InitOptions);
	}

	FMVVMEditorCommands::Register();
	FWidgetBlueprintDelegates::GetAssetTags.AddRaw(this, &FModelViewViewModelEditorModule::HandleAssetTags);
}


void FModelViewViewModelEditorModule::ShutdownModule()
{
	FWidgetBlueprintDelegates::GetAssetTags.RemoveAll(this);
	if (FMessageLogModule* MessageLogModule = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog"))
	{
		MessageLogModule->UnregisterLogListing("Model View Viewmodel");
	}

	FBlueprintEditorUtils::OnRenameVariableReferencesEvent.RemoveAll(this);

	if (IUMGEditorModule* UMGEditorModule = FModuleManager::GetModulePtr<IUMGEditorModule>("UMGEditor"))
	{
		UMGEditorModule->OnRegisterTabsForEditor().RemoveAll(this);
		UMGEditorModule->GetPropertyBindingExtensibilityManager()->RemoveExtension(PropertyBindingExtension.ToSharedRef());
	}
	PropertyBindingExtension.Reset();

#if UE_MVVM_WITH_VIEWMODEL_EDITOR
	// Unregister all the asset types that we registered
	if (FAssetToolsModule* AssetTools = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		if (ViewModelBlueprintActions)
		{
			AssetTools->Get().UnregisterAssetTypeActions(ViewModelBlueprintActions.ToSharedRef());
		}
	}
#endif

	FMVVMEditorStyle::DestroyInstance();

	FMVVMEditorCommands::Unregister();
}


void FModelViewViewModelEditorModule::HandleRegisterBlueprintEditorTab(const FWidgetBlueprintApplicationMode& ApplicationMode, FWorkflowAllowedTabSet& TabFactories)
{
	if (ApplicationMode.GetModeName() == FWidgetBlueprintApplicationModes::DesignerMode)
	{
		TabFactories.RegisterFactory(MakeShared<FMVVMBindingSummoner>(ApplicationMode.GetBlueprintEditor()));
		TabFactories.RegisterFactory(MakeShared<UE::MVVM::FViewModelSummoner>(ApplicationMode.GetBlueprintEditor()));

		if (ApplicationMode.LayoutExtender)
		{
			FTabManager::FTab NewTab(FTabId(FMVVMBindingSummoner::TabID, ETabIdFlags::SaveLayout), ETabState::ClosedTab);
			ApplicationMode.LayoutExtender->ExtendLayout(AnimationTabSummonerTabID, ELayoutExtensionPosition::After, NewTab);

			ApplicationMode.OnPostActivateMode.AddRaw(this, &FModelViewViewModelEditorModule::HandleActivateMode);
			ApplicationMode.OnPreDeactivateMode.AddRaw(this, &FModelViewViewModelEditorModule::HandleDeactiveMode);
		}
	}
}


void FModelViewViewModelEditorModule::HandleRenameVariableReferences(UBlueprint* Blueprint, UClass* VariableClass, const FName& OldVarName, const FName& NewVarName)
{
	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint))
	{
		if (UMVVMWidgetBlueprintExtension_View* ViewExtension = UWidgetBlueprintExtension::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint))
		{
			if (UMVVMBlueprintView* BlueprintView = ViewExtension->GetBlueprintView())
			{
				BlueprintView->WidgetRenamed(OldVarName, NewVarName);
			}
		}
	}
}

void FModelViewViewModelEditorModule::HandleDeactiveMode(FWidgetBlueprintApplicationMode& InDesignerMode)
{
	TSharedPtr<FWidgetBlueprintEditor> BP = InDesignerMode.GetBlueprintEditor();
	if (BP && BP->IsEditorClosing())
	{
		InDesignerMode.OnPostActivateMode.RemoveAll(this);
		InDesignerMode.OnPreDeactivateMode.RemoveAll(this);
	}
}

void FModelViewViewModelEditorModule::HandleActivateMode(FWidgetBlueprintApplicationMode& InDesignerMode)
{
	if (TSharedPtr<FWidgetBlueprintEditor> BP = InDesignerMode.GetBlueprintEditor())
	{
		if (!BP->GetExternalEditorWidget(FMVVMBindingSummoner::DrawerID))
		{
			bool bIsDrawerTab = true;
			FMVVMBindingSummoner MVVMDrawerSummoner(BP, bIsDrawerTab);
			FWorkflowTabSpawnInfo SpawnInfo;
			BP->AddExternalEditorWidget(FMVVMBindingSummoner::DrawerID, MVVMDrawerSummoner.CreateTabBody(SpawnInfo));
		}

		// Add MVVM Drawer
		{
			FWidgetDrawerConfig MVVMDrawer(FMVVMBindingSummoner::DrawerID);
			TWeakPtr<FWidgetBlueprintEditor> WeakBP = BP;
			MVVMDrawer.GetDrawerContentDelegate.BindLambda([WeakBP]()
			{
				if (TSharedPtr<FWidgetBlueprintEditor> BP = WeakBP.Pin())
				{
					TSharedPtr<SWidget> DrawerWidgetContent = BP->GetExternalEditorWidget(FMVVMBindingSummoner::DrawerID);
					if (DrawerWidgetContent)
					{
						return DrawerWidgetContent.ToSharedRef();
					}
				}

				return SNullWidget::NullWidget;
			});
			MVVMDrawer.OnDrawerOpenedDelegate.BindLambda([WeakBP](FName StatusBarWithDrawerName)
			{
				if (TSharedPtr<FWidgetBlueprintEditor> BP = WeakBP.Pin())
				{
					FSlateApplication::Get().SetUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard(), BP->GetExternalEditorWidget(FMVVMBindingSummoner::DrawerID));
				}
			});
			MVVMDrawer.OnDrawerDismissedDelegate.BindLambda([WeakBP](const TSharedPtr<SWidget>& NewlyFocusedWidget)
			{
				if (TSharedPtr<FWidgetBlueprintEditor> BP = WeakBP.Pin())
				{
					BP->SetKeyboardFocus();
				}
			});
			MVVMDrawer.ButtonText = LOCTEXT("StatusBar_MVVM", "View Bindings");
			MVVMDrawer.ToolTipText = LOCTEXT("StatusBar_MVVMToolTip", "Opens MVVM Bindings (Ctrl+Shift+B).");
			MVVMDrawer.Icon = FMVVMEditorStyle::Get().GetBrush("BlueprintView.TabIcon");
			BP->RegisterDrawer(MoveTemp(MVVMDrawer), 1);
		}

		BP->GetToolkitCommands()->MapAction(FMVVMEditorCommands::Get().ToggleMVVMDrawer,
			FExecuteAction::CreateStatic(&FMVVMBindingSummoner::ToggleMVVMDrawer)
		);
	}
}

void FModelViewViewModelEditorModule::HandleAssetTags(const UWidgetBlueprint* WidgetBlueprint, TArray<UObject::FAssetRegistryTag>& OutTags)
{
	if (WidgetBlueprint && GEditor)
	{
		if (UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>())
		{
			if (UMVVMBlueprintView* BlueprintView = Subsystem->GetView(WidgetBlueprint))
			{
				BlueprintView->AddAssetTags(OutTags);
			}
		}
	}
}

IMPLEMENT_MODULE(FModelViewViewModelEditorModule, ModelViewViewModelEditor);

#undef LOCTEXT_NAMESPACE
