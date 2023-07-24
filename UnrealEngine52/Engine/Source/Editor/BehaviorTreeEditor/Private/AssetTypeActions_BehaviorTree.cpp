// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_BehaviorTree.h"

#include "AIModule.h"
#include "AssetToolsModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeEditor.h"
#include "BehaviorTreeEditorModule.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMath.h"
#include "IAssetTools.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "SBehaviorTreeDiff.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"

class IBehaviorTreeEditor;
class IToolkitHost;
class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

uint32 FAssetTypeActions_BehaviorTree::GetCategories() 
{ 
	IAIModule& AIModule = FModuleManager::LoadModuleChecked<IAIModule>("AIModule").Get();
	return AIModule.GetAIAssetCategoryBit();
}

void FAssetTypeActions_BehaviorTree::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for(auto Object : InObjects)
	{
		auto BehaviorTree = Cast<UBehaviorTree>(Object);
		if(BehaviorTree != nullptr)
		{
			// check if we have an editor open for this BT's blackboard & use that if we can
			bool bFoundExisting = false;
			if(BehaviorTree->BlackboardAsset != nullptr)
			{
				const bool bFocusIfOpen = false;
				FBehaviorTreeEditor* ExistingInstance = static_cast<FBehaviorTreeEditor*>(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(BehaviorTree->BlackboardAsset, bFocusIfOpen));
				if(ExistingInstance != nullptr && ExistingInstance->GetBehaviorTree() == nullptr)
				{
					ExistingInstance->InitBehaviorTreeEditor(Mode, EditWithinLevelEditor, BehaviorTree);
					bFoundExisting = true;
				}
			}
			
			if(!bFoundExisting)
			{
				FBehaviorTreeEditorModule& BehaviorTreeEditorModule = FModuleManager::GetModuleChecked<FBehaviorTreeEditorModule>( "BehaviorTreeEditor" );
				TSharedRef< IBehaviorTreeEditor > NewEditor = BehaviorTreeEditorModule.CreateBehaviorTreeEditor( Mode, EditWithinLevelEditor, BehaviorTree );	
			}
		}
	}
}

UClass* FAssetTypeActions_BehaviorTree::GetSupportedClass() const
{ 
	return UBehaviorTree::StaticClass(); 
}

void FAssetTypeActions_BehaviorTree::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	UBehaviorTree* OldBehaviorTree = Cast<UBehaviorTree>(OldAsset);
	UBehaviorTree* NewBehaviorTree = Cast<UBehaviorTree>(NewAsset);
	check(NewBehaviorTree != nullptr || OldBehaviorTree != nullptr);

	// sometimes we're comparing different revisions of one single asset (other 
	// times we're comparing two completely separate assets altogether)
	const bool bIsSingleAsset = !NewBehaviorTree || !OldBehaviorTree || (NewBehaviorTree->GetName() == OldBehaviorTree->GetName());

	FText WindowTitle = LOCTEXT("NamelessBehaviorTreeDiff", "Behavior Tree Diff");
	// if we're diffing one asset against itself 
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		FString TreeName;
		if (NewBehaviorTree)
		{
			TreeName = NewBehaviorTree->GetName();
		}
		else if (OldBehaviorTree)
		{
			TreeName = OldBehaviorTree->GetName();
		}
		WindowTitle = FText::Format(LOCTEXT("Behavior Tree Diff", "{0} - Behavior Tree Diff"), FText::FromString(TreeName));
	}

	const TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(FVector2f(1000.f,800.f));

	Window->SetContent(SNew(SBehaviorTreeDiff)
		.BehaviorTreeOld(OldBehaviorTree)
		.BehaviorTreeNew(NewBehaviorTree)
		.OldRevision(OldRevision)
		.NewRevision(NewRevision)
		.ShowAssetNames(!bIsSingleAsset)
		.OpenInDefaults(const_cast<FAssetTypeActions_BehaviorTree*>(this), &FAssetTypeActions_BehaviorTree::OpenInDefaults) );

	// Make this window a child of the modal window if we've been spawned while one is active.
	TSharedPtr<SWindow> ActiveModal = FSlateApplication::Get().GetActiveModalWindow();
	if ( ActiveModal.IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild( Window.ToSharedRef(), ActiveModal.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( Window.ToSharedRef() );
	}
}

void FAssetTypeActions_BehaviorTree::OpenInDefaults( class UBehaviorTree* OldBehaviorTree, class UBehaviorTree* NewBehaviorTree ) const
{
	FString OldTextFilename = DumpAssetToTempFile(OldBehaviorTree);
	FString NewTextFilename = DumpAssetToTempFile(NewBehaviorTree);

	// Get diff program to use
	FString DiffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateDiffProcess(DiffCommand, OldTextFilename, NewTextFilename);
}

#undef LOCTEXT_NAMESPACE
