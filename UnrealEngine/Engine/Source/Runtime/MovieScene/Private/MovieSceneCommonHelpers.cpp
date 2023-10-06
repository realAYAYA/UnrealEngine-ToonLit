// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCommonHelpers.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "KeyParams.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSpawnable.h"
#include "Sections/MovieSceneSubSection.h"
#include "Algo/Sort.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "MovieSceneTrack.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

bool MovieSceneHelpers::IsSectionKeyable(const UMovieSceneSection* Section)
{
	if (!Section)
	{
		return false;
	}

	UMovieSceneTrack* Track = Section->GetTypedOuter<UMovieSceneTrack>();
	if (!Track)
	{
		return false;
	}

	return !Track->IsRowEvalDisabled(Section->GetRowIndex()) && !Track->IsEvalDisabled() && Section->IsActive();
}

UMovieSceneSection* MovieSceneHelpers::FindSectionAtTime( TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time, int32 RowIndex )
{
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		//@todo sequencer: There can be multiple sections overlapping in time. Returning instantly does not account for that.
		if( (RowIndex == INDEX_NONE || Section->GetRowIndex() == RowIndex) &&
				Section->IsTimeWithinSection( Time ) && IsSectionKeyable(Section) )
		{
			return Section;
		}
	}

	return nullptr;
}

UMovieSceneSection* MovieSceneHelpers::FindNearestSectionAtTime( TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time, int32 RowIndex )
{
	TArray<UMovieSceneSection*> OverlappingSections, NonOverlappingSections;
	for (UMovieSceneSection* Section : Sections)
	{
		if ((RowIndex == INDEX_NONE || Section->GetRowIndex() == RowIndex) &&
				IsSectionKeyable(Section))
		{
			if (Section->GetRange().Contains(Time))
			{
				OverlappingSections.Add(Section);
			}
			else
			{
				NonOverlappingSections.Add(Section);
			}
		}
	}

	if (OverlappingSections.Num())
	{
		Algo::Sort(OverlappingSections, SortOverlappingSections);
		return OverlappingSections[0];
	}

	if (NonOverlappingSections.Num())
	{
		Algo::SortBy(NonOverlappingSections, [](const UMovieSceneSection* A) { return A->GetRange().GetUpperBound(); }, SortUpperBounds);

		const int32 PreviousIndex = Algo::UpperBoundBy(NonOverlappingSections, TRangeBound<FFrameNumber>(Time), [](const UMovieSceneSection* A){ return A ? A->GetRange().GetUpperBound() : FFrameNumber(0); }, SortUpperBounds)-1;
		if (NonOverlappingSections.IsValidIndex(PreviousIndex))
		{
			return NonOverlappingSections[PreviousIndex];
		}
		else
		{
			Algo::SortBy(NonOverlappingSections, [](const UMovieSceneSection* A) { return A ? A->GetRange().GetLowerBound() : FFrameNumber(0); }, SortLowerBounds);
			return NonOverlappingSections[0];
		}
	}

	return nullptr;
}

UMovieSceneSection* MovieSceneHelpers::FindNextSection(TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time)
{
	FFrameNumber MinTime = TNumericLimits<FFrameNumber>::Max();

	TMap<FFrameNumber, int32> StartTimeMap;
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = Sections[SectionIndex];

		if (ShotSection && ShotSection->HasStartFrame() && !ShotSection->GetRange().Contains(Time))
		{
			StartTimeMap.Add(ShotSection->GetInclusiveStartFrame(), SectionIndex);
		}
	}

	StartTimeMap.KeySort(TLess<FFrameNumber>());

	int32 NextSectionIndex = -1;
	for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
	{
		FFrameNumber StartTime = StartTimeIt->Key;
		if (StartTime > Time)
		{
			FFrameNumber DiffTime = FMath::Abs(StartTime - Time);
			if (DiffTime < MinTime)
			{
				MinTime = DiffTime;
				NextSectionIndex = StartTimeIt->Value;
			}
		}
	}

	if (NextSectionIndex == -1)
	{
		return nullptr;
	}

	return Sections[NextSectionIndex];
}

UMovieSceneSection* MovieSceneHelpers::FindPreviousSection(TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time)
{
	FFrameNumber MinTime = TNumericLimits<FFrameNumber>::Max();

	TMap<FFrameNumber, int32> StartTimeMap;
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = Sections[SectionIndex];

		if (ShotSection && ShotSection->HasStartFrame() && !ShotSection->GetRange().Contains(Time))
		{
			StartTimeMap.Add(ShotSection->GetInclusiveStartFrame(), SectionIndex);
		}
	}

	StartTimeMap.KeySort(TLess<FFrameNumber>());

	int32 PreviousSectionIndex = -1;
	for (auto StartTimeIt = StartTimeMap.CreateIterator(); StartTimeIt; ++StartTimeIt)
	{
		FFrameNumber StartTime = StartTimeIt->Key;
		if (Time >= StartTime)
		{
			FFrameNumber DiffTime = FMath::Abs(StartTime - Time);
			if (DiffTime < MinTime)
			{
				MinTime = DiffTime;
				PreviousSectionIndex = StartTimeIt->Value;
			}
		}
	}

	if (PreviousSectionIndex == -1)
	{
		return nullptr;
	}

	return Sections[PreviousSectionIndex];
}

bool MovieSceneHelpers::SortOverlappingSections(const UMovieSceneSection* A, const UMovieSceneSection* B)
{
	return A->GetRowIndex() == B->GetRowIndex()
		? A->GetOverlapPriority() < B->GetOverlapPriority()
		: A->GetRowIndex() < B->GetRowIndex();
}

void MovieSceneHelpers::SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections)
{
	Algo::SortBy(Sections, [](const UMovieSceneSection* A) { return A ? A->GetRange().GetLowerBound() : FFrameNumber(0); }, SortLowerBounds);
}

bool MovieSceneHelpers::FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete, bool bCleanUp)
{
	// Find the previous section and extend it to take the place of the section being deleted
	int32 SectionIndex = INDEX_NONE;

	const TRange<FFrameNumber> SectionRange = Section.GetRange();

	if (SectionRange.HasLowerBound() && SectionRange.HasUpperBound() && SectionRange.GetLowerBoundValue() >= SectionRange.GetUpperBoundValue())
	{
		return false;
	}

	if (Sections.Find(&Section, SectionIndex))
	{
		int32 PrevSectionIndex = SectionIndex - 1;
		if( Sections.IsValidIndex( PrevSectionIndex ) )
		{
			// Extend the previous section
			UMovieSceneSection* PrevSection = Sections[PrevSectionIndex];

			PrevSection->Modify();

			if (bDelete)
			{
				TRangeBound<FFrameNumber> NewEndFrame = SectionRange.GetUpperBound();

				if (!PrevSection->HasStartFrame() || NewEndFrame.GetValue() > PrevSection->GetInclusiveStartFrame())
				{
					PrevSection->SetEndFrame(NewEndFrame);
				}
			}
			else
			{
				TRangeBound<FFrameNumber> NewEndFrame = TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.GetLowerBound());

				if (!PrevSection->HasStartFrame() || NewEndFrame.GetValue() > PrevSection->GetInclusiveStartFrame())
				{
					PrevSection->SetEndFrame(NewEndFrame);
				}
			}
		}

		if( !bDelete )
		{
			int32 NextSectionIndex = SectionIndex + 1;
			if(Sections.IsValidIndex(NextSectionIndex))
			{
				// Shift the next CameraCut's start time so that it starts when the new CameraCut ends
				UMovieSceneSection* NextSection = Sections[NextSectionIndex];

				NextSection->Modify();

				TRangeBound<FFrameNumber> NewStartFrame = TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.GetUpperBound());

				if (!NextSection->HasEndFrame() || NewStartFrame.GetValue() < NextSection->GetExclusiveEndFrame())
				{
					NextSection->SetStartFrame(NewStartFrame);
				}
			}
		}
	}

	bool bCleanUpDone = false;
	if (bCleanUp)
	{
		const TArray<UMovieSceneSection*> OverlappedSections = Sections.FilterByPredicate([&Section, SectionRange](const UMovieSceneSection* Cur)
				{
					if (Cur != &Section)
					{
						const TRange<FFrameNumber> CurRange = Cur->GetRange();
						return SectionRange.Contains(CurRange);
					}
					return false;
				});
		for (UMovieSceneSection* OverlappedSection : OverlappedSections)
		{
			Sections.Remove(OverlappedSection);
		}
		bCleanUpDone = (OverlappedSections.Num() > 0);
	}

	SortConsecutiveSections(Sections);

	return bCleanUpDone;
}

bool MovieSceneHelpers::FixupConsecutiveBlendingSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete, bool bCleanUp)
{
	int32 SectionIndex = INDEX_NONE;

	TRange<FFrameNumber> SectionRange = Section.GetRange();

	if (SectionRange.HasLowerBound() && SectionRange.HasUpperBound() && SectionRange.GetLowerBoundValue() >= SectionRange.GetUpperBoundValue())
	{
		return false;
	}

	if (Sections.Find(&Section, SectionIndex))
	{
		// Find the previous section and extend it to take the place of the section being deleted
		int32 PrevSectionIndex = SectionIndex - 1;
		if (Sections.IsValidIndex(PrevSectionIndex))
		{
			UMovieSceneSection* PrevSection = Sections[PrevSectionIndex];

			PrevSection->Modify();

			if (bDelete)
			{
				TRangeBound<FFrameNumber> NewEndFrame = SectionRange.GetUpperBound();

				if (!PrevSection->HasStartFrame() || NewEndFrame.GetValue() > PrevSection->GetInclusiveStartFrame())
				{
					// The current section was deleted... extend the previous section to fill the gap.
					PrevSection->SetEndFrame(NewEndFrame);
				}
			}
			else
			{
				const FFrameNumber GapOrOverlap = SectionRange.GetLowerBoundValue() - PrevSection->GetRange().GetUpperBoundValue();
				if (GapOrOverlap > 0)
				{
					// If we made a gap: adjust the previous section's end time so that it ends wherever the current section's ease-in ends.
					TRangeBound<FFrameNumber> NewEndFrame = TRangeBound<FFrameNumber>::Exclusive(SectionRange.GetLowerBoundValue() + Section.Easing.GetEaseInDuration());

					if (!PrevSection->HasStartFrame() || NewEndFrame.GetValue() > PrevSection->GetInclusiveStartFrame())
					{
						// It's a gap!
						PrevSection->SetEndFrame(NewEndFrame);
					}
				}
				else
				{
					// If we created an overlap: calls to UMovieSceneTrack::UpdateEasing will set the easing curves correctly based on overlaps.
					// However, we need to fixup some easing where overlaps don't occur, such as the very first ease-in and the very last ease-out.
					// Don't overlap so far that our ease-out, or the previous section's ease-in, get overlapped. Clamp these easing durations instead.
					if (Section.HasEndFrame() && PrevSection->HasEndFrame())
					{
						const FFrameNumber MaxEaseOutDuration = Section.GetExclusiveEndFrame() - PrevSection->GetExclusiveEndFrame();
						Section.Easing.AutoEaseOutDuration = FMath::Min(FMath::Max(0, MaxEaseOutDuration.Value), Section.Easing.AutoEaseOutDuration);
						Section.Easing.ManualEaseOutDuration = FMath::Min(FMath::Max(0, MaxEaseOutDuration.Value), Section.Easing.ManualEaseOutDuration);
					}
					if (Section.HasStartFrame() && PrevSection->HasStartFrame())
					{
						const FFrameNumber MaxPrevSectionEaseInDuration = Section.GetInclusiveStartFrame() - PrevSection->GetInclusiveStartFrame();
						PrevSection->Easing.AutoEaseInDuration = FMath::Min(FMath::Max(0, MaxPrevSectionEaseInDuration.Value), PrevSection->Easing.AutoEaseInDuration);
						PrevSection->Easing.ManualEaseInDuration = FMath::Min(FMath::Max(0, MaxPrevSectionEaseInDuration.Value), PrevSection->Easing.ManualEaseInDuration);
					}
				}
			}
		}
		else
		{
			if (!bDelete)
			{
				// The given section is the first section. Let's clear its auto ease-in since there's no overlap anymore with a previous section.
				Section.Easing.AutoEaseInDuration = 0;
			}
		}

		// Find the next section and adjust its start time to match the moved/resized section's new end time.
		if (!bDelete)
		{
			int32 NextSectionIndex = SectionIndex + 1;
			if (Sections.IsValidIndex(NextSectionIndex))
			{
				UMovieSceneSection* NextSection = Sections[NextSectionIndex];

				NextSection->Modify();

				const FFrameNumber GapOrOverlap = NextSection->GetRange().GetLowerBoundValue() - SectionRange.GetUpperBoundValue();
				if (GapOrOverlap > 0)
				{
					// If we made a gap: adjust the next section's start time so that it lines up with the current section's end.
					TRangeBound<FFrameNumber> NewStartFrame = TRangeBound<FFrameNumber>::Inclusive(SectionRange.GetUpperBoundValue() - NextSection->Easing.GetEaseInDuration());

					if (!NextSection->HasEndFrame() || NewStartFrame.GetValue() < NextSection->GetExclusiveEndFrame())
					{
						// It's a gap!
						NextSection->SetStartFrame(NewStartFrame);
					}
				}
				else
				{
					// If we created an overlap: calls to UMovieSceneTrack::UpdateEasing will set the easing curves correctly based on overlaps.
					// However, we need to fixup some easing where overlaps don't occur, such as the very first ease-in and the very last ease-out.
					// Don't overlap so far that our ease-in, or the next section's ease-out, get overlapped. Clamp these easing durations instead.
					if (Section.HasStartFrame() && NextSection->HasStartFrame())
					{
						const FFrameNumber MaxEaseInDuration = NextSection->GetInclusiveStartFrame() - Section.GetInclusiveStartFrame();
						Section.Easing.AutoEaseInDuration = FMath::Min(FMath::Max(0, MaxEaseInDuration.Value), Section.Easing.AutoEaseInDuration);
						Section.Easing.ManualEaseInDuration = FMath::Min(FMath::Max(0, MaxEaseInDuration.Value), Section.Easing.ManualEaseInDuration);
					}
					if (Section.HasEndFrame() && NextSection->HasEndFrame())
					{
						const FFrameNumber MaxNextSectionEaseOutDuration = NextSection->GetExclusiveEndFrame() - Section.GetExclusiveEndFrame();
						NextSection->Easing.AutoEaseOutDuration = FMath::Min(FMath::Max(0, MaxNextSectionEaseOutDuration.Value), NextSection->Easing.AutoEaseOutDuration);
						NextSection->Easing.ManualEaseOutDuration = FMath::Min(FMath::Max(0, MaxNextSectionEaseOutDuration.Value), NextSection->Easing.ManualEaseOutDuration);
					}
				}
			}
			else
			{
				// The given section is the last section. Let's clear its auto ease-out since there's no overlap anymore with a next section.
				Section.Easing.AutoEaseOutDuration = 0;
			}
		}
	}

	bool bCleanUpDone = false;
	if (bCleanUp)
	{
		const TArray<UMovieSceneSection*> OverlappedSections = Sections.FilterByPredicate([&Section, SectionRange](const UMovieSceneSection* Cur)
				{
					if (Cur != &Section)
					{
						const TRange<FFrameNumber> CurRange = Cur->GetRange();
						return SectionRange.Contains(CurRange);
					}
					return false;
				});
		for (UMovieSceneSection* OverlappedSection : OverlappedSections)
		{
			Sections.Remove(OverlappedSection);
		}
		bCleanUpDone = (OverlappedSections.Num() > 0);
	}

	SortConsecutiveSections(Sections);

	return bCleanUpDone;
}


void MovieSceneHelpers::GetDescendantMovieScenes(UMovieSceneSequence* InSequence, TArray<UMovieScene*> & InMovieScenes)
{
	UMovieScene* InMovieScene = InSequence->GetMovieScene();
	if (InMovieScene == nullptr || InMovieScenes.Contains(InMovieScene))
	{
		return;
	}

	InMovieScenes.Add(InMovieScene);

	for (auto Section : InMovieScene->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		if (SubSection != nullptr)
		{
			UMovieSceneSequence* SubSequence = SubSection->GetSequence();
			if (SubSequence != nullptr)
			{
				GetDescendantMovieScenes(SubSequence, InMovieScenes);
			}
		}
	}
}

void MovieSceneHelpers::GetDescendantSubSections(const UMovieScene* InMovieScene, TArray<UMovieSceneSubSection*>& InSubSections)
{
	if (!IsValid(InMovieScene))
	{
		return;
	}

	for (UMovieSceneSection* Section : InMovieScene->GetAllSections())
	{
		if (UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section))
		{
			InSubSections.Add(SubSection);
			
			if (const UMovieSceneSequence* SubSequence = SubSection->GetSequence())
			{
				GetDescendantSubSections(SubSequence->GetMovieScene(), InSubSections);
			}
		}
	}
}

USceneComponent* MovieSceneHelpers::SceneComponentFromRuntimeObject(UObject* Object)
{
	AActor* Actor = Cast<AActor>(Object);

	USceneComponent* SceneComponent = nullptr;
	if (Actor && Actor->GetRootComponent())
	{
		// If there is an actor, modify its root component
		SceneComponent = Actor->GetRootComponent();
	}
	else
	{
		// No actor was found.  Attempt to get the object as a component in the case that we are editing them directly.
		SceneComponent = Cast<USceneComponent>(Object);
	}
	return SceneComponent;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromActor(const AActor* InActor)
{
	TArray<UCameraComponent*> CameraComponents;
	InActor->GetComponents(CameraComponents);

	// If there's a camera component that's active, return that one
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (CameraComponent->IsActive())
		{
			return CameraComponent;
		}
	}

	// Otherwise, return the first camera component
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		return CameraComponent;
	}

	return nullptr;
}

UCameraComponent* MovieSceneHelpers::CameraComponentFromRuntimeObject(UObject* RuntimeObject)
{
	if (RuntimeObject)
	{
		// find camera we want to control
		UCameraComponent* const CameraComponent = Cast<UCameraComponent>(RuntimeObject);
		if (CameraComponent)
		{
			return CameraComponent;
		}

		// see if it's an actor that has a camera component
		AActor* const Actor = Cast<AActor>(RuntimeObject);
		if (Actor)
		{
			return CameraComponentFromActor(Actor);
		}
	}

	return nullptr;
}

float MovieSceneHelpers::GetSoundDuration(USoundBase* Sound)
{
	return Sound ? FMath::Max(0.0f, Sound->GetDuration()) : 0.0f;
}


float MovieSceneHelpers::CalculateWeightForBlending(UMovieSceneSection* SectionToKey, FFrameNumber Time)
{
	float Weight = 1.0f;
	UMovieSceneTrack* Track = SectionToKey->GetTypedOuter<UMovieSceneTrack>();
	FOptionalMovieSceneBlendType BlendType = SectionToKey->GetBlendType();
	if (Track && BlendType.IsValid() && (BlendType.Get() == EMovieSceneBlendType::Additive || BlendType.Get() == EMovieSceneBlendType::Absolute))
	{
		//if additive weight is just the inverse of any weight on it
		if (BlendType.Get() == EMovieSceneBlendType::Additive)
		{
			float TotalWeightValue = SectionToKey->GetTotalWeightValue(Time);
			Weight = !FMath::IsNearlyZero(TotalWeightValue) ? 1.0f / TotalWeightValue : 0.0f;
		}
		else
		{

			const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
			TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections;
			for (UMovieSceneSection* Section : Sections)
			{
				if (MovieSceneHelpers::IsSectionKeyable(Section) && Section->GetRange().Contains(Time))
				{
					OverlappingSections.Add(Section);
				}
			}
			//if absolute need to calculate weight based upon other sections weights (+ implicit absolute weights)
			int TotalNumOfAbsoluteSections = 1;
			for (UMovieSceneSection* Section : OverlappingSections)
			{
				FOptionalMovieSceneBlendType NewBlendType = Section->GetBlendType();

				if (Section != SectionToKey && NewBlendType.IsValid() && NewBlendType.Get() == EMovieSceneBlendType::Absolute)
				{
					++TotalNumOfAbsoluteSections;
				}
			}
			float TotalWeightValue = SectionToKey->GetTotalWeightValue(Time);
			Weight = !FMath::IsNearlyZero(TotalWeightValue) ? float(TotalNumOfAbsoluteSections) / TotalWeightValue : 0.0f;
		}
	}
	return Weight;
}

FString MovieSceneHelpers::MakeUniqueSpawnableName(UMovieScene* MovieScene, const FString& InName)
{
	FString NewName = InName;

	auto DuplName = [&](FMovieSceneSpawnable& InSpawnable)
	{
		return InSpawnable.GetName() == NewName;
	};

	int32 Index = 2;
	FString UniqueString;
	while (MovieScene->FindSpawnable(DuplName))
	{
		NewName.RemoveFromEnd(UniqueString);
		UniqueString = FString::Printf(TEXT(" (%d)"), Index++);
		NewName += UniqueString;
	}
	return NewName;
}

UObject* MovieSceneHelpers::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, UMovieScene* InMovieScene, FName InName)
{
	UObject* NewInstance = NewObject<UObject>(InMovieScene, InSourceObject.GetClass(), InName);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
	CopyParams.bPreserveRootComponent = false;
	CopyParams.bPerformDuplication = true;
	UEngine::CopyPropertiesForUnrelatedObjects(&InSourceObject, NewInstance, CopyParams);

	AActor* Actor = CastChecked<AActor>(NewInstance);
	if (Actor->GetAttachParentActor() != nullptr)
	{
		// We don't support spawnables and attachments right now
		// @todo: map to attach track?
		Actor->DetachFromActor(FDetachmentTransformRules(FAttachmentTransformRules(EAttachmentRule::KeepRelative, false), false));
	}

	// The spawnable source object was created with RF_Transient. The object generated from that needs its 
	// component flags cleared of RF_Transient so that the template object can be saved to the level sequence.
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component)
		{
			Component->ClearFlags(RF_Transient);
		}
	}

	return NewInstance;
}


MovieSceneHelpers::FMovieSceneScopedPackageDirtyGuard::FMovieSceneScopedPackageDirtyGuard(USceneComponent* InComponent)
{
	Component = InComponent;
	if (Component && Component->GetPackage())
	{
		bPackageWasDirty = Component->GetPackage()->IsDirty();
	}
}

MovieSceneHelpers::FMovieSceneScopedPackageDirtyGuard::~FMovieSceneScopedPackageDirtyGuard()
{
	if (Component && Component->GetPackage())
	{
		Component->GetPackage()->SetDirtyFlag(bPackageWasDirty);
	}
}

FTrackInstancePropertyBindings::FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath )
	: PropertyPath( InPropertyPath )
	, PropertyName( InPropertyName )
{
	static const FString Set(TEXT("Set"));
	const FString FunctionString = Set + PropertyName.ToString();

	FunctionName = FName(*FunctionString);
}

struct FPropertyAndIndex
{
	FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

	FProperty* Property;
	int32 ArrayIndex;
};

FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName)
{
	FPropertyAndIndex PropertyAndIndex;

	// Calculate the array index if possible
	int32 ArrayIndex = -1;
	if (PropertyName.Len() > 0 && PropertyName.GetCharArray()[PropertyName.Len() - 1] == ']')
	{
		int32 OpenIndex = 0;
		if (PropertyName.FindLastChar('[', OpenIndex))
		{
			FString TruncatedPropertyName(OpenIndex, *PropertyName);
			PropertyAndIndex.Property = FindFProperty<FProperty>(InStruct, *TruncatedPropertyName);

			const int32 NumberLength = PropertyName.Len() - OpenIndex - 2;
			if (NumberLength > 0 && NumberLength <= 10)
			{
				TCHAR NumberBuffer[11];
				FMemory::Memcpy(NumberBuffer, &PropertyName[OpenIndex + 1], sizeof(TCHAR) * NumberLength);
				LexFromString(PropertyAndIndex.ArrayIndex, NumberBuffer);
			}

			return PropertyAndIndex;
		}
	}

	PropertyAndIndex.Property = FindFProperty<FProperty>(InStruct, *PropertyName);
	return PropertyAndIndex;
}


FProperty* FTrackInstancePropertyBindings::FindPropertyRecursive(UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index)
{
	FProperty* Property = FindPropertyAndArrayIndex(InStruct, *InPropertyNames[Index]).Property;
	if (!Property)
	{
		return nullptr;
	}

	/* Recursive property types */
	if (Property->IsA(FArrayProperty::StaticClass()))
	{
		FArrayProperty* ArrayProp = CastFieldChecked<FArrayProperty>(Property);

		FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
		if (InnerStructProp && InPropertyNames.IsValidIndex(Index + 1))
		{
			return FindPropertyRecursive(InnerStructProp->Struct, InPropertyNames, Index + 1);
		}
		else
		{
			Property = ArrayProp->Inner;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if( InPropertyNames.IsValidIndex(Index+1) )
		{
			return FindPropertyRecursive(StructProp->Struct, InPropertyNames, Index+1);
		}
		else
		{
			check( StructProp->GetName() == InPropertyNames[Index] );
		}
	}

	return Property;
}

FProperty* FTrackInstancePropertyBindings::FindProperty(const UObject* Object, const FString& InPropertyPath)
{
	check(Object);

	TArray<FString> PropertyNames;
	InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

	if (PropertyNames.Num() > 0)
	{
		return FindPropertyRecursive(Object->GetClass(), PropertyNames, 0);
	}

	return nullptr;
}

FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindPropertyAddressRecursive( void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index )
{
	FPropertyAndIndex PropertyAndIndex = FindPropertyAndArrayIndex(InStruct, *InPropertyNames[Index]);
	
	FTrackInstancePropertyBindings::FPropertyAddress NewAddress;

	if (PropertyAndIndex.ArrayIndex != INDEX_NONE)
	{
		if (PropertyAndIndex.Property)
		{
			if (PropertyAndIndex.Property->IsA(FArrayProperty::StaticClass()))
			{
				FArrayProperty* ArrayProp = CastFieldChecked<FArrayProperty>(PropertyAndIndex.Property);

				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(BasePointer));
				if (ArrayHelper.IsValidIndex(PropertyAndIndex.ArrayIndex))
				{
					FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
					if (InnerStructProp && InPropertyNames.IsValidIndex(Index + 1))
					{
						return FindPropertyAddressRecursive(ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex), InnerStructProp->Struct, InPropertyNames, Index + 1);
					}
					else
					{
						NewAddress.Property = ArrayProp->Inner;
						NewAddress.Address = ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex);
					}
				}
			}
			else
			{
				UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *PropertyAndIndex.Property->GetName(), *FArrayProperty::StaticClass()->GetName());
			}
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(PropertyAndIndex.Property))
	{
		NewAddress.Property = StructProp;
		NewAddress.Address = BasePointer;

		if( InPropertyNames.IsValidIndex(Index+1) )
		{
			void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(BasePointer);
			return FindPropertyAddressRecursive( StructContainer, StructProp->Struct, InPropertyNames, Index+1 );
		}
		else
		{
			check( StructProp->GetName() == InPropertyNames[Index] );
		}
	}
	else if(PropertyAndIndex.Property)
	{
		NewAddress.Property = PropertyAndIndex.Property;
		NewAddress.Address = BasePointer;
	}

	return NewAddress;

}


FTrackInstancePropertyBindings::FPropertyAddress FTrackInstancePropertyBindings::FindPropertyAddress( const UObject& InObject, const FString& InPropertyPath )
{
	TArray<FString> PropertyNames;

	InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

	if(IsValid(&InObject) && PropertyNames.Num() > 0)
	{
		return FindPropertyAddressRecursive( (void*)&InObject, InObject.GetClass(), PropertyNames, 0 );
	}
	else
	{
		return FTrackInstancePropertyBindings::FPropertyAddress();
	}
}

void FTrackInstancePropertyBindings::CallFunctionForEnum( UObject& InRuntimeObject, int64 PropertyValue )
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);

	FProperty* Property = GetProperty(InRuntimeObject);
	if (Property && Property->HasSetter())
	{
		Property->CallSetter(&InRuntimeObject, &PropertyValue);
	}
	else if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
	{
		InvokeSetterFunction(&InRuntimeObject, SetterFunction, PropertyValue);
	}
	else if (Property && Property->IsA(FEnumProperty::StaticClass()))
	{
		if (FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Property))
		{
			FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
			void* ValueAddr = EnumProperty->ContainerPtrToValuePtr<void>(PropAndFunction.PropertyAddress.Address);
			UnderlyingProperty->SetIntPropertyValue(ValueAddr, PropertyValue);
		}
	}
	else if (Property)
	{
		UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *Property->GetName(), *FEnumProperty::StaticClass()->GetName());
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}

void FTrackInstancePropertyBindings::CacheBinding(const UObject& Object)
{
	FPropertyAndFunction PropAndFunction;
	{
		PropAndFunction.PropertyAddress = FindPropertyAddress(Object, PropertyPath);

		UFunction* SetterFunction = Object.FindFunction(FunctionName);
		if (SetterFunction && SetterFunction->NumParms >= 1)
		{
			PropAndFunction.SetterFunction = SetterFunction;
		}
		
		UFunction* NotifyFunction = NotifyFunctionName != NAME_None ? Object.FindFunction(NotifyFunctionName) : nullptr;
		if (NotifyFunction && NotifyFunction->NumParms == 0 && NotifyFunction->ReturnValueOffset == MAX_uint16)
		{
			PropAndFunction.NotifyFunction = NotifyFunction;
		}
	}

	RuntimeObjectToFunctionMap.Add(FObjectKey(&Object), PropAndFunction);
}

FProperty* FTrackInstancePropertyBindings::GetProperty(const UObject& Object) const
{
	FPropertyAndFunction PropAndFunction = RuntimeObjectToFunctionMap.FindRef(&Object);
	if (FProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		return Property;
	}

	return FindPropertyAddress(Object, PropertyPath).GetProperty();
}

int64 FTrackInstancePropertyBindings::GetCurrentValueForEnum(const UObject& Object)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

	if (FProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (Property->IsA(FEnumProperty::StaticClass()))
		{
			if (FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Property))
			{
				FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				void* ValueAddr = EnumProperty->ContainerPtrToValuePtr<void>(PropAndFunction.PropertyAddress.Address);
				int64 Result = UnderlyingProperty->GetSignedIntPropertyValue(ValueAddr);
				return Result;
			}
		}
		else
		{
			UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *Property->GetName(), *FEnumProperty::StaticClass()->GetName());
		}
	}

	return 0;
}

template<> void FTrackInstancePropertyBindings::CallFunction<bool>(UObject& InRuntimeObject, TCallTraits<bool>::ParamType PropertyValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);

	FProperty* Property = GetProperty(InRuntimeObject);
	if (Property && Property->HasSetter())
	{
		Property->CallSetter(&InRuntimeObject, &PropertyValue);
	}
	else if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
	{
		InvokeSetterFunction(&InRuntimeObject, SetterFunction, PropertyValue);
	}
	else if (Property && Property->IsA(FBoolProperty::StaticClass()))
	{
		if (FBoolProperty* BoolProperty = CastFieldChecked<FBoolProperty>(Property))
		{
			uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			BoolProperty->SetPropertyValue(ValuePtr, PropertyValue);
		}
	}
	else if (Property)
	{
		UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *Property->GetName(), *FBoolProperty::StaticClass()->GetName());
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}

template<> bool FTrackInstancePropertyBindings::ResolvePropertyValue<bool>(const FPropertyAddress& Address, bool& OutValue)
{
	if (FProperty* Property = Address.GetProperty())
	{
		if (Property->IsA(FBoolProperty::StaticClass()))
		{
			if (FBoolProperty* BoolProperty = CastFieldChecked<FBoolProperty>(Property))
			{
				const uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(Address.Address);
				OutValue = BoolProperty->GetPropertyValue(ValuePtr);
				return true;
			}
		}
		else
		{
			UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *Property->GetName(), *FBoolProperty::StaticClass()->GetName());
		}
	}

	return false;
}

template<> void FTrackInstancePropertyBindings::SetCurrentValue<bool>(UObject& Object, TCallTraits<bool>::ParamType InValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(Object);
	if (FProperty* Property = PropAndFunction.PropertyAddress.GetProperty())
	{
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			uint8* ValuePtr = BoolProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			BoolProperty->SetPropertyValue(ValuePtr, InValue);
		}
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		Object.ProcessEvent(NotifyFunction, nullptr);
	}
}


template<> void FTrackInstancePropertyBindings::CallFunction<UObject*>(UObject& InRuntimeObject, UObject* PropertyValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);

	FProperty* Property = GetProperty(InRuntimeObject);
	if (Property && Property->HasSetter())
	{
		Property->CallSetter(&InRuntimeObject, &PropertyValue);
	}
	else if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
	{
		InvokeSetterFunction(&InRuntimeObject, SetterFunction, PropertyValue);
	}
	else if (Property && Property->IsA(FObjectPropertyBase::StaticClass()))
	{
		if (FObjectPropertyBase* ObjectProperty = CastFieldChecked<FObjectPropertyBase>(Property))
		{
			uint8* ValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
			ObjectProperty->SetObjectPropertyValue(ValuePtr, PropertyValue);
		}
	}
	else if (Property)
	{
		UE_LOG(LogMovieScene, Error, TEXT("Mismatch in property evaluation. %s is not of type: %s"), *Property->GetName(), *FObjectPropertyBase::StaticClass()->GetName());
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}

template<> bool FTrackInstancePropertyBindings::ResolvePropertyValue<UObject*>(const FPropertyAddress& Address, UObject*& OutValue)
{
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Address.GetProperty()))
	{
		const uint8* ValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(Address.Address);
		OutValue = ObjectProperty->GetObjectPropertyValue(ValuePtr);
		return true;
	}

	return false;
}

template<> void FTrackInstancePropertyBindings::SetCurrentValue<UObject*>(UObject& InRuntimeObject, UObject* InValue)
{
	FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropAndFunction.PropertyAddress.GetProperty()))
	{
		uint8* ValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(PropAndFunction.PropertyAddress.Address);
		ObjectProperty->SetObjectPropertyValue(ValuePtr, InValue);
	}

	if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
	{
		InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
	}
}
