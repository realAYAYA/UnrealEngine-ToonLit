// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigParameterTrackEditor.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/TrackRowModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/Selection/Selection.h"
#include "Animation/AnimMontage.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Framework/Commands/Commands.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSpinBox.h"
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
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Exporters/AnimSeqExportOption.h"
#include "SBakeToControlRigDialog.h"
#include "ControlRigBlueprint.h"
#include "RigVMBlueprintGeneratedClass.h"
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
#include "Constraints/MovieSceneConstraintChannelHelper.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "PropertyEditorModule.h"
#include "Constraints/TransformConstraintChannelInterface.h"
#include "BakingAnimationKeySettings.h"
#include "FrameNumberDetailsCustomization.h"
#include "Editor/UnrealEd/Private/FbxExporter.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Widgets/Layout/SSpacer.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "LevelSequence.h"


#define LOCTEXT_NAMESPACE "FControlRigParameterTrackEditor"

bool FControlRigParameterTrackEditor::bControlRigEditModeWasOpen = false;
TArray <TPair<UClass*, TArray<FName>>> FControlRigParameterTrackEditor::PreviousSelectedControlRigs;

TAutoConsoleVariable<bool> CVarAutoGenerateControlRigTrack(TEXT("ControlRig.Sequencer.AutoGenerateTrack"), true, TEXT("When true automatically create control rig tracks in Sequencer when a control rig is added to a level."));

TAutoConsoleVariable<bool> CVarSelectedKeysSelectControls(TEXT("ControlRig.Sequencer.SelectedKeysSelectControls"), false, TEXT("When true when we select a key in Sequencer it will select the Control, by default false."));

TAutoConsoleVariable<bool> CVarSelectedSectionSetsSectionToKey(TEXT("ControlRig.Sequencer.SelectedSectionSetsSectionToKey"), true, TEXT("When true when we select a channel in a section, if it's the only section selected we set it as the Section To Key, by default false."));

TAutoConsoleVariable<bool> CVarEnableAdditiveControlRigs(TEXT("ControlRig.Sequencer.EnableAdditiveControlRigs"), true, TEXT("When true it is possible to add an additive control rig to a skeletal mesh component."));

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
FCriticalSection FControlRigParameterTrackEditor::ControlUndoTransactionMutex;

FControlRigParameterTrackEditor::FControlRigParameterTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FKeyframeTrackEditor<UMovieSceneControlRigParameterTrack>(InSequencer)
	, bCurveDisplayTickIsPending(false)
	, bIsDoingSelection(false)
	, bSkipNextSelectionFromTimer(false)
	, bIsLayeredControlRig(false)
	, bFilterAssetBySkeleton(true)
	, bFilterAssetByAnimatableControls(false)
	, ControlUndoBracket(0)
	, ControlChangedDuringUndoBracket(0)
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
		const FDelegateHandle OnObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddLambda([this](const TMap<UObject*, UObject*>& ReplacementMap)
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

					IterateTracks([this, &OldToNewControlRigs, ControlRigEditMode, &bRequestEvaluate](UMovieSceneControlRigParameterTrack* Track)
					{
						if (UControlRig* OldControlRig = Track->GetControlRig())
						{
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

						return false;
					});

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

	// Register all modified/selections for control rigs
	IterateTracks([this](UMovieSceneControlRigParameterTrack* Track)
	{
		if (UControlRig* ControlRig = Track->GetControlRig())
		{
			BindControlRig(ControlRig);
		}

		return false;
	});
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
		ControlRig->OnPostConstruction_AnyThread().AddRaw(this, &FControlRigParameterTrackEditor::HandleOnPostConstructed);
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
		ControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
		ControlRig->ControlSelected().RemoveAll(this);
		if (const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
		{
			Binding->OnControlRigBind().RemoveAll(this);
		}
		ControlRig->ControlUndoBracket().RemoveAll(this);
		ControlRig->ControlRigBound().RemoveAll(this);
		
		BoundControlRigs.Remove(ControlRig);
		ClearOutAllSpaceAndConstraintDelegates(ControlRig);
	}
}
void FControlRigParameterTrackEditor::UnbindAllControlRigs()
{
	ClearOutAllSpaceAndConstraintDelegates();
	TArray<TWeakObjectPtr<UControlRig>> ControlRigs = BoundControlRigs;
	for (TWeakObjectPtr<UControlRig>& ObjectPtr : ControlRigs)
	{
		if (ObjectPtr.IsValid())
		{
			UControlRig* ControlRig = ObjectPtr.Get();
			UnbindControlRig(ControlRig);
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

	FControlRigEditMode* ControlRigEditMode = GetEditMode();
	PreviousSelectedControlRigs.Reset();
	if (ControlRigEditMode)
	{
		bControlRigEditModeWasOpen = true;
		for (TWeakObjectPtr<UControlRig>& ControlRig : BoundControlRigs)
		{
			if (ControlRig.IsValid())
			{
				TPair<UClass*, TArray<FName>> ClassAndName;
				ClassAndName.Key = ControlRig->GetClass();
				ClassAndName.Value = ControlRig->CurrentControlSelection();
				PreviousSelectedControlRigs.Add(ClassAndName);
			}
		}
		ControlRigEditMode->Exit(); //deactive mode below doesn't exit for some reason so need to make sure things are cleaned up
		if (FEditorModeTools* Tools = GetEditorModeTools())
		{
			Tools->DeactivateMode(FControlRigEditMode::ModeName);
		}

		ControlRigEditMode->SetObjects(nullptr, nullptr, GetSequencer());
	}
	else
	{
		bControlRigEditModeWasOpen = false;
	}
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

static bool ClassViewerSortPredicate(const FClassViewerSortElementInfo& A, const  FClassViewerSortElementInfo& B)
{
	if ((A.Class == UFKControlRig::StaticClass() && B.Class == UFKControlRig::StaticClass()) ||
				(A.Class != UFKControlRig::StaticClass() && B.Class != UFKControlRig::StaticClass()))
	{
		return  (*A.DisplayName).Compare(*B.DisplayName, ESearchCase::IgnoreCase) < 0;
	}
	else
	{
		return A.Class == UFKControlRig::StaticClass();
	}
}

/** Filter class does not allow classes that already exist in a skeletal mesh component. */
class FClassViewerHideAlreadyAddedRigsFilter : public IClassViewerFilter
{
public:
	FClassViewerHideAlreadyAddedRigsFilter(const TArray<UClass*> InExistingClasses)
		: AlreadyAddedRigs(InExistingClasses)
	{}

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		return !AlreadyAddedRigs.Contains(InClass);
	}
	
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
										const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData,
										TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const FTopLevelAssetPath ClassPath = InUnloadedClassData.Get().GetClassPathName();
		return !AlreadyAddedRigs.ContainsByPredicate([ClassPath](const UClass* Class)
		{
			return ClassPath == Class->GetClassPathName();
		});
	}
	
private:
	TArray<UClass*> AlreadyAddedRigs;
};

void FControlRigParameterTrackEditor::BakeToControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton)
{
	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

	if (Skeleton)
	{
		FClassViewerInitializationOptions Options;
		Options.bShowUnloadedBlueprints = true;
		Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
		const TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter(bFilterAssetBySkeleton, false, true, Skeleton));
		Options.ClassFilters.Add(ClassFilter.ToSharedRef());
		Options.bShowNoneOption = false;
		Options.ExtraPickerCommonClasses.Add(UFKControlRig::StaticClass());
		Options.ClassViewerSortPredicate = ClassViewerSortPredicate;

		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		const TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FControlRigParameterTrackEditor::BakeToControlRig, ObjectBinding, BoundObject, SkelMeshComp, Skeleton));
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
			AnimSeqExportOption->bTransactRecording = false;

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
					EMovieSceneKeyInterpolation DefaultInterpolation = SequencerParent->GetKeyInterpolation();
					ParamSection->LoadAnimSequenceIntoThisSection(TempAnimSequence, OwnerMovieScene, SkelMeshComp,
						BakeSettings->bReduceKeys, BakeSettings->Tolerance, BakeSettings->bResetControls, FFrameNumber(0), DefaultInterpolation);

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

void FControlRigParameterTrackEditor::IterateTracks(TFunctionRef<bool(UMovieSceneControlRigParameterTrack*)> Callback) const
{
	UMovieScene* MovieScene = GetSequencer().IsValid() && GetSequencer()->GetFocusedMovieSceneSequence() ? GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	IterateTracksInMovieScene(*MovieScene, Callback);
}

void FControlRigParameterTrackEditor::IterateTracksInMovieScene(UMovieScene& MovieScene, TFunctionRef<bool(UMovieSceneControlRigParameterTrack*)> Callback) const
{
	TArray<UMovieSceneControlRigParameterTrack*> Tracks;
	
	const TArray<FMovieSceneBinding>& Bindings = MovieScene.GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		TArray<UMovieSceneTrack*> FoundTracks = MovieScene.FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
		for(UMovieSceneTrack* Track : FoundTracks)
		{
			if (UMovieSceneControlRigParameterTrack* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
			{
				Callback(CRTrack);
			}
		}
	}
	
	for (UMovieSceneTrack* Track : MovieScene.GetTracks())
	{
		if (UMovieSceneControlRigParameterTrack* CRTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
		{
			Callback(CRTrack);
		}
	}
}

void FControlRigParameterTrackEditor::BakeInvertedPose(UControlRig* InControlRig, UMovieSceneControlRigParameterTrack* Track)
{
	if (!InControlRig->IsAdditive())
	{
		return;
	}

	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());
	USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(InControlRig->GetObjectBinding()->GetBoundObject());
	UMovieSceneSequence* MovieSceneSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene();
	UAnimSeqExportOption* ExportOptions = NewObject<UAnimSeqExportOption>(GetTransientPackage(), NAME_None);
	UBakeToControlRigSettings* BakeSettings = GetMutableDefault<UBakeToControlRigSettings>();
	const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();
	FMovieSceneSequenceIDRef Template = ParentSequencer->GetFocusedTemplateID();
	FMovieSceneSequenceTransform RootToLocalTransform = ParentSequencer->GetFocusedMovieSceneSequenceTransform();

	if (ExportOptions == nullptr || MovieScene == nullptr || SkelMeshComp == nullptr)
	{
		UE_LOG(LogMovieScene, Error, TEXT("FControlRigParameterTrackEditor::BakeInvertedPose All parameters must be valid."));
		return;
	}
	
	const FScopedTransaction Transaction(LOCTEXT("BakeInvertedPose_Transaction", "Bake Inverted Pose"));

	UnFbx::FLevelSequenceAnimTrackAdapter AnimTrackAdapter(ParentSequencer.Get(), MovieScene, RootToLocalTransform);
	int32 AnimationLength = AnimTrackAdapter.GetLength();
	FScopedSlowTask Progress(AnimationLength, LOCTEXT("BakingToControlRig_SlowTask", "Baking To Control Rig..."));
	Progress.MakeDialog(true);

	auto DelegateHandle = InControlRig->OnPreAdditiveValuesApplication_AnyThread().AddLambda([](UControlRig* InControlRig, const FName& InEventName)
	{
		InControlRig->InvertInputPose();
	});

	auto KeyFrame = [this, ParentSequencer, InControlRig, SkelMeshComp](const FFrameNumber FrameNumber)
	{
		const FFrameNumber NewTime = ConvertFrameTime(FrameNumber, ParentSequencer->GetFocusedDisplayRate(), ParentSequencer->GetFocusedTickResolution()).FrameNumber;
		float LocalTime = ParentSequencer->GetFocusedTickResolution().AsSeconds(FFrameTime(NewTime));

		AddControlKeys(SkelMeshComp, InControlRig, InControlRig->GetFName(), NAME_None, EControlRigContextChannelToKey::AllTransform, 
				ESequencerKeyMode::ManualKeyForced, LocalTime);
	};

	FInitAnimationCB InitCallback = FInitAnimationCB::CreateLambda([]{});
	FStartAnimationCB StartCallback = FStartAnimationCB::CreateLambda([AnimTrackAdapter, KeyFrame]
	{
		KeyFrame(AnimTrackAdapter.GetLocalStartFrame());
	});
	FTickAnimationCB TickCallback = FTickAnimationCB::CreateLambda([KeyFrame, &Progress](float DeltaTime, FFrameNumber FrameNumber)
	{
		KeyFrame(FrameNumber);
		Progress.EnterProgressFrame(1);
	});
	FEndAnimationCB EndCallback = FEndAnimationCB::CreateLambda([]{});

	MovieSceneToolHelpers::BakeToSkelMeshToCallbacks(MovieScene,ParentSequencer.Get(),
		SkelMeshComp, Template, RootToLocalTransform, ExportOptions,
		InitCallback, StartCallback, TickCallback, EndCallback);

	InControlRig->OnPreAdditiveValuesApplication_AnyThread().Remove(DelegateHandle);
	ParentSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
}

bool FControlRigParameterTrackEditor::IsLayered(UMovieSceneControlRigParameterTrack* Track) const
{
	UControlRig* ControlRig = Track->GetControlRig();
	if (!ControlRig)
	{
		return false;
	}
	return ControlRig->IsAdditive();
}

void FControlRigParameterTrackEditor::ConvertIsLayered(UMovieSceneControlRigParameterTrack* Track)
{
	UControlRig* ControlRig = Track->GetControlRig();
	if (!ControlRig)
	{
		return;
	}

	const bool bSetAdditive = !ControlRig->IsAdditive();
	UControlRigSequencerEditorLibrary::SetControlRigLayeredMode(Track, bSetAdditive);
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
			UMovieSceneTrack* Track = nullptr;
			MenuBuilder.AddSubMenu(LOCTEXT("ControlRigText", "Control Rig"), FText(), FNewMenuDelegate::CreateSP(this, &FControlRigParameterTrackEditor::HandleAddTrackSubMenu, ObjectBindings, Track));
		}
	}
}


void FControlRigParameterTrackEditor::HandleAddTrackSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	if (CVarEnableAdditiveControlRigs->GetBool())
	{
		MenuBuilder.AddMenuEntry(
		LOCTEXT("IsLayeredControlRig", "Layered"),
		LOCTEXT("IsLayeredControlRigTooltip", "When checked, a layered control rig will be added"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FControlRigParameterTrackEditor::ToggleIsAdditiveControlRig),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FControlRigParameterTrackEditor::IsToggleIsAdditiveControlRig)
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	}

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
		LOCTEXT("AddControlRigClass", "Control Rig Classes"),
		LOCTEXT("AddControlRigClassTooltip", "Adds a Control Rig track based on selected class"),
		FNewMenuDelegate::CreateRaw(this, &FControlRigParameterTrackEditor::HandleAddControlRigSubMenu, ObjectBindings, Track)
	);
}

void FControlRigParameterTrackEditor::ToggleIsAdditiveControlRig()
{
	bIsLayeredControlRig = bIsLayeredControlRig ? false : true;
}

bool FControlRigParameterTrackEditor::IsToggleIsAdditiveControlRig()
{
	return bIsLayeredControlRig;
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

		const bool bCheckInversion = bIsLayeredControlRig;
		TSharedPtr<FControlRigClassFilter> ClassFilter = MakeShareable(new FControlRigClassFilter(bFilterAssetBySkeleton, bFilterAssetByAnimatableControls, bCheckInversion, Skeleton));
		Options.ClassFilters.Add(ClassFilter.ToSharedRef());
		Options.bShowNoneOption = false;
		Options.ClassViewerSortPredicate = ClassViewerSortPredicate;
		TArray<UClass*> ExistingRigs;
		USkeletalMeshComponent* SkeletalMeshComponent = AcquireSkeletalMeshFromObject(BoundObject, ParentSequencer);
		IterateTracks([&ExistingRigs, SkeletalMeshComponent](UMovieSceneControlRigParameterTrack* Track) -> bool
		{
			if (UControlRig* ControlRig = Track->GetControlRig())
			{
				if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
				{
					if (ObjectBinding.IsValid() && ObjectBinding->GetBoundObject() == SkeletalMeshComponent)
					{
						ExistingRigs.Add(ControlRig->GetClass());
					}
				}
			}
			return true;
		});
		TSharedPtr<FClassViewerHideAlreadyAddedRigsFilter> ExistingClassesFilter = MakeShareable(new FClassViewerHideAlreadyAddedRigsFilter(ExistingRigs));
		Options.ClassFilters.Add(ExistingClassesFilter.ToSharedRef());
		if (!ExistingRigs.Contains(UFKControlRig::StaticClass()))
		{
			Options.ExtraPickerCommonClasses.Add(UFKControlRig::StaticClass());
		}

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

void FControlRigParameterTrackEditor::AddControlRig(const UClass* InClass, UObject* BoundActor, FGuid ObjectBinding, UControlRig* InExistingControlRig)
{
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(GetSequencer()->GetFocusedMovieSceneSequence());
	UMovieSceneSequence* Sequence = GetSequencer()->GetFocusedMovieSceneSequence();
	FMovieSceneBindingProxy BindingProxy(ObjectBinding, Sequence);
	if (UMovieSceneTrack* Track = UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(World, LevelSequence, InClass, BindingProxy, bIsLayeredControlRig))
	{
		BindControlRig(CastChecked<UMovieSceneControlRigParameterTrack>(Track)->GetControlRig());
	}
}

void FControlRigParameterTrackEditor::AddControlRig(UClass* InClass, UObject* BoundActor, FGuid ObjectBinding)
{
	if (InClass == UFKControlRig::StaticClass())
	{
		AcquireSkeletonFromObjectGuid(ObjectBinding, &BoundActor, GetSequencer());
	}
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
			if (UObject* Object = ObjectBinding->GetBoundObject())
			{
				const FName Name(*ControlRig->GetName());
			
				const TArray<FName> ControlNames = ControlRig->CurrentControlSelection();
				for (const FName& ControlName : ControlNames)
				{
					AddControlKeys(Object, ControlRig, Name, ControlName, ChannelsToKey,
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
		URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
		
		//eval any space for this channel, if not additive section
		if (Section->GetBlendType().Get() != EMovieSceneBlendType::Additive)
		{
			TOptional<FMovieSceneControlRigSpaceBaseKey> SpaceKey = Section->EvaluateSpaceChannel(FrameTime, ControlName);
			if (SpaceKey.IsSet())
			{
				const FRigElementKey ControlKey = ControlElement->GetKey();
				switch (SpaceKey.GetValue().SpaceType)
				{
				case EMovieSceneControlRigSpaceType::Parent:
					ControlRig->SwitchToParent(ControlKey, RigHierarchy->GetDefaultParent(ControlKey), false, true);
					break;
				case EMovieSceneControlRigSpaceType::World:
					ControlRig->SwitchToParent(ControlKey, RigHierarchy->GetWorldSpaceReferenceKey(), false, true);
					break;
				case EMovieSceneControlRigSpaceType::ControlRig:
					ControlRig->SwitchToParent(ControlKey, SpaceKey.GetValue().ControlRigElement, false, true);
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
			case ERigControlType::ScaleFloat:
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
				// @MikeZ here I suppose we want to retrieve the rotation order and then also extract the Euler angles
				// instead of an assumed FRotator coming from the section?
				// EEulerRotationOrder RotationOrder = SomehowGetRotationOrder();
					
				TOptional <FEulerTransform> Value = Section->EvaluateTransformParameter(FrameTime, ControlName);
				if (Value.IsSet())
				{
					if (ControlElement->Settings.ControlType == ERigControlType::Transform)
					{
						FVector EulerAngle(Value.GetValue().Rotation.Roll, Value.GetValue().Rotation.Pitch, Value.GetValue().Rotation.Yaw);
						RigHierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
						ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, Value.GetValue().ToFTransform(), true, EControlRigSetKey::Never, bSetupUndo);
					}
					else if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
					{
						FTransformNoScale NoScale = Value.GetValue().ToFTransform();
						FVector EulerAngle(Value.GetValue().Rotation.Roll, Value.GetValue().Rotation.Pitch, Value.GetValue().Rotation.Yaw);
						RigHierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
						ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, NoScale, true, EControlRigSetKey::Never, bSetupUndo);
					}
					else if (ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
					{
						const FEulerTransform& Euler = Value.GetValue();
						FVector EulerAngle(Euler.Rotation.Roll, Euler.Rotation.Pitch, Euler.Rotation.Yaw);
						FQuat Quat = RigHierarchy->GetControlQuaternion(ControlElement, EulerAngle);
						RigHierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
						FRotator UERotator(Quat);
						FEulerTransform Transform = Euler;
						Transform.Rotation = UERotator;
						ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Transform, true, EControlRigSetKey::Never, bSetupUndo);

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
			if (Object != nullptr && (Object->IsA<UControlRigBlueprint>() || Object->IsA<UControlRigComponent>() || Object->IsA<URigVMBlueprintGeneratedClass>()))
			{
				FGuid Binding = InBinding.IsValid() ? InBinding : GetSequencer()->GetHandleToObject(InComponent, true /*bCreateHandle*/);
				if (Binding.IsValid())
				{
					UMovieSceneSequence* OwnerSequence = GetSequencer()->GetFocusedMovieSceneSequence();
					UMovieScene* OwnerMovieScene = OwnerSequence->GetMovieScene();
					UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(OwnerMovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding, NAME_None));
					if (Track == nullptr)
					{
						URigVMBlueprintGeneratedClass* RigClass = nullptr;
						if (UControlRigBlueprint* BPControlRig = Cast<UControlRigBlueprint>(Object))
						{
							RigClass = BPControlRig->GetRigVMBlueprintGeneratedClass();
						}
						else
						{
							RigClass = Cast<URigVMBlueprintGeneratedClass>(Object);
						}

						if (RigClass)
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
	TArray<USceneComponent*> ChildComponents;
	InComponent->GetChildrenComponents(false, ChildComponents);
	for (USceneComponent* ChildComponent : ChildComponents)
	{
		AddTrackForComponent(ChildComponent, FGuid());
	}
}

//test to see if actor has a constraint, in which case we need to add a constraint channel/key
//or a control rig in which case we create a track if cvar is off
void FControlRigParameterTrackEditor::HandleActorAdded(AActor* Actor, FGuid TargetObjectGuid)
{
	if (Actor == nullptr)
	{
		return;
	}

	//test for constraint
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(Actor->GetWorld());
	const TArray< TWeakObjectPtr<UTickableConstraint>> Constraints = Controller.GetAllConstraints();
	for (const TWeakObjectPtr<UTickableConstraint>& WeakConstraint : Constraints)
	{
		if (UTickableTransformConstraint* Constraint = WeakConstraint.IsValid() ? Cast<UTickableTransformConstraint>(WeakConstraint.Get()) : nullptr)
		{
			if (UObject* Child = Constraint->ChildTRSHandle ? Constraint->ChildTRSHandle->GetTarget().Get() : nullptr)
			{
				const AActor* TargetActor = Child->IsA<AActor>() ? Cast<AActor>(Child) : Child->GetTypedOuter<AActor>();
				if (TargetActor == Actor)
				{
					FMovieSceneConstraintChannelHelper::AddConstraintToSequencer(GetSequencer(), Constraint);
				}		
			}
		}
	}
	
	//test for control rig

	if (!CVarAutoGenerateControlRigTrack.GetValueOnGameThread())
	{
		return;
	}

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

void FControlRigParameterTrackEditor::OnActivateSequenceChanged(FMovieSceneSequenceIDRef ID)
{
	IterateTracks([this](UMovieSceneControlRigParameterTrack* Track)
	{
		if (UControlRig* ControlRig = Track->GetControlRig())
		{
			BindControlRig(ControlRig);
		}
		
		return false;
	});
	if (bControlRigEditModeWasOpen && GetSequencer()->IsLevelEditorSequencer())
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([this]()
		{
			//true here will turn it on
			FControlRigEditMode* ControlRigEditMode = GetEditMode(true);
			if (ControlRigEditMode)
			{
				GEditor->GetTimerManager()->SetTimerForNextTick([this, ControlRigEditMode]()
				{
					bool bSequencerSet = false;
					for (TWeakObjectPtr<UControlRig>& ControlRig : BoundControlRigs)
					{
						if (ControlRig.IsValid())
						{
							ControlRigEditMode->AddControlRigObject(ControlRig.Get(), GetSequencer());
							bSequencerSet = true;
							for (int32 Index = 0; Index < PreviousSelectedControlRigs.Num(); ++Index)
							{
								if (ControlRig.Get()->GetClass() == PreviousSelectedControlRigs[Index].Key)
								{
									for (const FName& ControlName : PreviousSelectedControlRigs[Index].Value)
									{
										ControlRig.Get()->SelectControl(ControlName, true);
									}
									PreviousSelectedControlRigs.RemoveAt(Index);
									break;
								}
							}
						}
					}
					if (bSequencerSet == false)
					{
						ControlRigEditMode->SetSequencer(GetSequencer());
					}
					PreviousSelectedControlRigs.Reset();
				});
			}
		});
	}
}


void FControlRigParameterTrackEditor::OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	const UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	FControlRigEditMode* ControlRigEditMode = GetEditMode();

	//if we have a valid control rig edit mode need to check and see the control rig in that mode is still in a track
	//if not we get rid of it.
	if (ControlRigEditMode && ControlRigEditMode->GetControlRigsArray(false /*bIsVisible*/).Num() != 0 && MovieScene && (DataChangeType == EMovieSceneDataChangeType::MovieSceneStructureItemRemoved ||
		DataChangeType == EMovieSceneDataChangeType::Unknown))
	{
		TArray<UControlRig*> ControlRigs = ControlRigEditMode->GetControlRigsArray(false /*bIsVisible*/);
		const float FPS = (float)GetSequencer()->GetFocusedDisplayRate().AsDecimal();
		for (UControlRig* ControlRig : ControlRigs)
		{
			if (ControlRig)
			{
				ControlRig->SetFramesPerSecond(FPS);

				bool bControlRigInTrack = false;
				IterateTracks([&bControlRigInTrack, ControlRig](const UMovieSceneControlRigParameterTrack* Track)
				{
					if(Track->GetControlRig() == ControlRig)
			        {
						bControlRigInTrack = true;
						return false;
			        }

					return true;
				});

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
		IterateTracksInMovieScene(*MovieScene, [this](const UMovieSceneControlRigParameterTrack* Track)
		{
			if (const UControlRig* ControlRig = Track->GetControlRig())
				{
					if (ControlRig->GetObjectBinding())
					{
						if (UControlRigComponent* ControlRigComponent = Cast<UControlRigComponent>(ControlRig->GetObjectBinding()->GetBoundObject()))
						{
							ControlRigComponent->Update(.1f); //delta time doesn't matter.
					}
				}
			}
			return false;
		});
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
	bool bEditModeExisted = ControlRigEditMode != nullptr;
	UControlRig* ControlRig = nullptr;

	TArray<const IKeyArea*> KeyAreas;
	const bool UseSelectedKeys = CVarSelectedKeysSelectControls.GetValueOnGameThread();
	GetSequencer()->GetSelectedKeyAreas(KeyAreas, UseSelectedKeys);
	if (KeyAreas.Num() <= 0)
	{ 
		if (FSlateApplication::Get().GetModifierKeys().IsShiftDown() == false && 
			FSlateApplication::Get().GetModifierKeys().IsControlDown() == false && ControlRigEditMode)
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
		const bool bSelectedSectionSetsSectionToKey = CVarSelectedSectionSetsSectionToKey.GetValueOnGameThread();
		if (bSelectedSectionSetsSectionToKey)
		{
			TMap<UMovieSceneTrack*, TSet<UMovieSceneControlRigParameterSection*>> TracksAndSections;
			using namespace UE::Sequencer;
			for (FViewModelPtr ViewModel : GetSequencer()->GetViewModel()->GetSelection()->Outliner)
			{
				if (TViewModelPtr<FTrackRowModel> TrackRowModel = ViewModel.ImplicitCast())
				{
					for (UMovieSceneSection* Section : TrackRowModel->GetSections())
					{
						if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section))
						{
							if (UMovieSceneTrack* Track = Section->GetTypedOuter<UMovieSceneTrack>())
							{
								TracksAndSections.FindOrAdd(Track).Add(CRSection);
							}
						}
					}
				}
			}

			//if we have only one  selected section per track and the track has more than one section we set that to the section to key
			for (TPair<UMovieSceneTrack*, TSet<UMovieSceneControlRigParameterSection*>>& TrackPair : TracksAndSections)
			{
				if (TrackPair.Key->GetAllSections().Num() > 0 && TrackPair.Value.Num() == 1)
				{
					TrackPair.Key->SetSectionToKey(TrackPair.Value.Array()[0]);
				}
			}

		}
		return;
	}
	SelectRigsAndControls(ControlRig, KeyAreas);

	// If the edit mode has been activated, we need to synchronize the external selection (possibly again to account for control rig control actors selection)
	if (!bEditModeExisted && GetEditMode() != nullptr)
	{
		FSequencerUtilities::SynchronizeExternalSelectionWithSequencerSelection(GetSequencer().ToSharedRef());
	}
	
}

void FControlRigParameterTrackEditor::SelectRigsAndControls(UControlRig* ControlRig, const TArray<const IKeyArea*>& KeyAreas)
{
	FControlRigEditMode* ControlRigEditMode = GetEditMode();
	
	//if selection set's section to key we need to keep track of selected sections for each track.
	const bool bSelectedSectionSetsSectionToKey = CVarSelectedSectionSetsSectionToKey.GetValueOnGameThread();
	TMap<UMovieSceneTrack*, TSet<UMovieSceneControlRigParameterSection*>> TracksAndSections;

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
								ControlElement->Settings.ControlType == ERigControlType::ScaleFloat ||
								ControlElement->Settings.ControlType == ERigControlType::Integer)
							{
								if (ControlElement->Settings.SupportsShape() || !Hierarchy->IsAnimatable(ControlElement))
								{

									if (const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(Hierarchy->GetFirstParent(ControlElement)))
									{
										if (const TSet<FName>* Controls = RigsAndControls.Find(ControlRig))
										{
											if (Controls->Contains(ParentControlElement->GetFName()))
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
			if (bSelectedSectionSetsSectionToKey)
			{
				if (UMovieSceneTrack* Track = MovieSection->GetTypedOuter<UMovieSceneTrack>())
				{
					TracksAndSections.FindOrAdd(Track).Add(MovieSection);
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
	//if we have only one  selected section per track and the track has more than one section we set that to the section to key
	for (TPair<UMovieSceneTrack*, TSet<UMovieSceneControlRigParameterSection*>>& TrackPair : TracksAndSections)
	{
		if (TrackPair.Key->GetAllSections().Num() > 0 && TrackPair.Value.Num() == 1)
		{
			TrackPair.Key->SetSectionToKey(TrackPair.Value.Array()[0]);
		}
	}
	if (bEndTransaction)
	{
		GEditor->EndTransaction();
	}
}


FMovieSceneTrackEditor::FFindOrCreateHandleResult FControlRigParameterTrackEditor::FindOrCreateHandleToObject(UObject* InObj, UControlRig* InControlRig)
{
	const bool bCreateHandleIfMissing = false;
	FName CreatedFolderName = NAME_None;

	FFindOrCreateHandleResult Result;
	bool bHandleWasValid = GetSequencer()->GetHandleToObject(InObj, bCreateHandleIfMissing).IsValid();

	Result.Handle = GetSequencer()->GetHandleToObject(InObj, bCreateHandleIfMissing, CreatedFolderName);
	Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();

	const UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

	// Prioritize a control rig parameter track on this component if it matches the handle
	if (Result.Handle.IsValid())
	{
		if (UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Result.Handle, NAME_None)))
		{
			if (InControlRig == nullptr || (Track->GetControlRig() == InControlRig))
			{
				return Result;
			}
		}
	}

	// If the owner has a control rig parameter track, let's use it
	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(InObj))
	{
		// If the owner has a control rig parameter track, let's use it
		UObject* OwnerObject = SceneComponent->GetOwner();
		const FGuid OwnerHandle = GetSequencer()->GetHandleToObject(OwnerObject, bCreateHandleIfMissing);
	    bHandleWasValid = OwnerHandle.IsValid();
	    if (OwnerHandle.IsValid())
	    {
		    if (UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), OwnerHandle, NAME_None)))
		    {
			    if (InControlRig == nullptr || (Track->GetControlRig() == InControlRig))
			    {
				    Result.Handle = OwnerHandle;
				    Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
				    return Result;
			    }
		    }
	    }
    
	    // If the component handle doesn't exist, let's use the owner handle
	    if (Result.Handle.IsValid() == false)
	    {
		    Result.Handle = OwnerHandle;
		    Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
		}
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

	IterateTracks([&Result, &bTrackExisted, ControlRig](UMovieSceneControlRigParameterTrack* Track)
	{
		if (Track->GetControlRig() == ControlRig)
		{
			Result.Track = Track;
			bTrackExisted = true;
		}
		return false;
	});

	// Only create track if the object handle is valid
	if (!Result.Track && bCreateTrackIfMissing && ObjectHandle.IsValid())
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = AddTrack(MovieScene, ObjectHandle, UMovieSceneControlRigParameterTrack::StaticClass(), PropertyName);
	}

	Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

	return Result;
}

UMovieSceneControlRigParameterTrack* FControlRigParameterTrackEditor::FindTrack(const UControlRig* InControlRig) const
{
	if (!GetSequencer().IsValid())
	{
		return nullptr;
	}
	
	UMovieSceneControlRigParameterTrack* FoundTrack = nullptr;
	IterateTracks([InControlRig, &FoundTrack](UMovieSceneControlRigParameterTrack* Track)
	{
		if (Track->GetControlRig() == InControlRig)
		{
			FoundTrack = Track;
			return false;
		}
		return true;
	});

	return FoundTrack;
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
	UTickableTransformConstraint::GetOnConstraintChanged().RemoveAll(this);
	
	const UMovieScene* MovieScene = GetSequencer().IsValid() && GetSequencer()->GetFocusedMovieSceneSequence() ? GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	IterateTracks([InOptionalControlRig](const UMovieSceneControlRigParameterTrack* Track)
	{
		if (InOptionalControlRig && Track->GetControlRig() != InOptionalControlRig)
		{
			return true;
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

		return false;
	});
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

			Data.Constraint = Cast<UTickableTransformConstraint>(ConstraintChannels[Index].GetConstraint().Get());

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

	if (!UTickableTransformConstraint::GetOnConstraintChanged().IsBoundToObject(this))
	{
		UTickableTransformConstraint::GetOnConstraintChanged().AddRaw(this, &FControlRigParameterTrackEditor::HandleConstraintPropertyChanged);
	}
}

void FControlRigParameterTrackEditor::HandleConstraintKeyDeleted(
	IMovieSceneConstrainedSection* InSection,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	const TArray<FKeyAddOrDeleteEventItem>& InDeletedItems) const
{
	if (FMovieSceneConstraintChannelHelper::bDoNotCompensate)
	{
		return;
	}
	
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

							const FConstraintAndActiveChannel* ConstraintChannel = InSection->GetConstraintChannel(Constraint->ConstraintID);
							if (!ConstraintChannel || ConstraintChannel->GetConstraint().Get() != Constraint)
							{
								return;
							}

							TSharedPtr<ISequencer> Sequencer = GetSequencer();
							
							const bool bCompensate = (InNotifyType == EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation);
							if (bCompensate && ConstraintChannel->GetConstraint().Get())
							{
								FMovieSceneConstraintChannelHelper::HandleConstraintRemoved(
									ConstraintChannel->GetConstraint().Get(),
									&ConstraintChannel->ActiveChannel,
									Sequencer,
									Section);
							}

							InSection->RemoveConstraintChannel(Constraint);

							if (Sequencer)
							{
								Sequencer->RecreateCurveEditor();
							}
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

void FControlRigParameterTrackEditor::HandleConstraintPropertyChanged(UTickableTransformConstraint* InConstraint, const FPropertyChangedEvent& InPropertyChangedEvent) const
{
	if (!IsValid(InConstraint))
	{
		return;
	}

	// find constraint section
	const UTransformableControlHandle* Handle = Cast<UTransformableControlHandle>(InConstraint->ChildTRSHandle);
	if (!IsValid(Handle) || !Handle->IsValid())
	{
		return;
	}

	const FConstraintChannelInterfaceRegistry& InterfaceRegistry = FConstraintChannelInterfaceRegistry::Get();	
	ITransformConstraintChannelInterface* Interface = InterfaceRegistry.FindConstraintChannelInterface(Handle->GetClass());
	if (!Interface)
	{
		return;
	}
	
	UMovieSceneSection* Section = Interface->GetHandleConstraintSection(Handle, GetSequencer());
	IMovieSceneConstrainedSection* ConstraintSection = Cast<IMovieSceneConstrainedSection>(Section);
	if (!ConstraintSection)
	{
		return;
	}

	// find corresponding channel
	const TArray<FConstraintAndActiveChannel>& ConstraintChannels = ConstraintSection->GetConstraintsChannels();
	const FConstraintAndActiveChannel* Channel = ConstraintChannels.FindByPredicate([InConstraint](const FConstraintAndActiveChannel& Channel)
	{
		return Channel.GetConstraint() == InConstraint;
	});

	if (!Channel)
	{
		return;
	}

	FMovieSceneConstraintChannelHelper::HandleConstraintPropertyChanged(
			InConstraint, Channel->ActiveChannel, InPropertyChangedEvent, GetSequencer(), Section);
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
	using namespace UE::Sequencer;

	if(ControlElement == nullptr)
	{
		return;
	}
	
	URigHierarchy* Hierarchy = Subject->GetHierarchy();
	static bool bIsSelectingIndirectControl = false;
	static TArray<FRigControlElement*> SelectedElements = {};

	// Avoid cyclic selection
	if (SelectedElements.Contains(ControlElement))
	{
		return;
	}

	if(ControlElement->CanDriveControls())
	{
		const TArray<FRigElementKey>& DrivenControls = ControlElement->Settings.DrivenControls;
		for(const FRigElementKey& DrivenKey : DrivenControls)
		{
			if(FRigControlElement* DrivenControl = Hierarchy->Find<FRigControlElement>(DrivenKey))
			{
				TGuardValue<bool> SubControlGuard(bIsSelectingIndirectControl, true);

				TArray<FRigControlElement*> NewSelection = SelectedElements;
				NewSelection.Add(ControlElement);
				TGuardValue<TArray<FRigControlElement*>> SelectedElementsGuard(SelectedElements, NewSelection);
				
				HandleControlSelected(Subject, DrivenControl, bSelected);
			}
		}
		if(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
		{
			return;
		}
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
						OtherControlElement->Settings.ControlType == ERigControlType::ScaleFloat ||
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

	const FName ControlRigName(*Subject->GetName());
	if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = Subject->GetObjectBinding())
	{
		UObject* Object = ObjectBinding->GetBoundObject();
		if (!Object)
		{
			return;
		}

		const bool bCreateTrack = false;
		const FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object, Subject);
		FFindOrCreateTrackResult TrackResult = FindOrCreateControlRigTrackForObject(HandleResult.Handle, Subject, ControlRigName, bCreateTrack);
		UMovieSceneControlRigParameterTrack* Track = CastChecked<UMovieSceneControlRigParameterTrack>(TrackResult.Track, ECastCheckedType::NullAllowed);
		if (Track)
		{
			FSelectionEventSuppressor SuppressSelectionEvents = GetSequencer()->GetViewModel()->GetSelection()->SuppressEvents();

			//Just select in section to key, if deselecting makes sure deselected everywhere
			if (bSelected == true)
			{
				UMovieSceneSection* Section = Track->GetSectionToKey();
				UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(Section);
				SelectSequencerNodeInSection(ParamSection, ControlElement->GetFName(), bSelected);
			}
			else
			{
				for (UMovieSceneSection* BaseSection : Track->GetAllSections())
				{
					if (UMovieSceneControlRigParameterSection* ParamSection = Cast< UMovieSceneControlRigParameterSection>(BaseSection))
					{
						SelectSequencerNodeInSection(ParamSection, ControlElement->GetFName(), bSelected);
					}
				}
			}

			SetUpEditModeIfNeeded(Subject);

			//Force refresh later, not now
			bSkipNextSelectionFromTimer = bSkipNextSelectionFromTimer ||
				(bIsSelectingIndirectControl && ControlElement->Settings.AnimationType == ERigControlAnimationType::AnimationControl);
			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshTree);

		}
	}
}

void FControlRigParameterTrackEditor::HandleOnPostConstructed(UControlRig* Subject, const FName& InEventName)
{
	if (IsInGameThread())
	{
		UControlRig* ControlRig = CastChecked<UControlRig>(Subject);

		if (GetSequencer().IsValid())
		{
			//refresh tree for ANY control rig may be FK or procedural
			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshTree);
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
	FTransform  Transform = ControlRig->GetControlLocalTransform(ControlElement->GetFName());
	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

	IterateTracks([this, ControlRig, ControlElement, Context](UMovieSceneControlRigParameterTrack* Track)
	{
		if (Track && Track->GetControlRig() == ControlRig)
		{
			FName Name(*ControlRig->GetName());
			if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
			{
				if (UObject* Object = ObjectBinding->GetBoundObject())
				{
					ESequencerKeyMode KeyMode = ESequencerKeyMode::AutoKey;
					if (Context.SetKey == EControlRigSetKey::Always)
					{
						KeyMode = ESequencerKeyMode::ManualKeyForced;
					}
					AddControlKeys(Object, ControlRig, Name, ControlElement->GetFName(), (EControlRigContextChannelToKey)Context.KeyMask, 
						KeyMode, Context.LocalTime);
					ControlChangedDuringUndoBracket++;
					return true;
				}
			}
		}
		return false;
	});
}

void FControlRigParameterTrackEditor::HandleControlUndoBracket(UControlRig* Subject, bool bOpenUndoBracket)
{
	if(IsInGameThread() && bOpenUndoBracket && ControlUndoBracket == 0)
	{
		FScopeLock ScopeLock(&ControlUndoTransactionMutex);
		ControlUndoTransaction = MakeShareable(new FScopedTransaction(LOCTEXT("KeyMultipleControls", "Auto-Key multiple controls")));
		ControlChangedDuringUndoBracket = 0;
	}

	ControlUndoBracket = FMath::Max<int32>(0, ControlUndoBracket + (bOpenUndoBracket ? 1 : -1));
	
	if(!bOpenUndoBracket && ControlUndoBracket == 0)
	{
		FScopeLock ScopeLock(&ControlUndoTransactionMutex);

		/*
		// canceling a sub transaction cancels everything to the top. we need to find a better mechanism for this.
		if(ControlChangedDuringUndoBracket == 0 && ControlUndoTransaction.IsValid())
		{
			ControlUndoTransaction->Cancel();
		}
		*/
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
	//reselect these control rigs since selection may get lost
	TMap<TWeakObjectPtr<UControlRig>, TArray<FName>> ReselectIfNeeded;
	// look for sections to update
	TArray<UMovieSceneControlRigParameterSection*> SectionsToUpdate;
	for (TWeakObjectPtr<UControlRig>& ControlRigPtr : BoundControlRigs)
	{
		if (ControlRigPtr.IsValid() == false)
		{
			continue;
		}
		TArray<FName> Selection = ControlRigPtr->CurrentControlSelection();
		if (Selection.Num() > 0)
		{
			ReselectIfNeeded.Add(ControlRigPtr, Selection);
		}
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

	if (ReselectIfNeeded.Num() > 0)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([ReselectIfNeeded]()
			{
				GEditor->GetTimerManager()->SetTimerForNextTick([ReselectIfNeeded]()
					{
						for (const TPair <TWeakObjectPtr<UControlRig>, TArray<FName>>& Pair : ReselectIfNeeded)
						{
							if (Pair.Key.IsValid())
							{
								Pair.Key->ClearControlSelection();
								for (const FName& ControlName : Pair.Value)
								{
									Pair.Key->SelectControl(ControlName, true);
								}
							}
						}
					});

			});
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
	URigHierarchy* Hierarchy = InControlRig->GetHierarchy();

	//Need seperate index fo bools,ints and enums and floats since there are seperate entries for each later when they are accessed by the set key stuff.
	int32 SpaceChannelIndex = 0;
	for (int32 LocalControlIndex = 0; LocalControlIndex < Controls.Num(); ++LocalControlIndex)
	{
		FRigControlElement* ControlElement = Controls[LocalControlIndex];
		check(ControlElement);

		if (!Hierarchy->IsAnimatable(ControlElement))
		{
			continue;
		}

		if (FChannelMapInfo* pChannelIndex = SectionToKey->ControlChannelMap.Find(ControlElement->GetFName()))
		{
			int32 ChannelIndex = pChannelIndex->ChannelIndex;
			const int32 MaskIndex = pChannelIndex->MaskIndex;

			bool bMaskKeyOut = (MaskIndex >= ControlsMask.Num() || ControlsMask[MaskIndex] == false);
			bool bSetKey = ParameterName.IsNone() || (ControlElement->GetFName() == ParameterName && !bMaskKeyOut);

			FRigControlValue ControlValue = InControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);

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
			case ERigControlType::ScaleFloat:
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
				FVector Vector = InControlRig->GetControlSpecifiedEulerAngle(ControlElement);
				FRotator Rotation = FRotator(Vector.Y, Vector.Z, Vector.X);
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

				if (bInConstraintSpace)
				{
					const uint32 ControlHash = UTransformableControlHandle::ComputeHash(InControlRig, ControlElement->GetFName());
					TOptional<FTransform> Transform = FTransformConstraintUtils::GetRelativeTransform(InControlRig->GetWorld(), ControlHash);
					if (Transform)
					{
						Translation = Transform->GetTranslation();
						if (InControlRig->GetHierarchy()->GetUsePreferredRotationOrder(ControlElement))
						{
							Rotation = ControlElement->PreferredEulerAngles.GetRotatorFromQuat(Transform->GetRotation());
							FVector Angle = Rotation.Euler();
							//need to wind rotators still
							ControlElement->PreferredEulerAngles.SetAngles(Angle, false, ControlElement->PreferredEulerAngles.RotationOrder, true);
							Angle = InControlRig->GetControlSpecifiedEulerAngle(ControlElement);
							Rotation = FRotator(Vector.Y, Vector.Z, Vector.X);
						}
						else
						{
							Rotation = Transform->GetRotation().Rotator();
						}
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

FKeyPropertyResult FControlRigParameterTrackEditor::AddKeysToControlRigHandle(UObject* InObject, UControlRig* InControlRig,
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
				ModifyOurGeneratedKeysByCurrentAndWeight(InObject, InControlRig, RigControlName, Track, SectionToKey, EvaluateTime, GeneratedKeys, Weight);
			}
			const UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(SectionToKey);
			if (!ParamSection->GetDoNotKey())
			{
				KeyPropertyResult |= AddKeysToSection(SectionToKey, KeyTime, GeneratedKeys, KeyMode, EKeyFrameTrackEditorSetDefault::SetDefaultOnAddKeys);
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
				const uint32 ControlHash = UTransformableControlHandle::ComputeHash(SectionControlRig, RigControlName);
				FMovieSceneConstraintChannelHelper::CompensateIfNeeded(GetSequencer(), ParamSection, OptionalKeyTime, ControlHash);
			}
		}
	}
	return KeyPropertyResult;
}

FKeyPropertyResult FControlRigParameterTrackEditor::AddKeysToControlRig(
	UObject* InObject, UControlRig* InControlRig, FFrameNumber KeyTime, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedKeys,
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

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(InObject, InControlRig);
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated = HandleResult.bWasCreated;
	KeyPropertyResult |= AddKeysToControlRigHandle(InObject, InControlRig, ObjectHandle, KeyTime, EvaluateTime, GeneratedKeys, KeyMode, TrackClass, ControlRigName, RigControlName);

	return KeyPropertyResult;
}

void FControlRigParameterTrackEditor::AddControlKeys(
	UObject* InObject,
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
	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(InObject, InControlRig);
	FGuid ObjectHandle = HandleResult.Handle;
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
		const FFrameNumber FrameTime = GetTimeForKey();
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

	auto OnKeyProperty = [=, this](FFrameNumber Time) -> FKeyPropertyResult
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
		
		return this->AddKeysToControlRig(InObject, InControlRig, LocalTime, EvaluateTime, *GeneratedKeys, KeyMode, UMovieSceneControlRigParameterTrack::StaticClass(), ControlRigName, RigControlName);
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
		case ERigControlType::ScaleFloat:
		{
			for (const FFloatInterrogationData& Val : InterrogationData.Iterate<FFloatInterrogationData>(UMovieSceneControlRigParameterSection::GetFloatInterrogationKey()))
			{
				if ((Val.ParameterName == ControlElement->GetFName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetFName());
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
				if ((Val.ParameterName == ControlElement->GetFName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetFName());
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
				if ((Val.ParameterName == ControlElement->GetFName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetFName());
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

				if ((Val.ParameterName == ControlElement->GetFName()))
				{
					pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetFName());
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

	
	// Check if the selected element is a section of the track
	bool bIsSection = Track->GetAllSections().Num() > 1;
	if (bIsSection)
	{
		TArray<TWeakObjectPtr<UObject>> TrackSections;
		for (UE::Sequencer::TViewModelPtr<UE::Sequencer::ITrackExtension> TrackExtension : GetSequencer()->GetViewModel()->GetSelection()->Outliner.Filter<UE::Sequencer::ITrackExtension>())
		{
			for (UMovieSceneSection* Section : TrackExtension->GetSections())
			{
				TrackSections.Add(Section);
			}
		}
		bIsSection = TrackSections.Num() > 0;
	}
	
	TArray<FRigControlFBXNodeAndChannels>* NodeAndChannels = Track->GetNodeAndChannelMappings(SectionToKey);

	MenuBuilder.BeginSection("Control Rig IO", LOCTEXT("ControlRigIO", "Control Rig I/O"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportControlRigFBX", "Import Control Rig FBX"),
			LOCTEXT("ImportControlRigFBXTooltip", "Import Control Rig animation from FBX"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ImportFBX, Track, SectionToKey, NodeAndChannels)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportControlRigFBX", "Export Control Rig FBX"),
			LOCTEXT("ExportControlRigFBXTooltip", "Export Control Rig animation to FBX"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ExportFBX, Track, SectionToKey)));
	}
	MenuBuilder.EndSection();

	if (!bIsSection)
	{
		MenuBuilder.BeginSection("Control Rig", LOCTEXT("ControlRig", "Control Rig"));
		{
			MenuBuilder.AddWidget(
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.ToolTipText(LOCTEXT("OrderTooltip", "Order for this Control Rig to evaluate compared to others on the same binding"))
				.Value_Lambda([Track]() { return Track->GetPriorityOrder(); })
				.OnValueChanged_Lambda([Track](int32 InValue) { Track->SetPriorityOrder(InValue); })
				,
				LOCTEXT("Order", "Order")
			);

			if (CVarEnableAdditiveControlRigs->GetBool())
			{
				MenuBuilder.AddMenuEntry(
					   LOCTEXT("ConvertIsLayeredControlRig", "Convert To Layered"),
					   LOCTEXT("ConvertIsLayeredControlRigToolTip", "Converts the Control Rig from an Absolute rig to a Layered rig"),
					   FSlateIcon(),
					   FUIAction(
						   FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::ConvertIsLayered, Track),
						   FCanExecuteAction(),
						   FIsActionChecked::CreateRaw(this, &FControlRigParameterTrackEditor::IsLayered, Track)
					   ),
					   NAME_None,
					   EUserInterfaceActionType::ToggleButton);
			}
		}
		MenuBuilder.EndSection();
	}

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
		}
		MenuBuilder.EndSection();

		MenuBuilder.AddMenuSeparator();
	}
	else if (UControlRig* LayeredRig = Cast<UControlRig>(Track->GetControlRig()))
	{
		if (LayeredRig->IsAdditive())
		{
			MenuBuilder.BeginSection("Layered Control Rig", LOCTEXT("LayeredControlRig", "Layered Control Rig"));
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("Bake Inverted Pose", "Bake Inverted Pose"),
					LOCTEXT("BakeInvertedPoseToolTip", "Bake inversion of the input pose into the rig"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateRaw(this, &FControlRigParameterTrackEditor::BakeInvertedPose, LayeredRig, Track)));
			}
			MenuBuilder.EndSection();
			MenuBuilder.AddMenuSeparator();
		}
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
	URigVMBlueprintGeneratedClass* RigClass = ControlRigBlueprint->GetRigVMBlueprintGeneratedClass();
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
	
	// Save Spawnable state as the default (with new name and skeletal mesh asset)
	{
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(NewGuid);
		GetSequencer()->GetSpawnRegister().SaveDefaultSpawnableState(*Spawnable, GetSequencer()->GetFocusedTemplateID(), *GetSequencer());
	}

	UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), NewGuid, NAME_None));
	if (Track == nullptr)
	{
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));
		check(CDO);

		AddControlRig(CDO->GetClass(), SpawnedSkeletalMeshActor->GetSkeletalMeshComponent(), NewGuid);
	}

	return true;
}

void FControlRigParameterTrackEditor::ImportFBX(UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection,
	TArray<FRigControlFBXNodeAndChannels>* NodeAndChannels)
{
	if (NodeAndChannels)
	{
		// NodeAndChannels will be deleted later
		MovieSceneToolHelpers::ImportFBXIntoControlRigChannelsWithDialog(GetSequencer().ToSharedRef(), NodeAndChannels);
	}
}

void FControlRigParameterTrackEditor::ExportFBX(UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection)
{
	if (InTrack && InTrack->GetControlRig())
	{
		// ControlComponentTransformsMapping will be deleted later
		MovieSceneToolHelpers::ExportFBXFromControlRigChannelsWithDialog(GetSequencer().ToSharedRef(), InTrack);
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

	TObjectPtr<UFKControlRig> AutoRig;
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

//////////////////////////////////////////////////////////////
/// SCollapseControlsWidget
///////////////////////////////////////////////////////////

/** Widget allowing collapsing of controls */
class SCollapseControlsWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCollapseControlsWidget)
		: _Sequencer(nullptr), _OwnerTrack(nullptr)
	{}

	SLATE_ARGUMENT(TSharedPtr<ISequencer>, Sequencer)
	SLATE_ARGUMENT(UMovieSceneTrack*, OwnerTrack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCollapseControlsWidget() override {}

	FReply OpenDialog(bool bModal = true);
	void CloseDialog();


private:
	void Collapse();

	TSharedPtr<ISequencer> Sequencer;
	TWeakObjectPtr<UMovieSceneTrack> OwnerTrack;
	//static to be reused
	static TOptional<FBakingAnimationKeySettings> CollapseControlsSettings;
	//structonscope for details panel
	TSharedPtr < TStructOnScope<FBakingAnimationKeySettings>> Settings;
	TWeakPtr<SWindow> DialogWindow;
	TSharedPtr<IStructureDetailsView> DetailsView;
};


TOptional<FBakingAnimationKeySettings> SCollapseControlsWidget::CollapseControlsSettings;

void SCollapseControlsWidget::Construct(const FArguments& InArgs)
{
	check(InArgs._Sequencer);
	Sequencer = InArgs._Sequencer;
	OwnerTrack = InArgs._OwnerTrack;

	if (CollapseControlsSettings.IsSet() == false)
	{
		CollapseControlsSettings = FBakingAnimationKeySettings();
		const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
		FFrameNumber CurrentTime = FrameTime.GetFrame();

		TRange<FFrameNumber> Range = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
		TArray<FFrameNumber> Keys;
		TArray < FKeyHandle> KeyHandles;

		CollapseControlsSettings.GetValue().StartFrame = Range.GetLowerBoundValue();
		CollapseControlsSettings.GetValue().EndFrame = Range.GetUpperBoundValue();
	}


	Settings = MakeShared<TStructOnScope<FBakingAnimationKeySettings>>();
	Settings->InitializeAs<FBakingAnimationKeySettings>(CollapseControlsSettings.GetValue());

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = false;
	ViewArgs.bShowObjectLabel = false;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	DetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface = Sequencer->GetNumericTypeInterface();
	DetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("FrameNumber",
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]() {return MakeShared<FFrameNumberDetailsCustomization>(NumericTypeInterface); }));
	DetailsView->SetStructureData(Settings);

	ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f)
				[
					DetailsView->GetWidget().ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.f)
				[

					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.HAlign(HAlign_Fill)
					[
						SNew(SSpacer)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("OK", "OK"))
						.OnClicked_Lambda([this, InArgs]()
							{
								Collapse();
								CloseDialog();
								return FReply::Handled();

							})
						.IsEnabled_Lambda([this]()
							{
								return (Settings.IsValid());
							})
					]
				]
			]
		];
}


void  SCollapseControlsWidget::Collapse()
{
	FBakingAnimationKeySettings* BakeSettings = Settings->Get();
	FControlRigParameterTrackEditor::CollapseAllLayers(Sequencer, OwnerTrack.Get(), *BakeSettings);

	CollapseControlsSettings = *BakeSettings;
}

class SCollapseControlsWidgetWindow : public SWindow
{
};

FReply SCollapseControlsWidget::OpenDialog(bool bModal)
{
	check(!DialogWindow.IsValid());

	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	TSharedRef<SCollapseControlsWidgetWindow> Window = SNew(SCollapseControlsWidgetWindow)
		.Title(LOCTEXT("CollapseControls", "Collapse Controls"))
		.CreateTitleBar(true)
		.Type(EWindowType::Normal)
		.SizingRule(ESizingRule::Autosized)
		.ScreenPosition(CursorPos)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::FirstShown)
		[
			AsShared()
		];

	Window->SetWidgetToFocusOnActivate(AsShared());

	DialogWindow = Window;

	Window->MoveWindowTo(CursorPos);

	if (bModal)
	{
		GEditor->EditorAddModalWindow(Window);
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}

	return FReply::Handled();
}

void SCollapseControlsWidget::CloseDialog()
{
	if (DialogWindow.IsValid())
	{
		DialogWindow.Pin()->RequestDestroyWindow();
		DialogWindow.Reset();
	}
}
///////////////
//end of SCollapseControlsWidget
/////////////////////
struct FKeyAndValuesAtFrame
{
	FFrameNumber Frame;
	TArray<FMovieSceneFloatValue> KeyValues;
	float FinalValue;
};

bool CollapseAllLayersPerKey(TSharedPtr<ISequencer>& SequencerPtr, UMovieSceneTrack* OwnerTrack, const FBakingAnimationKeySettings& InSettings)
{
	if (SequencerPtr.IsValid() && OwnerTrack)
	{
		TArray<UMovieSceneSection*> Sections = OwnerTrack->GetAllSections();
		return MovieSceneToolHelpers::CollapseSection(SequencerPtr, OwnerTrack, Sections, InSettings);
	}
	return false;
}

bool FControlRigParameterTrackEditor::CollapseAllLayers(TSharedPtr<ISequencer>&SequencerPtr, UMovieSceneTrack * OwnerTrack, const FBakingAnimationKeySettings &InSettings)
{
	if (InSettings.BakingKeySettings == EBakingKeySettings::KeysOnly)
	{
		return CollapseAllLayersPerKey(SequencerPtr, OwnerTrack, InSettings);
	}
	else
	{
		if (SequencerPtr.IsValid() && OwnerTrack)
		{
			TArray<UMovieSceneSection*> Sections = OwnerTrack->GetAllSections();
			//make sure right type
			if (Sections.Num() < 1)
			{
				UE_LOG(LogControlRigEditor, Log, TEXT("CollapseAllSections::No sections on track"));
				return false;
			}
			if (UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(Sections[0]))
			{
				if (ParameterSection->GetBlendType().Get() == EMovieSceneBlendType::Absolute)
				{
					FScopedTransaction Transaction(LOCTEXT("CollapseAllSections", "Collapse All Sections"));
					ParameterSection->Modify();
					UControlRig* ControlRig = ParameterSection->GetControlRig();
					FMovieSceneSequenceTransform RootToLocalTransform = SequencerPtr->GetFocusedMovieSceneSequenceTransform();

					FFrameNumber StartFrame = InSettings.StartFrame;
					FFrameNumber EndFrame = InSettings.EndFrame;
					TRange<FFrameNumber> Range(StartFrame, EndFrame);
					const FFrameRate& FrameRate = SequencerPtr->GetFocusedDisplayRate();
					const FFrameRate& TickResolution = SequencerPtr->GetFocusedTickResolution();

					//frames and (optional) tangents
					TArray<TPair<FFrameNumber, TArray<FMovieSceneTangentData>>> StoredTangents; //store tangents so we can reset them
					TArray<FFrameNumber> Frames;
					FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
					FrameRateInFrameNumber.Value *= InSettings.FrameIncrement;
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
						NameTransforms.Key = ControlElement->GetFName();
						NameTransforms.Value.SetNum(Frames.Num());
						ControlLocalTransforms.Add(NameTransforms);
					}

					//get all of the local 
					int32 Index = 0;
					for (Index = 0; Index < Frames.Num(); ++Index)
					{
						const FFrameNumber& FrameNumber = Frames[Index];
						FFrameTime GlobalTime(FrameNumber);
						GlobalTime = GlobalTime * RootToLocalTransform.InverseNoLooping();

						FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), SequencerPtr->GetPlaybackStatus()).SetHasJumped(true);

						SequencerPtr->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context);
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

					const EMovieSceneKeyInterpolation InterpMode = SequencerPtr->GetSequencerSettings()->GetKeyInterpolation();
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
								ControlRig->SetControlLocalTransform(TrailControlTransform.Key, TrailControlTransform.Value[Index], false, Context, false /*undo*/, true/* bFixEulerFlips*/);
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
					if (InSettings.bReduceKeys)
					{
						FKeyDataOptimizationParams Params;
						Params.bAutoSetInterpolation = true;
						Params.Tolerance = InSettings.Tolerance;
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
				else
				{
					UE_LOG(LogControlRigEditor, Log, TEXT("CollapseAllSections:: First section is not additive"));
					return false;
				}
			}
			else
			{
				UE_LOG(LogControlRigEditor, Log, TEXT("CollapseAllSections:: No Control Rig section"));
				return false;
			}
		}
		UE_LOG(LogControlRigEditor, Log, TEXT("CollapseAllSections:: Sequencer or track is invalid"));
	}
	return false;
}

void FControlRigParameterSection::CollapseAllLayers()
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	if (UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get()))
	{
		UMovieSceneTrack* OwnerTrack = ParameterSection->GetTypedOuter<UMovieSceneTrack>();
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		TSharedRef<SCollapseControlsWidget> BakeWidget =
			SNew(SCollapseControlsWidget)
			.Sequencer(Sequencer)
			.OwnerTrack(OwnerTrack);

		BakeWidget->OpenDialog(true);
	}
}

void FControlRigParameterSection::KeyZeroValue()
{
	UMovieSceneControlRigParameterSection* ParameterSection = CastChecked<UMovieSceneControlRigParameterSection>(WeakSection.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	FScopedTransaction Transaction(LOCTEXT("KeyZeroValue", "Key Zero Value"));
	ParameterSection->Modify();
	FFrameTime Time = SequencerPtr->GetLocalTime().Time;
	EMovieSceneKeyInterpolation DefaultInterpolation = SequencerPtr->GetKeyInterpolation();
	ParameterSection->KeyZeroValue(Time.GetFrame(), DefaultInterpolation, true);
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
	EMovieSceneKeyInterpolation DefaultInterpolation = SequencerPtr->GetKeyInterpolation();
	ParameterSection->KeyWeightValue(Time.GetFrame(), DefaultInterpolation, Val);
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
						FUIAction(FExecuteAction::CreateLambda([this] { CollapseAllLayers(); }))
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
						FUIAction(FExecuteAction::CreateLambda([this] { KeyZeroValue(); }))
					);
				}
				
				MenuBuilder.AddMenuEntry(
					LOCTEXT("KeyWeightZero", "Key Weight Zero"),
					LOCTEXT("KeyWeightZero_Tooltip", "Key a zero value on the Weight channel"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this] { KeyWeightValue(0.0f); }))
				);
				
				MenuBuilder.AddMenuEntry(
					LOCTEXT("KeyWeightOne", "Key Weight One"),
					LOCTEXT("KeyWeightOne_Tooltip", "Key a one value on the Weight channel"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this] { KeyWeightValue(1.0f); }))
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
					FExecuteAction::CreateLambda([this] { ShowSelectedControlsChannels(); }),
					FCanExecuteAction::CreateLambda([ControlRig] { return ControlRig->CurrentControlSelection().Num() > 0; } )
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowAllControls", "Show All Controls"),
				LOCTEXT("ShowAllControls_ToolTip", "Set active channels from all controls"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this] { return ShowAllControlsChannels(); }))
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
		for (const FRigControlElement* RigControl : Controls)
		{
			const FName RigName = RigControl->GetFName();
			if (ControlRig->IsControlSelected(RigName))
			{
				FChannelMapInfo* pChannelIndex = ParameterSection->ControlChannelMap.Find(RigName);
				if (pChannelIndex)
				{
					ParameterSection->SetControlsMask(pChannelIndex->MaskIndex, true);
				}
			}
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
		const FName& ChannelName = CategoryNamePaths.Last();
		const int32 Index = ParameterSection->GetConstraintsChannels().IndexOfByPredicate([ChannelName](const FConstraintAndActiveChannel& InChannel)
			{
				return InChannel.GetConstraint().Get() ? InChannel.GetConstraint()->GetFName() == ChannelName : false;
			});
		// remove constraint channel if there are no keys
		const FConstraintAndActiveChannel* ConstraintChannel = Index != INDEX_NONE ? &(ParameterSection->GetConstraintsChannels()[Index]): nullptr;
		if (ConstraintChannel && ConstraintChannel->ActiveChannel.GetNumKeys() == 0)
		{
			if (ParameterSection->TryModify())
			{
				ParameterSection->RemoveConstraintChannel(ConstraintChannel->GetConstraint().Get());
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
		AssetPickerConfig.bAddFilterUI = true;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateRaw(this, &FControlRigParameterSection::ShouldFilterAssetForFK);
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateUObject(Skeleton, &USkeleton::ShouldFilterAsset, TEXT("Skeleton"));
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
			EMovieSceneKeyInterpolation DefaultInterpolation = SequencerPtr->GetKeyInterpolation();
			if (!Section->LoadAnimSequenceIntoThisSection(AnimSequence, MovieScene, SkelMeshComp, false, 0.1f, true, StartFrame, DefaultInterpolation))
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
			if (bForceActivate)
			{
				FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(EditorModetools->GetActiveMode(FControlRigEditMode::ModeName));
				if (EditMode && EditMode->GetToolkit().IsValid() == false)
				{
					EditMode->Enter();
				}
			}
		}

		return static_cast<FControlRigEditMode*>(EditorModetools->GetActiveMode(FControlRigEditMode::ModeName));
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE
