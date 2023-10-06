// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_Redirector.h"

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "IAssetTools.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_Redirector"

namespace RedirectorUtilities
{
	static void FindTargets(const TArray<UObjectRedirector*>& Redirectors)
	{
		TArray<UObject*> ObjectsToSync;
		for (auto RedirectorIt = Redirectors.CreateConstIterator(); RedirectorIt; ++RedirectorIt)
		{
			const UObjectRedirector* Redirector = *RedirectorIt;
			if ( Redirector && Redirector->DestinationObject )
			{
				ObjectsToSync.Add(Redirector->DestinationObject);
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

namespace MenuExtension_Redirector
{
	static void ExecuteFindTarget(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TArray<UObjectRedirector*> Redirectors = Context->LoadSelectedObjects<UObjectRedirector>();
			RedirectorUtilities::FindTargets(Redirectors);
		}
	}

	static void ExecuteFixUp(const FToolMenuContext& MenuContext, bool bDeleteAssets)
	{
		// This will fix references to selected redirectors, except in the following cases:
		// Redirectors referenced by unloaded maps will not be fixed up, but any references to it that can be fixed up will
		// Redirectors referenced by code will not be completely fixed up
		// Redirectors that are not at head revision or checked out by another user will not be completely fixed up
		// Redirectors whose referencers are not at head revision, are checked out by another user, or are refused to be checked out will not be completely fixed up.

		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TArray<UObjectRedirector*> Redirectors = Context->LoadSelectedObjects<UObjectRedirector>();

			if (Redirectors.Num() > 0)
			{
				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
				AssetTools.FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/ true, bDeleteAssets ? ERedirectFixupMode::DeleteFixedUpRedirectors : ERedirectFixupMode::LeaveFixedUpRedirectors);
			}
		}
	}
	
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UObjectRedirector::StaticClass());
	        
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("Redirector_FindTarget", "Find Target");
					const TAttribute<FText> ToolTip = LOCTEXT("Redirector_FindTargetTooltip", "Finds the asset that this redirector targets in the asset tree.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ObjectRedirector");
					const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteFindTarget);

					InSection.AddMenuEntry("Redirector_FindTarget", Label, ToolTip, Icon, UIAction);
				}
				{
					const TAttribute<FText> Label = LOCTEXT("Redirector_FixUp", "Fix Up");
					const TAttribute<FText> ToolTip = LOCTEXT("Redirector_FixUpTooltip", "Finds referencers to selected redirectors and resaves them if possible, then deletes any redirectors that had all their referencers fixed.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ObjectRedirector");
					const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteFixUp, true);

					InSection.AddMenuEntry("Redirector_FixUp", Label, ToolTip, Icon, UIAction);
				}
				{
					const TAttribute<FText> Label = LOCTEXT("Redirector_FixUp_KeepingRedirector", "Fix Up (Keep Redirector)");
					const TAttribute<FText> ToolTip = LOCTEXT("Redirector_FixUp_KeepingRedirectorTooltip", "Finds referencers to selected redirectors and resaves them if possible.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MyAsset");
					const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteFixUp, false);

					InSection.AddMenuEntry("Redirector_FixUp_KeepingRedirector", Label, ToolTip, Icon, UIAction);
				}
			}));
		}));
	});
}

EAssetCommandResult UAssetDefinition_Redirector::ActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	if ( ActivateArgs.ActivationMethod == EAssetActivationMethod::DoubleClicked || ActivateArgs.ActivationMethod == EAssetActivationMethod::Opened )
	{
		// Sync to the target instead of opening an editor when double clicked
		TArray<UObjectRedirector*> Redirectors = ActivateArgs.LoadObjects<UObjectRedirector>();
		if ( Redirectors.Num() > 0 )
		{
			RedirectorUtilities::FindTargets(Redirectors);
			return EAssetCommandResult::Handled;
		}
	}

	return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
