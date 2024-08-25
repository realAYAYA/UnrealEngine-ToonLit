// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/EulerTransformPropertyTrackEditor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "SequencerUtilities.h"
#include "MVVM/Views/ViewUtilities.h"
#include "Sections/TransformPropertySection.h"
#include "MovieSceneToolHelpers.h"
#include "IKeyArea.h"

#include "MovieSceneTracksComponentTypes.h"
#include "Systems/MovieScenePropertyInstantiator.h"

TSharedRef<ISequencerTrackEditor> FEulerTransformPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FEulerTransformPropertyTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> FEulerTransformPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FEulerTransformPropertyTrackEditor"));

	return MakeShared<FTransformSection>(SectionObject, GetSequencer());
}


TSharedPtr<SWidget> FEulerTransformPropertyTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

	const int32 RowIndex = Params.TrackInsertRowIndex;
	auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, RowIndex, Track, WeakSequencer);

		return MenuBuilder.MakeWidget();
	};

	return UE::Sequencer::MakeAddButton(NSLOCTEXT("FEulerTransformPropertyTrackEditor", "AddSection", "Section"), FOnGetContent::CreateLambda(SubMenuCallback), Params.ViewModel);
}


void FEulerTransformPropertyTrackEditor::ProcessKeyOperation(FFrameNumber InKeyTime, const UE::Sequencer::FKeyOperation& Operation, ISequencer& InSequencer)
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


void FEulerTransformPropertyTrackEditor::ProcessKeyOperation(UObject* ObjectToKey, TArrayView<const UE::Sequencer::FKeySectionOperation> SectionsToKey, ISequencer& InSequencer, FFrameNumber KeyTime)
{
	using namespace UE::MovieScene;
	using namespace UE::Sequencer;

	FMovieSceneSequenceID SequenceID = GetSequencer()->GetFocusedTemplateID();
	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = GetSequencer()->GetEvaluationTemplate();

	UMovieSceneEulerTransformTrack* Track = nullptr;

	TArray<FMovieSceneEntityID> EntitiesPerSection, ValidEntities;
	for (const FKeySectionOperation& Operation : SectionsToKey)
	{
		FMovieSceneEntityID EntityID = EvaluationTemplate.FindEntityFromOwner(Operation.Section->GetSectionObject(), 0, SequenceID);

		if (!Track)
		{
			Track = CastChecked<UMovieSceneEulerTransformTrack>(Operation.Section->GetSectionObject()->GetOuter());
		}

		EntitiesPerSection.Add(EntityID);
		if (EntityID)
		{
			ValidEntities.Add(EntityID);
		}
	}

	if (!ensure(Track))
	{
		return;
	}

	UMovieSceneEntitySystemLinker*         EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
	UMovieScenePropertyInstantiatorSystem* System = EntityLinker ? EntityLinker->FindSystem<UMovieScenePropertyInstantiatorSystem>() : nullptr;

	if (System && ValidEntities.Num() != 0)
	{
		FDecompositionQuery Query;
		Query.Entities = ValidEntities;
		Query.Object   = ObjectToKey;

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &EntityLinker->EntityManager);

		FIntermediate3DTransform CurrentValue;
		ConvertOperationalProperty(Track->GetCurrentValue<FEulerTransform>(ObjectToKey).Get(FEulerTransform::Identity), CurrentValue);
		TRecompositionResult<FIntermediate3DTransform> TransformData = System->RecomposeBlendOperational(FMovieSceneTracksComponentTypes::Get()->EulerTransform, Query, CurrentValue);

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
				if (Handle.GetChannelTypeName() == FMovieSceneDoubleChannel::StaticStruct()->GetFName() && Handle.GetChannelIndex() < 9)
				{
					FMovieSceneDoubleChannel* Channel = static_cast<FMovieSceneDoubleChannel*>(Handle.Get());

					double Value = RecomposedTransform[Handle.GetChannelIndex()];
					EMovieSceneKeyInterpolation Interpolation = GetInterpolationMode(Channel, KeyTime, InSequencer.GetKeyInterpolation());
					AddKeyToChannel(Channel, KeyTime, Value, Interpolation);
				}
				else
				{
					KeyArea->AddOrUpdateKey(KeyTime, FGuid(), InSequencer);
				}
			}
		}
	}
}


void FEulerTransformPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys)
{
	const TCHAR* ChannelNames[9] = {
		TEXT("Location.X"),    TEXT("Location.Y"),     TEXT("Location.Z"),
		TEXT("Rotation.Roll"), TEXT("Rotation.Pitch"), TEXT("Rotation.Yaw"),
		TEXT("Scale.X"),       TEXT("Scale.Y"),        TEXT("Scale.Z")
	};

	bool bKeyChannels[9] = {
		true, true, true,
		true, true, true,
		true, true, true,
	};

	FString LeafPath;
	FString QualifiedLeafPath;

	int32 NumKeyedProperties = PropertyChangedParams.StructPathToKey.GetNumProperties();
	if (NumKeyedProperties >= 1)
	{
		LeafPath = PropertyChangedParams.StructPathToKey.GetPropertyInfo(NumKeyedProperties-1).Property->GetName();
	}

	if (NumKeyedProperties >= 2)
	{
		QualifiedLeafPath = PropertyChangedParams.StructPathToKey.GetPropertyInfo(NumKeyedProperties-2).Property->GetName();
		QualifiedLeafPath.AppendChar('.');
		QualifiedLeafPath += LeafPath;
	}

	if (LeafPath.Len() > 0)
	{
		for (int32 ChannelIndex = 0; ChannelIndex < UE_ARRAY_COUNT(ChannelNames); ++ChannelIndex)
		{
			// If it doesn't match the fully qualified path, and doesn't start with the leaf path, don't add a key
			bool bMatchesQualifiedPath = FPlatformString::Stricmp(*QualifiedLeafPath, ChannelNames[ChannelIndex]) == 0;
			bool bMatchesLeafPath      = FCString::Strnicmp(ChannelNames[ChannelIndex], *LeafPath, LeafPath.Len()) == 0;

			if (!bMatchesQualifiedPath && !bMatchesLeafPath)
			{
				bKeyChannels[ChannelIndex] = false;
			}
		}
	}

	FEulerTransform Transform           = PropertyChangedParams.GetPropertyValue<FEulerTransform>();
	FEulerTransform RecomposedTransform = RecomposeTransform(Transform, PropertyChangedParams.ObjectsThatChanged[0], SectionToKey);

	FVector Translation = RecomposedTransform.Location;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(0, (double)Translation.X, bKeyChannels[0]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(1, (double)Translation.Y, bKeyChannels[1]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(2, (double)Translation.Z, bKeyChannels[2]));

	FRotator Rotator = RecomposedTransform.Rotation;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(3, (double)Rotator.Roll,  bKeyChannels[3]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(4, (double)Rotator.Pitch, bKeyChannels[4]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(5, (double)Rotator.Yaw,   bKeyChannels[5]));

	FVector Scale = RecomposedTransform.Scale;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(6, (double)Scale.X, bKeyChannels[6]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(7, (double)Scale.Y, bKeyChannels[7]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(8, (double)Scale.Z, bKeyChannels[8]));
}


FEulerTransform FEulerTransformPropertyTrackEditor::RecomposeTransform(const FEulerTransform& InTransform, UObject* AnimatedObject, UMovieSceneSection* Section)
{
	using namespace UE::MovieScene;

	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = GetSequencer()->GetEvaluationTemplate();

	UMovieSceneEntitySystemLinker* EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
	FMovieSceneEntityID EntityID = EvaluationTemplate.FindEntityFromOwner(Section, 0, GetSequencer()->GetFocusedTemplateID());

	if (EntityLinker && EntityID)
	{
		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &EntityLinker->EntityManager);

		UMovieScenePropertyInstantiatorSystem* System = EntityLinker->FindSystem<UMovieScenePropertyInstantiatorSystem>();
		if (System)
		{
			FDecompositionQuery Query;
			Query.Entities = MakeArrayView(&EntityID, 1);
			Query.Object   = AnimatedObject;

			return System->RecomposeBlend(FMovieSceneTracksComponentTypes::Get()->EulerTransform, Query, InTransform).Values[0];
		}
	}

	return InTransform;
}
