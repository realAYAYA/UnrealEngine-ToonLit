// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceExporter.h"
#include "AvaSequenceCopyableBinding.h"
#include "AvaSequencer.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieSceneBindingProxy.h"
#include "AvaSequence.h"
#include "UnrealExporter.h"
#include "UObject/Package.h"

namespace UE::AvaSequencer::Private
{
	class FAvaSequenceExportObjectInnerContext : public FExportObjectInnerContext
	{
	public:
		explicit FAvaSequenceExportObjectInnerContext(UObject* InPlaybackContext)
			: PlaybackContext(InPlaybackContext)
		{
		}
		virtual ~FAvaSequenceExportObjectInnerContext() override = default;

		//~ Begin FExportObjectInnerContext
		virtual bool IsObjectSelected(const UObject* InObject) const override { return true; }
		//~ End FExportObjectInnerContext

		void SetBoundActors(const TArray<AActor*>& InBoundActors)
		{
			BoundActors = InBoundActors;
		}

		TConstArrayView<AActor*> GetBoundActors() const { return BoundActors; }

		UObject* GetPlaybackContext() const { return PlaybackContext; }

	private:
		TObjectPtr<UObject> PlaybackContext;

		TArray<TObjectPtr<AActor>> BoundActors;
	};
}

FAvaSequenceExporter::FAvaSequenceExporter(const TSharedRef<FAvaSequencer>& InAvaSequencer)
	: AvaSequencerWeak(InAvaSequencer)
{
}

void FAvaSequenceExporter::ExportText(FString& InOutCopiedData, TConstArrayView<AActor*> InCopiedActors)
{
	TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	if (!AvaSequencer.IsValid())
	{
		return;
	}

	TMap<UAvaSequence*, TArray<AActor*>> SequenceMap;

	// Gather the Sequences that the Copied Actors are bound to
	for (AActor* const Actor : InCopiedActors)
	{
		TArray<UAvaSequence*> Sequences = AvaSequencer->GetSequencesForObject(Actor);
		for (UAvaSequence* const Sequence : Sequences)
		{
			SequenceMap.FindOrAdd(Sequence).Add(Actor);
		}
	}

	// If there are no sequences, no need to add anything on our side, early return.
	if (SequenceMap.IsEmpty())
	{
		return;
	}

	UObject* const PlaybackContext = AvaSequencer->GetPlaybackContext();
	UE::AvaSequencer::Private::FAvaSequenceExportObjectInnerContext ExportContext(PlaybackContext);

	const TCHAR* const Filetype = TEXT("copy");
	const uint32 PortFlags = PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified;
	const int32 IndentLevel = 0;

	for (const TPair<UAvaSequence*, TArray<AActor*>>& Pair : SequenceMap)
	{
		UAvaSequence* const Sequence = Pair.Key;
		const TArray<AActor*>& BoundActors = Pair.Value;

		if (BoundActors.IsEmpty())
		{
			continue;
		}

		ExportContext.SetBoundActors(BoundActors);

		FStringOutputDevice Ar;
		UExporter::ExportToOutputDevice(&ExportContext, Sequence, nullptr, Ar, Filetype, IndentLevel, PortFlags);
		InOutCopiedData += Ar;
	}
}

UAvaSequenceExporter::UAvaSequenceExporter()
{
	SupportedClass = UAvaSequence::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("copy"));
	FormatDescription.Add(TEXT("Motion Design Sequence"));
}

bool UAvaSequenceExporter::ExportText(const FExportObjectInnerContext* InContext, UObject* InObject, const TCHAR* InType
	, FOutputDevice& Ar, FFeedbackContext* InWarn, uint32 InPortFlags)
{
	UAvaSequence* const Sequence = Cast<UAvaSequence>(InObject);
	if (!InContext || !IsValid(Sequence))
	{
		return false;
	}

	const UE::AvaSequencer::Private::FAvaSequenceExportObjectInnerContext& Context
		= static_cast<const UE::AvaSequencer::Private::FAvaSequenceExportObjectInnerContext&>(*InContext);

	UObject* const PlaybackContext = Context.GetPlaybackContext();

	// Gather guids for the object nodes and any child object nodes
	TArray<FMovieSceneBindingProxy> Bindings;

	auto TryGetBinding = [&Bindings, Sequence](UObject* InBoundObject)->bool
		{
			const FGuid Guid = Sequence->FindGuidFromObject(InBoundObject);

			if (Guid.IsValid())
			{
				Bindings.Add(FMovieSceneBindingProxy(Guid, Sequence));
				return true;
			}

			return false;
		};

	constexpr bool bIncludeNestedObjects  = true;
	constexpr EObjectFlags ExclusionFlags = RF_Transient | RF_TextExportTransient | RF_BeginDestroyed | RF_FinishDestroyed;

	for (AActor* const Actor : Context.GetBoundActors())
	{
		if (TryGetBinding(Actor))
		{
			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(Actor, Subobjects, bIncludeNestedObjects, ExclusionFlags);

			for (UObject* const Subobject : Subobjects)
			{
				TryGetBinding(Subobject);
			}
		}
	}

	if (Bindings.IsEmpty())
	{
		return false;
	}

	Ar.Logf(TEXT("%sBegin Sequence Label=%s\r\n")
		, FCString::Spc(TextIndent)
		, *Sequence->GetLabel().ToString());

	UObject* const CopyableBindingOuter = GetTransientPackage();

	TArray<UAvaSequenceCopyableBinding*> ObjectsToExport;
	ObjectsToExport.Reserve(Bindings.Num());

	for (const FMovieSceneBindingProxy& ObjectBinding : Bindings)
	{
		UMovieScene* const MovieScene = ObjectBinding.GetMovieScene();
		if (!IsValid(MovieScene))
		{
			continue;
		}

		UAvaSequenceCopyableBinding* const CopyableBinding = NewObject<UAvaSequenceCopyableBinding>(CopyableBindingOuter, NAME_None, RF_Transient);

		ObjectsToExport.Add(CopyableBinding);

		if (FMovieScenePossessable* const Possessable = MovieScene->FindPossessable(ObjectBinding.BindingID))
		{
			UObject* ResolutionContext = FAvaSequencer::FindResolutionContext(*Sequence
				, *MovieScene
				, Possessable->GetParent()
				, PlaybackContext);

			CopyableBinding->Possessable = *Possessable;

			TArray<UObject*, TInlineAllocator<1>> BoundObjects;
			Sequence->LocateBoundObjects(CopyableBinding->Possessable.GetGuid(), UE::UniversalObjectLocator::FResolveParams(ResolutionContext), BoundObjects);

			// Store the names of the bound objects so that they can be found on paste
			for (UObject* const BoundObject : BoundObjects)
			{
				if (!BoundObject)
				{
					continue;
				}

				if (AActor* const Actor = Cast<AActor>(BoundObject))
				{
					CopyableBinding->BoundActorNames.Add(Actor->GetFName());
				}
				else if (ResolutionContext != PlaybackContext)
				{
					CopyableBinding->BoundObjectPaths.Add(BoundObject->GetPathName(ResolutionContext));
				}
			}
		}
		else
		{
			FMovieSceneSpawnable* const Spawnable = MovieScene->FindSpawnable(ObjectBinding.BindingID);
			if (Spawnable)
			{
				CopyableBinding->Spawnable = *Spawnable;

				// We manually serialize the spawnable object template so that it's not a reference to a privately owned object. Spawnables all have unique copies of their template objects anyways.
				// Object Templates are re-created on paste (based on these templates) with the correct ownership set up.
				CopyableBinding->SpawnableObjectTemplate = Spawnable->GetObjectTemplate();
			}
		}

		if (const FMovieSceneBinding* const Binding = MovieScene->FindBinding(ObjectBinding.BindingID))
		{
			CopyableBinding->Binding = *Binding;
			for (UMovieSceneTrack* const Track : Binding->GetTracks())
			{
				// Tracks suffer from the same issues as Spawnable's Object Templates (reference to a privately owned object). We'll manually serialize the tracks to copy them,
				// and then restore them on paste.
				UMovieSceneTrack* const DuplicatedTrack = Cast<UMovieSceneTrack>(StaticDuplicateObject(Track, CopyableBinding));
				CopyableBinding->Tracks.Add(DuplicatedTrack);
			}
		}
	}

	UAvaSequenceExporter::ExportBindings(ObjectsToExport, TEXT("copy"), TextIndent + 3, Ar, InWarn);

	Ar.Logf(TEXT("%sEnd Sequence\r\n"), FCString::Spc(TextIndent));

	return true;
}

void UAvaSequenceExporter::ExportBindings(const TArray<UAvaSequenceCopyableBinding*>& InObjectsToExport, const TCHAR* InType
	, int32 InTextIndent
	, FOutputDevice& Ar
	, FFeedbackContext* InWarn)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(static_cast<EObjectMark>(EObjectMark::OBJECTMARK_TagExp | EObjectMark::OBJECTMARK_TagImp));

	UObject* const Outer = GetTransientPackage();

	const uint32 PortFlags = PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited;

	FExportObjectInnerContext EmptyContext;

	for (UAvaSequenceCopyableBinding* const ObjectToExport : InObjectsToExport)
	{
		// We can't use TextExportTransient on USTRUCTS (which our object contains) so we're going to manually null out some references before serializing them. These references are
		// serialized manually into the archive, as the auto-serialization will only store a reference (to a privately owned object) which creates issues on deserialization. Attempting 
		// to deserialize these private objects throws a superflous error in the console that makes it look like things went wrong when they're actually OK and expected.
		TArray<UMovieSceneTrack*> OldTracks = ObjectToExport->Binding.StealTracks(nullptr);

		UObject* const OldSpawnableTemplate = ObjectToExport->Spawnable.GetObjectTemplate();

		ObjectToExport->Spawnable.SetObjectTemplate(nullptr);

		UExporter::ExportToOutputDevice(&EmptyContext, ObjectToExport, nullptr, Ar, InType
			, InTextIndent, PortFlags, false, Outer);

		// Restore the references (as we don't want to modify the original in the event of a copy operation!)
		ObjectToExport->Binding.SetTracks(MoveTemp(OldTracks), nullptr);
		ObjectToExport->Spawnable.SetObjectTemplate(OldSpawnableTemplate);

		// We manually export the object template for the same private-ownership reason as above. Templates need to be re-created anyways as each Spawnable contains its own copy of the template.
		if (ObjectToExport->SpawnableObjectTemplate)
		{
			UExporter::ExportToOutputDevice(&EmptyContext, ObjectToExport->SpawnableObjectTemplate, nullptr, Ar, InType
				, InTextIndent, PortFlags);
		}
	}
}
