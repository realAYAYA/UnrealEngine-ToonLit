// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceImporter.h"
#include "AvaSequenceCopyableBinding.h"
#include "AvaSequence.h"
#include "AvaSequencer.h"
#include "EngineUtils.h"
#include "Factories.h"
#include "IAvaSequenceProvider.h"
#include "ISequencer.h"
#include "MovieScenePossessable.h"
#include "SequencerUtilities.h"
#include "UObject/Package.h"

FAvaSequenceImporter::FAvaSequenceImporter(const TSharedRef<FAvaSequencer>& InAvaSequencer)
	: AvaSequencerWeak(InAvaSequencer)
{
}

void FAvaSequenceImporter::ImportText(FStringView InPastedData, const TMap<FName, AActor*>& InPastedActors)
{
	TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	if (!AvaSequencer.IsValid())
	{
		return;
	}

	IAvaSequencerProvider& Provider  = AvaSequencer->GetProvider();
	TSharedRef<ISequencer> Sequencer = AvaSequencer->GetSequencer();

	UAvaSequence* const OriginallyViewedSequence = AvaSequencer->GetViewedSequence();

	const TCHAR* Buffer = InPastedData.GetData();

	FString StringLine;
	while (FParse::Line(&Buffer, StringLine))
	{
		const TCHAR* Str = *StringLine;
		if (!ParseCommand(&Str, TEXT("Begin")))
		{
			continue;
		}

		FName SequenceLabel;
		FParse::Value(Str, TEXT("Label="), SequenceLabel);

		UAvaSequence* const SequenceToUse = GetOrCreateSequence(*AvaSequencer, SequenceLabel);
		if (!ensure(IsValid(SequenceToUse)))
		{
			continue;
		}

		UMovieScene* const MovieScene = SequenceToUse->GetMovieScene();
		if (!ensure(IsValid(MovieScene)))
		{
			continue;
		}

		UsedSequences.Add(SequenceToUse);

		AvaSequencer->SetViewedSequence(SequenceToUse);

		FString BindingsString, BindingsStringLine;
		while (!ParseCommand(&Buffer, TEXT("End")) && FParse::Line(&Buffer, BindingsStringLine))
		{
			BindingsString += *BindingsStringLine;
			BindingsString += TEXT("\r\n");
		}

		const TArray<UAvaSequenceCopyableBinding*> ImportedBindings = ImportBindings(*Sequencer, BindingsString);
		ProcessImportedBindings(ImportedBindings, InPastedActors);
	}

	AvaSequencer->SetViewedSequence(OriginallyViewedSequence);
}

bool FAvaSequenceImporter::ParseCommand(const TCHAR** InStream, const TCHAR* InToken)
{
	static const TCHAR* const SequenceToken = TEXT("Sequence");
	const TCHAR* Original = *InStream;

	if (FParse::Command(InStream, InToken) && FParse::Command(InStream, SequenceToken))
	{
		return true;
	}

	*InStream = Original;

	return false;
}

void FAvaSequenceImporter::ResetCopiedTracksFlags(UMovieSceneTrack* InTrack)
{
	if (!IsValid(InTrack))
	{
		return;
	}

	InTrack->ClearFlags(RF_Transient);

	for (UMovieSceneSection* Section : InTrack->GetAllSections())
	{
		Section->ClearFlags(RF_Transient);
		Section->PostPaste();
	}
}

UAvaSequence* FAvaSequenceImporter::GetOrCreateSequence(FAvaSequencer& InAvaSequencer, FName InSequenceLabel)
{
	IAvaSequenceProvider* const SequenceProvider = InAvaSequencer.GetProvider().GetSequenceProvider();
	if (!SequenceProvider)
	{
		return nullptr;
	}

	// Find an unused sequence that matches the given label
	const TObjectPtr<UAvaSequence>* const  FoundSequence = SequenceProvider->GetSequences().FindByPredicate(
		[InSequenceLabel, this](const UAvaSequence* const InSequence)
		{
			return IsValid(InSequence)
				&& InSequence->GetLabel() == InSequenceLabel
				&& !UsedSequences.Contains(InSequence);
		});

	if (FoundSequence)
	{
		return *FoundSequence;
	}

	UAvaSequence* const NewSequence = InAvaSequencer.CreateSequence();
	NewSequence->SetLabel(InSequenceLabel);
	SequenceProvider->AddSequence(NewSequence);

	return NewSequence;
}

TArray<UAvaSequenceCopyableBinding*> FAvaSequenceImporter::ImportBindings(ISequencer& InSequencerRef, const FString& InBindingsString)
{
	class FAvaObjectBindingTextFactory : public FCustomizableTextObjectFactory
	{
	public:
		FAvaObjectBindingTextFactory(ISequencer& InSequencer) : FCustomizableTextObjectFactory(GWarn), Sequencer(InSequencer)
		{
		}

		virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
		{
			if (InObjectClass->IsChildOf<UAvaSequenceCopyableBinding>())
			{
				return true;
			}
			return Sequencer.GetSpawnRegister().CanSpawnObject(InObjectClass);
		}

		virtual void ProcessConstructedObject(UObject* InObject) override
		{
			check(InObject);

			if (InObject->IsA<UAvaSequenceCopyableBinding>())
			{
				UAvaSequenceCopyableBinding* CopyableBinding = Cast<UAvaSequenceCopyableBinding>(InObject);
				ImportedBindings.Add(CopyableBinding);
			}
			else
			{
				NewSpawnableObjectTemplates.Add(InObject);
			}
		}

		TArray<UAvaSequenceCopyableBinding*> ImportedBindings;
		TArray<UObject*> NewSpawnableObjectTemplates;

	private:
		ISequencer& Sequencer;
	};

	UPackage* const TempPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Editor/AvalancheSequencer/Transient"), RF_Transient);
	TempPackage->AddToRoot();

	// Turn the text buffer into objects
	FAvaObjectBindingTextFactory Factory(InSequencerRef);
	Factory.ProcessBuffer(TempPackage, RF_Transactional, InBindingsString);

	// We had to explicitly serialize object templates due to them being a reference to a privately owned object.
	// We now deserialize these object template copies
	// and match them up with their MovieSceneCopyableBinding again.
	int32 SpawnableObjectTemplateIndex = 0;
	for (UAvaSequenceCopyableBinding* const ImportedObject : Factory.ImportedBindings)
	{
		if (ImportedObject->Spawnable.GetGuid().IsValid() && SpawnableObjectTemplateIndex < Factory.NewSpawnableObjectTemplates.Num())
		{
			// This Spawnable Object Template is owned by our transient package, so you'll need to change the owner if you want to keep it later.
			ImportedObject->SpawnableObjectTemplate = Factory.NewSpawnableObjectTemplates[SpawnableObjectTemplateIndex++];
		}
	}

	// Remove the temp package from the root now that it has served its purpose
	TempPackage->RemoveFromRoot();

	return Factory.ImportedBindings;
}

void FAvaSequenceImporter::ProcessImportedBindings(const TArray<UAvaSequenceCopyableBinding*>& InImportedBindings
	, const TMap<FName, AActor*>& InPastedActors)
{
	if (InImportedBindings.IsEmpty())
	{
		return;
	}

	TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	if (!ensureAlways(AvaSequencer.IsValid()))
	{
		return;
	}

	TSharedRef<ISequencer> Sequencer = AvaSequencer->GetSequencer();
	UAvaSequence* const Sequence     = AvaSequencer->GetViewedSequence();
	UObject* const PlaybackContext   = Sequencer->GetPlaybackContext();

	if (!ensureAlways(Sequence && PlaybackContext))
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!ensureAlways(MovieScene))
	{
		return;
	}

	struct FImportedPossessable
	{
		TArray<FString> BoundObjectPaths;
	};
	TMap<FGuid, FImportedPossessable> ImportedPossessableMap;
	TMap<FGuid, FGuid> OldToNewGuidMap;
	TArray<FMovieSceneBinding> PastedBindings;

	ImportedPossessableMap.Reserve(InImportedBindings.Num());
	OldToNewGuidMap.Reserve(InImportedBindings.Num());
	PastedBindings.Reserve(InImportedBindings.Num());

	FImportContext ImportContext { Sequencer, *Sequence, *MovieScene, Sequencer->GetFocusedTemplateID(), InPastedActors, nullptr };

	for (UAvaSequenceCopyableBinding* const CopyableBinding : InImportedBindings)
	{
		// Clear transient flags on the imported tracks
		for (UMovieSceneTrack* const CopiedTrack : CopyableBinding->Tracks)
		{
			FAvaSequenceImporter::ResetCopiedTracksFlags(CopiedTrack);
		}

		ImportContext.CopyableBinding = CopyableBinding;

		// Possessable Imported Binding
		if (CopyableBinding->Possessable.GetGuid().IsValid())
		{
			FMovieSceneBinding NewBinding = BindPossessable(ImportContext);

			FImportedPossessable ImportedPossessable;
			ImportedPossessable.BoundObjectPaths = CopyableBinding->BoundObjectPaths;
			ImportedPossessableMap.Add(NewBinding.GetObjectGuid(), MoveTemp(ImportedPossessable));

			OldToNewGuidMap.Add(CopyableBinding->Possessable.GetGuid(), NewBinding.GetObjectGuid());
			PastedBindings.Add(MoveTemp(NewBinding));
		}
		// Spawnable Imported Binding
		else if (CopyableBinding->Spawnable.GetGuid().IsValid())
		{
			FMovieSceneBinding NewBinding = BindSpawnable(ImportContext);
			OldToNewGuidMap.Add(CopyableBinding->Spawnable.GetGuid(), NewBinding.GetObjectGuid());
			PastedBindings.Add(MoveTemp(NewBinding));
		}
	}

	// Fix up possessables' parent guids
	for (const TPair<FGuid, FImportedPossessable>& Pair : ImportedPossessableMap)
	{
		FMovieScenePossessable* const Possessable = MovieScene->FindPossessable(Pair.Key);
		if (!Possessable)
		{
			continue;
		}

		if (FGuid* const NewParentGuid = OldToNewGuidMap.Find(Possessable->GetParent()))
		{
			Possessable->SetParent(*NewParentGuid, MovieScene);
		}
	}

	// Fix possessables that have bound object paths
	for (const TPair<FGuid, FImportedPossessable>& Pair : ImportedPossessableMap)
	{
		const FGuid& PossessableGuid = Pair.Key;
		const FImportedPossessable& ImportedPossessable = Pair.Value;

		if (ImportedPossessable.BoundObjectPaths.IsEmpty())
		{
			continue;
		}

		FMovieScenePossessable* const Possessable = MovieScene->FindPossessable(PossessableGuid);
		if (!Possessable)
		{
			continue;
		}

		auto FindObjectsFunc = [&ImportedPossessableMap, Sequence](const FGuid& InGuid, UObject* InContextChecked)
			{
				TArray<UObject*, TInlineAllocator<1>> FoundObjects;

				// Attempt to use the bound object paths that were copied over through the copyable binding
				// to have an easier time trying to resolve the object
				if (const FImportedPossessable* const FoundEntry = ImportedPossessableMap.Find(InGuid))
				{
					for (const FString& BoundObjectPath : FoundEntry->BoundObjectPaths)
					{
						if (UObject* FoundObject = FindObject<UObject>(InContextChecked, *BoundObjectPath))
						{
							FoundObjects.Add(FoundObject);
						}
					}
				}

				// If nothing was found, use default method to locate the object
				if (FoundObjects.IsEmpty() || !FoundObjects[0])
				{
					Sequence->LocateBoundObjects(InGuid, UE::UniversalObjectLocator::FResolveParams(InContextChecked), FoundObjects);
				}

				return FoundObjects;
			};

		UObject* const ResolutionContext = FAvaSequencer::FindResolutionContext(*Sequence
			, *MovieScene
			, Possessable->GetParent()
			, PlaybackContext
			, FindObjectsFunc);

		TArray<UObject*> ObjectsToBind;
		for (const FString& BoundObjectPath : ImportedPossessable.BoundObjectPaths)
		{
			if (UObject* FoundObject = FindObject<UObject>(ResolutionContext, *BoundObjectPath))
			{
				ObjectsToBind.Add(FoundObject);
			}
		}

		AddObjectsToBinding(Sequencer
			, ObjectsToBind
			, FMovieSceneBindingProxy(PossessableGuid, Sequence)
			, ResolutionContext);
	}

	Sequencer->RestorePreAnimatedState();

	Sequencer->OnMovieSceneBindingsPasted().Broadcast(PastedBindings);

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

	// Refresh all immediately so that spawned actors will be generated immediately
	Sequencer->ForceEvaluate();

	// Remap bindings in sections (ie. attach tracks)
	for (const TPair<FGuid, FGuid>& Pair : OldToNewGuidMap)
	{
		FSequencerUtilities::UpdateBindingIDs(Sequencer, Pair.Key, Pair.Value);
	}
}

FMovieSceneBinding FAvaSequenceImporter::BindPossessable(const FImportContext& InContext)
{
	const FGuid NewGuid = FGuid::NewGuid();

	FMovieSceneBinding NewBinding(NewGuid, InContext.CopyableBinding->Binding.GetName(), InContext.CopyableBinding->Tracks);
	{
		FMovieScenePossessable NewPossessable = InContext.CopyableBinding->Possessable;
		NewPossessable.SetGuid(NewGuid);
		InContext.MovieScene.AddPossessable(NewPossessable, NewBinding);
	}

	// Find the actors that this pasted binding should bind to
	TArray<UObject*> ActorsToBind;

	// Only attempt to bind actor names here, as this is the only thing that can be rebound at this point
	// Subobjects first need their parents (actors) to resolve correctly, so wait on second pass
	for (const FName& BoundActorName : InContext.CopyableBinding->BoundActorNames)
	{
		if (AActor* const * const FoundActor = InContext.PastedActors.Find(BoundActorName))
		{
			AActor* const Actor = *FoundActor;
			if (!Actor)
			{
				continue;
			}

			// Don't bind if this actor is already bound
			if (InContext.Sequencer->FindObjectId(*Actor, InContext.SequenceIDRef).IsValid() || ActorsToBind.Contains(Actor))
			{
				continue;
			}

			ActorsToBind.Add(Actor);
		}
	}

	// Bind the actors
	if (!ActorsToBind.IsEmpty())
	{
		AddObjectsToBinding(InContext.Sequencer
			, ActorsToBind
			, FMovieSceneBindingProxy(NewGuid, &InContext.Sequence)
			, InContext.Sequencer->GetPlaybackContext());
	}

	return NewBinding;
}

FMovieSceneBinding FAvaSequenceImporter::BindSpawnable(const FImportContext& InContext)
{
	// We need to let the sequence create the spawnable so that it has everything set up properly internally.
	// This is required to get spawnables with the correct references to object templates, object templates with
	// correct owners, etc. However, making a new spawnable also creates the binding for us - this is a problem
	// because we need to use our binding (which has tracks associated with it). To solve this, we let it create
	// an object template based off of our (transient package owned) template, then find the newly created binding
	// and update it.

	FGuid NewGuid;
	if (InContext.CopyableBinding->SpawnableObjectTemplate)
	{
		NewGuid = InContext.Sequencer->MakeNewSpawnable(*InContext.CopyableBinding->SpawnableObjectTemplate
			, nullptr
			, false);
	}
	else
	{
		FMovieSceneSpawnable NewSpawnable{};
		NewSpawnable.SetGuid(FGuid::NewGuid());
		NewSpawnable.SetName(InContext.CopyableBinding->Spawnable.GetName());

		InContext.MovieScene.AddSpawnable(NewSpawnable, FMovieSceneBinding(NewSpawnable.GetGuid(), NewSpawnable.GetName()));

		NewGuid = NewSpawnable.GetGuid();
	}

	FMovieSceneBinding NewBinding(NewGuid, InContext.CopyableBinding->Binding.GetName(), InContext.CopyableBinding->Tracks);

	FMovieSceneSpawnable* const Spawnable = InContext.MovieScene.FindSpawnable(NewGuid);

	// Copy the name of the original spawnable too.
	Spawnable->SetName(InContext.CopyableBinding->Spawnable.GetName());

	// Clear the transient flags on the copyable binding before assigning to the new spawnable
	for (UMovieSceneTrack* const Track : NewBinding.GetTracks())
	{
		FAvaSequenceImporter::ResetCopiedTracksFlags(Track);
	}

	// Replace the auto-generated binding with our deserialized bindings (which has our tracks)
	InContext.MovieScene.ReplaceBinding(NewGuid, NewBinding);

	return NewBinding;
}

void FAvaSequenceImporter::AddObjectsToBinding(const TSharedRef<ISequencer>& InSequencer
	, const TArray<UObject*>& InObjectsToAdd
	, const FMovieSceneBindingProxy& InObjectBinding
	, UObject* InResolutionContext)
{
	UAvaSequence* Sequence = CastChecked<UAvaSequence>(InObjectBinding.Sequence);
	if (!Sequence)
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene || InObjectsToAdd.IsEmpty())
	{
		return;
	}

	UClass* ObjectClass = nullptr;
	int32 ValidObjectCount = 0;

	FGuid Guid = InObjectBinding.BindingID;

	TArrayView<TWeakObjectPtr<>> WeakObjectsInSequence = Sequence->FindObjectsFromGuid(Guid);
	for (TWeakObjectPtr<> ObjectWeak : WeakObjectsInSequence)
	{
		if (UObject* Object = ObjectWeak.Get())
		{
			ObjectClass = Object->GetClass();
			++ValidObjectCount;
		}
	}

	Sequence->Modify();
	MovieScene->Modify();

	TArray<UObject*> AddedObjects;
	AddedObjects.Reserve(InObjectsToAdd.Num());

	for (UObject* ObjectToAdd : InObjectsToAdd)
	{
		// Skip invalid objects or objects already in the sequence
		if (!ObjectToAdd || WeakObjectsInSequence.Contains(ObjectToAdd))
		{
			continue;
		}

		// Skip if the object has no common class with the objects already in the binding
		if (ObjectClass && !UClass::FindCommonBase(ObjectToAdd->GetClass(), ObjectClass))
		{
			continue;
		}

		// if no objects are in the binding, set the class to this object's
		if (!ObjectClass)
		{
			ObjectClass = ObjectToAdd->GetClass();
		}

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Guid);
		if (!ensureAlways(Possessable))
		{
			continue;
		}

		ObjectToAdd->Modify();
		if (!Possessable->BindSpawnableObject(InSequencer->GetFocusedTemplateID(), ObjectToAdd, InSequencer->GetSharedPlaybackState()))
		{
			Sequence->BindPossessableObject(Guid, *ObjectToAdd, InResolutionContext);
		}
		AddedObjects.Add(ObjectToAdd);
	}

	// Update Labels
	if (ValidObjectCount + AddedObjects.Num() > 0)
	{
		FMovieScenePossessable* const Possessable = MovieScene->FindPossessable(Guid);
		if (Possessable && ObjectClass)
		{
			// If there are multiple objects within the same possessable, name possessable as "ClassName (Count)"
			if (ValidObjectCount + AddedObjects.Num() > 1)
			{
				Possessable->SetName(FString::Printf(TEXT("%s (%d)")
					, *ObjectClass->GetName()
					, ValidObjectCount + AddedObjects.Num()));
			}
			else if (!AddedObjects.IsEmpty())
			{
				Possessable->SetName(FAvaSequencer::GetObjectName(AddedObjects[0]));
			}
			Possessable->SetPossessedObjectClass(ObjectClass);
		}
	}
}

