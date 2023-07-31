// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigParameterTrackEditor.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Framework/Commands/Commands.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "CommonMovieSceneTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "ISectionLayoutBuilder.h"
#include "Styling/AppStyle.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTimeHelpers.h"
#include "Fonts/FontMeasure.h"
#include "AnimationEditorUtils.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Blueprint.h"
#include "ControlRig.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "ControlRigObjectBinding.h"
#include "LevelEditorViewport.h"
#include "IKeyArea.h"
#include "ISequencer.h"
#include "ControlRigEditorModule.h"
#include "SequencerSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Channels/FloatChannelCurveModel.h"
#include "TransformNoScale.h"
#include "ControlRigComponent.h"
#include "ISequencerObjectChangeListener.h"
#include "MovieSceneToolHelpers.h"
#include "Rigs/FKControlRig.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Exporters/AnimSeqExportOption.h"
#include "SBakeToControlRigDialog.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Animation/SkeletalMeshActor.h"
#include "TimerManager.h"
#include "BakeToControlRigSettings.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Toolkits/IToolkitHost.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "ControlRigSpaceChannelEditors.h"
#include "TransformConstraint.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/TransactionObjectEvent.h"
#include "ConstraintChannelHelper.h"
#include "MovieSceneConstraintChannelHelper.h"
#include "Constraints/ControlRigTransformableHandle.h"

#define LOCTEXT_NAMESPACE "FControlRigParameterTrackEditor"

TAutoConsoleVariable<bool> CVarSelectedKeysSelectControls(TEXT("ControlRig.Sequencer.SelectedKeysSelectControls"), false, TEXT("When true when we select a key in Sequencer it will select the Control, by default false."));

static USkeletalMeshComponent* AcquireSkeletalMeshFromObject(UObject* BoundObject, TSharedPtr<ISequencer> SequencerPtr)
{
	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Actor->GetRootComponent()))
		{
			return SkeletalMeshComponent;
		}

		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Actor->GetComponents(SkeletalMeshComponents);

		if (SkeletalMeshComponents.Num() == 1)
		{
			return SkeletalMeshComponents[0];
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		if (SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			return SkeletalMeshComponent;
		}
	}

	return nullptr;
}


static USkeleton* GetSkeletonFromComponent(UActorComponent* InComponent)
{
	USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(InComponent);
	if (SkeletalMeshComp && SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
	{
		// @todo Multiple actors, multiple components
		return SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton();
	}

	return nullptr;
}

static USkeleton* AcquireSkeletonFromObjectGuid(const FGuid& Guid, UObject** Object, TSharedPtr<ISequencer> SequencerPtr)
{
	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;
	*Object = BoundObject;
	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Actor->GetRootComponent()))
		{
			return GetSkeletonFromComponent(SkeletalMeshComponent);
		}

		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Actor->GetComponents(SkeletalMeshComponents);
		if (SkeletalMeshComponents.Num() == 1)
		{
			return GetSkeletonFromComponent(SkeletalMeshComponents[0]);
		}
		SkeletalMeshComponents.Empty();

		AActor* ActorCDO = Cast<AActor>(Actor->GetClass()->GetDefaultObject());
		if (ActorCDO)
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorCDO->GetRootComponent()))
			{
				return GetSkeletonFromComponent(SkeletalMeshComponent);
			}

			ActorCDO->GetComponents(SkeletalMeshComponents);
			if (SkeletalMeshComponents.Num() == 1)
			{
				return GetSkeletonFromComponent(SkeletalMeshComponents[0]);
			}
			SkeletalMeshComponents.Empty();
		}

		UBlueprintGeneratedClass* ActorBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
		if (ActorBlueprintGeneratedClass)
		{
			const TArray<USCS_Node*>& ActorBlueprintNodes = ActorBlueprintGeneratedClass->SimpleConstructionScript->GetAllNodes();

			for (USCS_Node* Node : ActorBlueprintNodes)
			{
				if (Node->ComponentClass && Node->ComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass()))
				{
					if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(ActorBlueprintGeneratedClass)))
					{
						SkeletalMeshComponents.Add(SkeletalMeshComponent);
					}
				}
			}

			if (SkeletalMeshComponents.Num() == 1)
			{
				return GetSkeletonFromComponent(SkeletalMeshComponents[0]);
			}
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		if (USkeleton* Skeleton = GetSkeletonFromComponent(SkeletalMeshComponent))
		{
			return Skeleton;
		}
	}

	return nullptr;
}

bool FControlRigParameterTrackEditor::bAutoGenerateControlRigTrack = true;

FControlRigParameterTrackEditor::FControlRigParameterTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FKeyframeTrackEditor<UMovieSceneControlRigParameterTrack>(InSequencer)
	, bCurveDisplayTickIsPending(false)
	, bIsDoingSelection(false)
	, bSkipNextSelectionFromTimer(false)
	, bFilterAssetBySkeleton(true)
	, bFilterAssetByAnimatableControls(true)
	, ControlUndoBracket(0)
{
	FMovieSceneToolsModule::Get().RegisterAnimationBakeHelper(this);

	if (GEditor != nullptr)
	{
		GEditor->RegisterForUndo(this);
	}

	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	SelectionChangedHandle = InSequencer->GetSelectionChangedTracks().AddRaw(this, &FControlRigParameterTrackEditor::OnSelectionChanged);
	SequencerChangedHandle = InSequencer->OnMovieSceneDataChanged().AddRaw(this, &FControlRigParameterTrackEditor::OnSequencerDataChanged);
	OnActivateSequenceChangedHandle = InSequencer->OnActivateSequence().AddRaw(this, &FControlRigParameterTrackEditor::OnActivateSequenceChanged);
	OnChannelChangedHandle = InSequencer->OnChannelChanged().AddRaw(this, &FControlRigParameterTrackEditor::OnChannelChanged);
	OnMovieSceneChannelChangedHandle = MovieScene->OnChannelChanged().AddRaw(this, &FControlRigParameterTrackEditor::OnChannelChanged);
	OnActorAddedToSequencerHandle = InSequencer->OnActorAddedToSequencer().AddRaw(this, &FControlRigParameterTrackEditor::HandleActorAdded);

	{
		//we check for two things, one if the control rig has been replaced if so we need to switch.
		//the other is if bound object on the edit mode is null we request a re-evaluate which will reset it up.
		FDelegateHandle OnObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddLambda([&](const TMap<UObject*, UObject*>& ReplacementMap)
		{
			if (GetSequencer().IsValid())
			{
				static bool bHasEnteredSilent = false;
				
				bool bRequestEvaluate = false;
				TMap<UControlRig*, UControlRig*> OldToNewControlRigs;
				FControlRigEditMode* ControlRigEditMode = GetEditMode();
				if (ControlRigEditMode)
				{
					TArrayView<TWeakObjectPtr<UControlRig>> ControlRigs = ControlRigEditMode->GetControlRigs();
					for (TWeakObjectPtr<UControlRig>& ControlRigPtr : ControlRigs)
					{
						if (UControlRig* ControlRig = ControlRigPtr.Get())
						{
							if (ControlRig->GetObjectBinding() && ControlRig->GetObjectBinding()->GetBoundObject() == nullptr)
							{
								bRequestEvaluate = true;
								break;
							}
						}
					}
				}
				//Reset Bindings for replaced objects.
				for (TPair<UObject*, UObject*> ReplacedObject : ReplacementMap)
				{
					if (UControlRigComponent* OldControlRigComponent = Cast<UControlRigComponent>(ReplacedObject.Key))
					{
						UControlRigComponent* NewControlRigComponent = Cast<UControlRigComponent>(ReplacedObject.Value);
						if (OldControlRigComponent->GetControlRig())
						{
							UControlRig* NewControlRig = nullptr;
							if (NewControlRigComponent)
							{
								NewControlRig = NewControlRigComponent->GetControlRig();
							}
							OldToNewControlRigs.Emplace(OldControlRigComponent->GetControlRig(), NewControlRig);
						}
					}
					else if (UControlRig* OldControlRig = Cast<UControlRig>(ReplacedObject.Key))
					{
						UControlRig* NewControlRig = Cast<UControlRig>(ReplacedObject.Value);
						OldToNewControlRigs.Emplace(OldControlRig, NewControlRig);
					}
				}
				if (OldToNewControlRigs.Num() > 0)
				{
					//need to avoid any evaluations when doing this replacement otherwise we will evaluate sequencer
					if (bHasEnteredSilent == false)
					{
						GetSequencer()->EnterSilentMode();
						bHasEnteredSilent = true;
					}
					UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
					const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
					for (const FMovieSceneBinding& Binding : Bindings)
					{
						UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
						if (Track && Track->GetControlRig())
						{
							UControlRig* OldControlRig = Track->GetControlRig();
							UControlRig** NewControlRig = OldToNewControlRigs.Find(OldControlRig);
							if (NewControlRig)
							{
								TArray<FName> SelectedControls = OldControlRig->CurrentControlSelection();
								OldControlRig->ClearControlSelection();
								UnbindControlRig(OldControlRig);
								if (*NewControlRig)
								{
									Track->Modify();
									Track->ReplaceControlRig(*NewControlRig, OldControlRig->GetClass() != (*NewControlRig)->GetClass());
									BindControlRig(*NewControlRig);
									bRequestEvaluate = true;
								}
								else
								{
									Track->ReplaceControlRig(nullptr, true);
								}
								if (ControlRigEditMode)
								{
									ControlRigEditMode->ReplaceControlRig(OldControlRig, *NewControlRig);

									UControlRig* PtrNewControlRig = *NewControlRig;

									auto UpdateSelectionDelegate = [this, SelectedControls, PtrNewControlRig, bRequestEvaluate]()
									{

										if (!(FSlateApplication::Get().HasAnyMouseCaptor() || GUnrealEd->IsUserInteracting()))
										{
											TGuardValue<bool> Guard(bIsDoingSelection, true);
											GetSequencer()->ExternalSelectionHasChanged();
											if (PtrNewControlRig)
											{
												GEditor->GetTimerManager()->SetTimerForNextTick([this, SelectedControls, PtrNewControlRig, bRequestEvaluate]()
													{
														PtrNewControlRig->ClearControlSelection();
														for (const FName& ControlName : SelectedControls)
														{
															PtrNewControlRig->SelectControl(ControlName, true);
														}
													});
											}
											
											if (bHasEnteredSilent == true)
											{
												GetSequencer()->ExitSilentMode();
												bHasEnteredSilent = false;
											}
											
											if (bRequestEvaluate)
											{
												GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
											}
											if (UpdateSelectionTimerHandle.IsValid())
											{
												GEditor->GetTimerManager()->ClearTimer(UpdateSelectionTimerHandle);
											}
										}

									};

									GEditor->GetTimerManager()->SetTimer(UpdateSelectionTimerHandle, UpdateSelectionDelegate, 0.01f, true);
								}
							}
						}
					}
					if (!ControlRigEditMode)
					{
						if (bRequestEvaluate)
						{
							GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
						}
					}
				}

				if (ControlRigEditMode && bRequestEvaluate)
				{
					GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				}

				// ensure we exit silent mode if it has been entered
				if (bHasEnteredSilent)
				{
					GetSequencer()->ExitSilentMode();
					bHasEnteredSilent = false;
				}
			}
		});
	

		AcquiredResources.Add([=] { FCoreUObjectDelegates::OnObjectsReplaced.Remove(OnObjectsReplacedHandle); });
	}
	//register all modified/selections for control rigs
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig())
		{
			BindControlRig(Track->GetControlRig());
		}
	}
}

FControlRigParameterTrackEditor::~FControlRigParameterTrackEditor()
{
	if (GEditor != nullptr)
	{
		GEditor->UnregisterForUndo(this);
	}

	UnbindAllControlRigs();
	if (GetSequencer().IsValid())
	{
		//REMOVE ME IN UE5
		GetSequencer()->GetObjectChangeListener().GetOnPropagateObjectChanges().RemoveAll(this);
	}
	FMovieSceneToolsModule::Get().UnregisterAnimationBakeHelper(this);
}


void FControlRigParameterTrackEditor::BindControlRig(UControlRig* ControlRig)
{
	if (ControlRig && BoundControlRigs.Contains(ControlRig) == false)
	{
		ControlRig->ControlModified().AddRaw(this, &FControlRigParameterTrackEditor::HandleControlModified);
		ControlRig->OnInitialized_AnyThread().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnInitialized);
		ControlRig->ControlSelected().AddRaw(this, &FControlRigParameterTrackEditor::HandleControlSelected);
		ControlRig->ControlUndoBracket().AddRaw(this, &FControlRigParameterTrackEditor::HandleControlUndoBracket);
		ControlRig->ControlRigBound().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnControlRigBound);
		
		BoundControlRigs.Add(ControlRig);
		UMovieSceneControlRigParameterTrack* Track = FindTrack(ControlRig);
		if (Track)
		{
			for (UMovieSceneSection* BaseSection : Track->GetAllSections())
			{
				if (UMovieSceneControlRigParameterSection* Section = Cast< UMovieSceneControlRigParameterSection>(BaseSection))
				{
					if (Section->GetControlRig())
					{
						TArray<FSpaceControlNameAndChannel>& SpaceChannels = Section->GetSpaceChannels();
						for (FSpaceControlNameAndChannel& Channel : SpaceChannels)
						{
							HandleOnSpaceAdded(Section, Channel.ControlName, &(Channel.SpaceCurve));
						}
						
						TArray<FConstraintAndActiveChannel>& ConstraintChannels = Section->GetConstraintsChannels();
						for (FConstraintAndActiveChannel& Channel: ConstraintChannels)
						{ 
							HandleOnConstraintAdded(Section, &(Channel.ActiveChannel));
						}
					}
				}
			}
			Track->SpaceChannelAdded().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnSpaceAdded);
			Track->ConstraintChannelAdded().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnConstraintAdded);
		}
	}
}
void FControlRigParameterTrackEditor::UnbindControlRig(UControlRig* ControlRig)
{
	if (ControlRig && BoundControlRigs.Contains(ControlRig) == true)
	{
		UMovieSceneControlRigParameterTrack* Track = FindTrack(ControlRig);
		if (Track)
		{
			Track->SpaceChannelAdded().RemoveAll(this);
			Track->ConstraintChannelAdded().RemoveAll(this);
		}
		ControlRig->ControlModified().RemoveAll(this);
		ControlRig->OnInitialized_AnyThread().RemoveAll(this);
		ControlRig->ControlSelected().RemoveAll(this);
		if (const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
		{
			Binding->OnControlRigBind().RemoveAll(this);
		}
		ControlRig->ControlRigBound().RemoveAll(this);
		
		BoundControlRigs.Remove(ControlRig);
		ClearOutAllSpaceAndConstraintDelegates(ControlRig);
	}
}
void FControlRigParameterTrackEditor::UnbindAllControlRigs()
{
	ClearOutAllSpaceAndConstraintDelegates();
	for (TWeakObjectPtr<UControlRig>& ObjectPtr : BoundControlRigs)
	{
		if (ObjectPtr.IsValid())
		{
			UControlRig* ControlRig = ObjectPtr.Get();
			UMovieSceneControlRigParameterTrack* Track = FindTrack(ControlRig);
			if (Track)
			{
				Track->SpaceChannelAdded().RemoveAll(this);
				Track->ConstraintChannelAdded().RemoveAll(this);
			}
			ControlRig->ControlModified().RemoveAll(this);
			ControlRig->OnInitialized_AnyThread().RemoveAll(this);
			ControlRig->ControlSelected().RemoveAll(this);
			if (const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
			{
				Binding->OnControlRigBind().RemoveAll(this);
			}
			ControlRig->ControlRigBound().RemoveAll(this);
		}
	}
	BoundControlRigs.SetNum(0);
}


void FControlRigParameterTrackEditor::ObjectImplicitlyAdded(UObject* InObject)
{
	UControlRig* ControlRig = Cast<UControlRig>(InObject);
	if (ControlRig)
	{
		BindControlRig(ControlRig);
	}
}

void FControlRigParameterTrackEditor::ObjectImplicitlyRemoved(UObject* InObject)
{
	UControlRig* ControlRig = Cast<UControlRig>(InObject);
	if (ControlRig)
	{
		UnbindControlRig(ControlRig);
	}
}

void FControlRigParameterTrackEditor::OnRelease()
{
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	for (FDelegateHandle& Handle : ConstraintHandlesToClear)
	{
		if (Handle.IsValid())
		{
			Controller.GetNotifyDelegate().Remove(Handle);
		}
	}
	ConstraintHandlesToClear.Reset();

	UnbindAllControlRigs();
	if (GetSequencer().IsValid())
	{
		if (SelectionChangedHandle.IsValid())
		{
			GetSequencer()->GetSelectionChangedTracks().Remove(SelectionChangedHandle);
		}
		if (SequencerChangedHandle.IsValid())
		{
			GetSequencer()->OnMovieSceneDataChanged().Remove(SequencerChangedHandle);
		}
		if (OnActivateSequenceChangedHandle.IsValid())
		{
			GetSequencer()->OnActivateSequence().Remove(OnActivateSequenceChangedHandle);
		}
		if (CurveChangedHandle.IsValid())
		{
			GetSequencer()->GetCurveDisplayChanged().Remove(CurveChangedHandle);
		}
		if (OnActorAddedToSequencerHandle.IsValid())
		{
			GetSequencer()->OnActorAddedToSequencer().Remove(OnActorAddedToSequencerHandle);
		}
		if (OnChannelChangedHandle.IsValid())
		{
			GetSequencer()->OnChannelChanged().Remove(OnChannelChangedHandle);
		}

		if (GetSequencer()->GetFocusedMovieSceneSequence() && GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene())
		{
			UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
			if (OnMovieSceneChannelChangedHandle.IsValid())
			{
				MovieScene->OnChannelChanged().Remove(OnMovieSceneChannelChangedHandle);
			}
		}
	}
	FControlRigEditMode* ControlRigEditMode = GetEditMode();
	if (ControlRigEditMode)
	{
		ControlRigEditMode->Exit(); //deactive mode below doesn't exit for some reason so need to make sure things are cleaned up
		if (FEditorModeTools* Tools = GetEditorModeTools())
		{
			Tools->DeactivateMode(FControlRigEditMode::ModeName);
		}

		ControlRigEditMode->SetObjects(nullptr, nullptr, GetSequencer());
	}

	AcquiredResources.Release();

}

TSharedRef<ISequencerTrackEditor> FControlRigParameterTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FControlRigParameterTrackEditor(InSequencer));
}


bool FControlRigParameterTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneControlRigParameterTrack::StaticClass();
}


TSharedRef<ISequencerSection> FControlRigParameterTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FControlRigParameterSection(SectionObject, GetSequencer()));
}

void FControlRigParameterTrackEditor::BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if(!ObjectClass)
	{
		return;
	}
	
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) ||
		ObjectClass->IsChildOf(AActor::StaticClass()) ||
		ObjectClass->IsChildOf(UChildActorComponent::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
		UObject* BoundObject = nullptr;
		USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBindings[0], &BoundObject, ParentSequencer);
		USkeletalMeshComponent* SkelMeshComp = AcquireSkeletalMeshFromObject(BoundObject, ParentSequencer);

		if (Skeleton && SkelMeshComp)
		{
			MenuBuilder.BeginSection("Control Rig", LOCTEXT("ControlRig", "Control Rig"));
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("EditWithFKControlRig", "Edit With FK Control Rig"),
					LOCTEXT("ConvertToFKControlRigTooltip", "Convert to FK Control Rig and add a track for it"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ConvertToFKControlRig, ObjectBindings[0], BoundObject, SkelMeshComp, Skeleton)),
					NAME_None,
					EUserInterfaceActionType::Button);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("FilterAssetBySkeleton", "Filter Asset By Skeleton"),
					LOCTEXT("FilterAssetBySkeletonTooltip", "Filters Control Rig assets to match current skeleton"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::ToggleFilterAssetBySkeleton),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(this, &FControlRigParameterTrackEditor::IsToggleFilterAssetBySkeleton)
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton);

				MenuBuilder.AddSubMenu(
					LOCTEXT("BakeToControlRig", "Bake To Control Rig"),
					LOCTEXT("BakeToControlRigTooltip", "Bake to an invertible Control Rig that matches this skeleton"),
					FNewMenuDelegate::CreateRaw(this, &FControlRigParameterTrackEditor::BakeToControlRigSubMenu, ObjectBindings[0], BoundObject, SkelMeshComp, Skeleton)
				);
			}
			MenuBuilder.EndSection();
		}
	}
}

class FControlRigClassFilter : public IClassViewerFilter
{
public:
	FControlRigClassFilter(bool bInCheckSkeleton, bool bInCheckAnimatable, bool bInCheckInversion, USkeleton* InSkeleton) :
		bFilterAssetBySkeleton(bInCheckSkeleton),
		bFilterExposesAnimatableControls(bInCheckAnimatable),
		bFilterInversion(bInCheckInversion),
		AssetRegistry(FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	{
		Skeleton = InSkeleton;
	}
	bool bFilterAssetBySkeleton;
	bool bFilterExposesAnimatableControls;
	bool bFilterInversion;

	USkeleton* Skeleton;
	const IAssetRegistry& AssetRegistry;

	bool MatchesFilter(const FAssetData& AssetData)
	{
		bool bExposesAnimatableControls = AssetData.GetTagValueRef<bool>(TEXT("bExposesAnimatableControls"));
		if (bFilterExposesAnimatableControls == true && bExposesAnimatableControls == false)
		{
			return false;
		}
		if (bFilterInversion)
		{
			bool bHasInversion = false;
			FAssetDataTagMapSharedView::FFindTagResult Tag = AssetData.TagsAndValues.FindTag(TEXT("SupportedEventNames"));
			if (Tag.IsSet())
			{
				FString EventString = FRigUnit_InverseExecution::EventName.ToString();
				FString OldEventString = FString(TEXT("Inverse"));
				TArray<FString> SupportedEventNames;
				Tag.GetValue().ParseIntoArray(SupportedEventNames, TEXT(","), true);

				for (const FString& Name : SupportedEventNames)
				{
					if (Name.Contains(EventString) || Name.Contains(OldEventString))
					{
						bHasInversion = true;
						break;
					}
				}
				if (bHasInversion == false)
				{
					return false;
				}
			}
		}
		if (bFilterAssetBySkeleton)
		{
			FString SkeletonName;
			if (Skeleton)
			{
				SkeletonName = FAssetData(Skeleton).GetExportTextName();
			}
			FString PreviewSkeletalMesh = AssetData.GetTagValueRef<FString>(TEXT("PreviewSkeletalMesh"));
			if (PreviewSkeletalMesh.Len() > 0)
			{
				FAssetData SkelMeshData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(PreviewSkeletalMesh));
				FString PreviewSkeleton = SkelMeshData.GetTagValueRef<FString>(TEXT("Skeleton"));
				if (PreviewSkeleton == SkeletonName)
				{
					return true;
				}
				else if(Skeleton)
				{
					if (Skeleton->IsCompatibleSkeletonByAssetString(PreviewSkeleton))
					{
						return true;
					}
				}
			}
			FString PreviewSkeleton = AssetData.GetTagValueRef<FString>(TEXT("PreviewSkeleton"));
			if (PreviewSkeleton == SkeletonName)
			{
				return true;
			}
			else if (Skeleton)
			{
				if (Skeleton->IsCompatibleSkeletonByAssetString(PreviewSkeleton))
				{
					return true;
				}
			}
			FString SourceHierarchyImport = AssetData.GetTagValueRef<FString>(TEXT("SourceHierarchyImport"));
			if (SourceHierarchyImport == SkeletonName)
			{
				return true;
			}
			else if (Skeleton)
			{
				if (Skeleton->IsCompatibleSkeletonByAssetString(SourceHierarchyImport))
				{
					return true;
				}
			}
			FString SourceCurveImport = AssetData.GetTagValueRef<FString>(TEXT("SourceCurveImport"));
			if (SourceCurveImport == SkeletonName)
			{
				return true;
			}
			else if (Skeleton)
			{
				if (Skeleton->IsCompatibleSkeletonByAssetString(SourceCurveImport))
				{
					return true;
				}
			}
			return false;
		}
		return true;

	}
	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if(InClass)
		{
			const bool bChildOfObjectClass = InClass->IsChildOf(UControlRig::StaticClass());
			const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			const bool bNotNative = !InClass->IsNative();

			if (bChildOfObjectClass && bMatchesFlags && bNotNative)
			{
				FAssetData AssetData(InClass);
				return MatchesFilter(AssetData);

			}
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(UControlRig::StaticClass());
		const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
		if (bChildOfObjectClass && bMatchesFlags)
		{
			FString GeneratedClassPathString = InUnloadedClassData->GetClassPathName().ToString();
			FString BlueprintPath = GeneratedClassPathString.LeftChop(2); // Chop off _C
			FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(BlueprintPath));
			return MatchesFilter(AssetData);

		}
		return false;
	}

};

void FControlRigParameterTrackEditor::ConvertToFKControlRig(FGuid ObjectBinding, UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton)
{
	BakeToControlRig(UFKControlRig::StaticClass(), ObjectBinding, BoundObject, SkelMeshComp, Skeleton);
}

void FControlRigParameterTrackEditor::BakeToControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton)
{
	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

	if (Skeleton)
	{
		FClassViewerInitializationOptions Options;
		Options.bShowUnloadedBlueprints = true;
		Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
		TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter(bFilterAssetBySkeleton, true, true, Skeleton));
		Options.ClassFilters.Add(ClassFilter.ToSharedRef());
		Options.bShowNoneOption = false;

		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FControlRigParameterTrackEditor::BakeToControlRig, ObjectBinding, BoundObject, SkelMeshComp, Skeleton));
		MenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);
	}
}


class SBakeToAnimAndControlRigOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBakeToAnimAndControlRigOptionsWindow)
		: _ExportOptions(nullptr), _BakeSettings(nullptr)
		, _WidgetWindow()
	{}

	SLATE_ARGUMENT(UAnimSeqExportOption*, ExportOptions)
		SLATE_ARGUMENT(UBakeToControlRigSettings*, BakeSettings)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnExport()
	{
		bShouldExport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}


	FReply OnCancel()
	{
		bShouldExport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldExport() const
	{
		return bShouldExport;
	}


	SBakeToAnimAndControlRigOptionsWindow()
		: ExportOptions(nullptr)
		, BakeSettings(nullptr)
		, bShouldExport(false)
	{}

private:

	FReply OnResetToDefaultClick() const;

private:
	UAnimSeqExportOption* ExportOptions;
	UBakeToControlRigSettings* BakeSettings;
	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<class IDetailsView> DetailsView2;
	TWeakPtr< SWindow > WidgetWindow;
	bool			bShouldExport;
};


void SBakeToAnimAndControlRigOptionsWindow::Construct(const FArguments& InArgs)
{
	ExportOptions = InArgs._ExportOptions;
	BakeSettings = InArgs._BakeSettings;
	WidgetWindow = InArgs._WidgetWindow;

	check(ExportOptions);

	FText CancelText = LOCTEXT("AnimSequenceOptions_Cancel", "Cancel");
	FText CancelTooltipText = LOCTEXT("AnimSequenceOptions_Cancel_ToolTip", "Cancel control rig creation");

	TSharedPtr<SBox> HeaderToolBox;
	TSharedPtr<SHorizontalBox> AnimHeaderButtons;
	TSharedPtr<SBox> InspectorBox;
	TSharedPtr<SBox> InspectorBox2;
	this->ChildSlot
		[
			SNew(SBox)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SAssignNew(HeaderToolBox, SBox)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("CurveEd.LabelFont"))
		.Text(LOCTEXT("Export_CurrentFileTitle", "Current File: "))
		]
		]
		]
	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2)
		[
			SAssignNew(InspectorBox, SBox)
		]
	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2)
		[
			SAssignNew(InspectorBox2, SBox)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
		+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.Text(LOCTEXT("Create", "Create"))
		.OnClicked(this, &SBakeToAnimAndControlRigOptionsWindow::OnExport)
		]
	+ SUniformGridPanel::Slot(2, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.Text(CancelText)
		.ToolTipText(CancelTooltipText)
		.OnClicked(this, &SBakeToAnimAndControlRigOptionsWindow::OnCancel)
		]
		]
			]
		];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView2 = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InspectorBox->SetContent(DetailsView->AsShared());
	InspectorBox2->SetContent(DetailsView2->AsShared());
	HeaderToolBox->SetContent(
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
		[
			SAssignNew(AnimHeaderButtons, SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(SButton)
			.Text(LOCTEXT("AnimSequenceOptions_ResetOptions", "Reset to Default"))
		.OnClicked(this, &SBakeToAnimAndControlRigOptionsWindow::OnResetToDefaultClick)
		]
		]
		]
		]
	);

	DetailsView->SetObject(ExportOptions);
	DetailsView2->SetObject(BakeSettings);
}

FReply SBakeToAnimAndControlRigOptionsWindow::OnResetToDefaultClick() const
{
	if (ExportOptions)
	{
		ExportOptions->ResetToDefault();
		//Refresh the view to make sure the custom UI are updating correctly
		DetailsView->SetObject(ExportOptions, true);
	}

	if (BakeSettings)
	{
		BakeSettings->Reset();
		DetailsView2->SetObject(BakeSettings, true);
	}
	
	return FReply::Handled();
}

void FControlRigParameterTrackEditor::BakeToControlRig(UClass* InClass, FGuid ObjectBinding, UObject* BoundActor, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton)
{
	FSlateApplication::Get().DismissAllMenus();
	const TSharedPtr<ISequencer> SequencerParent = GetSequencer();

	if (InClass && InClass->IsChildOf(UControlRig::StaticClass()) && SequencerParent.IsValid())
	{

		UMovieSceneSequence* OwnerSequence = GetSequencer()->GetFocusedMovieSceneSequence();
		UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
		{
			UAnimSequence* TempAnimSequence = NewObject<UAnimSequence>(GetTransientPackage(), NAME_None);
			TempAnimSequence->SetSkeleton(Skeleton);
			const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
			FMovieSceneSequenceIDRef Template = ParentSequencer->GetFocusedTemplateID();
			FMovieSceneSequenceTransform RootToLocalTransform = ParentSequencer->GetFocusedMovieSceneSequenceTransform();
			UAnimSeqExportOption* AnimSeqExportOption = NewObject<UAnimSeqExportOption>(GetTransientPackage(), NAME_None);
			UBakeToControlRigSettings* BakeSettings = GetMutableDefault<UBakeToControlRigSettings>();

			TSharedPtr<SWindow> ParentWindow;
			if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
			{
				IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
				ParentWindow = MainFrame.GetParentWindow();
			}

			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(LOCTEXT("AnimSeqTitle", "Options For Baking"))
				.SizingRule(ESizingRule::UserSized)
				.AutoCenter(EAutoCenter::PrimaryWorkArea)
				.ClientSize(FVector2D(500, 445));

			TSharedPtr<SBakeToAnimAndControlRigOptionsWindow> OptionWindow;
			Window->SetContent
			(
				SAssignNew(OptionWindow, SBakeToAnimAndControlRigOptionsWindow)
				.ExportOptions(AnimSeqExportOption)
				.BakeSettings(BakeSettings)
				.WidgetWindow(Window)
			);

			FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

			if (OptionWindow.Get()->ShouldExport())
			{

				bool bResult = MovieSceneToolHelpers::ExportToAnimSequence(TempAnimSequence, AnimSeqExportOption, OwnerMovieScene, ParentSequencer.Get(), SkelMeshComp, Template, RootToLocalTransform);
				if (bResult == false)
				{
					TempAnimSequence->MarkAsGarbage();
					AnimSeqExportOption->MarkAsGarbage();
					return;
				}

				const FScopedTransaction Transaction(LOCTEXT("BakeToControlRig_Transaction", "Bake To Control Rig"));
				
				bool bReuseControlRig = false; //if same Class just re-use it, and put into a new section
				OwnerMovieScene->Modify();
				UMovieSceneControlRigParameterTrack* Track = OwnerMovieScene->FindTrack<UMovieSceneControlRigParameterTrack>(ObjectBinding);
				if (Track)
				{
					if (Track->GetControlRig() && Track->GetControlRig()->GetClass() == InClass)
					{
						bReuseControlRig = true;
					}
					Track->Modify();
					Track->RemoveAllAnimationData();//removes all sections and sectiontokey
				}
				else
				{
					Track = Cast<UMovieSceneControlRigParameterTrack>(AddTrack(OwnerMovieScene, ObjectBinding, UMovieSceneControlRigParameterTrack::StaticClass(), NAME_None));
					if (Track)
					{
						Track->Modify();
					}
				}

				if (Track)
				{

					FString ObjectName = InClass->GetName();
					ObjectName.RemoveFromEnd(TEXT("_C"));
					UControlRig* ControlRig = bReuseControlRig ? Track->GetControlRig() : NewObject<UControlRig>(Track, InClass, FName(*ObjectName), RF_Transactional);
					if (InClass != UFKControlRig::StaticClass() && !ControlRig->SupportsEvent(FRigUnit_InverseExecution::EventName))
					{
						TempAnimSequence->MarkAsGarbage();
						AnimSeqExportOption->MarkAsGarbage();
						OwnerMovieScene->RemoveTrack(*Track);
						return;
					}

					FControlRigEditMode* ControlRigEditMode = GetEditMode();
					if (!ControlRigEditMode)
					{
						ControlRigEditMode = GetEditMode(true);
					}
					else
					{
						/* mz todo we don't unbind  will test more
						UControlRig* OldControlRig = ControlRigEditMode->GetControlRig(false);
						if (OldControlRig)
						{
							UnbindControlRig(OldControlRig);
						}
						*/
					}

					if (bReuseControlRig == false)
					{
						ControlRig->Modify();
						ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
						ControlRig->GetObjectBinding()->BindToObject(BoundActor);
						ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
						ControlRig->Initialize();
						ControlRig->RequestInit();
						ControlRig->SetBoneInitialTransformsFromSkeletalMeshComponent(SkelMeshComp, true);
						ControlRig->Evaluate_AnyThread();
					}

					const bool bSequencerOwnsControlRig = true;
					UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig);
					UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(NewSection);

					//mz todo need to have multiple rigs with same class
					Track->SetTrackName(FName(*ObjectName));
					Track->SetDisplayName(FText::FromString(ObjectName));

					GetSequencer()->EmptySelection();
					GetSequencer()->SelectSection(NewSection);
					GetSequencer()->ThrobSectionSelection();
					GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
					ParamSection->LoadAnimSequenceIntoThisSection(TempAnimSequence, OwnerMovieScene, SkelMeshComp,
						BakeSettings->bReduceKeys, BakeSettings->Tolerance);

					//Turn Off Any Skeletal Animation Tracks
					TArray<UMovieSceneSkeletalAnimationTrack*> SkelAnimationTracks;
					if (const FMovieSceneBinding* Binding = OwnerMovieScene->FindBinding(ObjectBinding))
					{
						for (UMovieSceneTrack* MovieSceneTrack : Binding->GetTracks())
						{
							if (UMovieSceneSkeletalAnimationTrack* SkelTrack = Cast<UMovieSceneSkeletalAnimationTrack>(MovieSceneTrack))
							{
								SkelAnimationTracks.Add(SkelTrack);
							}
						}
					}
					FGuid SkelMeshGuid = GetSequencer()->FindObjectId(*SkelMeshComp, GetSequencer()->GetFocusedTemplateID());
					if (const FMovieSceneBinding* Binding = OwnerMovieScene->FindBinding(SkelMeshGuid))
					{
						for (UMovieSceneTrack* MovieSceneTrack : Binding->GetTracks())
						{
							if (UMovieSceneSkeletalAnimationTrack* SkelTrack = Cast<UMovieSceneSkeletalAnimationTrack>(MovieSceneTrack))
							{
								SkelAnimationTracks.Add(SkelTrack);
							}
						}
					}

					for (UMovieSceneSkeletalAnimationTrack* SkelTrack : SkelAnimationTracks)
					{
						SkelTrack->Modify();
						//can't just turn off the track so need to mute the sections
						const TArray<UMovieSceneSection*>& Sections = SkelTrack->GetAllSections();
						for (UMovieSceneSection* Section : Sections)
						{
							if (Section)
							{
								Section->TryModify();
								Section->SetIsActive(false);
							}
						}
					}

					//Finish Setup
					if (ControlRigEditMode)
					{
						ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer());
					}
					BindControlRig(ControlRig);

					TempAnimSequence->MarkAsGarbage();
					AnimSeqExportOption->MarkAsGarbage();
					GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);


				}
			}
		}
	}
}

void FControlRigParameterTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if(!ObjectClass)
	{
		return;
	}
	
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) ||
		ObjectClass->IsChildOf(AActor::StaticClass()) ||
		ObjectClass->IsChildOf(UChildActorComponent::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
		UObject* BoundObject = nullptr;
		USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBindings[0], &BoundObject, ParentSequencer);

		if (AActor* BoundActor = Cast<AActor>(BoundObject))
		{
			if (UControlRigComponent* ControlRigComponent = BoundActor->FindComponentByClass<UControlRigComponent>())
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AddControlRigTrack", "Add Control Rig Track"),
					LOCTEXT("AddControlRigTrackTooltip", "Adds an animation Control Rig track"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::AddControlRigFromComponent, ObjectBindings[0]),
						FCanExecuteAction()
					)
				);
				return;
			}
		}

		if (Skeleton)
		{
			//if there are any other control rigs we don't allow it for now..
			//mz todo will allow later
			UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
			UMovieSceneControlRigParameterTrack* ExistingTrack = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), ObjectBindings[0], NAME_None));
			if (!ExistingTrack)
			{
				UMovieSceneTrack* Track = nullptr;

				MenuBuilder.AddSubMenu(LOCTEXT("ControlRigText", "Control Rig"), FText(), FNewMenuDelegate::CreateSP(this, &FControlRigParameterTrackEditor::HandleAddTrackSubMenu, ObjectBindings, Track));
			}
		}
	}
}


void FControlRigParameterTrackEditor::HandleAddTrackSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddFKControlRig", "FK Control Rig"),
		LOCTEXT("AddFKControlRigTooltip", "Adds an FK Control Rig track"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::AddFKControlRig, ObjectBindings),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterAssetBySkeleton", "Filter Asset By Skeleton"),
		LOCTEXT("FilterAssetBySkeletonTooltip", "Filters Control Rig assets to match current skeleton"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::ToggleFilterAssetBySkeleton),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FControlRigParameterTrackEditor::IsToggleFilterAssetBySkeleton)
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterAssetByAnimatableControls", "Filter Asset By Animatable Controls"),
		LOCTEXT("FilterAssetByAnimatableControlsTooltip", "Filters Control Rig assets to only show those with Animatable Controls"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::ToggleFilterAssetByAnimatableControls),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FControlRigParameterTrackEditor::IsToggleFilterAssetByAnimatableControls)
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddSubMenu(
		LOCTEXT("AddAssetControlRig", "Asset-Based Control Rig"),
		LOCTEXT("AddAsetControlRigTooltip", "Adds an asset based Control Rig track"),
		FNewMenuDelegate::CreateRaw(this, &FControlRigParameterTrackEditor::HandleAddControlRigSubMenu, ObjectBindings, Track)
	);
}

void FControlRigParameterTrackEditor::ToggleFilterAssetBySkeleton()
{
	bFilterAssetBySkeleton = bFilterAssetBySkeleton ? false : true;
}

bool FControlRigParameterTrackEditor::IsToggleFilterAssetBySkeleton()
{
	return bFilterAssetBySkeleton;
}

void FControlRigParameterTrackEditor::ToggleFilterAssetByAnimatableControls()
{
	bFilterAssetByAnimatableControls = bFilterAssetByAnimatableControls ? false : true;

}

bool FControlRigParameterTrackEditor::IsToggleFilterAssetByAnimatableControls()
{
	return bFilterAssetByAnimatableControls;
}

void FControlRigParameterTrackEditor::HandleAddControlRigSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	/*
	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
	FAssetPickerConfig AssetPickerConfig;
	{
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FControlRigTrackEditor::OnSequencerAssetSelected, ObjectBindings, Track);
	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FControlRigTrackEditor::OnSequencerAssetEnterPressed, ObjectBindings, Track);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.Filter.ClassPaths.Add(UControlRigSequence::StaticClass()->GetClassPathName());
	AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
	.WidthOverride(300.0f)
	.HeightOverride(300.0f)
	[
	ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
	];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();
	*/


	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
	UObject* BoundObject = nullptr;
	//todo support multiple bindings?
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBindings[0], &BoundObject, GetSequencer());

	if (Skeleton)
	{
		//MenuBuilder.AddSubMenu(
		//	LOCTEXT("AddControlRigTrack", "ControlRigTrack"), NSLOCTEXT("ControlRig", "AddControlRigTrack", "Adds a Control Rigtrack."),
		//	FNewMenuDelegate::CreateRaw(this, &FControlRigParameterTrackEditor::AddAnimationSubMenu, BoundObject, ObjectBindings[0], Skeleton)
		//);

		FClassViewerInitializationOptions Options;
		Options.bShowUnloadedBlueprints = true;
		Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
		
		TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter(bFilterAssetBySkeleton, bFilterAssetByAnimatableControls, false, Skeleton));
		Options.ClassFilters.Add(ClassFilter.ToSharedRef());
		Options.bShowNoneOption = false;

		UMovieSceneSequence* Sequence = GetSequencer() ? GetSequencer()->GetFocusedMovieSceneSequence() : nullptr;
		Options.AdditionalReferencingAssets.Add(FAssetData(Sequence));

		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
		//	FNewMenuDelegate::CreateRaw(this, &FControlRigParameterTrackEditor::AddAnimationSubMenu, BoundObject, ObjectBindings[0], Skeleton)



		TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FControlRigParameterTrackEditor::AddControlRig, BoundObject, ObjectBindings[0]));
		MenuBuilder.AddWidget(ClassViewer, FText::GetEmpty(), true);

		/*
		FAssetPickerConfig AssetPickerConfig;
		{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FControlRigParameterTrackEditor::OnControlRigAssetSelected, ObjectBindings, Skeleton);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FControlRigParameterTrackEditor::OnControlRigAssetEnterPressed, ObjectBindings, Skeleton);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateRaw(this, &FControlRigParameterTrackEditor::ShouldFilterAsset);
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassPaths.Add((UControlRig::StaticClass())->GetClassPathName());
		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");

		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

		MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
		*/
	}

}

/*
bool FControlRigParameterTrackEditor::ShouldFilterAsset(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UControlRig::StaticClass()->GetClassPathName())
	{
		return true;
	}
	return false;
}

void FControlRigParameterTrackEditor::OnControlRigAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings, USkeleton* Skeleton)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();todo
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SelectedObject && SelectedObject->IsA(UControlRig::StaticClass()) && SequencerPtr.IsValid())
	{
		UControlRig* ControlRig = CastChecked<UControlRig>(AssetData.GetAsset());

		const FScopedTransaction Transaction(LOCTEXT("AddAnimation_Transaction", "Add Animation"));

		for (FGuid ObjectBinding : ObjectBindings)
		{
			UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);
			int32 RowIndex = INDEX_NONE;
			//AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FControlRigParameterTrackEditor::AddControlRig, BoundObject, ObjectBindings[0], Skeleton)));
		}
	}
*/

void FControlRigParameterTrackEditor::AddControlRig(UClass* InClass, UObject* BoundActor, FGuid ObjectBinding, UControlRig* InExistingControlRig)
{
	FSlateApplication::Get().DismissAllMenus();
	const TSharedPtr<ISequencer> SequencerParent = GetSequencer();

	if (InClass && InClass->IsChildOf(UControlRig::StaticClass()) && SequencerParent.IsValid())
	{
		UMovieSceneSequence* OwnerSequence = GetSequencer()->GetFocusedMovieSceneSequence();
		UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
		FScopedTransaction AddControlRigTrackTransaction(LOCTEXT("AddControlRigTrack", "Add Control Rig Track"));

		OwnerSequence->Modify();
		OwnerMovieScene->Modify();
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AddTrack(OwnerMovieScene, ObjectBinding, UMovieSceneControlRigParameterTrack::StaticClass(), NAME_None));
		if (Track)
		{
			FString ObjectName = InClass->GetName(); //GetDisplayNameText().ToString();
			ObjectName.RemoveFromEnd(TEXT("_C"));

			bool bSequencerOwnsControlRig = false;
			UControlRig* ControlRig = InExistingControlRig;
			if (ControlRig == nullptr)
			{
				ControlRig = NewObject<UControlRig>(Track, InClass, FName(*ObjectName), RF_Transactional);
				bSequencerOwnsControlRig = true;
			}

			ControlRig->Modify();
			ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
			ControlRig->GetObjectBinding()->BindToObject(BoundActor);
			ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
			// Do not re-initialize existing control rig
			if (!InExistingControlRig)
			{
				ControlRig->Initialize();
			}
			ControlRig->Evaluate_AnyThread();

			SequencerParent->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

			Track->Modify();
			UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig);
			NewSection->Modify();

			//mz todo need to have multiple rigs with same class
			Track->SetTrackName(FName(*ObjectName));
			Track->SetDisplayName(FText::FromString(ObjectName));

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

			FControlRigEditMode* ControlRigEditMode = GetEditMode(true);

			if (ControlRigEditMode)
			{
				ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer());
			}
			BindControlRig(ControlRig);

		}
	}
}

void FControlRigParameterTrackEditor::AddControlRig(UClass* InClass, UObject* BoundActor, FGuid ObjectBinding)
{
	AddControlRig(InClass, BoundActor, ObjectBinding, nullptr);
}

//This now adds all of the control rig components, not just the first one
void FControlRigParameterTrackEditor::AddControlRigFromComponent(FGuid InGuid)
{
	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
	UObject* BoundObject = ParentSequencer.IsValid() ? ParentSequencer->FindSpawnedObjectOrTemplate(InGuid) : nullptr;

	if (AActor* BoundActor = Cast<AActor>(BoundObject))
	{
		TArray<UControlRigComponent*> ControlRigComponents;
		BoundActor->GetComponents(ControlRigComponents);
		for (UControlRigComponent* ControlRigComponent : ControlRigComponents)
		{
			if (UControlRig* CR = ControlRigComponent->GetControlRig())
			{
				AddControlRig(CR->GetClass(), BoundActor, InGuid, CR);
			}
		}

	}
}

void FControlRigParameterTrackEditor::AddFKControlRig(TArray<FGuid> ObjectBindings)
{
	for (const FGuid& ObjectBinding : ObjectBindings)
	{
		UObject* BoundObject = nullptr;
		AcquireSkeletonFromObjectGuid(ObjectBinding, &BoundObject, GetSequencer());
		if (BoundObject)
		{
			AddControlRig(UFKControlRig::StaticClass(), BoundObject, ObjectBinding);
		}
	}
}

bool FControlRigParameterTrackEditor::HasTransformKeyOverridePriority() const
{
	return CanAddTransformKeysForSelectedObjects();

}
bool FControlRigParameterTrackEditor::CanAddTransformKeysForSelectedObjects() const
{
	// WASD hotkeys to fly the viewport can conflict with hotkeys for setting keyframes (ie. s). 
	// If the viewport is moving, disregard setting keyframes.
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsMovingCamera())
		{
			return false;
		}
	}

	if (!GetSequencer()->IsAllowedToChange())
	{
		return false;
	}

	FControlRigEditMode* ControlRigEditMode = GetEditMode();
	if (ControlRigEditMode)
	{
		TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
		ControlRigEditMode->GetAllSelectedControls(SelectedControls);
		return (SelectedControls.Num() > 0);
		}
	return false;
}

void FControlRigParameterTrackEditor::OnAddTransformKeysForSelectedObjects(EMovieSceneTransformChannel Channel)
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}
	
	const FControlRigEditMode* ControlRigEditMode = GetEditMode();
	if (!ControlRigEditMode)
	{
		return;
	}

	TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
	ControlRigEditMode->GetAllSelectedControls(SelectedControls);
	if (SelectedControls.Num() <= 0)
	{
		return;
	}
	const EControlRigContextChannelToKey ChannelsToKey = static_cast<EControlRigContextChannelToKey>(Channel); 
	FScopedTransaction KeyTransaction(LOCTEXT("SetKeysOnControls", "Set Keys On Controls"), !GIsTransacting);

	static constexpr bool bInConstraintSpace = true;
	for (const TPair<UControlRig*, TArray<FRigElementKey>>& Selection : SelectedControls)
	{
		UControlRig* ControlRig = Selection.Key;
		if (const TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
		{
			if (USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject()))
			{
				const FName Name(*ControlRig->GetName());
			
				const TArray<FName> ControlNames = ControlRig->CurrentControlSelection();
				for (const FName& ControlName : ControlNames)
				{
					AddControlKeys(Component, ControlRig, Name, ControlName, ChannelsToKey,
						ESequencerKeyMode::ManualKeyForced, FLT_MAX, bInConstraintSpace);
				}
			}
		}
	}
}

//function to evaluate a Control and Set it on the ControlRig
static void EvaluateThisControl(UMovieSceneControlRigParameterSection* Section, const FName& ControlName, const FFrameTime& FrameTime)
{
	if (!Section)
	{
		return;
	}
	UControlRig* ControlRig = Section->GetControlRig();
	if (!ControlRig)
	{
		return;
	}
	if(FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
	{ 
		FControlRigInteractionScope InteractionScope(ControlRig, ControlElement->GetKey());
		
		//eval any space for this channel, if not additive section
		if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
		{
			TOptional<FMovieSceneControlRigSpaceBaseKey> SpaceKey = Section->EvaluateSpaceChannel(FrameTime, ControlName);
			if (SpaceKey.IsSet())
			{
				URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
				switch (SpaceKey.GetValue().SpaceType)
				{
				case EMovieSceneControlRigSpaceType::Parent:
					RigHierarchy->SwitchToDefaultParent(ControlElement->GetKey());
					break;
				case EMovieSceneControlRigSpaceType::World:
					RigHierarchy->SwitchToWorldSpace(ControlElement->GetKey());
					break;
				case EMovieSceneControlRigSpaceType::ControlRig:
					URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
					RigHierarchy->SwitchToParent(ControlElement->GetKey(), SpaceKey.GetValue().ControlRigElement, false, true, Dependencies, nullptr);
					break;
				}
			}
		}
		const bool bSetupUndo = false;
		switch (ControlElement->Settings.ControlType)
		{
			case ERigControlType::Bool:
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{
					TOptional <bool> Value = Section->EvaluateBoolParameter(FrameTime, ControlName);
					if (Value.IsSet())
					{
						ControlRig->SetControlValue<bool>(ControlName, Value.GetValue(), true, EControlRigSetKey::Never, bSetupUndo);
					}
				}
				break;
			}
			case ERigControlType::Integer:
			{
				if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
				{
					if (ControlElement->Settings.ControlEnum)
					{
						TOptional<uint8> Value = Section->EvaluateEnumParameter(FrameTime, ControlName);
						if (Value.IsSet())
						{
							int32 IVal = (int32)Value.GetValue();
							ControlRig->SetControlValue<int32>(ControlName, IVal, true, EControlRigSetKey::Never, bSetupUndo);
						}
					}
					else
					{
						TOptional <int32> Value = Section->EvaluateIntegerParameter(FrameTime, ControlName);
						if (Value.IsSet())
						{
							ControlRig->SetControlValue<int32>(ControlName, Value.GetValue(), true, EControlRigSetKey::Never, bSetupUndo);
						}
					}
				}
				break;

			}
			case ERigControlType::Float:
			{
				TOptional <float> Value = Section->EvaluateScalarParameter(FrameTime, ControlName);
				if (Value.IsSet())
				{
					ControlRig->SetControlValue<float>(ControlName, Value.GetValue(), true, EControlRigSetKey::Never, bSetupUndo);
				}
				break;
			}
			case ERigControlType::Vector2D:
			{
				TOptional <FVector2D> Value = Section->EvaluateVector2DParameter(FrameTime, ControlName);
				if (Value.IsSet())
				{
					ControlRig->SetControlValue<FVector2D>(ControlName, Value.GetValue(), true, EControlRigSetKey::Never, bSetupUndo);
				}
				break;
			}
			case ERigControlType::Position:
			case ERigControlType::Scale:
			case ERigControlType::Rotator:
			{
				TOptional <FVector> Value = Section->EvaluateVectorParameter(FrameTime, ControlName);
				if (Value.IsSet())
				{
					FVector3f FloatVal = (FVector3f)Value.GetValue();
					ControlRig->SetControlValue<FVector3f>(ControlName, FloatVal, true, EControlRigSetKey::Never, bSetupUndo);
				}
				break;
			}

			case ERigControlType::Transform:
			case ERigControlType::TransformNoScale:
			case ERigControlType::EulerTransform:
			{
				TOptional <FEulerTransform> Value = Section->EvaluateTransformParameter(FrameTime, ControlName);
				if (Value.IsSet())
				{
					if (ControlElement->Settings.ControlType == ERigControlType::Transform)
					{
						ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, Value.GetValue().ToFTransform(), true, EControlRigSetKey::Never, bSetupUndo);
						ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Value.GetValue().Rotation);
					}
					else if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
					{
						FTransformNoScale NoScale = Value.GetValue().ToFTransform();
						ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, NoScale, true, EControlRigSetKey::Never, bSetupUndo);
						ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Value.GetValue().Rotation);
					}
					else if (ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
					{
						const FEulerTransform& Euler = Value.GetValue();
						ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Euler, true, EControlRigSetKey::Never, bSetupUndo);
					}
				}
				break;

			}
		}
		//note we don't need to evaluate the control rig, setting the value is enough
	}
}

//When a channel is changed via Sequencer we need to call SetControlValue on it so that Control Rig can handle seeing that this is a change, but just on this value
//and then it send back a key if needed, which happens with IK/FK switches. Hopefully new IK/FK system will remove need for this at some point.
//We also compensate since the changed control could happen at a space switch boundary.
//Finally, since they can happen thousands of times interactively when moving a bunch of keys on a control rig we move to doing this into the next tick
struct FChannelChangedStruct
{
	FTimerHandle TimerHandle;
	bool bWasSetAlready = false;
	TMap<UMovieSceneControlRigParameterSection*, TSet<FName>> SectionControlNames;
};

void FControlRigParameterTrackEditor::OnChannelChanged(const FMovieSceneChannelMetaData* MetaData, UMovieSceneSection* InSection)
{
	static FChannelChangedStruct ChannelChanged;

	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(InSection);
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (Section && Section->GetControlRig() && MetaData && SequencerPtr.IsValid())
	{
		TArray<FString> StringArray;
		FString String = MetaData->Name.ToString();
		String.ParseIntoArray(StringArray, TEXT("."));
		if (StringArray.Num() > 0)
		{
			FName ControlName(*StringArray[0]);
			if (ChannelChanged.bWasSetAlready == false)
			{
				ChannelChanged.bWasSetAlready = true;
				ChannelChanged.SectionControlNames.Reset();
				auto ChannelChangedDelegate = [this]()
				{
					TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
					if (!SequencerPtr.IsValid())
					{
						GEditor->GetTimerManager()->ClearTimer(ChannelChanged.TimerHandle);
						return;
					}

					if (!(FSlateApplication::Get().HasAnyMouseCaptor() || GUnrealEd->IsUserInteracting()))
					{
						if (ChannelChanged.TimerHandle.IsValid())
						{
							FFrameTime Time = SequencerPtr->GetLocalTime().Time;
							GEditor->GetTimerManager()->ClearTimer(ChannelChanged.TimerHandle);
							ChannelChanged.TimerHandle.Invalidate();
							ChannelChanged.bWasSetAlready = false;
							TOptional<FFrameNumber> Optional;
							ISequencer* SequencerRaw = SequencerPtr.Get();
							for (TPair <UMovieSceneControlRigParameterSection*, TSet<FName>>& Pair : ChannelChanged.SectionControlNames)
							{
								for (FName& ControlName : Pair.Value)
								{
									Pair.Key->ControlsToSet.Empty();
									Pair.Key->ControlsToSet.Add(ControlName);
									//only do the fk/ik hack for bool's since that's only where it's needed
									//otherwise we would incorrectly set values on an addtive section which has scale values of 0.0.
									if (FRigControlElement* ControlElement = Pair.Key->GetControlRig()->FindControl(ControlName))
									{
										if (ControlElement->Settings.ControlType == ERigControlType::Bool)
										{
											EvaluateThisControl(Pair.Key, ControlName, Time);
										}
									}
									FControlRigSpaceChannelHelpers::CompensateIfNeeded(Pair.Key->GetControlRig(), SequencerRaw, Pair.Key,
										ControlName, Optional);
									Pair.Key->ControlsToSet.Empty();
								}
							}
							ChannelChanged.SectionControlNames.Reset();
						}
					}
				};
				GEditor->GetTimerManager()->SetTimer(ChannelChanged.TimerHandle, ChannelChangedDelegate, 0.01f, true);			
			}
			if (TSet<FName>* SetOfNames = ChannelChanged.SectionControlNames.Find(Section))
			{
				SetOfNames->Add(ControlName);
			}
			else
			{
				TSet<FName> Names;
				Names.Add(ControlName);
				ChannelChanged.SectionControlNames.Add(Section, Names);
			}
		}
	}
}

void FControlRigParameterTrackEditor::AddTrackForComponent(USceneComponent* InComponent, FGuid InBinding)
{
	if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(InComponent))
	{
		if(bAutoGenerateControlRigTrack && !SkelMeshComp->GetDefaultAnimatingRig().IsNull())
		{
			UObject* Object = SkelMeshComp->GetDefaultAnimatingRig().LoadSynchronous();
			if (Object != nullptr && (Object->IsA<UControlRigBlueprint>() || Object->IsA<UControlRigComponent>()))
			{
				FGuid Binding = InBinding.IsValid() ? InBinding : GetSequencer()->GetHandleToObject(InComponent, true /*bCreateHandle*/);
				if (Binding.IsValid())
				{
					UMovieSceneSequence* OwnerSequence = GetSequencer()->GetFocusedMovieSceneSequence();
					UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
					UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(OwnerMovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding, NAME_None));
					if (Track == nullptr)
					{
						if (UControlRigBlueprint* BPControlRig = Cast<UControlRigBlueprint>(Object))
						{
							if (UControlRigBlueprintGeneratedClass* RigClass = BPControlRig->GetControlRigBlueprintGeneratedClass())
							{
								if (UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */)))
								{
									AddControlRig(CDO->GetClass(), InComponent, Binding);
								}
							}
						}
					}
				}
			}
		}
	}
	TArray<USceneComponent*> ChildComponents;
	InComponent->GetChildrenComponents(false, ChildComponents);
	for (USceneComponent* ChildComponent : ChildComponents)
	{
		AddTrackForComponent(ChildComponent, FGuid());
	}
}

void FControlRigParameterTrackEditor::HandleActorAdded(AActor* Actor, FGuid TargetObjectGuid)
{
	if (Actor)
	{
		if (UControlRigComponent* ControlRigComponent = Actor->FindComponentByClass<UControlRigComponent>())
		{
			AddControlRigFromComponent(TargetObjectGuid);
			return;
		}

		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Actor->GetRootComponent()))
		{
			AddTrackForComponent(SkeletalMeshComponent, TargetObjectGuid);
			return;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
			{
				AddTrackForComponent(SceneComp, FGuid());
			}
		}
	}
}

void FControlRigParameterTrackEditor::OnActivateSequenceChanged(FMovieSceneSequenceIDRef ID)
{
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	//register all modified/selections for control rigs
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig())
		{
			BindControlRig(Track->GetControlRig());
		}
	}
}


void FControlRigParameterTrackEditor::OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	FControlRigEditMode* ControlRigEditMode = GetEditMode();

	//if we have a valid control rig edit mode need to check and see the control rig in that mode is still in a track
	//if not we get rid of it.
	if (ControlRigEditMode && ControlRigEditMode->GetControlRigsArray(false /*bIsVisible*/).Num() != 0 && MovieScene && (DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved ||
		DataChangeType == EMovieSceneDataChangeType::Unknown))
	{
		TArray<UControlRig*> ControlRigs = ControlRigEditMode->GetControlRigsArray(false /*bIsVisible*/);
		float FPS = 1.f / (float)GetSequencer()->GetFocusedDisplayRate().AsInterval();
		for (UControlRig* ControlRig : ControlRigs)
		{
			if (ControlRig)
			{
				ControlRig->SetFramesPerSecond(FPS);

		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
				bool bControlRigInTrack = false;
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
					if (Track && Track->GetControlRig() == ControlRig)
			{
						bControlRigInTrack = true;
						break;; //continue, we still have a good track
			}
		}
				/* Nope don't do this anymore, todo mz
		if (FEditorModeTools* Tools = GetEditorModeTools())
		{
			Tools->DeactivateMode(FControlRigEditMode::ModeName);
		}
				*/
				if (bControlRigInTrack == false)
				{
					ControlRigEditMode->RemoveControlRig(ControlRig);
				}
			}
		}
	}
}


void FControlRigParameterTrackEditor::PostEvaluation(UMovieScene* MovieScene, FFrameNumber Frame)
{
	if (MovieScene)
	{
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			if (UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None)))
			{
				if (UControlRig* ControlRig = Track->GetControlRig())
				{
					if (ControlRig->GetObjectBinding())
					{
						if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
						{
							ControlRigComponent->Update(.1); //delta time doesn't matter.
						}
					}
				}
			}
		}
	}
}

void FControlRigParameterTrackEditor::OnSelectionChanged(TArray<UMovieSceneTrack*> InTracks)
{
	if (bIsDoingSelection || GetSequencer().IsValid() == false)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsDoingSelection, true);

	if(bSkipNextSelectionFromTimer)
	{
		bSkipNextSelectionFromTimer = false;
		return;
	}

	FControlRigEditMode* ControlRigEditMode = GetEditMode();
	UControlRig* ControlRig = nullptr;

	TArray<const IKeyArea*> KeyAreas;
	const bool UseSelectedKeys = CVarSelectedKeysSelectControls.GetValueOnGameThread();
	GetSequencer()->GetSelectedKeyAreas(KeyAreas, UseSelectedKeys);
	if (KeyAreas.Num() <= 0)
	{
		if (ControlRigEditMode)
		{
			TMap<UControlRig*, TArray<FRigElementKey>> AllSelectedControls;
			ControlRigEditMode->GetAllSelectedControls(AllSelectedControls);
			for (TPair<UControlRig*, TArray<FRigElementKey>>& SelectedControl : AllSelectedControls)
			{
				ControlRig = SelectedControl.Key;
				if (ControlRig && ControlRig->CurrentControlSelection().Num() > 0)
				{
					FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), !GIsTransacting);
					ControlRig->ClearControlSelection();
				}
			}
		}
		for (UMovieSceneTrack* Track : InTracks)
		{
			UMovieSceneControlRigParameterTrack* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track);
			if (CRTrack)
			{
				UControlRig* TrackControlRig = CRTrack->GetControlRig();
				if (TrackControlRig)
				{
					if (ControlRigEditMode)
					{
						ControlRigEditMode->AddControlRigObject(TrackControlRig, GetSequencer());
						ControlRigEditMode->RequestToRecreateControlShapeActors(TrackControlRig);
						break;
					}
					else
					{
						ControlRigEditMode = GetEditMode(true);
						if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = TrackControlRig->GetObjectBinding())
						{
							if (ControlRigEditMode)
							{
								ControlRigEditMode->AddControlRigObject(TrackControlRig, GetSequencer());
								ControlRigEditMode->RequestToRecreateControlShapeActors(TrackControlRig);
							}
						}
					}
				}
			}
		}
		return;
	}
	SelectRigsAndControls(ControlRig, KeyAreas);
	
}

void FControlRigParameterTrackEditor::SelectRigsAndControls(UControlRig* ControlRig, const TArray<const IKeyArea*>& KeyAreas)
{
	FControlRigEditMode* ControlRigEditMode = GetEditMode();

	TArray<FString> StringArray;
	//we have two sets here one to see if selection has really changed that contains the attirbutes, the other to select just the parent
	TMap<UControlRig*, TSet<FName>> RigsAndControls;
	for (const IKeyArea* KeyArea : KeyAreas)
	{
		UMovieSceneControlRigParameterSection* MovieSection = Cast<UMovieSceneControlRigParameterSection>(KeyArea->GetOwningSection());
		if (MovieSection)
		{
			ControlRig = MovieSection->GetControlRig();
			//Only create the edit mode if we have a KeyAra selected and it's not set and we have some boundobjects.
			if (!ControlRigEditMode)
			{
				ControlRigEditMode = GetEditMode(true);
				if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
				{
					if (ControlRigEditMode)
					{
						ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer());
					}
				}
			}
			else
			{
				if (ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer()))
				{
					//force an evaluation, this will get the control rig setup so edit mode looks good.
					if (GetSequencer().IsValid())
					{
						GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::Unknown);
					}
				}
			}

			const FMovieSceneChannelMetaData* MetaData = KeyArea->GetChannel().GetMetaData();
			if (MetaData)
			{
				StringArray.SetNum(0);
				FString String = MetaData->Name.ToString();
				String.ParseIntoArray(StringArray, TEXT("."));
				if (StringArray.Num() > 0)
				{
					const FName ControlName(*StringArray[0]);

					// skip nested controls which have the shape enabled flag turned on
					if(const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
					{

						if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(FRigElementKey(ControlName, ERigElementType::Control)))
						{
						
							if (ControlElement->Settings.ControlType == ERigControlType::Bool ||
								ControlElement->Settings.ControlType == ERigControlType::Float ||
								ControlElement->Settings.ControlType == ERigControlType::Integer)
							{
								if (ControlElement->Settings.SupportsShape() || !Hierarchy->IsAnimatable(ControlElement))
								{

									if (const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(Hierarchy->GetFirstParent(ControlElement)))
									{
										if (const TSet<FName>* Controls = RigsAndControls.Find(ControlRig))
										{
											if (Controls->Contains(ParentControlElement->GetName()))
											{
												continue;
											}
										}
									}
								}
							}
							RigsAndControls.FindOrAdd(ControlRig).Add(ControlName);
						}
					}
				}
			}
		}
	}
	//only create transaction if selection is really different.
	bool bEndTransaction = false;
	
	TMap<UControlRig*, TArray<FName>> ControlRigsToClearSelection;
	//get current selection which we will clear if different
	if (ControlRigEditMode)
	{
		TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
		ControlRigEditMode->GetAllSelectedControls(SelectedControls);
		for (TPair<UControlRig*, TArray<FRigElementKey>>& Selection : SelectedControls)
		{
			ControlRig = Selection.Key;
			if (ControlRig)
			{
				TArray<FName> SelectedControlNames = ControlRig->CurrentControlSelection();
				ControlRigsToClearSelection.Add(ControlRig, SelectedControlNames);
			}
		}
	}

	for (TPair<UControlRig*, TSet<FName>>& Pair : RigsAndControls)
	{
		//check to see if new selection is same als old selection
		bool bIsSame = true;
		if (TArray<FName>* SelectedNames = ControlRigsToClearSelection.Find(Pair.Key))
		{
			TSet<FName>* FullNames = RigsAndControls.Find(Pair.Key);
			if (!FullNames)
			{
				continue; // should never happen
			}
			if (SelectedNames->Num() != FullNames->Num())
			{ 
				bIsSame = false;
				if (!GIsTransacting && bEndTransaction == false)
				{
					bEndTransaction = true;
					GEditor->BeginTransaction(LOCTEXT("SelectControl", "Select Control"));
				}
				Pair.Key->ClearControlSelection();
				ControlRigsToClearSelection.Remove(Pair.Key); //remove it
			}
			else//okay if same check and see if equal...
			{
				for (const FName& Name : (*SelectedNames))
				{
					if (FullNames->Contains(Name) == false)
					{
						bIsSame = false;
						if (!GIsTransacting && bEndTransaction == false)
						{
							bEndTransaction = true;
							GEditor->BeginTransaction(LOCTEXT("SelectControl", "Select Control"));
						}
						Pair.Key->ClearControlSelection();
						ControlRigsToClearSelection.Remove(Pair.Key); //remove it
						break; //break out
					}
				}
			}
			if (bIsSame == true)
			{
				ControlRigsToClearSelection.Remove(Pair.Key); //remove it
			}
		}
		else
		{
			bIsSame = false;
		}
		if (bIsSame == false)
		{
			for (const FName& Name : Pair.Value)
			{
				if (!GIsTransacting && bEndTransaction == false)
				{
					bEndTransaction = true;
					GEditor->BeginTransaction(LOCTEXT("SelectControl", "Select Control"));
				}
				Pair.Key->SelectControl(Name, true);
			}
		}
	}
	//go through and clear those still not cleared
	for (TPair<UControlRig*, TArray<FName>>& SelectedPairs : ControlRigsToClearSelection)
	{
		if (!GIsTransacting && bEndTransaction == false)
		{
			bEndTransaction = true;
			GEditor->BeginTransaction(LOCTEXT("SelectControl", "Select Control"));
		}
		SelectedPairs.Key->ClearControlSelection();
	}

	if (bEndTransaction)
	{
		GEditor->EndTransaction();
	}
}


FMovieSceneTrackEditor::FFindOrCreateHandleResult FControlRigParameterTrackEditor::FindOrCreateHandleToSceneCompOrOwner(USceneComponent* InComp)
{
	const bool bCreateHandleIfMissing = false;
	FName CreatedFolderName = NAME_None;

	FFindOrCreateHandleResult Result;
	bool bHandleWasValid = GetSequencer()->GetHandleToObject(InComp, bCreateHandleIfMissing).IsValid();

	Result.Handle = GetSequencer()->GetHandleToObject(InComp, bCreateHandleIfMissing, CreatedFolderName);
	Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

	// Prioritize a control rig parameter track on this component
	if (Result.Handle.IsValid())
	{
		if (MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Result.Handle, NAME_None))
		{
			return Result;
		}
	}

	// If the owner has a control rig parameter track, let's use it
	UObject* OwnerObject = InComp->GetOwner();
	FGuid OwnerHandle = GetSequencer()->GetHandleToObject(OwnerObject, bCreateHandleIfMissing);
	bHandleWasValid = OwnerHandle.IsValid();
	if (OwnerHandle.IsValid())
	{
		if (MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), OwnerHandle, NAME_None))
		{
			Result.Handle = OwnerHandle;
			Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
			return Result;
		}
	}

	// If the component handle doesn't exist, let's use the owner handle
	if (Result.Handle.IsValid() == false)
	{
		Result.Handle = OwnerHandle;
		Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();

	}
	return Result;
}

void FControlRigParameterTrackEditor::SelectSequencerNodeInSection(UMovieSceneControlRigParameterSection* ParamSection, const FName& ControlName, bool bSelected)
{
	if (ParamSection)
	{
		FChannelMapInfo* pChannelIndex = ParamSection->ControlChannelMap.Find(ControlName);
		if (pChannelIndex != nullptr)
		{
			if (pChannelIndex->ParentControlIndex == INDEX_NONE)
			{
				int32 CategoryIndex = ParamSection->GetActiveCategoryIndex(ControlName);
				if (CategoryIndex != INDEX_NONE)
				{
					GetSequencer()->SelectByNthCategoryNode(ParamSection, CategoryIndex, bSelected);
				}
			}
			else
			{
				const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();

				FMovieSceneChannelProxy& ChannelProxy = ParamSection->GetChannelProxy();
				for (const FMovieSceneChannelEntry& Entry : ParamSection->GetChannelProxy().GetAllEntries())
				{
					const FName ChannelTypeName = Entry.GetChannelTypeName();
					if (pChannelIndex->ChannelTypeName == ChannelTypeName || (ChannelTypeName == FloatChannelTypeName && pChannelIndex->ChannelTypeName == NAME_None))
					{
						FMovieSceneChannelHandle Channel = ChannelProxy.MakeHandle(ChannelTypeName, pChannelIndex->ChannelIndex);
						TArray<FMovieSceneChannelHandle> Channels;
						Channels.Add(Channel);
						GetSequencer()->SelectByChannels(ParamSection, Channels, false, bSelected);
						break;
					}
				}
			}
		}
	}
}

FMovieSceneTrackEditor::FFindOrCreateTrackResult FControlRigParameterTrackEditor::FindOrCreateControlRigTrackForObject(FGuid ObjectHandle, UControlRig* ControlRig, FName PropertyName, bool bCreateTrackIfMissing)
{
	FFindOrCreateTrackResult Result;
	bool bTrackExisted = false;

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

	if (FMovieSceneBinding* Binding = MovieScene->FindBinding(ObjectHandle))
	{
		for (UMovieSceneTrack* Track : Binding->GetTracks())
		{
			if (UMovieSceneControlRigParameterTrack* ControlRigParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
			{
				if (ControlRigParameterTrack->GetControlRig() == ControlRig)
				{
					Result.Track = ControlRigParameterTrack;
					bTrackExisted = true;
				}
			}
		}
	}

	if (!Result.Track && bCreateTrackIfMissing)
	{
		Result.Track = AddTrack(MovieScene, ObjectHandle, UMovieSceneControlRigParameterTrack::StaticClass(), PropertyName);
	}

	Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

	return Result;
}

UMovieSceneControlRigParameterTrack* FControlRigParameterTrackEditor::FindTrack(UControlRig* InControlRig)
{
	if (!GetSequencer().IsValid())
	{
		return nullptr;
	}
	
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (MovieScene)
	{
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
			for (UMovieSceneTrack* AnyOleTrack : Tracks)
			{
				UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
				if (Track && Track->GetControlRig() == InControlRig)
				{
					return Track;
				}
			}
		}
	}
	return nullptr;
}

void FControlRigParameterTrackEditor::HandleOnSpaceAdded(UMovieSceneControlRigParameterSection* Section, const FName& ControlName, FMovieSceneControlRigSpaceChannel* SpaceChannel)
{
	if (SpaceChannel)
	{
		if (!SpaceChannel->OnKeyMovedEvent().IsBound())
		{
			SpaceChannel->OnKeyMovedEvent().AddLambda([this, Section](FMovieSceneChannel* Channel, const  TArray<FKeyMoveEventItem>& MovedItems)
				{
					FMovieSceneControlRigSpaceChannel* SpaceChannel = static_cast<FMovieSceneControlRigSpaceChannel*>(Channel);
					HandleSpaceKeyMoved(Section, SpaceChannel, MovedItems);
				});
		}
		if (!SpaceChannel->OnKeyDeletedEvent().IsBound())
		{
			SpaceChannel->OnKeyDeletedEvent().AddLambda([this, Section](FMovieSceneChannel* Channel, const  TArray<FKeyAddOrDeleteEventItem>& Items)
				{
					FMovieSceneControlRigSpaceChannel* SpaceChannel = static_cast<FMovieSceneControlRigSpaceChannel*>(Channel);
					HandleSpaceKeyDeleted(Section, SpaceChannel, Items);
				});
		}
	}
	//todoo do we need to remove this or not mz
}

bool FControlRigParameterTrackEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjects) const
{
	SectionsGettingUndone.SetNum(0);
	// Check if we care about the undo/redo
	bool bGettingUndone = false;
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjects)
	{

		UObject* Object = TransactionObjectPair.Key;
		while (Object != nullptr)
		{
			const UClass* ObjectClass = Object->GetClass();
			if (ObjectClass && ObjectClass->IsChildOf(UMovieSceneControlRigParameterSection::StaticClass()))
			{
				UMovieSceneControlRigParameterSection* Section = Cast< UMovieSceneControlRigParameterSection>(Object);
				if (Section)
				{
					SectionsGettingUndone.Add(Section);
				}
				bGettingUndone =  true;
				break;
			}
			Object = Object->GetOuter();
		}
	}
	
	return bGettingUndone;
}

void FControlRigParameterTrackEditor::PostUndo(bool bSuccess)
{
	for (UMovieSceneControlRigParameterSection* Section : SectionsGettingUndone)
	{
		if (Section->GetControlRig())
		{
			TArray<FSpaceControlNameAndChannel>& SpaceChannels = Section->GetSpaceChannels();
			for(FSpaceControlNameAndChannel& Channel: SpaceChannels)
			{ 
				HandleOnSpaceAdded(Section, Channel.ControlName, &(Channel.SpaceCurve));
			}

			TArray<FConstraintAndActiveChannel>& ConstraintChannels = Section->GetConstraintsChannels();
			for (FConstraintAndActiveChannel& Channel: ConstraintChannels)
			{ 
				HandleOnConstraintAdded(Section, &(Channel.ActiveChannel));
			}
		}
	}
}


void FControlRigParameterTrackEditor::HandleSpaceKeyDeleted(
	UMovieSceneControlRigParameterSection* Section,
	FMovieSceneControlRigSpaceChannel* Channel,
	const TArray<FKeyAddOrDeleteEventItem>& DeletedItems) const
{
	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

	if (Section && Section->GetControlRig() && Channel && ParentSequencer.IsValid())
	{
		const FName ControlName = Section->FindControlNameFromSpaceChannel(Channel);
		for (const FKeyAddOrDeleteEventItem& EventItem : DeletedItems)
		{
			FControlRigSpaceChannelHelpers::SequencerSpaceChannelKeyDeleted(
				Section->GetControlRig(), ParentSequencer.Get(), ControlName, Channel, Section,EventItem.Frame);
		}
	}
}

void FControlRigParameterTrackEditor::HandleSpaceKeyMoved(
	UMovieSceneControlRigParameterSection* Section,
	FMovieSceneControlRigSpaceChannel* SpaceChannel,
	const  TArray<FKeyMoveEventItem>& MovedItems)
{
	if (Section && Section->GetControlRig() && SpaceChannel)
	{
		const FName ControlName = Section->FindControlNameFromSpaceChannel(SpaceChannel);
		for (const FKeyMoveEventItem& MoveEventItem : MovedItems)
		{
			FControlRigSpaceChannelHelpers::HandleSpaceKeyTimeChanged(
				Section->GetControlRig(), ControlName, SpaceChannel, Section,
				MoveEventItem.Frame, MoveEventItem.NewFrame);
		}
	}
}

void FControlRigParameterTrackEditor::ClearOutAllSpaceAndConstraintDelegates(const UControlRig* InOptionalControlRig) const
{
	const UMovieScene* MovieScene = GetSequencer().IsValid() && GetSequencer()->GetFocusedMovieSceneSequence() ? GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		const UMovieSceneTrack* Track = MovieScene->FindTrack(
			UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
		if (const UMovieSceneControlRigParameterTrack* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
		{
			if (InOptionalControlRig && CRTrack->GetControlRig() != InOptionalControlRig)
			{
				continue;
			}
				
			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section))
				{
					// clear space channels
					TArray<FSpaceControlNameAndChannel>& Channels = CRSection->GetSpaceChannels();
					for (FSpaceControlNameAndChannel& SpaceAndChannel : Channels)
					{
						SpaceAndChannel.SpaceCurve.OnKeyMovedEvent().Clear();
						SpaceAndChannel.SpaceCurve.OnKeyDeletedEvent().Clear();
					}

					// clear constraint channels
					TArray<FConstraintAndActiveChannel>& ConstraintChannels = CRSection->GetConstraintsChannels();
					for (FConstraintAndActiveChannel& Channel: ConstraintChannels)
					{
						Channel.ActiveChannel.OnKeyMovedEvent().Clear();
						Channel.ActiveChannel.OnKeyDeletedEvent().Clear();							
					}

					if (CRSection->OnConstraintRemovedHandle.IsValid())
					{
						if (const UControlRig* ControlRig = CRSection->GetControlRig())
						{
							FConstraintsManagerController& Controller = FConstraintsManagerController::Get(ControlRig->GetWorld());
							Controller.GetNotifyDelegate().Remove(CRSection->OnConstraintRemovedHandle);
							CRSection->OnConstraintRemovedHandle.Reset();
						}
					}
				}
			}
		}
	}
}

namespace
{
	struct FConstraintAndControlData
	{
		static FConstraintAndControlData CreateFromSection(
			const UMovieSceneControlRigParameterSection* InSection,
			const FMovieSceneConstraintChannel* InConstraintChannel)
		{
			FConstraintAndControlData Data;
			
			// get constraint channel
			const TArray<FConstraintAndActiveChannel>& ConstraintChannels = InSection->GetConstraintsChannels();
			const int32 Index = ConstraintChannels.IndexOfByPredicate([InConstraintChannel](const FConstraintAndActiveChannel& InChannel)
			{
				return &(InChannel.ActiveChannel) == InConstraintChannel;
			});
	
			if (Index == INDEX_NONE)
			{
				return Data;
			}

			Data.Constraint = Cast<UTickableTransformConstraint>(ConstraintChannels[Index].Constraint.Get());

			// get constraint name
			auto GetControlName = [InSection, Index]()
			{
				using NameInfoIterator = TMap<FName, FChannelMapInfo>::TRangedForConstIterator;
				for (NameInfoIterator It = InSection->ControlChannelMap.begin(); It; ++It)
				{
					const FChannelMapInfo& Info = It->Value;
					if (Info.ConstraintsIndex.Contains(Index))
					{
						return It->Key;
					}
				}

				static const FName DummyName = NAME_None;
				return DummyName;
			};
	
			Data.ControlName = GetControlName();
			
			return Data;
		}

		bool IsValid() const
		{
			return Constraint.IsValid() && ControlName != NAME_None; 
		}
		
		TWeakObjectPtr<UTickableTransformConstraint> Constraint = nullptr;
		FName ControlName = NAME_None;
	};
}

void FControlRigParameterTrackEditor::HandleOnConstraintAdded(
	IMovieSceneConstrainedSection* InSection,
	FMovieSceneConstraintChannel* InConstraintChannel)
{
	if (!InConstraintChannel)
	{
		return;
	}

	// handle key moved
	if (!InConstraintChannel->OnKeyMovedEvent().IsBound())
	{
		InConstraintChannel->OnKeyMovedEvent().AddLambda([this, InSection](
			FMovieSceneChannel* InChannel, const TArray<FKeyMoveEventItem>& InMovedItems)
			{
				const FMovieSceneConstraintChannel* ConstraintChannel = static_cast<FMovieSceneConstraintChannel*>(InChannel);
				HandleConstraintKeyMoved(InSection, ConstraintChannel, InMovedItems);
			});
	}

	// handle key deleted
	if (!InConstraintChannel->OnKeyDeletedEvent().IsBound())
	{
		InConstraintChannel->OnKeyDeletedEvent().AddLambda([this, InSection](
			FMovieSceneChannel* InChannel, const TArray<FKeyAddOrDeleteEventItem>& InDeletedItems)
			{
				const FMovieSceneConstraintChannel* ConstraintChannel = static_cast<FMovieSceneConstraintChannel*>(InChannel);
				HandleConstraintKeyDeleted(InSection, ConstraintChannel, InDeletedItems);
			});
	}

	// handle constraint deleted
	if (InSection)
	{
		HandleConstraintRemoved(InSection);
	}
}

void FControlRigParameterTrackEditor::HandleConstraintKeyDeleted(
	IMovieSceneConstrainedSection* InSection,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	const TArray<FKeyAddOrDeleteEventItem>& InDeletedItems) const
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(InSection);

	const UControlRig* ControlRig = Section ? Section->GetControlRig() : nullptr;
	if (!ControlRig || !InConstraintChannel)
	{
		return;
	}
	
	const FConstraintAndControlData ConstraintAndControlData =
		FConstraintAndControlData::CreateFromSection(Section, InConstraintChannel);
	if (ConstraintAndControlData.IsValid())
	{

		UTickableTransformConstraint* Constraint = ConstraintAndControlData.Constraint.Get();
		for (const FKeyAddOrDeleteEventItem& EventItem: InDeletedItems)
		{
			FMovieSceneConstraintChannelHelper::HandleConstraintKeyDeleted(
				Constraint, InConstraintChannel,
				GetSequencer(), Section,
				EventItem.Frame);
		}
	}
}

void FControlRigParameterTrackEditor::HandleConstraintKeyMoved(
	IMovieSceneConstrainedSection* InSection,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	const TArray<FKeyMoveEventItem>& InMovedItems)
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(InSection);

	const FConstraintAndControlData ConstraintAndControlData =
	FConstraintAndControlData::CreateFromSection(Section, InConstraintChannel);

	if (ConstraintAndControlData.IsValid())
	{
		const UTickableTransformConstraint* Constraint = ConstraintAndControlData.Constraint.Get();
		for (const FKeyMoveEventItem& MoveEventItem : InMovedItems)
		{
			FMovieSceneConstraintChannelHelper::HandleConstraintKeyMoved(
				Constraint, InConstraintChannel, Section,
				MoveEventItem.Frame, MoveEventItem.NewFrame);
		}
	}
}

void FControlRigParameterTrackEditor::HandleConstraintRemoved(IMovieSceneConstrainedSection* InSection) 
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(InSection);

	if (const UControlRig* ControlRig = Section->GetControlRig())
	{
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(ControlRig->GetWorld());
		if (!InSection->OnConstraintRemovedHandle.IsValid())
		{
			InSection->OnConstraintRemovedHandle =
			Controller.GetNotifyDelegate().AddLambda(
				[InSection,Section, this](EConstraintsManagerNotifyType InNotifyType, UObject *InObject)
			{
				switch (InNotifyType)
				{
					case EConstraintsManagerNotifyType::ConstraintAdded:
						break;
					case EConstraintsManagerNotifyType::ConstraintRemoved:
					case EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation:
						{
							const UTickableConstraint* Constraint = Cast<UTickableConstraint>(InObject);
							if (!IsValid(Constraint))
							{
								return;
							}

							const FName ConstraintName = Constraint->GetFName();
							const FConstraintAndActiveChannel* ConstraintChannel = InSection->GetConstraintChannel(ConstraintName);
							if (!ConstraintChannel)
							{
								return;
							}

							const bool bCompensate = (InNotifyType == EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation);
							if (bCompensate && ConstraintChannel->Constraint.IsValid())
							{
								FMovieSceneConstraintChannelHelper::HandleConstraintRemoved(
									ConstraintChannel->Constraint.Get(),
									&ConstraintChannel->ActiveChannel,
									GetSequencer(),
									Section);
							}

							InSection->RemoveConstraintChannel(ConstraintName);
						}
						break;
					case EConstraintsManagerNotifyType::ManagerUpdated:
						InSection->OnConstraintsChanged();
						break;		
				}
			});
			ConstraintHandlesToClear.Add(InSection->OnConstraintRemovedHandle);
		}
	}
}

void FControlRigParameterTrackEditor::SetUpEditModeIfNeeded(UControlRig* ControlRig)
{
	if (ControlRig)
	{
		//this could clear the selection so if it does reset it
		TArray<FName> ControlRigSelection = ControlRig->CurrentControlSelection();

		FControlRigEditMode* ControlRigEditMode = GetEditMode();
		if (!ControlRigEditMode)
		{
			ControlRigEditMode = GetEditMode(true);
			if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
			{
				if (ControlRigEditMode)
				{
					ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer());
				}
			}
		}
		else
		{
			if (ControlRigEditMode->AddControlRigObject(ControlRig, GetSequencer()))
			{
				//force an evaluation, this will get the control rig setup so edit mode looks good.
				if (GetSequencer().IsValid())
				{
					GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::Unknown);
				}
			}
		}
		TArray<FName> NewControlRigSelection = ControlRig->CurrentControlSelection();
		if (ControlRigSelection.Num() != NewControlRigSelection.Num())
		{
			ControlRig->ClearControlSelection();
			for (const FName& Name : ControlRigSelection)
			{
				ControlRig->SelectControl(Name, true);
			}
		}
	}
}

void FControlRigParameterTrackEditor::HandleControlSelected(UControlRig* Subject, FRigControlElement* ControlElement, bool bSelected)
{
	if(ControlElement == nullptr)
	{
		return;
	}
	
	URigHierarchy* Hierarchy = Subject->GetHierarchy();
	static bool bIsSelectingIndirectControl = false;

	if(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
	{
		const TArray<FRigElementKey>& DrivenControls = ControlElement->Settings.DrivenControls;
		for(const FRigElementKey& DrivenKey : DrivenControls)
		{
			if(FRigControlElement* DrivenControl = Hierarchy->Find<FRigControlElement>(DrivenKey))
			{
				TGuardValue<bool> SubControlGuard(bIsSelectingIndirectControl, true);
				HandleControlSelected(Subject, DrivenControl, bSelected);
			}
		}
		return;
	}
	
	//if parent selected we select child here if it's a bool,integer or single float
	TArray<FRigControl> Controls;

	if(!bIsSelectingIndirectControl)
	{
		if (URigHierarchyController* Controller = Hierarchy->GetController())
		{
			Hierarchy->ForEach<FRigControlElement>([Hierarchy, ControlElement, Controller, bSelected](FRigControlElement* OtherControlElement) -> bool
				{
					if (OtherControlElement->Settings.ControlType == ERigControlType::Bool ||
						OtherControlElement->Settings.ControlType == ERigControlType::Float ||
						OtherControlElement->Settings.ControlType == ERigControlType::Integer)
					{
						if(OtherControlElement->Settings.SupportsShape() || !Hierarchy->IsAnimatable(OtherControlElement))
						{
							return true;
						}
						
						for (const FRigElementParentConstraint& ParentConstraint : OtherControlElement->ParentConstraints)
						{
							if (ParentConstraint.ParentElement == ControlElement)
							{
								Controller->SelectElement(OtherControlElement->GetKey(), bSelected);
								break;
							}
						}
					}

					return true;
				});
		}
	}
	
	if (bIsDoingSelection)
	{
		return;
	}
	TGuardValue<bool> Guard(bIsDoingSelection, true);

	FControlRigEditMode* ControlRigEditMode = GetEditMode();

	FName ControlRigName(*Subject->GetName());
	if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = Subject->GetObjectBinding())
	{
		UObject* ActorObject = nullptr;
		USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
		if (!Component)
		{
			return;
		}
		ActorObject = Component->GetOwner();
		bool bCreateTrack = false;
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToSceneCompOrOwner(Component);
		FGuid ObjectHandle = HandleResult.Handle;
		if (!ObjectHandle.IsValid())
		{
			return;
		}

		FFindOrCreateTrackResult TrackResult = FindOrCreateControlRigTrackForObject(ObjectHandle, Subject, ControlRigName, bCreateTrack);
		UMovieSceneControlRigParameterTrack* Track = CastChecked<UMovieSceneControlRigParameterTrack>(TrackResult.Track, ECastCheckedType::NullAllowed);
		if (Track)
		{
			GetSequencer()->SuspendSelectionBroadcast();
			//Just set in the section to key not all
			UMovieSceneSection* Section = Track->GetSectionToKey();
			UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(Section);
			SelectSequencerNodeInSection(ParamSection, ControlElement->GetName(), bSelected);
			
			GetSequencer()->ResumeSelectionBroadcast();

			SetUpEditModeIfNeeded(Subject);

			//Force refresh later, not now
			bSkipNextSelectionFromTimer = bSkipNextSelectionFromTimer ||
				(bIsSelectingIndirectControl && ControlElement->Settings.AnimationType == ERigControlAnimationType::AnimationControl);
			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshTree);
		}
	}
}



void FControlRigParameterTrackEditor::HandleOnInitialized(UControlRig* ControlRig, const EControlRigState InState, const FName& InEventName)
{
	if (GetSequencer().IsValid())
	{
		//If FK control rig on next tick we refresh the tree
		if (ControlRig->IsA<UFKControlRig>())
		{
			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshTree);
		}
	}
	//also do to new procedural rigs, we may be creating controls dynamically, so here we need to check to see if we need to reconstruct channels
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig() == ControlRig)
		{
			TArray<FRigControlElement*> SortedControls;
			ControlRig->GetControlsInOrder(SortedControls);
			for (UMovieSceneSection* BaseSection : Track->GetAllSections())
			{
				if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(BaseSection))
				{
					if (Section->IsDifferentThanLastControlsUsedToReconstruct(SortedControls))
					{
						Section->ReconstructChannelProxy();
						Section->MarkAsChanged();
					}
				}
			}
		}
	}
}

void FControlRigParameterTrackEditor::HandleControlModified(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if(ControlElement == nullptr)
	{
		return;
	}
	if (IsInGameThread() == false)
	{
		return;
	}
	if (!GetSequencer().IsValid() || !GetSequencer()->IsAllowedToChange() || Context.SetKey == EControlRigSetKey::Never 
		|| ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl
		|| ControlElement->Settings.AnimationType == ERigControlAnimationType::VisualCue)
	{
		return;
	}
	FTransform  Transform = ControlRig->GetControlLocalTransform(ControlElement->GetName());
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig() == ControlRig)
		{
			FName Name(*ControlRig->GetName());
			if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
			{
				USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
				if (Component)
				{
					ESequencerKeyMode KeyMode = ESequencerKeyMode::AutoKey;
					if (Context.SetKey == EControlRigSetKey::Always)
					{
						KeyMode = ESequencerKeyMode::ManualKeyForced;
					}
					AddControlKeys(Component, ControlRig, Name, ControlElement->GetName(), (EControlRigContextChannelToKey)Context.KeyMask, 
						KeyMode, Context.LocalTime);
				}
			}
		}
	}
}

void FControlRigParameterTrackEditor::HandleControlUndoBracket(UControlRig* Subject, bool bOpenUndoBracket)
{
	if(bOpenUndoBracket && ControlUndoBracket == 0)
	{
		ControlUndoTransaction = MakeShareable(new FScopedTransaction(LOCTEXT("KeyMultipleControls", "Auto-Key multiple controls")));
	}

	ControlUndoBracket = FMath::Max<int32>(0, ControlUndoBracket + (bOpenUndoBracket ? 1 : -1));
	
	if(!bOpenUndoBracket && ControlUndoBracket == 0)
	{
		ControlUndoTransaction.Reset();
	}
}

void FControlRigParameterTrackEditor::HandleOnControlRigBound(UControlRig* InControlRig)
{
	if (!InControlRig)
	{
		return;
	}
	
	UMovieSceneControlRigParameterTrack* Track = FindTrack(InControlRig);
	if (!Track)
	{
		return;
	}

	const TSharedPtr<IControlRigObjectBinding> Binding = InControlRig->GetObjectBinding();
	
	for (UMovieSceneSection* BaseSection : Track->GetAllSections())
	{
		if (UMovieSceneControlRigParameterSection* Section = Cast< UMovieSceneControlRigParameterSection>(BaseSection))
		{
			const UControlRig* ControlRig = Section->GetControlRig();
			if (ControlRig && InControlRig == ControlRig)
			{
				if (!Binding->OnControlRigBind().IsBoundToObject(this))
				{
					Binding->OnControlRigBind().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnObjectBoundToControlRig);
				}
			}
		}
	}
}

void FControlRigParameterTrackEditor::HandleOnObjectBoundToControlRig(UObject* InObject)
{
	// look for sections to update
	TArray<UMovieSceneControlRigParameterSection*> SectionsToUpdate;
	for (const TWeakObjectPtr<UControlRig>& ControlRigPtr : BoundControlRigs)
	{
		const TSharedPtr<IControlRigObjectBinding> Binding =
			ControlRigPtr.IsValid() ? ControlRigPtr->GetObjectBinding() : nullptr;
		const UObject* CurrentObject = Binding ? Binding->GetBoundObject() : nullptr;
		if (CurrentObject == InObject)
		{
			if (const UMovieSceneControlRigParameterTrack* Track = FindTrack(ControlRigPtr.Get()))
			{
				for (UMovieSceneSection* BaseSection : Track->GetAllSections())
				{
					if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(BaseSection))
					{
						SectionsToUpdate.AddUnique(Section);
					}
				}
			}
		}
	}

	// reconstruct proxies
	if (!SectionsToUpdate.IsEmpty())
	{
		for (UMovieSceneControlRigParameterSection* Section: SectionsToUpdate)
		{
			Section->ReconstructChannelProxy();
			Section->MarkAsChanged();
		}
	}
}

void FControlRigParameterTrackEditor::GetControlRigKeys(
	UControlRig* InControlRig,
	FName ParameterName,
	EControlRigContextChannelToKey ChannelsToKey,
	ESequencerKeyMode KeyMode,
	UMovieSceneControlRigParameterSection* SectionToKey,
	FGeneratedTrackKeys& OutGeneratedKeys,
	const bool bInConstraintSpace)
{
	const TArray<bool>& ControlsMask = SectionToKey->GetControlsMask();
	EMovieSceneTransformChannel TransformMask = SectionToKey->GetTransformMask().GetChannels();

	TArray<FRigControlElement*> Controls;
	InControlRig->GetControlsInOrder(Controls);
	// If key all is enabled, for a key on all the channels
	if (KeyMode != ESequencerKeyMode::ManualKeyForced && GetSequencer()->GetKeyGroupMode() == EKeyGroupMode::KeyAll)
	{
		ChannelsToKey = EControlRigContextChannelToKey::AllTransform;
	}

	//Need seperate index fo bools,ints and enums and floats since there are seperate entries for each later when they are accessed by the set key stuff.
	int32 SpaceChannelIndex = 0;
	for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ++ControlIndex)
	{
		FRigControlElement* ControlElement = Controls[ControlIndex];
		check(ControlElement);

		if (!InControlRig->GetHierarchy()->IsAnimatable(ControlElement))
		{
			continue;
		}

		if (FChannelMapInfo* pChannelIndex = SectionToKey->ControlChannelMap.Find(ControlElement->GetName()))
		{
			int32 ChannelIndex = pChannelIndex->ChannelIndex;


			bool bMaskKeyOut = (ControlIndex >= ControlsMask.Num() || ControlsMask[ControlIndex] == false);
			bool bSetKey = ControlElement->GetName() == ParameterName && !bMaskKeyOut;

			FRigControlValue ControlValue = InControlRig->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current);

			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Bool:
			{
				bool Val = ControlValue.Get<bool>();
				pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneBoolChannel>(ChannelIndex, Val, bSetKey));
				break;
			}
			case ERigControlType::Integer:
			{
				if (ControlElement->Settings.ControlEnum)
				{
					uint8 Val = (uint8)ControlValue.Get<uint8>();
					pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneByteChannel>(ChannelIndex, Val, bSetKey));
				}
				else
				{
					int32 Val = ControlValue.Get<int32>();
					pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneIntegerChannel>(ChannelIndex, Val, bSetKey));
				}
				break;
			}
			case ERigControlType::Float:
			{
				float Val = ControlValue.Get<float>();
				pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex, Val, bSetKey));
				break;
			}
			case ERigControlType::Vector2D:
			{
				//use translation x,y for key masks for vector2d
				bool bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationX);;
				bool bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationY);;
				FVector3f Val = ControlValue.Get<FVector3f>();
				pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, Val.X, bKeyX));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, Val.Y, bKeyY));
				break;
			}
			case ERigControlType::Position:
			case ERigControlType::Scale:
			case ERigControlType::Rotator:
			{
				bool bKeyX = bSetKey;
				bool bKeyY = bSetKey;
				bool bKeyZ = bSetKey;
				if (ControlElement->Settings.ControlType == ERigControlType::Position)
				{
					bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationX);
					bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationY);
					bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationZ);
				}
				else if(ControlElement->Settings.ControlType == ERigControlType::Rotator)
				{
					bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationX);
					bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationY);
					bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationZ);
				}
				else //scale
				{
					bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleX);
					bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleY);
					bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleZ);
				}
				
				FVector3f Val = ControlValue.Get<FVector3f>();
				pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, Val.X, bKeyX));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, Val.Y, bKeyY));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, Val.Z, bKeyZ));
				break;
			}

			case ERigControlType::Transform:
			case ERigControlType::TransformNoScale:
			case ERigControlType::EulerTransform:
			{
				FVector Translation, Scale(1.0f, 1.0f, 1.0f);
				FRotator Rotation = InControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);

				if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
				{
					FTransformNoScale NoScale = ControlValue.Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
					Translation = NoScale.Location;
				}
				else if (ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
				{
					FEulerTransform Euler = ControlValue.Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
					Translation = Euler.Location;
					Scale = Euler.Scale;
				}
				else
				{
					FTransform Val = ControlValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
					Translation = Val.GetTranslation();
					Scale = Val.GetScale3D();
				}

				// switch values to constraint space?
				if (bInConstraintSpace)
				{
					const uint32 ControlHash = UTransformableControlHandle::ComputeHash(InControlRig, ControlElement->GetName());
					TOptional<FTransform> Transform = FTransformConstraintUtils::GetRelativeTransform(InControlRig->GetWorld(), ControlHash);
					if (Transform)
					{
						Translation = Transform->GetTranslation();
						Rotation = Transform->GetRotation().Rotator();
						Scale = Transform->GetScale3D();
					}
				}
					
				FVector3f CurrentVector = (FVector3f)Translation;
				bool bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationX);
				bool bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationY);
				bool bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationZ);
				if (GetSequencer()->GetKeyGroupMode() == EKeyGroupMode::KeyGroup && (bKeyX || bKeyY || bKeyZ))
				{
					bKeyX = bKeyY = bKeyZ = true;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::TranslationX))
				{
					bKeyX = false;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::TranslationY))
				{
					bKeyY = false;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::TranslationZ))
				{
					bKeyZ = false;
				}

				pChannelIndex->GeneratedKeyIndex = OutGeneratedKeys.Num();

				if (pChannelIndex->bDoesHaveSpace)
				{
					//for some saved dev files this could be -1 so we used the local incremented value which is almost always safe, if not a resave will fix the file.
					FMovieSceneControlRigSpaceBaseKey NewKey;
					int32 RealSpaceChannelIndex = pChannelIndex->SpaceChannelIndex != -1 ? pChannelIndex->SpaceChannelIndex : SpaceChannelIndex;
					++SpaceChannelIndex;
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneControlRigSpaceChannel>(RealSpaceChannelIndex, NewKey, false));
				}


				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.X, bKeyX));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.Y, bKeyY));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.Z, bKeyZ));

				FRotator3f CurrentRotator = FRotator3f(Rotation);
				bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationX);
				bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationY);
				bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationZ);
				if (GetSequencer()->GetKeyGroupMode() == EKeyGroupMode::KeyGroup && (bKeyX || bKeyY || bKeyZ))
				{
					bKeyX = bKeyY = bKeyZ = true;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::RotationX))
				{
					bKeyX = false;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::RotationY))
				{
					bKeyY = false;
				}
				if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::RotationZ))
				{
					bKeyZ = false;
				}

				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentRotator.Roll, bKeyX));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentRotator.Pitch, bKeyY));
				OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentRotator.Yaw, bKeyZ));

				if (ControlElement->Settings.ControlType == ERigControlType::Transform || ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
				{
					CurrentVector = (FVector3f)Scale;
					bKeyX = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleX);
					bKeyY = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleY);
					bKeyZ = bSetKey && EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleZ);
					if (GetSequencer()->GetKeyGroupMode() == EKeyGroupMode::KeyGroup && (bKeyX || bKeyY || bKeyZ))
					{
						bKeyX = bKeyY = bKeyZ = true;
					}
					if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::ScaleX))
					{
						bKeyX = false;
					}
					if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::ScaleY))
					{
						bKeyY = false;
					}
					if (!EnumHasAnyFlags(TransformMask, EMovieSceneTransformChannel::ScaleZ))
					{
						bKeyZ = false;
					}
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.X, bKeyX));
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.Y, bKeyY));
					OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(ChannelIndex++, CurrentVector.Z, bKeyZ));
				}
				break;
			}
		}
		}
	}
}

FKeyPropertyResult FControlRigParameterTrackEditor::AddKeysToControlRigHandle(USceneComponent* InSceneComp, UControlRig* InControlRig,
	FGuid ObjectHandle, FFrameNumber KeyTime, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedKeys,
	ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName ControlRigName, FName RigControlName)
{
	EAutoChangeMode AutoChangeMode = GetSequencer()->GetAutoChangeMode();
	EAllowEditsMode AllowEditsMode = GetSequencer()->GetAllowEditsMode();

	bool bCreateTrack =
		(KeyMode == ESequencerKeyMode::AutoKey && (AutoChangeMode == EAutoChangeMode::AutoTrack || AutoChangeMode == EAutoChangeMode::All)) ||
		KeyMode == ESequencerKeyMode::ManualKey ||
		KeyMode == ESequencerKeyMode::ManualKeyForced ||
		AllowEditsMode == EAllowEditsMode::AllowSequencerEditsOnly;

	bool bCreateSection = false;
	// we don't do this, maybe revisit if a bug occurs, but currently extends sections on autokey.
	//bCreateTrack || (KeyMode == ESequencerKeyMode::AutoKey && (AutoChangeMode != EAutoChangeMode::None));

	// Try to find an existing Track, and if one doesn't exist check the key params and create one if requested.

	FFindOrCreateTrackResult TrackResult = FindOrCreateControlRigTrackForObject(ObjectHandle, InControlRig, ControlRigName, bCreateTrack);
	UMovieSceneControlRigParameterTrack* Track = CastChecked<UMovieSceneControlRigParameterTrack>(TrackResult.Track, ECastCheckedType::NullAllowed);

	bool bTrackCreated = TrackResult.bWasCreated;

	bool bSectionCreated = false;
	FKeyPropertyResult KeyPropertyResult;

	if (Track)
	{
		float Weight = 1.0f;

		UMovieSceneSection* SectionToKey = bCreateSection ? Track->FindOrExtendSection(KeyTime, Weight) : Track->FindSection(KeyTime);

		// If there's no overlapping section to key, create one only if a track was newly created. Otherwise, skip keying altogether
		// so that the user is forced to create a section to key on.
		if (bTrackCreated && !SectionToKey)
		{
			Track->Modify();
			SectionToKey = Track->FindOrAddSection(KeyTime, bSectionCreated);
			if (bSectionCreated && GetSequencer()->GetInfiniteKeyAreas())
			{
				SectionToKey->SetRange(TRange<FFrameNumber>::All());
			}
		}

		if (SectionToKey && SectionToKey->GetRange().Contains(KeyTime))
		{
			if (!bTrackCreated)
			{
				ModifyOurGeneratedKeysByCurrentAndWeight(InSceneComp, InControlRig, RigControlName, Track, SectionToKey, EvaluateTime, GeneratedKeys, Weight);
			}
			UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(SectionToKey);
			if (!ParamSection->GetDoNotKey())
			{
				KeyPropertyResult |= AddKeysToSection(SectionToKey, KeyTime, GeneratedKeys, KeyMode, FKeyframeTrackEditor::ESetDefault::DoNotSetDefault);
			}
		}


		KeyPropertyResult.bTrackCreated |= bTrackCreated || bSectionCreated;
		//if we create a key then compensate
		if (KeyPropertyResult.bKeyCreated)
		{
			UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());
			if (UControlRig* SectionControlRig = ParamSection ? ParamSection->GetControlRig() : nullptr)
			{
				TOptional<FFrameNumber> OptionalKeyTime = KeyTime;

				// compensate spaces
				FControlRigSpaceChannelHelpers::CompensateIfNeeded(
					SectionControlRig, GetSequencer().Get(), ParamSection,
					RigControlName, OptionalKeyTime);

				// compensate constraints
				FConstraintChannelHelper::CompensateIfNeeded(
					SectionControlRig->GetWorld(), GetSequencer(), ParamSection, OptionalKeyTime);
			}
		}
	}
	return KeyPropertyResult;
}

FKeyPropertyResult FControlRigParameterTrackEditor::AddKeysToControlRig(
	USceneComponent* InSceneComp, UControlRig* InControlRig, FFrameNumber KeyTime, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedKeys,
	ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName ControlRigName, FName RigControlName)
{
	FKeyPropertyResult KeyPropertyResult;
	EAutoChangeMode AutoChangeMode = GetSequencer()->GetAutoChangeMode();
	EAllowEditsMode AllowEditsMode = GetSequencer()->GetAllowEditsMode();
	bool bCreateHandle =
		(KeyMode == ESequencerKeyMode::AutoKey && (AutoChangeMode == EAutoChangeMode::All)) ||
		KeyMode == ESequencerKeyMode::ManualKey ||
		KeyMode == ESequencerKeyMode::ManualKeyForced ||
		AllowEditsMode == EAllowEditsMode::AllowSequencerEditsOnly;

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToSceneCompOrOwner(InSceneComp);
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated = HandleResult.bWasCreated;
	if (ObjectHandle.IsValid())
	{
		KeyPropertyResult |= AddKeysToControlRigHandle(InSceneComp, InControlRig, ObjectHandle, KeyTime, EvaluateTime, GeneratedKeys, KeyMode, TrackClass, ControlRigName, RigControlName);
	}

	return KeyPropertyResult;
}

void FControlRigParameterTrackEditor::AddControlKeys(
	USceneComponent* InSceneComp,
	UControlRig* InControlRig,
	FName ControlRigName,
	FName RigControlName,
	EControlRigContextChannelToKey ChannelsToKey,
	ESequencerKeyMode KeyMode,
	float InLocalTime,
	const bool bInConstraintSpace)
{
	if (KeyMode == ESequencerKeyMode::ManualKey || (GetSequencer().IsValid() && !GetSequencer()->IsAllowedToChange()))
	{
		return;
	}
	bool bCreateTrack = false;
	bool bCreateHandle = false;
	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToSceneCompOrOwner(InSceneComp);
	FGuid ObjectHandle = HandleResult.Handle;
	if (!ObjectHandle.IsValid())
	{
		return;
	}
	FFindOrCreateTrackResult TrackResult = FindOrCreateControlRigTrackForObject(ObjectHandle, InControlRig, ControlRigName, bCreateTrack);
	UMovieSceneControlRigParameterTrack* Track = CastChecked<UMovieSceneControlRigParameterTrack>(TrackResult.Track, ECastCheckedType::NullAllowed);
	UMovieSceneControlRigParameterSection* ParamSection = nullptr;
	if (Track)
	{
		//track editors use a hidden time so we need to set it if we are using non sequencer times when keying.
		if (InLocalTime != FLT_MAX)
		{
			//convert from frame time since conversion may give us one frame less, e.g 1.53333330 * 24000.0/1.0 = 36799.999199999998
			FFrameTime LocalFrameTime = GetSequencer()->GetFocusedTickResolution().AsFrameTime((double)InLocalTime);
			BeginKeying(LocalFrameTime.RoundToFrame());
		}
		FFrameNumber  FrameTime = GetTimeForKey();
		UMovieSceneSection* Section = Track->FindSection(FrameTime);
		ParamSection = Cast<UMovieSceneControlRigParameterSection>(Section);

		if (ParamSection && ParamSection->GetDoNotKey())
		{
			return;
		}
	}

	if (!ParamSection)
	{
		return;
	}

	TSharedRef<FGeneratedTrackKeys> GeneratedKeys = MakeShared<FGeneratedTrackKeys>();
	GetControlRigKeys(InControlRig, RigControlName, ChannelsToKey, KeyMode, ParamSection, *GeneratedKeys, bInConstraintSpace);
	
	TGuardValue<bool> Guard(bIsDoingSelection, true);

	auto OnKeyProperty = [=](FFrameNumber Time) -> FKeyPropertyResult
	{
		FFrameNumber LocalTime = Time;
		//for modify weights we evaluate so need to make sure we use the evaluated time
		FFrameNumber EvaluateTime = GetSequencer()->GetLastEvaluatedLocalTime().RoundToFrame();
		//if InLocalTime is specified that means time value was set with SetControlValue, so we don't use sequencer times at all, but this time instead
		if (InLocalTime != FLT_MAX)
		{
			//convert from frame time since conversion may give us one frame less, e.g 1.53333330 * 24000.0/1.0 = 36799.999199999998
			FFrameTime LocalFrameTime = GetSequencer()->GetFocusedTickResolution().AsFrameTime((double)InLocalTime);
			LocalTime = LocalFrameTime.RoundToFrame();
			EvaluateTime = LocalTime;
		}
		
		return this->AddKeysToControlRig(InSceneComp, InControlRig, LocalTime, EvaluateTime, *GeneratedKeys, KeyMode, UMovieSceneControlRigParameterTrack::StaticClass(), ControlRigName, RigControlName);
	};

	AnimatablePropertyChanged(FOnKeyProperty::CreateLambda(OnKeyProperty));
	EndKeying(); //fine even if we didn't BeginKeying
}

bool FControlRigParameterTrackEditor::ModifyOurGeneratedKeysByCurrentAndWeight(UObject* Object, UControlRig* InControlRig, FName RigControlName, UMovieSceneTrack* Track, UMovieSceneSection* SectionToKey, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();
	FMovieSceneEvaluationTrack EvalTrack = CastChecked<UMovieSceneControlRigParameterTrack>(Track)->GenerateTrackTemplate(Track);

	FMovieSceneInterrogationData InterrogationData;
	GetSequencer()->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

	//use the EvaluateTime to do the evaluation, may be different than the actually time we key
	FMovieSceneContext Context(FMovieSceneEvaluationRange(EvaluateTime, GetSequencer()->GetFocusedTickResolution()));
	EvalTrack.Interrogate(Context, InterrogationData, Object);
	TArray<FRigControlElement*> Controls = InControlRig->AvailableControls();
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey);
	FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
	int32 ChannelIndex = 0;
	FChannelMapInfo* pChannelIndex = nullptr;
	for (FRigControlElement* ControlElement : Controls)
	{
		if (!InControlRig->GetHierarchy()->IsAnimatable(ControlElement))
		{
			continue;
		}
		switch (ControlElement->Settings.ControlType)
		{
		case ERigControlType::Float:
		{
			for (const FFloatInterrogationData& Val : InterrogationData.Iterate<FFloatInterrogationData>(UMovieSceneControlRigParameterSection::GetFloatInterrogationKey()))
			{
				if ((Val.ParameterName == ControlElement->GetName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetName());
					if (pChannelIndex && pChannelIndex->GeneratedKeyIndex != INDEX_NONE)
					{
						ChannelIndex = pChannelIndex->GeneratedKeyIndex;
						float FVal = (float)Val.Val;
						GeneratedTotalKeys[ChannelIndex]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void *)&FVal, Weight);
					}
					break;
				}
			}
			break;
		}
		//no blending of bools,ints/enums
		case ERigControlType::Bool:
		case ERigControlType::Integer:
		{

			break;
		}
		case ERigControlType::Vector2D:
		{
			for (const FVector2DInterrogationData& Val : InterrogationData.Iterate<FVector2DInterrogationData>(UMovieSceneControlRigParameterSection::GetVector2DInterrogationKey()))
			{
				if ((Val.ParameterName == ControlElement->GetName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetName());
					if (pChannelIndex && pChannelIndex->GeneratedKeyIndex != INDEX_NONE)
					{
						ChannelIndex = pChannelIndex->GeneratedKeyIndex;
						float FVal = (float)Val.Val.X;
						GeneratedTotalKeys[ChannelIndex]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void *)&FVal, Weight);
						FVal = (float)Val.Val.Y;
						GeneratedTotalKeys[ChannelIndex + 1]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void *)&FVal, Weight);
					}
					break;
				}
			}
			break;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		case ERigControlType::Rotator:
		{
			for (const FVectorInterrogationData& Val : InterrogationData.Iterate<FVectorInterrogationData>(UMovieSceneControlRigParameterSection::GetVectorInterrogationKey()))
			{
				if ((Val.ParameterName == ControlElement->GetName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetName());
					if (pChannelIndex && pChannelIndex->GeneratedKeyIndex != INDEX_NONE)
					{
						ChannelIndex = pChannelIndex->GeneratedKeyIndex;
						float FVal = (float)Val.Val.X;
						GeneratedTotalKeys[ChannelIndex]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&FVal, Weight);
						FVal = (float)Val.Val.Y;
						GeneratedTotalKeys[ChannelIndex + 1]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&FVal, Weight);
						FVal = (float)Val.Val.Z;
						GeneratedTotalKeys[ChannelIndex + 2]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&FVal, Weight);			
					}
					break;
				}
			}
			break;
		}

		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		{
			for (const FEulerTransformInterrogationData& Val : InterrogationData.Iterate<FEulerTransformInterrogationData>(UMovieSceneControlRigParameterSection::GetTransformInterrogationKey()))
			{

				if ((Val.ParameterName == ControlElement->GetName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetName());
					if (pChannelIndex && pChannelIndex->GeneratedKeyIndex != INDEX_NONE)
					{
						ChannelIndex = pChannelIndex->GeneratedKeyIndex;

						if (pChannelIndex->bDoesHaveSpace)
						{
							++ChannelIndex;
						}
						
						FVector3f CurrentPos = (FVector3f)Val.Val.GetLocation();
						FRotator3f CurrentRot = FRotator3f(Val.Val.Rotator());
						GeneratedTotalKeys[ChannelIndex]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentPos.X, Weight);
						GeneratedTotalKeys[ChannelIndex + 1]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentPos.Y, Weight);
						GeneratedTotalKeys[ChannelIndex + 2]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentPos.Z, Weight);

						GeneratedTotalKeys[ChannelIndex + 3]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentRot.Roll, Weight);
						GeneratedTotalKeys[ChannelIndex + 4]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentRot.Pitch, Weight);
						GeneratedTotalKeys[ChannelIndex + 5]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentRot.Yaw, Weight);

						if (ControlElement->Settings.ControlType == ERigControlType::Transform || ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
						{
							FVector3f CurrentScale = (FVector3f)Val.Val.GetScale3D();
							GeneratedTotalKeys[ChannelIndex + 6]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentScale.X, Weight);
							GeneratedTotalKeys[ChannelIndex + 7]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentScale.Y, Weight);
							GeneratedTotalKeys[ChannelIndex + 8]->ModifyByCurrentAndWeight(Proxy, EvaluateTime, (void*)&CurrentScale.Z, Weight);
						}
					}
					break;
				}
			}
			break;
		}
		}
	}
	return true;
}

void FControlRigParameterTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* InTrack)
{
	bool bSectionAdded;
	UMovieSceneControlRigParameterTrack* Track = Cast< UMovieSceneControlRigParameterTrack>(InTrack);
	if (!Track || Track->GetControlRig() == nullptr)
	{
		return;
	}

	UMovieSceneControlRigParameterSection* SectionToKey = Cast<UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());
	if (SectionToKey == nullptr)
	{
		SectionToKey = Cast<UMovieSceneControlRigParameterSection>(Track->FindOrAddSection(0, bSectionAdded));
	}
	if (!SectionToKey)
	{
		return;
	}

	TArray<FFBXNodeAndChannels>* NodeAndChannels = Track->GetNodeAndChannelMappings(SectionToKey);

	MenuBuilder.BeginSection("Import To Control Rig", LOCTEXT("ImportToControlRig", "Import To Control Rig"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportControlRigFBX", "Import Control Rig FBX"),
			LOCTEXT("ImportControlRigFBXTooltip", "Import Control Rig FBX"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ImportFBX, Track, SectionToKey, NodeAndChannels)));
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	if (UFKControlRig* AutoRig = Cast<UFKControlRig>(Track->GetControlRig()))
	{
		MenuBuilder.BeginSection("FK Control Rig", LOCTEXT("FKControlRig", "FK Control Rig"));
		{

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SelectBonesToAnimate", "Select Bones Or Curves To Animate"),
				LOCTEXT("SelectBonesToAnimateToolTip", "Select which bones or curves you want to directly animate"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::SelectFKBonesToAnimate, AutoRig, Track)));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("FKRigApplyMode", "Additive"),
				LOCTEXT("FKRigApplyModeToolTip", "Toggles the apply mode between Replace and Additive"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ToggleFKControlRig, Track, AutoRig),
					FCanExecuteAction::CreateUObject(AutoRig, &UFKControlRig::CanToggleApplyMode),
					FIsActionChecked::CreateUObject(AutoRig, &UFKControlRig::IsApplyModeAdditive)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		}
		MenuBuilder.EndSection();

		MenuBuilder.AddMenuSeparator();
	}

}

bool FControlRigParameterTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (!Asset->IsA<UControlRigBlueprint>())
	{
		return false;
	}

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!MovieScene)
	{
		return false;
	}

	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Asset);
	UControlRigBlueprintGeneratedClass* RigClass = ControlRigBlueprint->GetControlRigBlueprintGeneratedClass();
	if (!RigClass)
	{
		return false;
	}

	USkeletalMesh* SkeletalMesh = ControlRigBlueprint->GetPreviewMesh();
	if (!SkeletalMesh)
	{
		FNotificationInfo Info(LOCTEXT("NoPreviewMesh", "Control rig has no preview mesh to create a spawnable skeletal mesh actor from"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddControlRigAsset", "Add Control Rig"));

	FGuid NewGuid = GetSequencer()->MakeNewSpawnable(*ASkeletalMeshActor::StaticClass());

	// MakeNewSpawnable can fail if spawnables are not allowed
	if (!NewGuid.IsValid())
	{
		return false;
	}
	
	ASkeletalMeshActor* SpawnedSkeletalMeshActor = Cast<ASkeletalMeshActor>(GetSequencer()->FindSpawnedObjectOrTemplate(NewGuid));
	if (!ensure(SpawnedSkeletalMeshActor))
	{
		return false;
	}

	SpawnedSkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMesh);

	FString NewName = MovieSceneHelpers::MakeUniqueSpawnableName(MovieScene, FName::NameToDisplayString(SkeletalMesh->GetName(), false));
	SpawnedSkeletalMeshActor->SetActorLabel(NewName, false);

	UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), NewGuid, NAME_None));
	if (Track == nullptr)
	{
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));
		check(CDO);

		AddControlRig(CDO->GetClass(), SpawnedSkeletalMeshActor->GetSkeletalMeshComponent(), NewGuid);
	}

	return true;
}


void FControlRigParameterTrackEditor::ToggleFKControlRig(UMovieSceneControlRigParameterTrack* Track, UFKControlRig* FKControlRig)
{
	FScopedTransaction Transaction(LOCTEXT("ToggleFKControlRig", "Toggle FK Control Rig"));
	FKControlRig->Modify();
	Track->Modify();
	FKControlRig->ToggleApplyMode();
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (Section)
		{
			UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section);
			if (CRSection)
			{
				Section->Modify();
				CRSection->ClearAllParameters();
				CRSection->RecreateWithThisControlRig(CRSection->GetControlRig(), true);
			}
		}
	}
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

}


void FControlRigParameterTrackEditor::ImportFBX(UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection,
	TArray<FFBXNodeAndChannels>* NodeAndChannels)
{
	if (NodeAndChannels)
	{
		//NodeAndChannels will be deleted later
		MovieSceneToolHelpers::ImportFBXIntoChannelsWithDialog(GetSequencer().ToSharedRef(), NodeAndChannels);
	}
}



class SFKControlRigBoneSelect : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SFKControlRigBoneSelect) {}
	SLATE_ATTRIBUTE(UFKControlRig*, AutoRig)
		SLATE_ATTRIBUTE(UMovieSceneControlRigParameterTrack*, Track)
		SLATE_ATTRIBUTE(ISequencer*, Sequencer)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
	{
		AutoRig = InArgs._AutoRig.Get();
		Track = InArgs._Track.Get();
		Sequencer = InArgs._Sequencer.Get();

		this->ChildSlot[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SFKControlRigBoneSelectDescription", "Select Bones You Want To Be Active On The FK Control Rig"))
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SSeparator)
				]
			+ SVerticalBox::Slot()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SBorder)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
				[
					//Save this widget so we can populate it later with check boxes
					SAssignNew(CheckBoxContainer, SVerticalBox)
				]
					]
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SFKControlRigBoneSelect::ChangeAllOptions, true)
				.Text(LOCTEXT("FKRigSelectAll", "Select All"))
				]
			+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SFKControlRigBoneSelect::ChangeAllOptions, false)
				.Text(LOCTEXT("FKRigDeselectAll", "Deselect All"))
				]

				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SSeparator)
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(8.0f, 4.0f, 8.0f, 4.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SFKControlRigBoneSelect::OnButtonClick, true)
				.Text(LOCTEXT("FKRigeOk", "OK"))
				]
			+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SFKControlRigBoneSelect::OnButtonClick, false)
				.Text(LOCTEXT("FKRigCancel", "Cancel"))
				]
				]
		];
	}


	/**
	* Creates a Slate check box
	*
	* @param	Label		Text label for the check box
	* @param	ButtonId	The ID for the check box
	* @return				The created check box widget
	*/
	TSharedRef<SWidget> CreateCheckBox(const FString& Label, int32 ButtonId)
	{
		return
			SNew(SCheckBox)
			.IsChecked(this, &SFKControlRigBoneSelect::IsCheckboxChecked, ButtonId)
			.OnCheckStateChanged(this, &SFKControlRigBoneSelect::OnCheckboxChanged, ButtonId)
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(AutoRig);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("SFKControlRigBoneSelect");
	}

	/**
	* Returns the state of the check box
	*
	* @param	ButtonId	The ID for the check box
	* @return				The status of the check box
	*/
	ECheckBoxState IsCheckboxChecked(int32 ButtonId) const
	{
		return CheckBoxInfoMap.FindChecked(ButtonId).bActive ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/**
	* Handler for all check box clicks
	*
	* @param	NewCheckboxState	The new state of the check box
	* @param	CheckboxThatChanged	The ID of the radio button that has changed.
	*/
	void OnCheckboxChanged(ECheckBoxState NewCheckboxState, int32 CheckboxThatChanged)
	{
		FFKBoneCheckInfo& Info = CheckBoxInfoMap.FindChecked(CheckboxThatChanged);
		Info.bActive = !Info.bActive;
	}

	/**
	* Handler for the Select All and Deselect All buttons
	*
	* @param	bNewCheckedState	The new state of the check boxes
	*/
	FReply ChangeAllOptions(bool bNewCheckedState)
	{
		for (TPair<int32, FFKBoneCheckInfo>& Pair : CheckBoxInfoMap)
		{
			FFKBoneCheckInfo& Info = Pair.Value;
			Info.bActive = bNewCheckedState;
		}
		return FReply::Handled();
	}

	/**
	* Populated the dialog with multiple check boxes, each corresponding to a bone
	*
	* @param	BoneInfos	The list of Bones to populate the dialog with
	*/
	void PopulateOptions(TArray<FFKBoneCheckInfo>& BoneInfos)
	{
		for (FFKBoneCheckInfo& Info : BoneInfos)
		{
			CheckBoxInfoMap.Add(Info.BoneID, Info);

			CheckBoxContainer->AddSlot()
				.AutoHeight()
				[
					CreateCheckBox(Info.BoneName.GetPlainNameString(), Info.BoneID)
				];
		}
	}


private:

	/**
	* Handles when a button is pressed, should be bound with appropriate EResult Key
	*
	* @param ButtonID - The return type of the button which has been pressed.
	*/
	FReply OnButtonClick(bool bValid)
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
		}
		//if ok selected bValid == true
		if (bValid && AutoRig)
		{
			TArray<FFKBoneCheckInfo> BoneCheckArray;
			BoneCheckArray.SetNumUninitialized(CheckBoxInfoMap.Num());
			int32 Index = 0;
			for (TPair<int32, FFKBoneCheckInfo>& Pair : CheckBoxInfoMap)
			{
				FFKBoneCheckInfo& Info = Pair.Value;
				BoneCheckArray[Index++] = Info;

			}
			if (Track && Sequencer)
			{
				TArray<bool> Mask;
				Mask.SetNum(BoneCheckArray.Num());
				for (const FFKBoneCheckInfo& Info : BoneCheckArray)
				{
					Mask[Info.BoneID] = Info.bActive;
				}

				TArray<UMovieSceneSection*> Sections = Track->GetAllSections();
				for (UMovieSceneSection* IterSection : Sections)
				{
					UMovieSceneControlRigParameterSection* Section = Cast< UMovieSceneControlRigParameterSection>(IterSection);
					if (Section)
					{
						Section->SetControlsMask(Mask);
					}
				}
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
			}
			AutoRig->SetControlActive(BoneCheckArray);
		}
		return bValid ? FReply::Handled() : FReply::Unhandled();
	}

	/** The slate container that the bone check boxes get added to */
	TSharedPtr<SVerticalBox>	 CheckBoxContainer;
	/** Store the check box state for each bone */
	TMap<int32, FFKBoneCheckInfo> CheckBoxInfoMap;

	UFKControlRig* AutoRig;
	UMovieSceneControlRigParameterTrack* Track;
	ISequencer* Sequencer;
};

void FControlRigParameterTrackEditor::SelectFKBonesToAnimate(UFKControlRig* AutoRig, UMovieSceneControlRigParameterTrack* Track)
{
	if (AutoRig)
	{
		const FText TitleText = LOCTEXT("SelectBonesOrCurvesToAnimate", "Select Bones Or Curves To Animate");

		// Create the window to choose our options
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(TitleText)
			.HasCloseButton(true)
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(400.0f, 200.0f))
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMinimize(false);

		TSharedRef<SFKControlRigBoneSelect> DialogWidget = SNew(SFKControlRigBoneSelect)
			.AutoRig(AutoRig)
			.Track(Track)
			.Sequencer(GetSequencer().Get());

		TArray<FName> ControlRigNames = AutoRig->GetControlNames();
		TArray<FFKBoneCheckInfo> BoneInfos;
		for (int32 Index = 0; Index < ControlRigNames.Num(); ++Index)
		{
			FFKBoneCheckInfo Info;
			Info.BoneID = Index;
			Info.BoneName = ControlRigNames[Index];
			Info.bActive = AutoRig->GetControlActive(Index);
			BoneInfos.Add(Info);
		}

		DialogWidget->PopulateOptions(BoneInfos);

		Window->SetContent(DialogWidget);
		FSlateApplication::Get().AddWindow(Window);
	}

	//reconstruct all channel proxies TODO or not to do that is the question
}
bool FControlRigParameterTrackEditor::CollapseAllLayers(TSharedPtr<ISequencer>& SequencerPtr, UMovieSceneTrack* OwnerTrack, UMovieSceneControlRigParameterSection* ParameterSection, bool bKeyReduce, float Tolerance)
{

	if (SequencerPtr.IsValid() && OwnerTrack && ParameterSection && ParameterSection->GetControlRig())
	{
		TArray<UMovieSceneSection*> Sections = OwnerTrack->GetAllSections();
		//make sure right type
		if (ParameterSection->GetBlendType().Get() != EMovieSceneBlendType::Absolute && Sections.Num() > 0 && Sections[0] != ParameterSection)
		{
			UE_LOG(LogControlRigEditor, Log, TEXT("Section wrong type or not first when collapsing layers"));
			return false;
		}
		FScopedTransaction Transaction(LOCTEXT("CollapseAllSections", "Collapse All Sections"));
		ParameterSection->Modify();
		UControlRig* ControlRig = ParameterSection->GetControlRig();
		TRange<FFrameNumber> Range = SequencerPtr->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
		FFrameNumber StartFrame = Range.GetLowerBoundValue();
		FFrameNumber EndFrame = Range.GetUpperBoundValue();
		const FFrameRate& FrameRate = SequencerPtr->GetFocusedDisplayRate();
		const FFrameRate& TickResolution = SequencerPtr->GetFocusedTickResolution();
		FMovieSceneSequenceTransform RootToLocalTransform = SequencerPtr->GetFocusedMovieSceneSequenceTransform();

		FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
		TArray<FFrameNumber> Frames;
		for (FFrameNumber& Frame = StartFrame; Frame <= EndFrame; Frame += FrameRateInFrameNumber)
		{
			Frames.Add(Frame);
		}
		//Store transforms
		TArray<TPair<FName, TArray<FTransform>>> ControlLocalTransforms;
		TArray<FRigControlElement*> Controls;
		ControlRig->GetControlsInOrder(Controls);

		for (FRigControlElement* ControlElement : Controls)
		{
			if (!ControlRig->GetHierarchy()->IsAnimatable(ControlElement))
			{
				continue;
			}
			TPair<FName, TArray<FTransform>> NameTransforms;
			NameTransforms.Key = ControlElement->GetName();
			NameTransforms.Value.SetNum(Frames.Num());
			ControlLocalTransforms.Add(NameTransforms);
		}

		//get all of the local 
		int32 Index = 0;
		for (Index = 0; Index < Frames.Num(); ++Index)
		{
			const FFrameNumber& FrameNumber = Frames[Index];
			FFrameTime GlobalTime(FrameNumber);
			GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();

			FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), SequencerPtr->GetPlaybackStatus()).SetHasJumped(true);

			SequencerPtr->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *SequencerPtr);
			ControlRig->Evaluate_AnyThread();
			for (TPair<FName, TArray<FTransform>>& TrailControlTransform : ControlLocalTransforms)
			{
				TrailControlTransform.Value[Index] = ControlRig->GetControlLocalTransform(TrailControlTransform.Key);
			}
		}
		//delete other sections
		OwnerTrack->Modify();
		for (Index = Sections.Num() - 1; Index >= 0; --Index)
		{
			if (Sections[Index] != ParameterSection)
			{
				OwnerTrack->RemoveSectionAt(Index);
			}
		}

		//remove all keys, except Space Channels, from the Section.
		ParameterSection->RemoveAllKeys(false /*bIncludedSpaceKeys*/);

		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;


		FScopedSlowTask Feedback(Frames.Num(), LOCTEXT("CollapsingSections", "Collapsing Sections"));
		Feedback.MakeDialog(true);

		const ERichCurveInterpMode InterpMode = bKeyReduce ? RCIM_Cubic : RCIM_Linear;

		Index = 0;
		for (Index = 0; Index < Frames.Num(); ++Index)
		{
			Feedback.EnterProgressFrame(1, LOCTEXT("CollapsingSections", "Collapsing Sections"));
			const FFrameNumber& FrameNumber = Frames[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(FrameNumber));
			//need to do the twice hack since controls aren't really in order
			for (int32 TwiceHack = 0; TwiceHack < 2; ++TwiceHack)
			{
				for (TPair<FName, TArray<FTransform>>& TrailControlTransform : ControlLocalTransforms)
				{
					ControlRig->SetControlLocalTransform(TrailControlTransform.Key, TrailControlTransform.Value[Index],false, Context, false);
				}
			}
			ControlRig->Evaluate_AnyThread();
			ParameterSection->RecordControlRigKey(FrameNumber, true, InterpMode);

			if (Feedback.ShouldCancel())
			{
				Transaction.Cancel();
				SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
				return false;
			}
		}
		if (bKeyReduce)
		{
			FKeyDataOptimizationParams Params;
			Params.bAutoSetInterpolation = true;
			Params.Tolerance = Tolerance;
			FMovieSceneChannelProxy& ChannelProxy = ParameterSection->GetChannelProxy();
			TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();

			for (FMovieSceneFloatChannel* Channel : FloatChannels)
			{
				Channel->Optimize(Params); //should also auto tangent
			}
		}
		//reset everything back
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		return true;
	}
	return false;
}
void FControlRigParameterSection::CollapseAllLayers()
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	UMovieSceneTrack* OwnerTrack = ParameterSection->GetTypedOuter<UMovieSceneTrack>();
	FControlRigParameterTrackEditor::CollapseAllLayers(SequencerPtr,OwnerTrack, ParameterSection);

}
void FControlRigParameterSection::KeyZeroValue()
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	FScopedTransaction Transaction(LOCTEXT("KeyZeroValue", "Key Zero Value"));
	ParameterSection->Modify();
	FFrameTime Time = SequencerPtr->GetLocalTime().Time;
	ParameterSection->KeyZeroValue(Time.GetFrame(),true);
	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}
void FControlRigParameterSection::KeyWeightValue(float Val)
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	FScopedTransaction Transaction(LOCTEXT("KeyWeightZero", "Key Weight Zero"));
	ParameterSection->Modify();
	EMovieSceneTransformChannel Channels = ParameterSection->GetTransformMask().GetChannels();
	if ((Channels & EMovieSceneTransformChannel::Weight) == EMovieSceneTransformChannel::None)
	{
		ParameterSection->SetTransformMask(ParameterSection->GetTransformMask().GetChannels() | EMovieSceneTransformChannel::Weight);
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
	FFrameTime Time = SequencerPtr->GetLocalTime().Time;
	ParameterSection->KeyWeightValue(Time.GetFrame(), Val);
	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}

void FControlRigParameterSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding)
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	UControlRig* ControlRig = ParameterSection->GetControlRig();

	if (ControlRig)
	{

		UFKControlRig* AutoRig = Cast<UFKControlRig>(ControlRig);
		if (AutoRig || ControlRig->SupportsEvent(FRigUnit_InverseExecution::EventName))
		{
			UObject* BoundObject = nullptr;
			USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(InObjectBinding, &BoundObject, WeakSequencer.Pin());

			if (Skeleton)
			{
				// Load the asset registry module
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

				// Collect a full list of assets with the specified class
				TArray<FAssetData> AssetDataList;
				AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetClassPathName(), AssetDataList, true);

				if (AssetDataList.Num())
				{
					MenuBuilder.AddSubMenu(
						LOCTEXT("ImportAnimSequenceIntoThisSection", "Import Anim Sequence Into This Section"), NSLOCTEXT("Sequencer", "ImportAnimSequenceIntoThisSectionTP", "Import Anim Sequence Into This Section"),
						FNewMenuDelegate::CreateRaw(this, &FControlRigParameterSection::AddAnimationSubMenuForFK, InObjectBinding, Skeleton, ParameterSection)
					);
				}
			}
		}
		TArray<FRigControlElement*> Controls;
		ControlRig->GetControlsInOrder(Controls);

		auto MakeUIAction = [=](EMovieSceneTransformChannel ChannelsToToggle)
		{
			return FUIAction(
				FExecuteAction::CreateLambda([=]
					{
						FScopedTransaction Transaction(LOCTEXT("SetActiveChannelsTransaction", "Set Active Channels"));
						ParameterSection->Modify();
						EMovieSceneTransformChannel Channels = ParameterSection->GetTransformMask().GetChannels();

						if (EnumHasAllFlags(Channels, ChannelsToToggle) || (Channels & ChannelsToToggle) == EMovieSceneTransformChannel::None)
						{
							ParameterSection->SetTransformMask(ParameterSection->GetTransformMask().GetChannels() ^ ChannelsToToggle);
						}
						else
						{
							ParameterSection->SetTransformMask(ParameterSection->GetTransformMask().GetChannels() | ChannelsToToggle);
						}

						// Restore pre-animated state for the bound objects so that inactive channels will return to their default values.
						for (TWeakObjectPtr<> WeakObject : SequencerPtr->FindBoundObjects(InObjectBinding, SequencerPtr->GetFocusedTemplateID()))
						{
							if (UObject* Object = WeakObject.Get())
							{
								SequencerPtr->RestorePreAnimatedState();
							}
						}

						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
					}
				),
				FCanExecuteAction(),
						FGetActionCheckState::CreateLambda([=]
							{
								EMovieSceneTransformChannel Channels = ParameterSection->GetTransformMask().GetChannels();
								if (EnumHasAllFlags(Channels, ChannelsToToggle))
								{
									return ECheckBoxState::Checked;
								}
								else if (EnumHasAnyFlags(Channels, ChannelsToToggle))
								{
									return ECheckBoxState::Undetermined;
								}
								return ECheckBoxState::Unchecked;
							})
						);
		};
		auto ToggleControls = [=](int32 Index)
		{
			return FUIAction(
				FExecuteAction::CreateLambda([=]
					{
						FScopedTransaction Transaction(LOCTEXT("ToggleRigControlFiltersTransaction", "Toggle Rig Control Filters"));
						ParameterSection->Modify();
						if (Index >= 0)
						{
							ParameterSection->SetControlsMask(Index, !ParameterSection->GetControlsMask(Index));
						}
						else
						{
							ParameterSection->FillControlsMask(!ParameterSection->GetControlsMask(0));
						}
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

					}
				),
				FCanExecuteAction(),
						FGetActionCheckState::CreateLambda([=]
							{
								TArray<bool> ControlBool = ParameterSection->GetControlsMask();
								if (Index >= 0)
								{
									if (ControlBool[Index])
									{
										return ECheckBoxState::Checked;

									}
									else
									{
										return ECheckBoxState::Unchecked;
									}
								}
								else
								{
									TOptional<bool> FirstVal;
									for (bool Val : ControlBool)
									{
										if (FirstVal.IsSet())
										{
											if (Val != FirstVal)
											{
												return ECheckBoxState::Undetermined;
											}
										}
										else
										{
											FirstVal = Val;
										}

									}
									return (FirstVal.IsSet() && FirstVal.GetValue()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								}
							})
						);
		};
		UMovieSceneControlRigParameterTrack* Track = ParameterSection->GetTypedOuter<UMovieSceneControlRigParameterTrack>();
		if (Track)
		{
			TArray<UMovieSceneSection*> Sections = Track->GetAllSections();
			//If Base Absolute section 
			if (ParameterSection->GetBlendType().Get() == EMovieSceneBlendType::Absolute && Sections[0] == ParameterSection)
			{
				MenuBuilder.BeginSection(NAME_None, LOCTEXT("AnimationLayers", "Animation Layers"));
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("CollapseAllSections", "Collapse All Sections"),
						LOCTEXT("CollapseAllSections_ToolTip", "Collapse all sections onto this section"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([=] { CollapseAllLayers(); }))
					);
				}
			}
			if (ParameterSection->GetBlendType().Get() == EMovieSceneBlendType::Additive)
			{

				MenuBuilder.BeginSection(NAME_None, LOCTEXT("AnimationLayers", "Animation Layers"));
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("KeyZeroValue", "Key Zero Value"),
						LOCTEXT("KeyZeroValue_Tooltip", "Set zero key on all controls in this section"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([=] { KeyZeroValue(); }))
					);
				}
				
				MenuBuilder.AddMenuEntry(
					LOCTEXT("KeyWeightZero", "Key Weight Zero"),
					LOCTEXT("KeyWeightZero_Tooltip", "Key a zero value on the Weight channel"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([=] { KeyWeightValue(0.0f); }))
				);
				
				MenuBuilder.AddMenuEntry(
					LOCTEXT("KeyWeightOne", "Key Weight One"),
					LOCTEXT("KeyWeightOne_Tooltip", "Key a one value on the Weight channel"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([=] { KeyWeightValue(1.0f); }))
				);
				
			}
		}
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("RigSectionActiveChannels", "Active Channels"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetFromSelectedControls", "Set From Selected Controls"),
				LOCTEXT("SetFromSelectedControls_ToolTip", "Set active channels from the current control selection"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] { ShowSelectedControlsChannels(); }),
					FCanExecuteAction::CreateLambda([=] { return ControlRig->CurrentControlSelection().Num() > 0; } )
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowAllControls", "Show All Controls"),
				LOCTEXT("ShowAllControls_ToolTip", "Set active channels from all controls"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([=] { return ShowAllControlsChannels(); }))
			);

			MenuBuilder.AddSubMenu(
				LOCTEXT("AllTranslation", "Translation"), LOCTEXT("AllTranslation_ToolTip", "Causes this section to affect the translation of rig control transforms"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("TranslationX", "X"), LOCTEXT("TranslationX_ToolTip", "Causes this section to affect the X channel of the transform's translation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationX), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("TranslationY", "Y"), LOCTEXT("TranslationY_ToolTip", "Causes this section to affect the Y channel of the transform's translation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationY), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("TranslationZ", "Z"), LOCTEXT("TranslationZ_ToolTip", "Causes this section to affect the Z channel of the transform's translation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationZ), NAME_None, EUserInterfaceActionType::ToggleButton);
					}),
				MakeUIAction(EMovieSceneTransformChannel::Translation),
						NAME_None,
						EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddSubMenu(
				LOCTEXT("AllRotation", "Rotation"), LOCTEXT("AllRotation_ToolTip", "Causes this section to affect the rotation of the rig control transform"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("RotationX", "Roll (X)"), LOCTEXT("RotationX_ToolTip", "Causes this section to affect the roll (X) channel the transform's rotation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationX), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("RotationY", "Pitch (Y)"), LOCTEXT("RotationY_ToolTip", "Causes this section to affect the pitch (Y) channel the transform's rotation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationY), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("RotationZ", "Yaw (Z)"), LOCTEXT("RotationZ_ToolTip", "Causes this section to affect the yaw (Z) channel the transform's rotation"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationZ), NAME_None, EUserInterfaceActionType::ToggleButton);
					}),
				MakeUIAction(EMovieSceneTransformChannel::Rotation),
						NAME_None,
						EUserInterfaceActionType::ToggleButton);

			MenuBuilder.AddSubMenu(
				LOCTEXT("AllScale", "Scale"), LOCTEXT("AllScale_ToolTip", "Causes this section to affect the scale of the rig control transform"),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("ScaleX", "X"), LOCTEXT("ScaleX_ToolTip", "Causes this section to affect the X channel of the transform's scale"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleX), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("ScaleY", "Y"), LOCTEXT("ScaleY_ToolTip", "Causes this section to affect the Y channel of the transform's scale"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleY), NAME_None, EUserInterfaceActionType::ToggleButton);
					SubMenuBuilder.AddMenuEntry(
						LOCTEXT("ScaleZ", "Z"), LOCTEXT("ScaleZ_ToolTip", "Causes this section to affect the Z channel of the transform's scale"),
						FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleZ), NAME_None, EUserInterfaceActionType::ToggleButton);
					}),
				MakeUIAction(EMovieSceneTransformChannel::Scale),
						NAME_None,
						EUserInterfaceActionType::ToggleButton);

			//mz todo h
			MenuBuilder.AddMenuEntry(
				LOCTEXT("Weight", "Weight"), LOCTEXT("Weight_ToolTip", "Causes this section to be applied with a user-specified weight curve"),
				FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::Weight), NAME_None, EUserInterfaceActionType::ToggleButton);
		}
		MenuBuilder.EndSection();
	}
}

void FControlRigParameterSection::ShowSelectedControlsChannels()
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	UControlRig* ControlRig = ParameterSection ? ParameterSection->GetControlRig() : nullptr;

	if (ParameterSection && ControlRig && SequencerPtr.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ShowSelecedControlChannels", "Show Selected Control Channels"));
		ParameterSection->Modify();
		ParameterSection->FillControlsMask(false);

		TArray<FRigControlElement*> Controls;
		ControlRig->GetControlsInOrder(Controls);
		int32 Index = 0;
		for (const FRigControlElement* RigControl : Controls)
		{
			const FName RigName = RigControl->GetName();
			if (ControlRig->IsControlSelected(RigName))
			{
				ParameterSection->SetControlsMask(Index, true);
			}
			++Index;
		}
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

void FControlRigParameterSection::ShowAllControlsChannels()
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	if (ParameterSection && SequencerPtr.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ShowAllControlChannels", "Show All Control Channels"));
		ParameterSection->Modify();
		ParameterSection->FillControlsMask(true);
		SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

//mz todo
bool FControlRigParameterSection::RequestDeleteCategory(const TArray<FName>& CategoryNamePaths)
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	if (ParameterSection && SequencerPtr)
	{
		const FName& Channel = CategoryNamePaths.Last();

		// remove constraint channel if there are no keys
		const FConstraintAndActiveChannel* ConstraintChannel = ParameterSection->GetConstraintChannel(Channel);
		if (ConstraintChannel && ConstraintChannel->ActiveChannel.GetNumKeys() == 0)
		{
			if (ParameterSection->TryModify())
			{
				ParameterSection->RemoveConstraintChannel(Channel);
				SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				return true;
			}
		}
	}
	
	/*
	const FScopedTransaction Transaction(LOCTEXT("DeleteTransformCategory", "Delete transform category"));

	if (ParameterSection->TryModify())
	{
	FName CategoryName = CategoryNamePaths[CategoryNamePaths.Num() - 1];

	EMovieSceneTransformChannel Channel = ParameterSection->GetTransformMask().GetChannels();
	EMovieSceneTransformChannel ChannelToRemove = ParameterSection->GetTransformMaskByName(CategoryName).GetChannels();

	Channel = Channel ^ ChannelToRemove;

	ParameterSection->SetTransformMask(Channel);

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	return true;
	}
	*/
	return false;
}

bool FControlRigParameterSection::RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePaths)
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	/*
	const FScopedTransaction Transaction(LOCTEXT("DeleteTransformChannel", "Delete transform channel"));

	if (ParameterSection->TryModify())
	{
	// Only delete the last key area path which is the channel. ie. TranslationX as opposed to Translation
	FName KeyAreaName = KeyAreaNamePaths[KeyAreaNamePaths.Num() - 1];

	EMovieSceneTransformChannel Channel = ParameterSection->GetTransformMask().GetChannels();
	EMovieSceneTransformChannel ChannelToRemove = ParameterSection->GetTransformMaskByName(KeyAreaName).GetChannels();

	Channel = Channel ^ ChannelToRemove;

	ParameterSection->SetTransformMask(Channel);

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	return true;
	}
	*/
	return true;
}


void FControlRigParameterSection::AddAnimationSubMenuForFK(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneControlRigParameterSection* Section)
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	UMovieSceneSequence* Sequence = SequencerPtr.IsValid() ? SequencerPtr->GetFocusedMovieSceneSequence() : nullptr;

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FControlRigParameterSection::OnAnimationAssetSelectedForFK, ObjectBinding, Section);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FControlRigParameterSection::OnAnimationAssetEnterPressedForFK, ObjectBinding, Section);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateRaw(this, &FControlRigParameterSection::ShouldFilterAssetForFK);
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
		AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
}


void FControlRigParameterSection::OnAnimationAssetSelectedForFK(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneControlRigParameterSection* Section)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	if (SelectedObject && SelectedObject->IsA(UAnimSequence::StaticClass()) && SequencerPtr.IsValid())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		UObject* BoundObject = nullptr;
		AcquireSkeletonFromObjectGuid(ObjectBinding, &BoundObject, SequencerPtr);
		USkeletalMeshComponent* SkelMeshComp = AcquireSkeletalMeshFromObject(BoundObject, SequencerPtr);

		if (AnimSequence && SkelMeshComp && AnimSequence->GetDataModel()->GetNumBoneTracks() > 0)
		{

			FScopedTransaction Transaction(LOCTEXT("BakeAnimation_Transaction", "Bake Animation To FK Control Rig"));
			Section->Modify();
			UMovieScene* MovieScene = SequencerPtr->GetFocusedMovieSceneSequence()->GetMovieScene();
			FFrameNumber StartFrame = SequencerPtr->GetLocalTime().Time.GetFrame();
			if (!Section->LoadAnimSequenceIntoThisSection(AnimSequence, MovieScene, SkelMeshComp, false, 0.1f, StartFrame))
			{
				Transaction.Cancel();
			}
			SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
	}
}

bool FControlRigParameterSection::ShouldFilterAssetForFK(const FAssetData& AssetData)
{
	// we don't want 

	if (AssetData.AssetClassPath == UAnimMontage::StaticClass()->GetClassPathName())
	{
		return true;
	}

	const FString EnumString = AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
	if (EnumString.IsEmpty())
	{
		return false;
	}

	UEnum* AdditiveTypeEnum = StaticEnum<EAdditiveAnimationType>();
	return ((EAdditiveAnimationType)AdditiveTypeEnum->GetValueByName(*EnumString) != AAT_None);

}

void FControlRigParameterSection::OnAnimationAssetEnterPressedForFK(const TArray<FAssetData>& AssetData, FGuid  ObjectBinding, UMovieSceneControlRigParameterSection* Section)
{
	if (AssetData.Num() > 0)
	{
		OnAnimationAssetSelectedForFK(AssetData[0].GetAsset(), ObjectBinding, Section);
	}
}

FEditorModeTools* FControlRigParameterTrackEditor::GetEditorModeTools() const
{
	TSharedPtr<ISequencer> SharedSequencer = GetSequencer();
	if (SharedSequencer.IsValid())
	{
		TSharedPtr<IToolkitHost> ToolkitHost = SharedSequencer->GetToolkitHost();
		if (ToolkitHost.IsValid())
		{
			return &ToolkitHost->GetEditorModeManager();
		}
	}

	return nullptr;
}

FControlRigEditMode* FControlRigParameterTrackEditor::GetEditMode(bool bForceActivate /*= false*/) const
{
	if (FEditorModeTools* EditorModetools = GetEditorModeTools())
	{
		if (bForceActivate && !EditorModetools->IsModeActive(FControlRigEditMode::ModeName))
		{
			EditorModetools->ActivateMode(FControlRigEditMode::ModeName);
		}

		return static_cast<FControlRigEditMode*>(EditorModetools->GetActiveMode(FControlRigEditMode::ModeName));
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE


