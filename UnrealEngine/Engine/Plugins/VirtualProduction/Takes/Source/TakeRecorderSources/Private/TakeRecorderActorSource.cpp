// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderActorSource.h"
#include "Styling/SlateIconFinder.h"
#include "ClassIconFinder.h"
#include "MovieScene.h"
#include "LevelSequence.h"
#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "ActorRecordingSettings.h"
#include "Misc/ScopedSlowTask.h"
#include "SequenceRecorderUtils.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderSourcesUtils.h"
#include "Recorder/TakeRecorderParameters.h"
#include "TakeRecorderSettings.h"
#include "TakesUtils.h"
#include "TakeMetaData.h"
#include "MovieSceneFolder.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "Compilation/MovieSceneCompiledDataManager.h"

#include "Animation/SkeletalMeshActor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "CameraRig_Crane.h"
#include "CameraRig_Rail.h"
#include "Components/SkeletalMeshComponent.h"

#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "TrackRecorders/MovieSceneTrackRecorder.h"
#include "TrackRecorders/MovieScenePropertyTrackRecorder.h"
#include "TrackRecorders/MovieSceneAnimationTrackRecorder.h"
#include "TrackRecorders/MovieScene3DTransformTrackRecorder.h"
#include "TrackRecorders/MovieScenePropertyTrackRecorder.h"
#include "TrackRecorders/MovieSceneTrackRecorderSettings.h"

#include "MovieSceneTakeTrack.h"
#include "MovieSceneTakeSection.h"
#include "MovieSceneTakeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderActorSource)

DEFINE_LOG_CATEGORY(ActorSerialization);

#define LOCTEXT_NAMESPACE "UTakeRecorderActorSource"
static const FName MovieSceneSectionRecorderFactoryName("MovieSceneTrackRecorderFactory");
static const FName SequencerTrackClassMetadataName("SequencerTrackClass");
static const FName DoNotRecordTag("DoNotRecord");

UTakeRecorderSource* UTakeRecorderActorSource::AddSourceForActor(AActor* InActor, UTakeRecorderSources* InSources)
{
	if (InSources == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The Source is invalid."), ELogVerbosity::Error);
		return nullptr;
	}
	
	if (InActor == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The Actor is invalid."), ELogVerbosity::Error);
		return nullptr;
	}

	if (!TakeRecorderSourcesUtils::IsActorRecordable(InActor))
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("The Actor: %s is not recordable."), *InActor->GetPathName()), ELogVerbosity::Error);
		return nullptr;
	}

	//Look through our sources and see if one actor matches the incoming one either from editor or PIE world.
	{
		//Cache  InputActor comparison data
		const bool bIsAlreadyPIEActor = InActor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		const AActor* InputActorEditor = EditorUtilities::GetEditorWorldCounterpartActor(InActor);
		const AActor* InputActorPIE = EditorUtilities::GetSimWorldCounterpartActor(InActor);

		TArray<UTakeRecorderSource*> SourceArray = InSources->GetSourcesCopy();
		for (UTakeRecorderSource* CurrentSource : SourceArray)
		{
			UTakeRecorderActorSource* CurrentActorSource = Cast<UTakeRecorderActorSource>(CurrentSource);
			if (CurrentActorSource != nullptr)
			{
				AActor* CurrentActor = CurrentActorSource->Target.Get();
				if (CurrentActor == nullptr)
				{
					continue;
				}

				if (InActor == CurrentActor)
				{
					return CurrentActorSource;
				}
				else 
				{
					if (bIsAlreadyPIEActor)
					{
						//The input actor is from PIE -> Bring it into Editor world and compare. 
						if (InputActorEditor == CurrentActor)
						{
							return CurrentActorSource;
						}
					}
					else
					{
						//The input actor is from Editor -> Bring it into PIE world and compare. 
						if (InputActorPIE == CurrentActor)
						{
							return CurrentActorSource;
						}
					}
				}
			}
		}
	}

	UTakeRecorderActorSource* NewSource = InSources->AddSource<UTakeRecorderActorSource>();
	NewSource->SetSourceActor(InActor);
	return NewSource;
}

void UTakeRecorderActorSource::RemoveActorFromSources(AActor* InActor, UTakeRecorderSources* InSources)
{
	if (InSources == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The Source to remove from is invalid."), ELogVerbosity::Error);
		return;
	}

	if (InActor == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("The Actor to remove is invalid."), ELogVerbosity::Error);
		return;
	}

	//Cache  InputActor comparison data
	const bool bIsAlreadyPIEActor = InActor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
	const AActor* InputActorEditor = EditorUtilities::GetEditorWorldCounterpartActor(InActor);
	const AActor* InputActorPIE = EditorUtilities::GetSimWorldCounterpartActor(InActor);

	TArray<UTakeRecorderSource*> SourceArray = InSources->GetSourcesCopy();
	for (UTakeRecorderSource* CurrentSource : SourceArray)
	{
		UTakeRecorderActorSource* CurrentActorSource = Cast<UTakeRecorderActorSource>(CurrentSource);
		if (CurrentActorSource != nullptr)
		{
			const AActor* CurrentActor = CurrentActorSource->Target.Get();
			if (CurrentActor == nullptr)
			{
				continue;
			}

			if (InActor == CurrentActor)
			{
				InSources->RemoveSource(CurrentSource);
			}
			else
			{
				if (bIsAlreadyPIEActor)
				{
					//The input actor is from PIE -> Bring it into Editor world and compare. 
					if (InputActorEditor == CurrentActor)
					{
						InSources->RemoveSource(CurrentSource);
					}
				}
				else
				{
					//The input actor is from Editor -> Bring it into PIE world and compare. 
					if (InputActorPIE == CurrentActor)
					{
						InSources->RemoveSource(CurrentSource);
					}
				}
			}
		}
	}
}

UTakeRecorderActorSource::UTakeRecorderActorSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	RecordType = ETakeRecorderActorRecordType::ProjectDefault;
	bReduceKeys = true;
	bRecordParentHierarchy = true;
	bShowProgressDialog = true;

	TargetSequenceID = MovieSceneSequenceID::Invalid;

	// Build the property map on initialization so that sources created at runtime have a default map
	RebuildRecordedPropertyMap();
}

bool UTakeRecorderActorSource::IsValid() const
{
	return (Target.IsValid());
}


TArray<UTakeRecorderSource*> UTakeRecorderActorSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	// Don't bother doing anything if we don't have a valid actor to record.
	if (!Target.IsValid())
	{
		return TArray<UTakeRecorderSource*>();
	}
	// We used to do this at the PostRecording but other Actor sources may need to check you recorded an animation so we keep it around
	TrackRecorders.Empty();

	// Resolve which actor we wish to record 
	AActor* ActorToRecord = Target.Get();
	TargetLevelSequence = InSequence;
	MasterLevelSequence = InMasterSequence;
	TargetSequenceID = InSequenceID;

	FString ObjectBindingName = ActorToRecord->GetName();

	// Resolve which movie scene our data should go into
	UMovieScene* MovieScene = TargetLevelSequence->GetMovieScene();

	// Look to see if the movie scene already has our object binding in it (which is common if we're recording a new take) so we can
	// replace the data that was already there.
	CachedObjectBindingGuid = ResolveActorFromSequence(ActorToRecord, TargetLevelSequence);
	CleanExistingDataFromSequence(CachedObjectBindingGuid, *TargetLevelSequence);
	
	// Initialize the header for this actor in the Manifest Serializer for streaming data capture.
	FName SerializedType("Actor");
	FActorFileHeader Header(ObjectBindingName, ActorToRecord->GetActorLabel(), SerializedType, ActorToRecord->GetClass()->GetPathName(), false);

	if (GetRecordToPossessable())
	{
		// If a user adds a PIE-only instance as a recordable object, they can't record this to a possessable (because the binding will be broken when they exit PIE).
		if (Target->GetWorld()->WorldType != EWorldType::Editor && GEditor && !GEditor->ObjectsThatExistInEditorWorld.Get(Target.Get()))
		{
			UE_LOG(LogTakesCore, Warning, TEXT("Attempted to record an actor that does not exist in the editor world to a possessable. Forcing recording of %s as a Spawnable so that the resulting binding is not broken!"), *Target->GetName());
			RecordType = ETakeRecorderActorRecordType::Spawnable;
		}
		else
		{
			// Create a Possessable object binding in the Sequence and then bind it to our actor
			CachedObjectBindingGuid = MovieScene->AddPossessable(ActorToRecord->GetActorLabel(), ActorToRecord->GetClass());
			TargetLevelSequence->BindPossessableObject(CachedObjectBindingGuid, *ActorToRecord, ActorToRecord->GetWorld());
			Header.bRecordToPossessable = true;
		}
	}
	
	if (!GetRecordToPossessable())
	{
		// We need to store the object template in the Movie Scene (because it's a complex UObject)
		// instead of trying to place this data into the non-UObject safe data stream.
		FName UniqueTemplateName = MakeUniqueObjectName(TargetLevelSequence, ActorToRecord->GetClass(), NAME_None);
		Header.TemplateName = UniqueTemplateName.ToString();
		CachedObjectTemplate = CastChecked<AActor>(TargetLevelSequence->MakeSpawnableTemplateFromInstance(*ActorToRecord, UniqueTemplateName));
		CachedObjectBindingGuid = MovieScene->AddSpawnable(ActorToRecord->GetActorLabel(), *CachedObjectTemplate);
		
		if (CachedObjectTemplate.IsValid())
		{
			PostProcessCreatedObjectTemplateImpl(CachedObjectTemplate.Get());
		}
	}

	Header.Guid = CachedObjectBindingGuid;
	if (InManifestSerializer)
	{
		FManifestProperty  ManifestProperty(ObjectBindingName, FName("Actor"), CachedObjectBindingGuid);
		InManifestSerializer->WriteFrameData(InManifestSerializer->FramesWritten, ManifestProperty);
	
		FString AssetPath = InManifestSerializer->GetLocalCaptureDir();  

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*AssetPath))
		{
			PlatformFile.CreateDirectory(*AssetPath);
		}

		AssetPath = AssetPath / ObjectBindingName;
		if (!PlatformFile.DirectoryExists(*AssetPath))
		{
			PlatformFile.CreateDirectory(*AssetPath);
		}

		ActorSerializer.SetLocalCaptureDir(AssetPath);

		FText Error;

		FString FileName = FString::Printf(TEXT("%s_%s"), *SerializedType.ToString(), *ObjectBindingName);

		if (!ActorSerializer.OpenForWrite(FileName, Header, Error))
		{
			//UE_LOG(LogFrameTransport, Error, TEXT("Cannot open frame debugger cache %s. Failed to create archive."), *InFilename);
			UE_LOG(ActorSerialization, Warning, TEXT("Error Opening Actor Sequencer File: Subject '%s' Error '%s'"), *ObjectBindingName, *Error.ToString());
		}

	}
	// Now we need to create the section recorders for each of the enabled properties based on the property map.
	// Any components spawned at runtime will get picked up on Tick and have section recorders created for them mid-recording.
	TArray<UObject*> TraversedObjects;
	CreateSectionRecordersRecursive(ActorToRecord, RecordedProperties, TraversedObjects);

	// Update our cached list of components so that we don't detect them all as new components on the first Tick
	GetAllComponents(CachedComponentList, false);

	// Walk our parent chain until we get to the root and make sure all of our parent actors are recorded. This allows our transforms
	// to always be in local space (conversion to world space can be done in Sequencer via baking transforms) and attach tracks
	EnsureParentHierarchyIsReferenced();

	// Create any new Actor Sources for Actors that we reference (either parents or attached components that belong to other actors)
	CreateNewActorSourceForReferencedActors();

	// We might have generated new Actors to be recorded so that the current actor can be recorded.
	// We may have added our parents (so that transforms work) or we might have added an actor who
	// has a component that is currently attached to us.
	return AddedActorSources;
}

void UTakeRecorderActorSource::EnsureParentHierarchyIsReferenced()
{
	if (!bRecordParentHierarchy)
	{
		return;
	}

	if (!Target.IsValid())
	{
		return;
	}

	if (!Target->GetRootComponent())
	{
		return;
	}

	// We need to start with our parent so that we don't try to add another recording for ourself
	// as we're already in the process of creating a recording for ourself!
	USceneComponent* CurrentComponent = Target->GetRootComponent()->GetAttachParent();
	while (CurrentComponent)
	{
		AActor* Owner = CurrentComponent->GetOwner();
		NewReferencedActors.Add(Owner);

		// We can skip forward to the root component for that Actor as it'll end up adding all of its children
		// which will include the component we may be attached to.
		CurrentComponent = Owner->GetRootComponent()->GetAttachParent();
	}
}

void UTakeRecorderActorSource::StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence)
{
	// Now that every source has had PreRecording called on it and we're about to start recording, iterate through each one and set
	// their starts to the most up to date time sample.
	for (UMovieSceneTrackRecorder* Recorder : TrackRecorders)
	{
		// The Frame Number is the sequence time we should record into, but we want to get
		// a Timecode stamp that reflects the real time it was recorded... This kind of conflicts
		// with 
		Recorder->SetSectionStartTimecode(InSectionStartTimecode, InSectionFirstFrame);
	}
}

void UTakeRecorderActorSource::CreateSectionRecordersRecursive(UObject* ObjectToRecord, UActorRecorderPropertyMap* PropertyMap, TArray<UObject*>& TraversedObjects)
{
	if (TraversedObjects.Contains(ObjectToRecord))
	{
		return;
	}

	TraversedObjects.Add(ObjectToRecord);

	FGuid Guid = CachedObjectBindingGuid;
	if (ObjectToRecord->IsA<UActorComponent>())
	{
		UActorComponent* Component = Cast<UActorComponent>(ObjectToRecord);
		// Components are duplicated into the Object Template that belongs inside of the owning Movie Scene.
		// A Spawnable Object Binding is created tied to it to re-create the actor itself, but to record
		// properties about components on that object we create Possessable Object Bindings instead and bind the 
		// Possessable to the object inside the template that gets spawned.

		
		// This can be called even on Possessables (and is encouraged to do so as it does a sanity check and a warning if a dynamically added component is put on a Possessable)
		// This will update the Object Template with the given component if it does not already have a component with the same relative path.
		UActorComponent* NewlyDuplicatedComponent = nullptr;
		bool bNewComponentAdded = EnsureObjectTemplateHasComponent(Component, NewlyDuplicatedComponent);

		
		UMovieScene* MovieScene = TargetLevelSequence->GetMovieScene();
		Guid = FGuid();
		for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
		{
			const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
			if (Possessable.GetParent() == CachedObjectBindingGuid && Possessable.GetName() == Component->GetName() && Possessable.GetPossessedObjectClass() == Component->GetClass())
			{
				Guid = Possessable.GetGuid();
				break;
			}
		}

		UActorComponent* ComponentToRecord = bNewComponentAdded ? NewlyDuplicatedComponent : Component;
		// cbb: Not sure what this is accomplishing
		if (!Guid.IsValid())
		{
			Guid = MovieScene->AddPossessable(ComponentToRecord->GetName(), ComponentToRecord->GetClass());
		}

		// Set up parent/child guids for possessables within spawnables
		FMovieScenePossessable* ChildPossessable = MovieScene->FindPossessable(Guid);
		if (ensure(ChildPossessable))
		{
			ChildPossessable->SetParent(CachedObjectBindingGuid, MovieScene);
		}

		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable(CachedObjectBindingGuid);
		if (ParentSpawnable)
		{
			ParentSpawnable->AddChildPossessable(Guid);
		}

		// Bindings are stored relative to their context outer. Newly duplicated components have a different outer
		TargetLevelSequence->BindPossessableObject(Guid, *ComponentToRecord, bNewComponentAdded ? CachedObjectTemplate.Get() : Target.Get());

		FActorProperty  ActorCompFrame(ComponentToRecord->GetName(), FName("Component"), Guid);
		ActorCompFrame.Type = EActoryPropertyType::ComponentType;
		ActorCompFrame.BindingName = Target.Get()->GetName();
		ActorCompFrame.ClassName = ComponentToRecord->GetClass()->GetName();
		ActorSerializer.WriteFrameData(ActorSerializer.FramesWritten, ActorCompFrame);
	}

	// We need to iterate through the Property Map to see if the user wants to record this property or not
	// We store the name of the Factory that can do the recording in the Property Map so for now we shortcut
	// and just look up Factories by name instead of replicating all of the logic that goes into initializing
	// the property map.
	for (FActorRecordedProperty& Property : PropertyMap->Properties)
	{
		// Skip any properties that the user doesn't want to record
		if (!Property.bEnabled)
		{
			continue;
		}

		TArray<IMovieSceneTrackRecorderFactory*> ModularFactories = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneTrackRecorderFactory>(MovieSceneSectionRecorderFactoryName);
		bool bFoundRecorder = false;
		for (IMovieSceneTrackRecorderFactory* Factory : ModularFactories)
		{
			// cbb: Can't FText == FText?
			if (Factory->GetDisplayName().ToString() == Property.RecorderName.ToString())
			{
				UMovieSceneTrackRecorder* SectionRecorder = Factory->CreateTrackRecorderForObject();
				if (SectionRecorder)
				{
					TrackRecorders.Add(SectionRecorder);
					if (Factory->IsSerializable())
					{
						AActor* ActorToRecord = Target.Get();
						FString Name =  ObjectToRecord->GetName();
						FActorProperty  ActorFrame(Name, Factory->GetSerializedType(), Guid);
						ActorSerializer.WriteFrameData(ActorSerializer.FramesWritten, ActorFrame);
						SectionRecorder->SetSavedRecordingDirectory(ActorSerializer.GetLocalCaptureDir());
					}
					SectionRecorder->CreateTrack(this, ObjectToRecord, TargetLevelSequence->GetMovieScene(), GetSettingsObjectForFactory(Factory->GetSettingsClass()), Guid);

					bFoundRecorder = true;
					break;
				}
			}
		}

		if (!bFoundRecorder)
		{
			// Our current fallback property recorder isn't registered in the modular factories list so that it always goes last.
			TArray<FString> PropertyNames;
			Property.PropertyName.ToString().ParseIntoArray(PropertyNames, TEXT("."));

			FProperty* PropertyInstance = nullptr;
			UStruct* SearchStruct = ObjectToRecord->GetClass();
			for (auto PropertyStringName : PropertyNames)
			{
				PropertyInstance = SearchStruct ? SearchStruct->FindPropertyByName(FName(*PropertyStringName)) : nullptr;
				SearchStruct = nullptr;
				if (PropertyInstance)
				{
					if (FStructProperty* AsStructProperty = CastField<FStructProperty>(PropertyInstance))
					{
						SearchStruct = AsStructProperty->Struct;
					}
				}
				if (!PropertyInstance) break;
			}

			if (PropertyInstance)
			{
				FMovieScenePropertyTrackRecorderFactory TrackRecorderFactory;
				if (TrackRecorderFactory.CanRecordProperty(ObjectToRecord, PropertyInstance))
				{
					UMovieSceneTrackRecorder* SectionRecorder = TrackRecorderFactory.CreateTrackRecorderForProperty(ObjectToRecord, Property.PropertyName);
					if (SectionRecorder)
					{
						TrackRecorders.Add(SectionRecorder);
						if (TrackRecorderFactory.IsSerializable())
						{
							AActor* ActorToRecord = Target.Get();
							FString Name =  ObjectToRecord->GetName();
							FActorProperty  ActorFrame(Name, TrackRecorderFactory.GetSerializedType(), Guid);
							ActorFrame.Type = EActoryPropertyType::PropertyType;
							ActorFrame.PropertyName = Property.PropertyName.ToString();
							ActorSerializer.WriteFrameData(ActorSerializer.FramesWritten, ActorFrame);
							SectionRecorder->SetSavedRecordingDirectory(ActorSerializer.GetLocalCaptureDir());
						}
						SectionRecorder->CreateTrack(this, ObjectToRecord, TargetLevelSequence->GetMovieScene(), GetSettingsObjectForFactory(TrackRecorderFactory.GetSettingsClass()), Guid);
					}
				}
			}
			else 
			{
				UE_LOG(LogTakesCore, Warning, TEXT("Unable to find property %s. Cannot record."), *ObjectToRecord->GetName()); 
			}
		}
	}

	// Now iterate through any children and repeat.
	for (UActorRecorderPropertyMap* Child : PropertyMap->Children)
	{
		UObject* ChildObject = Child->RecordedObject.Get();
		if (ChildObject)
		{
			CreateSectionRecordersRecursive(ChildObject, Child, TraversedObjects);
		}
		else
		{
			UE_LOG(LogTakesCore, Warning, TEXT("Attempted to resolve soft object path %s but failed, skipping entire child hierarchy for recording. This is likely because the object only exists at edit time. Ideally we would filter out these and not create entries in the Property Map, but they may want to record editor-only objects at edit time."), 
				*Child->RecordedObject.ToString());
		}
	}
}

void UTakeRecorderActorSource::TickRecording(const FQualifiedFrameTime& CurrentSequenceTime)
{
	// Each frame we want to compare against the list of components we were recording last frame. 
	// This will allow us to detect newly added components and components that were removed at runtime,
	// which allows us to properly update their resulting spawn track.
	TSet<UActorComponent*> CurrentComponentList;
	GetAllComponents(CurrentComponentList, false);

	TArray<UActorComponent*> NewComponentsAdded;
	TArray<UActorComponent*> NewComponentsRemoved;

	for (UActorComponent* CurrentComponent : CurrentComponentList)
	{
		// Track any components added to our list this frame
		if (!CachedComponentList.Contains(CurrentComponent))
		{
			NewComponentsAdded.Add(CurrentComponent);
		}
	}

	for (UActorComponent* OldComponent : CachedComponentList)
	{
		// Now do the reverse and mark any components that have been removed.
		if (!CurrentComponentList.Contains(OldComponent))
		{
			NewComponentsRemoved.Add(OldComponent);
		}
	}

	TArray<UObject*> TraversedObjects;
	for (UActorComponent* AddedComponent : NewComponentsAdded)
	{
		if (Target.IsValid() && ::IsValid(AddedComponent))
		{
			UE_LOG(LogTakesCore, Log, TEXT("Detected newly added component %s on Actor %s, beginning to record component's properties now."), *AddedComponent->GetReadableName(), *Target->GetName());
			TSet<UMovieSceneTrackRecorder*> PreviousTrackRecorders = TSet<UMovieSceneTrackRecorder*>(TrackRecorders);

			// We should create a new property map attached to the right parent, and then initialize it using existing flow. This works for Possessables too as it will throw a warning that
			// the binding will be broken.
			UActorRecorderPropertyMap* ComponentPropertyMap = NewObject<UActorRecorderPropertyMap>(this, MakeUniqueObjectName(this, UActorRecorderPropertyMap::StaticClass(), AddedComponent->GetFName()), RF_Transactional);
			ComponentPropertyMap->RecordedObject = AddedComponent;

			// Add the new property map as a child of the correct parent, otherwise recursion doesn't work when we try to update the cached number of recorded properties.
			UActorRecorderPropertyMap* ParentPropertyMap = GetParentPropertyMapForComponent(AddedComponent);
			if(ensureMsgf(ParentPropertyMap, TEXT("A component %s was added to actor %s at runtime but we couldn't find the property map for the parent. Is the parent no longer valid?"), *AddedComponent->GetName(), *Target->GetName()))
			{
				ParentPropertyMap->Children.Add(ComponentPropertyMap);
			}

			// Create the Property Map
			RebuildRecordedPropertyMapRecursive(AddedComponent, ComponentPropertyMap);

			// Create the Section Recorders required
			CreateSectionRecordersRecursive(AddedComponent, ComponentPropertyMap, TraversedObjects);

			// Update our numbers on the display
			UpdateCachedNumberOfRecordedProperties();

			// We need to call StartRecording on only the Track Recorders created in this situation.
			for (UMovieSceneTrackRecorder* TrackRecorder : TrackRecorders)
			{
				// If the track recorder existed before we added this component then it has already had StartRecording called on it.
				if (PreviousTrackRecorders.Contains(TrackRecorder))
				{
					continue;
				}
				
				// cbb: This should match the logic in TakeRecorderSources if changed.
				FFrameNumber FirstFrameOfSequence = CurrentSequenceTime.ConvertTo(TargetLevelSequence->GetMovieScene()->GetTickResolution()).FloorToFrame();
				TrackRecorder->SetSectionStartTimecode(FApp::GetTimecode(), FirstFrameOfSequence);
			}
		}
	}

	for (UActorComponent* RemovedComponent : NewComponentsRemoved)
	{
		if (Target.IsValid() && RemovedComponent)
		{
			UE_LOG(LogTakesCore, Log, TEXT("Detected removed component %s on Actor %s, stopping recording of component's properties now."), *RemovedComponent->GetReadableName(), *Target->GetName());
			// sequencer-todo: notify the spawn track that no more data is needed for this without actually removing the object from the template/cdo
		}
	}

	// Now that we've initialized any new components we can tick all of our recordings to get the last frame's data.
	for (UMovieSceneTrackRecorder* Recorder : TrackRecorders)
	{
		Recorder->RecordSample(CurrentSequenceTime);
	}

	// Update our cached list
	CachedComponentList = CurrentComponentList;
}

void UTakeRecorderActorSource::StopRecording(ULevelSequence* InSequence)
{
	for (UMovieSceneTrackRecorder* TrackRecorder : TrackRecorders)
	{
		TrackRecorder->StopRecording();
	}
	
	ActorSerializer.Close();
}

void UTakeRecorderActorSource::ProcessRecordedTimes(ULevelSequence* InSequence)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();

	TOptional<TRange<FFrameNumber> > FrameRange;
	FMovieSceneBinding* Binding = MovieScene->FindBinding(CachedObjectBindingGuid);
	if (!Binding)
	{
		return;
	}

	// In case we need it later, get the earliest timecode source *before* we
	// add the take section, since its timecode source will be default
	// constructed as all zeros and might accidentally compare as earliest.
	const FMovieSceneTimecodeSource EarliestTimecodeSource = MovieScene->GetEarliestTimecodeSource();

	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		for (UMovieSceneSection* Section : Track->GetAllSections())
		{
			if (Section->HasStartFrame() && Section->HasEndFrame())
			{
				if (!FrameRange.IsSet())
				{
					FrameRange = Section->GetRange();
				}
				else
				{
					FrameRange = TRange<FFrameNumber>::Hull(FrameRange.GetValue(), Section->GetRange());
				}
			}
		}
	}

	UMovieSceneTakeTrack* TakeTrack = MovieScene->FindTrack<UMovieSceneTakeTrack>(CachedObjectBindingGuid);
	if (!TakeTrack)
	{
		TakeTrack = InSequence->GetMovieScene()->AddTrack<UMovieSceneTakeTrack>(CachedObjectBindingGuid);
	}
	TakeTrack->RemoveAllAnimationData();

	UMovieSceneTakeSection* TakeSection = Cast<UMovieSceneTakeSection>(TakeTrack->CreateNewSection());
	TakeTrack->AddSection(*TakeSection);

	if (FrameRange.IsSet())
	{
		TArray<int32> Hours, Minutes, Seconds, Frames;
		TArray<FMovieSceneFloatValue> SubFrames;
		TArray<FFrameNumber> Times;

		const TArray<TPair<FQualifiedFrameTime, FQualifiedFrameTime>>& RecordedTimes = UTakeRecorderSources::RecordedTimes;

		Hours.Reserve(RecordedTimes.Num());
		Minutes.Reserve(RecordedTimes.Num());
		Seconds.Reserve(RecordedTimes.Num());
		Frames.Reserve(RecordedTimes.Num());
		SubFrames.Reserve(RecordedTimes.Num());
		Times.Reserve(RecordedTimes.Num());

		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameRate DisplayRate = MovieScene->GetDisplayRate();

		for (const TPair<FQualifiedFrameTime, FQualifiedFrameTime>& RecordedTimePair : RecordedTimes)
		{
			FFrameNumber FrameNumber = RecordedTimePair.Key.Time.FrameNumber;
			if (!FrameRange.GetValue().Contains(FrameNumber))
			{
				continue;
			}

			FFrameTime FrameTime = FFrameRate::TransformTime(RecordedTimePair.Key.Time, TickResolution, DisplayRate);

			FTimecode Timecode = RecordedTimePair.Value.ToTimecode();
		
			Hours.Add(Timecode.Hours);
			Minutes.Add(Timecode.Minutes);
			Seconds.Add(Timecode.Seconds);
			Frames.Add(Timecode.Frames);

			FMovieSceneFloatValue SubFrame;
			SubFrame.Value = RecordedTimePair.Value.Time.GetSubFrame();
			SubFrame.InterpMode = ERichCurveInterpMode::RCIM_Linear;
			SubFrames.Add(SubFrame);

			Times.Add(FrameNumber);
		}

		Hours.Shrink();
		Minutes.Shrink();
		Seconds.Shrink();
		Frames.Shrink();
		SubFrames.Shrink();
		Times.Shrink();

		TakeSection->HoursCurve.Set(Times, Hours);
		TakeSection->MinutesCurve.Set(Times, Minutes);
		TakeSection->SecondsCurve.Set(Times, Seconds);
		TakeSection->FramesCurve.Set(Times, Frames);
		TakeSection->SubFramesCurve.Set(Times, SubFrames);
	}

	// Since the take section was created post recording here in this
	// function, it wasn't available at the start of recording to have
	// its timecode source set with the other sections, so we set it here.
	if (TakeSection->HoursCurve.GetNumKeys() > 0)
	{
		// We populated the take section's timecode curves with data, so
		// use the first values as the timecode source.
		const int32 Hours = TakeSection->HoursCurve.GetValues()[0];
		const int32 Minutes = TakeSection->MinutesCurve.GetValues()[0];
		const int32 Seconds = TakeSection->SecondsCurve.GetValues()[0];
		const int32 Frames = TakeSection->FramesCurve.GetValues()[0];
		const bool bIsDropFrame = false;
		const FTimecode Timecode(Hours, Minutes, Seconds, Frames, bIsDropFrame);
		TakeSection->TimecodeSource = FMovieSceneTimecodeSource(Timecode);
	}
	else
	{
		// Otherwise, adopt the earliest timecode source from one of the movie
		// scene's other sections as the timecode source for the take section.
		// This case is unlikely.
		TakeSection->TimecodeSource = EarliestTimecodeSource;
	}

	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		TakeSection->Slate.SetDefault(FString::Printf(TEXT("%s_%d"), *TakeMetaData->GetSlate(), TakeMetaData->GetTakeNumber()));
	}

	if (TakeSection->GetAutoSizeRange().IsSet())
	{
		TakeSection->SetRange(TakeSection->GetAutoSizeRange().GetValue());
	}
}

TArray<UTakeRecorderSource*> UTakeRecorderActorSource::PostRecording(ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled)
{
	FTakeRecorderParameters Parameters;
	Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
	Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

	FScopedSlowTask SlowTask((float)TrackRecorders.Num() + 1.0f, FText::Format(LOCTEXT("ProcessingActor", "Generating MovieScene Data for Actor {0}"), Target.IsValid() ? FText::FromString(Target.Get()->GetActorLabel()) : FText::GetEmpty()));
	SlowTask.MakeDialog(false, bShowProgressDialog);

	// We need to do some post-processing tasks on the Track Recorders (such as animation motion source fixup) so we do this now before finalizing
	{
		SlowTask.EnterProgressFrame(0.1f, LOCTEXT("PostProcessingTrackRecorder", "Post Processing Track Recorders"));
		PostProcessTrackRecorders(InSequence);
	}

	// Finalize each Section Recorder and allow it to write data into the Level Sequence.
	int32 SectionRecorderIndex = 0;
	for (UMovieSceneTrackRecorder* SectionRecorder : TrackRecorders)
	{
		// Increment before entering the progress frame so we get "1/1" instead of "0/1"
		SectionRecorderIndex++;

		// takerecorder-todo: Section Recorders should have display names, update this to use those.
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("FinalizingTrackRecorder", "Finalizing Section Recorder {0}/{1}"), SectionRecorderIndex, TrackRecorders.Num()));
		if (bCancelled)
		{
			SectionRecorder->CancelTrack();
		}
		else
		{
			SectionRecorder->FinalizeTrack();
		}
	}

	if (!bCancelled)
	{
		if (Parameters.Project.bRecordTimecode)
		{
			ProcessRecordedTimes(InSequence);
		}

		// Expand the Movie Scene Playback Range to encompass all of the sections now that they've all been created.
		SequenceRecorderUtils::ExtendSequencePlaybackRange(InSequence);

		if (Target.IsValid() && !ParentSource)
		{
			// Automatically add or update the camera cut track if there is a camera component
			AActor* TargetActor = Target.Get();

			if (TargetActor->GetComponentByClass(UCameraComponent::StaticClass()))
			{
				FGuid RecordedCameraGuid = GetRecordedActorGuid(Target.Get());
				FMovieSceneSequenceID RecordedCameraSequenceID = GetLevelSequenceID(Target.Get());
				TakesUtils::CreateCameraCutTrack(InMasterSequence, RecordedCameraGuid, RecordedCameraSequenceID, InSequence->GetMovieScene()->GetPlaybackRange());
			}

			// Swap our target actor to the Editor actor (in case the recording was added while in PIE)
			if (AActor* EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(Target.Get()))
			{
				Target = EditorActor;
			}
		}
	}

	// Force to authority role in case of capturing replicated actors
	if (CachedObjectTemplate.IsValid() && !CachedObjectTemplate->HasAuthority())
	{
		CachedObjectTemplate->SetRole(ROLE_Authority);
	}

	// No longer need to track the Object Template that was created inside the level sequence.
	CachedObjectBindingGuid = FGuid();
	CachedObjectTemplate = nullptr;
	CachedComponentList.Empty();

	// We may have generated some temporary recording sources
	return AddedActorSources;
}

void UTakeRecorderActorSource::FinalizeRecording()
{
	// Null these out there and NOT in PostRecording because they are used for cross sequence object binding via GetLevelSequenceID in PostRecording
	TargetLevelSequence = nullptr;
	MasterLevelSequence = nullptr;

	ParentSource = nullptr;
}

void UTakeRecorderActorSource::PostProcessTrackRecorders(ULevelSequence* InSequence)
{
	FTakeRecorderParameters Parameters;
	Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
	Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

	FString HoursName = GetDefault<UMovieSceneTakeSettings>()->HoursName;
	FString MinutesName = GetDefault<UMovieSceneTakeSettings>()->MinutesName;
	FString SecondsName = GetDefault<UMovieSceneTakeSettings>()->SecondsName;
	FString FramesName = GetDefault<UMovieSceneTakeSettings>()->FramesName;
	FString SubFramesName = GetDefault<UMovieSceneTakeSettings>()->SubFramesName;
	FString SlateName = GetDefault<UMovieSceneTakeSettings>()->SlateName;
				
	FString Slate;
	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		Slate = FString::Printf(TEXT("%s_%d"), *TakeMetaData->GetSlate(), TakeMetaData->GetTakeNumber());
	}

	// We want to look at all Animation Track recorders and remove root motion if the transform
	// for that component is being recorded. We copy the animation out of the Animation Track
	// so that we accurately capture the original motion.
	UMovieScene3DTransformTrackRecorder* RootTransformRecorder = nullptr;
	UMovieSceneAnimationTrackRecorder* FirstAnimationRecorder = nullptr;

	for (UMovieSceneTrackRecorder* TrackRecorder : TrackRecorders)
	{
		AActor* SourceActor = Cast<AActor>(TrackRecorder->GetSourceObject());
		AActor* SourceEditorActor = SourceActor ? EditorUtilities::GetEditorWorldCounterpartActor(SourceActor) : nullptr;
		AActor* TargetActor = Target.Get();

		if (!RootTransformRecorder && TrackRecorder->IsA<UMovieScene3DTransformTrackRecorder>() && (TargetActor == SourceActor || TargetActor == SourceEditorActor))
		{
			RootTransformRecorder = Cast<UMovieScene3DTransformTrackRecorder>(TrackRecorder);
		}
		if (!FirstAnimationRecorder && TrackRecorder->IsA<UMovieSceneAnimationTrackRecorder>())
		{
			FirstAnimationRecorder = Cast<UMovieSceneAnimationTrackRecorder>(TrackRecorder);
		}

		// Early out once we have both of them.
		if (RootTransformRecorder && FirstAnimationRecorder)
		{
			break;
		}
	}
	// We need to take the root motion data from the animation and override the data the Transform Track had originally captured if we are removing root
	if (RootTransformRecorder && FirstAnimationRecorder && FirstAnimationRecorder->RootWasRemoved())
	{
		RootTransformRecorder->PostProcessAnimationData(FirstAnimationRecorder);
		FirstAnimationRecorder->RemoveRootMotion();
	}

	// Remove root motion on all other animation track recorders
	for (UMovieSceneTrackRecorder* TrackRecorder : TrackRecorders)
	{
		if (TrackRecorder->IsA<UMovieSceneAnimationTrackRecorder>())
		{
			UMovieSceneAnimationTrackRecorder* AnimationTrackRecorder = Cast<UMovieSceneAnimationTrackRecorder>(TrackRecorder);
			
			if (TrackRecorder != FirstAnimationRecorder && AnimationTrackRecorder->RootWasRemoved())
			{
				AnimationTrackRecorder->RemoveRootMotion();
			}
			
			if (Parameters.Project.bRecordTimecode)
			{
				AnimationTrackRecorder->ProcessRecordedTimes(HoursName, MinutesName, SecondsName, FramesName, SubFramesName, SlateName, Slate);
			}
		}
	}

	// Reset transform for recorded spawnable actors when their skeletal animation is recorded in world space 
	// but a transform track is not being recorded.
	// 
	// This is a very specific case with the following parameters:
	// 1. Skeletal mesh actor is recorded as a spawnable.
	// 2. Skeletal mesh actor is a child of another actor which is not being recorded (this results in the bones being recorded in world space)
	// 3. Transform track is set to not record.
	//
	// When this occurs, the transform of the spawnable is doubled up because the root bone is in world space
	// and the spawnable template is also in world space.The fix here is to reset the spawnable template's 
	// transform to identity.	
	if (FirstAnimationRecorder && !RootTransformRecorder)
	{
		if (CachedObjectTemplate.IsValid())
		{
			AActor* ActorToRecord = Target.Get();
			if (ActorToRecord)
			{
				USceneComponent* RootComponent = ActorToRecord->GetRootComponent();
				USceneComponent* AttachParent = RootComponent ? RootComponent->GetAttachParent() : nullptr;
				if (AttachParent && !IsOtherActorBeingRecorded(AttachParent->GetOwner()))
				{
					// The object template is marked as bComponentToWorldUpdated=true while the ComponentToWorld doesn't 
					// match the relative location and rotation. So, calling SetRelativeTransform() doesn't work. 
					// Set it directly here.
					CachedObjectTemplate->GetRootComponent()->SetRelativeTransform_Direct(FTransform::Identity);
				}
			}
		}
	}
}

bool UTakeRecorderActorSource::EnsureObjectTemplateHasComponent(UActorComponent* InComponent, UActorComponent*& OutComponent)
{
	check(InComponent);

	//If it's coming from a component that is created from a template defined in the Components section of the Blueprint
	//it will NOT be found as a Component in AllChildren below but it will exist when created so we also exit here.
	//So we only do the check if UserConstructionScript or Instance, with the latter may not be needed.. not sure
	if (InComponent->CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		return false;
	}
	// Attempt to find the component in our Object Template by comparing relative paths. This might fail in complex dynamic hierarchies if a component
	// is added and removed multiple times at runtime if they don't all end up with unique names but is pretty straightforward logic for now.
	FString NewComponentRelativePath = InComponent->GetFullName(InComponent->GetTypedOuter<AActor>());

	AActor* DestinationActor = GetRecordToPossessable() ? Target->GetClass()->GetDefaultObject<AActor>() : CachedObjectTemplate.Get();
	check(DestinationActor);

	TInlineComponentArray<UActorComponent*> AllChildren;
	DestinationActor->GetComponents(AllChildren);

	bool bFoundComponent = false;
	for (UActorComponent* Child : AllChildren)
	{
		if (Child == nullptr)
		{
			continue;
		}
			
		FString ChildRelativePath = Child->GetFullName(DestinationActor);

		if (NewComponentRelativePath == ChildRelativePath)
		{
			bFoundComponent = true;
			break;
		}
	}

	// If we found the component with the same relative path on either the CDO (for Possessables) or the Object Template (for Spawnables) then
	// there's nothing we need to do.
	if (bFoundComponent)
	{
		return false;
	}

	// Possessables can't have objects dynamically added so if this is a new object and they don't have them, warn the user.
	if (GetRecordToPossessable())
	{
		UE_LOG(LogTakesCore, Warning, TEXT("Actor %s had dynamically added component at runtime (%s) but this cannot be saved because we are recording to a possessable, component binding will be broken!"),
			*Target->GetName(), *InComponent->GetName());
		return false;
	}

	// Now we'll go through the process of duplicating the new component and updating our Object Template with it so that the bindings work after the fact.
	USceneComponent* TemplateRoot = CachedObjectTemplate->GetRootComponent();
	USceneComponent* AttachToComponent = nullptr;

	// If the new component is a Scene Component then we'll attach it to the correct parent.
	USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent);
	if (SceneComponent)
	{
		USceneComponent* AttachParent = SceneComponent->GetAttachParent();
		if (AttachParent != nullptr)
		{
			// Get the path to the parent component so we can find the matching path in the template.
			FString ParentRelativePath = AttachParent->GetFullName(Target.Get());

			TInlineComponentArray<USceneComponent*> AllTemplateChildren;
			CachedObjectTemplate->GetComponents(AllTemplateChildren);

			for (USceneComponent* Child : AllTemplateChildren)
			{
				if (Child == nullptr)
				{
					continue;
				}

				FString ChildRelativePath = Child->GetFullName(CachedObjectTemplate.Get());

				if (ParentRelativePath == ChildRelativePath)
				{
					AttachToComponent = Child;
					break;
				}
			}

			if (!AttachToComponent)
			{
				UE_LOG(LogTakesCore, Warning, TEXT("Dynamically added component %s failed to find attach parent %s in Object Template, attaching to root as fallback!"),
					*InComponent->GetName(), *AttachParent->GetName());
				
				AttachToComponent = CachedObjectTemplate->GetRootComponent();
			}
		}
	}

	// Ensure the component name is unique within the Object Template. If there's complex spawn/destroy patterns that don't always use unique names this can
	// cause UniqueComponentName to become a different name than the object it's being copied from which will cause anything attached to this to fail attachment.
	FName UniqueComponentName = MakeUniqueObjectName(CachedObjectTemplate.Get(), InComponent->GetClass(), InComponent->GetFName());
	OutComponent = Cast<UActorComponent>(StaticDuplicateObject(InComponent, CachedObjectTemplate.Get(), UniqueComponentName, RF_AllFlags & ~RF_Transient));

	// Restore attachment
	if (SceneComponent && AttachToComponent && OutComponent->IsA<USceneComponent>())
	{
		USceneComponent* NewSceneComponent = Cast<USceneComponent>(OutComponent);
		NewSceneComponent->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, SceneComponent->GetAttachSocketName());
	}

	// Update our Object Template with a reference to our component
	UE_LOG(LogTakesCore, Log, TEXT("Duplicating Component '%s' to '%s' and adding to Spawnable Object Template."), *InComponent->GetPathName(), *OutComponent->GetPathName());
	CachedObjectTemplate->AddInstanceComponent(OutComponent);

	return true;
}

void UTakeRecorderActorSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target))
	{
		TrackTint = FColor(67, 148, 135);
		AActor* TargetActor = Target.Get();
		if (TargetActor && TargetActor->GetComponentByClass(UCameraComponent::StaticClass()))
		{
			TrackTint = FColor(148, 67, 108);
		}

		// Whenever the actor to record changes we need to rebuild the recorded property map as it
		// displays all possible properties/components to record for the current actor class.
		RebuildRecordedPropertyMap();
	}
}

void UTakeRecorderActorSource::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// When we get deserialized from being duplicated we need to update our numbers.
	// This has to be done after the constructor as the Property Map hasn't been deserialized
	// by that point.
	UpdateCachedNumberOfRecordedProperties();
}

void UTakeRecorderActorSource::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateCachedNumberOfRecordedProperties();
}

void UTakeRecorderActorSource::RebuildRecordedPropertyMap()
{
	// Reset our property map before checking the current actor, this allows null actors to 
	// empty out the property map in the UI.
	FName RootName = Target.IsValid() ? Target->GetFName() : NAME_None;
	RecordedProperties = NewObject<UActorRecorderPropertyMap>(this, MakeUniqueObjectName(GetTransientPackage(), UActorRecorderPropertyMap::StaticClass(), RootName), RF_Transactional);

	//@matth this was making us not be able to record, everything was empty
	//RecordedProperties = NewObject<UActorRecorderPropertyMap>(this, MakeUniqueObjectName(this, UActorRecorderPropertyMap::StaticClass(), RootName), RF_Transactional);
	TrackRecorders.Empty();

	// No target actor means no properties will get recorded
	if (!Target.IsValid())
	{
		return;
	}

	RecordedProperties->RecordedObject = Target.Get();
	RebuildRecordedPropertyMapRecursive(Target.Get(), RecordedProperties);

	UpdateCachedNumberOfRecordedProperties();
}

void UTakeRecorderActorSource::RebuildRecordedPropertyMapRecursive(const FFieldVariant& InObject, UActorRecorderPropertyMap* PropertyMap, const FString& OuterStructPath)
{
	ensure(InObject);
	ensure(PropertyMap);

	// // Iterate through our recorders and find any that can record this object that aren't tied to a specific property. Some things
	// we wish to record (such as Transforms) don't have a specific FProperty or UActorComponent associated with them.
	TArray<IMovieSceneTrackRecorderFactory*> ModularFactories = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneTrackRecorderFactory>(MovieSceneSectionRecorderFactoryName);
	for (IMovieSceneTrackRecorderFactory* Factory : ModularFactories)
	{
	 	if (InObject.IsUObject() && Factory->CanRecordObject(InObject.ToUObject()))
	 	{
	 		// @sequencer-todo: Instead of defaulting to true this should copy from the global settings
	 		FName PropertyName = FName(*Factory->GetDisplayName().ToString());
	 		FActorRecordedProperty RecordedProperty(PropertyName, true, Factory->GetDisplayName());
	 		PropertyMap->Properties.Add(RecordedProperty);

			// Initialize an instance of this factory's settings object if we haven't already.
			TSubclassOf<UMovieSceneTrackRecorderSettings> FactorySettingsClass = Factory->GetSettingsClass();
			if (FactorySettingsClass)
			{
				InitializeFactorySettingsObject(FactorySettingsClass);
			}
	 	}
	}

	// Iterate through the properties on this object and look for ones marked with CPF_Interp ("Expose for Cinematics") or that have metadata
	// that explicitly specifies a sequence track metadata.

	UStruct* ObjectClass = InObject.IsUObject() ? InObject.ToUObject()->GetClass() : nullptr;
	if (FStructProperty* AsStructProperty = InObject.Get<FStructProperty>())
	{
		ObjectClass = AsStructProperty->Struct;
	}
	check(ObjectClass); // With FProperties ObjectClass can only be obtained from AsStructProperty. If we hit this we need to change this check to an 'if (ObjectClass)'
	for (TFieldIterator<FProperty> It(ObjectClass); It; ++It)
	{
		const bool bIsInterpField = It->HasAllPropertyFlags(CPF_Interp);
		const bool bHasTrackMetadata = It->HasMetaData(SequencerTrackClassMetadataName);

		FString PropertyName = It->GetFName().ToString();
		FString PropertyPath = OuterStructPath + FString(*PropertyName);

		if (bIsInterpField || bHasTrackMetadata)
		{
			bool bFoundRecorder = false;
			FText DebugDisplayName;

			// For each property we look to see if there is a specific recorder that can handle this property. This is the case for
			// properties such as "bVisible" which needs the specific Visibility Recorder (instead of a generic bool property recorder).
			// We fall back to the generic property recorder if we can't find a specific recorder, and if any recorder is found then we
			// create an instance to show up in the UI so the user can still choose to toggle on/off properties (and know that the properties
			// shown there do actually have something trying to record them).
			for (IMovieSceneTrackRecorderFactory* Factory : ModularFactories)
			{
				if (InObject.IsUObject() && Factory->CanRecordProperty(InObject.ToUObject(), *It))
				{
			 		DebugDisplayName = Factory->GetDisplayName();

					// Initialize an instance of this factory's settings object if we haven't already.
					TSubclassOf<UMovieSceneTrackRecorderSettings> FactorySettingsClass = Factory->GetSettingsClass();
					if (FactorySettingsClass)
					{
						InitializeFactorySettingsObject(FactorySettingsClass);
					}

			 		// Only one recorder gets a chance to record it
			 		bFoundRecorder = true;
			 		break;
				}
			}
			 
			if (!bFoundRecorder)
			{
				// If we didn't find an explicit recorder for the property, we'll fall back to a generic property recorder which simply stores their state changes in a track.
				FMovieScenePropertyTrackRecorderFactory TrackRecorderFactory;
				if (TrackRecorderFactory.CanRecordProperty(Target.Get(), *It))
				{
			 		DebugDisplayName = TrackRecorderFactory.GetDisplayName();
			 		bFoundRecorder = true;

					// Initialize an instance of this factory's settings object if we haven't already.
					TSubclassOf<UMovieSceneTrackRecorderSettings> FactorySettingsClass = TrackRecorderFactory.GetSettingsClass();
					if (FactorySettingsClass)
					{
						InitializeFactorySettingsObject(FactorySettingsClass);
					}
				}
			}

			if (!bFoundRecorder)
			{
				if (FStructProperty* StructProperty = CastField<FStructProperty>(*It)) 
				{
					FString NewOuterStructPath = OuterStructPath + PropertyName + TEXT(".");
					RebuildRecordedPropertyMapRecursive(StructProperty, PropertyMap, NewOuterStructPath);
				}
			}

			if (bFoundRecorder)
			{
				// @sequencer-todo: Instead of defaulting to true this should copy from the global settings
				FActorRecordedProperty RecordedProperty(*PropertyPath, true, DebugDisplayName);
				PropertyMap->Properties.Add(RecordedProperty);
			}
		}

		else if (FStructProperty* StructProperty = CastField<FStructProperty>(*It))
		{
			FString NewOuterStructPath = OuterStructPath + PropertyName + TEXT(".");
			RebuildRecordedPropertyMapRecursive(StructProperty, PropertyMap, NewOuterStructPath);
		}

	}

	// Now try to iterate through any children on this object and continue this process recursively.
	TSet<UActorComponent*> PossibleComponents;
	TSet<AActor*> ExternalActorsReferenced;

	if (InObject.IsA<AActor>())
	{
		AActor* Actor = InObject.Get<AActor>();
		
		// Actors only have their Root Component plus any Actor Components (which have no hierarchy)
		// After that the structure is recursive down from the Root Component.
		if (Actor->GetRootComponent())
		{
			PossibleComponents.Add(Actor->GetRootComponent());
		}
		GetActorComponents(Actor, PossibleComponents);
	}
	else if (InObject.IsA<USceneComponent>())
	{
		USceneComponent* SceneComponent = InObject.Get<USceneComponent>();
		GetChildSceneComponents(SceneComponent, PossibleComponents, true);
	}

	NewReferencedActors.Append(ExternalActorsReferenced);

	// Now iterate through our children and build the property map recursively.
	for (UActorComponent* Component : PossibleComponents)
	{
		UE_LOG(LogTakesCore, Log, TEXT("Component: %s EditorOnly: %d Transient: %d"), *Component->GetFName().ToString(), Component->IsEditorOnly(), Component->HasAnyFlags(RF_Transient));
		// takerecorder-todo: When merged with Dev Framework, CL 4279185, switch this to checking against
		// IsVisualizationComponent() so that we can exclude things like default component billboards.
		// We also need a deny list of classes, such as those that derive from the input framework that are
		// added at runtime, we don't want to record those.
		if (Component->IsEditorOnly())
		{
			continue;
		}

		UActorRecorderPropertyMap* ComponentPropertyMap = NewObject<UActorRecorderPropertyMap>(this, MakeUniqueObjectName(this, UActorRecorderPropertyMap::StaticClass(), Component->GetFName()), RF_Transactional);
		ComponentPropertyMap->RecordedObject = Component;
		PropertyMap->Children.Add(ComponentPropertyMap);

		RebuildRecordedPropertyMapRecursive(Component, ComponentPropertyMap);
	}
}

void UTakeRecorderActorSource::UpdateCachedNumberOfRecordedProperties()
{
	if (RecordedProperties)
	{
		RecordedProperties->UpdateCachedValues();
	}
}

const FSlateBrush* UTakeRecorderActorSource::GetDisplayIconImpl() const
{
	AActor* TargetActor = Target.Get();
	if (TargetActor)
	{
		return FSlateIconFinder::FindCustomIconBrushForClass(TargetActor->GetClass(), TEXT("ClassThumbnail"));
	}

	return FSlateIconFinder::FindIcon("ClassIcon.Deleted").GetIcon();
}

FText UTakeRecorderActorSource::GetDisplayTextImpl() const
{
	AActor* TargetActor = Target.Get();
	if (TargetActor)
	{
		return FText::FromString(TargetActor->GetActorLabel());
	}

	return LOCTEXT("ActorLabel", "Actor (None)");
}

FText UTakeRecorderActorSource::GetCategoryTextImpl() const
{
	AActor* TargetActor = Target.Get();
	if (TargetActor && TargetActor->GetComponentByClass(UCameraComponent::StaticClass()))
	{
		return LOCTEXT("CamerasCategoryLabel", "Cameras");
	}

	return FText();
}


FText UTakeRecorderActorSource::GetDescriptionTextImpl() const
{
	if (Target.IsValid())
	{
		UActorRecorderPropertyMap::Cache CachedValues;
		if (RecordedProperties)
		{
			CachedValues = RecordedProperties->CachedPropertyComponentCount();
		}
		return FText::Format(LOCTEXT("ActorDescriptionFormat", "{0} Properties {1} Components"), CachedValues.Properties, CachedValues.Components);
	}
	else
	{
		return LOCTEXT("InvalidActorDescription", "No Target Specified");
	}
}

FGuid UTakeRecorderActorSource::ResolveActorFromSequence(AActor* InActor, ULevelSequence* CurrentSequence) const
{
	UMovieScene* MovieScene = CurrentSequence->GetMovieScene();

	// Look through all Possessables in the sequence to see if there's one with the same name as the actor. We purposely do not look at 
	// Spawnables so that recording as spawnable will always create a new spawnable.
	for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
		if (Possessable.GetName() == InActor->GetActorLabel())
		{
			return Possessable.GetGuid();
		}
	}

	// There's no Possessable with the same name as the actor, so this actor hasn't been added
	// to the sequence yet.
	return FGuid();
}

void UTakeRecorderActorSource::PostProcessCreatedObjectTemplateImpl(AActor* ObjectTemplate)
{
	 // Override the Skeletal Mesh components animation modes so that they can play back the recorded
	 // animation asset instead of their original animation source (such as Animation Blueprint)
	 TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	 ObjectTemplate->GetComponents(SkeletalMeshComponents);
	 for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	 {
	 	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	 	SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
	 	SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	 	SkeletalMeshComponent->SetForcedLOD(1);
	 }

	// Disable auto-possession on recorded Pawns so that when the Spawnable is spawned it doesn't auto-possess the player
	// and override their current live player pawn.
	if (ObjectTemplate->IsA(APawn::StaticClass()))
	{
		APawn* Pawn = CastChecked<APawn>(ObjectTemplate);
		Pawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	}

	// Disable any Movement Components so that things such as RotatingMovementComponent or ProjectileMovementComponent don't suddenly
	// start moving and overriding our position at runtime.
	// takerecorder-todo: This should ideally check to see if you recorded the transform of the root object or not before assuming you
	// don't want its movement?
	TInlineComponentArray<UMovementComponent*> MovementComponents;
	ObjectTemplate->GetComponents(MovementComponents);
	for (UMovementComponent* MovementComponent : MovementComponents)
	{
		MovementComponent->bAutoActivate = false;
	}
}

void GetChildBindings(UMovieScene* InMovieScene, const FGuid& InGuid, TArray<FGuid>& OutChildGuids)
{
	for (int32 PossessableIndex = 0; PossessableIndex < InMovieScene->GetPossessableCount(); ++PossessableIndex)
	{
		FMovieScenePossessable& Child = InMovieScene->GetPossessable(PossessableIndex);
		if (Child.GetParent() == InGuid)
		{
			OutChildGuids.Add(Child.GetGuid());

			GetChildBindings(InMovieScene, Child.GetGuid(), OutChildGuids);
		}
	}
}

void UTakeRecorderActorSource::CleanExistingDataFromSequence(const FGuid& ForGuid, ULevelSequence& InSequence)
{
	if (ForGuid.IsValid())
	{
		// Check to see if there is a Possessable in this sequence with the specified Guid and remove their old data if needed.
		// Removing the Possessable will remove their bindings which will remove the associated tracks and their data as well.
		UMovieScene* MovieScene = InSequence.GetMovieScene();

		TArray<FGuid> OutChildGuids;
		GetChildBindings(MovieScene, ForGuid, OutChildGuids);

		MovieScene->RemoveSpawnable(ForGuid);
		MovieScene->RemovePossessable(ForGuid);

		for (auto ChildGuid : OutChildGuids)
		{
			MovieScene->RemovePossessable(ChildGuid);
		}
	}

	// Call any derived class implementation
	CleanExistingDataFromSequenceImpl(ForGuid, InSequence);
}

void UTakeRecorderActorSource::GetAllComponents(TSet<UActorComponent*>& OutArray, bool bUpdateReferencedActorList)
{
	if (Target.IsValid())
	{
		GetActorComponents(Target.Get(), OutArray);
		GetSceneComponents(Target->GetRootComponent(), OutArray, bUpdateReferencedActorList);
	}
}

void UTakeRecorderActorSource::GetSceneComponents(USceneComponent* OnSceneComponent, TSet<UActorComponent *>& OutArray, bool bUpdateReferencedActorList)
{
	if (!OnSceneComponent)
	{
		return;
	}

	// Add the passed in component to the out array and then we'll recursively call GetSceneComponents on each child
	// so that each child gets added to the out array and their children recursively.
	if (OnSceneComponent->ComponentHasTag(DoNotRecordTag))
	{
		UE_LOG(LogTakesCore, Verbose, TEXT("Skipping record component: %s with do not record tag"), *OnSceneComponent->GetName());
		return;
	}

	OutArray.Add(OnSceneComponent);

	TSet<UActorComponent*> ChildComponents;
	GetChildSceneComponents(OnSceneComponent, ChildComponents, bUpdateReferencedActorList);

	for (UActorComponent* Component : ChildComponents)
	{
		GetSceneComponents(Cast<USceneComponent>(Component), OutArray, bUpdateReferencedActorList);
	}
}

void UTakeRecorderActorSource::GetChildSceneComponents(USceneComponent* OnSceneComponent, TSet<UActorComponent*>& OutArray, bool bUpdateReferencedActorList)
{
	if (OnSceneComponent)
	{
		const bool bIncludeAllDescendants = false;
		TArray<USceneComponent*> OutDirectChildren;
		OnSceneComponent->GetChildrenComponents(bIncludeAllDescendants, OutDirectChildren);
		
		// Add Scene Components to the OutArray
		for (USceneComponent* SceneComponent : OutDirectChildren)
		{
			if (!SceneComponent)
			{
				continue;
			}

			// If this scene component is owned by another Actor we have to make a complicated decision. In general, we don't want to record
			// components that are owned by another Actor because if that Actor is also being recorded we end up with duplicate bindings in the resulting
			// sequence. To solve this one, we want to create a recording for the Actor that owns that component (if it's not already being recorded) and
			// re-create the effect using Attach tracks in Sequencer.
			// Unfortunately, this leads to its own set of problems. In a situation where a complex hierarchy has been created via the World Outliner and you
			// are recording the root object, it will create bindings for all children as they will all show up as belonging to different actors and we'll create
			// a recording for each one. This isn't desirable either so we could just not record components that belong to other actors unless you specifically add
			// them. In the usual twist of fate this isn't desirable either as character setups (especially QAPawn) use a separate actor for inventory and hot-swap
			// which gun your Pawn is holding by changing the attachment of the weapon skeletal mesh. In this case we do want to record the separate actor automatically
			// as it's pretty hard for the user to add the player already, much less actors the player spawns.
			// The current solution is as follows:
			// If the Scene Component is the Root Component of its owner then we do NOT add that actor to be recorded. This solves the case of nestled hierarchies. Do not recurse children.
			// If the Scene Component is not the Root Component of its owner, then we DO add the owner actor to be recorded (but skip the component as that actor will record it)
			// If the Scene Component's owner is spawned at Runtime, we record it.
			if (SceneComponent->GetOwner() != Target.Get())
			{
				bool bActorIsTemporary = GEditor && (SceneComponent->GetOwner()->GetWorld()->WorldType == EWorldType::PIE && !GEditor->ObjectsThatExistInEditorWorld.Get(SceneComponent->GetOwner()));
				if (bActorIsTemporary)
				{
					if (bUpdateReferencedActorList)
					{
						// Only log if they care about the referenced actors
						UE_LOG(LogTakesCore, Log, TEXT("Detected Runtime-Spawned Actor %s that is attached to current hierarchy. Adding Actor to list to be recorded so we can re-create this hierarchy through Attach Tracks!"),
							*SceneComponent->GetName(), *SceneComponent->GetOwner()->GetName());
						NewReferencedActors.Add(SceneComponent->GetOwner());
					}
					continue;
				}
				// This component belongs to another actor. We check to see if it's the root component to decide if we should record it or not.
				else if (SceneComponent == SceneComponent->GetOwner()->GetRootComponent())
				{
					if (bUpdateReferencedActorList)
					{
						// Only log if they care about the referenced actors.
						UE_LOG(LogTakesCore, Warning, TEXT("Detected Root Component %s on Actor %s attached to current hierarchy. Skipping the automatic addition of this actor to the Recording to avoid recording hierarchies created in the World Outliner!"),
							*SceneComponent->GetName(), *SceneComponent->GetOwner()->GetName());
					}
					continue;
				}
				else
				{
					if (bUpdateReferencedActorList)
					{
						// Only log if they care about the referenced actors
						UE_LOG(LogTakesCore, Log, TEXT("Detected Component %s from Actor %s that is attached to current hierarchy. Adding Actor to list to be recorded so we can re-create this hierarchy through Attach Tracks!"),
							*SceneComponent->GetName(), *SceneComponent->GetOwner()->GetName());
						NewReferencedActors.Add(SceneComponent->GetOwner());
					}
					continue;
				}
			}

			if (SceneComponent->ComponentHasTag(DoNotRecordTag))
			{
				UE_LOG(LogTakesCore, Warning, TEXT("Skipping record component: %s with do not record tag"), *SceneComponent->GetName());
				continue;
			}

			// We own this component so we're going to go ahead and return it so that we record it.
			OutArray.Add(SceneComponent);
		}
	}
}

void UTakeRecorderActorSource::GetActorComponents(AActor* OnActor, TSet<UActorComponent*>& OutArray) const
{
	if (OnActor)
	{
		TInlineComponentArray<UActorComponent*> ActorComponents(OnActor);
		OutArray.Reserve(ActorComponents.Num());

		for (UActorComponent* ActorComponent : ActorComponents)
		{
			USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent);

			// Child of the root component are gathered in GetSceneComponents(). 
			// Here we gather the rest of the components - either non scene components or 
			// scene components that are not directly attached to the root component. This 
			// includes spawned particle systems
			if (!SceneComponent || !SceneComponent->GetAttachParent())
			{
				if (ActorComponent->GetOwner() != Target.Get())
				{
					UE_LOG(LogTakesCore, Warning, TEXT("Unsupported Functionality: Actor Component: %s is owned by another Actor: %s, skipping record!"),
						*ActorComponent->GetName(), *ActorComponent->GetOwner()->GetName());
					continue;
				}

				if (ActorComponent->ComponentHasTag(DoNotRecordTag))
				{
					UE_LOG(LogTakesCore, Warning, TEXT("Skipping record component: %s with do not record tag"), *ActorComponent->GetName());
					continue;
				}

				OutArray.Add(ActorComponent);
			}
		}
	}
}

void UTakeRecorderActorSource::CreateNewActorSourceForReferencedActors()
{
	UTakeRecorderSources* SourcesList = GetTypedOuter<UTakeRecorderSources>();
	TArray<UTakeRecorderSource*> NewSources;

	TSet<AActor*> ActorsWithEnabledSources;
	TSet<AActor*> ActorsWithDisabledSources;
	for (UTakeRecorderSource* Source : SourcesList->GetSources())
	{
		if (UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source))
		{
			AActor* TargetActor = ActorSource->Target.Get();
			if (TargetActor)
			{
				if (ActorSource->bEnabled)
				{
					ActorsWithEnabledSources.Add(TargetActor);
				}
				else
				{
					ActorsWithDisabledSources.Add(TargetActor);
				}
			}
		}
	}

	for (AActor* Actor : NewReferencedActors)
	{
		// Don't create a recording for this actor if there is an existing source for this actor. Another source may have added it
		// or the user may have added it by hand and adjusted settings.
		if (ActorsWithEnabledSources.Contains(Actor))
		{
			continue;
		}

		// Also, don't create a recording if the actor has a disabled source
		if (ActorsWithDisabledSources.Contains(Actor))
		{
			UE_LOG(LogTakesCore, Warning, TEXT("Disregarding automatically adding %s as a recording source because it has been explicitly disabled."), *Actor->GetName());
			continue;
		}

		if (Actor == Target.Get())
		{
			// This probably shouldn't happen but safe guard to keep us from creating a new recording for ourself. We won't
			// fail the above check as the recording hasn't gotten created yet.
			continue;
		}

		// We don't use AddSource on the UTakeRecorderSources because this is called from functions that also adds returned items to the Source List. This prevents a 
		// double add from occuring.
		UTakeRecorderActorSource* ActorSource = NewObject<UTakeRecorderActorSource>(SourcesList, UTakeRecorderActorSource::StaticClass(), NAME_None, RF_Transactional);
		ActorSource->ParentSource = this;

		// We add it both to the local list (in case we need to start recording immediately) and to the class list
		// so that we can clean up the recording when we finish.
		NewSources.Add(ActorSource);
		AddedActorSources.Add(ActorSource);

		ActorSource->Target = Actor;

		// For consistency in the hierarchy, actor sources should have the same state as the source automatically adding them
		ActorSource->RecordType = RecordType;

		// Send a PropertyChangedEvent so the class catches the callback and rebuilds the property map. We can't rely on the Actor rebuilding the map on PreRecording
		// because that would wipe out any user adjustments from one added natively.
		FPropertyChangedEvent PropertyChangedEvent(UTakeRecorderActorSource::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)), EPropertyChangeType::ValueSet);
		ActorSource->PostEditChangeProperty(PropertyChangedEvent);
	}
	NewReferencedActors.Reset();
}

bool UTakeRecorderActorSource::IsOtherActorBeingRecorded(AActor* OtherActor) const
{
	return TakeRecorderSourcesUtils::IsActorBeingRecorded(this, OtherActor);
}

FGuid UTakeRecorderActorSource::GetRecordedActorGuid(class AActor* OtherActor) const
{
	return TakeRecorderSourcesUtils::GetRecordedActorGuid(this, OtherActor);
}

FTransform UTakeRecorderActorSource::GetRecordedActorAnimationInitialRootTransform(class AActor* OtherActor) const
{
	return TakeRecorderSourcesUtils::GetRecordedActorAnimationInitialRootTransform(this, OtherActor);
}

FMovieSceneSequenceID UTakeRecorderActorSource::GetLevelSequenceID(class AActor* OtherActor)
{
	return TakeRecorderSourcesUtils::GetLevelSequenceID(this, OtherActor, MasterLevelSequence);
}

FTrackRecorderSettings UTakeRecorderActorSource::GetTrackRecorderSettings() const
{
	FTrackRecorderSettings TrackRecorderSettings;

	UTakeRecorderSources* Sources = GetTypedOuter<UTakeRecorderSources>();
	if (!Sources)
	{
		return TrackRecorderSettings;
	}
			
	FTakeRecorderParameters Parameters;
	Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
	Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

	TrackRecorderSettings.bRecordToPossessable = GetRecordToPossessable();
	TrackRecorderSettings.bReduceKeys = bReduceKeys;
	TrackRecorderSettings.bRemoveRedundantTracks = Parameters.User.bRemoveRedundantTracks;
	TrackRecorderSettings.bSaveRecordedAssets = Sources->GetSettings().bSaveRecordedAssets || GEditor == nullptr;
	TrackRecorderSettings.ReduceKeysTolerance = Parameters.User.ReduceKeysTolerance;

	TrackRecorderSettings.DefaultTracks = Parameters.Project.DefaultTracks;
	TrackRecorderSettings.IncludeAnimationNames = IncludeAnimationNames;
	TrackRecorderSettings.ExcludeAnimationNames = ExcludeAnimationNames;

	return TrackRecorderSettings;
}

void UTakeRecorderActorSource::SetSourceActor(TSoftObjectPtr<AActor> InTarget)
{
	Target = InTarget;

	// Whenever the actor to record changes we need to rebuild the recorded property map as it
	// displays all possible properties/components to record for the current actor class.
	RebuildRecordedPropertyMap();
}

bool UTakeRecorderActorSource::GetRecordToPossessable() const
{
	if (RecordType == ETakeRecorderActorRecordType::ProjectDefault)
	{
		if (UTakeRecorderSources* Sources = GetTypedOuter<UTakeRecorderSources>())
		{
			return Sources->GetSettings().bRecordToPossessable;
		}
	}

	return RecordType == ETakeRecorderActorRecordType::Possessable;
}

void UTakeRecorderActorSource::InitializeFactorySettingsObject(TSubclassOf<UMovieSceneTrackRecorderSettings> InClass)
{
	ensure(InClass);

	bool bHasExisting = false;
	for (UObject* ExistingSetting : FactorySettings)
	{
		if (ExistingSetting->GetClass() == InClass)
		{
			bHasExisting = true;
		}
	}

	// We only want to add it to the list if we don't already have it so that only one instance shows up in the UI
	// regardless of how many instances of this factory are recording.
	if (!bHasExisting)
	{
		UMovieSceneTrackRecorderSettings* NewSettingsObject = NewObject<UMovieSceneTrackRecorderSettings>(this, InClass, NAME_None, RF_Transactional);
		FactorySettings.Add(NewSettingsObject);
	}
}

UMovieSceneTrackRecorderSettings* UTakeRecorderActorSource::GetSettingsObjectForFactory(TSubclassOf<UMovieSceneTrackRecorderSettings> InClass) const
{
	for (UObject* ExistingSetting : FactorySettings)
	{
		if (ExistingSetting->GetClass() == InClass)
		{
			return Cast<UMovieSceneTrackRecorderSettings>(ExistingSetting);
		}
	}

	// Most factories won't have a settings object and that's okay!
	return nullptr;
}

FString UTakeRecorderActorSource::GetSubsceneTrackName(ULevelSequence* InSequence) const
{
	if (Target.IsValid())
	{
		return Target->GetActorLabel();
	}

	return Super::GetSubsceneTrackName(InSequence);
}

FString UTakeRecorderActorSource::GetSubsceneAssetName(ULevelSequence* InSequence) const
{
	if (Target.IsValid())
	{
		if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
		{
			return FString::Printf(TEXT("%s_%s"), *Target->GetActorLabel(), *TakeMetaData->GenerateAssetPath("{slate}_{take}"));
		}
		return Target->GetActorLabel();
	}

	return Super::GetSubsceneAssetName(InSequence);
}

void UTakeRecorderActorSource::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	InFolder->AddChildObjectBinding(CachedObjectBindingGuid);
}

UActorRecorderPropertyMap* UTakeRecorderActorSource::GetPropertyMapForComponentRecursive(UActorComponent* InComponent, UActorRecorderPropertyMap* CurrentPropertyMap)
{
	check(CurrentPropertyMap);
	if (CurrentPropertyMap->RecordedObject.Get() == InComponent)
	{
		return CurrentPropertyMap;
	}

	for (UActorRecorderPropertyMap* Child : CurrentPropertyMap->Children)
	{
		UActorRecorderPropertyMap* ChildMap = GetPropertyMapForComponentRecursive(InComponent, Child);
		if (ChildMap)
		{
			return ChildMap;
		}
	}

	return nullptr;
}

UActorRecorderPropertyMap* UTakeRecorderActorSource::GetParentPropertyMapForComponent(UActorComponent* InComponent)
{
	check(InComponent);

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent))
	{
		USceneComponent* AttachParent = SceneComponent->GetAttachParent();
		if (AttachParent)
		{
			return GetPropertyMapForComponentRecursive(AttachParent, RecordedProperties);
		}
	}

	// ActorComponents and Root Scene Components will go through this path and we'll use the root actor property map.
	return RecordedProperties;
}

#undef LOCTEXT_NAMESPACE // "UTakeRecorderActorSource"

