// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintActions.h"
#include "ControlRigBlueprintFactory.h"
#include "ControlRigBlueprint.h"
#include "ControlRig.h"
#include "Editor/RigVMEditorStyle.h"
#include "IControlRigEditorModule.h"

#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h" 
#include "Subsystems/AssetEditorSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "EditorDirectories.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ControlRigObjectBinding.h"
#include "EditMode/ControlRigEditMode.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "MovieSceneToolsProjectSettings.h"
#include "SBlueprintDiff.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Sequencer/ControlRigParameterTrackEditor.h"
#include "Rigs/RigHierarchyController.h"
#include "ModularRig.h"

#define LOCTEXT_NAMESPACE "ControlRigBlueprintActions"

FDelegateHandle FControlRigBlueprintActions::OnSpawnedSkeletalMeshActorChangedHandle;

UFactory* FControlRigBlueprintActions::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UControlRigBlueprintFactory* ControlRigBlueprintFactory = NewObject<UControlRigBlueprintFactory>();
	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(InBlueprint);
	ControlRigBlueprintFactory->ParentClass = TSubclassOf<UControlRig>(*InBlueprint->GeneratedClass);
	return ControlRigBlueprintFactory;
}

void FControlRigBlueprintActions::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(*ObjIt))
		{
			const bool bBringToFrontIfOpen = true;
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(ControlRigBlueprint, bBringToFrontIfOpen))
			{
				EditorInstance->FocusWindow(ControlRigBlueprint);
			}
			else
			{				
				// If any other editors are opened (for example, a BlueprintDiff window), close them 
				AssetEditorSubsystem->CloseAllEditorsForAsset(ControlRigBlueprint);				
				
				IControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<IControlRigEditorModule>("ControlRigEditor");
				ControlRigEditorModule.CreateControlRigEditor(Mode, EditWithinLevelEditor, ControlRigBlueprint);
			}
		}
	}
}

TSharedPtr<SWidget> FControlRigBlueprintActions::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(UControlRigBlueprint::StaticClass());

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

void FControlRigBlueprintActions::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = Cast<UBlueprint>(OldAsset);
	UBlueprint* NewBlueprint = Cast<UBlueprint>(NewAsset);

	static const FText DiffWindowMessage = LOCTEXT("ControlRigDiffWindow", "Opening a diff window will close the control rig editor. {0}.\nAre you sure?");
	
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (OldAsset)
	{
		for (IAssetEditorInstance* Editor : AssetEditorSubsystem->FindEditorsForAsset(OldAsset))
		{
			const EAppReturnType::Type Answer = FMessageDialog::Open( EAppMsgType::YesNo,
					FText::Format(DiffWindowMessage, FText::FromString(OldBlueprint->GetName())));
			if(Answer == EAppReturnType::No)
			{
			   return;
			}
		}
	}
	if (NewAsset)
	{
		for (IAssetEditorInstance* Editor : AssetEditorSubsystem->FindEditorsForAsset(NewAsset))
		{
			const EAppReturnType::Type Answer = FMessageDialog::Open( EAppMsgType::YesNo,
					FText::Format(DiffWindowMessage, FText::FromString(NewBlueprint->GetName())));
			if(Answer == EAppReturnType::No)
			{
				return;
			}
		}
	}

	if (OldAsset)
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(OldAsset);
	}
	
	if (NewAsset)
	{
		AssetEditorSubsystem->CloseAllEditorsForAsset(NewAsset);
	}

	SBlueprintDiff::CreateDiffWindow(OldBlueprint, NewBlueprint, OldRevision, NewRevision, GetSupportedClass());
}

void FControlRigBlueprintActions::ExtendSketalMeshToolMenu()
{
	TArray<UToolMenu*> MenusToExtend;
	MenusToExtend.Add(UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SkeletalMesh.CreateSkeletalMeshSubmenu"));
	MenusToExtend.Add(UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.Skeleton.CreateSkeletalMeshSubmenu"));

	for(UToolMenu* Menu : MenusToExtend)
	{
		if (Menu == nullptr)
		{
			continue;
		}

		FToolMenuSection& Section = Menu->AddSection("ControlRig", LOCTEXT("ControlRigSectionName", "Control Rig"));
		Section.AddDynamicEntry("CreateControlRig", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (Context)
			{
				TArray<UObject*> SelectedObjects = Context->GetSelectedObjects();
				if (SelectedObjects.Num() > 0)
				{
					static constexpr bool bModularRig = true;
					InSection.AddMenuEntry(
						"CreateControlRig",
						LOCTEXT("CreateControlRig", "Control Rig"),
						LOCTEXT("CreateControlRig_ToolTip", "Creates a control rig and preconfigures it for this asset"),
						FSlateIcon(FRigVMEditorStyle::Get().GetStyleSetName(), "RigVM", "RigVM.Unit"),
						FExecuteAction::CreateLambda([SelectedObjects]()
						{
							for (UObject* SelectedObject : SelectedObjects)
							{
								CreateControlRigFromSkeletalMeshOrSkeleton(SelectedObject, !bModularRig);
							}
						})
					);
					InSection.AddMenuEntry(
						"CreateModularRig",
						LOCTEXT("CreateModularRig", "Modular Rig"),
						LOCTEXT("CreateModularRig_ToolTip", "Creates a modular rig and preconfigures it for this asset"),
						FSlateIcon(FRigVMEditorStyle::Get().GetStyleSetName(), "RigVM", "RigVM.Unit"),
						FExecuteAction::CreateLambda([SelectedObjects]()
						{
							for (UObject* SelectedObject : SelectedObjects)
							{
								CreateControlRigFromSkeletalMeshOrSkeleton(SelectedObject, bModularRig);
							}
						})
					);
				}
			}
		}));
	}
}

UControlRigBlueprint* FControlRigBlueprintActions::CreateNewControlRigAsset(const FString& InDesiredPackagePath, const bool bModularRig)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UControlRigBlueprintFactory* Factory = NewObject<UControlRigBlueprintFactory>();
	Factory->ParentClass = bModularRig ? UModularRig::StaticClass() : UControlRig::StaticClass();

	FString UniquePackageName;
	FString UniqueAssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(InDesiredPackagePath, TEXT(""), UniquePackageName, UniqueAssetName);

	if (UniquePackageName.EndsWith(UniqueAssetName))
	{
		UniquePackageName = UniquePackageName.LeftChop(UniqueAssetName.Len() + 1);
	}

	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(*UniqueAssetName, *UniquePackageName, nullptr, Factory);
	return Cast<UControlRigBlueprint>(NewAsset);
}

UControlRigBlueprint* FControlRigBlueprintActions::CreateControlRigFromSkeletalMeshOrSkeleton(UObject* InSelectedObject, const bool bModularRig)
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(InSelectedObject);
	USkeleton* Skeleton = Cast<USkeleton>(InSelectedObject);
	const FReferenceSkeleton* RefSkeleton = nullptr;

	if(SkeletalMesh)
	{
		Skeleton = SkeletalMesh->GetSkeleton();
		RefSkeleton = &SkeletalMesh->GetRefSkeleton();
	}
	else if (Skeleton)
	{
		RefSkeleton = &Skeleton->GetReferenceSkeleton();
	}
	else
	{
		UE_LOG(LogControlRigEditor, Error, TEXT("CreateControlRigFromSkeletalMeshOrSkeleton: Provided object has to be a SkeletalMesh or Skeleton."));
		return nullptr;
	}

	check(RefSkeleton);

	FString PackagePath = InSelectedObject->GetPathName();
	FString ControlRigName = FString::Printf(TEXT("%s_CtrlRig"), *InSelectedObject->GetName());

	int32 LastSlashPos = INDEX_NONE;
	if (PackagePath.FindLastChar('/', LastSlashPos))
	{
		PackagePath = PackagePath.Left(LastSlashPos);
	}

	UControlRigBlueprint* NewControlRigBlueprint = CreateNewControlRigAsset(PackagePath / ControlRigName, bModularRig);
	if (NewControlRigBlueprint == nullptr)
	{
		return nullptr;
	}

	if(URigHierarchyController* Controller = NewControlRigBlueprint->GetHierarchyController())
	{
		Controller->ImportBones(*RefSkeleton, NAME_None, false, false, false, false);
		Controller->ImportCurves(Skeleton, NAME_None, false, false);
	}
	NewControlRigBlueprint->SourceHierarchyImport = Skeleton;
	NewControlRigBlueprint->SourceCurveImport = Skeleton;
	NewControlRigBlueprint->PropagateHierarchyFromBPToInstances();

	if(SkeletalMesh)
	{
		NewControlRigBlueprint->SetPreviewMesh(SkeletalMesh);
	}

	if(!bModularRig)
	{
		NewControlRigBlueprint->RecompileVM();
	}

	return NewControlRigBlueprint;
}

USkeletalMesh* FControlRigBlueprintActions::GetSkeletalMeshFromControlRigBlueprint(const FAssetData& InAsset)
{
	if (InAsset.GetClass() != UControlRigBlueprint::StaticClass())
	{
		return nullptr;
	}
	
	if (const UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(InAsset.GetAsset()))
	{
		return Blueprint->GetPreviewMesh();
	}
	return nullptr;
}

void FControlRigBlueprintActions::PostSpawningSkeletalMeshActor(AActor* InSpawnedActor, UObject* InAsset)
{
	if (InSpawnedActor->HasAnyFlags(RF_Transient) || InSpawnedActor->bIsEditorPreviewActor)
	{
		return;
	}
	
	OnSpawnedSkeletalMeshActorChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&FControlRigBlueprintActions::OnSpawnedSkeletalMeshActorChanged, InAsset);
}

void FControlRigBlueprintActions::OnSpawnedSkeletalMeshActorChanged(UObject* InObject, FPropertyChangedEvent& InEvent, UObject* InAsset)
{
	if (!OnSpawnedSkeletalMeshActorChangedHandle.IsValid())
	{
		return;
	}

	// we are waiting for the top level property change event
	// after the spawn.
	if (InEvent.Property != nullptr)
	{
		return;
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnSpawnedSkeletalMeshActorChangedHandle);
	OnSpawnedSkeletalMeshActorChangedHandle.Reset();

	// Create a level sequence but delay until next tick so that the creation of the asset is not in the existing transaction
	GEditor->GetTimerManager()->SetTimerForNextTick([InObject, InAsset]()
	{
		ASkeletalMeshActor* MeshActor = Cast<ASkeletalMeshActor>(InObject);
		check(MeshActor);
		UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(InAsset);
		if (RigBlueprint == nullptr)
		{
			return;
		}

		UClass* ControlRigClass = RigBlueprint->GeneratedClass;

		TGuardValue<bool> DisableTrackCreation(FControlRigParameterTrackEditor::bAutoGenerateControlRigTrack, false);

		// find a level sequence in the world, if can't find that, create one
		ULevelSequence* Sequence = ULevelSequenceEditorBlueprintLibrary::GetFocusedLevelSequence();
		if (Sequence == nullptr)
		{
			FString SequenceName = FString::Printf(TEXT("%s_Take1"), *InAsset->GetName());

			FString PackagePath;
			const FString DefaultDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
			FPackageName::TryConvertFilenameToLongPackageName(DefaultDirectory, PackagePath);
			if (PackagePath.IsEmpty())
			{
				PackagePath = TEXT("/Game");
			}

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			FString UniquePackageName;
			FString UniqueAssetName;
			AssetToolsModule.Get().CreateUniqueAssetName(PackagePath / SequenceName, TEXT(""), UniquePackageName, UniqueAssetName);

			UPackage* Package = CreatePackage(*UniquePackageName);
			Sequence = NewObject<ULevelSequence>(Package, *UniqueAssetName, RF_Public | RF_Standalone);
			Sequence->Initialize(); //creates movie scene
			Sequence->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Sequence);

			// Set up some sensible defaults
			const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
			FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
			Sequence->GetMovieScene()->SetPlaybackRange((ProjectSettings->DefaultStartTime * TickResolution).FloorToFrame(), (ProjectSettings->DefaultDuration * TickResolution).FloorToFrame().Value);

			if (UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass()))
			{
				if (ALevelSequenceActor* LevelSequenceActor = Cast<ALevelSequenceActor>(GEditor->UseActorFactory(ActorFactory, FAssetData(Sequence), &FTransform::Identity)))
				{
					LevelSequenceActor->SetSequence(Sequence);
				}
			}
		}

		if (Sequence == nullptr)
		{
			return;
		}

		UMovieScene* MovieScene = Sequence->GetMovieScene();

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);

		IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Sequence, false);
		ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
		TWeakPtr<ISequencer> WeakSequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;

		if (WeakSequencer.IsValid())
		{
			TArray<TWeakObjectPtr<AActor> > ActorsToAdd;
			ActorsToAdd.Add(MeshActor);
			TArray<FGuid> ActorTracks = WeakSequencer.Pin()->AddActors(ActorsToAdd, false);
			FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));

			for (FGuid ActorTrackGuid : ActorTracks)
			{
				//Delete binding from default animating rig
				FGuid CompGuid = WeakSequencer.Pin()->FindObjectId(*(MeshActor->GetSkeletalMeshComponent()), WeakSequencer.Pin()->GetFocusedTemplateID());
				if (CompGuid.IsValid())
				{
					if (ControlRigEditMode)
					{
						UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), CompGuid, NAME_None));
						if (Track && Track->GetControlRig())
						{
							ControlRigEditMode->RemoveControlRig(Track->GetControlRig());
						}
					}
					if (!MovieScene->RemovePossessable(CompGuid))
					{
						MovieScene->RemoveSpawnable(CompGuid);
					}
				}

				UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), ActorTrackGuid));
				if (!Track)
				{
					Track = MovieScene->AddTrack<UMovieSceneControlRigParameterTrack>(ActorTrackGuid);
				}

				UControlRig* ControlRig = Track->GetControlRig();

				FString ObjectName = (ControlRigClass->GetName());

				if (!ControlRig || ControlRig->GetClass() != ControlRigClass)
				{
					USkeletalMesh* SkeletalMesh = MeshActor->GetSkeletalMeshComponent()->GetSkeletalMeshAsset();
					USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

					ObjectName.RemoveFromEnd(TEXT("_C"));

					// This is either a UControlRig or a UModularRig
					ControlRig = NewObject<UControlRig>(Track, ControlRigClass, FName(*ObjectName), RF_Transactional);
					ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
					ControlRig->GetObjectBinding()->BindToObject(MeshActor->GetSkeletalMeshComponent());
					ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
					ControlRig->Initialize();
					ControlRig->Evaluate_AnyThread();
					ControlRig->CreateRigControlsForCurveContainer();

					WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				}

				UMovieSceneSection* Section = Track->GetAllSections().Num() ? Track->GetAllSections()[0] : nullptr;
				if (!Section)
				{
					Track->Modify();
					Section = Track->CreateControlRigSection(0, ControlRig, true);
					//mz todo need to have multiple rigs with same class
					Track->SetTrackName(FName(*ObjectName));
					Track->SetDisplayName(FText::FromString(ObjectName));

					WeakSequencer.Pin()->EmptySelection();
					WeakSequencer.Pin()->SelectSection(Section);
					WeakSequencer.Pin()->ThrobSectionSelection();
					WeakSequencer.Pin()->ObjectImplicitlyAdded(ControlRig);
				}

				WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
				if (!ControlRigEditMode)
				{
					GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
					ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
				}
				if (ControlRigEditMode)
				{
					ControlRigEditMode->AddControlRigObject(ControlRig, WeakSequencer.Pin());
				}
			}
		}
	});
}

const TArray<FText>& FControlRigBlueprintActions::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AnimControlRigSubMenu", "Control Rig")
	};
	return SubMenus;
}

#undef LOCTEXT_NAMESPACE

