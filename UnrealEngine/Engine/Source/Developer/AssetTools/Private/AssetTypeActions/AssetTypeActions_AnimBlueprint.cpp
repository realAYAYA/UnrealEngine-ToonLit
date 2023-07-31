// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_AnimBlueprint.h"
#include "ToolMenus.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "Animation/AnimInstance.h"
#include "Factories/AnimBlueprintFactory.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "AssetTools.h"
#include "ContentBrowserModule.h"
#include "PersonaModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SBlueprintDiff.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SSkeletonWidget.h"
#include "Styling/SlateIconFinder.h"
#include "IAnimationBlueprintEditorModule.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Preferences/PersonaOptions.h"
#if WITH_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_AnimBlueprint::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UAnimBlueprint>> AnimBlueprints = GetTypedWeakObjectPtrs<UAnimBlueprint>(InObjects);

	if (AnimBlueprints.Num() == 1 && CanCreateNewDerivedBlueprint())
	{
		UAnimBlueprint* AnimBlueprint = AnimBlueprints[0].Get();

		if(AnimBlueprint && AnimBlueprint->BlueprintType != BPTYPE_Interface)
		{
			// Call base class for the regular 'create child blueprint' option
			FAssetTypeActions_Blueprint::GetActions(InObjects, Section);
		}
		
		// Accept (non-interface) template anim BPs or anim BPs with compatible skeletons
		if(AnimBlueprint && AnimBlueprint->BlueprintType != BPTYPE_Interface && ((AnimBlueprint->TargetSkeleton == nullptr && AnimBlueprint->bIsTemplate) || (AnimBlueprint->TargetSkeleton != nullptr && AnimBlueprint->TargetSkeleton->GetCompatibleSkeletons().Num() > 0)))
		{
			Section.AddSubMenu(
				"AnimBlueprint_NewSkeletonChildBlueprint",
				LOCTEXT("AnimBlueprint_NewSkeletonChildBlueprint", "Create Child Anim Blueprint with Skeleton"),
				LOCTEXT("AnimBlueprint_NewSkeletonChildBlueprint_Tooltip", "Create a child Anim Blueprint that uses a different compatible skeleton"),
				FNewToolMenuDelegate::CreateLambda([this, WeakAnimBlueprint = TWeakObjectPtr<UAnimBlueprint>(AnimBlueprint)](UToolMenu* CompatibleSkeletonMenu)
				{
					auto HandleAssetSelected = [this, WeakAnimBlueprint](const FAssetData& InAssetData)
					{
						FSlateApplication::Get().DismissAllMenus();
						
						if(UAnimBlueprint* TargetParentBP = WeakAnimBlueprint.Get())
						{
							USkeleton* TargetSkeleton = CastChecked<USkeleton>(InAssetData.GetAsset());
							UClass* TargetParentClass = TargetParentBP->GeneratedClass;

							if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(TargetParentClass))
							{
								FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("InvalidClassToMakeBlueprintFrom", "Invalid class with which to make a Blueprint."));
								return;
							}

							FString Name;
							FString PackageName;
							CreateUniqueAssetName(TargetParentBP->GetOutermost()->GetName(), TEXT("_Child"), PackageName, Name);
							const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

							UAnimBlueprintFactory* AnimBlueprintFactory = NewObject<UAnimBlueprintFactory>();
							AnimBlueprintFactory->ParentClass = TSubclassOf<UAnimInstance>(*TargetParentBP->GeneratedClass);
							AnimBlueprintFactory->TargetSkeleton = TargetSkeleton;
							AnimBlueprintFactory->bTemplate = false;

							FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
							ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, TargetParentBP->GetClass(), AnimBlueprintFactory);
						}
					};

					FAssetPickerConfig AssetPickerConfig;
					AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda([HandleAssetSelected](const TArray<FAssetData>& SelectedAssetData)
					{
						if (SelectedAssetData.Num() == 1)
						{
							HandleAssetSelected(SelectedAssetData[0]);
						}
					});
					AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(HandleAssetSelected);
					AssetPickerConfig.bAllowNullSelection = false;
					AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
					AssetPickerConfig.Filter.bRecursiveClasses = false;
					AssetPickerConfig.Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
					AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([WeakAnimBlueprint](const FAssetData& AssetData)
					{
						if(UAnimBlueprint* LocalAnimBlueprint = WeakAnimBlueprint.Get())
						{
							if(LocalAnimBlueprint->TargetSkeleton == nullptr && LocalAnimBlueprint->bIsTemplate)
							{
								// Template anim BP - do not filter
								return false;
							}
							else if(LocalAnimBlueprint->TargetSkeleton != nullptr)
							{
								// Filter on compatible skeletons
								const FString ExportTextName = AssetData.GetExportTextName();
								return !LocalAnimBlueprint->TargetSkeleton->IsCompatibleSkeletonByAssetString(ExportTextName);
							}
						}
						
						return true;
					});
					
					FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

					FToolMenuSection& InSection = CompatibleSkeletonMenu->AddSection("CompatibleSkeletonMenu", LOCTEXT("CompatibleSkeletonHeader", "Compatible Skeletons"));
					InSection.AddEntry(
						FToolMenuEntry::InitWidget("CompatibleSkeletonPicker",
							SNew(SBox)
							.WidthOverride(300.0f)
							.HeightOverride(300.0f)
							[
								ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
							],
							FText::GetEmpty())
					);
				}),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Blueprint"));
		}
	}

	if(AreOnlyNonTemplateAnimBlueprintsSelected(AnimBlueprints) && AreOnlyNonInterfaceAnimBlueprintsSelected(AnimBlueprints))
	{
		Section.AddMenuEntry(
			"AnimBlueprint_FindSkeleton",
			LOCTEXT("AnimBlueprint_FindSkeleton", "Find Skeleton"),
			LOCTEXT("AnimBlueprint_FindSkeletonTooltip", "Finds the skeleton used by the selected Anim Blueprints in the content browser."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Find"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton, AnimBlueprints ),
				FCanExecuteAction()
				)
			);

		Section.AddMenuEntry(
			"AnimBlueprint_AssignSkeleton",
			LOCTEXT("AnimBlueprint_AssignSkeleton", "Assign Skeleton"),
			LOCTEXT("AnimBlueprint_AssignSkeletonTooltip", "Assigns a skeleton to the selected Animation Blueprint(s)."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.AssignSkeleton"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_AnimBlueprint::ExecuteAssignSkeleton, AnimBlueprints),
				FCanExecuteAction()
			)
		);

	}
}

UThumbnailInfo* FAssetTypeActions_AnimBlueprint::GetThumbnailInfo(UObject* Asset) const
{
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(Asset);
	UThumbnailInfo* ThumbnailInfo = AnimBlueprint->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(AnimBlueprint, NAME_None, RF_Transactional);
		AnimBlueprint->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

UFactory* FAssetTypeActions_AnimBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(InBlueprint);

	if(InBlueprint->BlueprintType == BPTYPE_Interface)
	{
		return NewObject<UAnimLayerInterfaceFactory>();
	}
	else
	{
		UAnimBlueprintFactory* AnimBlueprintFactory = NewObject<UAnimBlueprintFactory>();
		AnimBlueprintFactory->ParentClass = TSubclassOf<UAnimInstance>(*InBlueprint->GeneratedClass);
		AnimBlueprintFactory->TargetSkeleton = AnimBlueprint->TargetSkeleton;
		AnimBlueprintFactory->bTemplate = AnimBlueprint->bIsTemplate;
		return AnimBlueprintFactory;
	}
}

void FAssetTypeActions_AnimBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto AnimBlueprint = Cast<UAnimBlueprint>(*ObjIt);
		if (AnimBlueprint != NULL && AnimBlueprint->SkeletonGeneratedClass && AnimBlueprint->GeneratedClass)
		{
			if(AnimBlueprint->BlueprintType != BPTYPE_Interface && !AnimBlueprint->TargetSkeleton && !AnimBlueprint->bIsTemplate)
			{
				FText ShouldRetargetMessage = LOCTEXT("ShouldRetarget_Message", "Could not find the skeleton for Anim Blueprint '{BlueprintName}' Would you like to choose a new one?");
				
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("BlueprintName"), FText::FromString(AnimBlueprint->GetName()));

				if (FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldRetargetMessage, Arguments)) == EAppReturnType::Yes)
				{
					TArray<TObjectPtr<UObject>> AssetsToRetarget;
					AssetsToRetarget.Add(AnimBlueprint);
					const bool bSkeletonReplaced = ReplaceMissingSkeleton(AssetsToRetarget);
					if (!bSkeletonReplaced)
					{
						return; // Persona will crash if trying to load asset without a skeleton
					}
				}
				else
				{
					return;
				}
			}
			const bool bBringToFrontIfOpen = true;
#if WITH_EDITOR
			if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(AnimBlueprint, bBringToFrontIfOpen))
			{
				EditorInstance->FocusWindow(AnimBlueprint);
			}
			else
#endif
			{
				IAnimationBlueprintEditorModule& AnimationBlueprintEditorModule = FModuleManager::LoadModuleChecked<IAnimationBlueprintEditorModule>("AnimationBlueprintEditor");
				AnimationBlueprintEditorModule.CreateAnimationBlueprintEditor(Mode, EditWithinLevelEditor, AnimBlueprint);
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadCorruptAnimBlueprint", "The Anim Blueprint could not be loaded because it is corrupt."));
		}
	}
}

void FAssetTypeActions_AnimBlueprint::PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = CastChecked<UBlueprint>(Asset1);
	UBlueprint* NewBlueprint = CastChecked<UBlueprint>(Asset2);

	// sometimes we're comparing different revisions of one single asset (other 
	// times we're comparing two completely separate assets altogether)
	bool bIsSingleAsset = (NewBlueprint->GetName() == OldBlueprint->GetName());

	FText WindowTitle = LOCTEXT("NamelessAnimationBlueprintDiff", "Animation Blueprint Diff");
	// if we're diffing one asset against itself 
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		WindowTitle = FText::Format(LOCTEXT("AnimationBlueprintDiff", "{0} - Animation Blueprint Diff"), FText::FromString(NewBlueprint->GetName()));
	}

	SBlueprintDiff::CreateDiffWindow(WindowTitle, OldBlueprint, NewBlueprint, OldRevision, NewRevision);
}

bool FAssetTypeActions_AnimBlueprint::AreOnlyNonTemplateAnimBlueprintsSelected(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects) const
{
	for(TWeakObjectPtr<UAnimBlueprint> WeakAnimBlueprint : Objects)
	{
		if(WeakAnimBlueprint->bIsTemplate)
		{
			return false; 
		}
	}

	return true;
}

bool FAssetTypeActions_AnimBlueprint::AreOnlyNonInterfaceAnimBlueprintsSelected(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects) const
{
	for(TWeakObjectPtr<UAnimBlueprint> WeakAnimBlueprint : Objects)
	{
		if(WeakAnimBlueprint->BlueprintType == BPTYPE_Interface)
		{
			return false; 
		}
	}

	return true;
}

void FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->TargetSkeleton;
			if (Skeleton)
			{
				ObjectsToSync.AddUnique(Skeleton);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
	else
	{
		FText ShouldRetargetMessage = LOCTEXT("NoSkeletonFound", "Could not find the skeleton");
		FMessageDialog::Open(EAppMsgType::Ok, ShouldRetargetMessage);
	}
}

void FAssetTypeActions_AnimBlueprint::ExecuteAssignSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects)
{
	if (Objects.Num() > 0)
	{
		TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
			.Title(LOCTEXT("ChooseSkeletonWindowTitle", "Choose Skeleton"))
			.ClientSize(FVector2D(400, 600));

		TSharedPtr<SSkeletonSelectorWindow> SkeletonSelectorWindow;
		WidgetWindow->SetContent
		(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(SkeletonSelectorWindow, SSkeletonSelectorWindow)
				.Object(Objects[0].Get())
			.WidgetWindow(WidgetWindow)
			]
		);

		GEditor->EditorAddModalWindow(WidgetWindow);
		USkeleton* SelectedSkeleton = SkeletonSelectorWindow->GetSelectedSkeleton();

		// only do this if not same
		if (SelectedSkeleton)
		{
			for (TWeakObjectPtr<UAnimBlueprint>& AnimBP : Objects)
			{
				AnimBP->TargetSkeleton = SelectedSkeleton;
			}
		}
	}
}

bool FAssetTypeActions_AnimBlueprint::ReplaceMissingSkeleton(TArray<UObject*> InAnimBlueprints) const
{
	// record anim assets that need skeleton replaced
	const TArray<TWeakObjectPtr<UObject>> ABPsToFix = GetTypedWeakObjectPtrs<UObject>(InAnimBlueprints);
	// get a skeleton from the user and replace it
	const TSharedPtr<SReplaceMissingSkeletonDialog> PickSkeletonWindow = SNew(SReplaceMissingSkeletonDialog).AnimAssets(ABPsToFix);
	const bool bWasSkeletonReplaced = PickSkeletonWindow.Get()->ShowModal();
	return bWasSkeletonReplaced;
}

TSharedPtr<SWidget> FAssetTypeActions_AnimBlueprint::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(UAnimBlueprint::StaticClass());

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetNoBrush())
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SImage)
			.Image(Icon)
		];
}

FText FAssetTypeActions_AnimBlueprint::GetDisplayNameFromAssetData(const FAssetData& AssetData) const
{
	FString BlueprintTypeValue;
	AssetData.GetTagValue("BlueprintType", BlueprintTypeValue);
	if(BlueprintTypeValue.Equals(TEXT("BPTYPE_Normal")))
	{
		return LOCTEXT("AssetTypeActions_AnimBlueprint", "Animation Blueprint");
	}
	else if(BlueprintTypeValue.Equals(TEXT("BPTYPE_Interface")))
	{
		return LOCTEXT("AssetTypeActions_AnimLayerInterface", "Animation Layer Interface");
	}

	return GetName();
}

#undef LOCTEXT_NAMESPACE
