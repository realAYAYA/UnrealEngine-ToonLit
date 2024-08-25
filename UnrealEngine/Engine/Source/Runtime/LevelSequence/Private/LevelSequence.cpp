// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequence.h"
#include "IMovieSceneMetaData.h"
#include "MovieSceneMetaData.h"
#include "Engine/EngineTypes.h"
#include "HAL/IConsoleManager.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorFragmentType.h"
#include "UniversalObjectLocatorResolveParameterBuffer.inl"
#include "UniversalObjectLocators/ActorLocatorFragment.h"
#include "WorldPartition/IWorldPartitionObjectResolver.h"
#include "LegacyLazyObjectPtrFragment.h"
#include "SubObjectLocator.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "LevelSequenceDirector.h"
#include "Engine/Engine.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Animation/AnimInstance.h"
#include "LevelSequenceModule.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieScene3DPathTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieSceneDataLayerTrack.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneSlomoTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCVarTrack.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"
#include "Modules/ModuleManager.h"
#include "LevelSequencePlayer.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "UniversalObjectLocators/AnimInstanceLocatorFragment.h"
#include "Engine/AssetUserData.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequence)


#if WITH_EDITOR
	#include "UObject/SequencerObjectVersion.h"
	#include "UObject/ObjectRedirector.h"

ULevelSequence::FPostDuplicateEvent ULevelSequence::PostDuplicateEvent;

#endif

static TAutoConsoleVariable<int32> CVarDefaultLockEngineToDisplayRate(
	TEXT("LevelSequence.DefaultLockEngineToDisplayRate"),
	0,
	TEXT("0: Playback locked to playback frames\n1: Unlocked playback with sub frame interpolation"),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDefaultTickResolution(
	TEXT("LevelSequence.DefaultTickResolution"),
	TEXT("24000fps"),
	TEXT("Specifies the default tick resolution for newly created level sequences. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDefaultDisplayRate(
	TEXT("LevelSequence.DefaultDisplayRate"),
	TEXT("30fps"),
	TEXT("Specifies the default display frame rate for newly created level sequences; also defines frame locked frame rate where sequences are set to be frame locked. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDefaultClockSource(
	TEXT("LevelSequence.DefaultClockSource"),
	0,
	TEXT("Specifies the default clock source for newly created level sequences. 0: Tick, 1: Platform, 2: Audio, 3: RelativeTimecode, 4: Timecode, 5: Custom"),
	ECVF_Default);

ULevelSequence::ULevelSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{
	bParentContextsAreSignificant = true;
}

void ULevelSequence::Initialize()
{
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);

	const bool bFrameLocked = CVarDefaultLockEngineToDisplayRate.GetValueOnGameThread() != 0;

	MovieScene->SetEvaluationType( bFrameLocked ? EMovieSceneEvaluationType::FrameLocked : EMovieSceneEvaluationType::WithSubFrames );

	FFrameRate TickResolution(60000, 1);
	TryParseString(TickResolution, *CVarDefaultTickResolution.GetValueOnGameThread());
	MovieScene->SetTickResolutionDirectly(TickResolution);

	FFrameRate DisplayRate(30, 1);
	TryParseString(DisplayRate, *CVarDefaultDisplayRate.GetValueOnGameThread());
	MovieScene->SetDisplayRate(DisplayRate);

	int32 ClockSource = CVarDefaultClockSource.GetValueOnGameThread();
	MovieScene->SetClockSource((EUpdateClockSource)ClockSource);

#if WITH_EDITOR
	UMovieSceneMetaData* MetaData = FindOrAddMetaData<UMovieSceneMetaData>();
	MetaData->SetCreated(FDateTime::UtcNow());
	MetaData->SetAuthor(FApp::GetSessionOwner());
#endif
}

UObject* ULevelSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	return MovieSceneHelpers::MakeSpawnableTemplateFromInstance(InSourceObject, MovieScene, ObjectName);
}

bool ULevelSequence::CanAnimateObject(UObject& InObject) const 
{
	return InObject.IsA<AActor>() || InObject.IsA<UActorComponent>() || InObject.IsA<UAnimInstance>();
}

#if WITH_EDITOR

ETrackSupport ULevelSequence::IsTrackSupported(TSubclassOf<class UMovieSceneTrack> InTrackClass) const
{
	if (!UMovieScene::IsTrackClassAllowed(InTrackClass))
	{
		return ETrackSupport::NotSupported;
	}

	if (InTrackClass == UMovieScene3DAttachTrack::StaticClass() ||
		InTrackClass == UMovieScene3DPathTrack::StaticClass() ||
		InTrackClass == UMovieSceneAudioTrack::StaticClass() ||
		InTrackClass == UMovieSceneCameraCutTrack::StaticClass() ||
		InTrackClass == UMovieSceneCinematicShotTrack::StaticClass() ||
		InTrackClass == UMovieSceneEventTrack::StaticClass() ||
		InTrackClass == UMovieSceneFadeTrack::StaticClass() ||
		InTrackClass == UMovieSceneLevelVisibilityTrack::StaticClass() ||
		InTrackClass == UMovieSceneDataLayerTrack::StaticClass() ||
		InTrackClass == UMovieSceneMaterialParameterCollectionTrack::StaticClass() ||
		InTrackClass == UMovieSceneSkeletalAnimationTrack::StaticClass() ||
		InTrackClass == UMovieSceneSlomoTrack::StaticClass() ||
		InTrackClass == UMovieSceneSpawnTrack::StaticClass() ||
		InTrackClass == UMovieSceneSubTrack::StaticClass() ||
		InTrackClass == UMovieSceneCVarTrack::StaticClass() ||
		InTrackClass == UMovieSceneBindingLifetimeTrack::StaticClass())
	{
		return ETrackSupport::Supported;
	}

	return Super::IsTrackSupported(InTrackClass);
}

void ULevelSequence::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void ULevelSequence::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
#if WITH_EDITORONLY_DATA
	if (DirectorBlueprint)
	{
		DirectorBlueprint->GetAssetRegistryTags(Context);
	}
#endif

	for (UObject* MetaData : MetaDataObjects)
	{
		IMovieSceneMetaDataInterface* MetaDataInterface = Cast<IMovieSceneMetaDataInterface>(MetaData);
		if (MetaDataInterface)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS;
			TArray<UObject::FAssetRegistryTag> DeprecatedFunctionTags;
			MetaDataInterface->ExtendAssetRegistryTags(DeprecatedFunctionTags);
			for (UObject::FAssetRegistryTag& Tag : DeprecatedFunctionTags)
			{
				Context.AddTag(MoveTemp(Tag));
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS;
			MetaDataInterface->ExtendAssetRegistryTags(Context);
		}
	}

	Super::GetAssetRegistryTags(Context);
}

void ULevelSequence::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	for (UObject* MetaData : MetaDataObjects)
	{
		IMovieSceneMetaDataInterface* MetaDataInterface = Cast<IMovieSceneMetaDataInterface>(MetaData);
		if (MetaDataInterface)
		{
			MetaDataInterface->ExtendAssetRegistryTagMetaData(OutMetadata);
		}
	}

	Super::GetAssetRegistryTagMetadata(OutMetadata);
}

void ULevelSequence::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	// GetAssetRegistryTags appends the DirectorBlueprint tags to the World's tags, so we also have to run the Blueprint PostLoadAssetRegistryTags
	UBlueprint::PostLoadBlueprintAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);
}

void PurgeLegacyBlueprints(UObject* InObject, UPackage* Package)
{
	if (UBlueprint* BP = Cast<UBlueprint>(InObject))
	{
		UPackage* TransientPackage = GetTransientPackage();

		{
			FString OldName = BP->GetName();

			BP->ClearFlags(RF_Public);
			BP->SetFlags(RF_Transient);
			BP->RemoveFromRoot();

			FName NewName = MakeUniqueObjectName(TransientPackage, UBlueprint::StaticClass(), *FString::Printf(TEXT("DEAD_SPAWNABLE_BLUEPRINT_%s"), *BP->GetName()));
			BP->Rename(*NewName.ToString(), GetTransientPackage(), (REN_NonTransactional|REN_ForceNoResetLoaders|REN_DoNotDirty));

			UE_LOG(LogLevelSequence, Log, TEXT("Discarding blueprint '%s' from package '%s'."), *OldName, *Package->GetName());
		}

		if (BP->GeneratedClass)
		{
			FName    OldName    = BP->GeneratedClass->GetFName();
			UObject* OldOuter   = BP->GeneratedClass->GetOuter();
			UClass*  SuperClass = BP->GeneratedClass->GetSuperClass();

			if( BP->GeneratedClass->ClassDefaultObject )
			{
				BP->GeneratedClass->ClassDefaultObject->ClearFlags(RF_Public);
				BP->GeneratedClass->ClassDefaultObject->SetFlags(RF_Transient);
				BP->GeneratedClass->ClassDefaultObject->RemoveFromRoot();
			}

			BP->GeneratedClass->ClearFlags(RF_Public);
			BP->GeneratedClass->SetFlags(RF_Transient);
			BP->GeneratedClass->ClassFlags |= CLASS_Deprecated;
			BP->GeneratedClass->RemoveFromRoot();

			FName NewName = MakeUniqueObjectName(TransientPackage, BP->GeneratedClass, *FString::Printf(TEXT("DEAD_SPAWNABLE_BP_CLASS_%s_C"), *BP->GeneratedClass->ClassGeneratedBy->GetName()));
			BP->GeneratedClass->Rename(*NewName.ToString(), GetTransientPackage(), (REN_DoNotDirty|REN_NonTransactional|REN_ForceNoResetLoaders));

			if (SuperClass)
			{
				UObjectRedirector* Redirector = NewObject<UObjectRedirector>(OldOuter, OldName);
				Redirector->DestinationObject = SuperClass;

				UE_LOG(LogLevelSequence, Log, TEXT("Discarding generated class '%s' from package '%s'. Replacing with redirector to '%s'"), *OldName.ToString(), *Package->GetName(), *SuperClass->GetName());
			}
			else
			{
				UE_LOG(LogLevelSequence, Log, TEXT("Discarding generated class '%s' from package '%s'. Unable to create redirector due to no super class."), *OldName.ToString(), *Package->GetName());
			}
		}
	}
}
#endif

void ULevelSequence::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

#if WITH_EDITORONLY_DATA
	if (DirectorBlueprint)
	{
		DirectorClass = DirectorBlueprint->GeneratedClass.Get();

		// Remove the binding for the director blueprint recompilation and re-add it to be sure there is only one entry in the list
		DirectorBlueprint->OnCompiled().RemoveAll(this);
		DirectorBlueprint->OnCompiled().AddUObject(this, &ULevelSequence::OnDirectorRecompiled);
	}
	else
	{
		DirectorClass = nullptr;
	}
#endif

#if WITH_EDITOR
	if (PostDuplicateEvent.IsBound())
	{
		PostDuplicateEvent.Execute(this);
	}
#endif

#if WITH_EDITOR
	UMovieSceneMetaData* MetaData = FindOrAddMetaData<UMovieSceneMetaData>();
	MetaData->SetCreated(FDateTime::UtcNow());
	MetaData->SetAuthor(FApp::GetSessionOwner());
	MetaData->SetNotes(FString()); // Intentionally clear the notes
#endif
}

void ULevelSequence::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (MovieScene)
	{
		// Remove any invalid object bindings. This was moved from PostInitProperties
		//   because it has to happen after the asset has actually been serialized.
		TSet<FGuid> ValidObjectBindings;
		for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
		{
			ValidObjectBindings.Add(MovieScene->GetSpawnable(Index).GetGuid());
		}
		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			ValidObjectBindings.Add(MovieScene->GetPossessable(Index).GetGuid());
		}

		BindingReferences.RemoveInvalidBindings(ValidObjectBindings);
	}

	if (!DirectorBlueprint)
	{
		UBlueprint* PhantomDirector = FindObject<UBlueprint>(this, TEXT("SequenceDirector"));
		if (!ensureMsgf(!PhantomDirector, TEXT("Phantom sequence director found in sequence '%s' which has a nullptr DirectorBlueprint. Re-assigning to prevent future crash."), *GetName()))
		{
			DirectorBlueprint = PhantomDirector;
		}
	}

	if (DirectorBlueprint)
	{
		DirectorBlueprint->ClearFlags(RF_Standalone);

		// Remove the binding for the director blueprint recompilation and re-add it to be sure there is only one entry in the list
		DirectorBlueprint->OnCompiled().RemoveAll(this);
		DirectorBlueprint->OnCompiled().AddUObject(this, &ULevelSequence::OnDirectorRecompiled);

		if (DirectorBlueprint->Rename(*GetDirectorBlueprintName(), nullptr, (REN_NonTransactional|REN_ForceNoResetLoaders|REN_DoNotDirty|REN_Test|REN_DontCreateRedirectors)))
		{
			DirectorBlueprint->Rename(*GetDirectorBlueprintName(), nullptr, (REN_NonTransactional|REN_ForceNoResetLoaders|REN_DoNotDirty|REN_DontCreateRedirectors));
		}
	}

	if (MovieScene)
	{
		TSet<FGuid> InvalidSpawnables;

		for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
		{
			FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(Index);
			if (!Spawnable.GetObjectTemplate())
			{
				if (Spawnable.GeneratedClass_DEPRECATED && Spawnable.GeneratedClass_DEPRECATED->ClassGeneratedBy)
				{
					const FName TemplateName = MakeUniqueObjectName(MovieScene, UObject::StaticClass(), Spawnable.GeneratedClass_DEPRECATED->ClassGeneratedBy->GetFName());

					UObject* NewTemplate = NewObject<UObject>(MovieScene, Spawnable.GeneratedClass_DEPRECATED->GetSuperClass(), TemplateName);
					if (NewTemplate)
					{
						Spawnable.CopyObjectTemplate(*NewTemplate, *this);
					}
				}
			}

			if (!Spawnable.GetObjectTemplate())
			{
				InvalidSpawnables.Add(Spawnable.GetGuid());
				UE_LOG(LogLevelSequence, Warning, TEXT("Spawnable '%s' with ID '%s' does not have a valid object template"), *Spawnable.GetName(), *Spawnable.GetGuid().ToString());
			}
		}
	}

	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::PurgeSpawnableBlueprints)
	{
		// Remove any old generated classes from the package that will have been left behind from when we used blueprints for spawnables		
		{
			UPackage* Package = GetOutermost();
			TArray<UObject*> PackageSubobjects;
			GetObjectsWithOuter(Package, PackageSubobjects, false);
			for (UObject* ObjectInPackage : PackageSubobjects)
			{
				PurgeLegacyBlueprints(ObjectInPackage, Package);
			}
		}
	}

	for (TPair<FGuid, FLevelSequenceLegacyObjectReference>& Pair : ObjectReferences_DEPRECATED.Map)
	{
		if (Pair.Value.ObjectId.IsValid())
		{
			FUniversalObjectLocator NewLocator;
			NewLocator.AddFragment<FLegacyLazyObjectPtrFragment>(Pair.Value.ObjectId.GetGuid());
			BindingReferences.FMovieSceneBindingReferences::AddBinding(Pair.Key, MoveTemp(NewLocator));
		}
		else if (Pair.Value.ObjectPath.Len() > 0)
		{
			FUniversalObjectLocator NewLocator;
			NewLocator.AddFragment<FSubObjectLocator>(Pair.Value.ObjectPath);
			BindingReferences.FMovieSceneBindingReferences::AddBinding(Pair.Key, MoveTemp(NewLocator));
		}
	}
	ObjectReferences_DEPRECATED.Map.Empty();

#endif
}

#if WITH_EDITORONLY_DATA
void ULevelSequence::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UObjectRedirector::StaticClass()));
}
#endif

void ULevelSequence::PostInitProperties()
{
	Super::PostInitProperties();
}

bool ULevelSequence::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
	bool bRetVal = Super::Rename(NewName, NewOuter, Flags);

#if WITH_EDITOR
	if (DirectorBlueprint)
	{
		DirectorBlueprint->Rename(*GetDirectorBlueprintName(), this, Flags);
	}
#endif

	return bRetVal;
}

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (Context)
	{
		BindingReferences.AddBinding(ObjectId, &PossessedObject, Context);
	}
}

bool ULevelSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return true;
}

void ULevelSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, const FLevelSequenceBindingReference::FResolveBindingParams& InResolveBindingParams, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	using namespace UE::UniversalObjectLocator;

	TResolveParamsWithBuffer<128> ResolveParams;

	ResolveParams.AddParameter(FActorLocatorFragmentResolveParameter::ParameterType,
		InResolveBindingParams.StreamingWorld,
		InResolveBindingParams.WorldPartitionResolveData ? InResolveBindingParams.WorldPartitionResolveData->ContainerID : FActorContainerID(),
		InResolveBindingParams.WorldPartitionResolveData ? InResolveBindingParams.WorldPartitionResolveData->SourceWorldAssetPath : InResolveBindingParams.StreamedLevelAssetPath
		);

	LocateBoundObjects(ObjectId, ResolveParams, OutObjects);
}

FGuid ULevelSequence::FindBindingFromObject(UObject* InObject, UObject* Context) const
{
	return BindingReferences.FindBindingFromObject(InObject, Context);
}

void ULevelSequence::GatherExpiredObjects(const FMovieSceneObjectCache& InObjectCache, TArray<FGuid>& OutInvalidIDs) const
{
	using namespace UE::UniversalObjectLocator;

	TArrayView<const FMovieSceneBindingReference> References = BindingReferences.GetAllReferences();
	for (int32 Index = 0; Index < References.Num(); ++Index)
	{
		const FMovieSceneBindingReference& Reference = References[Index];
		
		if (Reference.Locator.GetLastFragmentTypeHandle() == FAnimInstanceLocatorFragment::FragmentType)
		{
			for (TWeakObjectPtr<> WeakObject : InObjectCache.IterateBoundObjects(Reference.ID))
			{
				UAnimInstance* AnimInstance = Cast<UAnimInstance>(WeakObject.Get());
				if (!AnimInstance || !AnimInstance->GetOwningComponent() || AnimInstance->GetOwningComponent()->GetAnimInstance() != AnimInstance)
				{
					OutInvalidIDs.Add(Reference.ID);
				}
			}

			// Skip over subsequent matched IDs
			while (Index < References.Num()-1 && References[Index+1].ID == Reference.ID)
			{
				++Index;
			}
		}
	}
}

UMovieScene* ULevelSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* ULevelSequence::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Object))
	{
		if (AnimInstance->GetWorld())
		{
			return AnimInstance->GetOwningComponent();
		}
	}

	return nullptr;
}

bool ULevelSequence::AllowsSpawnableObjects() const
{
#if WITH_EDITOR
	if (!UMovieScene::IsTrackClassAllowed(UMovieSceneSpawnTrack::StaticClass()))
	{
		return false;
	}
#endif
	return true;
}

bool ULevelSequence::CanRebindPossessable(const FMovieScenePossessable& InPossessable) const
{
	return !InPossessable.GetParent().IsValid();
}

void ULevelSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	BindingReferences.RemoveBinding(ObjectId);
}

void ULevelSequence::UnbindObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* InContext)
{
	BindingReferences.RemoveObjects(ObjectId, InObjects, InContext);
}

void ULevelSequence::UnbindInvalidObjects(const FGuid& ObjectId, UObject* InContext)
{
	BindingReferences.RemoveInvalidObjects(ObjectId, InContext);
}

const FMovieSceneBindingReferences* ULevelSequence::GetBindingReferences() const
{
	return &BindingReferences;
}

#if WITH_EDITOR

UBlueprint* ULevelSequence::GetDirectorBlueprint() const
{
	return DirectorBlueprint;
}

FString ULevelSequence::GetDirectorBlueprintName() const
{
	return GetDisplayName().ToString() + "_DirectorBP";
}

void ULevelSequence::SetDirectorBlueprint(UBlueprint* NewDirectorBlueprint)
{
	if (DirectorBlueprint)
	{
		DirectorBlueprint->OnCompiled().RemoveAll(this);
	}

	DirectorBlueprint = NewDirectorBlueprint;

	if (DirectorBlueprint)
	{
		DirectorClass = NewDirectorBlueprint->GeneratedClass.Get();
		DirectorBlueprint->OnCompiled().AddUObject(this, &ULevelSequence::OnDirectorRecompiled);
	}
	else
	{
		DirectorClass = nullptr;
	}

	MarkAsChanged();
}

void ULevelSequence::OnDirectorRecompiled(UBlueprint* InCompiledBlueprint)
{
	ensure(InCompiledBlueprint == DirectorBlueprint);
	DirectorClass = DirectorBlueprint->GeneratedClass.Get();

	MarkAsChanged();
}

FGuid ULevelSequence::FindOrAddBinding(UObject* InObject)
{
	using namespace UE::MovieScene;

	UObject* PlaybackContext = InObject ? InObject->GetWorld() : nullptr;
	if (!InObject || !PlaybackContext)
	{
		return FGuid();
	}

	AActor* Actor = Cast<AActor>(InObject);
	if (Actor && Actor->ActorHasTag("SequencerActor"))
	{
		TOptional<FMovieSceneSpawnableAnnotation> Annotation = FMovieSceneSpawnableAnnotation::Find(Actor);
		if (Annotation.IsSet() && Annotation->OriginatingSequence == this)
		{
			return Annotation->ObjectBindingID;
		}
		
		// If this actor is a spawnable and is not in the same originating sequence, it's likely a spawnable that will be possessed. 
		// SetSpawnableObjectBindingID will need to be called on that possessable.
	}

	UObject* ParentObject = GetParentObject(InObject);
	FGuid    ParentGuid   = ParentObject ? FindOrAddBinding(ParentObject) : FGuid();

	if (ParentObject && !ParentGuid.IsValid())
	{
		UE_LOG(LogLevelSequence, Error, TEXT("Unable to possess object '%s' because it's parent could not be bound."), *InObject->GetName());
		return FGuid();
	}

	// Perform a potentially slow lookup of every possessable binding in the sequence to see if we already have this
	{
		FSharedPlaybackStateCreateParams CreateParams;
		CreateParams.PlaybackContext = PlaybackContext;
		TSharedRef<FSharedPlaybackState> TransientPlaybackState = MakeShared<FSharedPlaybackState>(*this, CreateParams);

		FMovieSceneEvaluationState State;
		TransientPlaybackState->AddCapabilityRaw(&State);
		State.AssignSequence(MovieSceneSequenceID::Root, *this, TransientPlaybackState);

		FGuid ExistingID = State.FindObjectId(*InObject, MovieSceneSequenceID::Root, TransientPlaybackState);
		if (ExistingID.IsValid())
		{
			return ExistingID;
		}
	}

	// We have to possess this object
	if (!CanPossessObject(*InObject, PlaybackContext))
	{
		return FGuid();
	}

	FString NewName = Actor ? Actor->GetActorLabel() : InObject->GetName();

	const FGuid NewGuid = MovieScene->AddPossessable(NewName, InObject->GetClass());

	// Attempt to use the parent as a context if necessary
	UObject* BindingContext = ParentObject && AreParentContextsSignificant() ? ParentObject : PlaybackContext;

	// Set up parent/child guids for possessables within spawnables
	if (ParentGuid.IsValid())
	{
		FMovieScenePossessable* ChildPossessable = MovieScene->FindPossessable(NewGuid);
		if (ensure(ChildPossessable))
		{
			ChildPossessable->SetParent(ParentGuid, MovieScene);
		}

		FMovieSceneSpawnable* ParentSpawnable = MovieScene->FindSpawnable(ParentGuid);
		if (ParentSpawnable)
		{
			ParentSpawnable->AddChildPossessable(NewGuid);
		}
	}

	BindPossessableObject(NewGuid, *InObject, BindingContext);

	return NewGuid;

}

FGuid ULevelSequence::CreatePossessable(UObject* ObjectToPossess)
{
	return FindOrAddBinding(ObjectToPossess);
}

FGuid ULevelSequence::CreateSpawnable(UObject* ObjectToSpawn)
{
	if (!MovieScene || !ObjectToSpawn)
	{
		return FGuid();
	}

	TArray<TSharedRef<IMovieSceneObjectSpawner>> ObjectSpawners;

	// In order to create a spawnable, we have to instantiate all the relevant object spawners for level sequences, and try to create a spawnable from each
	FLevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<FLevelSequenceModule>("LevelSequence");
	LevelSequenceModule.GenerateObjectSpawners(ObjectSpawners);

	// The first object spawner to return a valid result will win
	for (TSharedRef<IMovieSceneObjectSpawner> Spawner : ObjectSpawners)
	{
		TValueOrError<FNewSpawnable, FText> Result = Spawner->CreateNewSpawnableType(*ObjectToSpawn, *MovieScene, nullptr);
		if (Result.IsValid())
		{
			FNewSpawnable& NewSpawnable = Result.GetValue();

			NewSpawnable.Name = MovieSceneHelpers::MakeUniqueSpawnableName(MovieScene, NewSpawnable.Name);			

			FGuid NewGuid = MovieScene->AddSpawnable(NewSpawnable.Name, *NewSpawnable.ObjectTemplate);

			UMovieSceneSpawnTrack* NewSpawnTrack = MovieScene->AddTrack<UMovieSceneSpawnTrack>(NewGuid);
			if (NewSpawnTrack)
			{
				NewSpawnTrack->Modify();

				NewSpawnTrack->AddSection(*NewSpawnTrack->CreateNewSection());
			}
			return NewGuid;
		}
	}

	return FGuid();
}

#endif // WITH_EDITOR

UObject* ULevelSequence::CreateDirectorInstance(TSharedRef<const FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceID SequenceID)
{
	UObject* DirectorOuter = SharedPlaybackState->GetPlaybackContext();
	IMovieScenePlayer* OptionalPlayer = UE::MovieScene::FPlayerIndexPlaybackCapability::GetPlayer(SharedPlaybackState);

#if WITH_EDITOR
	if (!UMovieScene::IsTrackClassAllowed(ULevelSequenceDirector::StaticClass()))
	{
		return nullptr;
	}
#endif

	if (DirectorClass && DirectorOuter && DirectorClass->IsChildOf(ULevelSequenceDirector::StaticClass()))
	{
		FName DirectorName = NAME_None;

#if WITH_EDITOR
		// Give it a pretty name so it shows up in the debug instances drop down nicely
		DirectorName = MakeUniqueObjectName(DirectorOuter, DirectorClass, *(GetFName().ToString() + TEXT("_Director")));
#endif

		ULevelSequencePlayer* LevelSequencePlayer = nullptr;
		if (OptionalPlayer)
		{
			LevelSequencePlayer = Cast<ULevelSequencePlayer>(OptionalPlayer->AsUObject());
		}

		ULevelSequenceDirector* NewDirector = NewObject<ULevelSequenceDirector>(DirectorOuter, DirectorClass, DirectorName, RF_Transient);
		NewDirector->SubSequenceID = SequenceID.GetInternalValue();
		NewDirector->WeakLinker = SharedPlaybackState->GetLinker();
		NewDirector->InstanceID = SharedPlaybackState->GetRootInstanceHandle().InstanceID;
		NewDirector->InstanceSerial = SharedPlaybackState->GetRootInstanceHandle().InstanceSerial;
		NewDirector->Player = LevelSequencePlayer;
		NewDirector->MovieScenePlayerIndex = OptionalPlayer ? OptionalPlayer->GetUniqueIndex() : INDEX_NONE;
		NewDirector->OnCreated();
		return NewDirector;
	}

	return nullptr;
}

void ULevelSequence::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* ULevelSequence::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void ULevelSequence::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* ULevelSequence::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

