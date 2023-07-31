// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_ViewModelBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SBlueprintDiff.h"

#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace UE::MVVM
{

void FAssetTypeActions_ViewModelBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(*ObjIt);
		if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass )
		{
			TSharedRef< FMVVMViewModelBlueprintEditor > NewBlueprintEditor(new FMVVMViewModelBlueprintEditor());

			bool IsDataOnlyBlueprint = FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
				&& !Blueprint->bForceFullEditor
				&& !Blueprint->bIsNewlyCreated;
			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Blueprint);
			NewBlueprintEditor->InitViewModelBlueprintEditor(Mode, EditWithinLevelEditor, Blueprints, IsDataOnlyBlueprint);
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadViewModelBlueprint", "Viewmodel Blueprint could not be loaded because it derives from an invalid class.\nCheck to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}


UClass* FAssetTypeActions_ViewModelBlueprint::GetSupportedClass() const
{
	return UMVVMViewModelBlueprint::StaticClass();
}


FText FAssetTypeActions_ViewModelBlueprint::GetAssetDescription( const FAssetData& AssetData ) const
{
	FString Description = AssetData.GetTagValueRef<FString>( GET_MEMBER_NAME_CHECKED( UBlueprint, BlueprintDescription ) );
	if ( !Description.IsEmpty() )
	{
		Description.ReplaceInline( TEXT( "\\n" ), TEXT( "\n" ) );
		return FText::FromString( MoveTemp(Description) );
	}

	return FText::GetEmpty();
}


void FAssetTypeActions_ViewModelBlueprint::PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = CastChecked<UBlueprint>(Asset1);
	UBlueprint* NewBlueprint = CastChecked<UBlueprint>(Asset2);

	// sometimes we're comparing different revisions of one single asset (other 
	//times we're comparing two completely separate assets altogether)
	bool bIsSingleAsset = (NewBlueprint->GetName() == OldBlueprint->GetName());

	FText WindowTitle = LOCTEXT("NamelessViewmodelBlueprintDiff", "Viewmodel Blueprint Diff");
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		WindowTitle = FText::Format(LOCTEXT("WidgetBlueprintDiff", "{0} - Viewmodel Blueprint Diff"), FText::FromString(NewBlueprint->GetName()));
	}

	SBlueprintDiff::CreateDiffWindow(WindowTitle, OldBlueprint, NewBlueprint, OldRevision, NewRevision);
}

} //namespace

#undef LOCTEXT_NAMESPACE
