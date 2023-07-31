// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/TransformTrackEditor.h"
#include "GameFramework/Actor.h"
#include "Framework/Commands/Commands.h"
#include "Animation/AnimSequence.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Components/SceneComponent.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "GameFramework/Character.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Sections/TransformPropertySection.h"
#include "SequencerUtilities.h"
#include "MovieSceneToolHelpers.h"
#include "Animation/AnimData/AnimDataModel.h"

#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "MovieSceneTracksComponentTypes.h"

#include "Tracks/IMovieSceneTransformOrigin.h"
#include "IMovieScenePlaybackClient.h"

#include "Editor.h"
#include "LevelEditorViewport.h"
#include "TransformConstraint.h"
#include "MovieSceneConstraintChannelHelper.h"
#include "Misc/TransactionObjectEvent.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "MovieScene_TransformTrack"

void GetActorAndSceneComponentFromObject( UObject* Object, AActor*& OutActor, USceneComponent*& OutSceneComponent )
{
	OutActor = Cast<AActor>( Object );
	if ( OutActor != nullptr && OutActor->GetRootComponent() )
	{
		OutSceneComponent = OutActor->GetRootComponent();
	}
	else
	{
		// If the object wasn't an actor attempt to get it directly as a scene component and then get the actor from there.
		OutSceneComponent = Cast<USceneComponent>( Object );
		if ( OutSceneComponent != nullptr )
		{
			OutActor = Cast<AActor>( OutSceneComponent->GetOuter() );
		}
	}
}


FName F3DTransformTrackEditor::TransformPropertyName("Transform");

F3DTransformTrackEditor::F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FKeyframeTrackEditor<UMovieScene3DTransformTrack>( InSequencer ) 
{
	// Listen for actor/component movement
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddRaw(this, &F3DTransformTrackEditor::OnPrePropertyChanged);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &F3DTransformTrackEditor::OnPostPropertyChanged);
	if (GEditor != nullptr)
	{
		GEditor->RegisterForUndo(this);
	}
}

F3DTransformTrackEditor::~F3DTransformTrackEditor()
{
	if (GEditor != nullptr)
	{
		GEditor->UnregisterForUndo(this);
	}
}
//for 5.2 we will move this over to the header
static TSet<TWeakObjectPtr<UMovieScene3DTransformSection>> SectionsToClear;

void F3DTransformTrackEditor::OnRelease()
{
	ClearOutConstraintDelegates();
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	if (OnSceneComponentConstrainedHandle.IsValid())
	{
		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		Controller.OnSceneComponentConstrained().Remove(OnSceneComponentConstrainedHandle);
		OnSceneComponentConstrainedHandle.Reset();
	}

	for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown)
		{
			LevelVC->ViewFOV = LevelVC->FOVAngle;
		}
	}
}

TSharedRef<ISequencerTrackEditor> F3DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DTransformTrackEditor( InSequencer ) );
}


bool F3DTransformTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DTransformTrack::StaticClass();
}


void F3DTransformTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>( Track );

	auto AnimSubMenuDelegate = [](FMenuBuilder& InMenuBuilder, TSharedRef<ISequencer> InSequencer, UMovieScene3DTransformTrack* InTransformTrack)
	{
		UMovieSceneSequence* Sequence = InSequencer->GetFocusedMovieSceneSequence();

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateStatic(&F3DTransformTrackEditor::ImportAnimSequenceTransforms, InSequencer, InTransformTrack);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateStatic(&F3DTransformTrackEditor::ImportAnimSequenceTransformsEnterPressed, InSequencer, InTransformTrack);
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
		AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		InMenuBuilder.AddWidget(
			SNew(SBox)
			.WidthOverride(200.0f)
			.HeightOverride(400.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			], 
			FText(), true, false);
	};

	MenuBuilder.AddSubMenu(
		NSLOCTEXT( "Sequencer", "ImportTransforms", "Import From Animation Root" ),
		NSLOCTEXT( "Sequencer", "ImportTransformsTooltip", "Import transform keys from an animation sequence's root motion." ),
		FNewMenuDelegate::CreateLambda(AnimSubMenuDelegate, GetSequencer().ToSharedRef(), TransformTrack)
	);

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


TSharedRef<ISequencerSection> F3DTransformTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(&SectionObject);
	if (Section)
	{
		Section->ConstraintChannelAdded().AddRaw(this, &F3DTransformTrackEditor::HandleOnConstraintAdded);
	}
	//if there are channels already we need to act like they were added
	TArray<FConstraintAndActiveChannel>& Channels = Section->GetConstraintsChannels();
	for (FConstraintAndActiveChannel& Channel : Channels)
	{
		HandleOnConstraintAdded(Section, &Channel.ActiveChannel);
	}
	return MakeShared<FTransformSection>(SectionObject, GetSequencer());
}

bool F3DTransformTrackEditor::HasTransformTrack(UObject& InObject) const
{
	FGuid Binding = GetSequencer()->FindObjectId(InObject, GetSequencer()->GetFocusedTemplateID());
	if (Binding.IsValid())
	{
		if (GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(Binding, TransformPropertyName))
		{
			return true;
		}
	}

	return false;
}


void F3DTransformTrackEditor::OnPreTransformChanged( UObject& InObject )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	AActor* Actor = Cast<AActor>(&InObject);
	// If Sequencer is allowed to autokey and we are clicking on an Actor that can be autokeyed
	if(Actor && !Actor->IsEditorOnly())
	{
		AActor* ActorThatChanged = nullptr;
		USceneComponent* SceneComponentThatChanged = nullptr;
		GetActorAndSceneComponentFromObject(&InObject, ActorThatChanged, SceneComponentThatChanged);

		if( SceneComponentThatChanged )
		{
			// Cache off the existing transform so we can detect which components have changed
			// and keys only when something has changed
			FTransformData Transform( SceneComponentThatChanged );
			
			ObjectToExistingTransform.Add(&InObject, Transform);
			
			bool bObjectHasTransformTrack = HasTransformTrack(InObject);
			bool bComponentHasTransformTrack = HasTransformTrack(*SceneComponentThatChanged);

			// If there's no existing track, key the existing transform on pre-change so that the current transform before interaction is stored as the default state. 
			// If keying only happens at the end of interaction, the transform after interaction would end up incorrectly as the default state.
			if (!bObjectHasTransformTrack && !bComponentHasTransformTrack)
			{
				TOptional<FTransformData> LastTransform;

				UObject* ObjectToKey = nullptr;
				if (bComponentHasTransformTrack)
				{
					ObjectToKey = SceneComponentThatChanged;
				}
				// If the root component broadcasts a change, we want to key the actor instead
				else if (ActorThatChanged && ActorThatChanged->GetRootComponent() == &InObject)
				{
					ObjectToKey = ActorThatChanged;
				}
				else
				{
					ObjectToKey = &InObject;
				}

				AddTransformKeys(ObjectToKey, LastTransform, Transform, EMovieSceneTransformChannel::All, ESequencerKeyMode::AutoKey);
			}
		}
	}
}


void F3DTransformTrackEditor::OnTransformChanged( UObject& InObject )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	AActor* Actor = nullptr;
	USceneComponent* SceneComponentThatChanged = nullptr;
	GetActorAndSceneComponentFromObject(&InObject, Actor, SceneComponentThatChanged);

	// If the Actor that just finished transforming doesn't have autokey disabled
	if( SceneComponentThatChanged != nullptr && (Actor && !Actor->IsEditorOnly()))
	{
		// Find an existing transform if possible.  If one exists we will compare against the new one to decide what components of the transform need keys
		TOptional<FTransformData> ExistingTransform;
		if (const FTransformData* Found = ObjectToExistingTransform.Find( &InObject ))
		{
			ExistingTransform = *Found;
		}

		// Remove it from the list of cached transforms. 
		// @todo sequencer livecapture: This can be made much for efficient by not removing cached state during live capture situation
		ObjectToExistingTransform.Remove( &InObject );

		// Build new transform data
		FTransformData NewTransformData( SceneComponentThatChanged );

		bool bComponentHasTransformTrack = HasTransformTrack(*SceneComponentThatChanged);

		UObject* ObjectToKey = nullptr;
		if (bComponentHasTransformTrack)
		{
			ObjectToKey = SceneComponentThatChanged;
		}
		// If the root component broadcasts a change, we want to key the actor instead
		else if (Actor && Actor->GetRootComponent() == &InObject)
		{
			ObjectToKey = Actor;
		}
		else
		{
			ObjectToKey = &InObject;
		}

		AddTransformKeys(ObjectToKey, ExistingTransform, NewTransformData, EMovieSceneTransformChannel::All, ESequencerKeyMode::AutoKey);
	}
}

void F3DTransformTrackEditor::OnPrePropertyChanged(UObject* InObject, const FEditPropertyChain& InPropertyChain)
{
	FProperty* PropertyAboutToChange = InPropertyChain.GetActiveMemberNode()->GetValue();
	const FName MemberPropertyName = PropertyAboutToChange != nullptr ? PropertyAboutToChange->GetFName() : NAME_None;
	const bool bTransformationToChange =
		(MemberPropertyName == USceneComponent::GetRelativeLocationPropertyName() ||
		 MemberPropertyName == USceneComponent::GetRelativeRotationPropertyName() ||
		 MemberPropertyName == USceneComponent::GetRelativeScale3DPropertyName());

	if (InObject && bTransformationToChange)
	{
		OnPreTransformChanged(*InObject);
	}
}

void F3DTransformTrackEditor::OnPostPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName MemberPropertyName = InPropertyChangedEvent.MemberProperty != nullptr ? InPropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	const bool bTransformationChanged =
		(MemberPropertyName == USceneComponent::GetRelativeLocationPropertyName() ||
			MemberPropertyName == USceneComponent::GetRelativeRotationPropertyName() ||
			MemberPropertyName == USceneComponent::GetRelativeScale3DPropertyName());

	if (InObject && bTransformationChanged)
	{
		OnTransformChanged(*InObject);
	}
}

void F3DTransformTrackEditor::OnPreSaveWorld(UWorld* World)
{
	LockedCameraBindings.Reset();

	TArray<FGuid> CameraBindingIDs;
	GetSequencer()->GetCameraObjectBindings(CameraBindingIDs);
	for (const FGuid& CameraBindingID : CameraBindingIDs)
	{
		if (IsCameraBindingLocked(CameraBindingID))
		{
			LockedCameraBindings.Add(CameraBindingID);
		}
	}
}

void F3DTransformTrackEditor::OnPostSaveWorld(UWorld* World)
{
	for (const FGuid& CameraBindingID : LockedCameraBindings)
	{
		LockCameraBinding(true, CameraBindingID);
	}

	LockedCameraBindings.Reset();
}

bool F3DTransformTrackEditor::CanAddTransformKeysForSelectedObjects() const
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
	TArray<UObject*> SelectedObjects;
	for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(*It);
		if (SceneComponent)
		{
			return true;
		}
	}

	if (SelectedObjects.Num() == 0)
	{
		USelection* CurrentSelection = GEditor->GetSelectedActors();
		CurrentSelection->GetSelectedObjects(AActor::StaticClass(), SelectedObjects);
	}
	return SelectedObjects.Num() > 0;
}

void F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects( EMovieSceneTransformChannel Channel )
{
	// WASD hotkeys to fly the viewport can conflict with hotkeys for setting keyframes (ie. s). 
	// If the viewport is moving, disregard setting keyframes.
	for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsMovingCamera())
		{
			return;
		}
	}

	TArray<UObject*> SelectedObjects;
	for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(*It);
		if (SceneComponent)
		{
			SelectedObjects.Add(SceneComponent);
		}
	}
	
	if (SelectedObjects.Num() == 0)
	{
		USelection* CurrentSelection = GEditor->GetSelectedActors();
		CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedObjects );
	}

	for (TArray<UObject*>::TIterator It(SelectedObjects); It; ++It)
	{
		AddTransformKeysForObject(*It, Channel, ESequencerKeyMode::ManualKeyForced);
	}
}

void F3DTransformTrackEditor::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectGuid, const UClass* ObjectClass)
{
	// If this is a camera track, add a button to lock the viewport to the camera
	EditBox.Get()->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(SCheckBox)		
				.Style( &FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBoxAlt"))
				.Type(ESlateCheckBoxType::CheckBox)
				.Padding(FMargin(0.f))
				.IsFocusable(false)
				.Visibility(this, &F3DTransformTrackEditor::IsCameraVisible, ObjectGuid)
				.IsChecked(this, &F3DTransformTrackEditor::IsCameraLocked, ObjectGuid)
				.OnCheckStateChanged(this, &F3DTransformTrackEditor::OnLockCameraClicked, ObjectGuid)
				.ToolTipText(this, &F3DTransformTrackEditor::GetLockCameraToolTip, ObjectGuid)
				.CheckedImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedHoveredImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
				.CheckedPressedImage(FAppStyle::GetBrush("Sequencer.LockCamera"))
				.UncheckedImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedHoveredImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
				.UncheckedPressedImage(FAppStyle::GetBrush("Sequencer.UnlockCamera"))
		];
};
void F3DTransformTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass != nullptr && (ObjectClass->IsChildOf(AActor::StaticClass()) || ObjectClass->IsChildOf(USceneComponent::StaticClass())))
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AddTransform", "Transform"),
			NSLOCTEXT("Sequencer", "AddTransformTooltip", "Adds a transform track."),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::AddTransformKeysForHandle, ObjectBindings, EMovieSceneTransformChannel::All, ESequencerKeyMode::ManualKey )
			)
		);
	}
}


bool F3DTransformTrackEditor::CanAddTransformTrackForActorHandle( FGuid ObjectBinding ) const
{
	if (GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding, TransformPropertyName))
	{
		return false;
	}
	return true;
}

EVisibility F3DTransformTrackEditor::IsCameraVisible(FGuid ObjectGuid) const
{
	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Collapsed;
}

ECheckBoxState F3DTransformTrackEditor::IsCameraLocked(FGuid ObjectGuid) const
{
	return IsCameraBindingLocked(ObjectGuid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool F3DTransformTrackEditor::IsCameraBindingLocked(FGuid ObjectGuid) const
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		// First, check the active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->GetViewMode() != VMI_Unknown)
			{
				if (LevelVC->Viewport == ActiveViewport)
				{
					return (CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()));
				}
			}
		}

		// Otherwise check all other viewports
		for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
			{
				return true;
			}
		}
	}

	return false;
}

void F3DTransformTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid)
{
	LockCameraBinding((CheckBoxState == ECheckBoxState::Checked), ObjectGuid);
}

void F3DTransformTrackEditor::LockCameraBinding(bool bLock, FGuid ObjectGuid)
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	// Lock the active viewport to the camera
	if (bLock)
	{
		// Set the active viewport or any viewport if there is no active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		FLevelEditorViewportClient* LevelVC = nullptr;

		for(FLevelEditorViewportClient* Viewport : GEditor->GetLevelViewportClients())
		{		
			if (Viewport && Viewport->GetViewMode() != VMI_Unknown && Viewport->AllowsCinematicControl())
			{
				LevelVC = Viewport;

				if (LevelVC->Viewport == ActiveViewport)
				{
					break;
				}
			}
		}

		if (LevelVC != nullptr && CameraActor.IsValid())
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(CameraActor.Get());

			if (CameraComponent && CameraComponent->ProjectionMode == ECameraProjectionMode::Type::Perspective)
			{
				if (LevelVC->GetViewportType() != LVT_Perspective)
				{
					LevelVC->SetViewportType(LVT_Perspective);
				}
			}

			GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
			LevelVC->SetCinematicActorLock(nullptr);
			LevelVC->SetActorLock(CameraActor.Get());
			LevelVC->bLockedCameraView = true;
			LevelVC->UpdateViewForLockedActor();
			LevelVC->Invalidate();
		}
	}
	// Otherwise, clear all locks on the camera
	else
	{
		ClearLockedCameras(CameraActor.Get());
	}
}

void F3DTransformTrackEditor::ClearLockedCameras(AActor* LockedActor)
{
	for(FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->GetViewMode() != VMI_Unknown && LevelVC->AllowsCinematicControl())
		{
			if (LevelVC->IsActorLocked(LockedActor))
			{
				LevelVC->SetCinematicActorLock(nullptr);
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->ViewFOV = LevelVC->FOVAngle;
				LevelVC->RemoveCameraRoll();
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
	}
}


FText F3DTransformTrackEditor::GetLockCameraToolTip(FGuid ObjectGuid) const
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		return IsCameraLocked(ObjectGuid) == ECheckBoxState::Checked ?
			FText::Format(LOCTEXT("UnlockCamera", "Unlock {0} from Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel())) :
			FText::Format(LOCTEXT("LockCamera", "Lock {0} to Selected Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel()));
	}
	return FText();
}

double UnwindChannel(const double& OldValue, double NewValue)
{
	while( NewValue - OldValue > 180.0f )
	{
		NewValue -= 360.0f;
	}
	while( NewValue - OldValue < -180.0f )
	{
		NewValue += 360.0f;
	}
	return NewValue;
}
FRotator UnwindRotator(const FRotator& InOld, const FRotator& InNew)
{
	FRotator Result;
	Result.Pitch = UnwindChannel(InOld.Pitch, InNew.Pitch);
	Result.Yaw   = UnwindChannel(InOld.Yaw, InNew.Yaw);
	Result.Roll  = UnwindChannel(InOld.Roll, InNew.Roll);
	return Result;
}


void F3DTransformTrackEditor::GetTransformKeys( const TOptional<FTransformData>& LastTransform, const FTransformData& CurrentTransform, EMovieSceneTransformChannel ChannelsToKey, UObject* Object, UMovieSceneSection* Section, FGeneratedTrackKeys& OutGeneratedKeys)
{
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
	EMovieSceneTransformChannel TransformMask = TransformSection->GetMask().GetChannels();

	using namespace UE::MovieScene;

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	// If key all is enabled, for a key on all the channels
	bool bLastVectorIsValid = LastTransform.IsSet();
	if (SequencerPtr->GetKeyGroupMode() == EKeyGroupMode::KeyAll)
	{
		bLastVectorIsValid = false;
		ChannelsToKey = EMovieSceneTransformChannel::All;
	}

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	FTransformData RecomposedTransform = RecomposeTransform(CurrentTransform, Object, Section);

	// Get the channel indices for our curve channels.
	// We will get invalid handles for any channels that are anything else than FMovieSceneDoubleChannel (such as when
	// a channel has been overriden by a procedural channel). This means that the value setters below will get null
	// channels and will simply ignore those non-double channels. This is what we want, since we assume (perhaps
	// incorrectly) that overriden channels are most probably not keyable.
	FMovieSceneChannelProxy& SectionChannelProxy = Section->GetChannelProxy();
	TMovieSceneChannelHandle<FMovieSceneDoubleChannel> ChannelHandles[] = {
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.X"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.Y"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.Z"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.X"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.Y"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.Z"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.X"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.Y"),
		SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.Z")
	};

	// Set translation keys/defaults
	{
		bool bKeyX = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::TranslationX);
		bool bKeyY = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::TranslationY);
		bool bKeyZ = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::TranslationZ);

		if (bLastVectorIsValid)
		{
			bKeyX &= !FMath::IsNearlyEqual(LastTransform->Translation.X, CurrentTransform.Translation.X);
			bKeyY &= !FMath::IsNearlyEqual(LastTransform->Translation.Y, CurrentTransform.Translation.Y);
			bKeyZ &= !FMath::IsNearlyEqual(LastTransform->Translation.Z, CurrentTransform.Translation.Z);
		}

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

		FVector KeyVector = RecomposedTransform.Translation;

		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[0].GetChannelIndex(), (double)KeyVector.X, bKeyX));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[1].GetChannelIndex(), (double)KeyVector.Y, bKeyY));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[2].GetChannelIndex(), (double)KeyVector.Z, bKeyZ));
	}

	// Set rotation keys/defaults
	{
		bool bKeyX = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::RotationX);
		bool bKeyY = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::RotationY);
		bool bKeyZ = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::RotationZ);

		FRotator KeyRotator = CurrentTransform.Rotation;
		if (bLastVectorIsValid)
		{
			KeyRotator = UnwindRotator(LastTransform->Rotation, CurrentTransform.Rotation);

			bKeyX &= !FMath::IsNearlyEqual(LastTransform->Rotation.Roll,  KeyRotator.Roll);
			bKeyY &= !FMath::IsNearlyEqual(LastTransform->Rotation.Pitch, KeyRotator.Pitch);
			bKeyZ &= !FMath::IsNearlyEqual(LastTransform->Rotation.Yaw,   KeyRotator.Yaw);
		}

		if (GetSequencer()->GetKeyGroupMode() == EKeyGroupMode::KeyGroup && ( bKeyX || bKeyY || bKeyZ) )
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

		// Do we need to unwind re-composed rotations?
		KeyRotator = UnwindRotator(CurrentTransform.Rotation, RecomposedTransform.Rotation);
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[3].GetChannelIndex(), (double)KeyRotator.Roll, bKeyX));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[4].GetChannelIndex(), (double)KeyRotator.Pitch, bKeyY));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[5].GetChannelIndex(), (double)KeyRotator.Yaw, bKeyZ));
	}

	// Set scale keys/defaults
	{
		bool bKeyX = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::ScaleX);
		bool bKeyY = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::ScaleY);
		bool bKeyZ = EnumHasAnyFlags(ChannelsToKey, EMovieSceneTransformChannel::ScaleZ);

		if (bLastVectorIsValid)
		{
			bKeyX &= !FMath::IsNearlyEqual(LastTransform->Scale.X, CurrentTransform.Scale.X);
			bKeyY &= !FMath::IsNearlyEqual(LastTransform->Scale.Y, CurrentTransform.Scale.Y);
			bKeyZ &= !FMath::IsNearlyEqual(LastTransform->Scale.Z, CurrentTransform.Scale.Z);
		}

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

		FVector KeyVector = RecomposedTransform.Scale;
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[6].GetChannelIndex(), (double)KeyVector.X, bKeyX));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[7].GetChannelIndex(), (double)KeyVector.Y, bKeyY));
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(ChannelHandles[8].GetChannelIndex(), (double)KeyVector.Z, bKeyZ));
	}
}

FTransform F3DTransformTrackEditor::GetTransformOrigin() const
{
	FTransform TransformOrigin;

	const IMovieScenePlaybackClient*  Client       = GetSequencer()->GetPlaybackClient();
	const UObject*                    InstanceData = Client ? Client->GetInstanceData() : nullptr;
	const IMovieSceneTransformOrigin* RawInterface = Cast<const IMovieSceneTransformOrigin>(InstanceData);

	const bool bHasInterface = RawInterface || (InstanceData && InstanceData->GetClass()->ImplementsInterface(UMovieSceneTransformOrigin::StaticClass()));
	if (bHasInterface)
	{
		// Retrieve the current origin
		TransformOrigin = RawInterface ? RawInterface->GetTransformOrigin() : IMovieSceneTransformOrigin::Execute_BP_GetTransformOrigin(InstanceData);
	}

	return TransformOrigin;
}

void F3DTransformTrackEditor::AddTransformKeysForHandle(TArray<FGuid> ObjectHandles, EMovieSceneTransformChannel ChannelToKey, ESequencerKeyMode KeyMode)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddTransformTrack", "Add Transform Track"));
	
	for (FGuid ObjectHandle : ObjectHandles)
	{
		for (TWeakObjectPtr<UObject> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectHandle))
		{
			AddTransformKeysForObject(Object.Get(), ChannelToKey, KeyMode);
		}
	}
}


void F3DTransformTrackEditor::AddTransformKeysForObject( UObject* Object, EMovieSceneTransformChannel ChannelToKey, ESequencerKeyMode KeyMode )
{
	USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object);
	if ( SceneComponent != nullptr )
	{
		FTransformData CurrentTransform( SceneComponent );
		AddTransformKeys( Object, TOptional<FTransformData>(), CurrentTransform, ChannelToKey, KeyMode );
	}
}


void F3DTransformTrackEditor::AddTransformKeys( UObject* ObjectToKey, const TOptional<FTransformData>& LastTransform, const FTransformData& CurrentTransform, EMovieSceneTransformChannel ChannelsToKey, ESequencerKeyMode KeyMode )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	auto InitializeNewTrack = [](UMovieScene3DTransformTrack* NewTrack)
	{
		NewTrack->SetPropertyNameAndPath(TransformPropertyName, TransformPropertyName.ToString());
	};
	auto GenerateKeys = [=](UMovieSceneSection* Section, FGeneratedTrackKeys& GeneratedKeys)
	{
		this->GetTransformKeys(LastTransform, CurrentTransform, ChannelsToKey, ObjectToKey, Section, GeneratedKeys);
	};
	auto OnKeyProperty = [=](FFrameNumber Time) -> FKeyPropertyResult
	{
		FKeyPropertyResult KeyPropertyResult = this->AddKeysToObjects(MakeArrayView(&ObjectToKey, 1), Time,  KeyMode, UMovieScene3DTransformTrack::StaticClass(), TransformPropertyName, InitializeNewTrack, GenerateKeys);
		if (KeyPropertyResult.SectionsKeyed.Num() > 0)
		{
			for (TWeakObjectPtr<UMovieSceneSection>& WeakSection : KeyPropertyResult.SectionsKeyed)
			{
				if (UMovieScene3DTransformSection* Section = Cast< UMovieScene3DTransformSection>(WeakSection.Get()))
				{
					FMovieSceneConstraintChannelHelper::CompensateIfNeeded(GetSequencer(), Section, Time);
				}
			}
		}
		return KeyPropertyResult;
	};

	AnimatablePropertyChanged( FOnKeyProperty::CreateLambda(OnKeyProperty) );
}

//todo move to external function so it can also be used by sdk/python
static void UpdateTransformBasedOnConstraint(FTransform& CurrentTransform, USceneComponent* SceneComponent)
{
	TArray< TObjectPtr<UTickableConstraint> > Constraints;
	AActor* ShapeActor = SceneComponent->GetTypedOuter<AActor>();
	
	if (ShapeActor)
	{
		FTransformConstraintUtils::GetParentConstraints(SceneComponent->GetWorld(), ShapeActor, Constraints);

		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(Constraints);
		if (Constraints.IsValidIndex(LastActiveIndex))
		{
			// switch to constraint space
			const FTransform WorldTransform = SceneComponent->GetSocketTransform(SceneComponent->GetAttachSocketName());
			const TOptional<FTransform> RelativeTransform =
				FTransformConstraintUtils::GetConstraintsRelativeTransform(Constraints, CurrentTransform, WorldTransform);
			if (RelativeTransform)
			{
				CurrentTransform = *RelativeTransform; 
			}
		}
	}
}

FTransformData F3DTransformTrackEditor::RecomposeTransform(const FTransformData& InTransformData, UObject* AnimatedObject, UMovieSceneSection* Section)
{
	using namespace UE::MovieScene;

	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = GetSequencer()->GetEvaluationTemplate();

	UMovieSceneEntitySystemLinker* EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
	if (!EntityLinker)
	{
		return InTransformData;
	}

	TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &EntityLinker->EntityManager);

	// We want the transform value contributed to by the given transform section.
	//
	// In most cases, the section only has one ECS entity for it specifiy all 9 channel curves and resulting values, but if
	// some of those channels have been overriden (e.g. with some noise channel), then there will be one extra ECS entity
	// for each overriden channel, and the "main" entity will be missing that channel. We therefore gather up all entities
	// related to the given section and recompose their contribution, and then join them all together by matching which one
	// handles which channel.
	//
	TArray<FMovieSceneEntityID> ImportedEntityIDs;
	EvaluationTemplate.FindEntitiesFromOwner(Section, GetSequencer()->GetFocusedTemplateID(), ImportedEntityIDs);

	if (ImportedEntityIDs.Num())
	{
		UMovieScenePropertyInstantiatorSystem* System = EntityLinker->FindSystem<UMovieScenePropertyInstantiatorSystem>();
		if (System)
		{
			FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
			FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
			USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(AnimatedObject);

			TArray<FMovieSceneEntityID> EntityIDs;
			{
				// In order to check for the result channels later, we need to look up the children entities that are
				// bound to the given animated object. Imported entities generally don't have the result channels.
				FEntityTaskBuilder()
				.ReadEntityIDs()
				.Read(BuiltInComponents->ParentEntity)
				.Read(BuiltInComponents->BoundObject)
				.FilterAll({ TrackComponents->ComponentTransform.PropertyTag })
				.Iterate_PerEntity(
					&EntityLinker->EntityManager, 
					[SceneComponent, ImportedEntityIDs, &EntityIDs](FMovieSceneEntityID EntityID, FMovieSceneEntityID ParentEntityID, UObject* BoundObject)
					{
						if (SceneComponent == BoundObject && ImportedEntityIDs.Contains(ParentEntityID))
						{
							EntityIDs.Add(EntityID);
						}
					});
			}

			FDecompositionQuery Query;
			Query.Entities = MakeArrayView(EntityIDs);
			Query.Object   = SceneComponent;
			Query.bConvertFromSourceEntityIDs = false;  // We already pass the children entity IDs

			FIntermediate3DTransform CurrentValue(InTransformData.Translation, InTransformData.Rotation, InTransformData.Scale);

			TRecompositionResult<FIntermediate3DTransform> TransformData = System->RecomposeBlendOperational(TrackComponents->ComponentTransform, Query, CurrentValue);

			double CurrentTransformChannels[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			EMovieSceneTransformChannel ChannelsObtained(EMovieSceneTransformChannel::None);
			check(EntityIDs.Num() == TransformData.Values.Num());
			for (int32 EntityIndex = 0; EntityIndex < TransformData.Values.Num(); ++EntityIndex)
			{
				// For each entity, find which channel they contribute to by checking the result channel.
				// We don't (yet) handle the case where two entities contribute to the same channel -- in theory the entities
				// should be mutually exclusive in that regard, at least as far as overriden channels are concerned.
				FMovieSceneEntityID EntityID = EntityIDs[EntityIndex];
				FIntermediate3DTransform EntityTransformData = TransformData.Values[EntityIndex];
				FComponentMask EntityType = EntityLinker->EntityManager.GetEntityType(EntityID);
				for (int32 CompositeIndex = 0; CompositeIndex < 9; ++CompositeIndex)
				{
					EMovieSceneTransformChannel ChannelMask = (EMovieSceneTransformChannel)(1 << CompositeIndex);
					if (!EnumHasAnyFlags(ChannelsObtained, ChannelMask) && EntityType.Contains(BuiltInComponents->DoubleResult[CompositeIndex]))
					{
						EnumAddFlags(ChannelsObtained, (EMovieSceneTransformChannel)(1 << CompositeIndex));
						CurrentTransformChannels[CompositeIndex] = EntityTransformData[CompositeIndex];
					}
				}
			}

			FTransform CurrentTransform(
				FRotator(CurrentTransformChannels[4], CurrentTransformChannels[5], CurrentTransformChannels[3]), // pitch yaw roll
				FVector(CurrentTransformChannels[0], CurrentTransformChannels[1], CurrentTransformChannels[2]),
				FVector(CurrentTransformChannels[6], CurrentTransformChannels[7], CurrentTransformChannels[8]));

			// Account for the transform origin only if this is not parented because the transform origin is already being applied to the parent.
			if (!SceneComponent->GetAttachParent())
			{
				CurrentTransform *= GetTransformOrigin().Inverse();
			}

			UpdateTransformBasedOnConstraint(CurrentTransform,SceneComponent);
			return FTransformData(CurrentTransform.GetLocation(), CurrentTransform.GetRotation().Rotator(), CurrentTransform.GetScale3D());
		}
	}

	return InTransformData;
}

void F3DTransformTrackEditor::ProcessKeyOperation(FFrameNumber InKeyTime, const UE::Sequencer::FKeyOperation& Operation, ISequencer& InSequencer)
{
	using namespace UE::Sequencer;

	auto Iterator = [this, InKeyTime, &Operation, &InSequencer](UMovieSceneTrack* Track, TArrayView<const UE::Sequencer::FKeySectionOperation> Operations)
	{
		FGuid ObjectBinding = Track->FindObjectBindingGuid();
		if (ObjectBinding.IsValid())
		{
			for (TWeakObjectPtr<> WeakObject : InSequencer.FindBoundObjects(ObjectBinding, InSequencer.GetFocusedTemplateID()))
			{
				if (UObject* Object = WeakObject.Get())
				{
					this->ProcessKeyOperation(Object, Operations, InSequencer, InKeyTime);
					return;
				}
			}
		}

		// Default behavior
		FKeyOperation::ApplyOperations(InKeyTime, Operations, ObjectBinding, InSequencer);
	};

	Operation.IterateOperations(Iterator);
}

void F3DTransformTrackEditor::ProcessKeyOperation(UObject* ObjectToKey, TArrayView<const UE::Sequencer::FKeySectionOperation> SectionsToKey, ISequencer& InSequencer, FFrameNumber KeyTime)
{
	USceneComponent* Component = MovieSceneHelpers::SceneComponentFromRuntimeObject(ObjectToKey);
	if (!Component)
	{
		return;
	}

	using namespace UE::MovieScene;
	using namespace UE::Sequencer;

	FSystemInterrogator Interrogator;
	Interrogator.TrackImportedEntities(true);

	TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &Interrogator.GetLinker()->EntityManager);

	TArray<FInterrogationChannel> InterrogationChannelsPerOperations;
	for (const FKeySectionOperation& Operation : SectionsToKey)
	{
		if (UMovieScenePropertyTrack* Track = Operation.Section->GetSectionObject()->GetTypedOuter<UMovieScenePropertyTrack>())
		{
			const FMovieScenePropertyBinding PropertyBinding = Track->GetPropertyBinding();
			const FInterrogationChannel InterrogationChannel = Interrogator.AllocateChannel(Component, PropertyBinding);
			InterrogationChannelsPerOperations.Add(InterrogationChannel);
			Interrogator.ImportTrack(Track, InterrogationChannel);
		}
		else
		{
			InterrogationChannelsPerOperations.Add(FInterrogationChannel::Invalid());
		}
	}

	Interrogator.AddInterrogation(KeyTime);

	Interrogator.Update();

	TArray<FMovieSceneEntityID> EntitiesPerSection, ValidEntities;
	for (int32 Index = 0; Index < SectionsToKey.Num(); ++Index)
	{
		const FKeySectionOperation& Operation = SectionsToKey[Index];
		const FInterrogationChannel InterrogationChannel = InterrogationChannelsPerOperations[Index];
		const FInterrogationKey InterrogationKey(InterrogationChannel, 0);
		FMovieSceneEntityID EntityID = Interrogator.FindEntityFromOwner(InterrogationKey, Operation.Section->GetSectionObject(), 0);

		EntitiesPerSection.Add(EntityID);
		if (EntityID)
		{
			ValidEntities.Add(EntityID);
		}
	}

	UMovieSceneInterrogatedPropertyInstantiatorSystem* System = Interrogator.GetLinker()->FindSystem<UMovieSceneInterrogatedPropertyInstantiatorSystem>();

	if (ensure(System && ValidEntities.Num() != 0))
	{
		FDecompositionQuery Query;
		Query.Entities = ValidEntities;
		Query.bConvertFromSourceEntityIDs = false;
		Query.Object   = Component;

		FTransform CurrentTransform(Component->GetRelativeRotation(), Component->GetRelativeLocation(), Component->GetRelativeScale3D());

		// Account for the transform origin only if this is not parented because the transform origin is already being applied to the parent.
		if (!Component->GetAttachParent())
		{
			CurrentTransform *= GetTransformOrigin().Inverse();
		}

		UpdateTransformBasedOnConstraint(CurrentTransform, Component);

		FIntermediate3DTransform CurrentValue(CurrentTransform.GetTranslation(), CurrentTransform.GetRotation().Rotator(), CurrentTransform.GetScale3D());
		TRecompositionResult<FIntermediate3DTransform> TransformData = System->RecomposeBlendOperational(FMovieSceneTracksComponentTypes::Get()->ComponentTransform, Query, CurrentValue);

		for (int32 Index = 0; Index < SectionsToKey.Num(); ++Index)
		{
			FMovieSceneEntityID EntityID = EntitiesPerSection[Index];
			if (!EntityID)
			{
				continue;
			}

			const FIntermediate3DTransform& RecomposedTransform = TransformData.Values[Index];

			for (TSharedPtr<IKeyArea> KeyArea : SectionsToKey[Index].KeyAreas)
			{
				FMovieSceneChannelHandle Handle  = KeyArea->GetChannel();
				if (Handle.GetChannelTypeName() == FMovieSceneDoubleChannel::StaticStruct()->GetFName())
				{
					FMovieSceneDoubleChannel* Channel = static_cast<FMovieSceneDoubleChannel*>(Handle.Get());

					double Value = RecomposedTransform[Handle.GetChannelIndex()];
					AddKeyToChannel(Channel, KeyTime, Value, InSequencer.GetKeyInterpolation());
				}
				else
				{
					KeyArea->AddOrUpdateKey(KeyTime, FGuid(), InSequencer);
				}
			}
		}
	}
}

void AddUnwoundKey(FMovieSceneDoubleChannel& Channel, FFrameNumber Time, double Value)
{
	int32 Index = Channel.AddLinearKey(Time, Value);

	TArrayView<FMovieSceneDoubleValue> Values = Channel.GetData().GetValues();
	if (Index >= 1)
	{
		const double PreviousValue = Values[Index - 1].Value;
		double NewValue = Value;

		while (NewValue - PreviousValue > 180.0f)
		{
			NewValue -= 360.f;
		}
		while (NewValue - PreviousValue < -180.0f)
		{
			NewValue += 360.f;
		}

		Values[Index].Value = NewValue;
	}
}


void F3DTransformTrackEditor::ImportAnimSequenceTransforms(const FAssetData& Asset, TSharedRef<ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());

	// find object binding to recover any component transforms we need to incorporate (for characters)
	FTransform InvComponentTransform;
	UMovieSceneSequence* MovieSceneSequence = Sequencer->GetFocusedMovieSceneSequence();
	if(MovieSceneSequence)
	{
		UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene();
		if(MovieScene)
		{
			FGuid ObjectBinding;
			if(MovieScene->FindTrackBinding(*TransformTrack, ObjectBinding))
			{
				const UClass* ObjectClass = nullptr;
				if(FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding))
				{
					ObjectClass = Spawnable->GetObjectTemplate()->GetClass();
				}
				else if(FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding))
				{
					ObjectClass = Possessable->GetPossessedObjectClass();
				}

				if(ObjectClass)
				{
					const ACharacter* Character = Cast<const ACharacter>(ObjectClass->ClassDefaultObject);
					if(Character)
					{
						const USkeletalMeshComponent* SkeletalMeshComponent = Character->GetMesh();
						FTransform MeshRelativeTransform = SkeletalMeshComponent->GetRelativeTransform();
						InvComponentTransform = MeshRelativeTransform.GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform()).Inverse();
					}
				}
			}
		}
	}

	if (AnimSequence && AnimSequence->GetDataModel()->GetNumBoneTracks() > 0)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "ImportAnimSequenceTransforms", "Import Anim Sequence Transforms" ) );

		TransformTrack->Modify();

		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());

		Section->SetBlendType(EMovieSceneBlendType::Additive);
		Section->SetMask(EMovieSceneTransformChannel::Translation | EMovieSceneTransformChannel::Rotation);
		
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		// We know that there aren't any channel overrides (all 9 channels are double channels) because we're working
		// on a brand new transform section.
		TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();

		// Set default translation and rotation
		for (int32 Index = 0; Index < 6; ++Index)
		{
			DoubleChannels[Index]->SetDefault(0.f);
		}
		// Set default scale
		for (int32 Index = 6; Index < 9; ++Index)
		{
			DoubleChannels[Index]->SetDefault(1.f);
		}

		TransformTrack->AddSection(*Section);

		if (Section->TryModify())
		{
			struct FTempTransformKey
			{
				FTransform Transform;
				FRotator WoundRotation;
				float Time;
			};

			TArray<FTempTransformKey> TempKeys;

			const FBoneAnimationTrack& AnimationTrack = AnimSequence->GetDataModel()->GetBoneTrackByIndex(0);
			const FRawAnimSequenceTrack& RawTrack = AnimationTrack.InternalTrackData;

			const int32 KeyCount = FMath::Max(FMath::Max(RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num()), RawTrack.ScaleKeys.Num());
			for(int32 KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
			{
				FTempTransformKey TempKey;
				TempKey.Time = AnimSequence->GetTimeAtFrame(KeyIndex);

				if(RawTrack.PosKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetTranslation(FVector(RawTrack.PosKeys[KeyIndex]));
				}
				else if(RawTrack.PosKeys.Num() > 0)
				{
					TempKey.Transform.SetTranslation(FVector(RawTrack.PosKeys[0]));
				}
				
				if(RawTrack.RotKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetRotation(FQuat(RawTrack.RotKeys[KeyIndex]));
				}
				else if(RawTrack.RotKeys.Num() > 0)
				{
					TempKey.Transform.SetRotation(FQuat(RawTrack.RotKeys[0]));
				}

				if(RawTrack.ScaleKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetScale3D(FVector(RawTrack.ScaleKeys[KeyIndex]));
				}
				else if(RawTrack.ScaleKeys.Num() > 0)
				{
					TempKey.Transform.SetScale3D(FVector(RawTrack.ScaleKeys[0]));
				}

				// apply component transform if any
				TempKey.Transform = InvComponentTransform * TempKey.Transform;

				TempKey.WoundRotation = TempKey.Transform.GetRotation().Rotator();

				TempKeys.Add(TempKey);
			}

			int32 TransformCount = TempKeys.Num();
			for(int32 TransformIndex = 0; TransformIndex < TransformCount - 1; TransformIndex++)
			{
				FRotator& Rotator = TempKeys[TransformIndex].WoundRotation;
				FRotator& NextRotator = TempKeys[TransformIndex + 1].WoundRotation;

				FMath::WindRelativeAnglesDegrees(Rotator.Pitch, NextRotator.Pitch);
				FMath::WindRelativeAnglesDegrees(Rotator.Yaw, NextRotator.Yaw);
				FMath::WindRelativeAnglesDegrees(Rotator.Roll, NextRotator.Roll);
			}

			TRange<FFrameNumber> Range = Section->GetRange();
			for(const FTempTransformKey& TempKey : TempKeys)
			{
				FFrameNumber KeyTime = (TempKey.Time * TickResolution).RoundToFrame();

				Range = TRange<FFrameNumber>::Hull(Range, TRange<FFrameNumber>(KeyTime));

				const FVector3f Translation = (FVector3f)TempKey.Transform.GetTranslation();
				const FVector3f Rotation = (FVector3f)TempKey.WoundRotation.Euler();
				const FVector3f Scale = (FVector3f)TempKey.Transform.GetScale3D();

				TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();

				Channels[0]->AddLinearKey(KeyTime, Translation.X);
				Channels[1]->AddLinearKey(KeyTime, Translation.Y);
				Channels[2]->AddLinearKey(KeyTime, Translation.Z);

				AddUnwoundKey(*Channels[3], KeyTime, Rotation.X);
				AddUnwoundKey(*Channels[4], KeyTime, Rotation.Y);
				AddUnwoundKey(*Channels[5], KeyTime, Rotation.Z);

				Channels[6]->AddLinearKey(KeyTime, Scale.X);
				Channels[7]->AddLinearKey(KeyTime, Scale.Y);
				Channels[8]->AddLinearKey(KeyTime, Scale.Z);
			}

			Section->SetRange(Range);
			Section->SetRowIndex(MovieSceneToolHelpers::FindAvailableRowIndex(TransformTrack, Section));

			Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
		}
	}
}

void F3DTransformTrackEditor::ImportAnimSequenceTransformsEnterPressed(const TArray<FAssetData>& Asset, TSharedRef<ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack)
{
	if (Asset.Num() > 0)
	{
		ImportAnimSequenceTransforms(Asset[0].GetAsset(), Sequencer, TransformTrack);
	}
}

bool F3DTransformTrackEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjects) const
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
			if (ObjectClass && ObjectClass->IsChildOf(UMovieScene3DTransformSection::StaticClass()))
			{
				UMovieScene3DTransformSection* Section = Cast< UMovieScene3DTransformSection>(Object);
				if (Section)
				{
					SectionsGettingUndone.Add(Section);
				}
				bGettingUndone = true;
				break;
			}
			Object = Object->GetOuter();
		}
	}

	return bGettingUndone;
}

void F3DTransformTrackEditor::PostUndo(bool bSuccess)
{
	for (TWeakObjectPtr<UMovieScene3DTransformSection> &Section : SectionsGettingUndone)
	{
		if (Section.IsValid())
		{
			TArray<FConstraintAndActiveChannel>& ConstraintChannels = Section->GetConstraintsChannels();
			for (FConstraintAndActiveChannel& Channel : ConstraintChannels)
			{
				HandleOnConstraintAdded(Section.Get(), &(Channel.ActiveChannel));
			}
		}
	}
}

void F3DTransformTrackEditor::HandleOnConstraintAdded(IMovieSceneConstrainedSection* InSection, FMovieSceneConstraintChannel* InConstraintChannel)
{
	if (!InConstraintChannel)
	{
		return;
	}
	// handle scene component changes so we can update the gizmo location when it changes
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	if (!Controller.OnSceneComponentConstrained().IsBound())
	{
		OnSceneComponentConstrainedHandle =
			Controller.OnSceneComponentConstrained().AddLambda([](USceneComponent* InSceneComponent)
				{
					//only update gizmo if it or it's actor is seleced, otherwise can be wasteful
					GUnrealEd->UpdatePivotLocationForSelection();
					AActor* Actor = InSceneComponent->GetTypedOuter<AActor>();
					if (USelection* SelectedActors = GEditor->GetSelectedActors())
					{
						if (SelectedActors->IsSelected(Actor))
						{
							GUnrealEd->UpdatePivotLocationForSelection();
							return;
						}
					}
					else if (USelection* SelectedComponents = GEditor->GetSelectedComponents())
					{
						if (SelectedComponents->IsSelected(InSceneComponent))
						{
							GUnrealEd->UpdatePivotLocationForSelection();
						}
					}
				});
	}
	// Store Section so we can remove these delegates
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(InSection);
	SectionsToClear.Add(Section);

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

static UTickableTransformConstraint* GetTickableTransformConstraint(IMovieSceneConstrainedSection* InSection, const FMovieSceneConstraintChannel* InConstraintChannel)
{
	UTickableTransformConstraint* Constraint = nullptr;
	// get constraint channel
	TArray<FConstraintAndActiveChannel>& ConstraintChannels = InSection->GetConstraintsChannels();
	const int32 Index = ConstraintChannels.IndexOfByPredicate([InConstraintChannel](const FConstraintAndActiveChannel& InChannel)
		{
			return &(InChannel.ActiveChannel) == InConstraintChannel;
		});

	if (Index != INDEX_NONE)
	{
		Constraint = Cast<UTickableTransformConstraint>(ConstraintChannels[Index].Constraint.Get());
	}
	return Constraint;
}

void F3DTransformTrackEditor::HandleConstraintKeyDeleted(IMovieSceneConstrainedSection* InSection, const FMovieSceneConstraintChannel* InConstraintChannel,
	const TArray<FKeyAddOrDeleteEventItem>& InDeletedItems) const
{
	if (!InConstraintChannel)
	{
		return;
	}
	UTickableTransformConstraint* Constraint = GetTickableTransformConstraint(InSection, InConstraintChannel);
	if (!Constraint)
	{
		return;
	}

    if(UTickableTransformConstraint* Constrain = GetTickableTransformConstraint(InSection,InConstraintChannel))
	{
		for (const FKeyAddOrDeleteEventItem& EventItem : InDeletedItems)
		{
			UMovieSceneSection* Section = Cast<UMovieSceneSection>(InSection);
			FMovieSceneConstraintChannelHelper::HandleConstraintKeyDeleted(
				Constraint, InConstraintChannel,
				GetSequencer(), Section,
				EventItem.Frame);
			
		}
	}
}

void F3DTransformTrackEditor::HandleConstraintKeyMoved(IMovieSceneConstrainedSection* InSection, const FMovieSceneConstraintChannel* InConstraintChannel,
	const TArray<FKeyMoveEventItem>& InMovedItems)
{
	if (!InConstraintChannel)
	{
		return;
	}
	UTickableTransformConstraint* Constraint = GetTickableTransformConstraint(InSection, InConstraintChannel);
	if (!Constraint)
	{
		return;
	}
	UMovieSceneSection* Section = Cast<UMovieSceneSection>(InSection);

	for (const FKeyMoveEventItem& MoveEventItem : InMovedItems)
	{
		FMovieSceneConstraintChannelHelper::HandleConstraintKeyMoved(
			Constraint, InConstraintChannel, Section,
			MoveEventItem.Frame, MoveEventItem.NewFrame);
	}
}

void F3DTransformTrackEditor::HandleConstraintRemoved(IMovieSceneConstrainedSection* InSection) 
{
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(InSection);

	FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	if (!InSection->OnConstraintRemovedHandle.IsValid())
	{
		InSection->OnConstraintRemovedHandle =
			Controller.GetNotifyDelegate().AddLambda([InSection,Section, this](EConstraintsManagerNotifyType InNotifyType, UObject *InObject)
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

void F3DTransformTrackEditor::ClearOutConstraintDelegates() 
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

	for (TWeakObjectPtr<UMovieScene3DTransformSection>& Section : SectionsToClear)
	{
		if (IMovieSceneConstrainedSection* CRSection = Section.Get())
		{
			// clear constraint channels
			TArray<FConstraintAndActiveChannel>& ConstraintChannels = CRSection->GetConstraintsChannels();
			for (FConstraintAndActiveChannel& Channel : ConstraintChannels)
			{
				Channel.ActiveChannel.OnKeyMovedEvent().Clear();
				Channel.ActiveChannel.OnKeyDeletedEvent().Clear();
			}

			if (CRSection->OnConstraintRemovedHandle.IsValid())
			{
				CRSection->OnConstraintRemovedHandle.Reset();
			}
		}
	}
	SectionsToClear.Reset();
}
#undef LOCTEXT_NAMESPACE
