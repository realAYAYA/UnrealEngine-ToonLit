// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationModifiersModule.h"

#include "Animation/AnimSequence.h"
#include "AnimationModifier.h"
#include "AnimationModifierDetailCustomization.h"
#include "AnimationModifierHelpers.h"
#include "AnimationModifierSettings.h"
#include "AnimationModifiersAssetUserData.h"
#include "AnimationModifiersTabSummoner.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "Internationalization/Internationalization.h"
#include "Math/Vector2D.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "SAnimationModifierContentBrowserWindow.h"
#include "ScopedTransaction.h"
#include "Subsystems/ImportSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

class UFactory;

#define LOCTEXT_NAMESPACE "AnimationModifiersModule"

IMPLEMENT_MODULE(FAnimationModifiersModule, AnimationModifiers);

void FAnimationModifiersModule::StartupModule()
{
	// Register class/struct customizations
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout("AnimationModifier", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimationModifierDetailCustomization::MakeInstance));
	
	// Add application mode extender
	Extender = FWorkflowApplicationModeExtender::CreateRaw(this, &FAnimationModifiersModule::ExtendApplicationMode);
	FWorkflowCentricApplication::GetModeExtenderList().Add(Extender);

	// Register delegates during PostEngineInit as this module is part of preload phase and GEditor is not valid yet
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		if (GEditor)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddRaw(this, &FAnimationModifiersModule::OnAssetPostImport);
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetReimport.AddRaw(this, &FAnimationModifiersModule::OnAssetPostReimport);
		}
	});
}

TSharedRef<FApplicationMode> FAnimationModifiersModule::ExtendApplicationMode(const FName ModeName, TSharedRef<FApplicationMode> InMode)
{
	// For skeleton and animation editor modes add our custom tab factory to it
	if (ModeName == TEXT("SkeletonEditorMode") || ModeName == TEXT("AnimationEditorMode"))
	{
		InMode->AddTabFactory(FCreateWorkflowTabFactory::CreateStatic(&FAnimationModifiersTabSummoner::CreateFactory));
		RegisteredApplicationModes.Add(InMode);
	}
	
	return InMode;
}

void FAnimationModifiersModule::OnAssetPostImport(UFactory* ImportFactory, UObject* ImportedObject)
{
	// Check whether or not the imported asset is a AnimSequence
	if (UAnimSequence* AnimationSequence = Cast<UAnimSequence>(ImportedObject))
	{
		// Check whether or not there are any default modifiers which should be added to the new sequence
		const TArray<TSubclassOf<UAnimationModifier>>& DefaultModifiers = GetDefault<UAnimationModifierSettings>()->DefaultAnimationModifiers;
		if (DefaultModifiers.Num())
		{
			UAnimationModifiersAssetUserData* AssetUserData = FAnimationModifierHelpers::RetrieveOrCreateModifierUserData(AnimationSequence);			
			for (TSubclassOf<UAnimationModifier> ModifierClass : DefaultModifiers)
			{
				if (ModifierClass.Get())
				{
					UObject* Outer = AssetUserData;
					UAnimationModifier* Processor = FAnimationModifierHelpers::CreateModifierInstance(Outer, *ModifierClass);
					AssetUserData->Modify();
					AssetUserData->AddAnimationModifier(Processor);
				}
			}

			if (GetDefault<UAnimationModifierSettings>()->bApplyAnimationModifiersOnImport)
			{
				ApplyAnimationModifiers({AnimationSequence});
			}
		}
	}
}

void FAnimationModifiersModule::OnAssetPostReimport(UObject* ReimportedObject)
{
	// Check whether or not the reimported asset is a AnimSequence
	if (UAnimSequence* AnimationSequence = Cast<UAnimSequence>(ReimportedObject))
	{
		// Check whether or not any contained modifiers should be applied 
		if (GetDefault<UAnimationModifierSettings>()->bApplyAnimationModifiersOnImport)
		{			
			ApplyAnimationModifiers({AnimationSequence});
		}
	}
}

void FAnimationModifiersModule::ShutdownModule()
{
	// Make sure we unregister the class layout 
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomClassLayout("AnimationModifier");
	}

	// Remove extender delegate
	FWorkflowCentricApplication::GetModeExtenderList().RemoveAll([this](FWorkflowApplicationModeExtender& StoredExtender) { return StoredExtender.GetHandle() == Extender.GetHandle(); });

	// During shutdown clean up all factories from any modes which are still active/alive
	for (TWeakPtr<FApplicationMode> WeakMode : RegisteredApplicationModes)
	{
		if (WeakMode.IsValid())
		{
			TSharedPtr<FApplicationMode> Mode = WeakMode.Pin();
			Mode->RemoveTabFactory(FAnimationModifiersTabSummoner::AnimationModifiersName);
		}
	}

	RegisteredApplicationModes.Empty();

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
	}
}

void FAnimationModifiersModule::ShowAddAnimationModifierWindow(const TArray<UAnimSequence*>& InSequences)
{
	TSharedPtr<SAnimationModifierContentBrowserWindow> WindowContent;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Add Animation Modifier(s)"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500, 500));

	Window->SetContent
	(
		SAssignNew(WindowContent, SAnimationModifierContentBrowserWindow)
		.WidgetWindow(Window)
		.AnimSequences(InSequences)
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

void FAnimationModifiersModule::ApplyAnimationModifiers(const TArray<UAnimSequence*>& InSequences, bool bForceApply /*= true*/)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_ApplyModifiers", "Applying Animation Modifier(s) to Animation Sequence(s)"));
	
	// Iterate over each Animation Sequence and all of its contained modifiers, applying each one
	UE::Anim::FApplyModifiersScope Scope;
	TArray<UAnimationModifiersAssetUserData*> AssetUserData;
	for (UAnimSequence* AnimationSequence : InSequences)
	{
		if (AnimationSequence)
		{
			UAnimationModifiersAssetUserData* UserData = AnimationSequence->GetAssetUserData<UAnimationModifiersAssetUserData>();
			if (UserData)
			{
				AnimationSequence->Modify();
				const TArray<UAnimationModifier*>& ModifierInstances = UserData->GetAnimationModifierInstances();
				for (UAnimationModifier* Modifier : ModifierInstances)
				{
					if (bForceApply || !Modifier->IsLatestRevisionApplied())
					{
						Modifier->ApplyToAnimationSequence(AnimationSequence);
					}
				}
			}
		}		
	}
}

#undef LOCTEXT_NAMESPACE // "AnimationModifiersModule"
