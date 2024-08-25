// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/TransformPropertyTrackEditor.h"
#include "TransformData.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Sections/TransformPropertySection.h"
#include "SequencerUtilities.h"
#include "MVVM/Views/ViewUtilities.h"
#include "MovieSceneToolHelpers.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

#include "Systems/MovieScenePropertyInstantiator.h"
#include "MovieSceneTracksComponentTypes.h"

TSharedRef<ISequencerTrackEditor> FTransformPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FTransformPropertyTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> FTransformPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShared<FTransformSection>(SectionObject, GetSequencer());
}


TSharedPtr<SWidget> FTransformPropertyTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

	const int32 RowIndex = Params.TrackInsertRowIndex;
	auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, RowIndex, Track, WeakSequencer);

		return MenuBuilder.MakeWidget();
	};

	return UE::Sequencer::MakeAddButton(NSLOCTEXT("FTransformPropertyTrackEditor", "AddSection", "Section"), FOnGetContent::CreateLambda(SubMenuCallback), Params.ViewModel);
}


void FTransformPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	using namespace UE::MovieScene;

	FIntermediate3DTransform CurrentTransform;
	ConvertOperationalProperty(PropertyChangedParams.GetPropertyValue<FTransform>(), CurrentTransform);

	FIntermediate3DTransform Recomposed = RecomposeTransform(CurrentTransform, PropertyChangedParams.ObjectsThatChanged[0], SectionToKey);

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(0, Recomposed.T_X, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(1, Recomposed.T_Y, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(2, Recomposed.T_Z, true));

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(3, Recomposed.R_X, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(4, Recomposed.R_Y, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(5, Recomposed.R_Z, true));

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(6, Recomposed.S_X, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(7, Recomposed.S_Y, true));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(8, Recomposed.S_Z, true));
}

UE::MovieScene::FIntermediate3DTransform FTransformPropertyTrackEditor::RecomposeTransform(const UE::MovieScene::FIntermediate3DTransform& InTransformData, UObject* AnimatedObject, UMovieSceneSection* Section)
{
	using namespace UE::MovieScene;

	const FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = GetSequencer()->GetEvaluationTemplate();

	UMovieSceneEntitySystemLinker* EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
	FMovieSceneEntityID EntityID = EvaluationTemplate.FindEntityFromOwner(Section, 0, GetSequencer()->GetFocusedTemplateID());

	if (EntityLinker && EntityID)
	{
		UMovieScenePropertyInstantiatorSystem* System = EntityLinker->FindSystem<UMovieScenePropertyInstantiatorSystem>();
		if (System)
		{
			FDecompositionQuery Query;
			Query.Entities = MakeArrayView(&EntityID, 1);
			Query.Object   = AnimatedObject;

			return System->RecomposeBlendOperational(FMovieSceneTracksComponentTypes::Get()->Transform, Query, InTransformData).Values[0];
		}
	}

	return InTransformData;
}
